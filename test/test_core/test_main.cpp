// ============================================================
// Host-compiled unit tests (pio test -e native)
// Covers: roadbook progression, physics simulator, formatting.
// Pure C++ — no Arduino, no LovyanGFX.
// ============================================================
#include <unity.h>
#include <string.h>
#include "config.h"
#include "core/simulator.h"
#include "core/roadbook.h"
#include "core/formatter.h"
#include "core/driver.h"
#include "core/race.h"
#include "core/anim.h"
#include "core/easing.h"
#include "core/noise.h"
#include "core/compass.h"
#include "core/ui_settings.h"
#include "core/rock_line_game.h"
#include "core/experience.h"
#include "core/rtl_logic.h"
#include <type_traits>

static_assert(std::is_trivially_copyable<RockGameSnapshot>::value,
              "game snapshot must remain safe for SharedFrame copies");
static_assert(std::is_trivially_copyable<ExperienceFrame>::value,
              "experience frame must remain safe for SharedFrame copies");
static_assert(sizeof(RockGameSnapshot) <= 64,
              "game snapshot must remain lightweight");
static_assert(std::is_trivially_copyable<RtlFrame>::value,
              "RTL frame must remain safe for deterministic copies");

void setUp() {}
void tearDown() {}

// ── Roadbook ─────────────────────────────────────────────────────────────────

static void test_roadbook_initial_state() {
    Roadbook rb;
    VehicleState s;
    rb.update(s);
    TEST_ASSERT_EQUAL_INT(0, s.active_wp);
    TEST_ASSERT_EQUAL_INT(9, rb.getCount());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.45f, s.dist_to_next);
    TEST_ASSERT_FALSE(rb.isFinished());
}

static void test_roadbook_advances_waypoint() {
    Roadbook rb;
    VehicleState s;
    s.odo_mi = 0.50f;           // past WP1 @ 0.45
    rb.update(s);
    TEST_ASSERT_EQUAL_INT(1, s.active_wp);

    s.odo_mi = 1.22f;           // just before WP2 @ 1.23
    rb.update(s);
    TEST_ASSERT_EQUAL_INT(1, s.active_wp);

    s.odo_mi = 1.24f;           // past WP2
    rb.update(s);
    TEST_ASSERT_EQUAL_INT(2, s.active_wp);
}

static void test_roadbook_skips_multiple_waypoints() {
    Roadbook rb;
    VehicleState s;
    s.odo_mi = 9.0f;            // between WP6 @ 8.34 and WP7 @ 9.78
    rb.update(s);
    TEST_ASSERT_EQUAL_INT(6, s.active_wp);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 9.78f - 9.0f, s.dist_to_next);
}

static void test_roadbook_finish_detection() {
    // Lap looping is now owned by RaceController: past the finish the
    // roadbook just reports the last waypoint as active/finished.
    Roadbook rb;
    VehicleState s;
    s.odo_mi = 12.10f;          // past FINISH @ 12.05
    rb.update(s);
    TEST_ASSERT_EQUAL_INT(rb.getCount() - 1, s.active_wp);
    TEST_ASSERT_TRUE(rb.isFinished());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, s.dist_to_next);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.10f, s.odo_mi);  // odo untouched
}

static void test_roadbook_reset() {
    Roadbook rb;
    VehicleState s;
    s.odo_mi = 6.0f;
    rb.update(s);
    TEST_ASSERT_TRUE(s.active_wp > 0);
    rb.reset();
    TEST_ASSERT_EQUAL_INT(0, rb.getActive());
}

static void test_roadbook_stage_data_consistency() {
    // total_mi must be monotonically increasing and match segment sums
    Roadbook rb;
    float sum = 0.0f;
    for (int i = 0; i < rb.getCount(); i++) {
        sum += rb.get(i).dist_mi;
        TEST_ASSERT_FLOAT_WITHIN(0.005f, rb.get(i).total_mi, sum);
        if (i > 0)
            TEST_ASSERT_TRUE(rb.get(i).total_mi > rb.get(i - 1).total_mi);
    }
}

// ── Simulator ────────────────────────────────────────────────────────────────

static void test_sim_reset_defaults() {
    Simulator sim;
    sim.reset();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, sim.state.speed_mph);
    TEST_ASSERT_EQUAL_INT(1, sim.state.gear);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, sim.state.odo_mi);
}

static void test_sim_zero_dt_is_noop() {
    Simulator sim;
    sim.reset();
    VehicleState before = sim.state;
    sim.update(0);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, before.speed_mph, sim.state.speed_mph);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, before.rpm, sim.state.rpm);
}

static void test_sim_accelerates_from_standstill() {
    Simulator sim;
    sim.reset();
    // 2 s into the autopilot cycle = hard acceleration phase
    for (int i = 0; i < 20; i++) sim.update(100);
    TEST_ASSERT_TRUE(sim.state.speed_mph > 5.0f);
    TEST_ASSERT_TRUE(sim.state.rpm > SIM_IDLE_RPM);
}

static void test_sim_gears_follow_speed() {
    Simulator sim;
    sim.reset();
    int max_gear = 1;
    float max_speed = 0.0f;
    // Two full autopilot cycles (16 s)
    for (int i = 0; i < 160; i++) {
        sim.update(100);
        if (sim.state.gear > max_gear)          max_gear  = sim.state.gear;
        if (sim.state.speed_mph > max_speed)    max_speed = sim.state.speed_mph;
    }
    TEST_ASSERT_TRUE(max_speed > 15.0f);
    TEST_ASSERT_TRUE(max_gear >= 2);
    // After the braking phase speed must come back down near zero
    TEST_ASSERT_TRUE(sim.state.speed_mph < max_speed);
}

static void test_sim_values_stay_in_bounds() {
    Simulator sim;
    sim.reset();
    for (int i = 0; i < 600; i++) {   // 60 s
        sim.update(100);
        TEST_ASSERT_TRUE(sim.state.speed_mph >= 0.0f);
        TEST_ASSERT_TRUE(sim.state.speed_mph <= SIM_MAX_SPEED_MPH);
        TEST_ASSERT_TRUE(sim.state.rpm <= SIM_MAX_RPM);
        TEST_ASSERT_TRUE(sim.state.battery_v >= 11.0f);
        TEST_ASSERT_TRUE(sim.state.battery_v <= 14.6f);
        TEST_ASSERT_TRUE(sim.state.engine_temp_f >= 140.0f);
        TEST_ASSERT_TRUE(sim.state.engine_temp_f <= 265.0f);
        TEST_ASSERT_TRUE(sim.state.gear >= 1);
        TEST_ASSERT_TRUE(sim.state.gear <= SIM_NUM_GEARS);
    }
}

