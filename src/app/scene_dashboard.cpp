#ifndef NATIVE_BUILD
#include <stdio.h>
#include <string.h>
#include "scene_dashboard.h"
#include "core/formatter.h"
#include "core/anim.h"
#include "core/ui_i18n.h"
#include "config.h"

// ── Palette (RGB565) — racing light-blue theme ──────────────────────────────
namespace Col {
constexpr uint16_t SKY      = 0x4D5D;  // main background
constexpr uint16_t BOX_BG   = 0xAEFE;  // pale box interior
constexpr uint16_t BOX_HI   = 0xD73F;  // gear-box interior (lighter)
constexpr uint16_t INK      = 0x11D0;  // dark navy: values, borders
constexpr uint16_t INK_MID  = 0x2AF5;  // medium blue: labels, box borders
constexpr uint16_t ACCENT   = 0xD904;  // red/orange: big gear letter
constexpr uint16_t SEG_OFF  = 0x3BDA;  // unlit RPM segment (mid blue)
constexpr uint16_t RPM_LOW  = 0x2580;  // RPM gradient start (green)
constexpr uint16_t RPM_MID  = 0xF600;  // RPM gradient middle (yellow)
constexpr uint16_t RPM_HIGH = 0xE082;  // RPM gradient end / limiter (red)
constexpr uint16_t WHITE    = 0xFFFF;
constexpr uint16_t DKRED    = 0x8082;  // alert banner off-pulse
}

// ── Layout constants — canvas 160×80, 2 px safe margin ─────────────────────
namespace Lay {
constexpr int W = 160, H = 80;

// outer frame
constexpr int FRAME_X = 1, FRAME_Y = 1, FRAME_W = 158, FRAME_H = 78, FRAME_R = 4;

// RPM bar: 30 thin gradient steps, wedge-shaped (taller at the left,
// tapering to the right, racing style)
constexpr int BAR_X = 4, BAR_Y = 3, BAR_W = 150;
constexpr int BAR_SEGS = 30;
constexpr int SEG_W = BAR_W / BAR_SEGS;              // 5 px (4 + 1 gap)
constexpr int BAR_H_L = 15, BAR_H_R = 8;             // wedge heights

// framed RPM readout (value + RPM label)
constexpr int RPMBOX_X = 8, RPMBOX_Y = 24, RPMBOX_W = 92, RPMBOX_H = 22;

// gear box right
constexpr int GBOX_X = 112, GBOX_Y = 21, GBOX_W = 40, GBOX_H = 30, GBOX_R = 4;

// middle row: framed MPH / LAP TIME
constexpr int MPH_X = 8,  MPH_Y = 48, MPH_W = 52, MPH_H = 14;
constexpr int LAP_X = 64, LAP_Y = 48, LAP_W = 48, LAP_H = 14;

// bottom row: MOTOR / ESC / VOLTAGE
constexpr int BOT_Y = 63, BOT_H = 14, BOT_W = 48;
constexpr int BOT1_X = 4, BOT2_X = 56, BOT3_X = 108;
constexpr int BOX_R = 3;
}

SceneDashboard::SceneDashboard(lgfx::LGFX_Device& disp)
    : _spr(&disp), _disp(disp) {}

void SceneDashboard::init() {
    _spr.setColorDepth(16);
    _spr.setPsram(false);          // internal RAM → DMA-friendly
    _spr.createSprite(Lay::W, Lay::H);
}

void SceneDashboard::push() {
    _spr.pushSprite(&_disp, 0, 0);
}

void SceneDashboard::setPortalCredentials(bool visible, const char* ssid,
                                          const char* password) {
    // Suppress the overlay while replacing bounded, NUL-terminated copies.
    _portalCredentialsVisible = false;
    snprintf(_portalSsid, sizeof(_portalSsid), "%s", ssid ? ssid : "");
    snprintf(_portalPassword, sizeof(_portalPassword), "%s",
             password ? password : "");
    _portalCredentialsVisible = visible;
}

