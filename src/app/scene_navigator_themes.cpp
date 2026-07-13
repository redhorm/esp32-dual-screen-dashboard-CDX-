#ifndef NATIVE_BUILD
#include <stdio.h>
#include <math.h>
#include "scene_navigator.h"
#include "core/formatter.h"
#include "core/anim.h"
#include "core/ui_i18n.h"
#include "config.h"

// REALISM / navigator display
// Full-bleed onX Offroad-inspired topographic map, built only from LovyanGFX
// primitives.  No bitmap, heap allocation or network map tile is required.
namespace {
constexpr uint16_t rgbn(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

constexpr uint16_t MAP_SAND     = rgbn(225, 218, 194);
constexpr uint16_t MAP_LIGHT    = rgbn(239, 234, 214);
constexpr uint16_t MAP_GREEN    = rgbn(198, 207, 177);
constexpr uint16_t MAP_GREEN_2  = rgbn(181, 196, 159);
constexpr uint16_t CONTOUR      = rgbn(166, 145, 112);
constexpr uint16_t CONTOUR_SOFT = rgbn(197, 181, 151);
constexpr uint16_t WASH         = rgbn(184, 203, 207);
constexpr uint16_t MINOR_ROAD   = rgbn(151, 145, 132);
constexpr uint16_t TRAIL_EDGE   = rgbn(255, 255, 247);
constexpr uint16_t TRAIL_OPEN   = rgbn(35, 143, 76);
constexpr uint16_t TRAIL_CLOSE  = rgbn(207, 47, 39);
constexpr uint16_t ROUTE        = rgbn(238, 101, 24);
constexpr uint16_t ROUTE_DARK   = rgbn(142, 51, 13);
constexpr uint16_t BLUE         = rgbn(16, 112, 243);
constexpr uint16_t BLUE_FAN     = rgbn(100, 173, 250);
constexpr uint16_t INK          = rgbn(28, 31, 30);
constexpr uint16_t DIM          = rgbn(93, 99, 95);
constexpr uint16_t CARD         = rgbn(252, 252, 250);
constexpr uint16_t SHADOW       = rgbn(83, 80, 72);
constexpr uint16_t ONX_YELLOW   = rgbn(255, 205, 0);
constexpr uint16_t WHITE        = 0xFFFF;

struct Pt { int16_t x; int16_t y; };

constexpr Pt ROUTE_PTS[] = {
    {24, 232}, {44, 209}, {63, 196}, {68, 174}, {93, 157},
    {112, 133}, {132, 112}, {145, 84}, {171, 66}, {190, 39}, {217, 20}
};
constexpr Pt OPEN_SPUR[] = {
    {9, 145}, {35, 139}, {56, 122}, {80, 119}, {112, 133},
    {143, 143}, {179, 134}, {224, 144}
};
constexpr Pt CLOSED_SPUR[] = {
    {132, 112}, {158, 109}, {177, 93}, {204, 88}, {235, 68}
};
constexpr Pt WASH_PTS[] = {
    {0, 72}, {27, 84}, {52, 77}, {80, 91}, {101, 87},
    {127, 101}, {159, 97}, {184, 114}, {240, 105}
};

void path(LGFX_Sprite& spr, const Pt* pts, int count, float radius,
          uint16_t color) {
    for (int i = 1; i < count; ++i)
        spr.drawWideLine(pts[i - 1].x, pts[i - 1].y,
                         pts[i].x, pts[i].y, radius, color);
}

void dashedPath(LGFX_Sprite& spr, const Pt* pts, int count, float radius,
                uint16_t color) {
    for (int i = 1; i < count; ++i) {
        int x0 = pts[i - 1].x, y0 = pts[i - 1].y;
        int x1 = pts[i].x, y1 = pts[i].y;
        for (int part = 0; part < 4; part += 2) {
            float a = part * 0.25f;
            float b = (part + 1) * 0.25f;
            spr.drawWideLine(static_cast<int>(x0 + (x1 - x0) * a),
                             static_cast<int>(y0 + (y1 - y0) * a),
                             static_cast<int>(x0 + (x1 - x0) * b),
                             static_cast<int>(y0 + (y1 - y0) * b),
                             radius, color);
        }
    }
}

void routePoint(float progress, int& x, int& y) {
    constexpr int count = sizeof(ROUTE_PTS) / sizeof(ROUTE_PTS[0]);
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 0.999f) progress = 0.999f;
    float scaled = progress * (count - 1);
    int i = static_cast<int>(scaled);
    float f = scaled - i;
    x = static_cast<int>(ROUTE_PTS[i].x +
                         (ROUTE_PTS[i + 1].x - ROUTE_PTS[i].x) * f);
    y = static_cast<int>(ROUTE_PTS[i].y +
                         (ROUTE_PTS[i + 1].y - ROUTE_PTS[i].y) * f);
}

void topoBase(LGFX_Sprite& spr, UiLanguage language) {
    spr.fillScreen(MAP_SAND);

    // Land-management / terrain tinting, deliberately subdued like topo tiles.
    spr.fillTriangle(0, 0, 116, 0, 0, 116, MAP_GREEN);
    spr.fillTriangle(240, 0, 116, 0, 240, 115, MAP_LIGHT);
    spr.fillTriangle(0, 240, 0, 116, 117, 240, MAP_LIGHT);
    spr.fillTriangle(240, 240, 240, 115, 117, 240, MAP_GREEN_2);
    spr.fillTriangle(42, 68, 147, 22, 175, 120, MAP_GREEN_2);
    spr.fillTriangle(42, 68, 175, 120, 75, 158, MAP_GREEN_2);

    // Layered contour rings give convincing elevation without tile assets.
    for (int r = 22; r <= 108; r += 14)
        spr.drawEllipse(26, 30, r, static_cast<int>(r * 0.62f),
                        (r / 14) & 1 ? CONTOUR : CONTOUR_SOFT);
    for (int r = 19; r <= 103; r += 14)
        spr.drawEllipse(198, 168, r, static_cast<int>(r * 0.72f),
                        (r / 14) & 1 ? CONTOUR_SOFT : CONTOUR);
    for (int r = 25; r <= 81; r += 14)
        spr.drawEllipse(91, 210, r, static_cast<int>(r * 0.48f), CONTOUR);
    for (int x = 18; x < 235; x += 36)
        spr.drawFastVLine(x, 0, 240, CONTOUR_SOFT);

    // Dry wash and minor access roads.
    path(spr, WASH_PTS, sizeof(WASH_PTS) / sizeof(WASH_PTS[0]), 4.0f, WASH);
    path(spr, WASH_PTS, sizeof(WASH_PTS) / sizeof(WASH_PTS[0]), 1.0f,
         MAP_LIGHT);
    spr.drawWideLine(0, 186, 238, 112, 1.8f, TRAIL_EDGE);
    spr.drawWideLine(0, 186, 238, 112, 0.8f, MINOR_ROAD);
    spr.drawWideLine(18, 18, 225, 216, 1.5f, TRAIL_EDGE);
    spr.drawWideLine(18, 18, 225, 216, 0.6f, MINOR_ROAD);

    // Official open trail and a clearly closed spur.
    path(spr, OPEN_SPUR, sizeof(OPEN_SPUR) / sizeof(OPEN_SPUR[0]),
         2.8f, TRAIL_EDGE);
    path(spr, OPEN_SPUR, sizeof(OPEN_SPUR) / sizeof(OPEN_SPUR[0]),
         1.5f, TRAIL_OPEN);
    dashedPath(spr, CLOSED_SPUR,
               sizeof(CLOSED_SPUR) / sizeof(CLOSED_SPUR[0]),
               2.8f, TRAIL_EDGE);
    dashedPath(spr, CLOSED_SPUR,
               sizeof(CLOSED_SPUR) / sizeof(CLOSED_SPUR[0]),
               1.5f, TRAIL_CLOSE);

    // Active trail: the outlined route stays above every map layer.
    path(spr, ROUTE_PTS, sizeof(ROUTE_PTS) / sizeof(ROUTE_PTS[0]),
         4.2f, TRAIL_EDGE);
    path(spr, ROUTE_PTS, sizeof(ROUTE_PTS) / sizeof(ROUTE_PTS[0]),
         2.7f, ROUTE_DARK);
    path(spr, ROUTE_PTS, sizeof(ROUTE_PTS) / sizeof(ROUTE_PTS[0]),
         1.7f, ROUTE);

    spr.setFont(&lgfx::fonts::Font0);
    spr.setTextColor(INK);
    spr.drawString("ROCK CREEK", 87, 91);
    spr.setTextColor(TRAIL_OPEN);
    spr.drawString(uiText(language, "APERTA", "OPEN"), 154, 144);
    spr.setTextColor(TRAIL_CLOSE);
    spr.drawString(uiText(language, "CHIUSA", "CLOSED"), 184, 94);
    spr.drawLine(215, 83, 223, 91, TRAIL_CLOSE);
    spr.drawLine(223, 83, 215, 91, TRAIL_CLOSE);

    // Map POI pins.
    constexpr Pt poi[] = {{52, 181}, {144, 84}, {206, 127}};
    for (const auto& p : poi) {
        spr.fillSmoothCircle(p.x, p.y, 5, WHITE);
        spr.fillSmoothCircle(p.x, p.y, 3, ONX_YELLOW);
        spr.drawFastVLine(p.x, p.y + 4, 5, INK);
    }
}

void logoPill(LGFX_Sprite& spr) {
    spr.fillRoundRect(6, 6, 96, 27, 13, INK);
    // Use only guaranteed built-in uppercase glyphs.  The previous lowercase
    // `on` was absent from the reduced device font, leaving a lone X.
    spr.setTextDatum(lgfx::MC_DATUM);
    spr.setFont(&lgfx::fonts::Font2);
    spr.setTextColor(ONX_YELLOW, INK);
    spr.drawString("ON", 25, 19);
    spr.drawString("X", 43, 19);
    spr.setFont(&lgfx::fonts::Font0);
    spr.setTextColor(WHITE, INK);
    spr.drawString("OFFROAD", 72, 19);
    spr.setTextDatum(lgfx::TL_DATUM);
}

void basemapPill(LGFX_Sprite& spr) {
    spr.fillRoundRect(7, 38, 58, 22, 11, SHADOW);
    spr.fillRoundRect(6, 37, 58, 22, 11, CARD);
    spr.fillRect(14, 43, 10, 9, MAP_GREEN_2);
    spr.drawFastHLine(14, 47, 10, CONTOUR);
    spr.setFont(&lgfx::fonts::Font0);
    spr.setTextColor(INK, CARD);
    spr.drawString("TOPO", 29, 44);
}

void compass(LGFX_Sprite& spr, float heading) {
    spr.fillSmoothCircle(218, 24, 18, SHADOW);
    spr.fillSmoothCircle(217, 23, 18, CARD);
    spr.drawCircle(217, 23, 17, rgbn(205, 205, 201));
    spr.setTextDatum(lgfx::TC_DATUM);
    spr.setFont(&lgfx::fonts::Font0);
    spr.setTextColor(INK, CARD);
    spr.drawString("N", 217, 7);
    float a = heading * 0.0174532925f;
    int dx = static_cast<int>(sinf(a) * 11.0f);
    int dy = static_cast<int>(-cosf(a) * 11.0f);
    spr.drawWideLine(217, 23, 217 + dx, 23 + dy, 1.5f, ROUTE);
    spr.fillTriangle(217 + dx, 23 + dy,
                     214 + dx / 2, 24 + dy / 2,
                     220 + dx / 2, 24 + dy / 2, ROUTE);
    spr.setTextDatum(lgfx::TL_DATUM);
}

void toolbar(LGFX_Sprite& spr) {
    spr.fillRoundRect(201, 49, 34, 109, 8, SHADOW);
    spr.fillRoundRect(200, 48, 34, 109, 8, CARD);
    spr.drawFastHLine(200, 84, 34, rgbn(216, 216, 211));
    spr.drawFastHLine(200, 120, 34, rgbn(216, 216, 211));
    spr.setTextDatum(lgfx::MC_DATUM);
    spr.setTextColor(INK, CARD);
    spr.setFont(&lgfx::fonts::Font4);
    spr.drawString("+", 217, 65);
    spr.drawString("-", 217, 100);
    // Layers glyph.
    spr.fillTriangle(217, 129, 207, 136, 227, 136, MAP_GREEN_2);
    spr.drawTriangle(217, 134, 207, 141, 227, 141, INK);
    spr.drawTriangle(217, 140, 207, 147, 227, 147, INK);
    spr.setTextDatum(lgfx::TL_DATUM);
}

void positionMarker(LGFX_Sprite& spr, int x, int y, float heading) {
    float a = heading * 0.0174532925f;
    float dx = sinf(a), dy = -cosf(a);
    float px = -dy, py = dx;
    spr.fillTriangle(x + static_cast<int>(dx * 23.0f),
                     y + static_cast<int>(dy * 23.0f),
                     x + static_cast<int>(px * 10.0f),
                     y + static_cast<int>(py * 10.0f),
                     x - static_cast<int>(px * 10.0f),
                     y - static_cast<int>(py * 10.0f), BLUE_FAN);
    spr.fillSmoothCircle(x, y, 9, WHITE);
    spr.fillSmoothCircle(x, y, 6, BLUE);
    spr.fillSmoothCircle(x - 2, y - 2, 2, WHITE);
}
}

