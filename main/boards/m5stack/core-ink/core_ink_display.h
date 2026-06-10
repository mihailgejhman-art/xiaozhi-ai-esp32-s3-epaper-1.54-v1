#ifndef __CORE_INK_DISPLAY_H__
#define __CORE_INK_DISPLAY_H__

#include <driver/spi_master.h>
#include "lcd_display.h"

class CoreInkDisplay : public LcdDisplay {
public:
    CoreInkDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                   int width, int height, int offset_x, int offset_y,
                   bool mirror_x, bool mirror_y, bool swap_xy);

    void EPD_Init();
    void EPD_Clear();
    void EPD_Display();
    void EPD_DisplayPartBaseImage();
    void EPD_Init_Partial();
    void EPD_DisplayPart();

private:
    spi_device_handle_t spi_;
    uint8_t *buffer_ = nullptr;
    int width_;
    int height_;

    static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p);

    void spi_init();
    void read_busy();
    void send_command(uint8_t cmd);
    void send_data(uint8_t data);
    void send_data_buffer(const uint8_t *data, int len);
    void reset();
    void wait_busy();
    void set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void turn_on_display();
    void turn_on_display_part();
    void load_lut(const uint8_t *lut);
};

#endif
