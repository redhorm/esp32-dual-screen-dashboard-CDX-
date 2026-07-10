#pragma once
#include "state.h"

class Simulator {
public:
    VehicleState state;

    void reset();
    // dt_ms: milliseconds since last call
    void update(uint32_t dt_ms);

    // External driver (demo mode): overrides the internal autopilot
    // until releaseInputs() is called.
    void setInputs(float throttle, float brake);
    void releaseInputs() { _external = false; }

    float throttle() const { return _throttle; }
    float brake()    const { return _brake; }

private:
    uint32_t _elapsed_ms    = 0;
    uint32_t _last_shift_ms = 0;   // sequential-box shift cooldown
    bool     _external   = false;
    float    _bat_base   = 14.4f;  // slow-moving battery voltage
    float    _bat_sag_v  = 0.0f;   // transient voltage sag under load
    float    _rpm_jit    = 0.0f;   // last applied RPM jitter (stripped each step)

    void _updateAutopilot();
    void _updatePowertrain(float dt_s);
    void _updateThermal(float dt_s);
    void _updateNav(float dt_s);

    float _throttle = 0.0f;
    float _brake    = 0.0f;
};