void SceneDashboard::_renderPortalCredentials() {
    constexpr uint16_t BG     = 0x1082;  // warm graphite
    constexpr uint16_t PANEL  = 0x2104;
    constexpr uint16_t ORANGE = 0xFC40;
    constexpr uint16_t SAND   = 0xE6F3;
    constexpr uint16_t DIM    = 0x9CD3;

    _spr.fillScreen(BG);
    _spr.fillRect(0, 0, Lay::W, 18, ORANGE);
    _spr.drawRect(1, 1, Lay::W - 2, Lay::H - 2, ORANGE);

    _spr.setTextSize(1);
    _spr.setTextDatum(lgfx::MC_DATUM);
    _spr.setFont(&lgfx::fonts::Font2);
    _spr.setTextColor(BG, ORANGE);
    _spr.drawString(uiText(_settings.language, "ABBINA IL MEZZO",
                           "PAIR THIS RIG"), Lay::W / 2, 9);

    _spr.fillRoundRect(5, 20, Lay::W - 10, 20, 3, PANEL);
    _spr.fillRoundRect(5, 42, Lay::W - 10, 20, 3, PANEL);
    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextDatum(lgfx::TC_DATUM);
    _spr.setTextColor(DIM, PANEL);
    _spr.drawString("SSID", Lay::W / 2, 21);
    _spr.drawString(uiText(_settings.language, "PASSWORD", "PASSWORD"),
                    Lay::W / 2, 43);
    _spr.setTextColor(SAND, PANEL);
    _spr.drawString(_portalSsid, Lay::W / 2, 30);
    _spr.drawString(_portalPassword, Lay::W / 2, 52);

    _spr.setTextDatum(lgfx::MC_DATUM);
    _spr.setTextColor(ORANGE, BG);
    _spr.drawString(uiText(_settings.language, "APRI  192.168.4.1",
                           "OPEN  192.168.4.1"), Lay::W / 2, 71);
    _spr.setTextDatum(lgfx::TL_DATUM);
}

// ── Text helper ──────────────────────────────────────────────────────────────

// Centers txt inside the (x,y,w,h) box, transparent background so it
// composes over fills and borders without erasing them.
void SceneDashboard::_drawCenteredText(const char* txt, int x, int y,
                                       int w, int h,
                                       const lgfx::IFont* font,
                                       uint16_t color) {
    _spr.setFont(font);
    _spr.setTextSize(1);
    _spr.setTextColor(color);
    _spr.setTextDatum(lgfx::MC_DATUM);
    _spr.drawString(txt, x + w / 2, y + h / 2);
    _spr.setTextDatum(lgfx::TL_DATUM);
}

// ── Building blocks ──────────────────────────────────────────────────────────

void SceneDashboard::_drawFrame() {
    _spr.fillScreen(Col::SKY);
    _spr.drawRoundRect(Lay::FRAME_X, Lay::FRAME_Y,
                       Lay::FRAME_W, Lay::FRAME_H, Lay::FRAME_R, Col::INK);
}

// RPM bar: 30 thin wedge segments (taller left → thinner right) with a
// green→yellow→red gradient, lit up to the current RPM. At the limiter
// the WHOLE bar strobes red.
void SceneDashboard::_drawRPMBar(float rpm, bool flash_on) {
    using namespace Lay;
    float pct = Anim::clamp01(rpm / SIM_MAX_RPM);
    int   lit = (int)(pct * BAR_SEGS + 0.5f);
    bool  limiter = rpm >= REDLINE_RPM;

    for (int i = 0; i < BAR_SEGS; i++) {
        int x = BAR_X + i * SEG_W;
        // wedge: segment height shrinks linearly left → right
        int h = BAR_H_L - (BAR_H_L - BAR_H_R) * i / (BAR_SEGS - 1);
        uint16_t col;
        if (limiter) {
            col = flash_on ? Col::RPM_HIGH : Col::DKRED;
        } else if (i < lit) {
            float t = (float)i / (BAR_SEGS - 1);
            col = (t < 0.5f)
                ? Anim::lerp565(Col::RPM_LOW, Col::RPM_MID, t * 2.0f)
                : Anim::lerp565(Col::RPM_MID, Col::RPM_HIGH, (t - 0.5f) * 2.0f);
        } else {
            col = Col::SEG_OFF;
        }
        _spr.fillRect(x, BAR_Y, SEG_W - 1, h, col);
    }
    // top edge + slanted bottom edge following the taper
    _spr.drawFastHLine(BAR_X - 1, BAR_Y - 1, BAR_W + 2, Col::INK);
    _spr.drawFastVLine(BAR_X - 1, BAR_Y - 1, BAR_H_L + 2, Col::INK);
    _spr.drawLine(BAR_X - 1, BAR_Y + BAR_H_L,
                  BAR_X + BAR_W, BAR_Y + BAR_H_R, Col::INK);
}

