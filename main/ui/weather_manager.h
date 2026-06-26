#pragma once

#include <string>
#include <functional>
#include <mutex>
#include <atomic>
#include <esp_timer.h>

struct WeatherData {
    float temperature = 0;
    int weather_code = 0;
    std::string description;
    bool valid = false;
};

class WeatherManager {
public:
    static WeatherManager& GetInstance();
    void Initialize();
    void SetUpdateCallback(std::function<void(const WeatherData& data)> callback);
    WeatherData GetCurrentWeather() const;
    std::string GetFormattedWeather() const;
    void RequestUpdate();
    static const char* WeatherCodeToString(int code);

    ~WeatherManager();

private:
    WeatherManager() = default;
    static void FetchTask(void* arg);
    void DoFetch();

    esp_timer_handle_t timer_ = nullptr;
    std::atomic<bool> destroyed_{false};
    WeatherData weather_data_;
    std::string cached_formatted_;
    mutable std::mutex mutex_;
    std::atomic<bool> busy_{false};
    std::function<void(const WeatherData& data)> callback_;
    std::atomic<bool> initialized_{false};
};
