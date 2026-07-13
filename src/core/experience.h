#pragma once
#include <stdint.h>
#include "state.h"
#include "race.h"
#include "rock_line_game.h"

constexpr const char* EXPERIENCE_VERSION = "4.1.0-beta.1";

enum class ExperienceMode : uint8_t { NORMAL, GAME };

// The mass-device setup intentionally exposes only these three identity
// fields. Score metadata is internal and survives game restarts.
struct RigProfile {
    char number[7] = "04";
    char vehicle_name[25] = "ULTRA4 4400";
    char driver[25] = "DRIVER";
    uint16_t accent = 0xFC40;
    uint32_t best_game_score = 0;
    uint16_t game_wins = 0;
};

struct ExperienceFrame {
    ExperienceMode mode = ExperienceMode::NORMAL;
    RigProfile identity;
    RockGameSnapshot game;
    float speed_mph = 0.0f;
    float rpm = 0.0f;
    float heading_deg = 0.0f;
    float battery_v = 0.0f;
    uint8_t gear = 1;
    uint8_t active_wp = 0;
};

class ExperienceEngine {
public:
    void reset(uint32_t now_ms);
    void update(uint32_t now_ms, const VehicleState& state, const UiFx& fx);
    ExperienceFrame frame(uint32_t now_ms) const;

    void setConfig(const RigProfile& config) { _config = config; }
    const RigProfile& config() const { return _config; }
    bool consumeConfigDirty();

    void enterGameAttract(uint32_t now_ms);
    void startGame(uint32_t now_ms);
    void pauseGame(uint32_t now_ms);
    void resumeGame(uint32_t now_ms);
    void controlGame(int steer, bool throttle, uint32_t now_ms);
    void stopGame();

private:
    RigProfile _config;
    RockLineGame _game;
    VehicleState _vehicle;
    bool _game_recorded = false;
    bool _config_dirty = false;
};
