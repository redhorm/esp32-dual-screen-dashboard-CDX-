#include <unity.h>

#include "core/esc_signal.h"
#include "core/rtl_logic.h"
#include "core/shift_light.h"

void setUp() {}
void tearDown() {}

namespace {
EscSignalConfig testEscConfig(EscType type = EscType::FORWARD_BRAKE_REVERSE) {
    EscSignalConfig config;
    config.type = type;
    config.smoothing_ms = 0;
    config.max_slew_per_second = 1000.0f;
    config.neutral_dwell_ms = 180;
    config.reverse_confirm_ms = 220;
    config.reverse_arm_timeout_ms = 1600;
    return config;
}
}  // namespace

static void test_esc_normalization_and_neutral_hysteresis() {
    EscSignalProcessor esc(testEscConfig());
    EscSignalFrame frame = esc.update(1750, true, 0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, frame.normalized);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::FORWARD, (int)frame.state);

    frame = esc.update(1540, true, 10);  // below entry, still above exit
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::FORWARD, (int)frame.state);
    frame = esc.update(1525, true, 20);  // crosses the smaller release level
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::NEUTRAL, (int)frame.state);
    frame = esc.update(1445, true, 30);  // inside reverse entry threshold
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::NEUTRAL, (int)frame.state);
}

static void test_race_esc_requires_brake_release_and_reverse_confirmation() {
    EscSignalProcessor esc(testEscConfig());
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::BRAKING,
                          (int)esc.update(1000, true, 0).state);

    // Holding the first reverse command is always braking, never reverse.
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::BRAKING,
                          (int)esc.update(1000, true, 2000).state);

    TEST_ASSERT_EQUAL_INT((int)EscDriveState::BRAKING,
                          (int)esc.update(1500, true, 2010).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::REVERSE_ARMED,
                          (int)esc.update(1500, true, 2190).state);

    TEST_ASSERT_EQUAL_INT((int)EscDriveState::REVERSE_ARMED,
                          (int)esc.update(1000, true, 2200).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::REVERSE_ARMED,
                          (int)esc.update(1000, true, 2419).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::REVERSE,
                          (int)esc.update(1000, true, 2420).state);
}

static void test_invalid_esc_signal_fails_neutral() {
    EscSignalProcessor esc(testEscConfig());
    esc.update(1800, true, 100);
    EscSignalFrame lost = esc.update(0, false, 150);
    TEST_ASSERT_FALSE(lost.valid);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::NEUTRAL, (int)lost.state);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, lost.normalized);
}

static void test_crawler_reverse_is_confirmed_without_fake_braking() {
    EscSignalProcessor esc(testEscConfig(EscType::CRAWLER_FORWARD_REVERSE));
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::REVERSE_ARMED,
                          (int)esc.update(1000, true, 10).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::REVERSE_ARMED,
                          (int)esc.update(1000, true, 229).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::REVERSE,
                          (int)esc.update(1000, true, 230).state);
}

static void test_esc_state_machine_timers_survive_millis_wrap() {
    EscSignalProcessor esc(testEscConfig(EscType::CRAWLER_FORWARD_REVERSE));
    const uint32_t start = UINT32_MAX - 100U;
    esc.update(1000, true, start);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::REVERSE_ARMED,
                          (int)esc.update(1000, true, 50).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::REVERSE,
                          (int)esc.update(1000, true, 120).state);
}

static void test_rtl_maps_braking_and_reverse_to_distinct_zones() {
    RtlCommandState braking = RtlLogic::stateForEsc(EscDriveState::BRAKING);
    TEST_ASSERT_TRUE(braking.brake);
    TEST_ASSERT_FALSE(braking.reverse_work);

    RtlCommandState armed = RtlLogic::stateForEsc(EscDriveState::REVERSE_ARMED);
    TEST_ASSERT_FALSE(armed.brake);
    TEST_ASSERT_FALSE(armed.reverse_work);

    RtlCommandState reverse = RtlLogic::stateForEsc(EscDriveState::REVERSE);
    TEST_ASSERT_FALSE(reverse.brake);
    TEST_ASSERT_TRUE(reverse.reverse_work);

    RtlLogic rtl(RtlProfile::ULTRA4_4400, 16, 1000);
    rtl.submit(braking, 0);
    const RtlFrame brake_frame = rtl.frame(0);
    rtl.submit(reverse, 1);
    const RtlFrame reverse_frame = rtl.frame(1);
    TEST_ASSERT_EQUAL_INT((int)RtlFunction::BRAKE, (int)brake_frame.primary);
    TEST_ASSERT_EQUAL_INT((int)RtlFunction::REVERSE_WORK,
                          (int)reverse_frame.primary);
    TEST_ASSERT_EQUAL_UINT8(255, brake_frame.pixels[0].r);
    TEST_ASSERT_EQUAL_UINT8(255, reverse_frame.pixels[7].g);
}

static void test_shift_lights_ignore_boundary_jitter() {
    ShiftLightConfig config;
    config.minimum_rpm = 0.0f;
    config.maximum_rpm = 1000.0f;
    config.led_count = 10;
    config.hysteresis_rpm = 20.0f;
    config.smoothing_ms = 0;
    config.max_rpm_per_second = 100000.0f;
    config.step_interval_ms = 0;
    ShiftLightController lights(config);

    TEST_ASSERT_EQUAL_UINT8(4, lights.update(495.0f, 0));
    TEST_ASSERT_EQUAL_UINT8(4, lights.update(505.0f, 10));
    TEST_ASSERT_EQUAL_UINT8(4, lights.update(499.0f, 20));
    TEST_ASSERT_EQUAL_UINT8(5, lights.update(521.0f, 30));
    TEST_ASSERT_EQUAL_UINT8(5, lights.update(481.0f, 40));
    TEST_ASSERT_EQUAL_UINT8(4, lights.update(479.0f, 50));
}

static void test_shift_lights_rate_limit_large_steps() {
    ShiftLightConfig config;
    config.minimum_rpm = 0.0f;
    config.maximum_rpm = 1000.0f;
    config.led_count = 10;
    config.hysteresis_rpm = 0.0f;
    config.smoothing_ms = 0;
    config.max_rpm_per_second = 100000.0f;
    config.step_interval_ms = 40;
    ShiftLightController lights(config);

    TEST_ASSERT_EQUAL_UINT8(0, lights.update(0.0f, 100));
    TEST_ASSERT_EQUAL_UINT8(1, lights.update(1000.0f, 140));
    TEST_ASSERT_EQUAL_UINT8(1, lights.update(1000.0f, 160));
    TEST_ASSERT_EQUAL_UINT8(2, lights.update(1000.0f, 180));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_esc_normalization_and_neutral_hysteresis);
    RUN_TEST(test_race_esc_requires_brake_release_and_reverse_confirmation);
    RUN_TEST(test_invalid_esc_signal_fails_neutral);
    RUN_TEST(test_crawler_reverse_is_confirmed_without_fake_braking);
    RUN_TEST(test_esc_state_machine_timers_survive_millis_wrap);
    RUN_TEST(test_rtl_maps_braking_and_reverse_to_distinct_zones);
    RUN_TEST(test_shift_lights_ignore_boundary_jitter);
    RUN_TEST(test_shift_lights_rate_limit_large_steps);
    return UNITY_END();
}
