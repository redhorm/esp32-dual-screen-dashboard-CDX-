#pragma once
#include <stdint.h>

// Three actual received pulses, not three render/control iterations. This
// rejects one isolated spike and accepts a sustained step on its second pulse.
// Always inlined because hardware capture calls it from its IRAM interrupt.
inline __attribute__((always_inline)) uint16_t pwmMedian3(
        uint16_t a, uint16_t b, uint16_t c) {
    if (a > b) { const uint16_t t = a; a = b; b = t; }
    if (b > c) b = c;
    return a > b ? a : b;
}
