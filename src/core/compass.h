#pragma once
#include <stdint.h>
#include <string.h>

// Aviation-style scrolling compass tape geometry — pure C++, testable.
// The renderer just draws the ticks this function emits.
namespace Compass {

struct Tick {
    int  x;         // pixel x inside the tape (0..width-1)
    bool major;     // true every 45° (has a label)
    char label[3];  // cardinal ("N", "NE", ...) when major, else ""
};

static const char* const CARD8[8] = {"N","NE","E","SE","S","SW","W","NW"};

// Fills `out` with the ticks visible on a tape `width_px` wide centered
// on `heading_deg`, minor ticks every 15°, major every 45°.
// Returns the tick count (≤ max_out).
inline int tape(float heading_deg, int width_px, float px_per_deg,
                Tick* out, int max_out) {
    while (heading_deg < 0)      heading_deg += 360.0f;
    while (heading_deg >= 360.0f) heading_deg -= 360.0f;

    float half_deg = (width_px * 0.5f) / px_per_deg;
    // First 15°-multiple at or after the left edge
    float left  = heading_deg - half_deg;
    int   first = (int)(left / 15.0f);
    if (first * 15.0f < left) first++;

    int n = 0;
    for (int k = first; n < max_out; k++) {
        float deg = k * 15.0f;
        float dx  = deg - heading_deg;          // may exceed 360 on wrap
        int   x   = (int)(width_px * 0.5f + dx * px_per_deg + 0.5f);
        if (x >= width_px) break;
        if (x < 0) continue;
        Tick& t = out[n++];
        t.x = x;
        int norm = ((k * 15) % 360 + 360) % 360;
        t.major = (norm % 45) == 0;
        if (t.major) strncpy(t.label, CARD8[norm / 45], 3);
        else         t.label[0] = '\0';
    }
    return n;
}

} // namespace Compass
