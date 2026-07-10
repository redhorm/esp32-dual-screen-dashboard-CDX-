#include "roadbook.h"

Roadbook::Roadbook() {
    _loadStage();
}

void Roadbook::_loadStage() {
    // Demo stage ~12 miles, 9 waypoints (hardcoded roadbook, WAY MAP style)
    using D = WaypointDir;
    using K = NoteKind;
    _wp[0] = {0.00f,  0.00f, D::STRAIGHT,  "START",      "LINE",       K::NEUTRAL,   false};
    _wp[1] = {0.45f,  0.45f, D::RIGHT_45,  "R 45",       "",           K::GOOD,      false};
    _wp[2] = {0.78f,  1.23f, D::LEFT_90,   "L 90",       "CAUTION !!", K::WARN,      true };
    _wp[3] = {1.11f,  2.34f, D::WATER,     "WATER XING", "2FT DEEP",   K::INFO_BLUE, false};
    _wp[4] = {0.87f,  3.21f, D::RIGHT_45,  "S 45",       "FAST",       K::GOOD,      false};
    _wp[5] = {2.46f,  5.67f, D::HAIRPIN_L, "L HAIRPIN",  "SLOW !!",    K::WARN,      true };
    _wp[6] = {2.67f,  8.34f, D::RIGHT_90,  "R 90",       "STAY RIGHT", K::GOOD,      false};
    _wp[7] = {1.44f,  9.78f, D::LEFT_45,   "L 45",       "",           K::GOOD,      false};
    _wp[8] = {2.27f, 12.05f, D::FINISH,    "FINISH",     "LINE",       K::NEUTRAL,   false};
    _count  = 9;
    _active = 0;
}

void Roadbook::reset() {
    _active = 0;
}

void Roadbook::update(VehicleState& s) {
    // Note: the lap does NOT loop here — RaceController owns the
    // finish detection (result screens) and calls reset() afterwards.

    // Advance active waypoint when odometer passes its total distance
    while (_active < _count - 1 && s.odo_mi >= _wp[_active + 1].total_mi)
        _active++;

    s.active_wp    = _active;
    s.dist_to_next = (_active < _count - 1)
                         ? (_wp[_active + 1].total_mi - s.odo_mi)
                         : 0.0f;
}
