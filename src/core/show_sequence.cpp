#include "show_sequence.h"

namespace {
// Triangle with a 1.6 second breathing cycle, never a rapid strobe.
uint8_t breathing(uint32_t ms) {
    const uint32_t t = ms % 1600U;
    return static_cast<uint8_t>((t < 800U ? t : 1600U - t) * 255U / 800U);
}
}

void ShowSequence::start(uint32_t now_ms) {
    _started_ms = now_ms;
    _active = true;
}

void ShowSequence::stop() { _active = false; }

void ShowSequence::update(uint32_t now_ms) {
    // Unsigned subtraction also works when millis() wraps during a show.
    if (_active && static_cast<uint32_t>(now_ms - _started_ms) >= kDurationMs)
        _active = false;
}

ShowFrame ShowSequence::frame(uint32_t now_ms) const {
    ShowFrame out;
    if (!_active) return out;
    const uint32_t elapsed = static_cast<uint32_t>(now_ms - _started_ms);
    // Renderers cannot display a stale show even if update() was delayed.
    if (elapsed >= kDurationMs) return out;
    out.active = true;
    out.elapsed_ms = elapsed;
    out.progress_permille = static_cast<uint16_t>(elapsed * 1000U / kDurationMs);
    if (elapsed < 2000U) {
        out.phase = ShowPhase::ACQUIRE;
        out.phase_ms = elapsed;
        out.lit = static_cast<uint8_t>(1U + elapsed * 15U / 1200U);
        if (out.lit > 16U) out.lit = 16U;
        out.pulse = static_cast<uint8_t>(elapsed < 1200U ? elapsed * 255U / 1200U : 255U);
    } else if (elapsed < 8000U) {
        out.phase = ShowPhase::ROUTE;
        out.phase_ms = elapsed - 2000U;
        out.route_permille = static_cast<uint16_t>(out.phase_ms * 1000U / 12000U);
        out.pulse = breathing(out.phase_ms);
        out.speed_demo = static_cast<uint8_t>(out.phase_ms * 88U / 6000U);
        out.lit = static_cast<uint8_t>(out.speed_demo * 16U / 88U);
    } else if (elapsed < 14500U) {
        out.phase = ShowPhase::LINK;
        out.phase_ms = elapsed - 8000U;
        out.route_permille = static_cast<uint16_t>(500U + out.phase_ms * 500U / 6500U);
        // Start the signature moment on a shared full-brightness peak.
        out.pulse = breathing(out.phase_ms + 800U);
        out.lit = static_cast<uint8_t>(4U + static_cast<uint16_t>(out.pulse) * 12U / 255U);
        out.speed_demo = static_cast<uint8_t>(72U + out.pulse * 16U / 255U);
    } else {
        out.phase = ShowPhase::FINISH;
        out.phase_ms = elapsed - 14500U;
        out.route_permille = 1000;
        out.pulse = static_cast<uint8_t>(255U - out.phase_ms * 255U / 3500U);
        out.lit = 16;
    }
    out.checkpoint = static_cast<uint8_t>(1U + out.route_permille * 3U / 1000U);
    out.beat = out.pulse >= 230U;
    return out;
}
