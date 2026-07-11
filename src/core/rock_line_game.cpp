#include "rock_line_game.h"
#include <math.h>

namespace {
float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
}

int16_t RockLineGame::safeLine(uint8_t gate) {
    // Stable pseudo-random sequence, wide enough to demand steering but never
    // close enough to an edge to become unfair on a phone touch controller.
    uint32_t x = 0x9E3779B9u ^ ((uint32_t)gate + 1u) * 0x85EBCA6Bu;
    x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15;
    return (int16_t)((int)(x % 131u) - 65);
}

void RockLineGame::start(uint32_t now_ms) {
    _status = RockGameStatus::COUNTDOWN;
    _last_result = GateResult::NONE;
    _started_ms = _phase_t0 = _last_ms = _last_input_ms = now_ms;
    _x = _speed = _distance = 0.0f;
    _next_gate = GATE_SPACING;
    _steer = 0; _throttle = false;
    _score = 0; _combo = 0; _lives = 3; _gate = 0;
}

void RockLineGame::stop() {
    _status = RockGameStatus::IDLE;
    _steer = 0; _throttle = false;
}

void RockLineGame::control(int steer, bool throttle, uint32_t now_ms) {
    if (steer < -100) steer = -100;
    if (steer > 100) steer = 100;
    _steer = (int8_t)steer;
    _throttle = throttle;
    _last_input_ms = now_ms;
}

void RockLineGame::_judgeGate(uint32_t now_ms) {
    int err = (int)fabsf(_x - safeLine(_gate));
    if (err <= 32) {
        _combo++;
        if (err <= 10) {
            _last_result = GateResult::PERFECT;
            _score += 200 + (uint32_t)_combo * 35;
        } else {
            _last_result = GateResult::CLEAN;
            _score += 100 + (uint32_t)_combo * 25 + (uint32_t)(32 - err) * 2;
        }
    } else {
        _last_result = GateResult::HIT;
        _combo = 0;
        if (_lives) _lives--;
        _status = _lives ? RockGameStatus::CRASHED : RockGameStatus::FINISHED;
        _phase_t0 = now_ms;
        _speed *= 0.30f;
    }
    _gate++;
    _next_gate += GATE_SPACING;
    if (_gate >= TOTAL_GATES) {
        _status = RockGameStatus::FINISHED;
        _phase_t0 = now_ms;
        _score += (uint32_t)_lives * 300u;
    }
}

void RockLineGame::update(uint32_t now_ms) {
    if (_status == RockGameStatus::IDLE || _status == RockGameStatus::FINISHED) return;

    uint32_t dt_ms = now_ms - _last_ms;
    _last_ms = now_ms;
    if (dt_ms > 100) dt_ms = 100;

    if (_status == RockGameStatus::COUNTDOWN) {
        if (now_ms - _phase_t0 >= 2200) {
            _status = RockGameStatus::PLAYING;
            _phase_t0 = now_ms;
        }
        return;
    }
    if (_status == RockGameStatus::CRASHED) {
        if (now_ms - _phase_t0 >= 700) {
            _status = RockGameStatus::PLAYING;
            _phase_t0 = now_ms;
            _last_result = GateResult::NONE;
        }
        return;
    }

    if (now_ms - _last_input_ms > 650) {
        _steer = 0;
        _throttle = false;
    }

    float dt = dt_ms * 0.001f;
    float target = _throttle ? 230.0f : 95.0f;
    float response = _throttle ? 3.2f : 2.0f;
    _speed += (target - _speed) * clampf(response * dt, 0.0f, 1.0f);
    _x = clampf(_x + _steer * 1.25f * dt, -90.0f, 90.0f);
    _distance += _speed * dt;

    if (_distance >= _next_gate && _gate < TOTAL_GATES)
        _judgeGate(now_ms);
}

RockGameSnapshot RockLineGame::snapshot(uint32_t now_ms) const {
    RockGameSnapshot s;
    s.status = _status;
    s.last_result = _last_result;
    s.line_x = (int16_t)_x;
    s.safe_x = safeLine(_gate < TOTAL_GATES ? _gate : TOTAL_GATES - 1);
    s.speed = (uint16_t)_speed;
    float remain = _next_gate - _distance;
    s.distance_to_gate = (uint16_t)(remain > 0 ? remain : 0);
    float total = TOTAL_GATES * GATE_SPACING;
    s.progress_permille = (uint16_t)clampf(_distance / total * 1000.0f, 0.0f, 1000.0f);
    s.score = _score;
    s.combo = _combo;
    s.lives = _lives;
    s.gate = _gate;
    s.total_gates = TOTAL_GATES;
    s.phase_ms = now_ms - _phase_t0;
    return s;
}