// Two thin racing lines under the bar, parallel to its tapered edge
void SceneDashboard::_drawSwoosh() {
    using namespace Lay;
    int yl = BAR_Y + BAR_H_L;   // wedge bottom at the left
    int yr = BAR_Y + BAR_H_R;   // wedge bottom at the right
    _spr.drawLine(BAR_X - 1, yl + 3, BAR_X + BAR_W, yr + 3, Col::INK);
    _spr.drawLine(BAR_X - 1, yl + 5, BAR_X + BAR_W, yr + 5, Col::INK);
}

// Framed value+unit group, centered inside a rounded box
void SceneDashboard::_drawFramedValue(int x, int y, int w, int h,
                                      const char* value,
                                      const lgfx::IFont* val_font,
                                      uint16_t val_color,
                                      const char* unit, int val_dy) {
    _spr.fillRoundRect(x, y, w, h, Lay::BOX_R, Col::BOX_BG);
    _spr.drawRoundRect(x, y, w, h, Lay::BOX_R, Col::INK);

    _spr.setTextSize(1);
    _spr.setFont(val_font);
    int vw = _spr.textWidth(value);
    _spr.setFont(&lgfx::fonts::Font0);
    int uw = unit[0] ? _spr.textWidth(unit) + 3 : 0;
    int x0 = x + (w - vw - uw) / 2;
    int ym = y + h / 2;                 // vertical center, like _drawMetricBox

    _spr.setTextDatum(lgfx::ML_DATUM);
    _spr.setFont(val_font);
    _spr.setTextColor(val_color);
    _spr.drawString(value, x0, ym + val_dy);
    if (unit[0]) {
        _spr.setFont(&lgfx::fonts::Font0);
        _spr.setTextColor(Col::INK_MID);
        _spr.drawString(unit, x0 + vw + 3, ym);
    }
    _spr.setTextDatum(lgfx::TL_DATUM);
}

// Big gear digit, red on a lighter box — numeric like the navigator
void SceneDashboard::_drawGearBox(int g, const UiFx& fx) {
    using namespace Lay;
    char buf[4];
    Fmt::gear(buf, g, false);

    bool flash_on = fx.redline && Anim::pulse(fx.now_ms, SHIFT_FLASH_MS);
    _spr.fillRoundRect(GBOX_X, GBOX_Y, GBOX_W, GBOX_H, GBOX_R, Col::BOX_HI);
    _spr.drawRoundRect(GBOX_X, GBOX_Y, GBOX_W, GBOX_H, GBOX_R,
                       flash_on ? Col::ACCENT : Col::INK);
    _spr.drawRoundRect(GBOX_X + 1, GBOX_Y + 1, GBOX_W - 2, GBOX_H - 2,
                       GBOX_R - 1, flash_on ? Col::ACCENT : Col::INK);

    // Snap animation on gear change (130% → 100%)
    float scale = Anim::snapScale(fx.gear_snap_p, GEAR_SNAP_SCALE);
    _spr.setFont(&lgfx::fonts::Font4);
    _spr.setTextSize(scale);
    _spr.setTextColor(Col::ACCENT);
    _spr.setTextDatum(lgfx::MC_DATUM);
    _spr.drawString(buf, GBOX_X + GBOX_W / 2, GBOX_Y + GBOX_H / 2 + 1);
    _spr.setTextSize(1);
    _spr.setTextDatum(lgfx::TL_DATUM);
}