static void test_sim_odometer_advances() {
    Simulator sim;
    sim.reset();
    float prev = 0.0f;
    for (int i = 0; i < 100; i++) {
        sim.update(100);
        TEST_ASSERT_TRUE(sim.state.odo_mi >= prev);   // monotonic
        prev = sim.state.odo_mi;
    }
    TEST_ASSERT_TRUE(sim.state.odo_mi > 0.0f);
}

static void test_sim_dt_clamp() {
    Simulator sim;
    sim.reset();
    sim.update(60000);   // absurd dt (resume after pause) must not explode
    TEST_ASSERT_TRUE(sim.state.speed_mph <= SIM_MAX_SPEED_MPH);
    TEST_ASSERT_TRUE(sim.state.odo_mi < 0.1f);   // clamped to 200 ms
}

static void test_sim_gear_no_chatter_at_steady_speed() {
    // 32 mph sits inside the hysteresis band (DOWN[3]=30, UP[2]=35);
    // 12 mph is the hairpin case that flickered 1↔2 with the old table.
    const float speeds[] = {32.0f, 12.0f};
    for (float hold : speeds) {
        Simulator sim;
        sim.reset();
        sim.setInputs(0.2f, 0.0f);
        for (int i = 0; i < 150; i++) {          // 5 s to settle the gear
            sim.state.speed_mph = hold;
            sim.update(33);
        }
        int changes = 0, prev = sim.state.gear;
        for (int i = 0; i < 300; i++) {          // observe 10 s
            sim.state.speed_mph = hold;
            sim.update(33);
            if (sim.state.gear != prev) { changes++; prev = sim.state.gear; }
        }
        TEST_ASSERT_EQUAL_INT(0, changes);
    }
}

static void test_sim_shift_cooldown_spacing() {
    Simulator sim;
    sim.reset();
    sim.setInputs(0.95f, 0.0f);
    uint32_t t = 0;
    while (sim.state.gear < 4 && t < 30000) { sim.update(33); t += 33; }
    TEST_ASSERT_EQUAL_INT(4, sim.state.gear);

    // slam the speed down: downshifts must step SIM_SHIFT_COOLDOWN_MS apart
    sim.setInputs(0.0f, 0.0f);
    sim.state.speed_mph = 10.0f;
    int prev = sim.state.gear;
    uint32_t last_change = 0, min_gap = 0xFFFFFFFF, changes = 0;
    for (uint32_t e = 0; e < 5000; e += 33) {
        sim.state.speed_mph = 10.0f;     // hold
        sim.update(33);
        if (sim.state.gear != prev) {
            if (changes > 0 && e - last_change < min_gap) min_gap = e - last_change;
            last_change = e;
            changes++;
            prev = sim.state.gear;
        }
    }
    TEST_ASSERT_EQUAL_INT(1, sim.state.gear);          // walked 4→3→2→1
    TEST_ASSERT_TRUE(changes >= 3);
    TEST_ASSERT_TRUE(min_gap >= SIM_SHIFT_COOLDOWN_MS - 40);
}

static void test_sim_hits_rev_limiter_before_shifts() {
    Simulator sim;
    sim.reset();
    sim.setInputs(0.95f, 0.0f);
    float max_rpm = 0.0f;
    for (int i = 0; i < 360; i++) {      // 12 s of hard acceleration
        sim.update(33);
        if (sim.state.rpm > max_rpm) max_rpm = sim.state.rpm;
    }
    TEST_ASSERT_TRUE(max_rpm >= REDLINE_RPM);   // bounces on the limiter
    TEST_ASSERT_TRUE(sim.state.gear >= 4);      // and still shifts up
}

// ── Driver (roadbook-aware autopilot) ────────────────────────────────────────

static void test_driver_brakes_into_danger_waypoint() {
    Roadbook rb;
    VehicleState s;
    s.odo_mi = 5.64f;            // 0.03 mi before WP5 (L HAIRPIN @ 5.67)
    rb.update(s);
    TEST_ASSERT_EQUAL_INT(4, s.active_wp);
    s.speed_mph = 60.0f;

    Driver drv;
    DriveInputs in = drv.compute(s, rb);
    TEST_ASSERT_TRUE(in.brake > 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, in.throttle);
}

static void test_driver_accelerates_on_straight() {
    Roadbook rb;
    VehicleState s;
    s.odo_mi = 6.0f;             // 2.34 mi of room before WP6 (R 90 @ 8.34)
    rb.update(s);
    s.speed_mph = 30.0f;

    Driver drv;
    DriveInputs in = drv.compute(s, rb);
    TEST_ASSERT_TRUE(in.throttle > 0.8f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, in.brake);
}

static void test_driver_holds_corner_speed_at_apex() {
    Roadbook rb;
    VehicleState s;
    s.odo_mi = 5.655f;           // 0.015 mi before the hairpin, already slow
    rb.update(s);
    s.speed_mph = 11.0f;         // just under DRV_TGT_HAIRPIN_MPH

    Driver drv;
    DriveInputs in = drv.compute(s, rb);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, in.brake);
}

static void test_driver_corner_speed_mapping() {
    TEST_ASSERT_FLOAT_WITHIN(0.1f, DRV_TGT_HAIRPIN_MPH,
                             Driver::cornerSpeed(WaypointDir::HAIRPIN_L));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, DRV_TGT_90_MPH,
                             Driver::cornerSpeed(WaypointDir::RIGHT_90));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, DRV_TGT_WATER_MPH,
                             Driver::cornerSpeed(WaypointDir::WATER));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, DRV_CRUISE_MPH,
                             Driver::cornerSpeed(WaypointDir::FINISH));
}

// ── RaceController ───────────────────────────────────────────────────────────

// Steps the controller in DASH_FRAME_MS increments, returns final time
static uint32_t stepRace(RaceController& rc, Simulator& sim, Roadbook& rb,
                         uint32_t from_ms, uint32_t duration_ms) {
    uint32_t now = from_ms;
    uint32_t end = from_ms + duration_ms;
    while (now < end) {
        now += DASH_FRAME_MS;
        rc.update(now, sim, rb);
    }
    return now;
}