void SceneNavigator::_renderThemedBoot(const UiFx& fx) {
    topoBase(_spr, _settings.language);
    _spr.fillRoundRect(25, 73, 190, 96, 12, SHADOW);
    _spr.fillRoundRect(23, 71, 190, 96, 12, INK);
    float q = Ease::outCubic(Anim::progress(fx.phase_ms, 0, BOOT_TOTAL_MS));

    _spr.setTextDatum(lgfx::MC_DATUM);
    _spr.setFont(&lgfx::fonts::Font4);
    _spr.setTextColor(ONX_YELLOW, INK);
    _spr.drawString("ON", 67, 98);
    _spr.drawString("X", 101, 98);
    _spr.setFont(&_fonts.med);
    _spr.setTextColor(WHITE, INK);
    _spr.drawString("OFFROAD", 145, 100);
    _spr.setFont(&_fonts.small);
    _spr.setTextColor(rgbn(190, 193, 190), INK);
    _spr.drawString(uiText(_settings.language,
                           "MAPPE OFFLINE / RICERCA GPS",
                           "OFFLINE MAPS / GPS ACQUIRING"), 118, 130);
    _spr.fillRoundRect(43, 145, 150, 5, 2, rgbn(63, 67, 65));
    _spr.fillRoundRect(43, 145, static_cast<int>(150.0f * q), 5, 2,
                       ONX_YELLOW);
    _spr.setTextDatum(lgfx::TL_DATUM);
}

