#ifndef NATIVE_BUILD
#include <stdio.h>
#include <math.h>
#include "scene_dashboard.h"
#include "core/formatter.h"
#include "core/anim.h"
#include "core/ui_i18n.h"
#include "config.h"

// REALISM / driver display
// A code-native, 160x80 interpretation of the Racepak IQ3S: dark moulded
// housing, cyan LCD, 1-9 tach scale, ten external shift lamps and compact
// race telemetry.  The Classic Rally renderer remains in scene_dashboard.cpp.
namespace {
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

constexpr uint16_t BEZEL      = rgb(18, 21, 22);
constexpr uint16_t BEZEL_HI   = rgb(83, 89, 90);
constexpr uint16_t BEZEL_LO   = rgb(4, 5, 5);
constexpr uint16_t LCD        = rgb(83, 199, 219);
constexpr uint16_t LCD_HI     = rgb(137, 226, 238);
constexpr uint16_t LCD_INK    = rgb(2, 25, 31);
constexpr uint16_t LCD_DIM    = rgb(27, 111, 126);
constexpr uint16_t LED_OFF    = rgb(35, 38, 36);
constexpr uint16_t LED_GREEN  = rgb(44, 255, 79);
constexpr uint16_t LED_YELLOW = rgb(255, 216, 25);
constexpr uint16_t LED_RED    = rgb(255, 34, 20);
constexpr uint16_t LED_EDGE   = rgb(112, 118, 113);

void drawHousing(LGFX_Sprite& spr) {
    spr.fillScreen(BEZEL_LO);
    spr.fillRoundRect(0, 0, 160, 80, 7, BEZEL);
    spr.drawRoundRect(0, 0, 160, 80, 7, BEZEL_HI);
    spr.drawFastHLine(6, 1, 148, rgb(112, 117, 117));
    spr.fillRoundRect(2, 12, 156, 66, 3, BEZEL_LO);
    spr.fillRect(4, 14, 152, 62, LCD);
    spr.drawRect(4, 14, 152, 62, LCD_HI);
}

void drawShiftLamps(LGFX_Sprite& spr, int stable_lit, bool warning,
                    uint32_t now_ms, int forced_lit = -1) {
    int lit = forced_lit >= 0 ? forced_lit : stable_lit;
    if (lit < 0) lit = 0;
    if (lit > 10) lit = 10;
    bool flash = warning && Anim::pulse(now_ms, SHIFT_FLASH_MS);

    // Four warning lamps flank the progressive strip, as on the IQ3S face.
    constexpr int warn_x[4] = {5, 13, 147, 155};
    for (int i = 0; i < 4; ++i) {
        bool on = warning && (i == 0 || i == 3 || flash);
        spr.fillSmoothCircle(warn_x[i], 6, 2, on ? LED_RED : LED_OFF);
        spr.drawCircle(warn_x[i], 6, 2, LED_EDGE);
    }

    for (int i = 0; i < 10; ++i) {
        uint16_t active = i < 5 ? LED_GREEN : (i < 8 ? LED_YELLOW : LED_RED);
        // The progressive strip is intentionally steady.  Warning animation
        // belongs to the four dedicated lamps above, never to the RPM level.
        uint16_t color = i < lit ? active : LED_OFF;
        int x = 21 + i * 12;
        spr.fillRoundRect(x, 3, 9, 7, 2, color);
        spr.drawRoundRect(x, 3, 9, 7, 2, LED_EDGE);
        if (color != LED_OFF) spr.drawFastHLine(x + 2, 4, 5, 0xFFFF);
    }
}

void drawTachScale(LGFX_Sprite& spr, float rpm) {
    spr.setTextDatum(lgfx::TC_DATUM);
    spr.setFont(&lgfx::fonts::Font0);
    spr.setTextColor(LCD_INK, LCD);
    for (int i = 1; i <= 9; ++i) {
        int x = 10 + (i - 1) * 15;
        int d = i > 5 ? i - 5 : 5 - i;
        char n[2] = { static_cast<char>('0' + i), 0 };
        spr.drawString(n, x, 14 + d);
    }
    spr.setTextDatum(lgfx::TR_DATUM);
    spr.drawString("RPM", 153, 15);

    float p = rpm / 9000.0f;
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;
    int lit = static_cast<int>(p * 29.0f + 0.5f);
    int last_x = 6;
    int last_y = 29;
    for (int i = 0; i < 29; ++i) {
        int x = 6 + i * 5;
        int dx = x - 76;
        int y = 24 + (dx * dx) / 980;
        uint16_t col = i < lit ? LCD_INK : LCD_DIM;
        if (i) spr.drawWideLine(last_x, last_y, x, y, 0.7f, col);
        spr.fillRect(x, y - 1, 3, 3, col);
        last_x = x;
        last_y = y;
    }
    spr.setTextDatum(lgfx::TL_DATUM);
}
}

