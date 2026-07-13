#include "experience.h"

void ExperienceEngine::reset(uint32_t now_ms) {
    (void)now_ms;
    _game.stop();
    _game_recorded = false;
    _config_dirty = false;
}

void ExperienceEngine::update(uint32_t now_ms, const VehicleState& state,
                              const UiFx&) {
    _vehicle = state;
    _game.update(now_ms);

    if (_game.isFinished() && !_game_recorded) {
        const RockGameSnapshot result = _game.snapshot(now_ms);
        if (result.score > _config.best_game_score) {
            _config.best_game_score = result.score;
            _config_dirty = true;
        }
        if (result.course_clear) {
            ++_config.game_wins;
            _config_dirty = true;
        }
        _game_recorded = true;
    }
}

ExperienceFrame ExperienceEngine::frame(uint32_t now_ms) const {
    ExperienceFrame out;
    out.identity = _config;
    out.game = _game.snapshot(now_ms);
    out.speed_mph = _vehicle.speed_mph;
    out.rpm = _vehicle.rpm;
    out.heading_deg = _vehicle.heading_deg;
    out.battery_v = _vehicle.battery_v;
    out.gear = static_cast<uint8_t>(_vehicle.gear);
    out.active_wp = static_cast<uint8_t>(_vehicle.active_wp);
    if (_game.isActive()) out.mode = ExperienceMode::GAME;
    return out;
}

bool ExperienceEngine::consumeConfigDirty() {
    const bool dirty = _config_dirty;
    _config_dirty = false;
    return dirty;
}

void ExperienceEngine::enterGameAttract(uint32_t now_ms) {
    _game_recorded = false;
    _game.enterArcade(now_ms);
}

void ExperienceEngine::startGame(uint32_t now_ms) {
    _game_recorded = false;
    _game.start(now_ms);
}

void ExperienceEngine::pauseGame(uint32_t now_ms) {
    _game.pause(now_ms);
}

void ExperienceEngine::resumeGame(uint32_t now_ms) {
    _game.resume(now_ms);
}

void ExperienceEngine::controlGame(int steer, bool throttle,
                                   uint32_t now_ms) {
    _game.control(steer, throttle, now_ms);
}

void ExperienceEngine::stopGame() { _game.stop(); }
