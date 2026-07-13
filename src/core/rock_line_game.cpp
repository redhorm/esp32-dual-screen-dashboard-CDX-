#include "rock_line_game.h"
#include <math.h>

namespace {
float clampf(float value, float lo, float hi) {
    return value < lo ? lo : (value > hi ? hi : value);
}

int absInt(int value) { return value < 0 ? -value : value; }
}

int16_t RockLineGame::obstacleLine(uint8_t index) {
    // Hand-tuned rather than random: every transition is reachable with the
    // spring-loaded phone wheel, including at full throttle.
    static constexpr int8_t LINES[TOTAL_OBSTACLES] = {
        -48, 34, 4, 61, -28, -66, 24,
        58, -8, -55, 42, 8, -38, 64,
    };
    return LINES[index % TOTAL_OBSTACLES];
}

uint8_t RockLineGame::obstacleRadius(uint8_t index) {
    static constexpr uint8_t RADII[TOTAL_OBSTACLES] = {
        15, 14, 16, 14, 15, 14, 17,
        14, 15, 16, 14, 17, 15, 16,
    };
    return RADII[index % TOTAL_OBSTACLES];
}

RockObstacleKind RockLineGame::obstacleKind(uint8_t index) {
    static constexpr RockObstacleKind KINDS[TOTAL_OBSTACLES] = {
        RockObstacleKind::BOULDER, RockObstacleKind::LOG,
        RockObstacleKind::RUT, RockObstacleKind::BOULDER,
        RockObstacleKind::LOG, RockObstacleKind::BOULDER,
        RockObstacleKind::RUT, RockObstacleKind::LOG,
        RockObstacleKind::BOULDER, RockObstacleKind::RUT,
        RockObstacleKind::LOG, RockObstacleKind::BOULDER,
        RockObstacleKind::RUT, RockObstacleKind::BOULDER,
    };
    return KINDS[index % TOTAL_OBSTACLES];
}

bool RockLineGame::isCollision(float vehicle_x, int16_t obstacle_x,
                               uint8_t obstacle_radius) {
    return fabsf(vehicle_x - obstacle_x) <=
           obstacle_radius + VEHICLE_HALF_WIDTH;
}

void RockLineGame::_resetRun(RockGameStatus status, uint32_t now_ms) {
    _status = status;
    _last_event = RockGameEvent::NONE;
    _phase_t0 = _last_ms = _last_input_ms = now_ms;
    _result_t0 = 0;
    _elapsed_ms = 0;
    _has_result = false;
    _course_clear = false;
    _x = 0.0f;
    _steer_smoothed = 0.0f;
    _lateral_velocity = 0.0f;
    _speed = 0.0f;
    _distance = 0.0f;
    _steer = 0;
    _throttle = false;
    _score = 0;
    _combo = 0;
    _lives = 3;
    _obstacle = 0;
}

void RockLineGame::enterArcade(uint32_t now_ms) {
    _resetRun(RockGameStatus::ATTRACT, now_ms);
}

void RockLineGame::start(uint32_t now_ms) {
    _resetRun(RockGameStatus::COUNTDOWN, now_ms);
}

void RockLineGame::stop() {
    _status = RockGameStatus::IDLE;
    _steer = 0;
    _throttle = false;
    _steer_smoothed = 0.0f;
    _lateral_velocity = 0.0f;
}

void RockLineGame::pause(uint32_t now_ms) {
    if (_status != RockGameStatus::PLAYING) return;
    _status = RockGameStatus::PAUSED;
    _phase_t0 = now_ms;
    _last_ms = now_ms;
    _steer = 0;
    _throttle = false;
}

void RockLineGame::resume(uint32_t now_ms) {
    if (_status != RockGameStatus::PAUSED) return;
    _status = RockGameStatus::PLAYING;
    _phase_t0 = now_ms;
    _last_ms = now_ms;
    _last_input_ms = now_ms;
}

