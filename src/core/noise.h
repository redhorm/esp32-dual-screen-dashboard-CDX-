#pragma once
#include <stdint.h>
#include "anim.h"

// Deterministic, seedable value noise — pure C++, host-testable.
// Same (t, seed) always yields the same output: simulations using it
// stay reproducible in tests.
namespace Noise {

inline uint32_t hash(uint32_t x, uint32_t seed) {
    x ^= seed;
    x ^= x >> 16;  x *= 0x7FEB352Du;
    x ^= x >> 15;  x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

// White noise in -1..1 for integer coordinate x
inline float unit(uint32_t x, uint32_t seed) {
    return (float)(hash(x, seed) & 0xFFFF) / 32767.5f - 1.0f;
}

// Smooth value noise in -1..1: linear interpolation between lattice
// points spaced period_ms apart.
inline float smooth(uint32_t t_ms, uint32_t period_ms, uint32_t seed) {
    if (period_ms == 0) return unit(t_ms, seed);
    uint32_t i = t_ms / period_ms;
    float    f = (float)(t_ms % period_ms) / (float)period_ms;
    float a = unit(i, seed);
    float b = unit(i + 1, seed);
    return a + (b - a) * f;
}

// Vibration pixel offset for gauges: amplitude scales with rpm_pct,
// bounded to ±max_px. Deterministic per (t, seed).
inline int vibPx(uint32_t t_ms, float rpm_pct, int max_px,
                 uint32_t period_ms, uint32_t seed) {
    float amp = Anim::clamp01(rpm_pct) * (float)max_px;
    float v   = smooth(t_ms, period_ms, seed) * amp;
    int   px  = (int)(v + (v >= 0 ? 0.5f : -0.5f));
    if (px >  max_px) px =  max_px;
    if (px < -max_px) px = -max_px;
    return px;
}

} // namespace Noise
