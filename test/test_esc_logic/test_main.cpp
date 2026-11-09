#include <unity.h>

#include "core/esc_signal.h"
#include "core/pwm_median.h"
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

static void test_asymmetric_calibration_keeps_equal_microsecond_deadband() {
    EscSignalConfig config = testEscConfig(EscType::CRAWLER_FORWARD_REVERSE);
    config.reverse_us = 1000;
    config.neutral_us = 1600;
    config.forward_us = 2000;
    EscSignalProcessor::configureDeadbandUs(config, 60);

    EscSignalProcessor forward(config);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::NEUTRAL,
                          (int)forward.update(1659, true, 0).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::FORWARD,
                          (int)forward.update(1660, true, 1).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::FORWARD,
                          (int)forward.update(1634, true, 2).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::NEUTRAL,
                          (int)forward.update(1632, true, 3).state);

    EscSignalProcessor reverse(config);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::NEUTRAL,
                          (int)reverse.update(1541, true, 0).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::REVERSE,
                          (int)reverse.update(1540, true, 1).state);
}

static void test_deadband_extremes_match_configured_microseconds() {
    EscSignalConfig config = testEscConfig(EscType::CRAWLER_FORWARD_REVERSE);
    config.reverse_us = 1000;
    config.neutral_us = 1700;
    config.forward_us = 2000;
    EscSignalProcessor::configureDeadbandUs(config, 180);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 180.0f,
        config.forward_enter_threshold * 300.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 180.0f,
        config.reverse_enter_threshold * 700.0f);

    EscSignalProcessor forward(config);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::NEUTRAL,
                          (int)forward.update(1879, true, 0).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::FORWARD,
                          (int)forward.update(1881, true, 1).state);
    EscSignalProcessor reverse(config);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::NEUTRAL,
                          (int)reverse.update(1521, true, 0).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::REVERSE,
                          (int)reverse.update(1519, true, 1).state);

    config.neutral_us = 1300;
    EscSignalProcessor::configureDeadbandUs(config, 10);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f,
        config.forward_enter_threshold * 700.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f,
        config.reverse_enter_threshold * 300.0f);
}

static void test_crawler_changes_direction_immediately_without_fake_braking() {
    EscSignalConfig config; // Real default smoothing/slew would formerly lag.
    config.type = EscType::CRAWLER_FORWARD_REVERSE;
    EscSignalProcessor esc(config);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::FORWARD,
                          (int)esc.update(2000, true, 0).state);
    auto reverse = esc.update(1000, true, 10);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::REVERSE, (int)reverse.state);
    TEST_ASSERT_FLOAT_WITHIN(.001f, -1, reverse.normalized);
    auto neutral = esc.update(1500, true, 20);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::NEUTRAL, (int)neutral.state);
    TEST_ASSERT_FLOAT_WITHIN(.001f, 0, neutral.normalized);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::REVERSE,
                          (int)esc.update(1000, true, 30).state);
    auto forward = esc.update(2000, true, 40);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::FORWARD, (int)forward.state);
    TEST_ASSERT_FLOAT_WITHIN(.001f, 1, forward.normalized);
}

static void test_esc_state_machine_timers_survive_millis_wrap() {
    EscSignalProcessor esc(testEscConfig());
    esc.update(1000, true, UINT32_MAX - 100U); // Brake first, as a road ESC.
    esc.update(1500, true, UINT32_MAX - 90U);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::REVERSE_ARMED,
                          (int)esc.update(1500, true, 89).state);
    esc.update(1000, true, 100);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::REVERSE_ARMED,
                          (int)esc.update(1000, true, 319).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::REVERSE,
                          (int)esc.update(1000, true, 320).state);
}

