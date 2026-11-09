#include <unity.h>
#include <type_traits>
#include "core/show_sequence.h"
#include "app/show_renderer.h"
#include "../../tools/recoil/host_canvas.h"

void setUp() {}
void tearDown() {}

static void test_duration_is_exact_and_delayed_render_cannot_extend_show() {
    ShowSequence show;
    TEST_ASSERT_FALSE(show.active());
    show.start(100);
    TEST_ASSERT_TRUE(show.frame(100).active);
    TEST_ASSERT_TRUE(show.frame(18099).active);
    TEST_ASSERT_FALSE(show.frame(18100).active);
    show.update(18100);
    TEST_ASSERT_FALSE(show.active());
    TEST_ASSERT_FALSE(show.frame(50000).active);
}

static void test_cancel_and_repeat_restart_every_field() {
    ShowSequence show;
    show.start(0);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ShowPhase::LINK), static_cast<int>(show.frame(9000).phase));
    show.stop();
    TEST_ASSERT_FALSE(show.active());
    TEST_ASSERT_FALSE(show.frame(9000).active);
    show.start(9000);
    const auto first = show.frame(9000);
    TEST_ASSERT_TRUE(first.active);
    TEST_ASSERT_EQUAL_UINT32(0, first.elapsed_ms);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ShowPhase::ACQUIRE), static_cast<int>(first.phase));
    TEST_ASSERT_EQUAL_INT(0, first.route_permille);
    TEST_ASSERT_EQUAL_INT(1, first.checkpoint);
    show.start(12000);
    TEST_ASSERT_EQUAL_UINT32(0, show.frame(12000).elapsed_ms);
    show.update(29999);
    TEST_ASSERT_TRUE(show.active());
    show.update(30000);
    TEST_ASSERT_FALSE(show.active());
}

static void test_time_wrap_keeps_phases_and_completion_correct() {
    ShowSequence show;
    const uint32_t start = UINT32_MAX - 9000U;
    show.start(start);
    const auto peak = show.frame(start + 8000U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ShowPhase::LINK), static_cast<int>(peak.phase));
    TEST_ASSERT_EQUAL_INT(255, peak.pulse);
    TEST_ASSERT_EQUAL_INT(16, peak.lit);
    const auto wrap = show.frame(start + 10000U);
    TEST_ASSERT_TRUE(wrap.active);
    TEST_ASSERT_EQUAL_UINT32(10000, wrap.elapsed_ms);
    show.update(start + 17999U);
    TEST_ASSERT_TRUE(show.active());
    show.update(start + 18000U);
    TEST_ASSERT_FALSE(show.active());
}

static void test_timeline_boundaries_and_values_are_bounded_and_monotonic() {
    ShowSequence show; show.start(0);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ShowPhase::ACQUIRE), static_cast<int>(show.frame(1999).phase));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ShowPhase::ROUTE), static_cast<int>(show.frame(2000).phase));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ShowPhase::ROUTE), static_cast<int>(show.frame(7999).phase));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ShowPhase::LINK), static_cast<int>(show.frame(8000).phase));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ShowPhase::LINK), static_cast<int>(show.frame(14499).phase));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ShowPhase::FINISH), static_cast<int>(show.frame(14500).phase));
    uint16_t last_route = 0, last_progress = 0;
    for (uint32_t now = 0; now < ShowSequence::kDurationMs; ++now) {
        const auto frame = show.frame(now);
        TEST_ASSERT_TRUE(frame.active);
        TEST_ASSERT_TRUE(frame.lit <= 16);
        TEST_ASSERT_TRUE(frame.speed_demo <= 88);
        TEST_ASSERT_TRUE(frame.checkpoint >= 1 && frame.checkpoint <= 4);
        TEST_ASSERT_TRUE(frame.route_permille >= last_route && frame.route_permille <= 1000);
        TEST_ASSERT_TRUE(frame.progress_permille >= last_progress && frame.progress_permille <= 1000);
        last_route = frame.route_permille;
        last_progress = frame.progress_permille;
    }
}