static void test_race_boot_then_live() {
    Simulator sim;  sim.reset();
    Roadbook rb;    rb.reset();
    RaceController rc;
    rc.reset(1000);

    rc.update(1000 + DASH_FRAME_MS, sim, rb);
    TEST_ASSERT_TRUE(rc.fx().phase == Phase::BOOT);

    stepRace(rc, sim, rb, 1000 + DASH_FRAME_MS, BOOT_TOTAL_MS + 100);
    TEST_ASSERT_TRUE(rc.fx().phase == Phase::LIVE);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, sim.state.odo_mi);
}

static void test_race_full_lap_finish_and_restart() {
    Simulator sim;  sim.reset();
    Roadbook rb;    rb.reset();
    RaceController rc;
    rc.reset(0);

    // Run up to 30 simulated minutes; the demo driver must complete
    // the 12.05 mi stage well within that.
    uint32_t now = stepRace(rc, sim, rb, 0, BOOT_TOTAL_MS + 100);
    bool finished = false;
    for (int i = 0; i < 30 * 60000 / DASH_FRAME_MS && !finished; i++) {
        now += DASH_FRAME_MS;
        rc.update(now, sim, rb);
        finished = rc.fx().phase == Phase::FINISH;
    }
    TEST_ASSERT_TRUE(finished);
    TEST_ASSERT_TRUE(rc.stats().max_speed_mph > 30.0f);
    TEST_ASSERT_TRUE(rc.stats().max_rpm > 4000.0f);
    TEST_ASSERT_TRUE(rc.stats().stage_ms > 60000);

    // After the hold the lap restarts from zero
    stepRace(rc, sim, rb, now, FINISH_HOLD_MS + 200);
    TEST_ASSERT_TRUE(rc.fx().phase == Phase::LIVE);
    TEST_ASSERT_TRUE(sim.state.odo_mi < 0.5f);
    TEST_ASSERT_EQUAL_INT(0, sim.state.active_wp);
    TEST_ASSERT_TRUE(rc.stats().stage_ms < 5000);
}

static void test_race_alert_near_danger_waypoint() {
    Simulator sim;  sim.reset();
    Roadbook rb;    rb.reset();
    RaceController rc;
    rc.reset(0);
    uint32_t now = stepRace(rc, sim, rb, 0, BOOT_TOTAL_MS + 100);

    // Far from any danger waypoint → no alert
    sim.state.odo_mi = 0.50f;
    now += DASH_FRAME_MS;
    rc.update(now, sim, rb);
    TEST_ASSERT_FALSE(rc.fx().alert);

    // 0.12 mi before WP2 (L 90 CAUTION @ 1.23, danger) → alert on
    sim.state.odo_mi = 1.11f;
    now += DASH_FRAME_MS;
    rc.update(now, sim, rb);
    TEST_ASSERT_TRUE(rc.fx().alert);
    TEST_ASSERT_EQUAL_INT(2, rc.fx().alert_wp);
    TEST_ASSERT_TRUE(rc.fx().alert_dist <= ALERT_DIST_MI);
    TEST_ASSERT_EQUAL_STRING("L 90", rc.fx().alert_info);
}

static void test_race_gear_snap_progress() {
    Simulator sim;  sim.reset();
    Roadbook rb;    rb.reset();
    RaceController rc;
    rc.reset(0);
    uint32_t now = stepRace(rc, sim, rb, 0, BOOT_TOTAL_MS + 100);

    // Drive until the first gear change happens
    float p_at_change = 1.0f;
    for (int i = 0; i < 20000 / DASH_FRAME_MS; i++) {
        now += DASH_FRAME_MS;
        rc.update(now, sim, rb);
        if (sim.state.gear > 1) { p_at_change = rc.fx().gear_snap_p; break; }
    }
    TEST_ASSERT_TRUE(sim.state.gear > 1);
    TEST_ASSERT_TRUE(p_at_change < 1.0f);   // snap animation just started

    // Well past GEAR_SNAP_MS (and no new change within a couple frames)
    now += GEAR_SNAP_MS + 3 * DASH_FRAME_MS;
    rc.update(now, sim, rb);
    int g = sim.state.gear;
    now += DASH_FRAME_MS;
    rc.update(now, sim, rb);
    if (sim.state.gear == g)
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, rc.fx().gear_snap_p);
}

// ── Anim helpers ─────────────────────────────────────────────────────────────

static void test_anim_progress() {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, Anim::progress(100, 100, 200));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, Anim::progress(200, 100, 200));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, Anim::progress(400, 100, 200));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, Anim::progress(50, 100, 0));
}

static void test_anim_pulse() {
    TEST_ASSERT_TRUE(Anim::pulse(0, 100));
    TEST_ASSERT_TRUE(Anim::pulse(99, 100));
    TEST_ASSERT_FALSE(Anim::pulse(100, 100));
    TEST_ASSERT_TRUE(Anim::pulse(200, 100));
}

static void test_anim_triangle_and_snap() {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, Anim::triangle(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, Anim::triangle(0.5f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, Anim::triangle(1.0f));

    TEST_ASSERT_FLOAT_WITHIN(0.001f, GEAR_SNAP_SCALE,
                             Anim::snapScale(0.0f, GEAR_SNAP_SCALE));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f,
                             Anim::snapScale(1.0f, GEAR_SNAP_SCALE));
    // monotonically decreasing
    float prev = GEAR_SNAP_SCALE;
    for (float t = 0.1f; t <= 1.0f; t += 0.1f) {
        float v = Anim::snapScale(t, GEAR_SNAP_SCALE);
        TEST_ASSERT_TRUE(v <= prev + 0.001f);
        prev = v;
    }
}

static void test_anim_lerp565() {
    TEST_ASSERT_EQUAL_INT(0x0000, Anim::lerp565(0x0000, 0xFFFF, 0.0f));
    TEST_ASSERT_EQUAL_INT(0xFFFF, Anim::lerp565(0x0000, 0xFFFF, 1.0f));
    uint16_t mid = Anim::lerp565(0x0000, 0xFFFF, 0.5f);
    int r = (mid >> 11) & 31, g = (mid >> 5) & 63, b = mid & 31;
    TEST_ASSERT_TRUE(r >= 14 && r <= 16);
    TEST_ASSERT_TRUE(g >= 30 && g <= 32);
    TEST_ASSERT_TRUE(b >= 14 && b <= 16);
}

// ── Easing / Follow / SlideAnim ──────────────────────────────────────────────

