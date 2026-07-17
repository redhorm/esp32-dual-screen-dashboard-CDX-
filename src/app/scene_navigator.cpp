#ifndef NATIVE_BUILD
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "scene_navigator.h"
#include "core/formatter.h"
#include "core/anim.h"
#include "core/ui_i18n.h"
#include "config.h"

// ── Palette (RGB565) — WAY MAP style ─────────────────────────────────────────
static constexpr uint16_t C_BG     = 0x0000; // black chrome
static constexpr uint16_t C_WHITE  = 0xFFFF;
static constexpr uint16_t C_ROW    = 0xFFFF; // roadbook row background
static constexpr uint16_t C_BLACK  = 0x0000; // ink on white rows
static constexpr uint16_t C_YELLOW = 0xFFE0; // active waypoint
static constexpr uint16_t C_RED    = 0xF800; // danger waypoint
static constexpr uint16_t C_GREENB = 0x07E0; // bright green (GPS dot, 4WD)
static constexpr uint16_t C_GREENT = 0x0480; // dark green text on white
static constexpr uint16_t C_DKGRN  = 0x0320; // SAT LOCK flash background
static constexpr uint16_t C_BLUE   = 0x0339; // water note text
static constexpr uint16_t C_GRAY   = 0x39E7; // separators
static constexpr uint16_t C_LGRAY  = 0x8410; // dim labels
static constexpr uint16_t C_THDR   = 0x2104; // table header background

// ── Layout (240 × 240) ───────────────────────────────────────────────────────
static constexpr int W = 240, H = 240;

static constexpr int HDR_H     = 22;   // header strip
static constexpr int THDR_Y    = 24;   // table column-header row
static constexpr int THDR_H    = 14;
static constexpr int BODY_Y    = 38;   // first roadbook row
static constexpr int ROW_H     = 42;   // row pitch (41 px cell + 1 px gap)
static constexpr int N_ROWS    = 4;    // visible rows
static constexpr int FOOT_Y    = 206;  // footer top

static constexpr int TBL_W     = 172;  // roadbook table width
// Table columns: DIST | ARROW | INFO | TOT
static constexpr int COL_ARR_X  = 46;
static constexpr int COL_INFO_X = 78;
static constexpr int COL_TOT_X  = 138;
static constexpr int CX_DIST    = 23;   // cell centers
static constexpr int CX_ARR     = 62;
static constexpr int CX_INFO    = 108;
static constexpr int CX_TOT     = 155;

static constexpr int SIDE_X    = 172;  // right column separator
static constexpr int SIDE_CX   = 206;  // right column center

static constexpr float AA_STROKE = 1.3f;  // arrow shaft radius (wedge line)

SceneNavigator::SceneNavigator(lgfx::LGFX_Device& disp, const Roadbook& rb)
    : _spr(&disp), _disp(disp), _rb(rb) {}

bool SceneNavigator::init() {
    _spr.setColorDepth(16);
    _spr.setPsram(false);          // internal RAM → DMA-friendly (112.5 KB)
    _ready = _spr.createSprite(W, H) != nullptr;
    if (!_ready) return false;
    _ready = _fonts.init();
    return _ready;
}

void SceneNavigator::push() {
    if (!_ready) return;
    _spr.pushSprite(&_disp, 0, 0);
}

void SceneNavigator::setPortalCredentials(bool visible, const char* ssid,
                                          const char* password) {
    // Suppress the overlay while replacing bounded, NUL-terminated copies.
    _portalCredentialsVisible = false;
    snprintf(_portalSsid, sizeof(_portalSsid), "%s", ssid ? ssid : "");
    snprintf(_portalPassword, sizeof(_portalPassword), "%s",
             password ? password : "");
    _portalCredentialsVisible = visible;
}

