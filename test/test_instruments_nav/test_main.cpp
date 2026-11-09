#include <unity.h>
#include <limits>
#include "app/instrument_navigator.h"
#include "core/roadbook.h"
#include "../../tools/recoil/host_canvas.h"
void setUp() {}
void tearDown() {}
static void test_route_matches_roadbook_distances_and_turns() {
    Roadbook rb;
    for(int i=0;i<HdsRender::kWaypointCount;++i) {
        TEST_ASSERT_FLOAT_WITHIN(.01f,rb.get(i).total_mi*1609.344f,HdsRender::waypointMetres(i));
        if(i==0)continue;
        auto a=HdsRender::waypointPosition(i-1),b=HdsRender::waypointPosition(i);
        float length=hypotf(b.east_m-a.east_m,b.north_m-a.north_m);
        TEST_ASSERT_FLOAT_WITHIN(.01f,rb.get(i).dist_mi*1609.344f,length);
    }
    // Independent expected compass courses from straight, R45, L90, water,
    // R45, left hairpin, R90, L45. The hairpin deliberately uses 160 degrees.
    const float courses[8]={0,45,315,315,0,200,290,245};
    for(int i=0;i<8;++i) {
        const float at=HdsRender::waypointMetres(i);
        auto s=HdsRender::sampleRoute(at);
        TEST_ASSERT_EQUAL_INT(i+1,s.nextWaypoint);
        TEST_ASSERT_FLOAT_WITHIN(.001f,courses[i],s.heading_deg);
        TEST_ASSERT_FLOAT_WITHIN(.001f,1,hypotf(s.dir_east,s.dir_north));
        auto next=HdsRender::sampleRoute(at+10);
        TEST_ASSERT_FLOAT_WITHIN(.01f,10,hypotf(next.east_m-s.east_m,next.north_m-s.north_m));
        TEST_ASSERT_FLOAT_WITHIN(.01f,s.distance_m-10,next.distance_m);
    }
}
static void test_waypoint_crossings_are_continuous_and_dtw_resets_to_next_leg() {
    for(int i=1;i<8;++i) {
        float at=HdsRender::waypointMetres(i);
        auto before=HdsRender::sampleRoute(at-.1f),exact=HdsRender::sampleRoute(at),after=HdsRender::sampleRoute(at+.1f);
        TEST_ASSERT_EQUAL_INT(i,before.nextWaypoint);TEST_ASSERT_EQUAL_INT(i+1,after.nextWaypoint);
        TEST_ASSERT_FLOAT_WITHIN(.003f,.1f,before.distance_m);
        TEST_ASSERT_FLOAT_WITHIN(.003f,.1f,hypotf(exact.east_m-before.east_m,exact.north_m-before.north_m));
        TEST_ASSERT_FLOAT_WITHIN(.003f,.1f,hypotf(exact.east_m-after.east_m,exact.north_m-after.north_m));
        TEST_ASSERT_FLOAT_WITHIN(.003f,HdsRender::waypointMetres(i+1)-at,exact.distance_m);
    }
}
static void test_finish_and_bad_inputs_are_finite_and_bounded() {
    auto start=HdsRender::sampleRoute(-100);
    auto nan=HdsRender::sampleRoute(std::numeric_limits<float>::quiet_NaN());
    TEST_ASSERT_EQUAL_FLOAT(0,start.odo_m);TEST_ASSERT_EQUAL_FLOAT(0,nan.odo_m);
    for(float at:{HdsRender::kRouteLengthM,HdsRender::kRouteLengthM+1000,std::numeric_limits<float>::infinity()}) {
        auto s=HdsRender::sampleRoute(at);auto end=HdsRender::waypointPosition(8);
        TEST_ASSERT_TRUE(s.finished);TEST_ASSERT_EQUAL_INT(8,s.nextWaypoint);
        TEST_ASSERT_FLOAT_WITHIN(.005f,0,s.distance_m);
        TEST_ASSERT_FLOAT_WITHIN(.005f,end.east_m,s.east_m);TEST_ASSERT_FLOAT_WITHIN(.005f,end.north_m,s.north_m);
    }
}
static void test_chart_pan_and_scale_are_metric_and_north_up() {
    auto p=HdsRender::sampleRoute(100);HdsRender::Chart chart={p};
    TEST_ASSERT_EQUAL_INT(120,chart.x(p.east_m));TEST_ASSERT_EQUAL_INT(94,chart.y(p.north_m));
    TEST_ASSERT_EQUAL_INT(160,chart.x(p.east_m+500));TEST_ASSERT_EQUAL_INT(54,chart.y(p.north_m+500));
    auto moved=HdsRender::sampleRoute(112.5f);HdsRender::Chart chart2={moved};
    TEST_ASSERT_EQUAL_INT(chart.y(100)+1,chart2.y(100));
}
static void test_full_stage_render_is_clipped_and_finish_is_stationary() {
    HostCanvas c(240,135);HdsRender::Data d;d.warning=true;
    for(int i=0;i<=400;++i) {
        d.odo_m=i*HdsRender::kRouteLengthM/400;d.speed_mps=30;d.kmh=i%2;d.now_ms=i*50;
        TEST_ASSERT_TRUE(HdsRender::render(c,d));
    }
    for(auto screen:{HdsRender::Screen::BOOT,HdsRender::Screen::FINISH,HdsRender::Screen::ALIGN}) {
        d.screen=screen;TEST_ASSERT_TRUE(HdsRender::render(c,d));
    }
    TEST_ASSERT_EQUAL_INT(0,c.clipped);
    d.screen=HdsRender::Screen::FINISH;d.odo_m=0;d.speed_mps=100;HdsRender::render(c,d);auto finish=c.pixels;
    d.odo_m=10000;d.speed_mps=0;HdsRender::render(c,d);TEST_ASSERT_TRUE(finish==c.pixels);
    d.screen=HdsRender::Screen::LIVE;d.odo_m=std::numeric_limits<float>::quiet_NaN();d.speed_mps=std::numeric_limits<float>::infinity();
    TEST_ASSERT_TRUE(HdsRender::render(c,d));TEST_ASSERT_EQUAL_INT(0,c.clipped);
    HostCanvas wrong(240,240);TEST_ASSERT_FALSE(HdsRender::render(wrong,d));
}
int main(int,char**) {
    UNITY_BEGIN();RUN_TEST(test_route_matches_roadbook_distances_and_turns);
    RUN_TEST(test_waypoint_crossings_are_continuous_and_dtw_resets_to_next_leg);
    RUN_TEST(test_finish_and_bad_inputs_are_finite_and_bounded);
    RUN_TEST(test_chart_pan_and_scale_are_metric_and_north_up);
    RUN_TEST(test_full_stage_render_is_clipped_and_finish_is_stationary);
    return UNITY_END();
}