static void test_ease_curves() {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, Ease::inOutQuad(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, Ease::inOutQuad(0.5f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, Ease::inOutQuad(1.0f));
    // slow start: first quarter covers less than linear
    TEST_ASSERT_TRUE(Ease::inOutQuad(0.25f) < 0.25f);
    TEST_ASSERT_TRUE(Ease::inOutQuad(0.75f) > 0.75f);
}

static void test_follow_attack_release_asymmetry() {
    Follow f;
    f.update(0.0f, 0, 60.0f, 400.0f);      // primes at 0

    // Rising with tau=60ms: after 100ms it should be most of the way up
    float up = f.update(1.0f, 100, 60.0f, 400.0f);
    TEST_ASSERT_TRUE(up > 0.7f);

    // Falling with tau=400ms: after another 100ms it barely dropped
    float dn = f.update(0.0f, 200, 60.0f, 400.0f);
    TEST_ASSERT_TRUE(dn > up * 0.6f);
    // and it always converges eventually
    for (uint32_t t = 300; t <= 5000; t += 100)
        dn = f.update(0.0f, t, 60.0f, 400.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, dn);
}

static void test_follow_first_update_snaps() {
    Follow f;
    float v = f.update(42.0f, 12345, 100.0f, 100.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 42.0f, v);   // no lerp from stale 0
}

static void test_slide_anim() {
    SlideAnim s;
    s.setTarget(0, 1000, 260);                // primes without animating
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, s.pos(1000));

    s.setTarget(1, 2000, 260);                // 0 → 1 slide
    TEST_ASSERT_TRUE(s.active(2100));
    float mid = s.pos(2130);
    TEST_ASSERT_TRUE(mid > 0.0f && mid < 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, s.pos(2400));
    TEST_ASSERT_FALSE(s.active(2400));

    // retarget mid-flight must not jump backwards past current pos
    s.setTarget(2, 3000, 260);
    float p1 = s.pos(3100);
    s.setTarget(3, 3100, 260);
    float p2 = s.pos(3101);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, p1, p2);
}

// ── Noise (deterministic micro-life) ─────────────────────────────────────────

static void test_noise_deterministic_and_bounded() {
    for (uint32_t t = 0; t < 5000; t += 37) {
        float a = Noise::smooth(t, 120, 0xC0FFEE);
        float b = Noise::smooth(t, 120, 0xC0FFEE);
        TEST_ASSERT_FLOAT_WITHIN(0.0001f, a, b);   // same seed → same value
        TEST_ASSERT_TRUE(a >= -1.0f && a <= 1.0f);
    }
    // different seed → different sequence (at least somewhere)
    bool differs = false;
    for (uint32_t t = 0; t < 1000 && !differs; t += 13)
        differs = Noise::smooth(t, 120, 1) != Noise::smooth(t, 120, 2);
    TEST_ASSERT_TRUE(differs);
}

static void test_noise_vib_bounded() {
    for (uint32_t t = 0; t < 3000; t += 7) {
        int v = Noise::vibPx(t, 1.0f, 1, 45, 99);
        TEST_ASSERT_TRUE(v >= -1 && v <= 1);
    }
    // zero rpm → zero shake
    TEST_ASSERT_EQUAL_INT(0, Noise::vibPx(1234, 0.0f, 1, 45, 99));
}

static void test_sim_rpm_jitter_alive_but_bounded() {
    Simulator sim;
    sim.reset();
    sim.setInputs(0.4f, 0.0f);           // steady cruise
    for (int i = 0; i < 100; i++) sim.update(33);

    // At steady state RPM must still move a little frame-to-frame
    float prev = sim.state.rpm;
    float max_delta = 0.0f;
    for (int i = 0; i < 60; i++) {
        sim.update(33);
        float d = sim.state.rpm - prev;
        if (d < 0) d = -d;
        if (d > max_delta) max_delta = d;
        prev = sim.state.rpm;
    }
    TEST_ASSERT_TRUE(max_delta > 1.0f);              // alive...
    TEST_ASSERT_TRUE(max_delta < SIM_RPM_JITTER * 4); // ...but sane
}

static void test_sim_battery_sags_and_recovers() {
    Simulator sim;
    sim.reset();
    sim.setInputs(0.2f, 0.0f);           // settle at light load
    for (int i = 0; i < 30; i++) sim.update(33);
    float before = sim.state.battery_v;

    sim.setInputs(1.0f, 0.0f);           // hold full throttle for 2 s
    for (int i = 0; i < 60; i++) sim.update(33);
    float dipped = sim.state.battery_v;
    TEST_ASSERT_TRUE(before - dipped > 0.15f);   // visible sag

    sim.setInputs(0.0f, 0.0f);           // release 3 s → slow recovery
    for (int i = 0; i < 90; i++) sim.update(33);
    TEST_ASSERT_TRUE(sim.state.battery_v > dipped + 0.05f);
}

static void test_sim_thermal_inertia_asymmetric() {
    Simulator sim;
    sim.reset();
    float cold = sim.state.engine_temp_f;

    sim.setInputs(1.0f, 0.0f);           // 5 s full load
    for (int i = 0; i < 150; i++) sim.update(33);
    float hot  = sim.state.engine_temp_f;
    float rise = hot - cold;

    sim.setInputs(0.0f, 0.0f);           // 5 s coast
    for (int i = 0; i < 150; i++) sim.update(33);
    float fall = hot - sim.state.engine_temp_f;

    TEST_ASSERT_TRUE(rise > 30.0f);      // heats up for real
    TEST_ASSERT_TRUE(fall > 0.5f);       // does cool...
    TEST_ASSERT_TRUE(rise > fall * 2.0f); // ...but much more slowly
}

static void test_sim_esc_heats_faster_than_motor() {
    Simulator sim;
    sim.reset();
    float motor0 = sim.state.engine_temp_f;
    float esc0   = sim.state.esc_temp_f;

    sim.setInputs(1.0f, 0.0f);           // 3 s full load
    for (int i = 0; i < 90; i++) sim.update(33);

    float motor_rise = sim.state.engine_temp_f - motor0;
    float esc_rise   = sim.state.esc_temp_f - esc0;
    TEST_ASSERT_TRUE(esc_rise > 20.0f);          // ESC spikes for real
    TEST_ASSERT_TRUE(esc_rise > motor_rise);     // smaller thermal mass

    for (int i = 0; i < 3000; i++) sim.update(33);   // long soak: bounded
    TEST_ASSERT_TRUE(sim.state.esc_temp_f <= 250.0f);
    TEST_ASSERT_TRUE(sim.state.esc_temp_f >= 110.0f);
}

