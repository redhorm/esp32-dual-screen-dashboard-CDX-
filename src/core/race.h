#pragma once
#include "state.h"
#include "simulator.h"
#include "roadbook.h"
#include "driver.h"

// Global run phase — drives what both scenes render.
enum class Phase : uint8_t {
    BOOT,     // cinematic boot sequence on both displays
    LIVE,     // demo lap in progress
    FINISH    // result screens, held FINISH_HOLD_MS then lap restarts
};

struct RunStats {
    float    max_speed_mph = 0.0f;
    float    max_rpm       = 0.0f;
    uint32_t stage_ms      = 0;     // elapsed race time of the current lap
};

// Per-frame FX state shared with both renderers (pure data, no timing
// logic in the scenes: everything derives from these fields).
struct UiFx {
    Phase       phase       = Phase::BOOT;
    uint32_t    now_ms      = 0;     // wall clock of this frame
    uint32_t    phase_ms    = 0;     // ms elapsed in current phase
    bool        redline     = false; // rpm ≥ REDLINE_RPM (shift light)
    float       gear_snap_p = 1.0f;  // 0..1 gear-change snap progress (1 = idle)
    bool        alert       = false; // danger waypoint within ALERT_DIST_MI
    int         alert_wp    = -1;
    float       alert_dist  = 0.0f;  // miles to the danger waypoint
    // alert_info/note stay valid after the alert ends so the banner
    // can fade out with the last text still readable.
    const char* alert_info  = "";
    const char* alert_note  = "";
    uint32_t    alert_age_ms  = 0;          // ms since alert became active
    uint32_t    alert_gone_ms = 0xFFFFFFFF; // ms since it ended (max = never)
};

// Orchestrates simulator + roadbook + driver through the phase machine.
// millis()-driven, no blocking waits. Pure C++, host-testable.
class RaceController {
public:
    void reset(uint32_t now_ms);
    void update(uint32_t now_ms, Simulator& sim, Roadbook& rb);

    const UiFx&     fx()    const { return _fx; }
    const RunStats& stats() const { return _stats; }

private:
    Phase    _phase          = Phase::BOOT;
    uint32_t _phase_t0       = 0;
    uint32_t _last_ms        = 0;
    int      _prev_gear      = 1;
    uint32_t _gear_change_ms = 0;   // 0 = no change seen yet
    uint32_t _alert_on_ms    = 0;   // when the current alert started
    uint32_t _alert_off_ms   = 0;   // when the last alert ended
    bool     _alert_active   = false;
    bool     _alert_ever_off = false;
    RunStats _stats;
    UiFx     _fx;
    Driver   _driver;

    void _enter(Phase p, uint32_t now_ms);
    void _restartLap(Simulator& sim, Roadbook& rb);
    void _scanAlert(const Simulator& sim, const Roadbook& rb);
};
