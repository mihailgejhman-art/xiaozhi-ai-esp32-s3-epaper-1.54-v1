#include <stdio.h>
#include <driver/i2c_master.h>
#include <esp_log.h>
#include "application.h"
#include "button.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "wifi_board.h"
#include "core_ink_display.h"
#include "lvgl.h"

#define TAG "M5CoreInk"

class CoreInkBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    CoreInkDisplay *display_;

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {};
        i2c_bus_cfg.i2c_port = (i2c_port_t)0;
        i2c_bus_cfg.sda_io_num = AUDIO_CODEC_I2C_SDA_PIN;
        i2c_bus_cfg.scl_io_num = AUDIO_CODEC_I2C_SCL_PIN;
        i2c_bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
        i2c_bus_cfg.glitch_ignore_cnt = 7;
        i2c_bus_cfg.intr_priority = 0;
        i2c_bus_cfg.trans_queue_depth = 0;
        i2c_bus_cfg.flags.enable_internal_pullup = 1;
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto &app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

    void InitializeLcdDisplay() {
        display_ = new CoreInkDisplay(NULL, NULL, EXAMPLE_LCD_WIDTH, EXAMPLE_LCD_HEIGHT,
                                      DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                      DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

public:
    CoreInkBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeButtons();
        InitializeLcdDisplay();
    }

    virtual AudioCodec *GetAudioCodec() override {
        static NoAudioCodecSimplexPdm audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT,
            AUDIO_PDM_CLK_PIN, AUDIO_PDM_DATA_PIN);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override {
        return display_;
    }

    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging) override {
        charging = false;
        discharging = true;
        level = 100;
        return false;
    }
};

DECLARE_BOARD(CoreInkBoard);
