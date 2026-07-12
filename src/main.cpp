// ============================================================
// ESP32-S3 Dual Display — RC Ultra4 Dashboard + Waypoint Navigator
//
// Core 1 (Arduino loop): RaceController (sim+driver+roadbook)
//                        + dashboard @ ~30 FPS (ST7735, SPI2)
// Core 0 (FreeRTOS task): navigator @ ~12 FPS (ST7789, SPI3),
//                         slowed to NAV_FRAME_SLOW_MS while the
//                         dash runs flash-critical FX.
// Separate SPI hosts → the two DMA pushes run truly in parallel.
// No blocking delay() anywhere: loop() paces itself with millis(),
// the navigator task with vTaskDelayUntil().
// ============================================================
#include <Arduino.h>
#include "config.h"
#include "hal/display_a.h"
#include "hal/display_b.h"
#include "core/simulator.h"
#include "core/roadbook.h"
#include "core/race.h"
#include "core/ui_settings.h"
#include "core/experience.h"
#include "app/scene_dashboard.h"
#include "app/scene_navigator.h"
#include "services/config_portal.h"

static LGFX_DisplayA   dispA;
static LGFX_DisplayB   dispB;
static Simulator       sim;
static Roadbook        roadbook;
static RaceController  race;
static ExperienceEngine experience;
static SceneDashboard  dash(dispA);
static SceneNavigator  nav(dispB, roadbook);
static ConfigPortal    portal(experience);

// ── Shared frame (written by core 1, read by the nav task) ──────────────────
struct SharedFrame {
    VehicleState s;
    UiFx         fx;
    RunStats     stats;
    UiSettings   settings;
    ExperienceFrame experience;
};
static SharedFrame  shared;
static portMUX_TYPE shareMux = portMUX_INITIALIZER_UNLOCKED;

// ── Frame-time profiling (avg/max, reported every PROFILE_LOG_MS) ───────────
struct Prof {
    uint32_t n = 0, sum_us = 0, max_us = 0;
    void add(uint32_t us) { n++; sum_us += us; if (us > max_us) max_us = us; }
    uint32_t avg() const { return n ? sum_us / n : 0; }
    void reset() { n = sum_us = max_us = 0; }
};
static Prof dashRender, dashPush;
// Nav task publishes its window results here (plain 32-bit reads are atomic)
static volatile uint32_t navRenderAvg = 0, navRenderMax = 0;
static volatile uint32_t navPushAvg = 0, navPushMax = 0;
static volatile uint32_t navFps = 0;

static void navTask(void*) {
    Prof render, push;
    uint32_t frames = 0, win_t0 = millis();
    uint8_t prev_brightness = 0;
    TickType_t last = xTaskGetTickCount();

    for (;;) {
        SharedFrame f;
        taskENTER_CRITICAL(&shareMux);
        f = shared;
        taskEXIT_CRITICAL(&shareMux);

        if (DISP_B_BL >= 0 && f.settings.navigator_brightness != prev_brightness) {
            dispB.setBrightness(f.settings.navigator_brightness);
            prev_brightness = f.settings.navigator_brightness;
        }

        uint32_t t0 = micros();
        nav.setSettings(f.settings);
        nav.setExperienceFrame(f.experience);
        nav.render(f.s, f.fx, f.stats);
        uint32_t t1 = micros();
        nav.push();
        uint32_t t2 = micros();
        render.add(t1 - t0);
        push.add(t2 - t1);
        frames++;

        uint32_t now = millis();
        if (now - win_t0 >= PROFILE_LOG_MS) {
            navRenderAvg = render.avg();  navRenderMax = render.max_us;
            navPushAvg   = push.avg();    navPushMax   = push.max_us;
            navFps       = frames * 1000 / (now - win_t0);
            render.reset(); push.reset();
            frames = 0; win_t0 = now;
        }

        // Yield the wall clock to the dash during flash-critical FX
        bool fx_critical = f.fx.phase == Phase::LIVE
                        && (f.fx.redline || f.fx.alert);
        uint32_t period = fx_critical ? NAV_FRAME_SLOW_MS : NAV_FRAME_MS;
        vTaskDelayUntil(&last, pdMS_TO_TICKS(period));
    }
}

void setup() {
    Serial.begin(115200);

    bool ap_ok = portal.begin();
    Serial.printf("[portal] AP %s | SSID=%s | http://%s\n",
                  ap_ok ? "ready" : "failed", CONFIG_AP_SSID, portal.ipString());

    dispA.init();
    dispB.init();
    const UiSettings& settings = portal.settings();
    if (DISP_A_BL >= 0) dispA.setBrightness(settings.driver_brightness);
    if (DISP_B_BL >= 0) dispB.setBrightness(settings.navigator_brightness);

    dash.init();
    nav.init();
    sim.reset();
    roadbook.reset();
    race.reset(millis());
    experience.reset(millis());
    shared.settings = settings;

    xTaskCreatePinnedToCore(navTask, "nav", 8192, nullptr, 1, nullptr, 0);
}

void loop() {
    static uint32_t last_frame = 0;
    static uint32_t frames = 0, win_t0 = 0;

    portal.handle();

    uint32_t now = millis();
    if (now - last_frame < DASH_FRAME_MS) return;
    last_frame = now;

    const UiSettings settings = portal.settings();
    static uint8_t prev_dash_brightness = 0;
    if (DISP_A_BL >= 0 && settings.driver_brightness != prev_dash_brightness) {
        dispA.setBrightness(settings.driver_brightness);
        prev_dash_brightness = settings.driver_brightness;
    }

    race.update(now, sim, roadbook);
    experience.update(now, sim.state, race.fx());
    const ExperienceFrame experience_frame = experience.frame(now);

    taskENTER_CRITICAL(&shareMux);
    shared.s     = sim.state;
    shared.fx    = race.fx();
    shared.stats = race.stats();
    shared.settings = settings;
    shared.experience = experience_frame;
    taskEXIT_CRITICAL(&shareMux);

    uint32_t t0 = micros();
    dash.setSettings(settings);
    dash.setExperienceFrame(experience_frame);
    dash.render(sim.state, race.fx(), race.stats());
    uint32_t t1 = micros();
    dash.push();
    uint32_t t2 = micros();
    dashRender.add(t1 - t0);
    dashPush.add(t2 - t1);
    frames++;

    if (now - win_t0 >= PROFILE_LOG_MS) {
        if (win_t0 != 0) {
            uint32_t fps = frames * 1000 / (now - win_t0);
            Serial.printf(
                "[prof] dash %2u FPS render %lu/%lu us push %lu/%lu us | "
                "nav %2u FPS render %lu/%lu us push %lu/%lu us | "
                "heap %u KB | "
                "spd=%.0f rpm=%.0f gear=%d wp=%d odo=%.2f phase=%d\n",
                (unsigned)fps,
                (unsigned long)dashRender.avg(), (unsigned long)dashRender.max_us,
                (unsigned long)dashPush.avg(),   (unsigned long)dashPush.max_us,
                (unsigned)navFps,
                (unsigned long)navRenderAvg, (unsigned long)navRenderMax,
                (unsigned long)navPushAvg,   (unsigned long)navPushMax,
                (unsigned)(esp_get_free_heap_size() / 1024),
                sim.state.speed_mph, sim.state.rpm, sim.state.gear,
                sim.state.active_wp, sim.state.odo_mi, (int)race.fx().phase);
        }
        dashRender.reset();
        dashPush.reset();
        frames = 0;
        win_t0 = now;
    }
}
