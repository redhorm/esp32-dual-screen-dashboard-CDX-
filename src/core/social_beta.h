#pragma once
#include <stdint.h>
#include "state.h"
#include "race.h"
#include "rock_line_game.h"

constexpr const char* SOCIAL_BETA_VERSION = "2.0.0-beta.1";

enum class BetaMode : uint8_t { NORMAL, SHOW, HYPE, GAME };
enum class ShowProgram : uint8_t { MUD_SPLASH, WATER_FORD, NIGHT_RAID, WINCH_PULL, SEND_IT };
enum class HypeReaction : uint8_t { NONE, LEFT, RIGHT, STOP, WATER, CLEAR, WINCH, CHAMPION, CUSTOM };
enum class TrailBadge : uint8_t { NONE, SEND_IT, REDLINE, FOUR_BY_FOUR, CLEAN_LINE, STAGE_CRUSHER };

struct BetaConfig {
    char callsign[17] = "TRAIL WOLF";
    char team[21] = "ULTRA4 CREW";
    char handle[25] = "@trailwolf";
    char number[5] = "04";
    uint16_t accent = 0xCFE4;  // high-energy lime in RGB565
    uint32_t best_game_score = 0;
    uint16_t game_wins = 0;
};

struct BetaFrame {
    BetaMode mode = BetaMode::NORMAL;
    ShowProgram show_program = ShowProgram::MUD_SPLASH;
    HypeReaction hype = HypeReaction::NONE;
    TrailBadge latest_badge = TrailBadge::NONE;
    uint32_t mode_ms = 0;
    uint32_t badge_age_ms = 0xFFFFFFFFu;
    uint32_t badge_mask = 0;
    char hype_text[25] = "";
    BetaConfig identity;
    RockGameSnapshot game;
    float speed_mph = 0.0f;
    float rpm = 0.0f;
    float heading_deg = 0.0f;
    float battery_v = 0.0f;
    uint8_t gear = 1;
    uint8_t active_wp = 0;
};

class SocialBetaEngine {
public:
    void reset(uint32_t now_ms);
    void update(uint32_t now_ms, const VehicleState& s, const UiFx& fx);
    BetaFrame frame(uint32_t now_ms) const;

    void setConfig(const BetaConfig& config) { _config = config; }
    const BetaConfig& config() const { return _config; }
    bool consumeConfigDirty();

    void startShow(ShowProgram program, uint32_t now_ms);
    void stopShow();
    void triggerHype(HypeReaction reaction, const char* custom, uint32_t now_ms);
    void startGame(uint32_t now_ms);
    void controlGame(int steer, bool throttle, uint32_t now_ms);
    void stopGame();

    static const char* badgeName(TrailBadge badge);
    static const char* hypeName(HypeReaction reaction);
    static const char* showName(ShowProgram program);

private:
    BetaConfig _config;
    RockLineGame _game;
    VehicleState _vehicle;
    bool _show_active = false;
    ShowProgram _show_program = ShowProgram::MUD_SPLASH;
    uint32_t _show_t0 = 0;
    HypeReaction _hype = HypeReaction::NONE;
    char _hype_text[25] = "";
    uint32_t _hype_t0 = 0;
    uint32_t _hype_until = 0;
    TrailBadge _latest_badge = TrailBadge::NONE;
    uint32_t _badge_t0 = 0;
    uint32_t _badge_mask = 0;
    Phase _prev_phase = Phase::BOOT;
    int _prev_wp = 0;
    bool _game_recorded = false;
    bool _config_dirty = false;

    void _unlock(TrailBadge badge, uint32_t now_ms);
};
