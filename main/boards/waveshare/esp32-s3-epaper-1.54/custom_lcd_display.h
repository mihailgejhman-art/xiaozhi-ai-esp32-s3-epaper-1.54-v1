#ifndef CUSTOM_LCD_DISPLAY_H_
#define CUSTOM_LCD_DISPLAY_H_

#include <atomic>
#include <driver/gpio.h>
#include "lcd_display.h"

/* Display color */
typedef enum {
    DRIVER_COLOR_WHITE  = 0xff,
    DRIVER_COLOR_BLACK  = 0x00,
    FONT_BACKGROUND = DRIVER_COLOR_WHITE,
}COLOR_IMAGE;

typedef struct {
    uint8_t cs;
    uint8_t dc;
    uint8_t rst;
    uint8_t busy;
    uint8_t mosi;
    uint8_t scl;
    int spi_host;
    int buffer_len;
}custom_lcd_spi_t;


class CustomLcdDisplay : public LcdDisplay {
public:
    CustomLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy,custom_lcd_spi_t _lcd_spi_data);
    ~CustomLcdDisplay();

    void EPD_Init();    /* 墨水屏初始化 */
    void EPD_Clear();   /* 清空屏幕 */
    void EPD_Display(); /* 刷buffer到墨水屏 */
    
    /*快速刷新*/
    void EPD_DisplayPartBaseImage();
    void EPD_Init_Partial();
    void EPD_DisplayPart();
    void EPD_DrawColorPixel(uint16_t x, uint16_t y,uint8_t color);
    
private:
    const custom_lcd_spi_t lcd_spi_data;
    const int Width;
    const int Height;
    spi_device_handle_t spi;
    uint8_t *buffer = nullptr;
    lv_obj_t *clock_label_ = nullptr;
    lv_obj_t *date_label_ = nullptr;
    std::string prev_clock_str_;
    std::string prev_date_str_;
#ifdef CONFIG_WEATHER_ENABLE
    lv_obj_t *weather_label_ = nullptr;
    lv_obj_t *weather_cond_label_ = nullptr;
    std::string weather_text_;
    std::string weather_cond_text_;
    std::atomic<bool> weather_valid_{false};
    std::atomic<bool> weather_dirty_{false};
    esp_timer_handle_t weather_timer_ = nullptr;
    esp_timer_handle_t weather_init_timer_ = nullptr;

    std::atomic<bool> weather_fetching_{false};

    static void weather_timer_cb(void* arg);
    void fetch_weather();
#endif
public:
    void UpdateWeather() override;
    
    static void lvgl_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * color_p);
    
    virtual void SetupUI() override;
    virtual void UpdateStatusBar(bool update_all = false) override;

    void spi_gpio_init();
    void spi_port_init();
    void read_busy();

    void set_cs_1(){gpio_set_level(static_cast<gpio_num_t>(lcd_spi_data.cs),1);}
    void set_cs_0(){gpio_set_level(static_cast<gpio_num_t>(lcd_spi_data.cs),0);}
    void set_dc_1(){gpio_set_level(static_cast<gpio_num_t>(lcd_spi_data.dc),1);}
    void set_dc_0(){gpio_set_level(static_cast<gpio_num_t>(lcd_spi_data.dc),0);}
    void set_rst_1(){gpio_set_level(static_cast<gpio_num_t>(lcd_spi_data.rst),1);}
    void set_rst_0(){gpio_set_level(static_cast<gpio_num_t>(lcd_spi_data.rst),0);}

    void SPI_SendByte(uint8_t data);
    void EPD_SendData(uint8_t data);
    void EPD_SendCommand(uint8_t command);
    void writeBytes(uint8_t *buffer,int len);
    void writeBytes(const uint8_t *buffer, int len);
    void EPD_SetWindows(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend);
    void EPD_SetCursor(uint16_t Xstart, uint16_t Ystart);
    void EPD_SetLut(const uint8_t *lut);
    void EPD_TurnOnDisplay();
    void EPD_TurnOnDisplayPart();
};

#endif // CUSTOM_LCD_DISPLAY_H_