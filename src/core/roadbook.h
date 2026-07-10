#pragma once
#include "state.h"

enum class WaypointDir : uint8_t {
    STRAIGHT,
    RIGHT_45,
    RIGHT_90,
    LEFT_45,
    LEFT_90,
    HAIRPIN_R,
    HAIRPIN_L,
    WATER,
    FINISH
};

// Color hint for the note line. Core stays color-agnostic:
// the renderer maps these to actual RGB565 values.
enum class NoteKind : uint8_t {
    NEUTRAL,   // black/white
    GOOD,      // green  ("FAST", "STAY RIGHT")
    WARN,      // red    ("CAUTION !!", "SLOW !!")
    INFO_BLUE  // blue   ("2FT DEEP")
};

struct Waypoint {
    float       dist_mi;   // distance from PREVIOUS waypoint (segment)
    float       total_mi;  // cumulative distance from stage start
    WaypointDir dir;
    const char* info;      // main description line ("L 90")
    const char* note;      // second line ("CAUTION !!"), "" if none
    NoteKind    kind;      // color of the note line
    bool        danger;    // highlight DIST cell in red
};

class Roadbook {
public:
    static const int MAX_WP = 12;

    Roadbook();

    void reset();
    // Called every tick; advances active waypoint when odo passes total_mi
    void update(VehicleState& s);

    int             getActive()  const { return _active; }
    int             getCount()   const { return _count; }
    const Waypoint& get(int i)   const { return _wp[i]; }
    bool            isFinished() const { return _active >= _count - 1; }
    float           stageTotal() const { return _wp[_count - 1].total_mi; }

private:
    Waypoint _wp[MAX_WP];
    int      _count  = 0;
    int      _active = 0;

    void _loadStage();
};
