#include "experience.h"

namespace {
uint32_t mixSeed(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    return value ^ (value >> 16);
}
}

void ExperienceEngine::reset(uint32_t now_ms) {
    _game.stop();
    _rift_active = false;
    _rift_run_id = 0;
    _rift_seed = mixSeed(now_ms ^ 0x52494654u);
    _rift_start_at = 0;
    _rift_layout = 0;
    _game_recorded = false;
    _config_dirty = false;
}

void ExperienceEngine::update(uint32_t now_ms, const VehicleState& state,
                              const UiFx&) {
    _vehicle = state;
    _game.update(now_ms);

    if (_rift_active && (int32_t)(now_ms - _rift_start_at) >= 0 &&
        now_ms - _rift_start_at >= RIFT_DURATION_MS)
        _rift_active = false;

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

RiftPhase ExperienceEngine::riftPhase(uint32_t elapsed_ms) {
    if (elapsed_ms < 1200) return RiftPhase::BLACKOUT;
    if (elapsed_ms < 3200) return RiftPhase::SCAN;
    if (elapsed_ms < 6100) return RiftPhase::CHARGE;
    if (elapsed_ms < 8200) return RiftPhase::CROSSOVER;
    if (elapsed_ms < 8800) return RiftPhase::IMPACT;
    if (elapsed_ms < 12600) return RiftPhase::HERO;
    if (elapsed_ms < RIFT_DURATION_MS) return RiftPhase::EXIT;
    return RiftPhase::IDLE;
}

ExperienceFrame ExperienceEngine::frame(uint32_t now_ms) const {
    ExperienceFrame out;
    out.identity = _config;
    out.game = _game.snapshot(now_ms);
    out.speed_mph = _vehicle.speed_mph;
    out.rpm = _vehicle.rpm;
    out.heading_deg = _vehicle.heading_deg;
    out.battery_v = _vehicle.battery_v;
    out.gear = (uint8_t)_vehicle.gear;
    out.active_wp = (uint8_t)_vehicle.active_wp;

    bool rift_live = _rift_active;
    if (rift_live && (int32_t)(now_ms - _rift_start_at) >= 0 &&
        now_ms - _rift_start_at >= RIFT_DURATION_MS)
        rift_live = false;

    if (_game.isActive()) {
        out.mode = ExperienceMode::GAME;
    } else if (rift_live) {
        out.mode = ExperienceMode::RIFT;
        out.rift.run_id = _rift_run_id;
        out.rift.seed = _rift_seed;
        out.rift.start_at_ms = _rift_start_at;
        out.rift.layout = _rift_layout;
        if ((int32_t)(now_ms - _rift_start_at) < 0) {
            out.rift.phase = RiftPhase::ARMED;
        } else {
            uint32_t elapsed = now_ms - _rift_start_at;
            out.rift.elapsed_ms = elapsed;
            out.rift.phase = riftPhase(elapsed);
            out.rift.progress_permille = elapsed >= RIFT_DURATION_MS
                ? 1000 : (uint16_t)(elapsed * 1000UL / RIFT_DURATION_MS);
        }
    }
    return out;
}

bool ExperienceEngine::consumeConfigDirty() {
    bool dirty = _config_dirty;
    _config_dirty = false;
    return dirty;
}

RiftFrame ExperienceEngine::startRift(uint32_t now_ms, uint8_t layout) {
    _game.stop();
    _rift_active = true;
    _rift_run_id++;
    _rift_seed = mixSeed(now_ms ^ (_rift_run_id * 0x9E3779B9u));
    _rift_start_at = now_ms + RIFT_LEAD_IN_MS;
    _rift_layout = layout > 2 ? 0 : layout;
    return frame(now_ms).rift;
}

void ExperienceEngine::stopRift() { _rift_active = false; }

void ExperienceEngine::enterGameAttract(uint32_t now_ms) {
    _rift_active = false;
    _game_recorded = false;
    _game.enterArcade(now_ms);
}

void ExperienceEngine::startGame(uint32_t now_ms) {
    _rift_active = false;
    _game_recorded = false;
    _game.start(now_ms);
}

void ExperienceEngine::controlGame(int steer, bool throttle, uint32_t now_ms) {
    _game.control(steer, throttle, now_ms);
}

void ExperienceEngine::stopGame() { _game.stop(); }

const char* ExperienceEngine::riftPhaseName(RiftPhase phase) {
    switch (phase) {
        case RiftPhase::ARMED:     return "armed";
        case RiftPhase::BLACKOUT:  return "blackout";
        case RiftPhase::SCAN:      return "scan";
        case RiftPhase::CHARGE:    return "charge";
        case RiftPhase::CROSSOVER: return "crossover";
        case RiftPhase::IMPACT:    return "impact";
        case RiftPhase::HERO:      return "hero";
        case RiftPhase::EXIT:      return "exit";
        default:                   return "idle";
    }
}
