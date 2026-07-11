#include "social_beta.h"
#include "config.h"
#include <string.h>

void SocialBetaEngine::reset(uint32_t now_ms) {
    _game.stop();
    _show_active = false;
    _show_t0 = now_ms;
    _hype = HypeReaction::NONE;
    _hype_text[0] = '\0';
    _hype_t0 = _hype_until = 0;
    _latest_badge = TrailBadge::NONE;
    _badge_t0 = 0;
    _badge_mask = 0;
    _prev_phase = Phase::BOOT;
    _prev_wp = 0;
    _game_recorded = false;
    _config_dirty = false;
}

void SocialBetaEngine::_unlock(TrailBadge badge, uint32_t now_ms) {
    if (badge == TrailBadge::NONE) return;
    uint32_t bit = 1UL << static_cast<uint8_t>(badge);
    if (_badge_mask & bit) return;
    _badge_mask |= bit;
    _latest_badge = badge;
    _badge_t0 = now_ms;
}

void SocialBetaEngine::update(uint32_t now_ms, const VehicleState& s, const UiFx& fx) {
    _vehicle = s;
    _game.update(now_ms);

    if (s.speed_mph >= 55.0f) _unlock(TrailBadge::SEND_IT, now_ms);
    if (s.rpm >= REDLINE_RPM) _unlock(TrailBadge::REDLINE, now_ms);
    if (s.four_wd_lock && s.speed_mph >= 25.0f) _unlock(TrailBadge::FOUR_BY_FOUR, now_ms);
    if (s.active_wp != _prev_wp && s.active_wp > 0 && !fx.alert)
        _unlock(TrailBadge::CLEAN_LINE, now_ms);
    if (fx.phase == Phase::FINISH && _prev_phase != Phase::FINISH)
        _unlock(TrailBadge::STAGE_CRUSHER, now_ms);
    _prev_wp = s.active_wp;
    _prev_phase = fx.phase;

    if (_game.isFinished() && !_game_recorded) {
        RockGameSnapshot result = _game.snapshot(now_ms);
        if (result.score > _config.best_game_score) {
            _config.best_game_score = result.score;
            _config_dirty = true;
        }
        if (result.lives > 0) {
            _config.game_wins++;
            _config_dirty = true;
        }
        _game_recorded = true;
    }
}

BetaFrame SocialBetaEngine::frame(uint32_t now_ms) const {
    BetaFrame out;
    out.identity = _config;
    out.show_program = _show_program;
    out.hype = _hype;
    strncpy(out.hype_text, _hype_text, sizeof(out.hype_text) - 1);
    out.hype_text[sizeof(out.hype_text) - 1] = '\0';
    out.latest_badge = _latest_badge;
    out.badge_mask = _badge_mask;
    out.badge_age_ms = _latest_badge == TrailBadge::NONE
        ? 0xFFFFFFFFu : now_ms - _badge_t0;
    out.game = _game.snapshot(now_ms);
    out.speed_mph = _vehicle.speed_mph;
    out.rpm = _vehicle.rpm;
    out.heading_deg = _vehicle.heading_deg;
    out.battery_v = _vehicle.battery_v;
    out.gear = (uint8_t)_vehicle.gear;
    out.active_wp = (uint8_t)_vehicle.active_wp;

    if (_game.isActive()) {
        out.mode = BetaMode::GAME;
        out.mode_ms = out.game.phase_ms;
    } else if (_hype != HypeReaction::NONE && now_ms < _hype_until) {
        out.mode = BetaMode::HYPE;
        out.mode_ms = now_ms - _hype_t0;
    } else if (_show_active) {
        out.mode = BetaMode::SHOW;
        out.mode_ms = now_ms - _show_t0;
    }
    return out;
}

bool SocialBetaEngine::consumeConfigDirty() {
    bool dirty = _config_dirty;
    _config_dirty = false;
    return dirty;
}

void SocialBetaEngine::startShow(ShowProgram program, uint32_t now_ms) {
    _game.stop();
    _show_program = program;
    _show_active = true;
    _show_t0 = now_ms;
}

void SocialBetaEngine::stopShow() { _show_active = false; }

void SocialBetaEngine::triggerHype(HypeReaction reaction, const char* custom,
                                   uint32_t now_ms) {
    _hype = reaction;
    _hype_t0 = now_ms;
    _hype_until = now_ms + 2600;
    if (reaction == HypeReaction::CUSTOM && custom) {
        strncpy(_hype_text, custom, sizeof(_hype_text) - 1);
        _hype_text[sizeof(_hype_text) - 1] = '\0';
    } else {
        strncpy(_hype_text, hypeName(reaction), sizeof(_hype_text) - 1);
        _hype_text[sizeof(_hype_text) - 1] = '\0';
    }
}

void SocialBetaEngine::startGame(uint32_t now_ms) {
    _show_active = false;
    _hype = HypeReaction::NONE;
    _hype_until = 0;
    _game_recorded = false;
    _game.start(now_ms);
}
void SocialBetaEngine::controlGame(int steer, bool throttle, uint32_t now_ms) {
    _game.control(steer, throttle, now_ms);
}
void SocialBetaEngine::stopGame() { _game.stop(); }

const char* SocialBetaEngine::badgeName(TrailBadge badge) {
    switch (badge) {
        case TrailBadge::SEND_IT:       return "SEND IT 55";
        case TrailBadge::REDLINE:       return "REDLINE KING";
        case TrailBadge::FOUR_BY_FOUR:  return "4X4 HERO";
        case TrailBadge::CLEAN_LINE:    return "CLEAN LINE";
        case TrailBadge::STAGE_CRUSHER: return "STAGE CRUSHER";
        default:                        return "";
    }
}
const char* SocialBetaEngine::hypeName(HypeReaction reaction) {
    switch (reaction) {
        case HypeReaction::LEFT:     return "SPOTTER: LEFT";
        case HypeReaction::RIGHT:    return "SPOTTER: RIGHT";
        case HypeReaction::STOP:     return "STOP! STOP!";
        case HypeReaction::WATER:    return "WATER AHEAD";
        case HypeReaction::CLEAR:    return "LINE CLEAR";
        case HypeReaction::WINCH:    return "WINCH READY";
        case HypeReaction::CHAMPION: return "TRAIL CHAMPION";
        default:                     return "";
    }
}
const char* SocialBetaEngine::showName(ShowProgram program) {
    switch (program) {
        case ShowProgram::WATER_FORD: return "WATER FORD";
        case ShowProgram::NIGHT_RAID: return "NIGHT RAID";
        case ShowProgram::WINCH_PULL: return "WINCH PULL";
        case ShowProgram::SEND_IT:    return "SEND IT";
        default:                      return "MUD SPLASH";
    }
}