void SceneNavigator::_renderThemedLive(const VehicleState& s,
                                       const UiFx& fx) {
    topoBase(_spr, _settings.language);

    float total = _rb.stageTotal();
    float progress = total > 0.0f ? s.odo_mi / total : 0.0f;
    int px, py;
    routePoint(progress, px, py);
    positionMarker(_spr, px, py, s.heading_deg);

    logoPill(_spr);
    basemapPill(_spr);
    compass(_spr, s.heading_deg);
    toolbar(_spr);

    int active = s.active_wp;
    if (active < 0) active = 0;
    if (active >= _rb.getCount()) active = _rb.getCount() - 1;
    const Waypoint& wp = _rb.get(active);

    // onX-style bottom route card: real simulator heading, distance, speed and WP.
    _spr.fillRoundRect(7, 185, 228, 52, 10, SHADOW);
    _spr.fillRoundRect(6, 183, 228, 52, 10, CARD);
    _spr.fillRoundRect(101, 187, 38, 3, 2, rgbn(196, 198, 194));
    _spr.fillRoundRect(13, 191, 52, 14, 7,
                       wp.danger ? TRAIL_CLOSE : TRAIL_OPEN);
    _spr.setTextDatum(lgfx::MC_DATUM);
    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextColor(WHITE, wp.danger ? TRAIL_CLOSE : TRAIL_OPEN);
    _spr.drawString(wp.danger
                    ? uiText(_settings.language, "ATTENZ", "CAUTION")
                    : uiText(_settings.language, "APERTA", "OPEN"), 39, 198);

    char buf[22];
    _spr.setTextDatum(lgfx::TL_DATUM);
    _spr.setFont(&_fonts.med);
    _spr.setTextColor(INK, CARD);
    _spr.drawString(localizedRoadbookText(_settings.language, wp.info), 13, 208);
    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextColor(wp.danger ? TRAIL_CLOSE : DIM, CARD);
    _spr.drawString(wp.note && wp.note[0]
                    ? localizedRoadbookText(_settings.language, wp.note)
                    : uiText(_settings.language, "SENTIERO DESERTO",
                             "DESERT TRAIL"), 14, 225);

    float dist = _distF.update(fx.alert ? fx.alert_dist : s.dist_to_next,
                               fx.now_ms, NAV_VALUE_SMOOTH_MS,
                               NAV_VALUE_SMOOTH_MS);
    char d[10];
    Fmt::distNum(d, dist, _settings.use_kmh);
    snprintf(buf, sizeof(buf), "%s %s", d, _settings.use_kmh ? "KM" : "MI");
    _spr.setTextDatum(lgfx::TR_DATUM);
    _spr.setFont(&_fonts.med);
    _spr.setTextColor(fx.alert ? TRAIL_CLOSE : ROUTE, CARD);
    _spr.drawString(buf, 225, 207);
    float speed = _spdF.update(s.speed_mph, fx.now_ms,
                               NAV_VALUE_SMOOTH_MS, NAV_VALUE_SMOOTH_MS);
    char spd[10];
    Fmt::speed(spd, speed, _settings.use_kmh);
    char hd[12];
    localizedHeadingFull(_settings.language, hd, s.heading_deg);
    snprintf(buf, sizeof(buf), "%s  %s %s", hd, spd,
             _settings.use_kmh ? "KM/H" : "MPH");
    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextColor(DIM, CARD);
    _spr.drawString(buf, 225, 225);
    _spr.setTextDatum(lgfx::TL_DATUM);
}

