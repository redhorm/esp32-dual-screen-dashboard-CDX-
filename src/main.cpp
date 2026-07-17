// ESP32-S3 Ultra4 Microdash — production runtime
// Core 1: deterministic simulation, portal and 30 FPS pilot display.
// Core 0: 100 Hz safety/control I/O plus independent navigator rendering.
#include <Arduino.h>
#include <esp_heap_caps.h>
#include "config.h"
#include "hal/display_a.h"
#include "hal/display_b.h"
#include "core/simulator.h"
#include "core/roadbook.h"
#include "core/race.h"
#include "core/ui_settings.h"
#include "core/ui_i18n.h"
#include "core/experience.h"
#include "core/rtl_logic.h"
#include "core/diagnostics.h"
#include "app/scene_dashboard.h"
#include "app/scene_navigator.h"
#include "hal/ws2812_rtl.h"
#include "hal/esc_pwm_input.h"
#include "services/config_portal.h"

static LGFX_DisplayA dispA;
static LGFX_DisplayB dispB;
static Simulator sim;
static Roadbook roadbook;
static RaceController race;
static ExperienceEngine experience;
static DiagnosticsRegistry diagnostics;
static RtlLogic rtl(RtlProfile::ULTRA4_4400, RTL_LED_COUNT,
                    RTL_COMMAND_TIMEOUT_MS);
static Ws2812Rtl rtlHardware;
static EscPwmInput escPwm;
static SceneDashboard dash(dispA);
static SceneNavigator nav(dispB, roadbook);
static ConfigPortal portal(experience, rtl, rtlHardware, escPwm, diagnostics);

static bool dashReady = false;
static bool navReady = false;
static bool navTaskStarted = false;
static bool controlTaskStarted = false;
static volatile uint32_t dashHeartbeatMs = 0;
static volatile uint32_t navHeartbeatMs = 0;
static volatile uint32_t controlHeartbeatMs = 0;
static volatile uint32_t dashStackFreeBytes = 0;
static volatile uint32_t navStackFreeBytes = 0;
static volatile uint32_t controlStackFreeBytes = 0;

struct SharedFrame {
    VehicleState state;
    UiFx fx;
    RunStats stats;
    UiSettings settings;
    ExperienceFrame experience;
    bool portal_credentials_visible = true;
};

// Sequence lock: the single writer never blocks either display task, while a
// reader retries if it observes an in-progress copy.  The frame is immutable
// for the remainder of each renderer pass.
static SharedFrame sharedFrame;
static volatile uint32_t sharedSequence = 0;

static void publishSharedFrame(const SharedFrame& frame) {
    ++sharedSequence;  // odd: write in progress
    __sync_synchronize();
    sharedFrame = frame;
    __sync_synchronize();
    ++sharedSequence;  // even: stable snapshot
}

static SharedFrame readSharedFrame() {
    SharedFrame frame;
    for (;;) {
        const uint32_t before = sharedSequence;
        if (before & 1u) {
            taskYIELD();
            continue;
        }
        __sync_synchronize();
        frame = sharedFrame;
        __sync_synchronize();
        const uint32_t after = sharedSequence;
        if (before == after && !(after & 1u)) return frame;
    }
}

struct Profiler {
    uint32_t samples = 0;
    uint64_t sum_us = 0;
    uint32_t max_us = 0;

    void add(uint32_t microseconds) {
        ++samples;
        sum_us += microseconds;
        if (microseconds > max_us) max_us = microseconds;
    }
    uint32_t average() const {
        return samples ? static_cast<uint32_t>(sum_us / samples) : 0u;
    }
    void reset() {
        samples = 0;
        sum_us = 0;
        max_us = 0;
    }
};

static DisplayDiagnostics pendingNavProfile;
static bool navProfilePending = false;
static portMUX_TYPE navProfileMux = portMUX_INITIALIZER_UNLOCKED;

static void publishNavProfile(const DisplayDiagnostics& profile) {
    taskENTER_CRITICAL(&navProfileMux);
    pendingNavProfile = profile;
    navProfilePending = true;
    taskEXIT_CRITICAL(&navProfileMux);
}