static void test_fmt_temp_plain() {
    char b[8];
    Fmt::tempPlain(b, 212.0f);
    TEST_ASSERT_EQUAL_STRING("212F", b);
    Fmt::tempPlain(b, 212.0f, true);     // 212 °F = 100 °C
    TEST_ASSERT_EQUAL_STRING("100C", b);
}

// ── Compass tape (hero detail) ───────────────────────────────────────────────

static void test_compass_north_centered() {
    Compass::Tick t[12];
    int n = Compass::tape(0.0f, 72, 0.8f, t, 12);
    TEST_ASSERT_TRUE(n > 0);
    bool found_n = false;
    for (int i = 0; i < n; i++) {
        TEST_ASSERT_TRUE(t[i].x >= 0 && t[i].x < 72);
        if (t[i].major && strcmp(t[i].label, "N") == 0) {
            found_n = true;
            TEST_ASSERT_EQUAL_INT(36, t[i].x);   // dead center
        }
    }
    TEST_ASSERT_TRUE(found_n);
}

static void test_compass_wraparound() {
    // Heading 350°: N (360°) must appear right of center (+10°)
    Compass::Tick t[12];
    int n = Compass::tape(350.0f, 72, 0.8f, t, 12);
    bool found_n = false;
    for (int i = 0; i < n; i++) {
        if (t[i].major && strcmp(t[i].label, "N") == 0) {
            found_n = true;
            TEST_ASSERT_EQUAL_INT(36 + 8, t[i].x);   // +10° × 0.8 px/deg
        }
    }
    TEST_ASSERT_TRUE(found_n);
}

static void test_compass_tick_spacing() {
    Compass::Tick t[12];
    int n = Compass::tape(123.0f, 72, 0.8f, t, 12);
    TEST_ASSERT_TRUE(n >= 4);            // 90° window / 15° = ~6 ticks
    for (int i = 1; i < n; i++)          // 15° → 12 px apart
        TEST_ASSERT_EQUAL_INT(12, t[i].x - t[i - 1].x);
}

// ── Alert banner in/out timing ───────────────────────────────────────────────

static void test_race_alert_in_out_timing() {
    Simulator sim;  sim.reset();
    Roadbook rb;    rb.reset();
    RaceController rc;
    rc.reset(0);
    uint32_t now = stepRace(rc, sim, rb, 0, BOOT_TOTAL_MS + 100);

    // Enter alert range → age starts counting, gone resets to "never"
    sim.state.odo_mi = 1.10f;
    now += DASH_FRAME_MS; rc.update(now, sim, rb);
    TEST_ASSERT_TRUE(rc.fx().alert);
    uint32_t age0 = rc.fx().alert_age_ms;
    now += DASH_FRAME_MS; rc.update(now, sim, rb);
    TEST_ASSERT_TRUE(rc.fx().alert_age_ms > age0);
    TEST_ASSERT_EQUAL_INT((int)0xFFFFFFFF, (int)rc.fx().alert_gone_ms);
    const char* info = rc.fx().alert_info;

    // Leave alert range → gone counts up, text is kept for the fade-out
    sim.state.odo_mi = 1.50f;
    now += DASH_FRAME_MS; rc.update(now, sim, rb);
    TEST_ASSERT_FALSE(rc.fx().alert);
    TEST_ASSERT_TRUE(rc.fx().alert_gone_ms <= DASH_FRAME_MS + 1);
    TEST_ASSERT_EQUAL_STRING(info, rc.fx().alert_info);
    now += DASH_FRAME_MS; rc.update(now, sim, rb);
    TEST_ASSERT_TRUE(rc.fx().alert_gone_ms >= DASH_FRAME_MS);
}

// ── Formatter ────────────────────────────────────────────────────────────────

static void test_fmt_speed() {
    char b[8];
    Fmt::speed(b, 45.7f);
    TEST_ASSERT_EQUAL_STRING("45", b);
    Fmt::speed(b, 45.0f, true);          // 45 mph = 72.4 km/h
    TEST_ASSERT_EQUAL_STRING("72", b);
}

static void test_fmt_battery() {
    char b[8];
    Fmt::battery(b, 14.23f);
    TEST_ASSERT_EQUAL_STRING("14.2V", b);
}

static void test_fmt_temp() {
    char b[8];
    Fmt::temp(b, 210.4f);
    TEST_ASSERT_EQUAL_STRING("210\xF8""F", b);
    Fmt::temp(b, 212.0f, true);          // 212 °F = 100 °C
    TEST_ASSERT_EQUAL_STRING("100\xF8""C", b);
}

static void test_fmt_heading() {
    char b[10];
    Fmt::heading(b, 0.0f);    TEST_ASSERT_EQUAL_STRING("N",  b);
    Fmt::heading(b, 90.0f);   TEST_ASSERT_EQUAL_STRING("E",  b);
    Fmt::heading(b, 225.0f);  TEST_ASSERT_EQUAL_STRING("SW", b);
    Fmt::heading(b, 348.0f);  TEST_ASSERT_EQUAL_STRING("N",  b);
    Fmt::heading(b, 300.0f);  TEST_ASSERT_EQUAL_STRING("NW", b);
    Fmt::heading(b, -45.0f);  TEST_ASSERT_EQUAL_STRING("NW", b);
    Fmt::headingFull(b, 348.0f);
    TEST_ASSERT_EQUAL_STRING("N 348", b);
}

static void test_fmt_dist() {
    char b[10];
    Fmt::dist(b, 0.45f);
    TEST_ASSERT_EQUAL_STRING("0.45 mi", b);
    Fmt::distNum(b, 0.45f);
    TEST_ASSERT_EQUAL_STRING("0.45", b);
    Fmt::distNum(b, 1.0f, true);         // 1 mi = 1.61 km
    TEST_ASSERT_EQUAL_STRING("1.61", b);
}

static void test_fmt_gear() {
    char b[4];
    Fmt::gear(b, 3, true);
    TEST_ASSERT_EQUAL_STRING("D", b);
    Fmt::gear(b, 3, false);
    TEST_ASSERT_EQUAL_STRING("3", b);
}

static void test_fmt_percent() {
    char b[6];
    Fmt::percent(b, 0.78f);
    TEST_ASSERT_EQUAL_STRING("78%", b);
}

static void test_fmt_clock() {
    char b[10];
    Fmt::clockHMS(b, 0);
    TEST_ASSERT_EQUAL_STRING("12:00:00", b);
    Fmt::clockHMS(b, 3725);              // +1h 2m 5s
    TEST_ASSERT_EQUAL_STRING("13:02:05", b);
    Fmt::clockHMS(b, 13 * 3600);         // wraps past midnight
    TEST_ASSERT_EQUAL_STRING("01:00:00", b);
}