static void test_inverted_crawler_keeps_asymmetric_neutral_band() {
    auto config = testEscConfig(EscType::CRAWLER_FORWARD_REVERSE);
    config.inverted = true;
    config.neutral_us = 1535;
    EscSignalProcessor::configureDeadbandUs(config, 45);
    TEST_ASSERT_FLOAT_WITHIN(.001f, 45.0f,
                            config.forward_enter_threshold * 535.0f);
    TEST_ASSERT_FLOAT_WITHIN(.001f, 45.0f,
                            config.reverse_enter_threshold * 465.0f);
    EscSignalProcessor esc(config);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::NEUTRAL,
                          (int)esc.update(1491, true, 0).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::FORWARD,
                          (int)esc.update(1489, true, 10).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::NEUTRAL,
                          (int)esc.update(1511, true, 20).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::NEUTRAL,
                          (int)esc.update(1579, true, 30).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::REVERSE,
                          (int)esc.update(1581, true, 40).state);
    TEST_ASSERT_FLOAT_WITHIN(.001f, 1, esc.update(1000, true, 50).normalized);
    TEST_ASSERT_FLOAT_WITHIN(.001f, -1, esc.update(2000, true, 60).normalized);
    const auto lost = esc.update(2000, false, 70);
    TEST_ASSERT_FALSE(lost.valid);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::NEUTRAL, (int)lost.state);
    TEST_ASSERT_FLOAT_WITHIN(.001f, 0, lost.normalized);
    const auto reconnect = esc.update(1535, true, 1000);
    TEST_ASSERT_TRUE(reconnect.valid);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::NEUTRAL, (int)reconnect.state);
    TEST_ASSERT_FLOAT_WITHIN(.001f, 0, reconnect.normalized);
}

static void test_median_rejects_spike_but_step_has_one_pulse_delay() {
    auto config = testEscConfig(EscType::CRAWLER_FORWARD_REVERSE);
    config.inverted = true;
    EscSignalProcessor esc(config);
    uint16_t previous = 1500, older = 1500;
    auto pulse = [&](uint16_t value, uint32_t now) {
        const auto filtered = pwmMedian3(value, previous, older);
        older = previous; previous = value;
        return esc.update(filtered, true, now);
    };
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::NEUTRAL, (int)pulse(1500, 0).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::NEUTRAL, (int)pulse(2000, 20).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::NEUTRAL, (int)pulse(1500, 40).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::NEUTRAL, (int)pulse(1500, 60).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::NEUTRAL, (int)pulse(1000, 80).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::FORWARD, (int)pulse(1000, 100).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::FORWARD, (int)pulse(2000, 120).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::REVERSE, (int)pulse(2000, 140).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::REVERSE, (int)pulse(1500, 160).state);
    TEST_ASSERT_EQUAL_INT((int)EscDriveState::NEUTRAL, (int)pulse(1500, 180).state);
    for (int a=1000;a<=2000;a+=500) for (int b=1000;b<=2000;b+=500)
        for (int c=1000;c<=2000;c+=500) {
            uint16_t v[3] = {(uint16_t)a,(uint16_t)b,(uint16_t)c};
            for(int i=0;i<3;++i)for(int j=i+1;j<3;++j)
                if(v[j]<v[i]) { auto t=v[j];v[j]=v[i];v[i]=t; }
            TEST_ASSERT_EQUAL_UINT16(v[1], pwmMedian3(a,b,c));
        }
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
    RUN_TEST(test_asymmetric_calibration_keeps_equal_microsecond_deadband);
    RUN_TEST(test_deadband_extremes_match_configured_microseconds);
    RUN_TEST(test_crawler_changes_direction_immediately_without_fake_braking);
    RUN_TEST(test_inverted_crawler_keeps_asymmetric_neutral_band);
    RUN_TEST(test_median_rejects_spike_but_step_has_one_pulse_delay);
    RUN_TEST(test_esc_state_machine_timers_survive_millis_wrap);
    RUN_TEST(test_rtl_maps_braking_and_reverse_to_distinct_zones);
    RUN_TEST(test_shift_lights_ignore_boundary_jitter);
    RUN_TEST(test_shift_lights_rate_limit_large_steps);
    return UNITY_END();
}