void SceneNavigator::_renderThemedFinish(const UiFx& fx,
                                         const RunStats& stats) {
    topoBase(_spr, _settings.language);
    logoPill(_spr);
    _spr.fillRoundRect(17, 52, 208, 147, 14, SHADOW);
    _spr.fillRoundRect(15, 50, 208, 147, 14, CARD);
    _spr.fillSmoothCircle(119, 74, 14, TRAIL_OPEN);
    _spr.drawWideLine(111, 74, 117, 80, 1.8f, WHITE);
    _spr.drawWideLine(117, 80, 127, 67, 1.8f, WHITE);

    _spr.setTextDatum(lgfx::MC_DATUM);
    _spr.setFont(&_fonts.med);
    _spr.setTextColor(INK, CARD);
    _spr.drawString(uiText(_settings.language, "TRACCIA SALVA", "TRIP SAVED"),
                    119, 100);
    _spr.setFont(&_fonts.small);
    _spr.setTextColor(DIM, CARD);
    _spr.drawString(uiText(_settings.language, "ULTRA4 / PERCORSO DESERTO",
                           "ULTRA4 / DESERT ROUTE"), 119, 122);

    char time[16], speed[10], buf[28];
    Fmt::raceTime(time, stats.stage_ms);
    _spr.setFont(&_fonts.speed);
    _spr.setTextColor(ROUTE, CARD);
    _spr.drawString(time, 119, 143);
    Fmt::speed(speed, stats.max_speed_mph, _settings.use_kmh);
    const float stage_total = _settings.use_kmh
        ? _rb.stageTotal() * 1.60934f : _rb.stageTotal();
    snprintf(buf, sizeof(buf), "MAX %s %s / %.1f %s", speed,
             _settings.use_kmh ? "KM/H" : "MPH", stage_total,
             _settings.use_kmh ? "KM" : "MI");
    _spr.setFont(&_fonts.small);
    _spr.setTextColor(INK, CARD);
    _spr.drawString(buf, 119, 174);
    _spr.setTextDatum(lgfx::TL_DATUM);
    (void)fx;
}

