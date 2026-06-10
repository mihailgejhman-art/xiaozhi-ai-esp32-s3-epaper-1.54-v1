#include "core_ink_display.h"
#include "config.h"
#include <esp_log.h>
#include <esp_check.h>

#define TAG "CoreInkDisplay"

static const uint8_t lut_full[] = {
    0x80, 0x60, 0x40, 0x00, 0x00, 0x00, 0x00,
    0x10, 0x60, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x60, 0x40, 0x00, 0x00, 0x00, 0x00,
    0x10, 0x60, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t lut_partial[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

CoreInkDisplay::CoreInkDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                               int width, int height, int offset_x, int offset_y,
                               bool mirror_x, bool mirror_y, bool swap_xy)
    : LcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy),
      width_(width), height_(height) {

    buffer_ = (uint8_t *)heap_caps_malloc(5000, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    assert(buffer_ != nullptr);
    memset(buffer_, 0xFF, 5000);

    spi_init();
    EPD_Init();

    lv_display_set_user_data(display_, this);
    lv_display_set_flush_cb(display_, lvgl_flush_cb);
    lv_display_set_buffers(display_, buffer_, nullptr, 5000, LV_DISPLAY_RENDER_MODE_DIRECT);
}

void CoreInkDisplay::spi_init() {
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = EPD_MOSI_PIN;
    bus_cfg.miso_io_num = GPIO_NUM_NC;
    bus_cfg.sclk_io_num = EPD_SCK_PIN;
    bus_cfg.quadwp_io_num = GPIO_NUM_NC;
    bus_cfg.quadhd_io_num = GPIO_NUM_NC;
    bus_cfg.max_transfer_sz = 5000;
    ESP_ERROR_CHECK(spi_bus_initialize(EPD_SPI_NUM, &bus_cfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.clock_speed_hz = 40 * 1000 * 1000;
    dev_cfg.mode = 0;
    dev_cfg.spics_io_num = EPD_CS_PIN;
    dev_cfg.queue_size = 1;
    ESP_ERROR_CHECK(spi_bus_add_device(EPD_SPI_NUM, &dev_cfg, &spi_));
}

void CoreInkDisplay::read_busy() {
    while (gpio_get_level((gpio_num_t)EPD_BUSY_PIN) == 1) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void CoreInkDisplay::send_command(uint8_t cmd) {
    gpio_set_level((gpio_num_t)EPD_DC_PIN, 0);
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_buffer = &cmd;
    spi_device_transmit(spi_, &t);
}

void CoreInkDisplay::send_data(uint8_t data) {
    gpio_set_level((gpio_num_t)EPD_DC_PIN, 1);
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_buffer = &data;
    spi_device_transmit(spi_, &t);
}

void CoreInkDisplay::send_data_buffer(const uint8_t *data, int len) {
    gpio_set_level((gpio_num_t)EPD_DC_PIN, 1);
    spi_transaction_t t = {};
    t.length = len * 8;
    t.tx_buffer = data;
    spi_device_transmit(spi_, &t);
}

void CoreInkDisplay::reset() {
    gpio_set_level((gpio_num_t)EPD_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level((gpio_num_t)EPD_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

void CoreInkDisplay::wait_busy() {
    read_busy();
}

void CoreInkDisplay::set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    send_command(0x44);
    send_data((x >> 3) & 0xFF);
    send_data(((x + w - 1) >> 3) & 0xFF);
    send_command(0x45);
    send_data(y & 0xFF);
    send_data((y >> 8) & 0xFF);
    send_data((y + h - 1) & 0xFF);
    send_data(((y + h - 1) >> 8) & 0xFF);
}

void CoreInkDisplay::turn_on_display() {
    send_command(0x22);
    send_data(0xC7);
    send_command(0x20);
    wait_busy();
}

void CoreInkDisplay::turn_on_display_part() {
    send_command(0x22);
    send_data(0x0C);
    send_command(0x20);
    wait_busy();
}

void CoreInkDisplay::load_lut(const uint8_t *lut) {
    send_command(0x32);
    for (int i = 0; i < 153; i++) {
        send_data(lut[i]);
    }
    wait_busy();
}

void CoreInkDisplay::EPD_Init() {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << EPD_CS_PIN) | (1ULL << EPD_DC_PIN) | (1ULL << EPD_RST_PIN);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    gpio_config_t busy_conf = {};
    busy_conf.pin_bit_mask = (1ULL << EPD_BUSY_PIN);
    busy_conf.mode = GPIO_MODE_INPUT;
    busy_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    busy_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    busy_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&busy_conf);

    reset();
    wait_busy();

    send_command(0x12);
    wait_busy();

    send_command(0x01);
    send_data(0xC7);
    send_data(0x00);
    send_data(0x00);

    send_command(0x11);
    send_data(0x01);

    load_lut(lut_full);
}

void CoreInkDisplay::EPD_Clear() {
    send_command(0x24);
    for (int i = 0; i < 5000; i++) {
        send_data(0xFF);
    }
    turn_on_display();
}

void CoreInkDisplay::EPD_Display() {
    send_command(0x24);
    send_data_buffer(buffer_, 5000);
    turn_on_display();
}

void CoreInkDisplay::EPD_DisplayPartBaseImage() {
    send_command(0x24);
    send_data_buffer(buffer_, 5000);
    send_command(0x26);
    send_data_buffer(buffer_, 5000);
}

void CoreInkDisplay::EPD_Init_Partial() {
    send_command(0x2C);
    send_data(0xA8);
    send_command(0x37);
    send_data(0x00);
    send_data(0x00);
    send_data(0x00);
    send_data(0x00);
    send_data(0x00);
    send_data(0x00);
    send_data(0x00);
    send_data(0x00);
    send_data(0x00);
    send_data(0x00);
    send_command(0x3C);
    send_data(0x80);
    send_command(0x22);
    send_data(0xC0);
    send_command(0x20);
    wait_busy();
    load_lut(lut_partial);
}

void CoreInkDisplay::EPD_DisplayPart() {
    send_command(0x24);
    send_data_buffer(buffer_, 5000);
    turn_on_display_part();
}

void CoreInkDisplay::lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p) {
    auto *self = (CoreInkDisplay *)lv_display_get_user_data(disp);
    int w = lv_area_get_width(area);
    int h = lv_area_get_height(area);

    if (w == self->width_ && h == self->height_) {
        for (int y = 0; y < h; y++) {
            uint8_t *buf = (uint8_t *)color_p + y * w * LV_COLOR_DEPTH / 8;
            for (int x = 0; x < w; x++) {
                int byte_idx = (y * self->width_ + x) >> 3;
                int bit_idx = 7 - ((y * self->width_ + x) & 0x7);
                if (buf[0] == 0 && buf[1] == 0 && buf[2] == 0) {
                    self->buffer_[byte_idx] &= ~(1 << bit_idx);
                } else {
                    self->buffer_[byte_idx] |= (1 << bit_idx);
                }
                buf += LV_COLOR_DEPTH / 8;
            }
        }
        self->EPD_Display();
    } else if (w == 200 && h == 200) {
        for (int y = 0; y < h; y++) {
            uint8_t *buf = (uint8_t *)color_p + y * w * LV_COLOR_DEPTH / 8;
            for (int x = 0; x < w; x++) {
                int byte_idx = (y * self->width_ + x) >> 3;
                int bit_idx = 7 - ((y * self->width_ + x) & 0x7);
                if (buf[0] == 0 && buf[1] == 0 && buf[2] == 0) {
                    self->buffer_[byte_idx] &= ~(1 << bit_idx);
                } else {
                    self->buffer_[byte_idx] |= (1 << bit_idx);
                }
                buf += LV_COLOR_DEPTH / 8;
            }
        }
        self->EPD_DisplayPartBaseImage();
        self->EPD_Init_Partial();
        self->EPD_DisplayPart();
    } else {
        for (int y = area->y1; y <= area->y2; y++) {
            uint8_t *buf = (uint8_t *)color_p;
            for (int x = area->x1; x <= area->x2; x++) {
                int byte_idx = (y * self->width_ + x) >> 3;
                int bit_idx = 7 - ((y * self->width_ + x) & 0x7);
                if (buf[0] == 0 && buf[1] == 0 && buf[2] == 0) {
                    self->buffer_[byte_idx] &= ~(1 << bit_idx);
                } else {
                    self->buffer_[byte_idx] |= (1 << bit_idx);
                }
                buf += LV_COLOR_DEPTH / 8;
            }
        }
        self->EPD_DisplayPart();
    }

    lv_display_flush_ready(disp);
}