void SceneNavigator::_renderPortalCredentials() {
    constexpr uint16_t BG       = 0x1082;  // warm graphite
    constexpr uint16_t PANEL    = 0x2104;
    constexpr uint16_t ORANGE   = 0xFC40;
    constexpr uint16_t SAND     = 0xE6F3;
    constexpr uint16_t DIM      = 0x9CD3;
    constexpr uint16_t DARK_INK = 0x1082;

    _spr.fillScreen(BG);
    _spr.fillRect(0, 0, W, 36, ORANGE);
    _spr.drawRect(3, 3, W - 6, H - 6, ORANGE);

    // Code-native radio arcs: recognizable even before any text is read.
    _spr.drawCircle(W / 2, 69, 24, ORANGE);
    _spr.fillRect(W / 2 - 26, 69, 52, 28, BG);
    _spr.drawCircle(W / 2, 69, 15, ORANGE);
    _spr.fillRect(W / 2 - 17, 69, 34, 19, BG);
    _spr.fillCircle(W / 2, 69, 4, ORANGE);

    _spr.setTextSize(1);
    _spr.setTextDatum(lgfx::MC_DATUM);
    _spr.setFont(&lgfx::fonts::Font4);
    _spr.setTextColor(DARK_INK, ORANGE);
    _spr.drawString(uiText(_settings.language, "ABBINA IL MEZZO",
                           "PAIR THIS RIG"), W / 2, 18);

    _spr.fillRoundRect(12, 96, W - 24, 45, 5, PANEL);
    _spr.fillRoundRect(12, 147, W - 24, 45, 5, PANEL);
    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextDatum(lgfx::TC_DATUM);
    _spr.setTextColor(DIM, PANEL);
    _spr.drawString("SSID", W / 2, 101);
    _spr.drawString("PASSWORD", W / 2, 152);
    _spr.setTextColor(SAND, PANEL);
    _spr.setFont(&lgfx::fonts::Font2);
    if (_spr.textWidth(_portalSsid) > W - 36)
        _spr.setFont(&lgfx::fonts::Font0);
    _spr.drawString(_portalSsid, W / 2, 116);
    _spr.setFont(&lgfx::fonts::Font2);
    if (_spr.textWidth(_portalPassword) > W - 36)
        _spr.setFont(&lgfx::fonts::Font0);
    _spr.drawString(_portalPassword, W / 2, 167);

    _spr.fillRoundRect(30, 205, W - 60, 25, 4, ORANGE);
    _spr.setFont(&lgfx::fonts::Font2);
    _spr.setTextDatum(lgfx::MC_DATUM);
    _spr.setTextColor(DARK_INK, ORANGE);
    _spr.drawString("192.168.4.1", W / 2, 217);
    _spr.setTextDatum(lgfx::TL_DATUM);
}

// ── Anti-aliased helpers ─────────────────────────────────────────────────────

void SceneNavigator::_aaLine(int x0, int y0, int x1, int y1, uint16_t color) {
    _spr.drawWideLine(x0, y0, x1, y1, AA_STROKE, color);
}

// Arc from a0 to a1 (degrees, math convention: 0=east, 90=north)
void SceneNavigator::_aaArc(int cx, int cy, int r, int a0_deg, int a1_deg,
                            uint16_t color) {
    const float d2r = 0.017453f;
    int steps = abs(a1_deg - a0_deg) / 12;
    if (steps < 3) steps = 3;
    float px = cx + r * cosf(a0_deg * d2r);
    float py = cy - r * sinf(a0_deg * d2r);
    for (int i = 1; i <= steps; i++) {
        float a = (a0_deg + (a1_deg - a0_deg) * i / (float)steps) * d2r;
        float nx = cx + r * cosf(a);
        float ny = cy - r * sinf(a);
        _spr.drawWideLine((int)px, (int)py, (int)nx, (int)ny, AA_STROKE, color);
        px = nx; py = ny;
    }
}