// STOCK OFFROAD / navigator display
// A code-native OEM instrument page inspired by premium production 4x4s.  It
// uses a restrained black face, a lime tach halo and amber drivetrain data.
namespace {
constexpr uint16_t SN_BG      = rgbn(1, 3, 3);
constexpr uint16_t SN_PANEL   = rgbn(9, 13, 13);
constexpr uint16_t SN_PANEL_2 = rgbn(17, 22, 21);
constexpr uint16_t SN_EDGE    = rgbn(64, 75, 69);
constexpr uint16_t SN_DIM     = rgbn(129, 139, 132);
constexpr uint16_t SN_WHITE   = rgbn(242, 245, 240);
constexpr uint16_t SN_LIME    = rgbn(178, 226, 48);
constexpr uint16_t SN_AMBER   = rgbn(255, 166, 24);
constexpr uint16_t SN_RED     = rgbn(239, 48, 37);

void stockNavBase(LGFX_Sprite& spr) {
    spr.fillScreen(SN_BG);
    spr.fillRoundRect(2, 2, 236, 236, 13, SN_PANEL);
    spr.drawRoundRect(2, 2, 236, 236, 13, SN_EDGE);
    spr.drawFastHLine(12, 27, 216, SN_EDGE);
    spr.drawFastHLine(12, 193, 216, SN_EDGE);
    spr.fillRoundRect(6, 57, 36, 113, 8, SN_PANEL_2);
    spr.fillRoundRect(198, 57, 36, 113, 8, SN_PANEL_2);
    spr.drawRoundRect(6, 57, 36, 113, 8, SN_EDGE);
    spr.drawRoundRect(198, 57, 36, 113, 8, SN_EDGE);
}

void stockNavArc(LGFX_Sprite& spr, float rpm) {
    float p = rpm / 8000.0f;
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;
    int lit = static_cast<int>(p * 32.0f + 0.5f);
    constexpr float d2r = 0.0174532925f;
    int ox = 0, oy = 0;
    for (int i = 0; i < 33; ++i) {
        float a = (140.0f + i * (260.0f / 32.0f)) * d2r;
        int x = 120 + static_cast<int>(cosf(a) * 70.0f);
        int y = 108 + static_cast<int>(sinf(a) * 70.0f);
        uint16_t color = i <= lit
            ? (i > 29 ? SN_RED : SN_LIME) : SN_EDGE;
        if (i) spr.drawWideLine(ox, oy, x, y, 1.5f, color);
        int xi = 120 + static_cast<int>(cosf(a) * (i % 4 ? 64.0f : 60.0f));
        int yi = 108 + static_cast<int>(sinf(a) * (i % 4 ? 64.0f : 60.0f));
        spr.drawWideLine(x, y, xi, yi, i % 4 ? 0.8f : 1.4f, color);
        ox = x;
        oy = y;
    }
    spr.drawCircle(120, 108, 55, SN_EDGE);
    spr.drawCircle(120, 108, 53, SN_BG);
}

void stockVerticalMeter(LGFX_Sprite& spr, int x, int percent,
                        uint16_t color, const char* label) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    spr.setFont(&lgfx::fonts::Font0);
    spr.setTextDatum(lgfx::TC_DATUM);
    spr.setTextColor(SN_DIM, SN_PANEL_2);
    spr.drawString(label, x + 18, 64);
    for (int i = 0; i < 7; ++i) {
        int threshold = (6 - i) * 100 / 7;
        spr.fillRoundRect(x + 10, 80 + i * 10, 16, 6, 2,
                          percent >= threshold ? color : SN_EDGE);
    }
    spr.setTextColor(SN_WHITE, SN_PANEL_2);
    char text[8];
    snprintf(text, sizeof(text), "%d%%", percent);
    spr.drawString(text, x + 18, 154);
    spr.setTextDatum(lgfx::TL_DATUM);
}
}

