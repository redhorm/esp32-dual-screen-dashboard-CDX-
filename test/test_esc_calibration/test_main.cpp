#include <unity.h>
#include <initializer_list>
#include "core/esc_display_calibration.h"
void setUp() {} void tearDown() {}
void calibration_rescales_only_forward() {
    EscSignalConfig c; c.inverted=true; c.neutral_us=1524;
    TEST_ASSERT_FLOAT_WITHIN(.0001f,1.0f,EscDisplayCalibration::apply(.77f,c,1120,1120));
    TEST_ASSERT_FLOAT_WITHIN(.0001f,.5f,EscDisplayCalibration::apply(.38f,c,1120,1322));
    TEST_ASSERT_EQUAL_FLOAT(1,EscDisplayCalibration::apply(1,c,1120,1000));
    TEST_ASSERT_EQUAL_FLOAT(-.5f,EscDisplayCalibration::apply(-.5f,c,1120,1800));
    TEST_ASSERT_EQUAL_FLOAT(0,EscDisplayCalibration::apply(0,c,1120,1524));
}
void invalid_calibration_cannot_change_scale() {
    EscSignalConfig c;c.inverted=true;c.neutral_us=1524;
    for (uint16_t bad : {uint16_t(0),uint16_t(1500),uint16_t(1900),uint16_t(700)}) {
        TEST_ASSERT_FALSE(EscDisplayCalibration::valid(c,bad));
        TEST_ASSERT_EQUAL_FLOAT(.8f,EscDisplayCalibration::apply(.8f,c,bad,1100));
    }
    TEST_ASSERT_EQUAL_FLOAT(0,EscDisplayCalibration::apply(NAN,c,1120,1120));
    TEST_ASSERT_EQUAL_FLOAT(0,EscDisplayCalibration::apply(.8f,c,1120,0));
}
void both_polarities_and_new_neutral_work() {
    EscSignalConfig c;c.neutral_us=1524;
    TEST_ASSERT_TRUE(EscDisplayCalibration::valid(c,1900));
    TEST_ASSERT_FLOAT_WITHIN(.0001f,1,EscDisplayCalibration::apply(.79f,c,1900,1900));
    c.neutral_us=1510;
    TEST_ASSERT_FLOAT_WITHIN(.0001f,1,EscDisplayCalibration::apply(.8f,c,1900,1900));
    c.inverted=true;
    TEST_ASSERT_FALSE(EscDisplayCalibration::valid(c,1900));
}
void wider_than_nominal_travel_still_reaches_full_scale() {
    EscSignalConfig c;c.neutral_us=1524;c.inverted=true;
    TEST_ASSERT_EQUAL_FLOAT(1,EscDisplayCalibration::apply(1,c,900,900));
    TEST_ASSERT_FLOAT_WITHIN(.0001f,.5f,EscDisplayCalibration::apply(.59f,c,900,1212));
    c.inverted=false;
    TEST_ASSERT_EQUAL_FLOAT(1,EscDisplayCalibration::apply(1,c,2100,2100));
    TEST_ASSERT_FLOAT_WITHIN(.0001f,.5f,EscDisplayCalibration::apply(.6f,c,2100,1812));
}
int main() { UNITY_BEGIN();RUN_TEST(calibration_rescales_only_forward);RUN_TEST(invalid_calibration_cannot_change_scale);RUN_TEST(both_polarities_and_new_neutral_work);RUN_TEST(wider_than_nominal_travel_still_reaches_full_scale);return UNITY_END();}