void SceneDashboard::_renderThemedBoot(const VehicleState&, const UiFx& fx) {
    drawHousing(_spr);
    float q = Ease::outCubic(Anim::progress(fx.phase_ms, 0, BOOT_LOGO_MS));
    drawShiftLamps(_spr, 0, false, fx.now_ms,
                   static_cast<int>(q * 10.0f + 0.5f));

    _spr.setTextDatum(lgfx::MC_DATUM);
    _spr.setFont(&lgfx::fonts::Font4);
    _spr.setTextColor(Anim::lerp565(LCD, LCD_INK, q), LCD);
    _spr.drawString("RACEPAK", 80, 35);
    _spr.setFont(&lgfx::fonts::Font2);
    _spr.setTextColor(LCD_INK, LCD);
    _spr.drawString("IQ3S", 80, 57);
    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextColor(LCD_DIM, LCD);
    _spr.drawString(uiText(_settings.language, "LOGGER / CONTROLLO",
                           "LOGGER / SYSTEM CHECK"), 80, 70);
    _spr.setTextDatum(lgfx::TL_DATUM);
}

void SceneDashboard::_renderThemedLive(const VehicleState& s, const UiFx& fx,
                                       const RunStats&) {
    drawHousing(_spr);
    float rpm = _rpmF.update(s.rpm, fx.now_ms, RPM_ATTACK_MS, RPM_RELEASE_MS);
    float speed = _speedF.update(s.speed_mph, fx.now_ms,
                                 SPEED_SMOOTH_MS, SPEED_SMOOTH_MS);
    bool warning = fx.redline || fx.alert || s.engine_temp_f >= 235.0f ||
                   s.battery_v <= 11.2f;
    const int stable_lit = _shiftLights.update(rpm, fx.now_ms);
    drawShiftLamps(_spr, stable_lit, warning, fx.now_ms);
    drawTachScale(_spr, rpm);

    char buf[18];
    // Main IQ3S readouts: speed left, large gear in the centre, fuel right.
    _spr.setTextDatum(lgfx::TC_DATUM);
    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextColor(LCD_DIM, LCD);
    _spr.drawString(_settings.use_kmh ? "KM/H" : "MPH", 31, 32);
    _spr.drawString(uiText(_settings.language, "MARCIA", "GEAR"), 80, 32);
    _spr.drawString(uiText(_settings.language, "CARB", "FUEL"), 130, 32);

    Fmt::speed(buf, speed, _settings.use_kmh);
    _spr.setFont(&lgfx::fonts::Font4);
    _spr.setTextColor(LCD_INK, LCD);
    _spr.drawString(buf, 31, 39);
    Fmt::gear(buf, s.gear, false);
    _spr.drawString(buf, 80, 39);

    int fuel = static_cast<int>(s.fuel_pct * 100.0f + 0.5f);
    if (fuel < 0) fuel = 0;
    if (fuel > 100) fuel = 100;
    snprintf(buf, sizeof(buf), "%d%%", fuel);
    _spr.setFont(&lgfx::fonts::Font2);
    _spr.drawString(buf, 130, 42);
    for (int i = 0; i < 5; ++i)
        _spr.fillRect(112 + i * 8, 57, 6, 3,
                      i * 20 < fuel ? LCD_INK : LCD_DIM);

    // Lower four-channel telemetry ribbon, intentionally dense like the real LCD.
    _spr.drawFastHLine(6, 62, 148, LCD_DIM);
    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextDatum(lgfx::TL_DATUM);
    _spr.setTextColor(LCD_INK, LCD);
    float ect = _settings.use_celsius
              ? (s.engine_temp_f - 32.0f) * (5.0f / 9.0f)
              : s.engine_temp_f;
    float esc = _settings.use_celsius
              ? (s.esc_temp_f - 32.0f) * (5.0f / 9.0f)
              : s.esc_temp_f;
    snprintf(buf, sizeof(buf), "ECT %d", static_cast<int>(ect));
    _spr.drawString(buf, 6, 65);
    snprintf(buf, sizeof(buf), "ESC %d", static_cast<int>(esc));
    _spr.drawString(buf, 47, 65);
    snprintf(buf, sizeof(buf), "%.1fV", s.battery_v);
    _spr.drawString(buf, 89, 65);
    const float odo = _settings.use_kmh ? s.odo_mi * 1.60934f : s.odo_mi;
    snprintf(buf, sizeof(buf), "%.1f%s", odo,
             _settings.use_kmh ? "KM" : "MI");
    _spr.drawString(buf, 122, 65);

    if (fx.alert) {
        uint16_t bg = Anim::pulse(fx.now_ms, ALERT_PULSE_MS) ? LED_RED : LCD_INK;
        _spr.fillRect(4, 62, 152, 14, bg);
        snprintf(buf, sizeof(buf), "%s %.10s",
                 uiText(_settings.language, "ALL", "WARN"),
                 localizedRoadbookText(_settings.language, fx.alert_info));
        _spr.setTextDatum(lgfx::MC_DATUM);
        _spr.setFont(&lgfx::fonts::Font2);
        _spr.setTextColor(0xFFFF, bg);
        _spr.drawString(buf, 80, 69);
        _spr.setTextDatum(lgfx::TL_DATUM);
    }
}

