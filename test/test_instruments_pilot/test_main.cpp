#include <unity.h>
#include <climits>
#include "app/instrument_pilot.h"
#include "../../tools/recoil/host_canvas.h"

void setUp() {}
void tearDown() {}

static void assert_mask_and_apertures(const HostCanvas& c, int pairs) {
    const int xs[16] = {10,19,28,37,46,55,64,73,86,95,104,113,122,131,140,149};
    const int ys[16] = {23,19,15,13,10,9,7,7,7,7,9,10,13,15,19,23};
    for (int i = 0; i < 16; ++i) {
        const int pair = i < 8 ? i : 15 - i;
        const uint16_t color = pair >= pairs ? 0 : pair < 4 ? 0x3EEA : pair < 6 ? 0xF5E3 : 0xF205;
        for (int dy = -4; dy <= 4; ++dy) for (int dx = -4; dx <= 4; ++dx)
            if (dx * dx + dy * dy <= 16) TEST_ASSERT_EQUAL_HEX16(color, c.at(xs[i] + dx, ys[i] + dy));
    }
    for (int y = 0; y < 80; ++y) for (int x = 0; x < 160; ++x) {
        bool visible = RecoilGeometry::windowTop[x] != 255 && y >= RecoilGeometry::windowTop[x] && y <= 75;
        for (int i = 0; i < 16; ++i)
            if ((x - xs[i]) * (x - xs[i]) + (y - ys[i]) * (y - ys[i]) <= 16) visible = true;
        if (!visible) TEST_ASSERT_EQUAL_HEX16(0, c.at(x, y));
    }
    TEST_ASSERT_EQUAL_INT(0, c.clipped);
}

static void assert_demo(const HostCanvas& c) {
    // Independent D glyph positions in the always-visible bottom-centre badge.
    TEST_ASSERT_EQUAL_HEX16(Iq3Render::INK, c.at(73, 69));
    TEST_ASSERT_EQUAL_HEX16(Iq3Render::INK, c.at(74, 69));
    TEST_ASSERT_EQUAL_HEX16(Iq3Render::INK, c.at(73, 73));
    TEST_ASSERT_EQUAL_HEX16(Iq3Render::LCD, c.at(75, 69));
}

static void test_pair_lamps_are_symmetric_and_keep_every_cad_aperture() {
    HostCanvas c(160,80); Iq3Render::Data data;
    for (int pairs = 0; pairs <= 8; ++pairs) {
        data.shift_pairs = pairs;
        TEST_ASSERT_TRUE(Iq3Render::pilot(c,data));
        assert_mask_and_apertures(c,pairs);
        assert_demo(c);
    }
}

static void test_rpm_drives_tach_and_pairs_without_moving_instrument_labels() {
    HostCanvas idle(160,80), rev(160,80); Iq3Render::Data data;
    data.rpm = 0; Iq3Render::pilot(idle,data);
    data.rpm = 7200; Iq3Render::pilot(rev,data);
    TEST_ASSERT_EQUAL_HEX16(Iq3Render::INACTIVE, idle.at(13,35));
    TEST_ASSERT_EQUAL_HEX16(Iq3Render::INK, rev.at(13,35));
    TEST_ASSERT_TRUE(idle.pixels != rev.pixels);
    assert_mask_and_apertures(idle,0);
    assert_mask_and_apertures(rev,8);
    for (int y = 32; y <= 62; ++y) for (int x = 65; x <= 95; ++x)
        TEST_ASSERT_EQUAL_HEX16(idle.at(x,y),rev.at(x,y));
    TEST_ASSERT_EQUAL_INT(0,Iq3Render::pairCount(INT_MIN));
    TEST_ASSERT_EQUAL_INT(0,Iq3Render::pairCount(4399));
    TEST_ASSERT_EQUAL_INT(1,Iq3Render::pairCount(4400));
    TEST_ASSERT_EQUAL_INT(8,Iq3Render::pairCount(7200));
    TEST_ASSERT_EQUAL_INT(8,Iq3Render::pairCount(INT_MAX));
}

static void test_central_gear_changes_and_side_units_are_visible() {
    HostCanvas a(160,80), b(160,80); Iq3Render::Data data;
    data.gear=1; Iq3Render::pilot(a,data);
    data.gear=8; Iq3Render::pilot(b,data);
    int changed=0;
    for(int y=39;y<=62;++y)for(int x=68;x<=92;++x)if(a.at(x,y)!=b.at(x,y))++changed;
    TEST_ASSERT_TRUE(changed>50);
    data.kmh=false; Iq3Render::pilot(a,data);
    data.kmh=true; Iq3Render::pilot(b,data);
    changed=0;
    for(int y=40;y<=44;++y)for(int x=26;x<=44;++x)if(a.at(x,y)!=b.at(x,y))++changed;
    TEST_ASSERT_TRUE(changed>0);
}

static void test_extremes_startup_finish_and_warnings_keep_demo_and_mask() {
    HostCanvas c(160,80); Iq3Render::Data data;
    for (auto page : {RecoilRender::Screen::BOOT,RecoilRender::Screen::LIVE,RecoilRender::Screen::FINISH}) {
        data.screen=page;
        for (int value : {INT_MIN,-1,0,1,999,INT_MAX}) {
            data.speed=value; data.gear=value; data.rpm=value; data.battery_x10=value;
            data.temp=value; data.elapsed_ms=UINT32_MAX; data.warning=true;
            TEST_ASSERT_TRUE(Iq3Render::pilot(c,data));
            assert_demo(c);
            assert_mask_and_apertures(c,Iq3Render::pairCount(value));
        }
    }
}

static void test_alignment_remains_byte_identical_to_delivered_calibration() {
    HostCanvas a(160,80), b(160,80);
    Iq3Render::Data data;data.screen=RecoilRender::Screen::ALIGN;
    RecoilRender::PilotData original;original.screen=RecoilRender::Screen::ALIGN;
    Iq3Render::pilot(a,data); RecoilRender::pilot(b,original);
    TEST_ASSERT_TRUE(a.pixels == b.pixels);
    HostCanvas invalid(80,160);
    data.screen=RecoilRender::Screen::LIVE;
    TEST_ASSERT_FALSE(Iq3Render::pilot(invalid,data));
    TEST_ASSERT_EQUAL_INT(0,invalid.clipped);
}

int main(int,char**) {
    UNITY_BEGIN();
    RUN_TEST(test_pair_lamps_are_symmetric_and_keep_every_cad_aperture);
    RUN_TEST(test_rpm_drives_tach_and_pairs_without_moving_instrument_labels);
    RUN_TEST(test_central_gear_changes_and_side_units_are_visible);
    RUN_TEST(test_extremes_startup_finish_and_warnings_keep_demo_and_mask);
    RUN_TEST(test_alignment_remains_byte_identical_to_delivered_calibration);
    return UNITY_END();
}