void SceneNavigator::_aaTri(int x0, int y0, int x1, int y1, int x2, int y2,
                            uint16_t color) {
    _spr.fillTriangle(x0, y0, x1, y1, x2, y2, color);
    _spr.drawWideLine(x0, y0, x1, y1, 0.6f, color);
    _spr.drawWideLine(x1, y1, x2, y2, 0.6f, color);
    _spr.drawWideLine(x2, y2, x0, y0, 0.6f, color);
}

// ── Direction arrows (pure primitives, AA, drawn on white cell) ─────────────

void SceneNavigator::_drawArrow(int cx, int cy, const Waypoint& wp) {
    // Classic Rally and the REALISM information card both use black route ink.
    const uint16_t ink = C_BLACK;
    uint16_t dot = wp.danger                        ? C_RED
                 : wp.kind == NoteKind::INFO_BLUE   ? C_BLUE
                 : wp.kind == NoteKind::GOOD        ? C_GREENT
                 :                                    ink;

    switch (wp.dir) {

    case WaypointDir::STRAIGHT:
        _aaLine(cx, cy + 10, cx, cy - 4, ink);
        _aaTri(cx, cy - 12, cx - 5, cy - 3, cx + 5, cy - 3, ink);
        _spr.fillSmoothCircle(cx, cy + 12, 3, dot);
        break;

    case WaypointDir::RIGHT_45:
        _aaLine(cx - 3, cy + 10, cx - 3, cy + 2, ink);
        _aaLine(cx - 3, cy + 2,  cx + 4, cy - 5, ink);
        _aaTri(cx + 9, cy - 10, cx + 1, cy - 8, cx + 7, cy - 2, ink);
        _spr.fillSmoothCircle(cx - 3, cy + 12, 3, dot);
        break;

    case WaypointDir::LEFT_45:
        _aaLine(cx + 3, cy + 10, cx + 3, cy + 2, ink);
        _aaLine(cx + 3, cy + 2,  cx - 4, cy - 5, ink);
        _aaTri(cx - 9, cy - 10, cx - 1, cy - 8, cx - 7, cy - 2, ink);
        _spr.fillSmoothCircle(cx + 3, cy + 12, 3, dot);
        break;

    case WaypointDir::RIGHT_90:
        _aaLine(cx - 6, cy + 10, cx - 6, cy + 2, ink);
        _aaArc(cx - 1, cy + 2, 5, 90, 180, ink);   // corner
        _aaLine(cx - 1, cy - 3, cx + 3, cy - 3, ink);
        _aaTri(cx + 10, cy - 3, cx + 2, cy - 8, cx + 2, cy + 2, ink);
        _spr.fillSmoothCircle(cx - 6, cy + 12, 3, dot);
        break;

    case WaypointDir::LEFT_90:
        _aaLine(cx + 6, cy + 10, cx + 6, cy + 2, ink);
        _aaArc(cx + 1, cy + 2, 5, 0, 90, ink);
        _aaLine(cx + 1, cy - 3, cx - 3, cy - 3, ink);
        _aaTri(cx - 10, cy - 3, cx - 2, cy - 8, cx - 2, cy + 2, ink);
        _spr.fillSmoothCircle(cx + 6, cy + 12, 3, dot);
        break;

    case WaypointDir::HAIRPIN_L:
        _aaLine(cx + 5, cy + 10, cx + 5, cy - 2, ink);   // up
        _aaArc(cx, cy - 2, 5, 0, 180, ink);              // U-turn
        _aaLine(cx - 5, cy - 2, cx - 5, cy + 2, ink);    // down
        _aaTri(cx - 5, cy + 9, cx - 9, cy + 1, cx - 1, cy + 1, ink);
        _spr.fillSmoothCircle(cx + 5, cy + 12, 3, dot);
        break;

    case WaypointDir::HAIRPIN_R:
        _aaLine(cx - 5, cy + 10, cx - 5, cy - 2, ink);
        _aaArc(cx, cy - 2, 5, 0, 180, ink);
        _aaLine(cx + 5, cy - 2, cx + 5, cy + 2, ink);
        _aaTri(cx + 5, cy + 9, cx + 1, cy + 1, cx + 9, cy + 1, ink);
        _spr.fillSmoothCircle(cx - 5, cy + 12, 3, dot);
        break;

    case WaypointDir::WATER:
        for (int x = cx - 14; x <= cx + 12; x += 5) {
            _spr.drawFastHLine(x, cy + 5, 3, C_BLUE);
            _spr.drawFastHLine(x + 2, cy + 8, 3, C_BLUE);
        }
        _aaLine(cx, cy + 11, cx, cy - 4, ink);
        _aaTri(cx, cy - 12, cx - 5, cy - 3, cx + 5, cy - 3, ink);
        _spr.fillSmoothCircle(cx + 11, cy + 9, 3, C_BLUE);
        break;

    case WaypointDir::FINISH: {
        _aaLine(cx - 4, cy + 10, cx - 4, cy - 4, ink);
        _aaTri(cx - 4, cy - 12, cx - 9, cy - 3, cx + 1, cy - 3, ink);
        const int fx0 = cx + 3, fy0 = cy - 11, c = 3;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                if ((i + j) % 2 == 0)
                    _spr.fillRect(fx0 + i * c, fy0 + j * c, c, c, ink);
        _spr.drawRect(fx0, fy0, 4 * c, 4 * c, ink);
        _spr.fillSmoothCircle(cx - 4, cy + 12, 3, dot);
        break;
    }
    }
}

