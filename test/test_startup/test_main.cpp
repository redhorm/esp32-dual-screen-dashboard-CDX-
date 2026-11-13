#include <unity.h>
#include <type_traits>
#include "core/startup_sequence.h"
#include "app/startup_renderer.h"
#include "../../tools/recoil/host_canvas.h"

void setUp() {}
void tearDown() {}

static void test_duration_cancel_restart_and_wrap() {
    static_assert(std::is_trivially_copyable<StartupFrame>::value, "Snapshot crosses render calls by value");
    static_assert(sizeof(StartupFrame) <= 12, "Startup frame remains tiny");
    static_assert(sizeof(StartupSequence) <= 8, "No state buffer or timers");
    StartupSequence sequence;
    TEST_ASSERT_FALSE(sequence.active());
    sequence.start(UINT32_MAX - 999U);
    TEST_ASSERT_TRUE(sequence.frame(0).active);
    TEST_ASSERT_EQUAL_UINT32(1000, sequence.frame(0).elapsed_ms);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StartupPhase::IDENTITY), static_cast<int>(sequence.frame(0).phase));
    TEST_ASSERT_TRUE(sequence.frame(4599).active);
    TEST_ASSERT_FALSE(sequence.frame(4600).active);
    sequence.update(4600);
    TEST_ASSERT_FALSE(sequence.active());
    sequence.start(10000);
    sequence.stop();
    TEST_ASSERT_FALSE(sequence.frame(10001).active);
    sequence.start(11000);
    sequence.start(13000);
    TEST_ASSERT_EQUAL_UINT32(0, sequence.frame(13000).elapsed_ms);
    TEST_ASSERT_EQUAL_UINT32(0, sequence.frame(13000).phase_ms);
    sequence.update(18599);
    TEST_ASSERT_TRUE(sequence.active());
    sequence.update(18600);
    TEST_ASSERT_FALSE(sequence.active());
}

static void test_phase_boundaries_are_exact() {
    StartupSequence sequence; sequence.start(10);
    const uint32_t times[] = {0,999,1000,2999,3000,4599,4600,5599};
    const StartupPhase phases[] = {StartupPhase::SEGMENT_TEST,StartupPhase::SEGMENT_TEST,
        StartupPhase::IDENTITY,StartupPhase::IDENTITY,StartupPhase::CHART,StartupPhase::CHART,
        StartupPhase::READY,StartupPhase::READY};
    const uint32_t phases_ms[] = {0,999,0,1999,0,1599,0,999};
    for (unsigned i = 0; i < 8; ++i) {
        const auto frame = sequence.frame(10 + times[i]);
        TEST_ASSERT_TRUE(frame.active);
        TEST_ASSERT_EQUAL_UINT32(times[i], frame.elapsed_ms);
        TEST_ASSERT_EQUAL_UINT32(phases_ms[i], frame.phase_ms);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(phases[i]), static_cast<int>(frame.phase));
    }
    TEST_ASSERT_FALSE(sequence.frame(100000).active);
}

static void assert_pilot_geometry(const HostCanvas& c, bool lit) {
    // Fixed independent Fusion fixture; changing the geometry lookup itself
    // cannot conceal a moved light point from this test.
    const int xs[16] = {10,19,28,37,46,55,64,73,86,95,104,113,122,131,140,149};
    const int ys[16] = {23,19,15,13,10,9,7,7,7,7,9,10,13,15,19,23};
    for (int i = 0; i < 16; ++i) {
        const int pair = i < 8 ? i : 15-i;
        const uint16_t expected = !lit ? 0 : pair < 4 ? 0x3eea : pair < 6 ? 0xf5e3 : 0xf205;
        for (int dy = -4; dy <= 4; ++dy) for (int dx = -4; dx <= 4; ++dx)
            if (dx*dx + dy*dy <= 16) TEST_ASSERT_EQUAL_HEX16(expected,c.at(xs[i]+dx,ys[i]+dy));
    }
    for (int y = 0; y < 80; ++y) for (int x = 0; x < 160; ++x) {
        bool opening = RecoilGeometry::windowTop[x] != 255 && y >= RecoilGeometry::windowTop[x] && y <= 75;
        for (int i = 0; i < 16; ++i)
            if ((x-xs[i])*(x-xs[i]) + (y-ys[i])*(y-ys[i]) <= 16) opening = true;
        if (!opening) TEST_ASSERT_EQUAL_HEX16(0,c.at(x,y));
    }
}

static void test_all_phases_preserve_apertures_demo_and_bounds() {
    StartupSequence sequence; sequence.start(0);
    HostCanvas pilot(160,80), nav(240,135);
    const uint32_t times[] = {0,999,1000,2999,3000,3267,3999,4000,4599,4600,5599};
    for (bool english : {false,true}) for (const uint32_t time : times) {
        const auto f = sequence.frame(time);
        TEST_ASSERT_TRUE(StartupRender::pilot(pilot,f,english));
        TEST_ASSERT_TRUE(StartupRender::navigator(nav,f,english));
        assert_pilot_geometry(pilot,time<1000);
        TEST_ASSERT_EQUAL_HEX16(Iq3Render::INK,pilot.at(73,69));
        TEST_ASSERT_EQUAL_HEX16(Iq3Render::INK,pilot.at(74,69));
        TEST_ASSERT_EQUAL_HEX16(Iq3Render::LCD,pilot.at(75,69));
        TEST_ASSERT_EQUAL_HEX16(HdsRender::INK,nav.at(215,128));
        TEST_ASSERT_EQUAL_HEX16(HdsRender::INK,nav.at(216,128));
        TEST_ASSERT_EQUAL_HEX16(HdsRender::AMBER,nav.at(217,128));
    }
    TEST_ASSERT_EQUAL_INT(0,pilot.clipped);
    TEST_ASSERT_EQUAL_INT(0,nav.clipped);
}

