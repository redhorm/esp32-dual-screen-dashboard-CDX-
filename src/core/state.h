#pragma once
#include <stdint.h>

// Shared vehicle state — written by Simulator, read by both scene renderers.
// All fields are plain types so individual reads are word-atomic on ARM/Xtensa.
struct VehicleState {
    // ── Powertrain ──────────────────────────────────────────────
    float   speed_mph    = 0.0f;   // 0 – 120
    float   rpm          = 800.0f; // 0 – 7200
    int     gear         = 1;      // 1 – 5
    bool    drive_auto   = true;   // true = AUTO mode

    // ── Electrical / thermal ────────────────────────────────────
    float   battery_v    = 14.4f;  // 10 – 16 V
    float   engine_temp_f= 160.0f; // motor temp, ambient..260 °F
    float   esc_temp_f   = 130.0f; // ESC temp, heats faster than motor
    float   fuel_pct     = 0.80f;  // 0 – 1

    // ── Navigation ──────────────────────────────────────────────
    float   heading_deg  = 0.0f;   // 0 – 359
    bool    four_wd_lock = true;

    // ── Odometer ────────────────────────────────────────────────
    float   odo_mi       = 0.0f;   // stage odometer (miles from start)

    // ── Roadbook mirror (written by Roadbook::update) ────────────
    int     active_wp    = 0;
    float   dist_to_next = 0.0f;   // miles to upcoming waypoint
};