static void assert_unchanged_apertures_and_mask(const HostCanvas& canvas, const ShowFrame& frame, int pairs = 0) {
    // Independent, fixed Fusion fixture, rather than checking against the same
    // array used to render. Optical patch colours remain those of the original.
    const int xs[16] = {10,19,28,37,46,55,64,73,86,95,104,113,122,131,140,149};
    const int ys[16] = {23,19,15,13,10,9,7,7,7,7,9,10,13,15,19,23};
    for (int i = 0; i < 16; ++i) {
        uint16_t color;
        if (ShowRender::instrumentPhase(frame)) {
            const int pair = i < 8 ? i : 15-i;
            color = pair >= pairs ? 0 : pair < 4 ? 0x3eea : pair < 6 ? 0xf5e3 : 0xf205;
        } else color = i >= frame.lit ? 0 : i < 8 ? 0x3eea : i < 12 ? 0xf5e3 : 0xf205;
        for (int dy = -4; dy <= 4; ++dy) for (int dx = -4; dx <= 4; ++dx)
            if (dx * dx + dy * dy <= 16)
                TEST_ASSERT_EQUAL_HEX16(color, canvas.at(xs[i] + dx, ys[i] + dy));
    }
    for (int y = 0; y < 80; ++y) for (int x = 0; x < 160; ++x) {
        bool opening = RecoilGeometry::windowTop[x] != 255 && y >= RecoilGeometry::windowTop[x] && y <= 75;
        bool lamp = false;
        for (int i = 0; i < 16; ++i)
            if ((x - xs[i]) * (x - xs[i]) + (y - ys[i]) * (y - ys[i]) <= 16) lamp = true;
        if (!opening && !lamp) TEST_ASSERT_EQUAL_HEX16(0, canvas.at(x, y));
    }
}

static void assert_demo_badges(const HostCanvas& pilot, const HostCanvas& nav, const ShowFrame& frame) {
    if (ShowRender::instrumentPhase(frame)) {
        TEST_ASSERT_EQUAL_HEX16(Iq3Render::INK, pilot.at(73,69));
        TEST_ASSERT_EQUAL_HEX16(Iq3Render::INK, pilot.at(74,69));
        TEST_ASSERT_EQUAL_HEX16(HdsRender::INK, nav.at(215,128));
        TEST_ASSERT_EQUAL_HEX16(HdsRender::INK, nav.at(216,128));
        TEST_ASSERT_EQUAL_HEX16(HdsRender::AMBER, nav.at(217,128));
    } else {
        TEST_ASSERT_EQUAL_HEX16(ShowRender::AMBER, pilot.at(73,70));
        TEST_ASSERT_EQUAL_HEX16(ShowRender::AMBER, nav.at(195,5));
    }
}

static void test_every_phase_keeps_cad_geometry_and_demo_badges_visible() {
    ShowSequence show; show.start(0);
    HostCanvas pilot(160,80), nav(240,135);
    // Sample phase boundaries, the pulse peak/valley and final visible frame.
    const uint32_t samples[] = {0,100,600,1199,1999,2000,5000,7999,8000,8800,12000,14499,14500,17999};
    const int pairs[] =        {0,0,0,0,0,0,7,8,8,4,4,7,0,0};
    for (size_t i=0;i<sizeof(samples)/sizeof(samples[0]);++i) {
        const uint32_t time = samples[i];
        const auto frame = show.frame(time);
        TEST_ASSERT_TRUE(ShowRender::pilot(pilot, frame));
        TEST_ASSERT_TRUE(ShowRender::navigator(nav, frame));
        assert_unchanged_apertures_and_mask(pilot, frame, pairs[i]);
        assert_demo_badges(pilot,nav,frame);
    }
    TEST_ASSERT_EQUAL_INT(0, pilot.clipped);
    TEST_ASSERT_EQUAL_INT(0, nav.clipped);
}

static void test_profile_rejection_is_explicit_and_does_not_clip() {
    ShowSequence show; show.start(0);
    HostCanvas wrong_pilot(80,160), wrong_nav(240,240);
    TEST_ASSERT_FALSE(ShowRender::pilot(wrong_pilot, show.frame(0)));
    TEST_ASSERT_FALSE(ShowRender::navigator(wrong_nav, show.frame(0)));
    TEST_ASSERT_EQUAL_INT(0, wrong_pilot.clipped);
    TEST_ASSERT_EQUAL_INT(0, wrong_nav.clipped);
}

static void test_english_captions_preserve_apertures_and_persistent_demo() {
    ShowSequence show; show.start(0);
    HostCanvas pilot(160,80), nav(240,135);
    for (uint32_t time : {0U, 2000U, 8000U, 16000U}) {
        const auto frame = show.frame(time);
        TEST_ASSERT_TRUE(ShowRender::pilot(pilot, frame, true));
        TEST_ASSERT_TRUE(ShowRender::navigator(nav, frame, true));
        assert_unchanged_apertures_and_mask(pilot, frame, time==8000U?8:0);
        assert_demo_badges(pilot,nav,frame);
    }
    TEST_ASSERT_EQUAL_INT(0, pilot.clipped);
    TEST_ASSERT_EQUAL_INT(0, nav.clipped);
}