void SceneDashboard::_renderThemedFinish(const UiFx& fx,
                                         const RunStats& stats) {
    drawHousing(_spr);
    drawShiftLamps(_spr, 10, true, fx.now_ms, 10);
    _spr.setTextDatum(lgfx::MC_DATUM);
    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextColor(LCD_DIM, LCD);
    _spr.drawString(uiText(_settings.language, "SESSIONE COMPLETA",
                           "SESSION COMPLETE"), 80, 20);

    char time[14], speed[10], buf[24];
    Fmt::raceTime(time, stats.stage_ms);
    _spr.setFont(&lgfx::fonts::Font4);
    _spr.setTextColor(LCD_INK, LCD);
    _spr.drawString(time, 80, 39);
    Fmt::speed(speed, stats.max_speed_mph, _settings.use_kmh);
    snprintf(buf, sizeof(buf), "MAX %s %s", speed,
             _settings.use_kmh ? "KM/H" : "MPH");
    _spr.setFont(&lgfx::fonts::Font2);
    _spr.drawString(buf, 48, 63);
    snprintf(buf, sizeof(buf), "%d RPM", static_cast<int>(stats.max_rpm));
    _spr.drawString(buf, 124, 63);
    _spr.setTextDatum(lgfx::TL_DATUM);
}

