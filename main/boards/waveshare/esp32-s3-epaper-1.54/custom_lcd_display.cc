#include <stdio.h>
#include <esp_lcd_panel_io.h>
#include <freertos/FreeRTOS.h>
#include <vector>
#include <esp_log.h>
#include "custom_lcd_display.h"
#include "board.h"
#include "config.h"
#include "esp_lvgl_port.h"
#include "settings.h"
#include "application.h"
#include <time.h>
#include <cJSON.h>

#define TAG "CustomLcdDisplay"

LV_FONT_DECLARE(lv_font_montserrat_48);

#define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))
#define BUFF_SIZE (EXAMPLE_LCD_WIDTH * EXAMPLE_LCD_HEIGHT * BYTES_PER_PIXEL)

static constexpr int EMOJI_SCALE = 512;
static constexpr int CLOCK_SCALE = 346;
static constexpr int MAX_PARTIAL_REFRESHES = 10;

const uint8_t WF_Full_1IN54[159] =
{
    0x80,0x48,0x40,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x40,0x48,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x80,0x48,0x40,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x40,0x48,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0xA,0x0,0x0,0x0,0x0,0x0,0x0,
    0x8,0x1,0x0,0x8,0x1,0x0,0x2,
    0xA,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x22,0x22,0x22,0x22,0x22,0x22,0x0,0x0,0x0,
    0x22,0x17,0x41,0x0,0x32,0x20
};

const uint8_t WF_PARTIAL_1IN54_0[159] =
{
    0x0,0x40,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x80,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x40,0x40,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0xF,0x0,0x0,0x0,0x0,0x0,0x0,
    0x1,0x1,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x22,0x22,0x22,0x22,0x22,0x22,0x0,0x0,0x0,
    0x02,0x17,0x41,0xB0,0x32,0x28,
};

// 4x4 Bayer dithering matrix for B&W conversion quality
static constexpr uint8_t BAYER_4x4[4][4] = {
    { 0,  8,  2, 10},
    {12,  4, 14,  6},
    { 3, 11,  1,  9},
    {15,  7, 13,  5}
};

static uint8_t apply_dither(lv_color16_t pixel, int x, int y) {
    uint16_t gray = (pixel.red * 77 + pixel.green * 150 + pixel.blue * 29) >> 8;
    int threshold = BAYER_4x4[y & 3][x & 3] * 16;
    return (gray + threshold >= 128) ? DRIVER_COLOR_WHITE : DRIVER_COLOR_BLACK;
}

void CustomLcdDisplay::lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p) {
    assert(disp != NULL);
    CustomLcdDisplay *driver = static_cast<CustomLcdDisplay *>(lv_display_get_user_data(disp));
    lv_color16_t     *buffer = reinterpret_cast<lv_color16_t *>(color_p);
    driver->EPD_Clear();
    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
            uint8_t color = apply_dither(*buffer, x, y);
            driver->EPD_DrawColorPixel(x, y, color);
            buffer++;
        }
    }
    driver->partial_refresh_count_++;
    if (driver->partial_refresh_count_ >= MAX_PARTIAL_REFRESHES) {
        driver->EPD_Init();
        driver->EPD_Clear();
        driver->EPD_DisplayPartBaseImage();
        driver->EPD_Init_Partial();
        driver->partial_refresh_count_ = 0;
        ESP_LOGI(TAG, "Periodic full refresh completed");
    } else {
        driver->EPD_DisplayPart();
    }
    lv_display_flush_ready(disp);
}

