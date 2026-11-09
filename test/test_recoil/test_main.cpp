#include <unity.h>
#include "app/recoil_renderer.h"
#include "core/ui_settings.h"
#include "core/shift_light.h"
#include "../../tools/recoil/host_canvas.h"

void setUp() {}
void tearDown() {}

static void assertDots(const HostCanvas& c,int count) {
    // Independent fixture: the 16 CAD aperture centres delivered with the mask.
    const int x[16]={10,19,28,37,46,55,64,73,86,95,104,113,122,131,140,149};
    const int y[16]={23,19,15,13,10,9,7,7,7,7,9,10,13,15,19,23};
    for(int i=0;i<16;++i) {
        uint16_t color=i<8?RecoilRender::rgb(56,220,86):i<12?RecoilRender::AMBER:RecoilRender::RED;
        if(i>=count)color=0;
        // Every pixel centre within the circular 0.85 mm opening is illuminated.
        for(int dy=-4;dy<=4;++dy)for(int dx=-4;dx<=4;++dx)
            if((dx*21.7/160)*(dx*21.7/160)+(dy*10.8/80)*(dy*10.8/80)<=.425*.425)
                TEST_ASSERT_EQUAL_HEX16(color,c.at(x[i]+dx,y[i]+dy));
    }
}
static void test_every_aperture_matches_even_with_alert_and_three_digit_speed() {
    HostCanvas c(160,80);RecoilRender::PilotData d;
    d.speed=123;d.gear=-1;d.battery=12.6;d.warning=true;
    for(int count=0;count<=16;++count) {
        d.lit=count;TEST_ASSERT_TRUE(RecoilRender::pilot(c,d));assertDots(c,count);
    }
    TEST_ASSERT_EQUAL_INT(0,c.clipped);
}
static void test_calibration_boot_and_finish_keep_aperture_centres() {
    HostCanvas c(160,80);RecoilRender::PilotData d;
    for(auto screen:{RecoilRender::Screen::ALIGN,RecoilRender::Screen::BOOT,RecoilRender::Screen::FINISH}) {
        d.screen=screen;d.lit=16;d.stageSeconds=9999;
        TEST_ASSERT_TRUE(RecoilRender::pilot(c,d));assertDots(c,16);
    }
    TEST_ASSERT_EQUAL_INT(0,c.clipped);
}
static void test_masked_area_stays_black_except_light_patches() {
    HostCanvas c(160,80);RecoilRender::PilotData d;d.speed=999;d.lit=16;d.battery=16;
    RecoilRender::pilot(c,d);
    for(int y=0;y<80;++y)for(int x=0;x<160;++x) {
        bool opening=RecoilGeometry::windowTop[x]!=255 && y>=RecoilGeometry::windowTop[x] && y<=75;
        bool light=false;
        for(const auto& p:RecoilGeometry::dots)if((x-p.x)*(x-p.x)+(y-p.y)*(y-p.y)<=16)light=true;
        if(!opening&&!light)TEST_ASSERT_EQUAL_HEX16(0,c.at(x,y));
    }
}
static void test_native_navigation_states_and_profile_rejection() {
    HostCanvas c(240,135);RecoilRender::NavData d;d.warning=true;
    for(auto screen:{RecoilRender::Screen::ALIGN,RecoilRender::Screen::BOOT,RecoilRender::Screen::FINISH,RecoilRender::Screen::LIVE}) {
        for(float progress:{-1.0f,0.0f,0.5f,1.0f,2.0f}) {
            d.screen=screen;d.progress=progress;TEST_ASSERT_TRUE(RecoilRender::navigator(c,d));
        }
    }
    TEST_ASSERT_EQUAL_INT(0,c.clipped);
    HostCanvas wrong(240,240);TEST_ASSERT_FALSE(RecoilRender::navigator(wrong,d));
    HostCanvas wrongA(80,160);RecoilRender::PilotData a;TEST_ASSERT_FALSE(RecoilRender::pilot(wrongA,a));
}
static void test_theme_ids_preserve_old_settings_and_reserve_pro_race() {
    for(uint8_t i=0;i<=2;++i)TEST_ASSERT_EQUAL_INT(4,static_cast<int>(sanitizeTheme(i)));
    TEST_ASSERT_EQUAL_INT(4,static_cast<int>(sanitizeTheme(3)));
    TEST_ASSERT_EQUAL_INT(4,static_cast<int>(sanitizeTheme(4)));
    TEST_ASSERT_EQUAL_INT(5,static_cast<int>(sanitizeTheme(5)));
    TEST_ASSERT_EQUAL_INT(4,static_cast<int>(sanitizeTheme(255)));
}
static void test_sixteen_light_quantizer_reaches_both_ends_without_skips() {
    ShiftLightConfig config;config.led_count=16;ShiftLightController ctrl(config);
    TEST_ASSERT_EQUAL_INT(0,ctrl.update(0,0));int previous=0;
    for(uint32_t t=20;t<=5000;t+=20) {
        int lit=ctrl.update(8000,t);TEST_ASSERT_TRUE(lit>=previous&&lit<=previous+1&&lit<=16);previous=lit;
    }
    TEST_ASSERT_EQUAL_INT(16,previous);
    for(uint32_t t=5020;t<=10000;t+=20) {
        int lit=ctrl.update(0,t);TEST_ASSERT_TRUE(lit<=previous&&lit>=previous-1&&lit>=0);previous=lit;
    }
    TEST_ASSERT_EQUAL_INT(0,previous);
}
int main(int,char**) {
    UNITY_BEGIN();
    RUN_TEST(test_every_aperture_matches_even_with_alert_and_three_digit_speed);
    RUN_TEST(test_calibration_boot_and_finish_keep_aperture_centres);
    RUN_TEST(test_masked_area_stays_black_except_light_patches);
    RUN_TEST(test_native_navigation_states_and_profile_rejection);
    RUN_TEST(test_theme_ids_preserve_old_settings_and_reserve_pro_race);
    RUN_TEST(test_sixteen_light_quantizer_reaches_both_ends_without_skips);
    return UNITY_END();
}
