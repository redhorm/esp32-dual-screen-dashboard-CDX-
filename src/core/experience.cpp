#include "experience.h"

void ExperienceEngine::reset(uint32_t now_ms) {
    (void)now_ms;
    _game.stop();
    _game_recorded = false;
    _config_dirty = false;
    _ghost_dirty = false;
    _known_best_ghost_crc = _game.bestGhost().crc32;
    _legacy_sequence = 0;
    _legacy_session = 1u;
}

void ExperienceEngine::update(uint32_t now_ms, const VehicleState& state,
                              const UiFx&) {
    _vehicle = state;
    _game.update(now_ms);

    if (_game.isFinished() && !_game_recorded) {
        const TrailGameSnapshot result = _game.snapshot(now_ms);
        if (result.score > _config.best_game_score) {
            _config.best_game_score = result.score;
            _config_dirty = true;
        }
        if (result.course_clear) {
            ++_config.game_wins;
            _config_dirty = true;
        }
        if (_game.bestGhost().crc32 != 0u &&
            _game.bestGhost().crc32 != _known_best_ghost_crc) {
            _known_best_ghost_crc = _game.bestGhost().crc32;
            _ghost_dirty = true;
        }
        _game_recorded = true;
    }
}

ExperienceFrame ExperienceEngine::frame(uint32_t now_ms,
                                        bool include_game) const {
    ExperienceFrame out;
    out.identity = _config;
    const bool game_active = _game.isActive();
    // Building the full Trail snapshot calculates roadbook/path objects and
    // may scan the ghost recording. Do not do that on every dashboard frame
    // while the product is in normal dual-dashboard mode.
    if (include_game || game_active)
        out.game = _game.snapshot(now_ms);
    out.speed_mph = _vehicle.speed_mph;
    out.rpm = _vehicle.rpm;
    out.heading_deg = _vehicle.heading_deg;
    out.battery_v = _vehicle.battery_v;
    out.gear = static_cast<uint8_t>(_vehicle.gear);
    out.active_wp = static_cast<uint8_t>(_vehicle.active_wp);
    if (game_active) out.mode = ExperienceMode::GAME;
    return out;
}

bool ExperienceEngine::consumeConfigDirty() {
    const bool dirty = _config_dirty;
    _config_dirty = false;
    return dirty;
}

void ExperienceEngine::configureGame(const TrailGameConfig& config) {
    _game.configure(config);
    _game_recorded = false;
}

void ExperienceEngine::enterGameAttract(uint32_t now_ms) {
    _game_recorded = false;
    _game.enterAttract(now_ms);
}

void ExperienceEngine::startGame(uint32_t now_ms, uint32_t session_id) {
    _game_recorded = false;
    _legacy_session = session_id ? session_id : 1u;
    _legacy_sequence = 0;
    _game.start(now_ms, _legacy_session);
}

void ExperienceEngine::pauseGame(uint32_t now_ms) { _game.pause(now_ms); }

void ExperienceEngine::resumeGame(uint32_t now_ms) { _game.resume(now_ms); }

TrailInputResult ExperienceEngine::controlGame(const TrailInput& input) {
    return _game.submitInput(input);
}

TrailInputResult ExperienceEngine::controlGame(
    int steer, int throttle, int brake, uint32_t session_id,
    uint16_t sequence, uint32_t client_ms, uint32_t received_ms) {
    TrailInput input;
    input.session_id = session_id;
    input.sequence = sequence;
    input.client_ms = client_ms;
    input.received_ms = received_ms;
    if (steer < -100) steer = -100;
    if (steer > 100) steer = 100;
    if (throttle < 0) throttle = 0;
    if (throttle > 100) throttle = 100;
    if (brake < 0) brake = 0;
    if (brake > 100) brake = 100;
    input.steering = static_cast<int8_t>(steer);
    input.throttle = static_cast<uint8_t>(throttle);
    input.brake = static_cast<uint8_t>(brake);
    return _game.submitInput(input);
}

void ExperienceEngine::controlGame(int steer, bool throttle,
                                   uint32_t now_ms) {
    ++_legacy_sequence;
    if (_legacy_sequence == 0u) ++_legacy_sequence;
    (void)controlGame(steer, throttle ? 100 : 0, 0, _legacy_session,
                      _legacy_sequence, now_ms, now_ms);
}

TrailInputResult ExperienceEngine::neutralGame(uint32_t session_id,
                                               uint16_t sequence,
                                               uint32_t now_ms) {
    return _game.neutralize(session_id, sequence, now_ms);
}

void ExperienceEngine::stopGame() { _game.stop(); }

uint8_t ExperienceEngine::readAudioEvents(uint16_t after, AudioEvent* output,
                                          uint8_t capacity) const {
    return _game.audioEvents().readAfter(after, output, capacity);
}

bool ExperienceEngine::loadBestGhost(
    const TrailGhostRecording& recording) {
    if (!_game.loadBestGhost(recording)) return false;
    _known_best_ghost_crc = recording.crc32;
    _ghost_dirty = false;
    return true;
}

void ExperienceEngine::clearBestGhost() {
    _game.clearBestGhost();
    _known_best_ghost_crc = 0;
    _ghost_dirty = true;
}

bool ExperienceEngine::consumeGhostDirty() {
    const bool dirty = _ghost_dirty;
    _ghost_dirty = false;
    return dirty;
}