CustomLcdDisplay::CustomLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
    int width, int height, int offset_x, int offset_y,
    bool mirror_x, bool mirror_y, bool swap_xy, custom_lcd_spi_t _lcd_spi_data) :
    LcdDisplay(panel_io, panel, width, height),
    lcd_spi_data(_lcd_spi_data),
    Width(width), Height(height) {

    ESP_LOGI(TAG, "Initialize SPI");
    spi_port_init();
    spi_gpio_init();

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority   = 2;
    port_cfg.timer_period_ms = 500;
    lvgl_port_init(&port_cfg);
    lvgl_port_lock(0);

    buffer = static_cast<uint8_t *>(heap_caps_malloc(lcd_spi_data.buffer_len, MALLOC_CAP_INTERNAL));
    assert(buffer);
    display_ = lv_display_create(width, height);
    lv_display_set_flush_cb(display_, lvgl_flush_cb);
    lv_display_set_user_data(display_, this);

    uint8_t *buffer_1 = nullptr;
    buffer_1 = static_cast<uint8_t *>(heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM));
    if (buffer_1 == nullptr) {
        ESP_LOGW(TAG, "PSRAM allocation failed, trying internal RAM");
        buffer_1 = static_cast<uint8_t *>(heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_INTERNAL));
    }
    assert(buffer_1);
    lv_display_set_buffers(display_, buffer_1, nullptr, BUFF_SIZE, LV_DISPLAY_RENDER_MODE_FULL);

    ESP_LOGI(TAG, "EPD init");
    EPD_Init();
    EPD_Clear();
    EPD_DisplayPartBaseImage();
    EPD_Init_Partial();

    lvgl_port_unlock();
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }
}

CustomLcdDisplay::~CustomLcdDisplay() {
#ifdef CONFIG_WEATHER_ENABLE
    if (weather_timer_ != nullptr) {
        esp_timer_stop(weather_timer_);
        esp_timer_delete(weather_timer_);
        weather_timer_ = nullptr;
    }
    if (weather_init_timer_ != nullptr) {
        esp_timer_stop(weather_init_timer_);
        esp_timer_delete(weather_init_timer_);
        weather_init_timer_ = nullptr;
    }
#endif
}

void CustomLcdDisplay::spi_gpio_init() {
    int rst  = lcd_spi_data.rst;
    int cs   = lcd_spi_data.cs;
    int dc   = lcd_spi_data.dc;
    int busy = lcd_spi_data.busy;

    gpio_config_t gpio_conf = {};
    gpio_conf.intr_type     = GPIO_INTR_DISABLE;
    gpio_conf.mode          = GPIO_MODE_OUTPUT;
    gpio_conf.pin_bit_mask  = (0x1ULL << rst) | (0x1ULL << dc) | (0x1ULL << cs);
    gpio_conf.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en    = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));

    gpio_conf.mode         = GPIO_MODE_INPUT;
    gpio_conf.pin_bit_mask = (0x1ULL << busy);
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));

    set_rst_1();
}

