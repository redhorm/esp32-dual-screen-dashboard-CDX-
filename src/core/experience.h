#pragma once

#include <stdint.h>
#include "state.h"
#include "task_guard.h"
#include "race.h"

constexpr const char* EXPERIENCE_VERSION = "7.1.0-rc.9-audio";

// Vehicle identity remains wire-compatible with the previous beta. The two
// legacy counters remain reserved so persisted configuration is not lost.
struct RigProfile {
    char number[7] = "04";
    char vehicle_name[25] = "ULTRA4 4400";
    char driver[25] = "DRIVER";
    uint16_t accent = 0xFC40;
    uint32_t best_game_score = 0;
    uint16_t game_wins = 0;
};

struct ExperienceFrame {
    RigProfile identity;
    float speed_mph = 0.0f;
    float rpm = 0.0f;
    float heading_deg = 0.0f;
    float battery_v = 0.0f;
    int8_t gear = 1;
    bool esc_controlled = false;
    bool esc_signal_valid = false;
    uint8_t active_wp = 0;
};

class ExperienceEngine {
public:
    void reset(uint32_t now_ms);
    void update(uint32_t now_ms, const VehicleState& state, const UiFx& fx);
    ExperienceFrame frame(uint32_t now_ms) const;

    void setConfig(const RigProfile& config) { TaskGuard guard(_mutex); _config = config; }
    RigProfile config() const { TaskGuard guard(_mutex); return _config; }
    bool consumeConfigDirty();

private:
    mutable TaskMutex _mutex;
    RigProfile _config;
    VehicleState _vehicle;
    bool _config_dirty = false;
};