static void importNavProfile() {
    DisplayDiagnostics profile;
    bool pending;
    taskENTER_CRITICAL(&navProfileMux);
    pending = navProfilePending;
    if (pending) {
        profile = pendingNavProfile;
        navProfilePending = false;
    }
    taskEXIT_CRITICAL(&navProfileMux);
    if (pending)
        diagnostics.publishDisplayWindow(DiagnosticDisplay::NAVIGATOR, profile);
}

static void drawEarlyLogo() {
    constexpr uint16_t black = 0x0000;
    constexpr uint16_t amber = 0xFD20;
    constexpr uint16_t white = 0xFFFF;
    dispA.fillScreen(black);
    dispA.setTextDatum(lgfx::MC_DATUM);
    dispA.setFont(&lgfx::fonts::Font2);
    dispA.setTextColor(white, black);
    dispA.drawString("ULTRA4", 80, 27);
    dispA.setTextColor(amber, black);
    dispA.drawString("MICRODASH", 80, 48);
    dispA.drawFastHLine(25, 64, 110, amber);

    dispB.fillScreen(black);
    dispB.setTextDatum(lgfx::MC_DATUM);
    dispB.setFont(&lgfx::fonts::Font4);
    dispB.setTextColor(white, black);
    dispB.drawString("ULTRA4", 120, 82);
    dispB.setTextColor(amber, black);
    dispB.drawString("MICRODASH", 120, 116);
    dispB.setFont(&lgfx::fonts::Font2);
    dispB.drawString("DUAL SCREEN CONTROL SYSTEM", 120, 151);
    dispB.drawFastHLine(34, 174, 172, amber);
}

static void drawBootHealth(bool ap_ok, UiLanguage language) {
    constexpr uint16_t black = 0x0000;
    constexpr uint16_t panel = 0x18E3;
    constexpr uint16_t amber = 0xFD20;
    constexpr uint16_t green = 0x4E48;
    constexpr uint16_t red = 0xF800;
    constexpr uint16_t white = 0xFFFF;
    constexpr uint16_t muted = 0x7BEF;
    const bool fs_ok = photoStore.ready();
    const bool psram_ok = psramFound();
    const bool rtl_ok = rtlHardware.ready();

    dispA.fillScreen(black);
    dispA.fillRect(0, 0, 160, 16, amber);
    dispA.setTextDatum(lgfx::MC_DATUM);
    dispA.setFont(&lgfx::fonts::Font2);
    dispA.setTextColor(black, amber);
    dispA.drawString(uiText(language, "CONTROLLO", "SYSTEM CHECK"), 80, 8);
    dispA.setFont(&lgfx::fonts::Font0);
    dispA.setTextColor(dashReady ? green : red, black);
    dispA.drawString(dashReady ? "DISPLAY A  OK" :
                     uiText(language, "DISPLAY A GUASTO", "DISPLAY A  FAIL"),
                     80, 31);
    dispA.setTextColor(fs_ok ? green : red, black);
    dispA.drawString(fs_ok ? "MEDIA      OK" : "MEDIA      OFF", 80, 46);
    dispA.setTextColor(ap_ok ? green : red, black);
    dispA.drawString(ap_ok ? uiText(language, "RETE AP     OK", "ACCESS POINT OK") :
                     uiText(language, "RETE AP     OFF", "ACCESS POINT OFF"),
                     80, 61);

    dispB.fillScreen(black);
    dispB.fillRect(0, 0, 240, 32, amber);
    dispB.setTextDatum(lgfx::MC_DATUM);
    dispB.setFont(&lgfx::fonts::Font4);
    dispB.setTextColor(black, amber);
    dispB.drawString(uiText(language, "CONTROLLO SISTEMA", "SYSTEM CHECK"),
                     120, 16);
    struct HealthRow { const char* name; const char* value; uint16_t color; };
    const HealthRow rows[] = {
        {"DISPLAY A", dashReady ? uiText(language, "PRONTO", "READY") :
                                  uiText(language, "GUASTO", "FAILED"),
         dashReady ? green : red},
        {"DISPLAY B", navReady ? uiText(language, "PRONTO", "READY") :
                                 uiText(language, "GUASTO", "FAILED"),
         navReady ? green : red},
        {"LITTLEFS", fs_ok ? uiText(language, "PRONTO", "READY") : "MEDIA OFF",
         fs_ok ? green : red},
        {"PSRAM", psram_ok ? uiText(language, "PRONTA", "READY") :
                             uiText(language, "OPZIONALE", "OPTIONAL"),
         psram_ok ? green : muted},
        {"RTL", rtl_ok ? uiText(language, "PRONTA", "READY") :
                         uiText(language, "DISATTIVA", "OFFLINE"),
         rtl_ok ? green : red},
        {uiText(language, "INGRESSO ESC", "ESC INPUT"),
         escPwm.enabled() ? uiText(language, "ATTIVO", "ENABLED") :
                            uiText(language, "SICURO / OFF", "SAFE / OFF"),
         amber},
    };
    dispB.setFont(&lgfx::fonts::Font2);
    for (uint8_t index = 0; index < sizeof(rows) / sizeof(rows[0]); ++index) {
        const int y = 43 + index * 31;
        dispB.fillRoundRect(8, y, 224, 25, 4, panel);
        dispB.setTextDatum(lgfx::ML_DATUM);
        dispB.setTextColor(white, panel);
        dispB.drawString(rows[index].name, 16, y + 12);
        dispB.setTextDatum(lgfx::MR_DATUM);
        dispB.setTextColor(rows[index].color, panel);
        dispB.drawString(rows[index].value, 224, y + 12);
    }
    dispA.setTextDatum(lgfx::TL_DATUM);
    dispB.setTextDatum(lgfx::TL_DATUM);
}