static void test_first_second_checks_all_lcd_segments_then_clears() {
    StartupSequence sequence; sequence.start(0);
    HostCanvas actual(160,80), reference(160,80);
    Iq3Render::Data data; data.screen=RecoilRender::Screen::BOOT; data.shift_pairs=8;
    Iq3Render::pilot(reference,data);
    StartupRender::pilot(actual,sequence.frame(0));
    TEST_ASSERT_TRUE(actual.pixels == reference.pixels);
    StartupRender::pilot(actual,sequence.frame(999));
    TEST_ASSERT_TRUE(actual.pixels == reference.pixels);
    StartupRender::pilot(actual,sequence.frame(1000));
    TEST_ASSERT_TRUE(actual.pixels != reference.pixels);
    TEST_ASSERT_EQUAL_HEX16(0,actual.at(10,23));
    // Exact version banner, without claiming the Racepak unit's firmware.
    reference.fillScreen(Iq3Render::LCD);
    RecoilRender::text(reference,"V7.1.0-RC.8-OPT",80,63,Iq3Render::INK);
    for(int y=63;y<68;++y)for(int x=50;x<110;++x)
        TEST_ASSERT_EQUAL_HEX16(reference.at(x,y),actual.at(x,y));
}

static void test_idle_matches_normal_instruments_and_does_not_move() {
    StartupSequence sequence; sequence.start(0);
    HostCanvas actual(160,80), reference(160,80), nav(240,135), nav_reference(240,135);
    Iq3Render::Data data; data.rpm=800; data.battery_x10=126; data.gear=0; data.shift_pairs=0;
    Iq3Render::pilot(reference,data);
    StartupRender::pilot(actual,sequence.frame(4000));
    TEST_ASSERT_TRUE(actual.pixels == reference.pixels);
    StartupRender::pilot(actual,sequence.frame(5599));
    TEST_ASSERT_TRUE(actual.pixels == reference.pixels);
    HdsRender::Data hd; HdsRender::render(nav_reference,hd);
    StartupRender::navigator(nav,sequence.frame(4600));
    TEST_ASSERT_TRUE(nav.pixels == nav_reference.pixels);
    StartupRender::navigator(nav,sequence.frame(5599));
    TEST_ASSERT_TRUE(nav.pixels == nav_reference.pixels);
}

static void test_chart_reveal_keeps_real_readouts_and_demo_without_fake_status() {
    StartupSequence sequence; sequence.start(0);
    HostCanvas nav(240,135), reference(240,135);
    HdsRender::Data data; HdsRender::render(reference,data);
    size_t last_matches=0;
    for (uint32_t time=3000;time<4600;time+=267) {
        StartupRender::navigator(nav,sequence.frame(time));
        for (int y=0;y<27;++y) for (int x=0;x<240;++x)
            TEST_ASSERT_EQUAL_HEX16(reference.at(x,y),nav.at(x,y));
        for (int y=125;y<135;++y) for (int x=0;x<240;++x)
            TEST_ASSERT_EQUAL_HEX16(reference.at(x,y),nav.at(x,y));
        size_t matches=0;
        for (int y=27;y<113;++y) for (int x=0;x<240;++x)
            matches += reference.at(x,y)==nav.at(x,y);
        TEST_ASSERT_TRUE(matches>=last_matches);
        last_matches=matches;
    }
}

static void test_inactive_and_unsupported_profiles_clear_safely() {
    HostCanvas pilot(160,80), nav(240,135), wrong_pilot(80,160), wrong_nav(240,240);
    StartupSequence sequence; sequence.start(0);
    TEST_ASSERT_FALSE(StartupRender::pilot(wrong_pilot,sequence.frame(0)));
    TEST_ASSERT_FALSE(StartupRender::navigator(wrong_nav,sequence.frame(0)));
    sequence.stop();
    pilot.fillScreen(123);nav.fillScreen(123);
    TEST_ASSERT_TRUE(StartupRender::pilot(pilot,sequence.frame(0)));
    TEST_ASSERT_TRUE(StartupRender::navigator(nav,sequence.frame(0)));
    for (const auto color : pilot.pixels) TEST_ASSERT_EQUAL_HEX16(0,color);
    for (const auto color : nav.pixels) TEST_ASSERT_EQUAL_HEX16(0,color);
    TEST_ASSERT_EQUAL_INT(0,wrong_pilot.clipped+wrong_nav.clipped);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_duration_cancel_restart_and_wrap);
    RUN_TEST(test_phase_boundaries_are_exact);
    RUN_TEST(test_all_phases_preserve_apertures_demo_and_bounds);
    RUN_TEST(test_first_second_checks_all_lcd_segments_then_clears);
    RUN_TEST(test_idle_matches_normal_instruments_and_does_not_move);
    RUN_TEST(test_chart_reveal_keeps_real_readouts_and_demo_without_fake_status);
    RUN_TEST(test_inactive_and_unsupported_profiles_clear_safely);
    return UNITY_END();
}
