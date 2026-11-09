#include <unity.h>
#include <math.h>
#include "core/esc_dashboard.h"
#include "core/experience.h"
#include "app/instrument_data.h"

void setUp() {}
void tearDown() {}

namespace {
void run(EscDashboardController& dash, float command, EscDriveState state,
         uint32_t start = 0, uint32_t duration = 2000) {
    for (uint32_t dt = 0; dt <= duration; dt += 20)
        dash.update(start + dt, true, true, command, state);
}
void assertRest(const EscDashboardController& dash) {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, dash.state().speed_mph);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, dash.state().rpm);
    TEST_ASSERT_EQUAL_INT(0, dash.state().gear);
    TEST_ASSERT_FALSE(dash.fx().redline);
}
}

static void test_neutral_is_stationary_without_demo() {
    EscDashboardController dash;
    run(dash, 0.0f, EscDriveState::NEUTRAL);
    assertRest(dash);
    TEST_ASSERT_TRUE(dash.enabled());
    TEST_ASSERT_TRUE(dash.signalValid());
    TEST_ASSERT_EQUAL_FLOAT(0, dash.state().odo_mi);
    TEST_ASSERT_EQUAL_INT((int)Phase::LIVE, (int)dash.fx().phase);
}
static void test_forward_command_controls_speed_and_rpm() {
    EscDashboardController half, full;
    run(half, .5f, EscDriveState::FORWARD, 0, 12000);
    run(full, 1.0f, EscDriveState::FORWARD, 0, 12000);
    TEST_ASSERT_FLOAT_WITHIN(.01f, 50, half.state().speed_mph);
    TEST_ASSERT_TRUE(half.state().rpm < 6900);
    TEST_ASSERT_TRUE(half.state().gear < 5);
    TEST_ASSERT_FLOAT_WITHIN(.01f, 100, full.state().speed_mph);
    TEST_ASSERT_TRUE(full.state().rpm >= 7000 && full.state().rpm <= 7200);
    TEST_ASSERT_TRUE(full.fx().redline);
    TEST_ASSERT_EQUAL_INT(5, full.state().gear);
    TEST_ASSERT_TRUE(full.state().odo_mi > half.state().odo_mi);
}
static void test_all_forward_gears_show_rpm_drop_and_limiter() {
    EscDashboardController dash;
    int seen = 0, upshifts = 0, previous_gear = 0;
    float previous_rpm = 0;
    uint32_t previous_shift = 0;
    bool full_lamps = false, limiter_cut = false;
    for (uint32_t now = 0; now <= 12000; now += 20) {
        dash.update(now, true, true, 1, EscDriveState::FORWARD);
        const auto& state = dash.state();
        TEST_ASSERT_TRUE(state.gear >= 1 && state.gear <= 5);
        seen |= 1 << state.gear;
        if (state.gear > previous_gear && previous_gear > 0) {
            TEST_ASSERT_EQUAL_INT(previous_gear + 1, state.gear);
            TEST_ASSERT_TRUE(previous_rpm >= 7190);
            TEST_ASSERT_FLOAT_WITHIN(.01f, 4500, state.rpm);
            TEST_ASSERT_TRUE(now - previous_shift >= 800);
            previous_shift = now;
            ++upshifts;
        }
        if (state.gear == 5 && state.speed_mph >= 99.5f) {
            full_lamps |= Iq3Render::pairCount((int)state.rpm) == 8;
            limiter_cut |= state.rpm <= 7001;
        }
        previous_gear = state.gear;
        previous_rpm = state.rpm;
    }
    TEST_ASSERT_EQUAL_INT(0x3e, seen);
    TEST_ASSERT_EQUAL_INT(4, upshifts);
    TEST_ASSERT_TRUE(full_lamps);
    TEST_ASSERT_TRUE(limiter_cut);
    TEST_ASSERT_FLOAT_WITHIN(.01f, 7200, dash.stats().max_rpm);
}
static void test_partial_throttle_does_not_run_away_or_hunt_gears() {
    EscDashboardController dash;
    run(dash, .2f, EscDriveState::FORWARD, 0, 12000);
    TEST_ASSERT_FLOAT_WITHIN(.01f, 20, dash.state().speed_mph);
    TEST_ASSERT_TRUE(dash.state().gear <= 2);
    const int gear = dash.state().gear;
    for (uint32_t now = 12020; now <= 30000; now += 20) {
        dash.update(now, true, true, .2f, EscDriveState::FORWARD);
        TEST_ASSERT_EQUAL_INT(gear, dash.state().gear);
        TEST_ASSERT_TRUE(dash.state().rpm < 6900);
        TEST_ASSERT_TRUE(dash.state().speed_mph <= 20.01f);
    }
}
static void test_release_goes_to_rest_on_next_frame() {
    EscDashboardController dash;
    run(dash, 1, EscDriveState::FORWARD);
    dash.update(2020, true, true, 0, EscDriveState::NEUTRAL);
    assertRest(dash);
}
static void test_signal_loss_is_immediate_and_recovery_ramps() {
    EscDashboardController dash;
    run(dash, 1, EscDriveState::FORWARD);
    dash.update(2020, true, false, 1, EscDriveState::FORWARD);
    assertRest(dash);
    TEST_ASSERT_TRUE(dash.enabled());
    TEST_ASSERT_FALSE(dash.signalValid());
    dash.update(2040, true, true, 1, EscDriveState::FORWARD);
    TEST_ASSERT_FLOAT_WITHIN(.001f, 0.44f, dash.state().speed_mph);
    TEST_ASSERT_FLOAT_WITHIN(.001f, 360.0f, dash.state().rpm);
    TEST_ASSERT_TRUE(dash.signalValid());
}
static void test_disabled_source_clears_history() {
    EscDashboardController dash;
    run(dash, 1, EscDriveState::FORWARD);
    dash.update(2020, false, true, 1, EscDriveState::FORWARD);
    assertRest(dash);
    TEST_ASSERT_FALSE(dash.enabled());
    TEST_ASSERT_FALSE(dash.signalValid());
    TEST_ASSERT_EQUAL_FLOAT(0, dash.state().odo_mi);
    TEST_ASSERT_EQUAL_FLOAT(0, dash.stats().max_rpm);
    TEST_ASSERT_EQUAL_UINT32(0, dash.stats().stage_ms);
}
static void test_reverse_requires_explicit_permission() {
    EscDashboardController dash;
    run(dash, -1, EscDriveState::REVERSE_ARMED);
    assertRest(dash);
    run(dash, -1, EscDriveState::REVERSE, 2020);
    TEST_ASSERT_EQUAL_INT(-1, dash.state().gear);
    TEST_ASSERT_FLOAT_WITHIN(.01f, 20, dash.state().speed_mph);
    TEST_ASSERT_FLOAT_WITHIN(.01f, 7200, dash.state().rpm);
}
static void test_braking_decelerates_without_entering_reverse() {
    EscDashboardController dash;
    run(dash, 1, EscDriveState::FORWARD);
    dash.update(2020, true, true, -1, EscDriveState::BRAKING);
    TEST_ASSERT_TRUE(dash.state().speed_mph > 0);
    TEST_ASSERT_TRUE(dash.state().speed_mph < 100);
    TEST_ASSERT_TRUE(dash.state().rpm < 7200);
    TEST_ASSERT_TRUE(dash.state().gear > 0);
    run(dash, -1, EscDriveState::BRAKING, 2040);
    assertRest(dash);
}
static void test_reversal_does_not_reuse_forward_velocity() {
    EscDashboardController dash;
    run(dash, 1, EscDriveState::FORWARD);
    dash.update(2020, true, true, -1, EscDriveState::REVERSE);
    TEST_ASSERT_EQUAL_INT(-1, dash.state().gear);
    TEST_ASSERT_FLOAT_WITHIN(.001f, .7f, dash.state().speed_mph);
    TEST_ASSERT_FLOAT_WITHIN(.001f, 360, dash.state().rpm);
}
static void test_invalid_or_contradictory_command_cannot_drive() {
    EscDashboardController dash;
    run(dash, 1, EscDriveState::FORWARD);
    dash.update(2020, true, true, NAN, EscDriveState::FORWARD);
    assertRest(dash);
    TEST_ASSERT_FALSE(dash.signalValid());
    dash.update(2040, true, true, -1, EscDriveState::FORWARD);
    assertRest(dash);
    dash.update(2060, true, true, 1, EscDriveState::REVERSE);
    assertRest(dash);
}
static void test_time_gap_and_wrap_are_bounded() {
    EscDashboardController dash;
    dash.update(0xfffffff0U, true, true, 1, EscDriveState::FORWARD);
    TEST_ASSERT_EQUAL_FLOAT(0, dash.state().speed_mph);
    dash.update(4U, true, true, 1, EscDriveState::FORWARD);
    TEST_ASSERT_FLOAT_WITHIN(.001f, 0.44f, dash.state().speed_mph);
    dash.update(200004U, true, true, 1, EscDriveState::FORWARD);
    TEST_ASSERT_FLOAT_WITHIN(.001f, 2.64f, dash.state().speed_mph);
    TEST_ASSERT_EQUAL_UINT32(120, dash.stats().stage_ms);
}
static void test_live_snapshot_and_optical_lights_follow_source() {
    EscDashboardController dash;
    run(dash, 1, EscDriveState::FORWARD, 0, 12040);
    VehicleState state = dash.state();
    state.esc_controlled = true; state.esc_signal_valid = true;
    auto pixels = InstrumentData::pilot(state, dash.fx(), dash.stats(), UiSettings{});
    TEST_ASSERT_EQUAL_INT(8, Iq3Render::pairCount(pixels.rpm));
    TEST_ASSERT_TRUE(pixels.esc_controlled);
    dash.update(12060, true, false, 1, EscDriveState::FORWARD);
    state = dash.state(); state.esc_controlled = true;
    pixels = InstrumentData::pilot(state, dash.fx(), dash.stats(), UiSettings{});
    TEST_ASSERT_EQUAL_INT(0, Iq3Render::pairCount(pixels.rpm));
    TEST_ASSERT_FALSE(pixels.esc_signal_valid);
    ExperienceEngine experience;
    state.gear = -1;
    experience.update(12060, state, dash.fx());
    auto frame = experience.frame(12060);
    TEST_ASSERT_EQUAL_INT(-1, frame.gear);
    TEST_ASSERT_TRUE(frame.esc_controlled);
    TEST_ASSERT_FALSE(frame.esc_signal_valid);
}
static void test_command_is_clamped_to_display_limits() {
    EscDashboardController dash;
    run(dash, 30, EscDriveState::FORWARD, 0, 12040);
    TEST_ASSERT_FLOAT_WITHIN(.01f, 100, dash.state().speed_mph);
    TEST_ASSERT_FLOAT_WITHIN(.01f, 7200, dash.state().rpm);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_neutral_is_stationary_without_demo);
    RUN_TEST(test_forward_command_controls_speed_and_rpm);
    RUN_TEST(test_all_forward_gears_show_rpm_drop_and_limiter);
    RUN_TEST(test_partial_throttle_does_not_run_away_or_hunt_gears);
    RUN_TEST(test_release_goes_to_rest_on_next_frame);
    RUN_TEST(test_signal_loss_is_immediate_and_recovery_ramps);
    RUN_TEST(test_disabled_source_clears_history);
    RUN_TEST(test_reverse_requires_explicit_permission);
    RUN_TEST(test_braking_decelerates_without_entering_reverse);
    RUN_TEST(test_reversal_does_not_reuse_forward_velocity);
    RUN_TEST(test_invalid_or_contradictory_command_cannot_drive);
    RUN_TEST(test_time_gap_and_wrap_are_bounded);
    RUN_TEST(test_command_is_clamped_to_display_limits);
    RUN_TEST(test_live_snapshot_and_optical_lights_follow_source);
    return UNITY_END();
}