static void test_fmt_race_time() {
    char b[10];
    Fmt::raceTime(b, 0);
    TEST_ASSERT_EQUAL_STRING("00:00.0", b);
    Fmt::raceTime(b, 263700);            // 4 min 23.7 s
    TEST_ASSERT_EQUAL_STRING("04:23.7", b);
    Fmt::raceTime(b, 59950);
    TEST_ASSERT_EQUAL_STRING("00:59.9", b);
}

// ── Runtime UI settings ──────────────────────────────────────────────

static void test_ui_theme_sanitization() {
    TEST_ASSERT_EQUAL_INT((int)UiTheme::CLASSIC_RALLY, (int)sanitizeTheme(0));
    TEST_ASSERT_EQUAL_INT((int)UiTheme::REALISM, (int)sanitizeTheme(1));
    TEST_ASSERT_EQUAL_INT((int)UiTheme::CLASSIC_RALLY, (int)sanitizeTheme(3));
    TEST_ASSERT_EQUAL_STRING("Realism", themeName(UiTheme::REALISM));
}

static void test_ui_numeric_clamps() {
    TEST_ASSERT_EQUAL_UINT8(32, sanitizeBrightness(-10));
    TEST_ASSERT_EQUAL_UINT8(128, sanitizeBrightness(128));
    TEST_ASSERT_EQUAL_UINT8(255, sanitizeBrightness(999));
    TEST_ASSERT_EQUAL_UINT16(500, clampU16(1, 500, 3500));
    TEST_ASSERT_EQUAL_UINT16(1800, clampU16(1800, 500, 3500));
    TEST_ASSERT_EQUAL_UINT16(3500, clampU16(9000, 500, 3500));
}

// ── Dust Runner off-road arcade ─────────────────────────────────────

static void enterGamePlayAt(RockLineGame& game, uint32_t start,
                            uint32_t& now) {
    game.start(start);
    now = start + RockLineGame::COUNTDOWN_MS;
    game.update(now);
    TEST_ASSERT_EQUAL_INT((int)RockGameStatus::PLAYING,
                          (int)game.snapshot(now).status);
}

static void enterGamePlay(RockLineGame& game, uint32_t& now) {
    enterGamePlayAt(game, 0, now);
}

static int cleanTarget(const RockGameSnapshot& snapshot) {
    if (snapshot.obstacle >= snapshot.total_obstacles)
        return snapshot.line_x;
    const int rock = RockLineGame::obstacleLine(snapshot.obstacle);
    const int radius = RockLineGame::obstacleRadius(snapshot.obstacle) +
                       RockLineGame::VEHICLE_HALF_WIDTH + 8;
    if (!RockLineGame::isCollision(snapshot.line_x, rock,
                                    RockLineGame::obstacleRadius(
                                        snapshot.obstacle)))
        return snapshot.line_x;
    int left = rock - radius;
    int right = rock + radius;
    if (left < -80) left = -80;
    if (right > 80) right = 80;
    int left_delta = snapshot.line_x - left;
    if (left_delta < 0) left_delta = -left_delta;
    int right_delta = snapshot.line_x - right;
    if (right_delta < 0) right_delta = -right_delta;
    return left_delta <= right_delta ? left : right;
}

static void driveOneObstacle(RockLineGame& game, uint32_t& now,
                             bool hit) {
    const uint8_t before = game.snapshot(now).obstacle;
    for (int i=0; i<300 && game.snapshot(now).obstacle == before; ++i) {
        RockGameSnapshot snapshot = game.snapshot(now);
        int target = hit ? RockLineGame::obstacleLine(before)
                         : cleanTarget(snapshot);
        int steer = snapshot.line_x < target-2 ? 100
                  : snapshot.line_x > target+2 ? -100 : 0;
        game.control(steer, true, now);
        now += 20;
        game.update(now);
    }
}

static RockGameSnapshot driveCleanCourse(RockLineGame& game, uint32_t& now) {
    for (int i=0; i<4000 && !game.isFinished(); ++i) {
        RockGameSnapshot snapshot = game.snapshot(now);
        if (snapshot.status == RockGameStatus::CRASHED) {
            now += 20;
            game.update(now);
            continue;
        }
        int target = cleanTarget(snapshot);
        int steer = snapshot.line_x < target-2 ? 100
                  : snapshot.line_x > target+2 ? -100 : 0;
        game.control(steer, true, now);
        now += 20;
        game.update(now);
    }
    return game.snapshot(now);
}

static void test_rock_game_countdown_and_obstacle_sequence() {
    RockLineGame game;
    game.start(100);
    TEST_ASSERT_EQUAL_INT((int)RockGameStatus::COUNTDOWN,
                          (int)game.snapshot(100).status);
    game.update(2499);
    TEST_ASSERT_EQUAL_INT((int)RockGameStatus::COUNTDOWN,
                          (int)game.snapshot(2499).status);
    game.update(2500);
    TEST_ASSERT_EQUAL_INT((int)RockGameStatus::PLAYING,
                          (int)game.snapshot(2500).status);
    for (uint8_t i=0; i<RockLineGame::TOTAL_OBSTACLES; ++i) {
        int x = RockLineGame::obstacleLine(i);
        TEST_ASSERT_TRUE(x >= -72 && x <= 72);
        TEST_ASSERT_EQUAL_INT(x, RockLineGame::obstacleLine(i));
        TEST_ASSERT_TRUE(RockLineGame::obstacleRadius(i) >= 14);
        TEST_ASSERT_TRUE(RockLineGame::obstacleRadius(i) <= 17);
    }
}

static void test_rock_game_snapshot_projects_visible_obstacles() {
    RockLineGame game;
    game.enterArcade(100);
    RockGameSnapshot attract = game.snapshot(100);
    TEST_ASSERT_TRUE(attract.visible_obstacles >= 3);
    TEST_ASSERT_EQUAL_INT16(RockLineGame::obstacleLine(0),
                            attract.obstacles[0].line_x);
    TEST_ASSERT_EQUAL_UINT16(260, attract.nearest_distance);
    TEST_ASSERT_TRUE(RockLineGame::isCollision(-48.0f, -48, 15));
    TEST_ASSERT_TRUE(RockLineGame::isCollision(-23.0f, -48, 15));
    TEST_ASSERT_FALSE(RockLineGame::isCollision(-22.0f, -48, 15));
}

