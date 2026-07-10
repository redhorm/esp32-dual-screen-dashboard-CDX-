#include "race.h"
#include "anim.h"
#include "config.h"

void RaceController::reset(uint32_t now_ms) {
    _phase          = Phase::BOOT;
    _phase_t0       = now_ms;
    _last_ms        = now_ms;
    _prev_gear      = 1;
    _gear_change_ms = 0;
    _alert_on_ms    = 0;
    _alert_off_ms   = 0;
    _alert_active   = false;
    _alert_ever_off = false;
    _stats          = RunStats{};
    _fx             = UiFx{};
}

void RaceController::_enter(Phase p, uint32_t now_ms) {
    _phase    = p;
    _phase_t0 = now_ms;
}

void RaceController::_restartLap(Simulator& sim, Roadbook& rb) {
    sim.state.odo_mi = 0.0f;
    rb.reset();
    rb.update(sim.state);
    _stats = RunStats{};
}

void RaceController::_scanAlert(const Simulator& sim, const Roadbook& rb) {
    bool found     = false;
    _fx.alert_wp   = -1;
    _fx.alert_dist = 0.0f;

    if (_phase == Phase::LIVE) {
        // Waypoints are sorted by total_mi: stop at the first out of range.
        for (int j = sim.state.active_wp + 1; j < rb.getCount(); j++) {
            float d = rb.get(j).total_mi - sim.state.odo_mi;
            if (d > ALERT_DIST_MI) break;
            if (d > 0.0f && rb.get(j).danger) {
                found          = true;
                _fx.alert_wp   = j;
                _fx.alert_dist = d;
                _fx.alert_info = rb.get(j).info;  // kept after the alert
                _fx.alert_note = rb.get(j).note;  // ends (banner fade-out)
                break;
            }
        }
    }

    uint32_t now = _fx.now_ms;
    if (found && !_alert_active)  _alert_on_ms  = now;
    if (!found && _alert_active) { _alert_off_ms = now; _alert_ever_off = true; }
    _alert_active = found;

    _fx.alert         = found;
    _fx.alert_age_ms  = found ? now - _alert_on_ms : 0;
    _fx.alert_gone_ms = found ? 0xFFFFFFFF
                     : (_alert_ever_off ? now - _alert_off_ms : 0xFFFFFFFF);
}

void RaceController::update(uint32_t now_ms, Simulator& sim, Roadbook& rb) {
    uint32_t dt = now_ms - _last_ms;
    _last_ms = now_ms;

    switch (_phase) {

    case Phase::BOOT:
        sim.setInputs(0.0f, 0.0f);          // engine idles during boot
        sim.update(dt);
        if (now_ms - _phase_t0 >= BOOT_TOTAL_MS) {
            _enter(Phase::LIVE, now_ms);
            _restartLap(sim, rb);
        }
        break;

    case Phase::LIVE: {
#if DEMO_MODE
        DriveInputs in = _driver.compute(sim.state, rb);
        sim.setInputs(in.throttle, in.brake);
#else
        sim.releaseInputs();                // fixed autopilot cycle
#endif
        sim.update(dt);
        rb.update(sim.state);

        _stats.stage_ms += dt;
        if (sim.state.speed_mph > _stats.max_speed_mph)
            _stats.max_speed_mph = sim.state.speed_mph;
        if (sim.state.rpm > _stats.max_rpm)
            _stats.max_rpm = sim.state.rpm;

        if (sim.state.gear != _prev_gear) {
            _gear_change_ms = now_ms;
            _prev_gear      = sim.state.gear;
        }

        if (sim.state.odo_mi >= rb.stageTotal())
            _enter(Phase::FINISH, now_ms);
        break;
    }

    case Phase::FINISH:
        sim.setInputs(0.0f, 1.0f);          // brake to a stop, hold result
        sim.update(dt);
        if (now_ms - _phase_t0 >= FINISH_HOLD_MS) {
            _enter(Phase::LIVE, now_ms);
            _restartLap(sim, rb);
            _prev_gear = sim.state.gear;
        }
        break;
    }

    _fx.phase    = _phase;
    _fx.now_ms   = now_ms;
    _fx.phase_ms = now_ms - _phase_t0;
    _fx.redline  = (_phase == Phase::LIVE) && (sim.state.rpm >= REDLINE_RPM);
    _fx.gear_snap_p = (_gear_change_ms == 0)
        ? 1.0f
        : Anim::progress(now_ms, _gear_change_ms, GEAR_SNAP_MS);
    _scanAlert(sim, rb);
}
