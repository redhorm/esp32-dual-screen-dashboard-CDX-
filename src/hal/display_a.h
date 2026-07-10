#pragma once
// Display A — ST7735S  160×80  (SPI2_HOST / FSPI)
// Header-only LovyanGFX device class; never compiled in NATIVE_BUILD.
#ifndef NATIVE_BUILD

#include <LovyanGFX.hpp>
#include "config.h"

class LGFX_DisplayA : public lgfx::LGFX_Device {
    lgfx::Panel_ST7735S _panel;
    lgfx::Bus_SPI       _bus;
    lgfx::Light_PWM     _bl;

public:
    LGFX_DisplayA() {
        // ── SPI bus ─────────────────────────────────────────────
        {
            auto cfg         = _bus.config();
            cfg.spi_host     = SPI2_HOST;
            cfg.spi_mode     = 0;
            cfg.freq_write   = DISP_A_SPI_HZ;
            cfg.freq_read    = 16000000;
            cfg.spi_3wire    = false;
            cfg.use_lock     = true;
            cfg.dma_channel  = SPI_DMA_CH_AUTO;
            cfg.pin_sclk     = DISP_A_SCLK;
            cfg.pin_mosi     = DISP_A_MOSI;
            cfg.pin_miso     = -1;
            cfg.pin_dc       = DISP_A_DC;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        // ── Panel ────────────────────────────────────────────────
        {
            auto cfg              = _panel.config();
            cfg.pin_cs            = DISP_A_CS;
            cfg.pin_rst           = DISP_A_RST;
            cfg.pin_busy          = -1;
            // ST7735S GRAM is 132×162. Panel dimensions are given in the
            // module's NATIVE portrait orientation (80 wide × 160 tall);
            // DISP_A_ROTATION=1 then yields the 160×80 landscape UI.
            // The visible window sits at (col_offset, row_offset) in GRAM.
            cfg.memory_width      = 132;
            cfg.memory_height     = 162;
            cfg.panel_width       = 80;
            cfg.panel_height      = 160;
            cfg.offset_x          = DISP_A_COL_OFFSET;
            cfg.offset_y          = DISP_A_ROW_OFFSET;
            cfg.offset_rotation   = 0;
            cfg.dummy_read_pixel  = 8;
            cfg.dummy_read_bits   = 1;
            cfg.readable          = false;
            cfg.invert            = DISP_A_INVERT;
            cfg.rgb_order         = DISP_A_RGB_ORDER;
            cfg.dlen_16bit        = false;
            cfg.bus_shared        = false;
            _panel.config(cfg);
        }
        // ── Backlight (optional PWM) ────────────────────────────
        if (DISP_A_BL >= 0) {
            auto cfg        = _bl.config();
            cfg.pin_bl      = DISP_A_BL;
            cfg.invert      = false;
            cfg.freq        = 44100;
            cfg.pwm_channel = 0;
            _bl.config(cfg);
            _panel.setLight(&_bl);
        }

        setPanel(&_panel);
        setRotation(DISP_A_ROTATION);
    }
};

#endif // NATIVE_BUILD
