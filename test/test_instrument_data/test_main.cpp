#include <unity.h>
#include <math.h>
#include <initializer_list>
#include "app/instrument_data.h"

void setUp() {}
void tearDown() {}

void test_units_match_between_driver_and_navigation() {
    VehicleState state;
    state.speed_mph = 45.2f; state.rpm = 5534; state.odo_mi = .42f;
    state.gear = 3;
    UiFx fx; fx.phase = Phase::LIVE;
    UiSettings settings;
    RunStats stats; stats.stage_ms = 37250;
    for (bool metric : {false, true}) {
        settings.use_kmh = metric;
        const auto pilot = InstrumentData::pilot(state, fx, stats, settings);
        const auto nav = InstrumentData::navigator(state, fx, settings);
        const float nav_speed = nav.speed_mps * (metric ? 3.6f : 2.2369363f);
        TEST_ASSERT_FLOAT_WITHIN(.501f, nav_speed, pilot.speed);
        TEST_ASSERT_FLOAT_WITHIN(.01f, state.odo_mi * 1609.344f, nav.odo_m);
        TEST_ASSERT_EQUAL_INT(5540, pilot.rpm);
        TEST_ASSERT_EQUAL_UINT32(37250, pilot.elapsed_ms);
    }
}

void test_navigation_caution_is_not_an_engine_sensor_alarm() {
    VehicleState state;
    UiFx fx; fx.phase = Phase::LIVE; fx.alert = true;
    UiSettings settings;
    RunStats stats;
    TEST_ASSERT_FALSE(InstrumentData::pilot(state, fx, stats, settings).warning);
    TEST_ASSERT_TRUE(InstrumentData::navigator(state, fx, settings).warning);
    TEST_ASSERT_EQUAL_INT(0, InstrumentData::pilot(state, fx, stats, settings).gear);
    state.battery_v = 11.2f;
    TEST_ASSERT_TRUE(InstrumentData::pilot(state, fx, stats, settings).warning);
}

void test_calibration_and_invalid_values_do_not_leak_into_normal_display() {
    VehicleState state;
    state.speed_mph = NAN; state.rpm = NAN; state.battery_v = NAN; state.odo_mi = NAN;
    UiSettings settings;
    settings.driver_theme = settings.navigator_theme = UiTheme::RECOIL_ALIGN;
    UiFx fx;
    RunStats stats;
    const auto p = InstrumentData::pilot(state, fx, stats, settings);
    const auto n = InstrumentData::navigator(state, fx, settings);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RecoilRender::Screen::ALIGN), static_cast<int>(p.screen));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(p.screen), static_cast<int>(n.screen));
    TEST_ASSERT_EQUAL_INT(0, p.speed);
    TEST_ASSERT_EQUAL_INT(0, p.rpm);
    TEST_ASSERT_FLOAT_WITHIN(.001f, 0, n.odo_m);
    TEST_ASSERT_FLOAT_WITHIN(.001f, 0, n.speed_mps);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_units_match_between_driver_and_navigation);
    RUN_TEST(test_navigation_caution_is_not_an_engine_sensor_alarm);
    RUN_TEST(test_calibration_and_invalid_values_do_not_leak_into_normal_display);
    return UNITY_END();
}
