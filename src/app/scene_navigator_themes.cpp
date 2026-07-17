#ifndef NATIVE_BUILD
#include <stdio.h>
#include <math.h>
#include "scene_navigator.h"
#include "core/formatter.h"
#include "core/anim.h"
#include "core/ui_i18n.h"
#include "config.h"

// REALISM / navigator display
// Original full-bleed topographic trail map, built only from LovyanGFX
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
constexpr uint16_t MAP_YELLOW   = rgbn(255, 205, 0);
constexpr uint16_t WHITE        = 0xFFFF;

struct Pt { int16_t x; int16_t y; };

constexpr Pt ROUTE_PTS[] = {
    {24, 158}, {44, 150}, {63, 142}, {68, 130}, {93, 116},
    {112, 101}, {132, 87}, {145, 68}, {171, 53}, {190, 36}, {217, 20}
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
    int thickness = radius >= 2.0f ? 3 : 1;
    for (int i = 1; i < count; ++i) {
        for (int offset = -(thickness / 2); offset <= thickness / 2; ++offset)
            spr.drawLine(pts[i - 1].x + offset, pts[i - 1].y,
                         pts[i].x + offset, pts[i].y, color);
    }
}

void dashedPath(LGFX_Sprite& spr, const Pt* pts, int count, float radius,
                uint16_t color) {
    for (int i = 1; i < count; ++i) {
        int x0 = pts[i - 1].x, y0 = pts[i - 1].y;
        int x1 = pts[i].x, y1 = pts[i].y;
        for (int part = 0; part < 4; part += 2) {
            float a = part * 0.25f;
            float b = (part + 1) * 0.25f;
            int ax = static_cast<int>(x0 + (x1 - x0) * a);
            int ay = static_cast<int>(y0 + (y1 - y0) * a);
            int bx = static_cast<int>(x0 + (x1 - x0) * b);
            int by = static_cast<int>(y0 + (y1 - y0) * b);
            int thickness = radius >= 2.0f ? 3 : 1;
            for (int offset = -(thickness / 2); offset <= thickness / 2; ++offset)
                spr.drawLine(ax + offset, ay, bx + offset, by, color);
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

    // Precomputed contour polylines replace large clipped ellipses. On the
    // ESP32 those ellipses dominated the whole frame (~380 ms); this version
    // retains the map character with a bounded number of cheap line draws.
    constexpr Pt topo_contours[][8] = {
        {{0,31},{27,19},{54,25},{79,42},{111,37},{145,20},{181,25},{239,17}},
        {{0,54},{29,41},{58,48},{84,64},{116,59},{150,42},{184,48},{239,40}},
        {{0,80},{31,67},{62,73},{89,91},{122,84},{157,67},{192,74},{239,65}},
        {{0,109},{29,96},{59,102},{91,120},{127,113},{161,95},{197,102},{239,94}},
        {{0,139},{32,126},{64,133},{97,151},{132,144},{167,126},{203,134},{239,126}},
        {{0,170},{29,158},{62,164},{96,183},{132,176},{169,157},{204,165},{239,157}},
        {{0,201},{31,190},{65,196},{99,215},{135,208},{171,189},{207,197},{239,190}},
        {{0,228},{35,218},{70,224},{105,238},{140,232},{177,215},{211,222},{239,216}}
    };
    for (int row = 0; row < 8; row += 2)
        for (int i = 1; i < 8; ++i)
            spr.drawLine(topo_contours[row][i - 1].x,
                         topo_contours[row][i - 1].y,
                         topo_contours[row][i].x,
                         topo_contours[row][i].y,
                         row & 1 ? CONTOUR : CONTOUR_SOFT);

    // Two inexpensive access roads provide enough geographic context at
    // 240 px; secondary washes/spurs are intentionally omitted in live view.
    spr.drawLine(0, 186, 238, 112, MINOR_ROAD);
    spr.drawLine(18, 18, 225, 216, MINOR_ROAD);

    // Active trail: the outlined route stays above every map layer.
    path(spr, ROUTE_PTS, sizeof(ROUTE_PTS) / sizeof(ROUTE_PTS[0]),
         2.2f, TRAIL_EDGE);
    path(spr, ROUTE_PTS, sizeof(ROUTE_PTS) / sizeof(ROUTE_PTS[0]),
         1.7f, ROUTE);

    spr.setFont(&lgfx::fonts::Font0);
    spr.setTextColor(INK);
    spr.drawString("ROCK CREEK", 87, 91);
    (void)language;
}

void topoLiveBase(LGFX_Sprite& spr) {
    // Live-only map layer: every primitive earns its place in the 33 ms
    // budget. Full decorative cartography remains available on boot/finish.
    spr.fillScreen(MAP_SAND);
    spr.fillTriangle(0, 0, 126, 0, 0, 132, MAP_GREEN);
    spr.fillTriangle(240, 240, 112, 240, 240, 108, MAP_GREEN_2);
    constexpr Pt contours[][7] = {
        {{0,54},{36,42},{72,51},{108,68},{145,57},{184,42},{239,49}},
        {{0,157},{37,145},{74,154},{112,172},{150,161},{190,145},{239,153}}
    };
    for (const auto& contour : contours)
        for (int i = 1; i < 7; ++i)
            spr.drawLine(contour[i-1].x, contour[i-1].y,
                         contour[i].x, contour[i].y, CONTOUR);
    // Single-pixel high-contrast route, without AA/wide-line rasterization.
    constexpr int route_count = sizeof(ROUTE_PTS) / sizeof(ROUTE_PTS[0]);
    for (int i = 1; i < route_count; ++i) {
        spr.drawLine(ROUTE_PTS[i-1].x, ROUTE_PTS[i-1].y,
                     ROUTE_PTS[i].x, ROUTE_PTS[i].y, ROUTE);
    }
}

void logoPill(LGFX_Sprite& spr) {
    spr.fillRoundRect(6, 6, 96, 27, 13, INK);
    spr.setTextDatum(lgfx::MC_DATUM);
    spr.setFont(&lgfx::fonts::Font2);
    spr.setTextColor(MAP_YELLOW, INK);
    spr.drawString("U4", 24, 19);
    spr.setFont(&lgfx::fonts::Font0);
    spr.setTextColor(WHITE, INK);
    spr.drawString("TRAIL MAP", 66, 19);
    spr.setTextDatum(lgfx::TL_DATUM);
}

void nextTurnIcon(LGFX_Sprite& spr, WaypointDir dir, int x, int y,
                  uint16_t color) {
    int dx = 0;
    if (dir == WaypointDir::RIGHT_45 || dir == WaypointDir::RIGHT_90 ||
        dir == WaypointDir::HAIRPIN_R) dx = 8;
    if (dir == WaypointDir::LEFT_45 || dir == WaypointDir::LEFT_90 ||
        dir == WaypointDir::HAIRPIN_L) dx = -8;
    spr.drawLine(x, y + 6, x, y - 4, color);
    spr.drawLine(x + 1, y + 6, x + 1, y - 4, color);
    spr.drawLine(x, y - 4, x + dx, y - 10, color);
    spr.fillTriangle(x + dx, y - 12, x + dx - 3, y - 7,
                     x + dx + 3, y - 7, color);
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
    spr.fillCircle(218, 24, 18, SHADOW);
    spr.fillCircle(217, 23, 18, CARD);
    spr.drawCircle(217, 23, 17, rgbn(205, 205, 201));
    spr.setTextDatum(lgfx::TC_DATUM);
    spr.setFont(&lgfx::fonts::Font0);
    spr.setTextColor(INK, CARD);
    spr.drawString("N", 217, 7);
    float a = heading * 0.0174532925f;
    int dx = static_cast<int>(sinf(a) * 11.0f);
    int dy = static_cast<int>(-cosf(a) * 11.0f);
    spr.drawLine(217, 23, 217 + dx, 23 + dy, ROUTE);
    spr.drawLine(218, 23, 218 + dx, 23 + dy, ROUTE);
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
    spr.fillCircle(x, y, 9, WHITE);
    spr.fillCircle(x, y, 6, BLUE);
    spr.fillCircle(x - 2, y - 2, 2, WHITE);
}
}

void SceneNavigator::_renderThemedBoot(const UiFx& fx) {
    topoBase(_spr, _settings.language);
    _spr.fillRoundRect(25, 73, 190, 96, 12, SHADOW);
    _spr.fillRoundRect(23, 71, 190, 96, 12, INK);
    float q = Ease::outCubic(Anim::progress(fx.phase_ms, 0, BOOT_TOTAL_MS));

    _spr.setTextDatum(lgfx::MC_DATUM);
    _spr.setFont(&lgfx::fonts::Font4);
    _spr.setTextColor(MAP_YELLOW, INK);
    _spr.drawString("ULTRA4", 78, 98);
    _spr.setFont(&_fonts.med);
    _spr.setTextColor(WHITE, INK);
    _spr.drawString("TRAIL MAP", 151, 100);
    _spr.setFont(&_fonts.small);
    _spr.setTextColor(rgbn(190, 193, 190), INK);
    _spr.drawString(uiText(_settings.language,
                           "MAPPE OFFLINE / RICERCA GPS",
                           "OFFLINE MAPS / GPS ACQUIRING"), 118, 130);
    _spr.fillRoundRect(43, 145, 150, 5, 2, rgbn(63, 67, 65));
    _spr.fillRoundRect(43, 145, static_cast<int>(150.0f * q), 5, 2,
                       MAP_YELLOW);
    _spr.setTextDatum(lgfx::TL_DATUM);
}

void SceneNavigator::_renderThemedLive(const VehicleState& s,
                                       const UiFx& fx) {
    topoLiveBase(_spr);

    float total = _rb.stageTotal();
    float progress = total > 0.0f ? s.odo_mi / total : 0.0f;
    int px, py;
    routePoint(progress, px, py);
    positionMarker(_spr, px, py, s.heading_deg);

    // Compact top status replaces four decorative widgets during live drive.
    _spr.fillRoundRect(6, 6, 228, 27, 7, INK);
    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextColor(MAP_YELLOW, INK);
    _spr.drawString("U4 TRAIL", 14, 15);
    char live_heading[12];
    localizedHeadingFull(_settings.language, live_heading, s.heading_deg);
    _spr.setTextDatum(lgfx::TR_DATUM);
    _spr.setTextColor(WHITE, INK);
    _spr.drawString(live_heading, 226, 15);
    _spr.setTextDatum(lgfx::TL_DATUM);

    int active = s.active_wp;
    if (active < 0) active = 0;
    if (active >= _rb.getCount()) active = _rb.getCount() - 1;
    int target = active < _rb.getCount() - 1 ? active + 1 : active;
    if (fx.alert && fx.alert_wp >= 0 && fx.alert_wp < _rb.getCount())
        target = fx.alert_wp;
    const Waypoint& wp = _rb.get(target);

    // Bottom route card: next maneuver, matching distance, speed and heading.
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
    nextTurnIcon(_spr, wp.dir, 82, 202,
                 wp.danger ? TRAIL_CLOSE : ROUTE);

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
    spr.fillRoundRect(7, 7, 226, 38, 7, SN_PANEL_2);
    spr.fillRoundRect(7, 201, 226, 32, 7, SN_PANEL_2);
    spr.drawFastHLine(12, 48, 216, SN_EDGE);
    spr.drawFastHLine(12, 197, 216, SN_EDGE);
}

void stockTrailMap(LGFX_Sprite& spr, float progress) {
    // Fixed contour polylines. Large clipped ellipses took hundreds of ms on
    // the ESP32; these few straight segments preserve the topo language while
    // rendering in a small, deterministic fraction of a frame.
    constexpr Pt contours[][7] = {
        {{8,66},{36,54},{68,58},{95,72},{126,68},{162,55},{228,61}},
        {{8,82},{37,70},{71,75},{98,89},{130,84},{169,70},{232,76}},
        {{8,103},{39,91},{71,96},{101,111},{136,104},{174,90},{232,96}},
        {{8,127},{34,116},{65,120},{99,137},{137,129},{176,115},{232,121}},
        {{8,151},{39,140},{70,145},{104,161},{141,154},{179,140},{232,146}},
        {{8,177},{35,166},{67,171},{102,188},{139,180},{179,165},{232,172}}
    };
    for (const auto& contour : contours)
        for (int i = 1; i < 7; ++i)
            spr.drawLine(contour[i-1].x, contour[i-1].y,
                         contour[i].x, contour[i].y, SN_EDGE);
    constexpr Pt route[] = {{50,184},{70,166},{91,151},{84,130},{113,111},
                            {141,94},{134,73},{165,56},{190,52}};
    for (int i = 1; i < 9; ++i) {
        for (int offset = -2; offset <= 2; ++offset)
            spr.drawLine(route[i-1].x + offset, route[i-1].y,
                         route[i].x + offset, route[i].y,
                         offset == 0 ? SN_WHITE : SN_AMBER);
    }
    int at = static_cast<int>(progress * 7.9f);
    if (at < 0) at = 0;
    if (at > 7) at = 7;
    float f = progress * 7.9f - at;
    int x = route[at].x + static_cast<int>((route[at+1].x-route[at].x)*f);
    int y = route[at].y + static_cast<int>((route[at+1].y-route[at].y)*f);
    spr.fillCircle(x, y, 10, SN_WHITE);
    spr.fillCircle(x, y, 7, SN_BG);
    spr.fillTriangle(x, y-7, x-5, y+6, x+5, y+6, rgbn(64, 145, 238));
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
    _spr.drawString("TRAIL COMMAND", 120, 163);
    _spr.setFont(&_fonts.small);
    _spr.setTextColor(SN_AMBER, SN_PANEL);
    _spr.drawString(uiText(_settings.language, "NAVIGAZIONE PRONTA",
                           "NAVIGATION READY"), 120, 184);
    _spr.fillRoundRect(45, 207, 150, 5, 2, SN_EDGE);
    _spr.fillRoundRect(45, 207, static_cast<int>(150.0f * q), 5, 2, SN_LIME);
    _spr.setTextDatum(lgfx::TL_DATUM);
}

void SceneNavigator::_renderStockLive(const VehicleState& s, const UiFx& fx) {
    stockNavBase(_spr);
    {
    float speed = _spdF.update(s.speed_mph, fx.now_ms,
                               NAV_VALUE_SMOOTH_MS, NAV_VALUE_SMOOTH_MS);
    const float progress = fmodf(s.odo_mi * 3.7f, 1.0f);
    stockTrailMap(_spr, progress < 0.0f ? progress + 1.0f : progress);

    char trail_text[22];
    _spr.setTextDatum(lgfx::TC_DATUM);
    _spr.setFont(&_fonts.med);
    _spr.setTextColor(SN_AMBER, SN_PANEL_2);
    _spr.drawString("NEXT", 43, 11);
    _spr.setTextColor(SN_WHITE, SN_PANEL_2);
    _spr.drawString("2.4", 43, 25);
    _spr.setTextColor(SN_AMBER, SN_PANEL_2);
    _spr.drawString("TRAIL", 120, 11);
    char heading[12];
    localizedHeadingFull(_settings.language, heading, s.heading_deg);
    _spr.setTextColor(SN_WHITE, SN_PANEL_2);
    _spr.drawString(heading, 120, 25);
    _spr.setTextColor(SN_AMBER, SN_PANEL_2);
    _spr.drawString("ALT", 198, 11);
    snprintf(trail_text, sizeof(trail_text), "%dm", static_cast<int>(900.0f + s.odo_mi * 8.0f));
    _spr.setTextColor(SN_WHITE, SN_PANEL_2);
    _spr.drawString(trail_text, 198, 25);

    Fmt::speed(trail_text, speed, _settings.use_kmh);
    _spr.setFont(&_fonts.med);
    _spr.setTextColor(SN_WHITE, SN_PANEL_2);
    _spr.drawString(trail_text, 38, 211);
    _spr.setFont(&_fonts.small);
    _spr.setTextColor(SN_DIM, SN_PANEL_2);
    _spr.drawString(_settings.use_kmh ? "KM/H" : "MPH", 38, 224);
    _spr.setTextColor(SN_LIME, SN_PANEL_2);
    _spr.drawString(s.four_wd_lock ? "4H LOCK" : "4H AUTO", 120, 214);
    snprintf(trail_text, sizeof(trail_text), "%.1fV", s.battery_v);
    _spr.setTextColor(SN_WHITE, SN_PANEL_2);
    _spr.drawString(trail_text, 198, 214);
    _spr.setTextDatum(lgfx::TL_DATUM);

    if (fx.alert) {
        uint16_t border = Anim::pulse(fx.now_ms, ALERT_PULSE_MS) ? SN_RED : SN_AMBER;
        _spr.drawRoundRect(2, 2, 236, 236, 13, border);
    }
    return;
    }

    // OEM top bar: voltage, heading and temperature.
    char text[28];
    float speed = _spdF.update(s.speed_mph, fx.now_ms,
                               NAV_VALUE_SMOOTH_MS, NAV_VALUE_SMOOTH_MS);
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
