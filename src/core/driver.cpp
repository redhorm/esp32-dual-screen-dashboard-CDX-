#include "driver.h"
#include "config.h"

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

float Driver::cornerSpeed(WaypointDir dir) {
    switch (dir) {
        case WaypointDir::HAIRPIN_L:
        case WaypointDir::HAIRPIN_R: return DRV_TGT_HAIRPIN_MPH;
        case WaypointDir::LEFT_90:
        case WaypointDir::RIGHT_90:  return DRV_TGT_90_MPH;
        case WaypointDir::WATER:     return DRV_TGT_WATER_MPH;
        case WaypointDir::LEFT_45:
        case WaypointDir::RIGHT_45:  return DRV_TGT_45_MPH;
        case WaypointDir::STRAIGHT:
        case WaypointDir::FINISH:    return DRV_CRUISE_MPH;   // full send
    }
    return DRV_CRUISE_MPH;
}

DriveInputs Driver::compute(const VehicleState& s, const Roadbook& rb) const {
    const float v = s.speed_mph;
    float target = DRV_CRUISE_MPH;

    int next = s.active_wp + 1;
    if (next < rb.getCount()) {
        float corner = cornerSpeed(rb.get(next).dir);
        if (v > corner) {
            // Braking distance in miles: d = (v² − vt²) / (2·a·3600)
            // (v in mph, a in mph/s; mph·h = miles, s → h is the /3600)
            float d_brake = (v * v - corner * corner)
                          / (2.0f * DRV_BRAKE_DECEL * 3600.0f);
            if (s.dist_to_next <= d_brake * DRV_BRAKE_MARGIN)
                target = corner;
        } else if (s.dist_to_next < DRV_CORNER_HOLD_MI && corner < target) {
            target = corner;   // hold corner speed through the apex
        }
    }

    DriveInputs out;
    float err = target - v;
    if (err < -DRV_SPEED_DEADBAND) {
        out.brake = clampf(-err / DRV_BRAKE_GAIN_MPH, 0.25f, 1.0f);
    } else if (err > DRV_SPEED_DEADBAND) {
        out.throttle = clampf(err / DRV_THROTTLE_GAIN_MPH + 0.30f,
                              0.0f, DRV_MAX_THROTTLE);
    } else {
        out.throttle = 0.25f;  // maintain
    }
    return out;
}
