#include "simulator.h"
#include "noise.h"
#include "config.h"
#include <math.h>

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Gear thresholds indexed by the CURRENT gear: UP[g] = speed to leave
// gear g upward, DOWN[g] = speed to leave it downward. Hysteresis:
// after a downshift into g, UP[g] must still be above the current
// speed or the box chatters (the old table had UP[1]=0: first gear
// upshifted at ANY speed — that was the visible gear flicker).
static const float UP_MPH[SIM_NUM_GEARS + 1]   = {0, 22, 35, 49, 66, 999};
static const float DOWN_MPH[SIM_NUM_GEARS + 1] = {0,  0, 19, 30, 44,  61};
// Race gearing: each gear revs PAST the 7200 limiter target before its
// up-shift speed, so the engine bounces on the limiter at every shift,
// then drops to ~4-5k in the next gear. Top gear cruises 70 mph @ ~4.2k.
static const float GEAR_RATIO[SIM_NUM_GEARS + 1] = {0, 3.6f, 2.2f, 1.55f, 1.12f, 0.67f};
static const float RPM_PER_MPH = 90.0f;

void Simulator::reset() {
    state = VehicleState{};
    _elapsed_ms   = 0;
    _last_shift_ms = 0;
    _external   = false;
    _bat_base   = state.battery_v;
    _bat_sag_v  = 0.0f;
    _rpm_jit    = 0.0f;
    _throttle   = 0.0f;
    _brake      = 0.0f;
}

void Simulator::setInputs(float throttle, float brake) {
    _external = true;
    _throttle = clampf(throttle, 0.0f, 1.0f);
    _brake    = clampf(brake,    0.0f, 1.0f);
}

void Simulator::update(uint32_t dt_ms) {
    if (dt_ms == 0) return;
    if (dt_ms > 200) dt_ms = 200; // clamp large jumps (first frame / resume)

    _elapsed_ms += dt_ms;
    float dt_s = dt_ms * 0.001f;

    if (!_external) _updateAutopilot();
    _updatePowertrain(dt_s);
    _updateThermal(dt_s);
    _updateNav(dt_s);
}

// ── Autopilot ────────────────────────────────────────────────────────────────
// 8-second cycle: 3s hard accel → 2s cruise → 2s brake → 1s idle
void Simulator::_updateAutopilot() {
    uint32_t t = _elapsed_ms % 8000;
    if (t < 3000) {
        _throttle = 0.88f;
        _brake    = 0.0f;
    } else if (t < 5000) {
        _throttle = 0.28f;
        _brake    = 0.0f;
    } else if (t < 7000) {
        _throttle = 0.0f;
        _brake    = 0.75f;
    } else {
        _throttle = 0.0f;
        _brake    = 0.0f;
    }
}