void CustomLcdDisplay::spi_port_init() {
    int              mosi     = lcd_spi_data.mosi;
    int              scl      = lcd_spi_data.scl;
    int              spi_host = lcd_spi_data.spi_host;
    esp_err_t        ret;
    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num      = -1;
    buscfg.mosi_io_num      = mosi;
    buscfg.sclk_io_num      = scl;
    buscfg.quadwp_io_num    = -1;
    buscfg.quadhd_io_num    = -1;
    buscfg.max_transfer_sz  = Width * Height;

    spi_device_interface_config_t devcfg = {};
    devcfg.spics_io_num                  = -1;
    devcfg.clock_speed_hz                = 20 * 1000 * 1000;
    devcfg.mode                          = 0;
    devcfg.queue_size                    = 7;

    ret = spi_bus_initialize((spi_host_device_t) spi_host, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    ret = spi_bus_add_device((spi_host_device_t) spi_host, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);
}

void CustomLcdDisplay::read_busy() {
    int busy = lcd_spi_data.busy;
    int elapsed = 0;
    while (gpio_get_level((gpio_num_t) busy) == 1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        elapsed += 10;
        if (elapsed > BUSY_TIMEOUT_MS) {
            ESP_LOGE(TAG, "EPD BUSY timeout after %dms, resetting", elapsed);
            EPD_Init();
            return;
        }
    }
}

void CustomLcdDisplay::SPI_SendByte(uint8_t data) {
    esp_err_t         ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length    = 8;
    t.tx_buffer = &data;
    ret         = spi_device_polling_transmit(spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI transmit failed: %s", esp_err_to_name(ret));
    }
}

void CustomLcdDisplay::EPD_SendData(uint8_t data) {
    set_dc_1();
    set_cs_0();
    SPI_SendByte(data);
    set_cs_1();
}

void CustomLcdDisplay::EPD_SendCommand(uint8_t command) {
    set_dc_0();
    set_cs_0();
    SPI_SendByte(command);
    set_cs_1();
}

void CustomLcdDisplay::writeBytes(uint8_t *buffer, int len) {
    set_dc_1();
    set_cs_0();
    esp_err_t         ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length    = 8 * len;
    t.tx_buffer = buffer;
    ret         = spi_device_polling_transmit(spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI writeBytes failed: %s", esp_err_to_name(ret));
    }
    set_cs_1();
}

void CustomLcdDisplay::writeBytes(const uint8_t *buffer, int len) {
    set_dc_1();
    set_cs_0();
    esp_err_t         ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length    = 8 * len;
    t.tx_buffer = buffer;
    ret         = spi_device_polling_transmit(spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI writeBytes const failed: %s", esp_err_to_name(ret));
    }
    set_cs_1();
}

void CustomLcdDisplay::EPD_SetWindows(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend) {
    EPD_SendCommand(0x44);
    EPD_SendData((Xstart >> 3) & 0xFF);
    EPD_SendData((Xend >> 3) & 0xFF);

    EPD_SendCommand(0x45);
    EPD_SendData(Ystart & 0xFF);
    EPD_SendData((Ystart >> 8) & 0xFF);
    EPD_SendData(Yend & 0xFF);
    EPD_SendData((Yend >> 8) & 0xFF);
}

void CustomLcdDisplay::EPD_SetCursor(uint16_t Xstart, uint16_t Ystart) {
    EPD_SendCommand(0x4E);
    EPD_SendData(Xstart & 0xFF);

    EPD_SendCommand(0x4F);
    EPD_SendData(Ystart & 0xFF);
    EPD_SendData((Ystart >> 8) & 0xFF);
}

void CustomLcdDisplay::EPD_SetLut(const uint8_t *lut) {
    EPD_SendCommand(0x32);
    writeBytes(lut, 153);
    read_busy();

    EPD_SendCommand(0x3f);
    EPD_SendData(lut[153]);

    EPD_SendCommand(0x03);
    EPD_SendData(lut[154]);

    EPD_SendCommand(0x04);
    EPD_SendData(lut[155]);
    EPD_SendData(lut[156]);
    EPD_SendData(lut[157]);

    EPD_SendCommand(0x2c);
    EPD_SendData(lut[158]);
}

void CustomLcdDisplay::EPD_TurnOnDisplay() {
    EPD_SendCommand(0x22);
    EPD_SendData(0xc7);
    EPD_SendCommand(0x20);
    read_busy();
}

void CustomLcdDisplay::EPD_TurnOnDisplayPart() {
    EPD_SendCommand(0x22);
    EPD_SendData(0xcf);
    EPD_SendCommand(0x20);
    read_busy();
}

void CustomLcdDisplay::EPD_Init() {
    set_rst_1();
    vTaskDelay(pdMS_TO_TICKS(50));
    set_rst_0();
    vTaskDelay(pdMS_TO_TICKS(20));
    set_rst_1();
    vTaskDelay(pdMS_TO_TICKS(50));

    read_busy();
    EPD_SendCommand(0x12);
    read_busy();

    EPD_SendCommand(0x01);
    EPD_SendData(0xC7);
    EPD_SendData(0x00);
    EPD_SendData(0x01);

    EPD_SendCommand(0x11);
    EPD_SendData(0x01);

    EPD_SetWindows(0, Width - 1, Height - 1, 0);

    EPD_SendCommand(0x3C);
    EPD_SendData(0x01);

    EPD_SendCommand(0x18);
    EPD_SendData(0x80);

    EPD_SendCommand(0x22);
    EPD_SendData(0XB1);
    EPD_SendCommand(0x20);

    EPD_SetCursor(0, Height - 1);
    read_busy();

    EPD_SetLut(WF_Full_1IN54);
}

void CustomLcdDisplay::EPD_Sleep() {
    EPD_SendCommand(0x10);
    EPD_SendData(0x01);
    vTaskDelay(pdMS_TO_TICKS(100));
}

void CustomLcdDisplay::EPD_Clear() {
    int buffer_len = lcd_spi_data.buffer_len;
    memset(buffer, 0xff, buffer_len);
}

void CustomLcdDisplay::EPD_Display() {
    int buffer_len = lcd_spi_data.buffer_len;
    EPD_SendCommand(0x24);
    if (buffer == nullptr) {
        ESP_LOGE(TAG, "EPD_Display: buffer is null");
        return;
    }
    writeBytes(buffer, buffer_len);
    EPD_TurnOnDisplay();
    EPD_Sleep();
}

void CustomLcdDisplay::EPD_DisplayPartBaseImage() {
    int buffer_len = lcd_spi_data.buffer_len;
    EPD_SendCommand(0x24);
    if (buffer == nullptr) {
        ESP_LOGE(TAG, "EPD_DisplayPartBaseImage: buffer is null");
        return;
    }
    writeBytes(buffer, buffer_len);
    EPD_SendCommand(0x26);
    writeBytes(buffer, buffer_len);
    EPD_TurnOnDisplay();
}

void CustomLcdDisplay::EPD_Init_Partial() {
    set_rst_1();
    vTaskDelay(pdMS_TO_TICKS(50));
    set_rst_0();
    vTaskDelay(pdMS_TO_TICKS(20));
    set_rst_1();
    vTaskDelay(pdMS_TO_TICKS(50));

    read_busy();

    EPD_SetLut(WF_PARTIAL_1IN54_0);

    EPD_SendCommand(0x37);
    EPD_SendData(0x00);
    EPD_SendData(0x00);
    EPD_SendData(0x00);
    EPD_SendData(0x00);
    EPD_SendData(0x00);
    EPD_SendData(0x40);
    EPD_SendData(0x00);
    EPD_SendData(0x00);
    EPD_SendData(0x00);
    EPD_SendData(0x00);

    EPD_SendCommand(0x3C);
    EPD_SendData(0x80);

    EPD_SendCommand(0x22);
    EPD_SendData(0xc0);
    EPD_SendCommand(0x20);
    read_busy();
}

void CustomLcdDisplay::EPD_DisplayPart() {
    EPD_SendCommand(0x24);
    if (buffer == nullptr) {
        ESP_LOGE(TAG, "EPD_DisplayPart: buffer is null");
        return;
    }
    writeBytes(buffer, lcd_spi_data.buffer_len);
    EPD_TurnOnDisplayPart();
}

void CustomLcdDisplay::EPD_DrawColorPixel(uint16_t x, uint16_t y, uint8_t color) {
    if (x >= Width || y >= Height) {
        ESP_LOGE("EPD", "Out of bounds pixel: (%d,%d)", x, y);
        return;
    }

    uint16_t index = y * (Width >> 3) + (x >> 3);
    uint8_t  bit   = 7 - (x & 0x07);
    if (color == DRIVER_COLOR_WHITE) {
        buffer[index] |= (0x01 << bit);
    } else {
        buffer[index] &= ~(0x01 << bit);
    }
}

#ifdef CONFIG_WEATHER_ENABLE
void CustomLcdDisplay::UpdateWeather() {
    if (weather_fetching_.exchange(true)) return;
    xTaskCreate([](void* p) {
        auto* self = static_cast<CustomLcdDisplay*>(p);
        self->fetch_weather();
        self->weather_fetching_ = false;
        vTaskDelete(nullptr);
    }, "weather_fetch", 8192, this, 5, nullptr);
}

void CustomLcdDisplay::weather_timer_cb(void* arg) {
    auto* self = static_cast<CustomLcdDisplay*>(arg);
    if (self->weather_init_timer_ != nullptr) {
        esp_timer_delete(self->weather_init_timer_);
        self->weather_init_timer_ = nullptr;
    }
    if (self->weather_fetching_.exchange(true)) return;
    xTaskCreate([](void* p) {
        auto* task_self = static_cast<CustomLcdDisplay*>(p);
        task_self->fetch_weather();
        task_self->weather_fetching_ = false;
        vTaskDelete(nullptr);
    }, "weather_fetch", 8192, self, 5, nullptr);
}

void CustomLcdDisplay::fetch_weather() {
    auto network = Board::GetInstance().GetNetwork();
    if (network == nullptr) return;
    auto http = network->CreateHttp(10);
    if (http == nullptr) return;

    std::string url = "http://wttr.in/" + std::string(CONFIG_WEATHER_CITY) + "?format=%25t%250a%25h%250a%25C";

    http->SetHeader("Accept-Encoding", "identity");
    http->SetTimeout(10000);

    if (!http->Open("GET", url)) {
        ESP_LOGE(TAG, "Weather: HTTP open failed");
        http->Close();
        return;
    }

    if (http->GetStatusCode() != 200) {
        ESP_LOGE(TAG, "Weather: HTTP %d", http->GetStatusCode());
        http->Close();
        return;
    }

    std::string response;
    char read_buf[128];
    int n;
    while ((n = http->Read(read_buf, sizeof(read_buf))) > 0) {
        response.append(read_buf, n);
    }
    http->Close();

    std::string text_buf = response;
    if (!text_buf.empty() && text_buf.back() == '\n') {
        text_buf.pop_back();
    }

    std::string temp_str, hum_str, cond_str;
    size_t pos1 = text_buf.find('\n');
    if (pos1 != std::string::npos) {
        temp_str = text_buf.substr(0, pos1);
        size_t pos2 = text_buf.find('\n', pos1 + 1);
        if (pos2 != std::string::npos) {
            hum_str = text_buf.substr(pos1 + 1, pos2 - pos1 - 1);
            cond_str = text_buf.substr(pos2 + 1);
        } else {
            hum_str = text_buf.substr(pos1 + 1);
        }
    } else {
        temp_str = text_buf;
    }

    std::string line1 = temp_str;
    if (!hum_str.empty()) {
        line1 += "  " + hum_str;
    }

    ESP_LOGI(TAG, "Weather: %s | %s", line1.c_str(), cond_str.c_str());

    if (line1.empty()) {
        ESP_LOGW(TAG, "Weather: empty data, skipping update");
        return;
    }

    {
        DisplayLockGuard lock(this);
        weather_text_ = line1;
        weather_cond_text_ = cond_str;
        weather_valid_ = true;
        weather_dirty_ = true;
    }
}
#endif

void CustomLcdDisplay::SetupUI() {
    LcdDisplay::SetupUI();

    lvgl_port_lock(0);

    if (emoji_box_ != nullptr) {
        lv_obj_set_style_transform_pivot_x(emoji_box_, LV_PCT(50), 0);
        lv_obj_set_style_transform_pivot_y(emoji_box_, LV_PCT(50), 0);
        lv_obj_set_style_transform_scale(emoji_box_, EMOJI_SCALE, 0);
    }

    auto screen = lv_screen_active();

    if (emoji_box_ != nullptr) {
        lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
    }

    clock_label_ = lv_label_create(screen);
    lv_obj_set_style_text_font(clock_label_, &lv_font_montserrat_48, 0);
    lv_label_set_text(clock_label_, "--:--");
    lv_obj_align(clock_label_, LV_ALIGN_CENTER, 0, -30);
    lv_obj_set_style_transform_pivot_x(clock_label_, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(clock_label_, LV_PCT(50), 0);
    lv_obj_set_style_transform_scale(clock_label_, CLOCK_SCALE, 0);
    lv_obj_add_flag(clock_label_, LV_OBJ_FLAG_HIDDEN);

    date_label_ = lv_label_create(screen);
    lv_obj_set_style_text_font(date_label_, lv_obj_get_style_text_font(screen, LV_PART_MAIN), LV_PART_MAIN);
    lv_label_set_text(date_label_, "");
    lv_obj_align(date_label_, LV_ALIGN_CENTER, 0, 14);
    lv_obj_add_flag(date_label_, LV_OBJ_FLAG_HIDDEN);

#ifdef CONFIG_WEATHER_ENABLE
    weather_label_ = lv_label_create(screen);
    lv_obj_set_style_text_font(weather_label_, lv_obj_get_style_text_font(screen, LV_PART_MAIN), LV_PART_MAIN);
    lv_label_set_text(weather_label_, "");
    lv_obj_align(weather_label_, LV_ALIGN_CENTER, 0, 37);
    lv_obj_add_flag(weather_label_, LV_OBJ_FLAG_HIDDEN);

    weather_cond_label_ = lv_label_create(screen);
    lv_obj_set_style_text_font(weather_cond_label_, lv_obj_get_style_text_font(screen, LV_PART_MAIN), LV_PART_MAIN);
    lv_label_set_text(weather_cond_label_, "");
    lv_obj_align(weather_cond_label_, LV_ALIGN_CENTER, 0, 60);
    lv_obj_add_flag(weather_cond_label_, LV_OBJ_FLAG_HIDDEN);

    esp_timer_create_args_t timer_args = {
        .callback = weather_timer_cb,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "weather_timer",
        .skip_unhandled_events = true
    };
    esp_timer_create(&timer_args, &weather_timer_);
    esp_timer_start_periodic(weather_timer_, CONFIG_WEATHER_UPDATE_INTERVAL * 60 * 1000000ULL);

    esp_timer_create_args_t init_args = {
        .callback = weather_timer_cb,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "weather_init",
        .skip_unhandled_events = true
    };
    esp_timer_create(&init_args, &weather_init_timer_);
    esp_timer_start_once(weather_init_timer_, 15 * 1000000ULL);
#endif

    lvgl_port_unlock();
}

void CustomLcdDisplay::UpdateStatusBar(bool update_all) {
    auto& app = Application::GetInstance();
    auto state = app.GetDeviceState();

    if (state == kDeviceStateIdle) {
        {
            DisplayLockGuard lock(this);
            if (emoji_box_ != nullptr && !lv_obj_has_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
            }
        }

        struct tm tm = app.GetCachedTm();
        if (tm.tm_year < (2024 - 1900)) return;

        {
            char time_str[16];
            strftime(time_str, sizeof(time_str), "%H:%M", &tm);
            char date_str[32];
            strftime(date_str, sizeof(date_str), "%a %d %b", &tm);

            if (prev_clock_str_ == time_str && prev_date_str_ == date_str
#ifdef CONFIG_WEATHER_ENABLE
                && (!weather_valid_ || !weather_dirty_)
#endif
                ) {
                return;
            }
            prev_clock_str_ = time_str;
            prev_date_str_ = date_str;

            {
                DisplayLockGuard lock(this);
                lv_label_set_text(clock_label_, time_str);
                lv_label_set_text(date_label_, date_str);
                lv_obj_remove_flag(clock_label_, LV_OBJ_FLAG_HIDDEN);
                lv_obj_remove_flag(date_label_, LV_OBJ_FLAG_HIDDEN);
#ifdef CONFIG_WEATHER_ENABLE
                if (weather_valid_ && weather_dirty_) {
                    lv_label_set_text(weather_label_, weather_text_.c_str());
                    lv_obj_remove_flag(weather_label_, LV_OBJ_FLAG_HIDDEN);
                    if (!weather_cond_text_.empty()) {
                        lv_label_set_text(weather_cond_label_, weather_cond_text_.c_str());
                        lv_obj_remove_flag(weather_cond_label_, LV_OBJ_FLAG_HIDDEN);
                    }
                    weather_dirty_ = false;
                }
#endif
            }
        }
    } else {
        {
            DisplayLockGuard lock(this);
            if (emoji_box_ != nullptr && lv_obj_has_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_remove_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
            }
            lv_obj_add_flag(clock_label_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(date_label_, LV_OBJ_FLAG_HIDDEN);
#ifdef CONFIG_WEATHER_ENABLE
            lv_obj_add_flag(weather_label_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(weather_cond_label_, LV_OBJ_FLAG_HIDDEN);
#endif
        }
        LcdDisplay::UpdateStatusBar(update_all);
    }
}
