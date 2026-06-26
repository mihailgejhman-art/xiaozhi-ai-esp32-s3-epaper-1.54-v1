#include "weather_manager.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_http_client.h"

#define TAG "WeatherManager"
#ifdef CONFIG_WEATHER_CITY
#define WEATHER_URL "http://wttr.in/" CONFIG_WEATHER_CITY "?format=%C+%t"
#else
#define WEATHER_URL "http://wttr.in/Moscow?format=%C+%t"
#endif
#ifdef CONFIG_WEATHER_UPDATE_INTERVAL
#define WEATHER_UPDATE_INTERVAL_US (CONFIG_WEATHER_UPDATE_INTERVAL * 60 * 1000000ULL)
#else
#define WEATHER_UPDATE_INTERVAL_US (30 * 60 * 1000000ULL)
#endif

const char* WeatherManager::WeatherCodeToString(int code) {
    if (code == 0) return "Clear";
    if (code <= 3) return "Cloudy";
    if (code <= 48) return "Fog";
    if (code <= 57) return "Drizzle";
    if (code <= 67) return "Rain";
    if (code <= 77) return "Snow";
    if (code <= 82) return "Showers";
    if (code <= 86) return "Snow showers";
    if (code >= 95) return "Storm";
    return "Unknown";
}

WeatherManager& WeatherManager::GetInstance() {
    static WeatherManager instance;
    return instance;
}

WeatherManager::~WeatherManager() {
    destroyed_ = true;
    if (timer_ != nullptr) {
        esp_timer_stop(timer_);
        esp_timer_delete(timer_);
    }
}

void WeatherManager::Initialize() {
    if (initialized_) return;
    initialized_ = true;

    esp_timer_create_args_t args = {};
    args.callback = [](void* arg) {
        auto self = static_cast<WeatherManager*>(arg);
        if (self->destroyed_) return;
        self->RequestUpdate();
        if (self->timer_ != nullptr) {
            if (self->weather_data_.valid) {
                esp_timer_stop(self->timer_);
                esp_timer_start_periodic(self->timer_, WEATHER_UPDATE_INTERVAL_US);
            } else {
                esp_timer_stop(self->timer_);
                esp_timer_start_once(self->timer_, 30 * 1000000ULL);
            }
        }
    };
    args.arg = this;
    args.name = "weather_timer";
    esp_timer_create(&args, &timer_);
    esp_timer_start_once(timer_, 15 * 1000000ULL);
}

void WeatherManager::SetUpdateCallback(std::function<void(const WeatherData& data)> callback) {
    callback_ = std::move(callback);
}

WeatherData WeatherManager::GetCurrentWeather() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return weather_data_;
}

std::string WeatherManager::GetFormattedWeather() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cached_formatted_;
}

void WeatherManager::RequestUpdate() {
    if (busy_.exchange(true)) {
        ESP_LOGW(TAG, "Fetch already in progress, skipping");
        return;
    }
    TaskHandle_t task = nullptr;
    xTaskCreate(FetchTask, "weather_fetch", 6144, this, 5, &task);
}

void WeatherManager::FetchTask(void* arg) {
    auto self = static_cast<WeatherManager*>(arg);
    self->DoFetch();
    vTaskDelete(NULL);
}

void WeatherManager::DoFetch() {
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == nullptr || !esp_netif_is_netif_up(netif)) {
        ESP_LOGW(TAG, "Network not available, skipping weather fetch");
        busy_ = false;
        return;
    }

    esp_http_client_config_t config = {};
    config.url = WEATHER_URL;
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = 10000;

    auto client = esp_http_client_init(&config);
    if (client == nullptr) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        busy_ = false;
        return;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        busy_ = false;
        return;
    }

    auto headers_ret = esp_http_client_fetch_headers(client);
    if (headers_ret < 0) {
        ESP_LOGE(TAG, "Failed to fetch HTTP headers");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        busy_ = false;
        return;
    }

    std::string response;
    char buf[128];
    int read_len;
    while ((read_len = esp_http_client_read(client, buf, sizeof(buf) - 1)) > 0) {
        buf[read_len] = '\0';
        response.append(buf);
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (response.empty()) {
        ESP_LOGE(TAG, "Failed to read response");
        busy_ = false;
        return;
    }

    while (!response.empty() && (response.back() == '\n' || response.back() == '\r' || response.back() == ' ')) {
        response.pop_back();
    }

    size_t plus_pos = response.find('+');
    size_t minus_pos = response.find('-', 1);
    size_t num_pos = (plus_pos != std::string::npos) ? plus_pos : minus_pos;

    std::string desc_str;
    std::string temp_str;

    if (num_pos != std::string::npos) {
        desc_str = response.substr(0, num_pos);
        while (!desc_str.empty() && desc_str.back() == ' ') desc_str.pop_back();
        temp_str = response.substr(num_pos);
    } else {
        desc_str = response;
    }

    if (!temp_str.empty()) {
        WeatherData new_data;
        new_data.temperature = (float)atof(temp_str.c_str());
        new_data.description = desc_str;
        new_data.valid = true;
        ESP_LOGI(TAG, "Weather: %.1f°C, desc=%s", new_data.temperature, desc_str.c_str());

        char weather_buf[48];
        const char* desc = new_data.description.empty() ? WeatherCodeToString(new_data.weather_code) : new_data.description.c_str();
        snprintf(weather_buf, sizeof(weather_buf), "%s %.0f°C", desc, new_data.temperature);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            weather_data_ = new_data;
            cached_formatted_ = weather_buf;
        }

        if (callback_) {
            callback_(new_data);
        }
    } else {
        ESP_LOGE(TAG, "Failed to parse weather: %s", response.c_str());
    }

    busy_ = false;
}
