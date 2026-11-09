#include "esc_dashboard.h"
#include "config.h"
#include <math.h>

namespace {
// Scenic ratios: approximately the same RPM drop in each forward upshift.
// These are display speeds, never a speed measurement or motor command.
constexpr float GEAR_SPEED_MPH[5] = {17.0f, 27.0f, 43.0f, 66.0f, 100.0f};
constexpr uint32_t MIN_GEAR_MS = 800;
constexpr uint16_t REDLINE_HOLD_MS = 120;
constexpr uint16_t SHIFT_CUT_MS = 180;
float unit(float value) {
    return value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
}
float approach(float value, float target, float step) {
    if (value < target) return value + step < target ? value + step : target;
    return value - step > target ? value - step : target;
}
}  // namespace

EscDashboardController::EscDashboardController() {
    reset(0);
    _has_update = false;
}

void EscDashboardController::reset(uint32_t now_ms) {
    _state = VehicleState{};
    _state.drive_auto = false;
    _stats = RunStats{};
    _fx = UiFx{};
    _fx.phase = Phase::LIVE;
    _fx.now_ms = now_ms;
    _last_ms = now_ms;
    _gear_changed_ms = now_ms;
    _has_update = true;
    _enabled = false;
    _valid = false;
    _gear_animating = false;
    rest();
}

void EscDashboardController::rest() {
    _state.speed_mph = 0.0f;
    _state.rpm = 0.0f;
    _state.gear = 0;
    _fx.redline = false;
    _fx.gear_snap_p = 1.0f;
    _gear_animating = false;
    _redline_hold_ms = 0;
    _shift_remaining_ms = 0;
}

void EscDashboardController::setGear(int gear, uint32_t now_ms) {
    if (_state.gear == gear) return;
    _state.gear = gear;
    _gear_changed_ms = now_ms;
    _gear_animating = true;
}

void EscDashboardController::forward(float amount, uint32_t dt_ms, uint32_t now_ms) {
    if (_state.gear <= 0) setGear(1, now_ms);
    const float dt = static_cast<float>(dt_ms) / 1000.0f;
    const float gear_limit = GEAR_SPEED_MPH[_state.gear - 1];
    const float requested_speed = amount * GEAR_SPEED_MPH[4];
    const float target_speed = requested_speed < gear_limit ? requested_speed : gear_limit;
    const bool cutting = _shift_remaining_ms != 0;
    const float acceleration = cutting ? 8.0f : 12.0f + amount * 10.0f;
    const float rate = target_speed < _state.speed_mph ? 45.0f : acceleration;
    _state.speed_mph = approach(_state.speed_mph, target_speed, rate * dt);

    if (cutting) {
        // A short torque-cut animation makes the RPM fall readable while the
        // vehicle display continues moving through the shift.
        _state.rpm = 4500.0f;
        _shift_remaining_ms = dt_ms >= _shift_remaining_ms ? 0 :
            static_cast<uint16_t>(_shift_remaining_ms - dt_ms);
        return;
    }
    float target_rpm = 800.0f + 6400.0f * unit(_state.speed_mph / gear_limit);
    const bool limiter = _state.gear == 5 && amount >= 0.98f &&
                         _state.speed_mph >= 99.5f;
    if (limiter) {
        // Deterministic limiter cut: full 16-aperture illumination at 7200 RPM,
        // then a small 7000 RPM drop; never outruns the calibrated throttle.
        target_rpm = now_ms % 280u < 160u ? 7200.0f : 7000.0f;
    }
    _state.rpm = approach(_state.rpm, target_rpm, 18000.0f * dt);

    if (_state.gear < 5 && _state.rpm >= 7190.0f) {
        _redline_hold_ms = static_cast<uint16_t>(_redline_hold_ms + dt_ms);
        if (_redline_hold_ms >= REDLINE_HOLD_MS &&
            static_cast<uint32_t>(now_ms - _gear_changed_ms) >= MIN_GEAR_MS) {
            setGear(_state.gear + 1, now_ms);
            _state.rpm = 4500.0f;
            _shift_remaining_ms = SHIFT_CUT_MS;
            _redline_hold_ms = 0;
        }
    } else {
        _redline_hold_ms = 0;
        // Wide shift hysteresis prevents hunting under a steady partial trigger.
        if (_state.gear > 1 && _state.rpm < 3000.0f &&
            static_cast<uint32_t>(now_ms - _gear_changed_ms) >= MIN_GEAR_MS) {
            setGear(_state.gear - 1, now_ms);
            _state.rpm = 800.0f + 6400.0f *
                unit(_state.speed_mph / GEAR_SPEED_MPH[_state.gear - 1]);
        }
    }
}