void RockLineGame::control(int steer, bool throttle, uint32_t now_ms) {
    if (_status == RockGameStatus::PAUSED ||
        _status == RockGameStatus::FINISHED ||
        _status == RockGameStatus::IDLE) return;
    if (steer < -100) steer = -100;
    if (steer > 100) steer = 100;
    _steer = static_cast<int8_t>(steer);
    _throttle = throttle;
    _last_input_ms = now_ms;
}

void RockLineGame::_finish(uint32_t now_ms, bool clear) {
    _status = RockGameStatus::FINISHED;
    _phase_t0 = now_ms;
    _course_clear = clear;
    _steer = 0;
    _throttle = false;
    _speed = 0.0f;
    if (clear) {
        _score += static_cast<uint32_t>(_lives) * 250u;
        if (_elapsed_ms < 26000u)
            _score += (26000u - _elapsed_ms) / 40u;
    }
}

void RockLineGame::_judgeObstacle(uint32_t now_ms) {
    if (_obstacle >= TOTAL_OBSTACLES) return;

    const int16_t rock_x = obstacleLine(_obstacle);
    const uint8_t radius = obstacleRadius(_obstacle);
    const int error = absInt(static_cast<int>(_x) - rock_x);

    if (isCollision(_x, rock_x, radius)) {
        _last_event = RockGameEvent::HIT;
        _combo = 0;
        if (_lives) --_lives;
        _speed *= 0.22f;
        if (!_lives) {
            ++_obstacle;
            _result_t0 = now_ms;
            _has_result = true;
            _finish(now_ms, false);
            return;
        }
        _status = RockGameStatus::CRASHED;
        _phase_t0 = now_ms;
    } else {
        ++_combo;
        if (error <= radius + VEHICLE_HALF_WIDTH + 12) {
            _last_event = RockGameEvent::NEAR_MISS;
            _score += 180u + static_cast<uint32_t>(_combo) * 24u;
        } else {
            _last_event = RockGameEvent::DODGED;
            _score += 80u + static_cast<uint32_t>(_combo) * 14u;
        }
    }

    ++_obstacle;
    _result_t0 = now_ms;
    _has_result = true;
    if (_obstacle >= TOTAL_OBSTACLES &&
        _status == RockGameStatus::PLAYING)
        _finish(now_ms, true);
}

void RockLineGame::update(uint32_t now_ms) {
    if (_status == RockGameStatus::IDLE ||
        _status == RockGameStatus::ATTRACT ||
        _status == RockGameStatus::PAUSED ||
        _status == RockGameStatus::FINISHED) return;

    uint32_t dt_ms = now_ms - _last_ms;
    _last_ms = now_ms;
    if (dt_ms > 100u) dt_ms = 100u;

    if (_status == RockGameStatus::COUNTDOWN) {
        if (now_ms - _phase_t0 >= COUNTDOWN_MS) {
            _status = RockGameStatus::PLAYING;
            _phase_t0 = now_ms;
            _last_input_ms = now_ms;
        }
        return;
    }

    _elapsed_ms += dt_ms;

    if (_status == RockGameStatus::CRASHED) {
        if (now_ms - _phase_t0 >= CRASH_RECOVERY_MS) {
            _status = RockGameStatus::PLAYING;
            _phase_t0 = now_ms;
            _last_input_ms = now_ms;
            if (_obstacle >= TOTAL_OBSTACLES) _finish(now_ms, true);
        }
        return;
    }

    if (now_ms - _last_input_ms > INPUT_TIMEOUT_MS) {
        _steer = 0;
        _throttle = false;
    }

    const float dt = dt_ms * 0.001f;
    const float target_speed = _throttle ? 232.0f : 94.0f;
    const float speed_response = _throttle ? 3.4f : 2.2f;
    _speed += (target_speed - _speed) *
              clampf(speed_response * dt, 0.0f, 1.0f);

    const float steer_target = _steer / 100.0f;
    _steer_smoothed += (steer_target - _steer_smoothed) *
                       clampf(7.0f * dt, 0.0f, 1.0f);
    const float lateral_target =
        _steer_smoothed * (66.0f + _speed * 0.18f);
    _lateral_velocity += (lateral_target - _lateral_velocity) *
                         clampf(7.8f * dt, 0.0f, 1.0f);
    _x = clampf(_x + _lateral_velocity * dt,
                -static_cast<float>(ROAD_LIMIT),
                static_cast<float>(ROAD_LIMIT));
    if ((_x <= -ROAD_LIMIT && _lateral_velocity < 0.0f) ||
        (_x >= ROAD_LIMIT && _lateral_velocity > 0.0f))
        _lateral_velocity *= 0.25f;

    _distance += _speed * dt;
    while (_obstacle < TOTAL_OBSTACLES &&
           _distance >= (_obstacle + 1u) * OBSTACLE_SPACING &&
           _status == RockGameStatus::PLAYING)
        _judgeObstacle(now_ms);
}