// ── Header ───────────────────────────────────────────────────────────────────

void SceneNavigator::_drawHeader(const UiFx& fx) {
    _spr.fillRect(0, 0, W, HDR_H, C_BG);
    _spr.drawFastHLine(0, HDR_H, W, C_GRAY);

    _spr.fillSmoothCircle(8, 11, 3, C_GREENB);
    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextSize(1);
    _spr.setTextColor(C_GREENB, C_BG);
    _spr.setTextDatum(lgfx::TL_DATUM);
    _spr.drawString("GPS", 15, 7);
    char sat[10];
    snprintf(sat, sizeof(sat), "SAT %d", SAT_LOCK_COUNT);
    _spr.setTextColor(C_WHITE, C_BG);
    _spr.drawString(sat, 40, 7);

    _spr.setFont(&lgfx::fonts::Font2);
    _spr.setTextDatum(lgfx::TC_DATUM);
    _spr.drawString(uiText(_settings.language, "MAPPA", "WAY MAP"),
                    W / 2, 3);

    char clk[10];
    Fmt::clockHMS(clk, fx.now_ms / 1000);
    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextDatum(lgfx::TR_DATUM);
    _spr.drawString(clk, W - 4, 7);
    _spr.setTextDatum(lgfx::TL_DATUM);
}

// ── Roadbook table ───────────────────────────────────────────────────────────

