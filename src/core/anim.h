#pragma once
#include <stdint.h>

// Pure animation math — no Arduino, no LovyanGFX. Host-testable.
namespace Anim {

inline float clamp01(float t) {
    return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
}

// Linear progress 0..1 of `now` inside [t0, t0+dur]
inline float progress(uint32_t now, uint32_t t0, uint32_t dur) {
    if (dur == 0) return 1.0f;
    if (now <= t0) return 0.0f;
    return clamp01((float)(now - t0) / (float)dur);
}

// 50% duty square wave; true during the first half-period
inline bool pulse(uint32_t ms, uint32_t half_period) {
    if (half_period == 0) return true;
    return (ms / half_period) % 2 == 0;
}

inline float easeOutCubic(float t) {
    t = clamp01(t);
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}

// 0→1→0 over t = 0..1 (RPM sweep at boot)
inline float triangle(float t) {
    t = clamp01(t);
    return t < 0.5f ? t * 2.0f : 2.0f - t * 2.0f;
}

// Gear-change snap: overshoot → 1.0 with cubic ease-out
inline float snapScale(float t, float overshoot) {
    return overshoot - (overshoot - 1.0f) * easeOutCubic(t);
}

// RGB565 linear interpolation (fades on 16-bit sprites)
inline uint16_t lerp565(uint16_t a, uint16_t b, float t) {
    t = clamp01(t);
    int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
    int br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
    int r = ar + (int)((br - ar) * t);
    int g = ag + (int)((bg - ag) * t);
    int bl = ab + (int)((bb - ab) * t);
    return (uint16_t)((r << 11) | (g << 5) | bl);
}

} // namespace Anim