static void test_show_distance_is_integral_of_speed_and_reaches_real_first_turn() {
    ShowSequence show; show.start(0);
    TEST_ASSERT_FLOAT_WITHIN(.001f,620.0f,ShowRender::demoState(show.frame(2000)).odo_m);
    TEST_ASSERT_FLOAT_WITHIN(.001f,693.333333f,ShowRender::demoState(show.frame(8000)).odo_m);
    TEST_ASSERT_FLOAT_WITHIN(.001f,44.0f,ShowRender::demoState(show.frame(5000)).speed_kmh);
    TEST_ASSERT_FLOAT_WITHIN(.001f,88.0f,ShowRender::demoState(show.frame(8000)).speed_kmh);
    TEST_ASSERT_FLOAT_WITHIN(.001f,72.0f,ShowRender::demoState(show.frame(8800)).speed_kmh);
    const float full_cycle = ShowRender::demoState(show.frame(9600)).odo_m-ShowRender::demoState(show.frame(8000)).odo_m;
    TEST_ASSERT_FLOAT_WITHIN(.001f,80.0f/3.6f*1.6f,full_cycle);
    const auto before = HdsRender::sampleRoute(ShowRender::demoState(show.frame(8000)).odo_m);
    const auto after = HdsRender::sampleRoute(ShowRender::demoState(show.frame(10000)).odo_m);
    TEST_ASSERT_EQUAL_INT(1,before.nextWaypoint);
    TEST_ASSERT_EQUAL_INT(2,after.nextWaypoint);
    TEST_ASSERT_FLOAT_WITHIN(.001f,0,before.heading_deg);
    TEST_ASSERT_FLOAT_WITHIN(.001f,45,after.heading_deg);
    for (uint32_t t=2100;t<14500;t+=100) {
        const auto a=ShowRender::demoState(show.frame(t-50)),b=ShowRender::demoState(show.frame(t+50));
        const auto middle=ShowRender::demoState(show.frame(t));
        // A finite difference across a triangle corner has a small known error.
        TEST_ASSERT_FLOAT_WITHIN(.55f,middle.speed_kmh,(b.odo_m-a.odo_m)*36.0f);
        TEST_ASSERT_TRUE(b.odo_m>a.odo_m);
    }
}

static void test_show_uses_real_instruments_and_one_coherent_vehicle_state() {
    ShowSequence show; show.start(0);
    HostCanvas actual(160,80),expected(160,80),nav(240,135),base_nav(240,135);
    for(uint32_t t : {2000U,5000U,8000U,8800U,10000U,14499U}) {
        const auto frame=show.frame(t);
        const auto pilot=ShowRender::pilotData(frame);
        const auto navigator=ShowRender::navigatorData(frame);
        TEST_ASSERT_EQUAL_INT(pilot.speed,static_cast<int>(lroundf(navigator.speed_mps*3.6f)));
        TEST_ASSERT_TRUE(pilot.rpm>=800&&pilot.rpm<=7200);
        TEST_ASSERT_TRUE(pilot.gear>=0&&pilot.gear<=4);
        ShowRender::pilot(actual,frame);Iq3Render::pilot(expected,pilot);
        TEST_ASSERT_TRUE(actual.pixels==expected.pixels);
        ShowRender::navigator(nav,frame);HdsRender::render(base_nav,navigator);
        // Mode annotation never obscures the real SOG/COG/DTW or DEMO fields.
        for(int y=0;y<27;++y)for(int x=0;x<240;++x)
            TEST_ASSERT_EQUAL_HEX16(base_nav.at(x,y),nav.at(x,y));
        for(int y=125;y<134;++y)for(int x=0;x<240;++x)
            TEST_ASSERT_EQUAL_HEX16(base_nav.at(x,y),nav.at(x,y));
    }
}

static_assert(std::is_trivially_copyable<ShowFrame>::value, "Frames must remain safe to copy between display tasks");
static_assert(sizeof(ShowFrame) <= 24, "Do not grow show state into a framebuffer");

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_duration_is_exact_and_delayed_render_cannot_extend_show);
    RUN_TEST(test_cancel_and_repeat_restart_every_field);
    RUN_TEST(test_time_wrap_keeps_phases_and_completion_correct);
    RUN_TEST(test_timeline_boundaries_and_values_are_bounded_and_monotonic);
    RUN_TEST(test_every_phase_keeps_cad_geometry_and_demo_badges_visible);
    RUN_TEST(test_profile_rejection_is_explicit_and_does_not_clip);
    RUN_TEST(test_english_captions_preserve_apertures_and_persistent_demo);
    RUN_TEST(test_show_distance_is_integral_of_speed_and_reaches_real_first_turn);
    RUN_TEST(test_show_uses_real_instruments_and_one_coherent_vehicle_state);
    return UNITY_END();
}
