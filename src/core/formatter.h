#pragma once
#include <stdint.h>

// Pure-C++ formatting utils — no Arduino / LovyanGFX dependency.
// All functions write into caller-supplied buffers (no heap).

namespace Fmt {

// "45" / "72 km/h" — buf must be ≥8
void speed(char* buf, float mph, bool use_kmh = false);

// "5200" — buf ≥6
void rpm(char* buf, float rpm);

// "14.2V" — buf ≥7
void battery(char* buf, float v);

// "210°F" / "99°C" (° = \xF8, CP437/GLCD encoding) — buf ≥8
void temp(char* buf, float f, bool use_celsius = false);

// "99C" / "210F" — no degree sign, safe for VLW fonts (which have no
// ° glyph and decode UTF-8) — buf ≥6
void tempPlain(char* buf, float f, bool use_celsius = false);

// "NW" "N" "NNE" etc. — buf ≥4
void heading(char* buf, float deg);

// "N 348" (no degree sign: ASCII-only fonts) — buf ≥8
void headingFull(char* buf, float deg);

// "0.45" / "0.72 km" — buf ≥9
void dist(char* buf, float mi, bool use_km = false);

// "D" "1" "2" "3" "4" "5" — buf ≥3
void gear(char* buf, int g, bool drive_auto);

// "78%" — buf ≥5
void percent(char* buf, float pct);

// "0.45" — bare 2-decimal number, no unit (roadbook table cells) — buf ≥7
void distNum(char* buf, float mi, bool use_km = false);

// "12:34:56" from seconds since start, base clock 12:00:00 — buf ≥9
void clockHMS(char* buf, uint32_t seconds);

// "04:23.7" race time mm:ss.tenths from milliseconds — buf ≥9
void raceTime(char* buf, uint32_t ms);

} // namespace Fmt
