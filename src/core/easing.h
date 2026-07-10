#pragma once
#include <stdint.h>
#include <math.h>
#include "anim.h"

// Easing & value-tracking helpers — pure C++, host-testable.
// The renderers own instances but only call update(): all the
// smoothing logic lives (and is tested) here.
namespace Ease {

inline float inOutQuad(float t) {
    t = Anim::clamp01(t);
    return t < 0.5f ? 2.0f * t * t : 1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * 0.5f;
}

inline float outCubic(float t) { return Anim::easeOutCubic(t); }

// Critically-damped-ish exponential approach: alpha = 1-exp(-dt/tau)
inline float approach(float value, float target, float dt_ms, float tau_ms) {
    if (tau_ms <= 0.0f) return target;
    float a = 1.0f - expf(-dt_ms / tau_ms);
    return value + (target - value) * a;
}

} // namespace Ease

// Tracks a target with separate attack (rising) and release (falling)
// time constants — VU-meter style when they differ.
struct Follow {
    float    value   = 0.0f;
    uint32_t last_ms = 0;
    bool     primed  = false;

    float update(float target, uint32_t now_ms,
                 float tau_up_ms, float tau_dn_ms) {
        if (!primed) { primed = true; last_ms = now_ms; value = target; return value; }
        float dt = (float)(now_ms - last_ms);
        last_ms = now_ms;
        float tau = (target > value) ? tau_up_ms : tau_dn_ms;
        value = Ease::approach(value, target, dt, tau);
        return value;
    }

    void snap(float v, uint32_t now_ms) { value = v; last_ms = now_ms; primed = true; }
};

// Animates an integer position (e.g. roadbook scroll index) into a
// smooth eased float position over dur_ms.
struct SlideAnim {
    float    from   = 0.0f;
    int      target = 0;
    uint32_t t0     = 0;
    uint32_t dur    = 0;
    bool     primed = false;

    void setTarget(int idx, uint32_t now_ms, uint32_t dur_ms) {
        if (!primed) { primed = true; from = (float)idx; target = idx; dur = 0; return; }
        if (idx == target) return;
        from   = pos(now_ms);   // retarget mid-flight without jumping
        target = idx;
        t0     = now_ms;
        dur    = dur_ms;
    }

    float pos(uint32_t now_ms) const {
        if (!primed || dur == 0) return (float)target;
        float p = Anim::progress(now_ms, t0, dur);
        return from + ((float)target - from) * Ease::outCubic(p);
    }

    bool active(uint32_t now_ms) const {
        return primed && dur != 0 && (now_ms - t0) < dur;
    }
};
