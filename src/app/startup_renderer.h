#pragma once
#include "instrument_pilot.h"
#include "instrument_navigator.h"
#include "core/startup_sequence.h"
#include "core/experience.h"

// No raster assets, allocations or simulated hardware diagnostics. The logo
// lettering is a compact pixel adaptation, not a facsimile of the wordmarks.
namespace StartupRender {
struct Glyph { uint8_t rows[7]; };
inline Glyph glyph(char ch) {
    switch (ch) {
        case 'A': return {{14,17,17,31,17,17,17}};
        case 'C': return {{14,17,16,16,16,17,14}};
        case 'D': return {{30,17,17,17,17,17,30}};
        case 'E': return {{31,16,16,30,16,16,31}};
        case 'H': return {{17,17,17,31,17,17,17}};
        case 'I': return {{31,4,4,4,4,4,31}};
        case 'K': return {{17,18,20,24,20,18,17}};
        case 'L': return {{16,16,16,16,16,16,31}};
        case 'N': return {{17,25,25,21,19,19,17}};
        case 'O': return {{14,17,17,17,17,17,14}};
        case 'P': return {{30,17,17,30,16,16,16}};
        case 'Q': return {{14,17,17,17,21,18,13}};
        case 'R': return {{30,17,17,30,20,18,17}};
        case 'S': return {{15,16,16,14,1,1,30}};
        case 'V': return {{17,17,17,17,17,10,4}};
        case 'W': return {{17,17,17,21,21,21,10}};
        case '3': return {{30,1,1,14,1,1,30}};
        default: return {{0,0,0,0,0,0,0}};
    }
}

template<class C> void wordmark(C& c, const char* value, int center, int y,
                                 uint16_t color, int scale, bool italic = false) {
    const int length = static_cast<int>(strlen(value));
    int x = center - (length * 6 - 1) * scale / 2;
    for (int i = 0; i < length; ++i, x += 6 * scale) {
        const Glyph g = glyph(value[i]);
        for (int row = 0; row < 7; ++row) {
            const int skew = italic ? (6 - row) / 2 : 0;
            for (int col = 0; col < 5; ++col)
                if (g.rows[row] & (1U << (4 - col)))
                    c.fillRect(x + col * scale + skew, y + row * scale, scale, scale, color);
        }
    }
}

template<class C> bool pilot(C& canvas, const StartupFrame& frame, bool english = false) {
    (void)english;
    canvas.fillScreen(RecoilRender::BLACK);
    if (canvas.width() != 160 || canvas.height() != 80) return false;
    if (!frame.active) return true;
    Iq3Render::Data data;
    data.rpm = 800;
    data.battery_x10 = 126;
    data.gear = 0;
    data.shift_pairs = 0;
    if (frame.elapsed_ms < StartupSequence::kSegmentTestEndMs) {
        data.screen = RecoilRender::Screen::BOOT;
        data.shift_pairs = 8;
        return Iq3Render::pilot(canvas, data);
    }
    if (frame.elapsed_ms >= StartupSequence::kPilotIdentityEndMs)
        return Iq3Render::pilot(canvas, data);

    Iq3Render::Window<C> c(canvas);
    c.fillRect(0, 0, 160, 80, Iq3Render::LCD);
    // A small checkered flag plus slanted lettering fits below the curved
    // optical edge. All actual light apertures remain untouched and dark.
    for (int row = 0; row < 4; ++row) for (int col = 0; col < 5; ++col)
        if ((row + col) % 2 == 0) c.fillRect(21 + col * 3, 39 + row * 3, 3, 3, Iq3Render::INK);
    wordmark(c, "RACEPAK", 91, 37, Iq3Render::INK, 2, true);
    wordmark(c, "IQ3", 80, 55, Iq3Render::INK, 1);
    char version[24];
    snprintf(version, sizeof(version), "V%s", EXPERIENCE_VERSION);
    for (char& ch : version) {
        if (!ch) break;
        if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
    }
    RecoilRender::text(c, version, 80, 63, Iq3Render::INK);
    RecoilRender::text(c, "DEMO", 80, 69, Iq3Render::INK);
    Iq3Render::lamps(canvas, 0);
    return true;
}

template<class C> void demoFooter(C& c, bool english) {
    RecoilRender::text(c, english ? "RECOIL ADAPTATION" : "ADATTAMENTO RECOIL", 99, 128, HdsRender::WHITE);
    c.fillRect(208, 127, 28, 7, HdsRender::AMBER);
    RecoilRender::text(c, "DEMO", 222, 128, HdsRender::INK);
}

template<class C> bool navigator(C& canvas, const StartupFrame& frame, bool english = false) {
    canvas.fillScreen(RecoilRender::BLACK);
    if (canvas.width() != 240 || canvas.height() != 135) return false;
    if (!frame.active) return true;
    if (frame.elapsed_ms < StartupSequence::kIdentityEndMs) {
        wordmark(canvas, "LOWRANCE", 120, 36, HdsRender::WHITE, 4);
        wordmark(canvas, "HDS LIVE", 120, 76, HdsRender::WHITE, 2);
        canvas.fillRect(87, 98, 66, 2, RecoilRender::CYAN);
        RecoilRender::text(canvas, english ? "DEMO CHART" : "MAPPA DEMO", 120, 111, HdsRender::WHITE);
        demoFooter(canvas, english);
        return true;
    }
    HdsRender::Data data;
    data.odo_m = 0;
    data.speed_mps = 0;
    HdsRender::render(canvas, data);
    if (frame.elapsed_ms < StartupSequence::kChartEndMs) {
        // Reveal the actual fixed chart in six strips. This is map drawing,
        // never a GPS/sensor acquisition claim; the DEMO badge stays visible.
        const uint32_t chart_ms = frame.elapsed_ms - StartupSequence::kIdentityEndMs;
        const int revealed = static_cast<int>(chart_ms * 6U / (StartupSequence::kChartEndMs - StartupSequence::kIdentityEndMs));
        for (int strip = revealed; strip < 6; ++strip)
            canvas.fillRect(strip * 40, 27, 40, 98, HdsRender::PAPER);
        canvas.fillRect(87, 113, 67, 9, HdsRender::INK);
        RecoilRender::text(canvas, english ? "DEMO CHART" : "MAPPA DEMO", 120, 115, HdsRender::WHITE);
    }
    return true;
}
} // namespace StartupRender
