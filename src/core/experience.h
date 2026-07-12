#pragma once
#include <stdint.h>
#include "state.h"
#include "race.h"
#include "rock_line_game.h"

constexpr const char* EXPERIENCE_VERSION = "3.0.0-beta.2";
constexpr uint32_t RIFT_LEAD_IN_MS = 2500;
constexpr uint32_t RIFT_DURATION_MS = 13800;

enum class ExperienceMode : uint8_t { NORMAL, RIFT, GAME };
enum class RiftPhase : uint8_t {
    IDLE,
    ARMED,
    BLACKOUT,
    SCAN,
    CHARGE,
    CROSSOVER,
    IMPACT,
    HERO,
    EXIT,
};

struct RigProfile {
    char callsign[17] = "TRAIL WOLF";
    char team[21] = "ULTRA4 CREW";
    char handle[25] = "@trailwolf";
    char number[5] = "04";
    uint16_t accent = 0xCFE4;
    uint32_t best_game_score = 0;
    uint16_t game_wins = 0;
};

struct RiftFrame {
    RiftPhase phase = RiftPhase::IDLE;
    uint32_t run_id = 0;
    uint32_t seed = 0;
    uint32_t start_at_ms = 0;
    uint32_t elapsed_ms = 0;
    uint16_t progress_permille = 0;
    uint8_t layout = 0;
};

struct ExperienceFrame {
    ExperienceMode mode = ExperienceMode::NORMAL;
    RigProfile identity;
    RiftFrame rift;
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

    RiftFrame startRift(uint32_t now_ms, uint8_t layout = 0);
    void stopRift();
    void enterGameAttract(uint32_t now_ms);
    void startGame(uint32_t now_ms);
    void controlGame(int steer, bool throttle, uint32_t now_ms);
    void stopGame();

    static RiftPhase riftPhase(uint32_t elapsed_ms);
    static const char* riftPhaseName(RiftPhase phase);

private:
    RigProfile _config;
    RockLineGame _game;
    VehicleState _vehicle;
    bool _rift_active = false;
    uint32_t _rift_run_id = 0;
    uint32_t _rift_seed = 0;
    uint32_t _rift_start_at = 0;
    uint8_t _rift_layout = 0;
    bool _game_recorded = false;
    bool _config_dirty = false;
};