void SceneNavigator::_drawRow(int y, const Waypoint& wp, bool active,
                              bool alert_on) {
    const int h = ROW_H - 1;

    _spr.fillRect(0, y, TBL_W, h, C_ROW);
    uint16_t dist_bg, dist_ink;
    if (active)              { dist_bg = C_YELLOW; dist_ink = C_BLACK; }
    else if (wp.danger) {
        dist_bg  = alert_on ? C_WHITE : C_RED;
        dist_ink = alert_on ? C_RED   : C_WHITE;
    }
    else                     { dist_bg = C_ROW;    dist_ink = C_BLACK; }
    if (dist_bg != C_ROW) _spr.fillRect(0, y, COL_ARR_X, h, dist_bg);

    _spr.drawFastVLine(COL_ARR_X,  y, h, C_BLACK);
    _spr.drawFastVLine(COL_INFO_X, y, h, C_BLACK);
    _spr.drawFastVLine(COL_TOT_X,  y, h, C_BLACK);

    const int cy = y + h / 2;

    // DIST — segment distance, big smooth digits
    char buf[10];
    Fmt::distNum(buf, wp.dist_mi, _settings.use_kmh);
    _spr.setFont(&_fonts.med);
    _spr.setTextSize(1);
    _spr.setTextDatum(lgfx::MC_DATUM);
    _spr.setTextColor(dist_ink);
    _spr.drawString(buf, CX_DIST, cy);

    _drawArrow(CX_ARR, y + 19, wp);

    // INFO — main line (+ optional colored note line)
    uint16_t note_col = wp.kind == NoteKind::WARN      ? C_RED
                      : wp.kind == NoteKind::GOOD      ? C_GREENT
                      : wp.kind == NoteKind::INFO_BLUE ? C_BLUE
                      :                                  C_BLACK;
    const char* info = localizedRoadbookText(_settings.language, wp.info);
    const char* note = localizedRoadbookText(_settings.language, wp.note);
    bool has_note   = note && note[0];
    bool short_info = strlen(info) <= 7;
    _spr.setFont(short_info ? &_fonts.med : &_fonts.small);
    _spr.setTextColor(C_BLACK);
    _spr.drawString(info, CX_INFO, has_note ? y + 13 : cy);
    if (has_note) {
        _spr.setFont(&_fonts.small);
        _spr.setTextColor(note_col);
        _spr.drawString(note, CX_INFO, y + 29);
    }

    // TOTAL — cumulative distance
    Fmt::distNum(buf, wp.total_mi, _settings.use_kmh);
    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextColor(C_BLACK, C_ROW);
    _spr.drawString(buf, CX_TOT, cy);
    _spr.setTextDatum(lgfx::TL_DATUM);
}

void SceneNavigator::_drawTable(const VehicleState& s, const UiFx& fx) {
    _spr.fillRect(0, THDR_Y, TBL_W, THDR_H, C_THDR);
    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextSize(1);
    _spr.setTextColor(C_WHITE, C_THDR);
    _spr.setTextDatum(lgfx::TC_DATUM);
    char hd[10];
    snprintf(hd, sizeof(hd), "DIST %s", _settings.use_kmh ? "KM" : "MI");
    _spr.drawString(hd,     CX_DIST, THDR_Y + 3);
    _spr.drawString(uiText(_settings.language, "DIR", "DIR"),
                    CX_ARR, THDR_Y + 3);
    _spr.drawString("INFO", CX_INFO, THDR_Y + 3);
    _spr.drawString(uiText(_settings.language, "TOT", "TOT"),
                    CX_TOT, THDR_Y + 3);
    _spr.setTextDatum(lgfx::TL_DATUM);

    // Scrolling window: active row second from top; changes SLIDE
    // (SlideAnim in core) instead of snapping.
    int first = s.active_wp - 1;
    int max_first = _rb.getCount() - N_ROWS;
    if (first > max_first) first = max_first;
    if (first < 0) first = 0;
    _scroll.setTarget(first, fx.now_ms, SCROLL_MS);
    float pos = _scroll.pos(fx.now_ms);

    bool pulse_on = Anim::pulse(fx.now_ms, ALERT_PULSE_MS);
    _spr.setClipRect(0, BODY_Y, TBL_W, N_ROWS * ROW_H);
    int i0 = (int)floorf(pos);
    for (int i = i0; i <= i0 + N_ROWS && i < _rb.getCount(); i++) {
        if (i < 0) continue;
        int y = BODY_Y + (int)((i - pos) * ROW_H + 0.5f);
        if (y >= BODY_Y + N_ROWS * ROW_H) break;
        bool alert_on = fx.alert && i == fx.alert_wp && pulse_on;
        _drawRow(y, _rb.get(i), i == s.active_wp, alert_on);
    }
    _spr.clearClipRect();
}

// ── Right column: SPEED / GEAR / RPM / 4WD ──────────────────────────────────