static void controlTask(void*) {
    TickType_t last_wake = xTaskGetTickCount();
    uint16_t stack_sample = 0;
    for (;;) {
        portal.controlIoTick();
        controlHeartbeatMs = millis();
        if (++stack_sample >= 100u) {
            stack_sample = 0;
            controlStackFreeBytes = uxTaskGetStackHighWaterMark(nullptr) *
                                    sizeof(StackType_t);
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
    }
}

static void navigatorTask(void*) {
    Profiler frame_profile;
    Profiler render_profile;
    Profiler push_profile;
    uint32_t frames = 0;
    uint32_t window_started = millis();
    uint8_t previous_brightness = 0;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        const SharedFrame frame = readSharedFrame();
        if (DISP_B_BL >= 0 &&
            frame.settings.navigator_brightness != previous_brightness) {
            dispB.setBrightness(frame.settings.navigator_brightness);
            previous_brightness = frame.settings.navigator_brightness;
        }

        const uint32_t started_us = micros();
        nav.setSettings(frame.settings);
        nav.setExperienceFrame(frame.experience);
        nav.setPortalCredentials(frame.portal_credentials_visible,
                                 portal.ssidString(), portal.passwordString());
        nav.render(frame.state, frame.fx, frame.stats);
        const uint32_t rendered_us = micros();
        nav.push();
        const uint32_t pushed_us = micros();
        navHeartbeatMs = millis();
        render_profile.add(rendered_us - started_us);
        push_profile.add(pushed_us - rendered_us);
        frame_profile.add(pushed_us - started_us);
        ++frames;

        const uint32_t now = millis();
        if (now - window_started >= PROFILE_LOG_MS) {
            DisplayDiagnostics profile;
            profile.ok = navReady;
            profile.fps = frames * 1000u / (now - window_started);
            profile.window_frames = frames;
            profile.frame_avg_us = frame_profile.average();
            profile.frame_max_us = frame_profile.max_us;
            profile.render_avg_us = render_profile.average();
            profile.render_max_us = render_profile.max_us;
            profile.push_avg_us = push_profile.average();
            profile.push_max_us = push_profile.max_us;
            publishNavProfile(profile);
            navStackFreeBytes = uxTaskGetStackHighWaterMark(nullptr) *
                                sizeof(StackType_t);
            frame_profile.reset();
            render_profile.reset();
            push_profile.reset();
            frames = 0;
            window_started = now;
        }

        const bool flash_critical = frame.fx.phase == Phase::LIVE &&
            (frame.fx.redline || frame.fx.alert);
        uint32_t period = flash_critical ? NAV_FRAME_SLOW_MS : NAV_FRAME_MS;
        if (frame.experience.mode == ExperienceMode::GAME)
            period = GAME_NAV_FRAME_MS;
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(period));
    }
}