// Box with centered value; optional tiny label on the left and optional
// degree ring after the value (Font2 has no ° glyph)
void SceneDashboard::_drawMetricBox(int x, int y, int w, int h,
                                    const char* value, const char* label,
                                    bool degree) {
    _spr.fillRoundRect(x, y, w, h, Lay::BOX_R, Col::BOX_BG);
    _spr.drawRoundRect(x, y, w, h, Lay::BOX_R, Col::INK_MID);

    int lx = x, lw = 0;
    if (label && label[0]) {
        _spr.setFont(&lgfx::fonts::Font0);
        lw = _spr.textWidth(label) + 4;
        _spr.setTextSize(1);
        _spr.setTextColor(Col::INK_MID);
        _spr.setTextDatum(lgfx::ML_DATUM);
        _spr.drawString(label, lx + 3, y + h / 2);
        _spr.setTextDatum(lgfx::TL_DATUM);
    }

    // value centered in the remaining width
    _spr.setFont(&lgfx::fonts::Font2);
    int vw = _spr.textWidth(value);
    int vx = x + lw + (w - lw - vw) / 2;
    _spr.setTextSize(1);
    _spr.setTextColor(Col::INK);
    _spr.setTextDatum(lgfx::ML_DATUM);
    _spr.drawString(value, vx, y + h / 2);
    _spr.setTextDatum(lgfx::TL_DATUM);

    if (degree)   // small ° ring at the value's top-right
        _spr.drawCircle(vx + vw + 2, y + h / 2 - 4, 1, Col::INK);
}

void SceneDashboard::_drawRows(const VehicleState& s, const RunStats& stats) {
    using namespace Lay;
    char buf[10];

    // middle row: framed speed + lap time
    Fmt::speed(buf, _speedF.value, _settings.use_kmh);
    _drawFramedValue(MPH_X, MPH_Y, MPH_W, MPH_H, buf,
                     &lgfx::fonts::Font2, Col::INK,
                     _settings.use_kmh ? "KMH" : "MPH");

    Fmt::raceTime(buf, stats.stage_ms);
    const char* lap = (buf[0] == '0') ? buf + 1 : buf;   // "0:13.4"
    _drawMetricBox(LAP_X, LAP_Y, LAP_W, LAP_H, lap, "");

    // bottom row: motor / esc / battery ("M"/"E": full labels would
    // clip 3-digit temps in a 48 px box)
    float motor_temp = _settings.use_celsius
        ? (s.engine_temp_f - 32.0f) * 5.0f / 9.0f : s.engine_temp_f;
    float esc_temp = _settings.use_celsius
        ? (s.esc_temp_f - 32.0f) * 5.0f / 9.0f : s.esc_temp_f;
    snprintf(buf, sizeof(buf), "%d", (int)motor_temp);
    _drawMetricBox(BOT1_X, BOT_Y, BOT_W, BOT_H, buf, "M", true);

    snprintf(buf, sizeof(buf), "%d", (int)esc_temp);
    _drawMetricBox(BOT2_X, BOT_Y, BOT_W, BOT_H, buf, "E", true);

    snprintf(buf, sizeof(buf), "%.1fV", s.battery_v);
    _drawMetricBox(BOT3_X, BOT_Y, BOT_W, BOT_H, buf, "");
}

// Slides in with ease-out, fades out via lerp565 — never pops.
void SceneDashboard::_drawAlertBanner(const UiFx& fx) {
    bool active = fx.alert;
    if (!active && fx.alert_gone_ms >= ALERT_OUT_MS) return;

    float in_p = active
        ? Ease::outCubic(Anim::progress(fx.alert_age_ms, 0, ALERT_IN_MS))
        : 1.0f;
    float fade = active
        ? 1.0f
        : 1.0f - Anim::clamp01((float)fx.alert_gone_ms / ALERT_OUT_MS);

    const int y0 = Lay::BOT_Y - 3;
    const int bh = Lay::H - y0 - 2;
    int y = y0 + (int)((1.0f - in_p) * (Lay::H - y0));

    bool on = Anim::pulse(fx.now_ms, ALERT_PULSE_MS);
    uint16_t bg     = Anim::lerp565(Col::SKY, on ? Col::ACCENT : Col::DKRED, fade);
    uint16_t border = Anim::lerp565(Col::SKY, Col::WHITE, fade);
    _spr.fillRoundRect(2, y, Lay::W - 4, bh, Lay::BOX_R, bg);
    _spr.drawRoundRect(2, y, Lay::W - 4, bh, Lay::BOX_R, border);

    char msg[32];
    snprintf(msg, sizeof(msg), "%s %s",
             localizedRoadbookText(_settings.language, fx.alert_info),
             localizedRoadbookText(_settings.language, fx.alert_note));
    _drawCenteredText(msg, 2, y, Lay::W - 4, bh, &lgfx::fonts::Font2, border);
}

// Scrolling checkered band, navy/white on the light theme
void SceneDashboard::_drawCheckerStrip(int y, int h, uint32_t ms) {
    _spr.fillRect(0, y, Lay::W, h, Col::WHITE);
    int off = (int)((ms / 80) % (uint32_t)(2 * h));
    for (int x = -2 * h; x < Lay::W; x += 2 * h)
        _spr.fillRect(x + off, y, h, h, Col::INK);
}

