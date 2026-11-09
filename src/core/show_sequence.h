#pragma once

#include <stdint.h>

// One clock and one immutable frame drive both displays. No timers, allocation,
// hardware access or vehicle-state changes belong in this demonstration.
enum class ShowPhase : uint8_t { IDLE, ACQUIRE, ROUTE, LINK, FINISH };

struct ShowFrame {
    uint32_t elapsed_ms = 0;
    uint32_t phase_ms = 0;
    uint16_t progress_permille = 0;
    uint16_t route_permille = 0;
    ShowPhase phase = ShowPhase::IDLE;
    uint8_t pulse = 0;
    uint8_t lit = 0;
    uint8_t speed_demo = 0;
    uint8_t checkpoint = 1;
    bool active = false;
    bool beat = false;
};

class ShowSequence {
public:
    static constexpr uint32_t kDurationMs = 18000;
    void start(uint32_t now_ms);
    void stop();
    void update(uint32_t now_ms);
    bool active() const { return _active; }
    ShowFrame frame(uint32_t now_ms) const;

private:
    uint32_t _started_ms = 0;
    bool _active = false;
};