// STOCK OFFROAD / driver display
// Original OEM-style cluster: a premium black face, lime tach scale and amber
// status accents.  It is intentionally distinct from both the Racepak replica
// and the light Classic Rally skin while remaining readable at 160x80.
namespace {
constexpr uint16_t OEM_BG       = rgb(1, 3, 3);
constexpr uint16_t OEM_PANEL    = rgb(9, 13, 13);
constexpr uint16_t OEM_PANEL_2  = rgb(17, 22, 21);
constexpr uint16_t OEM_EDGE     = rgb(69, 79, 73);
constexpr uint16_t OEM_DIM      = rgb(130, 139, 133);
constexpr uint16_t OEM_WHITE    = rgb(242, 245, 240);
constexpr uint16_t OEM_LIME     = rgb(178, 226, 48);
constexpr uint16_t OEM_AMBER    = rgb(255, 166, 24);
constexpr uint16_t OEM_RED      = rgb(239, 48, 37);

void stockDashBase(LGFX_Sprite& spr) {
    spr.fillScreen(OEM_BG);
    spr.fillRoundRect(1, 1, 158, 78, 7, OEM_PANEL);
    spr.drawRoundRect(1, 1, 158, 78, 7, OEM_EDGE);
    spr.drawFastHLine(7, 13, 146, OEM_EDGE);
    spr.fillRoundRect(105, 16, 50, 59, 5, OEM_PANEL_2);
    spr.drawRoundRect(105, 16, 50, 59, 5, OEM_EDGE);
}

void stockTach(LGFX_Sprite& spr, float rpm) {
    float p = rpm / 8000.0f;
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;
    int lit = static_cast<int>(p * 25.0f + 0.5f);
    constexpr float d2r = 0.0174532925f;
    int old_x = 0, old_y = 0;
    for (int i = 0; i < 26; ++i) {
        float a = (140.0f + i * (260.0f / 25.0f)) * d2r;
        int x = 57 + static_cast<int>(cosf(a) * 31.0f);
        int y = 47 + static_cast<int>(sinf(a) * 31.0f);
        uint16_t color = i <= lit
            ? (i > 22 ? OEM_RED : OEM_LIME) : OEM_EDGE;
        if (i) spr.drawWideLine(old_x, old_y, x, y, 1.1f, color);
        int xi = 57 + static_cast<int>(cosf(a) * 27.0f);
        int yi = 47 + static_cast<int>(sinf(a) * 27.0f);
        spr.drawWideLine(x, y, xi, yi, i % 5 == 0 ? 1.2f : 0.7f, color);
        old_x = x;
        old_y = y;
    }
}

void stockTopBar(LGFX_Sprite& spr, const VehicleState& s,
                 const UiSettings& settings) {
    char text[14];
    spr.setFont(&lgfx::fonts::Font0);
    spr.setTextColor(OEM_WHITE, OEM_PANEL);
    spr.setTextDatum(lgfx::TL_DATUM);
    snprintf(text, sizeof(text), "%.1fV", s.battery_v);
    spr.drawString(text, 6, 4);
    char heading[12];
    localizedHeadingFull(settings.language, heading, s.heading_deg);
    spr.setTextDatum(lgfx::TC_DATUM);
    spr.setTextColor(OEM_LIME, OEM_PANEL);
    spr.drawString(heading, 80, 4);
    float temp = settings.use_celsius
        ? (s.engine_temp_f - 32.0f) * (5.0f / 9.0f) : s.engine_temp_f;
    snprintf(text, sizeof(text), "%d%c", static_cast<int>(temp),
             settings.use_celsius ? 'C' : 'F');
    spr.setTextDatum(lgfx::TR_DATUM);
    spr.setTextColor(temp >= (settings.use_celsius ? 110.0f : 230.0f)
                     ? OEM_RED : OEM_WHITE, OEM_PANEL);
    spr.drawString(text, 153, 4);
    spr.setTextDatum(lgfx::TL_DATUM);
}
}

void SceneDashboard::_renderStockBoot(const VehicleState& s, const UiFx& fx) {
    stockDashBase(_spr);
    stockTopBar(_spr, s, _settings);
    float q = Ease::outCubic(Anim::progress(fx.phase_ms, 0, BOOT_LOGO_MS));

    // Original seven-slot off-road grille emblem, drawn from primitives.
    _spr.fillRoundRect(43, 22, 74, 26, 5, OEM_BG);
    _spr.drawRoundRect(43, 22, 74, 26, 5, OEM_EDGE);
    for (int i = 0; i < 7; ++i)
        _spr.fillRoundRect(49 + i * 9, 28, 5, 14, 2,
                           i < static_cast<int>(q * 7.0f + 0.5f)
                           ? OEM_LIME : OEM_EDGE);
    _spr.setTextDatum(lgfx::MC_DATUM);
    _spr.setFont(&lgfx::fonts::Font2);
    _spr.setTextColor(OEM_WHITE, OEM_PANEL);
    _spr.drawString(uiText(_settings.language, "SISTEMA TRAIL", "TRAIL SYSTEM"),
                    80, 58);
    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextColor(OEM_AMBER, OEM_PANEL);
    _spr.drawString(uiText(_settings.language, "CONTROLLO IN CORSO",
                           "SYSTEM CHECK"), 80, 70);
    _spr.setTextDatum(lgfx::TL_DATUM);
}