void SceneNavigator::_drawSideColumn(const VehicleState& s, const UiFx& fx) {
    _spr.fillRect(SIDE_X, HDR_H + 1, 2, FOOT_Y - HDR_H - 1, C_GRAY);

    float spd = _spdF.update(s.speed_mph, fx.now_ms,
                             NAV_VALUE_SMOOTH_MS, NAV_VALUE_SMOOTH_MS);

    char buf[10];
    _spr.setTextDatum(lgfx::TC_DATUM);

    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextSize(1);
    _spr.setTextColor(C_LGRAY, C_BG);
    _spr.drawString(uiText(_settings.language, "VELOCITA", "SPEED"),
                    SIDE_CX, 27);
    Fmt::speed(buf, spd, _settings.use_kmh);
    _spr.setFont(&_fonts.speed);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(buf, SIDE_CX, 34);
    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextColor(C_LGRAY, C_BG);
    _spr.drawString(_settings.use_kmh ? "km/h" : "MPH", SIDE_CX, 82);

    _spr.drawFastHLine(SIDE_X + 4, 92, W - SIDE_X - 8, C_GRAY);

    _spr.drawString(uiText(_settings.language, "MARCIA", "GEAR"),
                    SIDE_CX, 96);
    Fmt::gear(buf, s.gear, false);   // navigator shows the actual gear number
    _spr.setFont(&_fonts.speed);
    _spr.setTextColor(C_YELLOW);
    _spr.drawString(buf, SIDE_CX, 100);

    _spr.drawFastHLine(SIDE_X + 4, 152, W - SIDE_X - 8, C_GRAY);

    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextColor(C_LGRAY, C_BG);
    _spr.drawString("RPM", SIDE_CX, 156);
    Fmt::rpm(buf, s.rpm);
    _spr.setFont(&_fonts.med);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(buf, SIDE_CX, 166);

    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextColor(s.four_wd_lock ? C_GREENB : C_LGRAY, C_BG);
    _spr.drawString(s.four_wd_lock ? "4WD LOCK" : "4WD AUTO", SIDE_CX, 193);
    _spr.setTextDatum(lgfx::TL_DATUM);
}

// ── Footer: heading + distance to next waypoint ──────────────────────────────

void SceneNavigator::_drawFooter(const VehicleState& s, const UiFx& fx) {
    _spr.fillRect(0, FOOT_Y, W, H - FOOT_Y, C_BG);
    uint16_t bar = fx.alert ? C_RED : C_YELLOW;
    _spr.drawFastHLine(0, FOOT_Y, W, bar);
    _spr.drawFastHLine(0, FOOT_Y + 1, W, bar);

    char buf[14];
    localizedHeadingFull(_settings.language, buf, s.heading_deg);
    _spr.setFont(&_fonts.med);
    _spr.setTextSize(1);
    _spr.setTextColor(C_WHITE);
    _spr.setTextDatum(lgfx::TL_DATUM);
    _spr.drawString(buf, 4, 214);

    // Countdown, smoothed so the 12 FPS refresh never steps visibly;
    // alert distance strobes red/white, normal distance is yellow.
    float target = fx.alert ? fx.alert_dist : s.dist_to_next;
    float dist = _distF.update(target, fx.now_ms,
                               NAV_VALUE_SMOOTH_MS, NAV_VALUE_SMOOTH_MS);
    uint16_t col = C_YELLOW;
    if (fx.alert)
        col = Anim::pulse(fx.now_ms, ALERT_PULSE_MS) ? C_RED : C_WHITE;

    char d[10];
    Fmt::distNum(d, dist, _settings.use_kmh);
    snprintf(buf, sizeof(buf), "%s %s", d, _settings.use_kmh ? "KM" : "MI");
    _spr.setTextColor(col);
    _spr.setTextDatum(lgfx::TR_DATUM);
    _spr.drawString(buf, W - 4, 214);
    _spr.setTextDatum(lgfx::TL_DATUM);
}

// ── Phase screens ────────────────────────────────────────────────────────────

