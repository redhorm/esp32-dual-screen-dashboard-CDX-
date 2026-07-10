#pragma once
#ifndef NATIVE_BUILD

#include <LovyanGFX.hpp>
#include "core/state.h"
#include "core/race.h"
#include "core/easing.h"
#include "core/ui_settings.h"
#include "services/photo_store.h"

// Dashboard renderer for Display A — ST7735  160×80 landscape
// RC racing style (light blue): wedge RPM bar, framed RPM/MPH readouts,
// red gear box, LAP row, MOTOR/ESC/VOLT row.
// Built-in bitmap fonts: crisp and plenty at 0.96"; the nav uses VLW.
class SceneDashboard {
public:
    explicit SceneDashboard(lgfx::LGFX_Device& disp);
    void init();
    void setSettings(const UiSettings& settings) { _settings = settings; }
    void render(const VehicleState& s, const UiFx& fx, const RunStats& stats);
    void push();

private:
    LGFX_Sprite        _spr;
    lgfx::LGFX_Device& _disp;
    UiSettings          _settings;
    PhotoFrameState     _photoFrame;
    bool                _bootPhotoLoaded = false;

    Follow _rpmF;    // RPM bar smoothing, VU-meter attack/release
    Follow _speedF;  // speed readout smoothing

    // Phase screens
    void _renderBoot(const VehicleState& s, const UiFx& fx);
    void _renderLive(const VehicleState& s, const UiFx& fx,
                     const RunStats& stats);
    void _renderFinish(const VehicleState& s, const UiFx& fx,
                       const RunStats& stats);
    void _renderThemedBoot(const VehicleState& s, const UiFx& fx);
    void _renderThemedLive(const VehicleState& s, const UiFx& fx,
                           const RunStats& stats);
    void _renderThemedFinish(const UiFx& fx, const RunStats& stats);

    // Layout building blocks (all coordinates in Lay:: constants)
    void _drawFrame();
    void _drawRPMBar(float rpm, bool flash_on);
    void _drawSwoosh();
    void _drawFramedValue(int x, int y, int w, int h,
                          const char* value, const lgfx::IFont* val_font,
                          uint16_t val_color, const char* unit,
                          int val_dy = 0);
    void _drawGearBox(int g, const UiFx& fx);
    void _drawMetricBox(int x, int y, int w, int h,
                        const char* value, const char* label,
                        bool degree = false);
    void _drawRows(const VehicleState& s, const RunStats& stats);
    void _drawAlertBanner(const UiFx& fx);
    void _drawCheckerStrip(int y, int h, uint32_t ms);

    // Text helper: centers txt inside (x,y,w,h) with the given font
    void _drawCenteredText(const char* txt, int x, int y, int w, int h,
                           const lgfx::IFont* font, uint16_t color);
};

#endif // NATIVE_BUILD
