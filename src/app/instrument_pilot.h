#pragma once

#include <stdint.h>
#include <stdio.h>
#include "recoil_renderer.h"

// A scale adaptation of the IQ3's blue segmented LCD hierarchy: curved RPM
// bar, a large central gear, speed/time either side and fixed lower readouts.
// The existing model has 16 optical apertures rather than the real unit's 10;
// eight symmetric pairs preserve the physical CAD centres without new LEDs.
namespace Iq3Render {
constexpr uint16_t LCD = RecoilRender::rgb(129, 151, 239);
constexpr uint16_t INK = RecoilRender::rgb(24, 33, 82);
constexpr uint16_t INACTIVE = RecoilRender::rgb(116, 138, 221);
constexpr uint16_t GREEN = RecoilRender::rgb(56, 220, 86);
constexpr uint16_t AMBER = RecoilRender::AMBER;
constexpr uint16_t RED = RecoilRender::RED;

struct Data {
    RecoilRender::Screen screen = RecoilRender::Screen::LIVE;
    int speed = 0;
    int gear = 0;
    int rpm = 0;
    int battery_x10 = 0;
    int temp = 0;  // Available to the caller; the compact fixed page prioritises RPM/volts.
    int shift_pairs = -1;  // -1 derives from RPM; 0..8 supports the startup lamp check.
    bool kmh = true;
    bool celsius = true;
    bool warning = false;
    bool esc_controlled = false;
    bool esc_signal_valid = false;
    uint32_t elapsed_ms = 0;
    uint32_t now_ms = 0;
};

inline int bounded(int value, int low, int high) {
    return value < low ? low : (value > high ? high : value);
}

// Each primitive is clipped against the delivered optical opening. A future
// longer label cannot leak light behind the mask or overwrite an aperture.
template<class C> class Window {
public:
    explicit Window(C& canvas) : canvas_(canvas) {}
    void fillRect(int x, int y, int width, int height, uint16_t color) {
        if (width <= 0 || height <= 0) return;
        const int end = x + width < 160 ? x + width : 160;
        for (int px = x < 0 ? 0 : x; px < end; ++px) {
            const int top = RecoilGeometry::windowTop[px];
            if (top == 255) continue;
            const int first = y > top ? y : top;
            const int last = y + height - 1 < RecoilGeometry::WINDOW_BOTTOM ? y + height - 1 : RecoilGeometry::WINDOW_BOTTOM;
            if (first <= last) canvas_.fillRect(px, first, 1, last - first + 1, color);
        }
    }
private:
    C& canvas_;
};

template<class C> class Slant {
public:
    Slant(C& canvas, int top, int height) : canvas_(canvas), top_(top), height_(height) {}
    void fillRect(int x, int y, int width, int height, uint16_t color) {
        for (int row = 0; row < height; ++row) {
            const int offset = (height_ - (y + row - top_) - 1) / 8;
            canvas_.fillRect(x + offset, y + row, width, 1, color);
        }
    }
private:
    C& canvas_;
    int top_, height_;
};

template<class C> void digit(C& c, char ch, int x, int y, int w, int h, int thickness) {
    Slant<C> slanted(c, y, h);
    RecoilRender::segmentDigit(slanted, '8', x, y, w, h, thickness, INACTIVE);
    RecoilRender::segmentDigit(slanted, ch, x, y, w, h, thickness, INK);
}

template<class C> void number(C& c, int value, int center, int y, int w, int h, int thickness) {
    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%d", value);
    const int count = static_cast<int>(strlen(buffer));
    const int slant_width = (h - 1) / 8;
    int x = center - (count * (w + 2) - 2 + slant_width) / 2;
    for (int i = 0; i < count; ++i, x += w + 2) digit(c, buffer[i], x, y, w, h, thickness);
}

template<class C> void gearLetter(C& c, bool reverse) {
    // Two-pixel strokes match the segmented numeral instead of enlarging a
    // three-pixel font whose N would collapse into an H on this tiny display.
    Slant<C> s(c,39,23);
    s.fillRect(72,39,2,23,INK);
    if (reverse) {
        s.fillRect(74,39,11,2,INK);
        s.fillRect(85,41,2,9,INK);
        s.fillRect(74,50,11,2,INK);
        for(int row=0;row<10;++row)s.fillRect(77+row,52+row,2,1,INK);
    } else {
        s.fillRect(85,39,2,23,INK);
        for(int row=0;row<23;++row)s.fillRect(74+row*10/22,39+row,2,1,INK);
    }
}

inline int pairCount(int rpm) {
    return bounded((bounded(rpm, 0, 99999) - 4000) / 400, 0, 8);
}

template<class C> void lamps(C& c, int pairs) {
    pairs = bounded(pairs, 0, 8);
    for (int i = 0; i < 16; ++i) {
        const int pair = i < 8 ? i : 15 - i;
        const uint16_t color = pair < 4 ? GREEN : (pair < 6 ? AMBER : RED);
        const auto& point = RecoilGeometry::dots[i];
        RecoilRender::disk(c, point.x, point.y, RecoilGeometry::DOT_RADIUS,
                           pair < pairs ? color : RecoilRender::BLACK);
    }
}

template<class C> void tachometer(C& c, int rpm) {
    const int bars = bounded(rpm, 0, 8000) / 200;
    // Forty distinct bars follow the actual window arch, not the lamp arch.
    for (int i = 0; i < 40; ++i) {
        const int x = 13 + i * 133 / 39;
        const int y = RecoilGeometry::windowTop[x] + 1;
        c.fillRect(x, y, 2, 4, i < bars ? INK : INACTIVE);
    }
    for (int tick = 0; tick <= 8; ++tick) {
        const int x = 14 + tick * 132 / 8;
        const int y = RecoilGeometry::windowTop[x] + 7;
        const char label[2] = {static_cast<char>('0' + tick), 0};
        RecoilRender::text(c, label, x, y, INK);
    }
}

template<class C> void timer(C& c, uint32_t elapsed_ms, bool test) {
    const uint32_t seconds = elapsed_ms / 1000U;
    const uint32_t minutes = (seconds / 60U) % 100U;
    char text[8];
    if (test) snprintf(text, sizeof(text), "88:88");
    else snprintf(text, sizeof(text), "%02lu:%02lu", static_cast<unsigned long>(minutes), static_cast<unsigned long>(seconds % 60U));
    int x = 105;
    for (int i = 0; i < 5; ++i) {
        if (text[i] == ':') {
            c.fillRect(x, 50, 1, 1, INK); c.fillRect(x, 56, 1, 1, INK); x += 3;
        } else { digit(c, text[i], x, 47, 6, 15, 1); x += 8; }
    }
}

template<class C> bool pilot(C& canvas, const Data& data) {
    if (data.screen == RecoilRender::Screen::ALIGN) {
        RecoilRender::PilotData original;
        original.screen = RecoilRender::Screen::ALIGN;
        return RecoilRender::pilot(canvas, original);
    }
    canvas.fillScreen(RecoilRender::BLACK);
    if (canvas.width() != 160 || canvas.height() != 80) return false;
    Window<C> c(canvas);
    c.fillRect(0, 0, 160, 80, LCD);
    const bool startup = data.screen == RecoilRender::Screen::BOOT;
    tachometer(c, startup ? 8000 : data.rpm);
    RecoilRender::text(c, data.kmh ? "KM/H" : "MPH", 35, 41, INK);
    number(c, startup ? 888 : bounded(data.speed, 0, 999), 35, 47, 9, 15, 1);

    const char* center_label = data.warning ? "ALERT" : data.screen == RecoilRender::Screen::FINISH ? "FIN" : "GEAR";
    if (data.warning) {
        c.fillRect(67, 32, 26, 7, INK);
        RecoilRender::text(c, center_label, 80, 33, LCD);
    } else RecoilRender::text(c, center_label, 80, 32, INK);
    if (startup) number(c, 8, 80, 39, 15, 23, 2);
    else if (data.gear > 0) number(c, bounded(data.gear, 1, 9), 80, 39, 15, 23, 2);
    else gearLetter(c, data.gear < 0);

    RecoilRender::text(c, "RUN", 122, 41, INK);
    timer(c, data.elapsed_ms, startup);
    char label[16];
    const int rpm = startup ? 8888 : bounded(data.rpm, 0, 99999);
    snprintf(label, sizeof(label), "%d", rpm);
    RecoilRender::text(c, "RPM", 33, 64, INK);
    RecoilRender::text(c, label, 33, 70, INK);
    const int volts = startup ? 888 : bounded(data.battery_x10, 0, 999);
    snprintf(label, sizeof(label), "%d.%d", volts / 10, volts % 10);
    RecoilRender::text(c, "VOLT", 125, 64, INK);
    RecoilRender::text(c, label, 125, 70, INK);
    // This compact fixed marker applies to every instrument page, even startup
    // and warnings. The board has no RPM/voltage/GPS sensors in this build.
    RecoilRender::text(c, data.esc_controlled ? (data.esc_signal_valid ? "ESC" : "WAIT") : "DEMO", 80, 69, INK);

    // Last draw pass always preserves all sixteen exact optical patch centres.
    lamps(canvas, data.shift_pairs < 0 ? pairCount(data.rpm) : data.shift_pairs);
    return true;
}
} // namespace Iq3Render