void SceneNavigator::_checkerBand(int y, int h, uint32_t ms) {
    int off = (int)((ms / 90) % (uint32_t)(2 * h));
    for (int x = -2 * h; x < W; x += 2 * h) {
        _spr.fillRect(x + off,     y,     h, h, C_WHITE);
        _spr.fillRect(x + off + h, y + h, h, h, C_WHITE);
    }
}

void SceneNavigator::_renderBoot(const UiFx& fx) {
    uint32_t t = fx.phase_ms;
    if (t < BOOT_NAV_DELAY_MS) return;   // dark until the dash logo lands

    uint32_t acq_end = BOOT_NAV_DELAY_MS + BOOT_NAV_ACQ_MS;
    if (t < acq_end) {
        float p    = Anim::progress(t, BOOT_NAV_DELAY_MS, BOOT_NAV_ACQ_MS);
        int   sats = (int)(p * SAT_LOCK_COUNT + 0.01f);

        char msg[20];
        int dots = (int)((t / 350) % 4);
        snprintf(msg, sizeof(msg), "%s%.*s",
                 uiText(_settings.language, "RICERCA GPS", "GPS ACQUIRING"),
                 dots, "...");
        _spr.setFont(&_fonts.med);
        _spr.setTextSize(1);
        _spr.setTextColor(C_WHITE);
        _spr.setTextDatum(lgfx::TC_DATUM);
        _spr.drawString(msg, W / 2, 36);

        const float d2r = 0.017453f;
        for (int i = 0; i < SAT_LOCK_COUNT; i++) {
            float a  = (90.0f - i * (360.0f / SAT_LOCK_COUNT)) * d2r;
            int   sx = W / 2 + (int)(56.0f * cosf(a));
            int   sy = 142  - (int)(56.0f * sinf(a));
            if (i < sats) _spr.fillSmoothCircle(sx, sy, 4, C_GREENB);
            else          _spr.drawCircle(sx, sy, 3, C_GRAY);
        }

        char cnt[10];
        snprintf(cnt, sizeof(cnt), "%d", sats);
        _spr.setFont(&_fonts.speed);
        _spr.setTextDatum(lgfx::MC_DATUM);
        _spr.setTextColor(C_WHITE);
        _spr.drawString(cnt, W / 2, 142);
        _spr.setFont(&lgfx::fonts::Font0);
        _spr.setTextColor(C_LGRAY, C_BG);
        _spr.setTextDatum(lgfx::TC_DATUM);
        _spr.drawString(uiText(_settings.language, "SATELLITI", "SATELLITES"),
                        W / 2, 176);
        _spr.setTextDatum(lgfx::TL_DATUM);

    } else {
        if (Anim::pulse(t - acq_end, 150))
            _spr.fillScreen(C_DKGRN);
        _spr.setFont(&_fonts.speed);
        _spr.setTextSize(1);
        _spr.setTextColor(C_WHITE);
        _spr.setTextDatum(lgfx::MC_DATUM);
        _spr.drawString(uiText(_settings.language, "GPS AGGANCIATO", "SAT LOCK"),
                        W / 2, 104);
        _spr.setFont(&_fonts.med);
        _spr.setTextColor(C_GREENB);
        _spr.drawString(uiText(_settings.language, "MAPPA PRONTA",
                               "WAY MAP READY"), W / 2, 146);
        _spr.setTextDatum(lgfx::TL_DATUM);
    }
}

void SceneNavigator::_renderLive(const VehicleState& s, const UiFx& fx) {
    _drawHeader(fx);
    _drawTable(s, fx);
    _drawSideColumn(s, fx);
    _drawFooter(s, fx);

    // BOOT→LIVE reveal: same wipe as the dash, lagging NAV_WIPE_LAG_MS
    if (fx.phase_ms < LIVE_WIPE_MS + NAV_WIPE_LAG_MS) {
        int x = (int)(W * Ease::inOutQuad(
                    Anim::progress(fx.phase_ms, NAV_WIPE_LAG_MS, LIVE_WIPE_MS)));
        if (x < W) _spr.fillRect(x, 0, W - x, H, C_BG);
    }
}