// ── Phase screens ────────────────────────────────────────────────────────────

void SceneDashboard::_renderBoot(const VehicleState& s, const UiFx& fx) {
    uint32_t t = fx.phase_ms;

    if (t < BOOT_LOGO_MS) {
        _drawFrame();
        float p = Anim::easeOutCubic(Anim::progress(t, 0, BOOT_LOGO_MS));
        _spr.setFont(&lgfx::fonts::Font4);
        _spr.setTextSize(1);
        _spr.setTextColor(Anim::lerp565(Col::SKY, Col::INK, p));
        _spr.setTextDatum(lgfx::MC_DATUM);
        _spr.drawString("ULTRA4", Lay::W / 2, 32);
        if (p > 0.5f) {
            _spr.setFont(&lgfx::fonts::Font0);
            _spr.setTextColor(Anim::lerp565(Col::SKY, Col::ACCENT,
                                            (p - 0.5f) * 2.0f));
            _spr.drawString("MICRODASH RC", Lay::W / 2, 56);
        }
        _spr.setTextDatum(lgfx::TL_DATUM);

    } else if (t < BOOT_LOGO_MS + BOOT_SWEEP_MS) {
        // Gauge check: RPM bar sweeps 0→max→0
        _drawFrame();
        float sweep = Anim::triangle(Anim::progress(t, BOOT_LOGO_MS, BOOT_SWEEP_MS));
        _drawRPMBar(sweep * (REDLINE_RPM - 100.0f), false);
        _drawSwoosh();
        _drawFramedValue(Lay::RPMBOX_X, Lay::RPMBOX_Y, Lay::RPMBOX_W,
                         Lay::RPMBOX_H, "8888", &lgfx::fonts::Font4,
                         Col::INK, "RPM", 2);
        _drawGearBox(8, fx);
        _drawCenteredText(uiText(_settings.language, "CONTROLLO", "SELF CHECK"),
                          0, Lay::BOT_Y, Lay::W, Lay::BOT_H,
                          &lgfx::fonts::Font0, Col::INK);

    } else {
        _renderLive(s, fx, RunStats{});
        if (Anim::pulse(t, 220)) {
            _spr.fillRoundRect(50, 28, 60, 22, Lay::BOX_R, Col::BOX_BG);
            _spr.drawRoundRect(50, 28, 60, 22, Lay::BOX_R, Col::INK);
            _drawCenteredText(uiText(_settings.language, "PRONTO", "READY"),
                              50, 28, 60, 22,
                              &lgfx::fonts::Font2, Col::INK);
        }
    }
}

void SceneDashboard::_renderLive(const VehicleState& s, const UiFx& fx,
                                 const RunStats& stats) {
    bool live = fx.phase == Phase::LIVE;
    bool flash_on = fx.redline && Anim::pulse(fx.now_ms, SHIFT_FLASH_MS);

    _drawFrame();
    float rpm_bar = _rpmF.update(s.rpm, fx.now_ms,
                                 RPM_ATTACK_MS, RPM_RELEASE_MS);
    _speedF.update(s.speed_mph, fx.now_ms, SPEED_SMOOTH_MS, SPEED_SMOOTH_MS);
    _drawRPMBar(rpm_bar, flash_on);
    _drawSwoosh();
    char rv[8];
    snprintf(rv, sizeof(rv), "%d", (int)s.rpm);   // raw: jitter keeps it alive
    _drawFramedValue(Lay::RPMBOX_X, Lay::RPMBOX_Y, Lay::RPMBOX_W, Lay::RPMBOX_H,
                     rv, &lgfx::fonts::Font4, Col::INK, "RPM", 2);
    _drawGearBox(s.gear, fx);
    _drawRows(s, stats);

    _drawAlertBanner(fx);

    // Redline: frame border flash red/white
    if (fx.redline) {
        uint16_t bc = flash_on ? Col::ACCENT : Col::WHITE;
        _spr.drawRoundRect(Lay::FRAME_X, Lay::FRAME_Y,
                           Lay::FRAME_W, Lay::FRAME_H, Lay::FRAME_R, bc);
        _spr.drawRoundRect(Lay::FRAME_X + 1, Lay::FRAME_Y + 1,
                           Lay::FRAME_W - 2, Lay::FRAME_H - 2,
                           Lay::FRAME_R - 1, bc);
    }

    // BOOT→LIVE reveal: horizontal wipe, coordinated with the navigator
    if (live && fx.phase_ms < LIVE_WIPE_MS) {
        int x = (int)(Lay::W * Ease::inOutQuad(
                    Anim::progress(fx.phase_ms, 0, LIVE_WIPE_MS)));
        if (x < Lay::W) _spr.fillRect(x, 0, Lay::W - x, Lay::H, Col::SKY);
    }
}

