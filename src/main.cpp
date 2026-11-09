// RECOIL PULSE: one display owner, one timeline, independent safety I/O.
#include <Arduino.h>
#include <atomic>
#include "services/storm_service.h"
#include "services/storm_ota.h"
#include "app/storm_renderer.h"
#include <esp_heap_caps.h>
#include "config.h"
#include "hal/display_a.h"
#include "hal/display_b.h"
#include "hal/ws2812_rtl.h"
#include "hal/esc_pwm_input.h"
#include "core/experience.h"
#include "core/esc_dashboard.h"
#include "core/diagnostics.h"
#include "core/button_gesture.h"
#include "core/show_sequence.h"
#include "core/startup_sequence.h"
#include "app/scene_dashboard.h"
#include "app/scene_navigator.h"
#include "app/show_renderer.h"
#include "app/startup_renderer.h"
#include "services/config_portal.h"

static LGFX_DisplayA dispA;
static LGFX_DisplayB dispB;
static Simulator sim;
static Roadbook roadbook;
static RaceController race;
static EscDashboardController escDashboard;
static ExperienceEngine experience;
static DiagnosticsRegistry diagnostics;
static RtlLogic rtl(RtlProfile::ULTRA4_4400,RTL_LED_COUNT,RTL_COMMAND_TIMEOUT_MS);
static Ws2812Rtl rtlHardware;
static EscPwmInput escPwm;
static SceneDashboard dash(dispA);
static SceneNavigator nav(dispB,roadbook);
static ConfigPortal portal(experience,rtl,rtlHardware,escPwm,diagnostics);
static ShowSequence show;
static StartupSequence startup;
static ButtonGesture button;
static bool apReady=false;
static bool dashReady=false,navReady=false,controlTaskStarted=false,portalTaskStarted=false;
static std::atomic<uint32_t> dashHeartbeat{0},navHeartbeat{0},controlHeartbeat{0};
static std::atomic<uint32_t> renderStackFree{0},controlStackFree{0};
static bool bootPhoto=false,bootPhotoA=false;
static bool startupPending=false,startupComplete=false;
static uint32_t bootStarted=0;
static bool psramTest=false;
static void verifyPsram(){
 const size_t count=262144;auto p=(uint32_t*)heap_caps_malloc(count*sizeof(uint32_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!p)return;for(size_t i=0;i<count;i++)p[i]=uint32_t(i)^0xA55A3CC3;
 psramTest=true;for(size_t i=0;i<count;i++)if(p[i]!=(uint32_t(i)^0xA55A3CC3)){psramTest=false;break;}
 heap_caps_free(p);
}


static void completeStartup(uint32_t now, bool interrupted=false){
    if(startupComplete)return;
    startup.stop();startupPending=false;startupComplete=true;
    bootPhoto=false;bootPhotoA=false;
    race.finishBoot(now);
    Serial.printf("[startup] %s; instrument view ready\n",interrupted?"interrupted":"complete");
}

static void publishHealth(uint32_t now){
    DiagnosticsSlowHealth h;
    const uint32_t a=dashHeartbeat.load(),b=navHeartbeat.load(),c=controlHeartbeat.load();
    now=millis(); // sample after the cross-core heartbeat loads
    h.driver_heartbeat_age_ms=a?uint32_t(now-a):UINT32_MAX;
    h.navigator_heartbeat_age_ms=b?uint32_t(now-b):UINT32_MAX;
    h.driver_ok=dashReady&&h.driver_heartbeat_age_ms<500;
    h.navigator_ok=navReady&&h.navigator_heartbeat_age_ms<500;
    h.driver_stack_free_bytes=h.navigator_stack_free_bytes=renderStackFree.load();
    h.memory.heap_free_bytes=ESP.getFreeHeap();
    h.memory.heap_min_free_bytes=ESP.getMinFreeHeap();
    h.memory.heap_largest_block_bytes=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    h.memory.psram_present=psramFound();
    h.memory.psram_total_bytes=ESP.getPsramSize();
    h.memory.psram_free_bytes=ESP.getFreePsram();
    // Slow filesystem/network work stays in the HTTP task, never in rendering.
    portal.fillDiagnosticsSlowHealth(h);
    h.output.control_age_ms=c?uint32_t(now-c):UINT32_MAX;
    h.output.control_alive=h.output.control_age_ms<250;
    h.output.control_stack_free_bytes=controlStackFree.load();
    if(!h.output.control_alive)h.last_error="control_task_stalled";
    diagnostics.publishSlowHealth(now,h);
    StormOta::health(h.driver_ok&&h.navigator_ok&&h.output.control_alive&&portalTaskStarted&&apReady&&psramTest);
}
static void controlTask(void*){
    TickType_t wake=xTaskGetTickCount();uint16_t samples=0;
    for(;;){
        portal.controlIoTick();controlHeartbeat.store(millis());
        if(++samples==100){samples=0;controlStackFree.store(uxTaskGetStackHighWaterMark(nullptr));}
        vTaskDelayUntil(&wake,pdMS_TO_TICKS(10));
    }
}
static void portalTask(void*){
    uint32_t lastHealth=0;
    for(;;){
        portal.handle();
        uint32_t now=millis();
        if(now-lastHealth>=1000){lastHealth=now;publishHealth(now);}
        vTaskDelay(1);
    }
}
void setup(){
    Serial.begin(115200);
    Serial.printf("[stormDASH memory] flash=%u psram=%u free=%u\n",ESP.getFlashChipSize(),ESP.getPsramSize(),ESP.getFreePsram());
    verifyPsram();
    diagnostics.reset(millis());diagnostics.setIdentity(EXPERIENCE_VERSION,4);
    const auto profile=portal.loadCopilotDisplayProfile();
    const auto& display=copilotDisplayConfig(profile);
    dispB.setProfile(profile);nav.setDisplayProfile(profile);
    photoStore.setNavigatorDisplay(display.logical_width,display.logical_height);
    const bool hardwareA=dispA.init(),hardwareB=dispB.init();
    dashReady=hardwareA&&dash.init();navReady=hardwareB&&nav.init();
    // Clear panel RAM while services initialise. The timed segment check
    // begins only once setup is ready, never frozen/repeated during FS mount.
    if(dashReady){dash.surface().fillScreen(0);dash.push();}
    if(navReady){nav.surface().fillScreen(0);nav.push();}
    const bool ap=portal.begin();apReady=ap;
    Serial.printf("[stormDASH] %s | display %ux%u | AP %s | %s | http://%s\n",
        EXPERIENCE_VERSION,display.logical_width,display.logical_height,ap?"ready":"offline",portal.ssidString(),portal.ipString());
    if(!dashReady||!navReady)diagnostics.setLastError("display_init_failed");
    const UiSettings settings=portal.settings();
    if(DISP_A_BL>=0)dispA.setBrightness(settings.driver_brightness);
    if(DISP_B_BL>=0)dispB.setBrightness(settings.navigator_brightness);
    pinMode(CONFIG_AP_REVEAL_PIN,INPUT_PULLUP);
    sim.reset();roadbook.reset();
    // One optional boot image, decoded once before the HTTP task starts.
    // No filesystem lock, JPEG decode or gallery work remains in the render loop.
    if(dashReady&&settings.boot_image_enabled&&photoStore.ready()){
        bootPhotoA=photoStore.drawBoot(dash.surface(),false);
        if(bootPhotoA){
            for(int x=0;x<160;++x){
                const int top=RecoilGeometry::windowTop[x];
                dash.surface().fillRect(x,0,1,top==255?80:top,0);
            }
            dash.surface().fillRect(0,76,160,4,0);
            RecoilRender::lamps(dash.surface(),0);dash.push();
        }
    }
    if(navReady&&settings.boot_image_enabled&&photoStore.ready()){
        bootPhoto=photoStore.drawBoot(nav.surface(),true);
        if(bootPhoto)nav.push();
    }
    // A personal image, if present, precedes the instrument startup. It is
    // already decoded; holding it requires no extra sprite or filesystem I/O.
    bootStarted=millis();race.reset(bootStarted);experience.reset(bootStarted);
    startupPending=bootPhoto||bootPhotoA;
    if(!startupPending)startup.start(bootStarted);
    TaskHandle_t handle=nullptr;
    controlTaskStarted=xTaskCreatePinnedToCore(controlTask,"control-io",4096,nullptr,3,&handle,0)==pdPASS;
    if(!controlTaskStarted)diagnostics.setLastError("control_task_start_failed");
    portalTaskStarted=xTaskCreatePinnedToCore(portalTask,"config-portal",8192,nullptr,1,&handle,0)==pdPASS;
    if(!portalTaskStarted)diagnostics.setLastError("portal_task_start_failed");
}
void loop(){
    static uint32_t lastFrame=0,lastNav=0,lastControl=0,lastProfile=0,wifiSince=0,lastFallbackHealth=0;
    static bool wifiVisible=false,previousShow=false;
    static uint8_t previousA=0,previousB=0;
    if(!portalTaskStarted)portal.handle();
    const uint32_t now=millis();
    if(!controlTaskStarted&&now-lastControl>=10){lastControl=now;portal.controlIoTick();controlHeartbeat.store(now);}
    bool start=false,force=false;
    if(portal.takeShowCommand(start)){
        if(start&&!show.active()){
            StormService::command(Storm::Action::Show);
            if(dashReady&&navReady){completeStartup(now,true);show.start(now);wifiVisible=false;}
            else diagnostics.setLastError("show_display_unavailable");
        }else if(!start){show.stop();StormService::command(Storm::Action::Exit);}
        force=true;
    }
    switch(button.update(digitalRead(CONFIG_AP_REVEAL_PIN)==LOW,now)){
        case ButtonAction::TOGGLE_SHOW: {
            completeStartup(now,true);
            const auto mode = StormService::snapshot().mode;
            if(mode==Storm::Mode::Game) StormService::command(Storm::Action::Jump,1);
            else if(Storm::isWow(mode)) StormService::command(Storm::Action::Impulse);
            else if(mode!=Storm::Mode::Instruments) StormService::command(Storm::Action::Exit);
            else if(dashReady&&navReady) StormService::command(Storm::Action::Show);
            wifiVisible=false;force=true;break;
        }
        case ButtonAction::REVEAL_WIFI:
            completeStartup(now,true);
            show.stop();StormService::command(Storm::Action::Exit);wifiVisible=true;wifiSince=now;force=true;break;
        default:break;
    }
    if(wifiVisible&&now-wifiSince>=15000){wifiVisible=false;force=true;}
    StormService::update(now);
    const auto storm=StormService::snapshot();
    const bool stormActive=storm.mode!=Storm::Mode::Instruments;
    if(stormActive){completeStartup(now,true);show.stop();wifiVisible=false;}
    show.update(now);
    const bool active=show.active();
    if(active!=previousShow){force=true;previousShow=active;}
    if(!force&&now-lastFrame<DASH_FRAME_MS){vTaskDelay(1);return;}
    lastFrame=now;
    const UiSettings settings=portal.settings();
    if(!startupComplete){
        if(settings.driver_theme==UiTheme::RECOIL_ALIGN || settings.navigator_theme==UiTheme::RECOIL_ALIGN){
            completeStartup(now,true);force=true;
        }else{
            if(startupPending&&(!settings.boot_image_enabled||now-bootStarted>=settings.boot_hold_ms)){
                startupPending=false;bootPhoto=false;bootPhotoA=false;startup.start(now);force=true;
            }
            startup.update(now);
            if(!startupPending&&!startup.active()){completeStartup(now);force=true;}
        }
    }
    if(DISP_A_BL>=0&&previousA!=settings.driver_brightness){previousA=settings.driver_brightness;dispA.setBrightness(previousA);}
    if(DISP_B_BL>=0&&previousB!=settings.navigator_brightness){previousB=settings.navigator_brightness;dispB.setBrightness(previousB);}
    const EscPwmSample esc = portal.escDashboardInput();
    static bool wasEsc = false;
    if (wasEsc && !esc.enabled) {
        sim.reset(); roadbook.reset(); race.reset(now);
        if (startupComplete) race.finishBoot(now);
    }
    wasEsc = esc.enabled;
    escDashboard.update(now, esc.enabled, esc.valid, esc.normalized, esc.state);
    if(startupComplete && !esc.enabled)race.update(now,sim,roadbook);
    VehicleState vehicle = esc.enabled ? escDashboard.state() : sim.state;
    vehicle.esc_controlled = esc.enabled;
    vehicle.esc_signal_valid = escDashboard.signalValid();
    const UiFx& instrumentFx = esc.enabled ? escDashboard.fx() : race.fx();
    const RunStats& instrumentStats = esc.enabled ? escDashboard.stats() : race.stats();
    experience.update(now,vehicle,instrumentFx);
    const ShowFrame showFrame=show.frame(now);
    const StartupFrame startupFrame=startup.frame(now);
    portal.publishShowState(active||storm.mode==Storm::Mode::Show,storm.mode==Storm::Mode::Show?storm.showMs:showFrame.elapsed_ms);
    // Single owner: both SHOW buffers are rendered from exactly this frame.
    uint32_t started=micros();
    if(dashReady){
        if(stormActive)StormRender::pilot(dash.surface(),storm);
        else if(active)ShowRender::pilot(dash.surface(),showFrame,settings.language==UiLanguage::ENGLISH);
        else if(startupFrame.active)StartupRender::pilot(dash.surface(),startupFrame,settings.language==UiLanguage::ENGLISH);
        else if(!startupPending){
            dash.setSettings(settings);dash.setPortalCredentials(wifiVisible,portal.ssidString(),portal.passwordString());
            dash.render(vehicle,instrumentFx,instrumentStats);
        }
        uint32_t rendered=micros();dash.push();uint32_t pushed=micros();
        dashHeartbeat.store(now);diagnostics.recordFrame(DiagnosticDisplay::DRIVER,now,pushed-started,rendered-started,pushed-rendered);
    }
    if(navReady&&(force||stormActive||active||startupFrame.active||now-lastNav>=NAV_FRAME_MS)){
        lastNav=now;started=micros();
        if(stormActive)StormRender::navigator(nav.surface(),storm);
        else if(active)ShowRender::navigator(nav.surface(),showFrame,settings.language==UiLanguage::ENGLISH);
        else if(startupFrame.active)StartupRender::navigator(nav.surface(),startupFrame,settings.language==UiLanguage::ENGLISH);
        else if(!startupPending){
            nav.setSettings(settings);nav.setPortalCredentials(wifiVisible,portal.ssidString(),portal.passwordString());
            nav.render(vehicle,instrumentFx,instrumentStats);
        }
        uint32_t rendered=micros();nav.push();uint32_t pushed=micros();
        navHeartbeat.store(now);diagnostics.recordFrame(DiagnosticDisplay::NAVIGATOR,now,pushed-started,rendered-started,pushed-rendered);
    }
    if(now-lastProfile>=PROFILE_LOG_MS){
        lastProfile=now;renderStackFree.store(uxTaskGetStackHighWaterMark(nullptr));
        auto a=diagnostics.resetFrameWindow(DiagnosticDisplay::DRIVER,now);
        auto b=diagnostics.resetFrameWindow(DiagnosticDisplay::NAVIGATOR,now);
        Serial.printf("[esc] mode=%s valid=%u pulse=%u speed=%.1f rpm=%.0f raw=%u cmd=%.3f state=%s\n",
            esc.enabled?"ESC":"DEMO", esc.valid, esc.filtered_us, vehicle.speed_mph, vehicle.rpm, esc.pulse_us, esc.normalized, EscSignalProcessor::stateName(esc.state));
        const auto health=diagnostics.snapshot(); // Locked cross-task copy; never read mutable PhotoStore flags here.
        Serial.printf("[memory] flash=%u psram=%u free_psram=%u psram_1MB_test=%s\n",ESP.getFlashChipSize(),ESP.getPsramSize(),ESP.getFreePsram(),psramTest?"PASS":"FAIL");
        Serial.printf("[pulse] show=%u startup=%u | A=%lu B=%lu FPS | heap=%lu min=%lu | storage=%s error=%s\n",active,!startupComplete,
            (unsigned long)a.fps,(unsigned long)b.fps,(unsigned long)ESP.getFreeHeap(),(unsigned long)ESP.getMinFreeHeap(),
            health.filesystem.ready?"ready":"unavailable",health.last_error[0]?health.last_error:"none");
    }
    if(!portalTaskStarted&&now-lastFallbackHealth>=1000){lastFallbackHealth=now;publishHealth(now);}
    vTaskDelay(1);
}