void SceneDashboard::_renderStockLive(const VehicleState& s, const UiFx& fx,
                                      const RunStats&) {
    stockDashBase(_spr);
    stockTopBar(_spr, s, _settings);
    float rpm = _rpmF.update(s.rpm, fx.now_ms, RPM_ATTACK_MS, RPM_RELEASE_MS);
    float speed = _speedF.update(s.speed_mph, fx.now_ms,
                                 SPEED_SMOOTH_MS, SPEED_SMOOTH_MS);
    stockTach(_spr, rpm);

    char text[18];
    Fmt::speed(text, speed, _settings.use_kmh);
    _spr.setTextDatum(lgfx::MC_DATUM);
    _spr.setFont(&lgfx::fonts::Font4);
    _spr.setTextColor(OEM_WHITE, OEM_PANEL);
    _spr.drawString(text, 57, 42);
    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextColor(OEM_DIM, OEM_PANEL);
    _spr.drawString(_settings.use_kmh ? "KM/H" : "MPH", 57, 58);
    snprintf(text, sizeof(text), "%.1f", rpm / 1000.0f);
    _spr.setTextColor(OEM_LIME, OEM_PANEL);
    _spr.drawString(text, 57, 68);

    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextColor(OEM_DIM, OEM_PANEL_2);
    _spr.drawString(uiText(_settings.language, "MARCIA", "GEAR"), 130, 19);
    Fmt::gear(text, s.gear, false);
    _spr.setFont(&lgfx::fonts::Font4);
    _spr.setTextColor(OEM_AMBER, OEM_PANEL_2);
    _spr.drawString(text, 130, 36);

    int fuel = static_cast<int>(s.fuel_pct * 100.0f + 0.5f);
    if (fuel < 0) fuel = 0;
    if (fuel > 100) fuel = 100;
    snprintf(text, sizeof(text), "%s %d%%",
             uiText(_settings.language, "CARB", "FUEL"), fuel);
    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextColor(fuel < 15 ? OEM_RED : OEM_WHITE, OEM_PANEL_2);
    _spr.drawString(text, 130, 58);
    _spr.fillRoundRect(111, 68, 38, 3, 1, OEM_EDGE);
    _spr.fillRoundRect(111, 68, fuel * 38 / 100, 3, 1,
                       fuel < 15 ? OEM_RED : OEM_LIME);
    _spr.setTextDatum(lgfx::TL_DATUM);

    if (fx.alert) {
        uint16_t border = Anim::pulse(fx.now_ms, ALERT_PULSE_MS)
                        ? OEM_RED : OEM_AMBER;
        _spr.drawRoundRect(1, 1, 158, 78, 7, border);
        _spr.drawRoundRect(2, 2, 156, 76, 6, border);
    }
}

void SceneDashboard::_renderStockFinish(const UiFx&, const RunStats& stats) {
    stockDashBase(_spr);
    _spr.setTextDatum(lgfx::MC_DATUM);
    _spr.setFont(&lgfx::fonts::Font2);
    _spr.setTextColor(OEM_LIME, OEM_PANEL);
    _spr.drawString(uiText(_settings.language, "PROVA COMPLETATA",
                           "TRAIL COMPLETE"), 80, 18);
    char time[14], speed[12], text[28];
    Fmt::raceTime(time, stats.stage_ms);
    _spr.setFont(&lgfx::fonts::Font4);
    _spr.setTextColor(OEM_WHITE, OEM_PANEL);
    _spr.drawString(time, 80, 40);
    Fmt::speed(speed, stats.max_speed_mph, _settings.use_kmh);
    snprintf(text, sizeof(text), "%s %s %s",
             uiText(_settings.language, "MAX", "MAX"), speed,
             _settings.use_kmh ? "KM/H" : "MPH");
    _spr.setFont(&lgfx::fonts::Font2);
    _spr.setTextColor(OEM_AMBER, OEM_PANEL);
    _spr.drawString(text, 80, 63);
    _spr.setTextDatum(lgfx::TL_DATUM);
}
#endif