void SceneNavigator::_renderStockBoot(const UiFx& fx) {
    stockNavBase(_spr);
    float q = Ease::outCubic(Anim::progress(fx.phase_ms, 0, BOOT_TOTAL_MS));
    _spr.fillRoundRect(54, 53, 132, 89, 14, SN_BG);
    _spr.drawRoundRect(54, 53, 132, 89, 14, SN_EDGE);
    for (int i = 0; i < 7; ++i)
        _spr.fillRoundRect(75 + i * 14, 70, 8, 37, 3,
                           i < static_cast<int>(q * 7.0f + 0.5f)
                           ? SN_LIME : SN_EDGE);
    _spr.setTextDatum(lgfx::MC_DATUM);
    _spr.setFont(&_fonts.med);
    _spr.setTextColor(SN_WHITE, SN_PANEL);
    _spr.drawString(uiText(_settings.language, "AUTO DI SERIE",
                           "STOCK OFFROAD"), 120, 163);
    _spr.setFont(&_fonts.small);
    _spr.setTextColor(SN_AMBER, SN_PANEL);
    _spr.drawString(uiText(_settings.language, "CONTROLLO SISTEMI 4X4",
                           "4X4 SYSTEM CHECK"), 120, 184);
    _spr.fillRoundRect(45, 207, 150, 5, 2, SN_EDGE);
    _spr.fillRoundRect(45, 207, static_cast<int>(150.0f * q), 5, 2, SN_LIME);
    _spr.setTextDatum(lgfx::TL_DATUM);
}