void SceneDashboard::_renderFinish(const VehicleState& s, const UiFx& fx,
                                   const RunStats& stats) {
    _drawFrame();

    // Instruments power down in sequence before the result screen
    if (fx.phase_ms < FINISH_SHUTDOWN_MS) {
        float p = (float)fx.phase_ms / FINISH_SHUTDOWN_MS;
        if (p < 0.25f) _drawRows(s, stats);
        if (p < 0.45f) { _drawRPMBar(_rpmF.value, false); _drawSwoosh(); }
        if (p < 0.70f) _drawGearBox(s.gear, fx);
        if (p < 0.85f) {
            char rv[8];
            snprintf(rv, sizeof(rv), "%d", (int)s.rpm);
            _drawFramedValue(Lay::RPMBOX_X, Lay::RPMBOX_Y, Lay::RPMBOX_W,
                             Lay::RPMBOX_H, rv, &lgfx::fonts::Font4,
                             Col::INK, "RPM", 2);
        }
        return;
    }

    _drawCheckerStrip(0, 8, fx.now_ms);
    _drawCheckerStrip(Lay::H - 8, 8, fx.now_ms);

    _drawCenteredText(uiText(_settings.language, "ARRIVO", "FINISH"),
                      0, 12, Lay::W, 26,
                      &lgfx::fonts::Font4, Col::INK);

    char buf[10];
    Fmt::speed(buf, stats.max_speed_mph, _settings.use_kmh);
    _drawMetricBox(6, 46, 70, 22, buf, _settings.use_kmh ? "KMH" : "MPH");
    Fmt::rpm(buf, stats.max_rpm);
    _drawMetricBox(84, 46, 70, 22, buf, "RPM");
}

// ── Dispatcher ───────────────────────────────────────────────────────────────

void SceneDashboard::render(const VehicleState& s, const UiFx& fx,
                            const RunStats& stats) {
    if (_portalCredentialsVisible) {
        _renderPortalCredentials();
        return;
    }
    if (_renderExperienceTakeover(fx)) return;
    if (fx.phase == Phase::BOOT && _settings.boot_image_enabled &&
        fx.phase_ms < _settings.boot_hold_ms && photoStore.hasBoot(false)) {
        if (!_bootPhotoLoaded) _bootPhotoLoaded = photoStore.drawBoot(_spr, false);
        if (_bootPhotoLoaded) return;
    } else if (fx.phase != Phase::BOOT) {
        _bootPhotoLoaded = false;
    }
    if (_settings.photo_mode_enabled && (_settings.photo_target & 0x01) &&
        photoStore.drawFrame(_spr, false, _photoFrame, _settings, fx.now_ms))
        return;

    if (_settings.driver_theme == UiTheme::STOCK_OFFROAD) {
        switch (fx.phase) {
            case Phase::BOOT:   _renderStockBoot(s, fx);        break;
            case Phase::LIVE:   _renderStockLive(s, fx, stats); break;
            case Phase::FINISH: _renderStockFinish(fx, stats);  break;
        }
        return;
    }

    if (_settings.driver_theme == UiTheme::REALISM) {
        switch (fx.phase) {
            case Phase::BOOT:   _renderThemedBoot(s, fx);        break;
            case Phase::LIVE:   _renderThemedLive(s, fx, stats); break;
            case Phase::FINISH: _renderThemedFinish(fx, stats);  break;
        }
        return;
    }

    switch (fx.phase) {
        case Phase::BOOT:   _renderBoot(s, fx);               break;
        case Phase::FINISH: _renderFinish(s, fx, stats);      break;
        case Phase::LIVE:   _renderLive(s, fx, stats);        break;
    }
}

#endif // NATIVE_BUILD