static void publishSlowDiagnostics(uint32_t now) {
    DiagnosticsSlowHealth health;
    const uint32_t dash_age = dashHeartbeatMs
        ? now - dashHeartbeatMs : UINT32_MAX;
    const uint32_t nav_age = navHeartbeatMs
        ? now - navHeartbeatMs : UINT32_MAX;
    const uint32_t control_age = controlHeartbeatMs
        ? now - controlHeartbeatMs : UINT32_MAX;
    health.driver_ok = dashReady && dash_age < 500u;
    health.navigator_ok = navReady && navTaskStarted && nav_age < 500u;
    health.driver_heartbeat_age_ms = dash_age;
    health.navigator_heartbeat_age_ms = nav_age;
    health.driver_stack_free_bytes = dashStackFreeBytes;
    health.navigator_stack_free_bytes = navStackFreeBytes;
    health.memory.heap_free_bytes = ESP.getFreeHeap();
    health.memory.heap_min_free_bytes = ESP.getMinFreeHeap();
    health.memory.heap_largest_block_bytes =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    health.memory.psram_present = psramFound();
    if (health.memory.psram_present) {
        health.memory.psram_total_bytes = ESP.getPsramSize();
        health.memory.psram_free_bytes = ESP.getFreePsram();
    }
    portal.fillDiagnosticsSlowHealth(health);
    health.output.control_alive = control_age < 250u;
    health.output.control_age_ms = control_age;
    health.output.control_stack_free_bytes = controlStackFreeBytes;
    if (!health.output.control_alive)
        health.last_error = "control_task_stalled";
    diagnostics.publishSlowHealth(now, health);
}

void setup() {
    Serial.begin(115200);
    diagnostics.reset(millis());
    diagnostics.setIdentity(EXPERIENCE_VERSION, 4);

    // Allocate the two DMA-friendly internal framebuffers before Wi-Fi and
    // LittleFS fragment the heap.  A failure degrades one display only.
    dispA.init();
    dispB.init();
    drawEarlyLogo();
    dashReady = dash.init();
    navReady = nav.init();

    const bool ap_ok = portal.begin();
    Serial.printf("[portal] AP %s | SSID=%s | http://%s\n",
                  ap_ok ? "ready" : "failed", portal.ssidString(),
                  portal.ipString());

    const UiSettings settings = portal.settings();
    if (DISP_A_BL >= 0) dispA.setBrightness(settings.driver_brightness);
    if (DISP_B_BL >= 0) dispB.setBrightness(settings.navigator_brightness);
    pinMode(CONFIG_AP_REVEAL_PIN, INPUT_PULLUP);
    sim.reset();
    roadbook.reset();
    const uint32_t now = millis();
    race.reset(now);
    experience.reset(now);

    SharedFrame initial;
    initial.state = sim.state;
    initial.fx = race.fx();
    initial.stats = race.stats();
    initial.settings = settings;
    initial.experience = experience.frame(now, false);
    initial.portal_credentials_visible = true;
    publishSharedFrame(initial);

    TaskHandle_t control_handle = nullptr;
    controlTaskStarted = xTaskCreatePinnedToCore(
        controlTask, "control-io", 4096, nullptr, 3,
        &control_handle, 0) == pdPASS;
    if (!controlTaskStarted)
        diagnostics.setLastError("control_task_start_failed");

    drawBootHealth(ap_ok, settings.language);
    vTaskDelay(pdMS_TO_TICKS(240));

    if (navReady) {
        TaskHandle_t nav_handle = nullptr;
        navTaskStarted = xTaskCreatePinnedToCore(
            navigatorTask, "navigator", 8192, nullptr, 1,
            &nav_handle, 0) == pdPASS;
        if (!navTaskStarted)
            diagnostics.setLastError("navigator_task_start_failed");
    }
    publishSlowDiagnostics(millis());
}