void SceneNavigator::_renderStockLive(const VehicleState& s, const UiFx& fx) {
    stockNavBase(_spr);
    float speed = _spdF.update(s.speed_mph, fx.now_ms,
                               NAV_VALUE_SMOOTH_MS, NAV_VALUE_SMOOTH_MS);
    stockNavArc(_spr, s.rpm);

    // OEM top bar: voltage, heading and temperature.
    char text[28];
    _spr.setFont(&_fonts.small);
    _spr.setTextColor(SN_WHITE, SN_PANEL);
    _spr.setTextDatum(lgfx::TL_DATUM);
    snprintf(text, sizeof(text), "%.1fV", s.battery_v);
    _spr.drawString(text, 12, 8);
    char heading[12];
    localizedHeadingFull(_settings.language, heading, s.heading_deg);
    _spr.setTextDatum(lgfx::TC_DATUM);
    _spr.setTextColor(SN_LIME, SN_PANEL);
    _spr.drawString(heading, 120, 8);
    float temp = _settings.use_celsius
        ? (s.engine_temp_f - 32.0f) * (5.0f / 9.0f) : s.engine_temp_f;
    snprintf(text, sizeof(text), "%d%c", static_cast<int>(temp),
             _settings.use_celsius ? 'C' : 'F');
    _spr.setTextDatum(lgfx::TR_DATUM);
    _spr.setTextColor(temp >= (_settings.use_celsius ? 110.0f : 230.0f)
                      ? SN_RED : SN_WHITE, SN_PANEL);
    _spr.drawString(text, 228, 8);

    Fmt::speed(text, speed, _settings.use_kmh);
    _spr.setTextDatum(lgfx::MC_DATUM);
    _spr.setFont(&_fonts.speed);
    _spr.setTextColor(SN_WHITE, SN_BG);
    _spr.drawString(text, 120, 88);
    _spr.setFont(&_fonts.small);
    _spr.setTextColor(SN_DIM, SN_BG);
    _spr.drawString(_settings.use_kmh ? "KM/H" : "MPH", 120, 123);

    char gear[4];
    Fmt::gear(gear, s.gear, false);
    _spr.setFont(&_fonts.med);
    _spr.setTextColor(SN_AMBER, SN_BG);
    _spr.drawString(gear, 120, 145);
    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextColor(SN_DIM, SN_BG);
    _spr.drawString(uiText(_settings.language, "MARCIA", "GEAR"), 120, 166);

    int fuel = static_cast<int>(s.fuel_pct * 100.0f + 0.5f);
    stockVerticalMeter(_spr, 6, fuel, fuel < 15 ? SN_RED : SN_LIME,
                       uiText(_settings.language, "CARB", "FUEL"));
    int temp_pct = static_cast<int>((s.engine_temp_f - 120.0f) * 100.0f / 130.0f);
    stockVerticalMeter(_spr, 198, temp_pct,
                       s.engine_temp_f >= 230.0f ? SN_RED : SN_AMBER,
                       uiText(_settings.language, "TEMP", "TEMP"));

    _spr.fillRoundRect(9, 199, 68, 31, 6, SN_PANEL_2);
    _spr.fillRoundRect(82, 199, 76, 31, 6, SN_PANEL_2);
    _spr.fillRoundRect(163, 199, 68, 31, 6, SN_PANEL_2);
    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextDatum(lgfx::MC_DATUM);
    _spr.setTextColor(SN_LIME, SN_PANEL_2);
    _spr.drawString(s.four_wd_lock ? "4WD LOCK" : "4WD AUTO", 43, 214);
    _spr.setTextColor(SN_WHITE, SN_PANEL_2);
    snprintf(text, sizeof(text), "%d RPM", static_cast<int>(s.rpm));
    _spr.drawString(text, 120, 214);
    float odo = _settings.use_kmh ? s.odo_mi * 1.609344f : s.odo_mi;
    snprintf(text, sizeof(text), "%.1f %s", odo,
             _settings.use_kmh ? "KM" : "MI");
    _spr.setTextColor(SN_AMBER, SN_PANEL_2);
    _spr.drawString(text, 197, 214);
    _spr.setTextDatum(lgfx::TL_DATUM);

    if (fx.alert) {
        uint16_t border = Anim::pulse(fx.now_ms, ALERT_PULSE_MS)
                        ? SN_RED : SN_AMBER;
        _spr.drawRoundRect(2, 2, 236, 236, 13, border);
        _spr.drawRoundRect(3, 3, 234, 234, 12, border);
    }
}

