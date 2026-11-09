#pragma once
#include <math.h>
#include "esc_signal.h"

// Display-only scale. Receiver direction, neutral, brake/reverse and RTL
// always use the original calibrated input and are never affected here.
namespace EscDisplayCalibration {
inline bool valid(const EscSignalConfig& c, uint16_t full) {
    if (full < 800 || full > 2200) return false;
    const int span = c.inverted ? int(c.neutral_us)-full : int(full)-c.neutral_us;
    return span >= 200;
}
inline float apply(float command, const EscSignalConfig& c, uint16_t full,
                   uint16_t filtered_pulse) {
    if (!isfinite(command)) return 0.0f;
    if (command <= 0.0f || !valid(c, full)) return command;
    if (filtered_pulse < 800 || filtered_pulse > 2200) return 0.0f;
    // Use the physical pulse: normalized input may already be clipped at the
    // old nominal 1000/2000 endpoints, which would under-read wider travel.
    const float travel = c.inverted ? int(c.neutral_us)-filtered_pulse : int(filtered_pulse)-c.neutral_us;
    const float calibrated = c.inverted ? int(c.neutral_us)-full : int(full)-c.neutral_us;
    const float result = travel / calibrated;
    return result < 0.0f ? 0.0f : result > 1.0f ? 1.0f : result;
}
}