// ── Powertrain ───────────────────────────────────────────────────────────────
void Simulator::_updatePowertrain(float dt_s) {
    // Speed
    float drag   = state.speed_mph * 0.016f;
    float accel  = _throttle * 9.0f - _brake * 18.0f - drag;
    state.speed_mph += accel * dt_s;
    state.speed_mph = clampf(state.speed_mph, 0.0f, SIM_MAX_SPEED_MPH);

    // Gear logic — sequential box: one shift, then a cooldown
    if (_elapsed_ms - _last_shift_ms >= SIM_SHIFT_COOLDOWN_MS) {
        if (state.speed_mph >= UP_MPH[state.gear] && state.gear < SIM_NUM_GEARS) {
            state.gear++;
            _last_shift_ms = _elapsed_ms;
        } else if (state.speed_mph < DOWN_MPH[state.gear] && state.gear > 1) {
            state.gear--;
            _last_shift_ms = _elapsed_ms;
        }
    }

    // RPM: blend engine speed from wheel speed + direct throttle blip
    float wheel_rpm = state.speed_mph * GEAR_RATIO[state.gear] * RPM_PER_MPH;
    float blip      = _throttle * 900.0f;
    float target    = (_throttle > 0.05f) ? (wheel_rpm + blip)
                                           : SIM_IDLE_RPM;
    // Low-pass filter for smooth RPM, then deterministic jitter on top.
    // Last frame's jitter is stripped first so noise never accumulates
    // through the filter.
    state.rpm -= _rpm_jit;
    float alpha = 1.0f - expf(-dt_s * 4.0f);
    state.rpm += (target - state.rpm) * alpha;
    _rpm_jit = Noise::smooth(_elapsed_ms, SIM_RPM_JITTER_MS, SIM_NOISE_SEED)
             * SIM_RPM_JITTER * (0.4f + 0.6f * state.rpm / SIM_MAX_RPM);
    state.rpm = clampf(state.rpm + _rpm_jit, SIM_IDLE_RPM * 0.9f, SIM_MAX_RPM);

    // Fuel drain
    state.fuel_pct -= _throttle * 2e-6f * (dt_s * 1000.0f);
    state.fuel_pct  = clampf(state.fuel_pct, 0.0f, 1.0f);

    // Battery: slow-moving base (drain under load, light recharge) plus a
    // transient sag that flexes fast under heavy throttle and recovers slowly.
    float load = _throttle * 0.4f + 0.08f;
    _bat_base -= load * 1e-4f * (dt_s * 1000.0f);
    if (_throttle > 0.15f && _throttle < 0.75f && _bat_base < 14.0f)
        _bat_base += 4e-5f * (dt_s * 1000.0f);
    _bat_base = clampf(_bat_base, 11.0f, 14.6f);

    float sag_target = SIM_BAT_SAG_V
                     * clampf((_throttle - 0.5f) * 2.0f, 0.0f, 1.0f);
    float sag_tau = (sag_target > _bat_sag_v) ? SIM_BAT_SAG_UP_MS
                                              : SIM_BAT_SAG_DN_MS;
    _bat_sag_v += (sag_target - _bat_sag_v)
                * (1.0f - expf(-(dt_s * 1000.0f) / sag_tau));

    state.battery_v = clampf(_bat_base - _bat_sag_v, 11.0f, 14.6f);
}

// ── Thermal ──────────────────────────────────────────────────────────────────
// Real thermal inertia: heats up quickly under load, cools down slowly.
void Simulator::_updateThermal(float dt_s) {
    float target = 155.0f + _throttle * 105.0f + _brake * 18.0f;
    float rate   = (target > state.engine_temp_f) ? SIM_TEMP_HEAT_RATE
                                                  : SIM_TEMP_COOL_RATE;
    float alpha  = 1.0f - expf(-dt_s * rate);
    state.engine_temp_f += (target - state.engine_temp_f) * alpha;
    state.engine_temp_f  = clampf(state.engine_temp_f, 140.0f, 265.0f);

    // ESC: smaller thermal mass — spikes faster under load, same slow cooling
    float esc_target = 125.0f + _throttle * 115.0f;
    float esc_rate   = (esc_target > state.esc_temp_f) ? SIM_ESC_HEAT_RATE
                                                       : SIM_TEMP_COOL_RATE;
    float esc_alpha  = 1.0f - expf(-dt_s * esc_rate);
    state.esc_temp_f += (esc_target - state.esc_temp_f) * esc_alpha;
    state.esc_temp_f  = clampf(state.esc_temp_f, 110.0f, 250.0f);
}

// ── Navigation ───────────────────────────────────────────────────────────────
void Simulator::_updateNav(float dt_s) {
    // Odometer: speed_mph → miles/s
    state.odo_mi += state.speed_mph * dt_s / 3600.0f;

    // Heading rotates slowly (simulated trail turns)
    state.heading_deg += 3.5f * dt_s;
    if (state.heading_deg >= 360.0f) state.heading_deg -= 360.0f;
}
