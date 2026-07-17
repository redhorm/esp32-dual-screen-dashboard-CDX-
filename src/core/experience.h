#pragma once

#include <stdint.h>
#include "state.h"
#include "race.h"
#include "trail_assault_game.h"

constexpr const char* EXPERIENCE_VERSION = "5.0.0-beta.1";

enum class ExperienceMode : uint8_t { NORMAL, GAME };

// Vehicle identity remains wire-compatible with the previous beta.  Game
// records stay internal and can be migrated by the persistence service.
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
    TrailGameSnapshot game;
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
    // Dashboard renderers only need the game payload while a race is active.
    // Portal callers can request the complete snapshot explicitly.
    ExperienceFrame frame(uint32_t now_ms, bool include_game = true) const;

    void setConfig(const RigProfile& config) { _config = config; }
    const RigProfile& config() const { return _config; }
    bool consumeConfigDirty();

    void configureGame(const TrailGameConfig& config);
    const TrailGameConfig& gameConfig() const { return _game.config(); }
    void enterGameAttract(uint32_t now_ms);
    void startGame(uint32_t now_ms, uint32_t session_id = 1u);
    void pauseGame(uint32_t now_ms);
    void resumeGame(uint32_t now_ms);
    TrailInputResult controlGame(const TrailInput& input);
    TrailInputResult controlGame(int steer, int throttle, int brake,
                                 uint32_t session_id, uint16_t sequence,
                                 uint32_t client_ms, uint32_t received_ms);
    // Temporary adapter for the v2 digital endpoint while the portal migrates.
    void controlGame(int steer, bool throttle, uint32_t now_ms);
    TrailInputResult neutralGame(uint32_t session_id, uint16_t sequence,
                                 uint32_t now_ms);
    void stopGame();

    uint8_t readAudioEvents(uint16_t after, AudioEvent* output,
                            uint8_t capacity) const;
    uint16_t latestAudioSequence() const {
        return _game.audioEvents().latestSequence();
    }

    const TrailGhostRecording& bestGhost() const { return _game.bestGhost(); }
    const TrailGhostRecording& currentGhost() const {
        return _game.currentGhost();
    }
    bool loadBestGhost(const TrailGhostRecording& recording);
    void clearBestGhost();
    bool consumeGhostDirty();

private:
    RigProfile _config;
    TrailAssaultGame _game;
    VehicleState _vehicle;
    bool _game_recorded = false;
    bool _config_dirty = false;
    bool _ghost_dirty = false;
    uint32_t _known_best_ghost_crc = 0;
    uint16_t _legacy_sequence = 0;
    uint32_t _legacy_session = 1u;
};