void SceneNavigator::_renderStockFinish(const UiFx&, const RunStats& stats) {
    stockNavBase(_spr);
    _spr.fillSmoothCircle(120, 73, 24, SN_LIME);
    _spr.drawWideLine(108, 73, 117, 82, 2.2f, SN_BG);
    _spr.drawWideLine(117, 82, 134, 61, 2.2f, SN_BG);
    _spr.setTextDatum(lgfx::MC_DATUM);
    _spr.setFont(&_fonts.med);
    _spr.setTextColor(SN_WHITE, SN_PANEL);
    _spr.drawString(uiText(_settings.language, "PROVA COMPLETATA",
                           "TRAIL COMPLETE"), 120, 112);
    char time[16], speed[12], text[30];
    Fmt::raceTime(time, stats.stage_ms);
    _spr.setFont(&_fonts.speed);
    _spr.setTextColor(SN_LIME, SN_PANEL);
    _spr.drawString(time, 120, 142);
    Fmt::speed(speed, stats.max_speed_mph, _settings.use_kmh);
    snprintf(text, sizeof(text), "MAX %s %s", speed,
             _settings.use_kmh ? "KM/H" : "MPH");
    _spr.setFont(&_fonts.med);
    _spr.setTextColor(SN_AMBER, SN_PANEL);
    _spr.drawString(text, 120, 181);
    _spr.setFont(&_fonts.small);
    _spr.setTextColor(SN_DIM, SN_PANEL);
    _spr.drawString(uiText(_settings.language, "DATI SALVATI",
                           "DATA SAVED"), 120, 210);
    _spr.setTextDatum(lgfx::TL_DATUM);
}
#endif
