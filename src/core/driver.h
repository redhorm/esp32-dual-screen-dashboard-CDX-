#pragma once
#include "state.h"
#include "roadbook.h"

// Roadbook-aware autopilot: brakes into slow waypoints with a planned
// braking distance, accelerates hard on the straights. Pure C++.
struct DriveInputs {
    float throttle = 0.0f;   // 0..1
    float brake    = 0.0f;   // 0..1
};

class Driver {
public:
    DriveInputs compute(const VehicleState& s, const Roadbook& rb) const;

    // Corner entry speed (mph) for a waypoint type
    static float cornerSpeed(WaypointDir dir);
};