void EscDashboardController::update(uint32_t now_ms, bool enabled, bool valid,
                                    float normalized, EscDriveState drive_state) {
    uint32_t dt_ms = _has_update ? now_ms - _last_ms : 0;
    if (dt_ms > 100) dt_ms = 100;
    _last_ms = now_ms;
    _has_update = true;
    _fx.phase = Phase::LIVE;
    _fx.now_ms = now_ms;
    _enabled = enabled;
    _valid = enabled && valid && isfinite(normalized);

    // Disabling this source clears its own history; selecting demo is the
    // caller's responsibility. Lost input must never select demo implicitly.
    if (!enabled) {
        rest();
        _state.odo_mi = 0.0f;
        _stats = RunStats{};
        _fx.phase_ms = 0;
        return;
    }
    if (!_valid) {
        rest();
        return;
    }

    const float dt = static_cast<float>(dt_ms) / 1000.0f;
    float amount = 0.0f;
    int direction = 0;
    if (drive_state == EscDriveState::FORWARD && normalized > 0.0f) {
        amount = unit(normalized);
        direction = 1;
    } else if (drive_state == EscDriveState::REVERSE && normalized < 0.0f) {
        amount = unit(-normalized);
        direction = -1;
    }

    if (drive_state == EscDriveState::BRAKING) {
        _redline_hold_ms = 0;
        _shift_remaining_ms = 0;
        // Brake is a deceleration of the previous displayed motion, never
        // permission to display reverse. Both reach zero in finite time.
        _state.speed_mph = approach(_state.speed_mph, 0.0f, 70.0f * dt);
        _state.rpm = approach(_state.rpm, 0.0f, 18000.0f * dt);
        if (_state.speed_mph <= 0.0f) {
            _state.rpm = 0.0f;
            setGear(0, now_ms);
        }
    } else if (direction != 0) {
        // A direction change starts the new visual motion from rest. It
        // cannot inherit a high forward speed as a high reverse speed.
        if ((_state.gear < 0 && direction > 0) ||
            (_state.gear > 0 && direction < 0)) rest();
        if (direction > 0) {
            forward(amount, dt_ms, now_ms);
        } else {
            // Preserve the established crawler reverse response: one R gear.
            _redline_hold_ms = 0;
            _shift_remaining_ms = 0;
            _state.speed_mph = approach(_state.speed_mph, amount * 20.0f, 35.0f * dt);
            _state.rpm = approach(_state.rpm, 800.0f + amount * 6400.0f, 18000.0f * dt);
            setGear(-1, now_ms);
        }
    } else {
        // Neutral, reverse-arming and contradictory/zero input all stop the
        // display immediately, including the RPM strip. No autonomous lap.
        rest();
    }

    _fx.redline = _state.rpm >= REDLINE_RPM;
    if (_gear_animating) {
        const uint32_t age = now_ms - _gear_changed_ms;
        _fx.gear_snap_p = age >= GEAR_SNAP_MS ? 1.0f :
            static_cast<float>(age) / static_cast<float>(GEAR_SNAP_MS);
        if (age >= GEAR_SNAP_MS) _gear_animating = false;
    }
    _stats.stage_ms += dt_ms;
    _fx.phase_ms = _stats.stage_ms;
    _state.odo_mi += _state.speed_mph * dt / 3600.0f;
    if (_state.speed_mph > _stats.max_speed_mph)
        _stats.max_speed_mph = _state.speed_mph;
    if (_state.rpm > _stats.max_rpm) _stats.max_rpm = _state.rpm;
}