void RockLineGame::_populateObstacles(RockGameSnapshot& out, float distance,
                                      uint8_t first) const {
    out.visible_obstacles = 0;
    for (uint8_t slot = 0; slot < MAX_VISIBLE_OBSTACLES; ++slot) {
        const uint8_t index = first + slot;
        if (index >= TOTAL_OBSTACLES) break;
        const float ahead = (index + 1u) * OBSTACLE_SPACING - distance;
        if (ahead < 0.0f || ahead > 1100.0f) continue;
        RockObstacleView& view = out.obstacles[out.visible_obstacles++];
        view.line_x = obstacleLine(index);
        view.distance = static_cast<uint16_t>(ahead);
        view.radius = obstacleRadius(index);
        view.kind = obstacleKind(index);
    }
    out.nearest_distance = out.visible_obstacles
        ? out.obstacles[0].distance : 0;
}

RockGameSnapshot RockLineGame::snapshot(uint32_t now_ms) const {
    RockGameSnapshot out;
    out.status = _status;
    out.last_event = _last_event;
    out.line_x = static_cast<int16_t>(_x);
    out.speed = static_cast<uint16_t>(clampf(_speed, 0.0f, 999.0f));
    out.score = _score;
    out.combo = _combo;
    out.lives = _lives;
    out.obstacle = _obstacle;
    out.total_obstacles = TOTAL_OBSTACLES;
    out.phase_ms = now_ms - _phase_t0;
    out.steering = _steer;
    out.throttle = _throttle ? 1 : 0;
    out.world_distance = static_cast<uint16_t>(clampf(_distance, 0.0f,
                                                       COURSE_DISTANCE));
    out.result_age_ms = _has_result
        ? static_cast<uint16_t>((now_ms - _result_t0) > 0xFFFEu
              ? 0xFFFEu : now_ms - _result_t0)
        : 0xFFFFu;
    out.elapsed_ms = _elapsed_ms;
    out.course_clear = _course_clear;

    float render_distance = _distance;
    uint8_t first = _obstacle;
    if (_status == RockGameStatus::ATTRACT) {
        const uint32_t demo = (now_ms - _phase_t0) * 11u / 100u;
        render_distance = static_cast<float>(demo %
            static_cast<uint32_t>(COURSE_DISTANCE));
        first = static_cast<uint8_t>(render_distance / OBSTACLE_SPACING);
        const int wave = static_cast<int>((demo / 5u) % 176u);
        out.line_x = static_cast<int16_t>(wave > 88 ? 176 - wave : wave);
        out.line_x -= 44;
        out.world_distance = static_cast<uint16_t>(render_distance);
    }
    _populateObstacles(out, render_distance, first);
    out.progress_permille = static_cast<uint16_t>(clampf(
        render_distance * 1000.0f / COURSE_DISTANCE, 0.0f, 1000.0f));
    return out;
}
