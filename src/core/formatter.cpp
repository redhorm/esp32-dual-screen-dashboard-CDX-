#include "formatter.h"
#include <stdio.h>
#include <math.h>

namespace Fmt {

void speed(char* buf, float mph, bool use_kmh) {
    if (use_kmh)
        snprintf(buf, 8, "%d", (int)(mph * 1.60934f));
    else
        snprintf(buf, 8, "%d", (int)mph);
}

void rpm(char* buf, float r) {
    snprintf(buf, 6, "%d", (int)r);
}

void battery(char* buf, float v) {
    snprintf(buf, 7, "%.1fV", v);
}

// \xF8 is the degree sign in CP437 — the encoding of the GLCD Font0
// used on the dashboard (\xB0 has no glyph there).
void temp(char* buf, float f, bool use_celsius) {
    if (use_celsius)
        snprintf(buf, 8, "%d\xF8""C", (int)((f - 32.0f) * 5.0f / 9.0f));
    else
        snprintf(buf, 8, "%d\xF8""F", (int)f);
}

void tempPlain(char* buf, float f, bool use_celsius) {
    if (use_celsius)
        snprintf(buf, 6, "%dC", (int)((f - 32.0f) * 5.0f / 9.0f));
    else
        snprintf(buf, 6, "%dF", (int)f);
}

static const char* CARD8[] = {
    "N","NE","E","SE","S","SW","W","NW"
};

void heading(char* buf, float deg) {
    // Normalise to 0–360
    while (deg < 0)   deg += 360.0f;
    while (deg >= 360) deg -= 360.0f;
    int idx = (int)((deg + 22.5f) / 45.0f) % 8;
    snprintf(buf, 4, "%s", CARD8[idx]);
}

// No degree sign: rendered with fonts that only cover plain ASCII.
void headingFull(char* buf, float deg) {
    while (deg < 0)   deg += 360.0f;
    while (deg >= 360) deg -= 360.0f;
    int idx = (int)((deg + 22.5f) / 45.0f) % 8;
    snprintf(buf, 8, "%s %d", CARD8[idx], (int)deg);
}

void dist(char* buf, float mi, bool use_km) {
    if (use_km)
        snprintf(buf, 9, "%.2f km", mi * 1.60934f);
    else
        snprintf(buf, 9, "%.2f mi", mi);
}

void gear(char* buf, int g, bool drive_auto) {
    if (drive_auto)
        snprintf(buf, 3, "D");
    else
        snprintf(buf, 3, "%d", g);
}

void percent(char* buf, float pct) {
    snprintf(buf, 5, "%d%%", (int)(pct * 100.0f));
}

void distNum(char* buf, float mi, bool use_km) {
    snprintf(buf, 7, "%.2f", use_km ? mi * 1.60934f : mi);
}

void clockHMS(char* buf, uint32_t seconds) {
    uint32_t h = (12 + seconds / 3600) % 24;
    uint32_t m = (seconds / 60) % 60;
    uint32_t s = seconds % 60;
    snprintf(buf, 9, "%02u:%02u:%02u",
             (unsigned)h, (unsigned)m, (unsigned)s);
}

void raceTime(char* buf, uint32_t ms) {
    uint32_t m = ms / 60000;
    uint32_t s = (ms / 1000) % 60;
    uint32_t t = (ms / 100) % 10;
    snprintf(buf, 9, "%02u:%02u.%u", (unsigned)m, (unsigned)s, (unsigned)t);
}

} // namespace Fmt
