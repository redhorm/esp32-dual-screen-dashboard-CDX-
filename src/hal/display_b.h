#pragma once
// Display B — selectable copilot panel on the shared SPI3/HSPI connector.
// Supported: ST7789V 240×240, ST7789V 135×240 landscape, ST7735 128×128.
#ifndef NATIVE_BUILD

#include <LovyanGFX.hpp>
#include "config.h"
#include "copilot_display_profile.h"

class LGFX_DisplayB : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel_st7789;
    lgfx::Panel_ST7735 _panel_st7735;
    lgfx::Bus_SPI      _bus;
    lgfx::Light_PWM   _bl_st7789;
    lgfx::Light_PWM   _bl_st7735;
    CopilotDisplayProfile _profile = copilotDisplayDefaultProfile();
    bool _initialized = false;

    void _configureBus() {
        auto cfg         = _bus.config();
        cfg.spi_host     = SPI3_HOST;
        cfg.spi_mode     = 0;
        cfg.freq_write   = DISP_B_SPI_HZ;
        cfg.freq_read    = 16000000;
        cfg.spi_3wire    = false;
        cfg.use_lock     = true;
        cfg.dma_channel  = SPI_DMA_CH_AUTO;
        cfg.pin_sclk     = DISP_B_SCLK;
        cfg.pin_mosi     = DISP_B_MOSI;
        cfg.pin_miso     = -1;
        cfg.pin_dc       = DISP_B_DC;
        _bus.config(cfg);
        _panel_st7789.setBus(&_bus);
        _panel_st7735.setBus(&_bus);
    }

    template <typename Panel>
    void _configureCommon(Panel& panel, const CopilotDisplayConfig& display) {
        auto cfg              = panel.config();
        cfg.pin_cs            = DISP_B_CS;
        cfg.pin_rst           = DISP_B_RST;
        cfg.pin_busy          = -1;
        cfg.memory_width      = display.memory_width;
        cfg.memory_height     = display.memory_height;
        cfg.panel_width       = display.physical_width;
        cfg.panel_height      = display.physical_height;
        cfg.offset_x          = display.offset_x;
        cfg.offset_y          = display.offset_y;
        cfg.offset_rotation   = 0;
        cfg.dummy_read_pixel  = 8;
        cfg.dummy_read_bits   = 1;
        cfg.readable          = false;
        cfg.invert            = display.invert;
        cfg.rgb_order         = display.rgb_order;
        cfg.dlen_16bit        = false;
        cfg.bus_shared        = false;
        panel.config(cfg);
    }

    void _configureBacklight(lgfx::Panel_Device& panel, lgfx::Light_PWM& light,
                             int pwm_channel) {
        if (DISP_B_BL < 0) return;
        auto cfg        = light.config();
        cfg.pin_bl      = DISP_B_BL;
        cfg.invert      = false;
        cfg.freq        = 44100;
        cfg.pwm_channel = pwm_channel;
        light.config(cfg);
        panel.setLight(&light);
    }

    void _selectPanel() {
        const CopilotDisplayConfig& display = copilotDisplayConfig(_profile);
        if (display.controller == CopilotDisplayController::ST7735) {
            setPanel(&_panel_st7735);
        } else {
            setPanel(&_panel_st7789);
        }
        setRotation(display.rotation);
    }

    void _configureActivePanel() {
        const CopilotDisplayConfig& display = copilotDisplayConfig(_profile);
        if (display.controller == CopilotDisplayController::ST7735) {
            _configureCommon(_panel_st7735, display);
            _configureBacklight(_panel_st7735, _bl_st7735, 1);
        } else {
            _configureCommon(_panel_st7789, display);
            _configureBacklight(_panel_st7789, _bl_st7789, 1);
        }
        _selectPanel();
    }

public:
    LGFX_DisplayB() { _configureBus(); }

    // Must be called before init().  Runtime switching is intentionally not
    // supported because changing a panel while the navigator task is running
    // would invalidate its DMA sprite and could corrupt the SPI transaction.
    bool setProfile(CopilotDisplayProfile profile) {
        if (_initialized) return false;
        _profile = profile;
        return true;
    }

    CopilotDisplayProfile profile() const { return _profile; }
    const CopilotDisplayConfig& displayConfig() const {
        return copilotDisplayConfig(_profile);
    }

    bool init() {
        _configureActivePanel();
        _initialized = lgfx::LGFX_Device::init();
        return _initialized;
    }
};

#endif // NATIVE_BUILD
