#pragma once

#include <stdint.h>
#include "state.h"
#include "race.h"
#include "esc_signal.h"

// A display-only interpretation of the receiver command. Speed, RPM, gears
// and distance are theatrical values, not ESC telemetry or measurements.
// Never writes to a receiver, servo or motor output.
class EscDashboardController {
public:
    EscDashboardController();
    void reset(uint32_t now_ms = 0);
    void update(uint32_t now_ms, bool enabled, bool valid,
                float normalized, EscDriveState drive_state);

    const VehicleState& state() const { return _state; }
    const UiFx& fx() const { return _fx; }
    const RunStats& stats() const { return _stats; }
    bool enabled() const { return _enabled; }
    bool signalValid() const { return _valid; }

private:
    VehicleState _state;
    UiFx _fx;
    RunStats _stats;
    uint32_t _last_ms = 0;
    uint32_t _gear_changed_ms = 0;
    uint16_t _redline_hold_ms = 0;
    uint16_t _shift_remaining_ms = 0;
    bool _has_update = false;
    bool _enabled = false;
    bool _valid = false;
    bool _gear_animating = false;

    void forward(float amount, uint32_t dt_ms, uint32_t now_ms);
    void rest();
    void setGear(int gear, uint32_t now_ms);
};
