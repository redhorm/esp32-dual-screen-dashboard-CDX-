#pragma once
#include <stdint.h>

// The display-family segment check and banner are adapted to two small TFTs.
// This 5.6-second duration is a product choice, not a measured HDS boot time.
enum class StartupPhase : uint8_t { IDLE, SEGMENT_TEST, IDENTITY, CHART, READY };

struct StartupFrame {
    uint32_t elapsed_ms = 0;
    uint32_t phase_ms = 0;
    StartupPhase phase = StartupPhase::IDLE;
    bool active = false;
};

class StartupSequence {
public:
    static constexpr uint32_t kSegmentTestEndMs = 1000;
    static constexpr uint32_t kIdentityEndMs = 3000;
    static constexpr uint32_t kPilotIdentityEndMs = 4000;
    static constexpr uint32_t kChartEndMs = 4600;
    static constexpr uint32_t kDurationMs = 5600;

    void start(uint32_t now_ms) { started_ms_ = now_ms; active_ = true; }
    void stop() { active_ = false; }
    void update(uint32_t now_ms) {
        if (active_ && uint32_t(now_ms - started_ms_) >= kDurationMs) stop();
    }
    bool active() const { return active_; }
    StartupFrame frame(uint32_t now_ms) const {
        StartupFrame out;
        if (!active_) return out;
        const uint32_t elapsed = now_ms - started_ms_;
        if (elapsed >= kDurationMs) return out;
        out.active = true;
        out.elapsed_ms = elapsed;
        if (elapsed < kSegmentTestEndMs) {
            out.phase = StartupPhase::SEGMENT_TEST;
            out.phase_ms = elapsed;
        } else if (elapsed < kIdentityEndMs) {
            out.phase = StartupPhase::IDENTITY;
            out.phase_ms = elapsed - kSegmentTestEndMs;
        } else if (elapsed < kChartEndMs) {
            out.phase = StartupPhase::CHART;
            out.phase_ms = elapsed - kIdentityEndMs;
        } else {
            out.phase = StartupPhase::READY;
            out.phase_ms = elapsed - kChartEndMs;
        }
        return out;
    }
private:
    uint32_t started_ms_ = 0;
    bool active_ = false;
};