void loop() {
    static uint32_t last_frame_ms = 0;
    static uint32_t last_health_ms = 0;
    static uint32_t last_control_fallback_ms = 0;
    static uint32_t profile_window_ms = 0;
    static uint32_t profile_dash_frames = 0;

    portal.handle();
    uint32_t now = millis();
    experience.update(now, sim.state, race.fx());
    importNavProfile();

    if (!controlTaskStarted && now - last_control_fallback_ms >= 10u) {
        last_control_fallback_ms = now;
        portal.controlIoTick();
        controlHeartbeatMs = now;
    }
    if (now - last_health_ms >= 1000u) {
        last_health_ms = now;
        publishSlowDiagnostics(now);
    }

    if (now - last_frame_ms < DASH_FRAME_MS) {
        vTaskDelay(1);
        return;
    }
    last_frame_ms = now;

    const UiSettings settings = portal.settings();
    static uint8_t previous_dash_brightness = 0;
    if (DISP_A_BL >= 0 &&
        settings.driver_brightness != previous_dash_brightness) {
        dispA.setBrightness(settings.driver_brightness);
        previous_dash_brightness = settings.driver_brightness;
    }

    race.update(now, sim, roadbook);
    experience.update(now, sim.state, race.fx());
    const ExperienceFrame experience_frame = experience.frame(now, false);
    const bool show_credentials = !portal.setupComplete() ||
        digitalRead(CONFIG_AP_REVEAL_PIN) == LOW;

    SharedFrame frame;
    frame.state = sim.state;
    frame.fx = race.fx();
    frame.stats = race.stats();
    frame.settings = settings;
    frame.experience = experience_frame;
    frame.portal_credentials_visible = show_credentials;
    publishSharedFrame(frame);

    const uint32_t started_us = micros();
    dash.setSettings(settings);
    dash.setExperienceFrame(experience_frame);
    dash.setPortalCredentials(show_credentials, portal.ssidString(),
                              portal.passwordString());
    dash.render(sim.state, race.fx(), race.stats());
    const uint32_t rendered_us = micros();
    dash.push();
    ++profile_dash_frames;
    const uint32_t pushed_us = micros();
    dashHeartbeatMs = now;
    dashStackFreeBytes = uxTaskGetStackHighWaterMark(nullptr) *
                         sizeof(StackType_t);
    diagnostics.recordFrame(DiagnosticDisplay::DRIVER, now,
                            pushed_us - started_us,
                            rendered_us - started_us,
                            pushed_us - rendered_us);
    if (now - profile_window_ms >= PROFILE_LOG_MS) {
        const DisplayDiagnostics dash_profile =
            diagnostics.resetFrameWindow(DiagnosticDisplay::DRIVER, now);
        const DiagnosticsSnapshot& report = diagnostics.snapshot();
        const uint32_t profile_elapsed = now - profile_window_ms;
        const uint32_t measured_dash_fps = profile_window_ms && profile_elapsed
            ? profile_dash_frames * 1000u / profile_elapsed : 0u;
        if (profile_window_ms != 0) {
            Serial.printf(
                "[prof] dash %lu FPS render %lu/%lu us push %lu/%lu us | "
                "nav %lu FPS render %lu/%lu us push %lu/%lu us | "
                "heap %lu KB min %lu KB | game tick=%lu status=%u\n",
                static_cast<unsigned long>(measured_dash_fps),
                static_cast<unsigned long>(dash_profile.render_avg_us),
                static_cast<unsigned long>(dash_profile.render_max_us),
                static_cast<unsigned long>(dash_profile.push_avg_us),
                static_cast<unsigned long>(dash_profile.push_max_us),
                static_cast<unsigned long>(report.navigator.fps),
                static_cast<unsigned long>(report.navigator.render_avg_us),
                static_cast<unsigned long>(report.navigator.render_max_us),
                static_cast<unsigned long>(report.navigator.push_avg_us),
                static_cast<unsigned long>(report.navigator.push_max_us),
                static_cast<unsigned long>(report.memory.heap_free_bytes / 1024u),
                static_cast<unsigned long>(report.memory.heap_min_free_bytes / 1024u),
                static_cast<unsigned long>(experience_frame.game.tick),
                static_cast<unsigned>(experience_frame.game.status));
        }
        profile_dash_frames = 0;
        profile_window_ms = now;
    }
}