void SceneNavigator::_renderFinish(const UiFx& fx, const RunStats& stats) {
    _checkerBand(0,      12, fx.now_ms);
    _checkerBand(H - 24, 12, fx.now_ms);

    _spr.setFont(&_fonts.speed);
    _spr.setTextSize(1);
    _spr.setTextColor(C_WHITE);
    _spr.setTextDatum(lgfx::MC_DATUM);
    _spr.drawString(uiText(_settings.language, "ARRIVO", "FINISH"),
                    W / 2, 70);

    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextColor(C_LGRAY, C_BG);
    _spr.setTextDatum(lgfx::TC_DATUM);
    _spr.drawString(uiText(_settings.language, "TEMPO PROVA", "STAGE TIME"),
                    W / 2, 100);

    char buf[16];
    Fmt::raceTime(buf, stats.stage_ms);
    _spr.setFont(&_fonts.speed);
    _spr.setTextColor(C_YELLOW);
    _spr.drawString(buf, W / 2, 112);

    char spd[10];
    Fmt::speed(spd, stats.max_speed_mph, _settings.use_kmh);
    snprintf(buf, sizeof(buf), "MAX %s %s", spd, _settings.use_kmh ? "KM/H" : "MPH");
    _spr.setFont(&_fonts.med);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(buf, W / 2, 168);

    Fmt::distNum(spd, _rb.stageTotal(), _settings.use_kmh);
    snprintf(buf, sizeof(buf), "%s %s - %d WP", spd,
             _settings.use_kmh ? "KM" : "MI", _rb.getCount());
    _spr.setFont(&_fonts.small);
    _spr.setTextColor(C_LGRAY);
    _spr.drawString(buf, W / 2, 192);
    _spr.setTextDatum(lgfx::TL_DATUM);
}

// ── Dispatcher ───────────────────────────────────────────────────────────────

void SceneNavigator::render(const VehicleState& s, const UiFx& fx,
                            const RunStats& stats) {
    if (!_ready) return;
    if (_portalCredentialsVisible) {
        _renderPortalCredentials();
        return;
    }
    if (_renderExperienceTakeover(fx)) return;
    if (fx.phase == Phase::BOOT && _settings.boot_image_enabled &&
        fx.phase_ms < _settings.boot_hold_ms && photoStore.hasBoot(true)) {
        if (!_bootPhotoLoaded) _bootPhotoLoaded = photoStore.drawBoot(_spr, true);
        if (_bootPhotoLoaded) return;
    } else if (fx.phase != Phase::BOOT) {
        _bootPhotoLoaded = false;
    }
    if (_settings.photo_mode_enabled && (_settings.photo_target & 0x02) &&
        photoStore.drawFrame(_spr, true, _photoFrame, _settings, fx.now_ms))
        return;

    if (_settings.navigator_theme == UiTheme::STOCK_OFFROAD) {
        switch (fx.phase) {
            case Phase::BOOT:   _renderStockBoot(fx);          break;
            case Phase::LIVE:   _renderStockLive(s, fx);       break;
            case Phase::FINISH: _renderStockFinish(fx, stats); break;
        }
        return;
    }

    if (_settings.navigator_theme == UiTheme::REALISM) {
        switch (fx.phase) {
            case Phase::BOOT:   _renderThemedBoot(fx);          break;
            case Phase::LIVE:   _renderThemedLive(s, fx);       break;
            case Phase::FINISH: _renderThemedFinish(fx, stats); break;
        }
        return;
    }

    _spr.fillScreen(C_BG);
    switch (fx.phase) {
        case Phase::BOOT:   _renderBoot(fx);            break;
        case Phase::FINISH: _renderFinish(fx, stats);   break;
        case Phase::LIVE:   _renderLive(s, fx);         break;
    }
}

#endif // NATIVE_BUILD
