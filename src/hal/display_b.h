#pragma once
// Display B — ST7789  240×240  (SPI3_HOST / HSPI)
#ifndef NATIVE_BUILD

#include <LovyanGFX.hpp>
#include "config.h"

class LGFX_DisplayB : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel;
    lgfx::Bus_SPI      _bus;
    lgfx::Light_PWM    _bl;

public:
    LGFX_DisplayB() {
        // ── SPI bus ─────────────────────────────────────────────
        {
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
            _panel.setBus(&_bus);
        }
        // ── Panel ────────────────────────────────────────────────
        {
            auto cfg              = _panel.config();
            cfg.pin_cs            = DISP_B_CS;
            cfg.pin_rst           = DISP_B_RST;
            cfg.pin_busy          = -1;
            // ST7789 GRAM is 240×320; the 240×240 panel uses the top part.
            // memory_height=320 lets LGFX compute the right offsets when
            // the rotation flips the scan direction.
            cfg.memory_width      = 240;
            cfg.memory_height     = 320;
            cfg.panel_width       = 240;
            cfg.panel_height      = 240;
            cfg.offset_x          = 0;
            cfg.offset_y          = 0;
            cfg.offset_rotation   = 0;
            cfg.dummy_read_pixel  = 8;
            cfg.dummy_read_bits   = 1;
            cfg.readable          = false;
            cfg.invert            = DISP_B_INVERT;
            cfg.rgb_order         = DISP_B_RGB_ORDER;
            cfg.dlen_16bit        = false;
            cfg.bus_shared        = false;
            _panel.config(cfg);
        }
        // ── Backlight ────────────────────────────────────────────
        if (DISP_B_BL >= 0) {
            auto cfg        = _bl.config();
            cfg.pin_bl      = DISP_B_BL;
            cfg.invert      = false;
            cfg.freq        = 44100;
            cfg.pwm_channel = 1;
            _bl.config(cfg);
            _panel.setLight(&_bl);
        }

        setPanel(&_panel);
        setRotation(DISP_B_ROTATION);
    }
};

#endif // NATIVE_BUILD