static void test_rock_game_dodge_scores_combo() {
    RockLineGame game; uint32_t now;
    enterGamePlay(game, now);
    driveOneObstacle(game, now, false);
    RockGameSnapshot s = game.snapshot(now);
    TEST_ASSERT_EQUAL_UINT8(1, s.obstacle);
    TEST_ASSERT_EQUAL_UINT8(3, s.lives);
    TEST_ASSERT_EQUAL_UINT8(1, s.combo);
    TEST_ASSERT_TRUE(s.score >= 80);
    TEST_ASSERT_TRUE(s.last_event == RockGameEvent::DODGED ||
                     s.last_event == RockGameEvent::NEAR_MISS);
}

static void test_rock_game_hit_costs_life() {
    RockLineGame game; uint32_t now;
    enterGamePlay(game, now);
    driveOneObstacle(game, now, true);
    RockGameSnapshot s = game.snapshot(now);
    TEST_ASSERT_EQUAL_UINT8(1, s.obstacle);
    TEST_ASSERT_EQUAL_UINT8(2, s.lives);
    TEST_ASSERT_EQUAL_UINT8(0, s.combo);
    TEST_ASSERT_EQUAL_INT((int)RockGameEvent::HIT, (int)s.last_event);
    TEST_ASSERT_EQUAL_INT((int)RockGameStatus::CRASHED, (int)s.status);
    now += RockLineGame::CRASH_RECOVERY_MS;
    game.update(now);
    TEST_ASSERT_EQUAL_INT((int)RockGameStatus::PLAYING,
                          (int)game.snapshot(now).status);
}

static void test_rock_game_pause_freezes_and_resumes() {
    RockLineGame game; uint32_t now;
    enterGamePlay(game, now);
    for (int i=0; i<20; ++i) {
        game.control(35, true, now);
        now += 20;
        game.update(now);
    }
    game.pause(now);
    RockGameSnapshot paused = game.snapshot(now);
    TEST_ASSERT_EQUAL_INT((int)RockGameStatus::PAUSED, (int)paused.status);
    game.control(100, true, now+100);
    game.update(now+5000);
    RockGameSnapshot frozen = game.snapshot(now+5000);
    TEST_ASSERT_EQUAL_UINT16(paused.world_distance, frozen.world_distance);
    TEST_ASSERT_EQUAL_UINT32(paused.elapsed_ms, frozen.elapsed_ms);
    TEST_ASSERT_EQUAL_INT8(0, frozen.steering);
    TEST_ASSERT_EQUAL_UINT8(0, frozen.throttle);

    now += 5000;
    game.resume(now);
    game.control(0, true, now);
    now += 50;
    game.update(now);
    RockGameSnapshot resumed = game.snapshot(now);
    TEST_ASSERT_EQUAL_INT((int)RockGameStatus::PLAYING, (int)resumed.status);
    TEST_ASSERT_TRUE(resumed.world_distance > frozen.world_distance);
    TEST_ASSERT_TRUE(resumed.elapsed_ms > frozen.elapsed_ms);
}

static void test_rock_game_attract_input_clamp_and_watchdog() {
    RockLineGame game; game.enterArcade(100);
    TEST_ASSERT_EQUAL_INT((int)RockGameStatus::ATTRACT,
                          (int)game.snapshot(600).status);
    game.start(1000);
    game.update(1000 + RockLineGame::COUNTDOWN_MS);
    uint32_t now = 1000 + RockLineGame::COUNTDOWN_MS;
    game.control(140, true, now);
    RockGameSnapshot live = game.snapshot(now);
    TEST_ASSERT_EQUAL_INT8(100, live.steering);
    TEST_ASSERT_EQUAL_UINT8(1, live.throttle);
    game.update(now + RockLineGame::INPUT_TIMEOUT_MS + 1u);
    RockGameSnapshot safe = game.snapshot(
        now + RockLineGame::INPUT_TIMEOUT_MS + 1u);
    TEST_ASSERT_EQUAL_INT8(0, safe.steering);
    TEST_ASSERT_EQUAL_UINT8(0, safe.throttle);
}

static void test_rock_game_full_course_and_restart() {
    RockLineGame game; uint32_t now;
    enterGamePlay(game, now);
    RockGameSnapshot result = driveCleanCourse(game, now);
    TEST_ASSERT_EQUAL_INT((int)RockGameStatus::FINISHED,
                          (int)result.status);
    TEST_ASSERT_TRUE(result.course_clear);
    TEST_ASSERT_EQUAL_UINT8(RockLineGame::TOTAL_OBSTACLES,
                            result.obstacle);
    TEST_ASSERT_TRUE(result.lives > 0);
    TEST_ASSERT_TRUE(result.score > 1000);

    game.start(now + 100u);
    RockGameSnapshot restarted = game.snapshot(now + 100u);
    TEST_ASSERT_EQUAL_INT((int)RockGameStatus::COUNTDOWN,
                          (int)restarted.status);
    TEST_ASSERT_EQUAL_UINT32(0, restarted.score);
    TEST_ASSERT_EQUAL_UINT8(3, restarted.lives);
    TEST_ASSERT_EQUAL_UINT8(0, restarted.obstacle);
}

static void test_rock_game_millis_wrap_safe() {
    RockLineGame game; uint32_t now;
    enterGamePlayAt(game, UINT32_MAX - 1000u, now);
    TEST_ASSERT_TRUE(now < 2000u);
    for (int i=0; i<20; ++i) {
        game.control(20, true, now);
        now += 20;
        game.update(now);
    }
    RockGameSnapshot before = game.snapshot(now);
    TEST_ASSERT_TRUE(before.elapsed_ms > 0);
    game.pause(now);
    game.update(now + 1000u);
    TEST_ASSERT_EQUAL_UINT32(before.elapsed_ms,
                             game.snapshot(now + 1000u).elapsed_ms);
}

static void test_rtl_profiles_priority_and_timeout() {
    RtlLogic rtl(RtlProfile::ULTRA4_4400, 16, 1000);
    RtlFrame safe = rtl.frame(0);
    TEST_ASSERT_TRUE(safe.command_timed_out);
    TEST_ASSERT_EQUAL_INT((int)RtlFunction::RUNNING, (int)safe.primary);
    TEST_ASSERT_TRUE(safe.pixels[4].r > 0);  // mandatory amber marker

    RtlCommandState command;
    command.brake = true;
    command.left = true;
    command.reverse_work = true;
    rtl.submit(command, 100);
    RtlFrame active = rtl.frame(100);
    TEST_ASSERT_FALSE(active.command_timed_out);
    TEST_ASSERT_EQUAL_INT((int)RtlFunction::BRAKE, (int)active.primary);
    TEST_ASSERT_EQUAL_UINT8(255, active.pixels[0].r);
    TEST_ASSERT_EQUAL_UINT8(255, active.pixels[7].r);
    TEST_ASSERT_EQUAL_UINT8(255, active.pixels[7].g);
    TEST_ASSERT_TRUE(rtl.frame(1101).command_timed_out);
}

static void test_rtl_count_clamp_and_wrap_keepalive() {
    RtlLogic rtl(RtlProfile::RTL_S, 2, 500);
    TEST_ASSERT_EQUAL_UINT16(RTL_LOGIC_MIN_PIXELS, rtl.ledCount());
    rtl.activate(RtlFunction::HAZARD, UINT32_MAX - 200);
    TEST_ASSERT_FALSE(rtl.frame(100).command_timed_out);
    TEST_ASSERT_TRUE(rtl.frame(301).command_timed_out);
    rtl.configure(RtlProfile::RTL_S, 200, 500);
    TEST_ASSERT_EQUAL_UINT16(RTL_LOGIC_MAX_PIXELS, rtl.ledCount());
}

static void test_experience_game_mode_pause_resume_and_stop() {
    ExperienceEngine experience;
    experience.reset(100);
    TEST_ASSERT_EQUAL_INT((int)ExperienceMode::NORMAL,
                          (int)experience.frame(100).mode);

    experience.enterGameAttract(200);
    TEST_ASSERT_EQUAL_INT((int)ExperienceMode::GAME,
                          (int)experience.frame(200).mode);
    TEST_ASSERT_EQUAL_INT((int)RockGameStatus::ATTRACT,
                          (int)experience.frame(200).game.status);

    experience.startGame(300);
    VehicleState vehicle;
    UiFx fx;
    const uint32_t play_at = 300 + RockLineGame::COUNTDOWN_MS;
    experience.update(play_at, vehicle, fx);
    experience.pauseGame(play_at);
    TEST_ASSERT_EQUAL_INT((int)RockGameStatus::PAUSED,
                          (int)experience.frame(play_at).game.status);
    experience.resumeGame(play_at + 500);
    TEST_ASSERT_EQUAL_INT((int)RockGameStatus::PLAYING,
                          (int)experience.frame(play_at + 500).game.status);

    experience.stopGame();
    TEST_ASSERT_EQUAL_INT((int)ExperienceMode::NORMAL,
                          (int)experience.frame(play_at + 600).mode);
}

// ── Runner ───────────────────────────────────────────────────────────────────

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_roadbook_initial_state);
    RUN_TEST(test_roadbook_advances_waypoint);
    RUN_TEST(test_roadbook_skips_multiple_waypoints);
    RUN_TEST(test_roadbook_finish_detection);
    RUN_TEST(test_roadbook_reset);
    RUN_TEST(test_roadbook_stage_data_consistency);

    RUN_TEST(test_sim_reset_defaults);
    RUN_TEST(test_sim_zero_dt_is_noop);
    RUN_TEST(test_sim_accelerates_from_standstill);
    RUN_TEST(test_sim_gears_follow_speed);
    RUN_TEST(test_sim_values_stay_in_bounds);
    RUN_TEST(test_sim_odometer_advances);
    RUN_TEST(test_sim_dt_clamp);

    RUN_TEST(test_sim_gear_no_chatter_at_steady_speed);
    RUN_TEST(test_sim_shift_cooldown_spacing);
    RUN_TEST(test_sim_hits_rev_limiter_before_shifts);

    RUN_TEST(test_driver_brakes_into_danger_waypoint);
    RUN_TEST(test_driver_accelerates_on_straight);
    RUN_TEST(test_driver_holds_corner_speed_at_apex);
    RUN_TEST(test_driver_corner_speed_mapping);

    RUN_TEST(test_race_boot_then_live);
    RUN_TEST(test_race_full_lap_finish_and_restart);
    RUN_TEST(test_race_alert_near_danger_waypoint);
    RUN_TEST(test_race_gear_snap_progress);

    RUN_TEST(test_anim_progress);
    RUN_TEST(test_anim_pulse);
    RUN_TEST(test_anim_triangle_and_snap);
    RUN_TEST(test_anim_lerp565);

    RUN_TEST(test_ease_curves);
    RUN_TEST(test_follow_attack_release_asymmetry);
    RUN_TEST(test_follow_first_update_snaps);
    RUN_TEST(test_slide_anim);

    RUN_TEST(test_noise_deterministic_and_bounded);
    RUN_TEST(test_noise_vib_bounded);
    RUN_TEST(test_sim_rpm_jitter_alive_but_bounded);
    RUN_TEST(test_sim_battery_sags_and_recovers);
    RUN_TEST(test_sim_thermal_inertia_asymmetric);

    RUN_TEST(test_sim_esc_heats_faster_than_motor);
    RUN_TEST(test_fmt_temp_plain);

    RUN_TEST(test_compass_north_centered);
    RUN_TEST(test_compass_wraparound);
    RUN_TEST(test_compass_tick_spacing);

    RUN_TEST(test_race_alert_in_out_timing);

    RUN_TEST(test_fmt_speed);
    RUN_TEST(test_fmt_battery);
    RUN_TEST(test_fmt_temp);
    RUN_TEST(test_fmt_heading);
    RUN_TEST(test_fmt_dist);
    RUN_TEST(test_fmt_gear);
    RUN_TEST(test_fmt_percent);
    RUN_TEST(test_fmt_clock);
    RUN_TEST(test_fmt_race_time);
    RUN_TEST(test_ui_theme_sanitization);
    RUN_TEST(test_ui_numeric_clamps);
    RUN_TEST(test_rock_game_countdown_and_obstacle_sequence);
    RUN_TEST(test_rock_game_snapshot_projects_visible_obstacles);
    RUN_TEST(test_rock_game_dodge_scores_combo);
    RUN_TEST(test_rock_game_hit_costs_life);
    RUN_TEST(test_rock_game_pause_freezes_and_resumes);
    RUN_TEST(test_rock_game_attract_input_clamp_and_watchdog);
    RUN_TEST(test_rock_game_full_course_and_restart);
    RUN_TEST(test_rock_game_millis_wrap_safe);
    RUN_TEST(test_rtl_profiles_priority_and_timeout);
    RUN_TEST(test_rtl_count_clamp_and_wrap_keepalive);
    RUN_TEST(test_experience_game_mode_pause_resume_and_stop);

    return UNITY_END();
}
