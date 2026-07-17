#include "trail_assault_game.h"

#include <string.h>

namespace {

template <typename T>
T clampValue(T value, T low, T high) {
    return value < low ? low : (value > high ? high : value);
}

int32_t abs32(int32_t value) { return value < 0 ? -value : value; }

uint32_t nextRandom(uint32_t& state) {
    if (state == 0u) state = 0xA341316Cu;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

bool sequenceNewer(uint16_t candidate, uint16_t reference) {
    const uint16_t delta = static_cast<uint16_t>(candidate - reference);
    return delta != 0u && delta < 0x8000u;
}

uint32_t fnvByte(uint32_t hash, uint8_t byte) {
    return (hash ^ byte) * 16777619u;
}

uint32_t fnvU16(uint32_t hash, uint16_t value) {
    hash = fnvByte(hash, static_cast<uint8_t>(value));
    return fnvByte(hash, static_cast<uint8_t>(value >> 8));
}

uint32_t fnvU32(uint32_t hash, uint32_t value) {
    hash = fnvU16(hash, static_cast<uint16_t>(value));
    return fnvU16(hash, static_cast<uint16_t>(value >> 16));
}

uint32_t crcByte(uint32_t crc, uint8_t byte) {
    crc ^= byte;
    for (uint8_t bit = 0; bit < 8; ++bit)
        crc = (crc >> 1) ^ (0xEDB88320u &
            static_cast<uint32_t>(-static_cast<int32_t>(crc & 1u)));
    return crc;
}

uint32_t crcU16(uint32_t crc, uint16_t value) {
    crc = crcByte(crc, static_cast<uint8_t>(value));
    return crcByte(crc, static_cast<uint8_t>(value >> 8));
}

uint32_t crcU32(uint32_t crc, uint32_t value) {
    crc = crcU16(crc, static_cast<uint16_t>(value));
    return crcU16(crc, static_cast<uint16_t>(value >> 16));
}

struct TerrainPhysics {
    uint16_t max_speed_cmps;
    uint16_t acceleration_cmps2;
    uint16_t rolling_drag_cmps2;
    uint16_t grip_permille;
};

constexpr TerrainPhysics TERRAIN_PHYSICS[] = {
    {5000, 1120, 130, 900},  // dirt
    {4200,  870, 225, 770},  // rock
    {3350,  620, 355, 545},  // mud
    {3850,  710, 300, 635},  // sand
    {2850,  520, 430, 700},  // water
    {5200, 1180, 105, 920},  // jump approach
};

const TerrainPhysics& physicsFor(TrailTerrain terrain) {
    uint8_t index = static_cast<uint8_t>(terrain);
    if (index >= sizeof(TERRAIN_PHYSICS) / sizeof(TERRAIN_PHYSICS[0]))
        index = 0;
    return TERRAIN_PHYSICS[index];
}

uint8_t obstacleRadius(TrailObstacleKind kind) {
    switch (kind) {
        case TrailObstacleKind::BOULDER: return 20;
        case TrailObstacleKind::LOG:     return 17;
        case TrailObstacleKind::RUT:     return 15;
        case TrailObstacleKind::GATE:    return 48;
        case TrailObstacleKind::JUMP_RAMP: return 30;
        default: return 0;
    }
}

}  // namespace

TrailAssaultGame::TrailAssaultGame() { configure(TrailGameConfig{}); }

void TrailAssaultGame::configure(const TrailGameConfig& config) {
    _config = config;
    if (static_cast<uint8_t>(_config.mode) >
        static_cast<uint8_t>(TrailMode::HILL_CLIMB))
        _config.mode = TrailMode::TIME_ATTACK;
    if (_config.seed == 0u) _config.seed = 0x4400C0DEu;
    _generateTrack();
    stop();
}

void TrailAssaultGame::_generateTrack() {
    uint32_t random = _config.seed ^
        (0x9E3779B9u * (static_cast<uint8_t>(_config.mode) + 1u)) ^
        (static_cast<uint32_t>(GENERATOR_VERSION) << 16);
    _segment_count = static_cast<uint8_t>(64u + nextRandom(random) % 17u);
    _course_length_cm = 0;
    _total_checkpoints = 0;
    _total_gates = 0;

    int curve = 0;
    int grade = 0;
    TrailTerrain terrain = TrailTerrain::DIRT;
    uint8_t terrain_run = 0;

    for (uint8_t i = 0; i < _segment_count; ++i) {
        TrailSegment& segment = _segments[i];
        segment = TrailSegment{};
        segment.length_m = static_cast<uint16_t>(10u + nextRandom(random) % 11u);

        if (i < 3u) {
            curve = 0;
            grade = _config.mode == TrailMode::HILL_CLIMB ? 1 : 0;
            terrain = TrailTerrain::DIRT;
            terrain_run = 3;
        } else {
            curve = clampValue(curve +
                static_cast<int>(nextRandom(random) % 5u) - 2, -6, 6);
            if ((nextRandom(random) & 7u) == 0u) curve /= 2;

            if (_config.mode == TrailMode::HILL_CLIMB) {
                grade = clampValue(grade +
                    static_cast<int>(nextRandom(random) % 3u) - 1, 1, 7);
                if ((nextRandom(random) & 15u) == 0u) grade = 0;
            } else {
                grade = clampValue(grade +
                    static_cast<int>(nextRandom(random) % 5u) - 2, -5, 5);
            }

            if (terrain_run == 0u) {
                terrain = static_cast<TrailTerrain>(nextRandom(random) % 6u);
                terrain_run = static_cast<uint8_t>(2u + nextRandom(random) % 4u);
            } else {
                --terrain_run;
            }
        }

        // Every generated trail contains all supported surfaces.  Their
        // positions still vary because segment lengths, bends and obstacles
        // are seeded.
        for (uint8_t forced = 0; forced < 6u; ++forced) {
            if (i == static_cast<uint8_t>(8u + forced * 8u)) {
                terrain = static_cast<TrailTerrain>(forced);
                terrain_run = 1;
            }
        }

        segment.curve = static_cast<int8_t>(curve);
        segment.grade = static_cast<int8_t>(grade);
        segment.width = static_cast<uint8_t>(74u + nextRandom(random) % 27u);
        segment.terrain = terrain;
        segment.obstacle_lateral = static_cast<int8_t>(
            static_cast<int>(nextRandom(random) % 131u) - 65);

        const bool last = i + 1u == _segment_count;
        if ((i > 0u && ((i + 1u) % 16u == 0u)) || last) {
            segment.flags |= TRAIL_SEGMENT_CHECKPOINT;
            ++_total_checkpoints;
        }
        if (_config.mode == TrailMode::GATE_RUSH && i >= 4u &&
            (i % 5u == 4u) && !last) {
            segment.flags |= TRAIL_SEGMENT_GATE;
            segment.obstacle = TrailObstacleKind::GATE;
            segment.obstacle_lateral = 0;
            ++_total_gates;
        } else if (terrain == TrailTerrain::JUMP) {
            segment.flags |= TRAIL_SEGMENT_JUMP;
            segment.obstacle = TrailObstacleKind::JUMP_RAMP;
            segment.obstacle_lateral = 0;
        } else if (i > 2u && (nextRandom(random) % 3u) == 0u) {
            segment.obstacle = static_cast<TrailObstacleKind>(
                1u + nextRandom(random) % 3u);
        }
        _course_length_cm += static_cast<uint32_t>(segment.length_m) * 100u;
    }

    // Stable fingerprint used by tests, diagnostics and future persistence.
    uint32_t fingerprint = 2166136261u;
    fingerprint = fnvU32(fingerprint, _config.seed);
    fingerprint = fnvByte(fingerprint, static_cast<uint8_t>(_config.mode));
    fingerprint = fnvU16(fingerprint, GENERATOR_VERSION);
    fingerprint = fnvByte(fingerprint, _segment_count);
    for (uint8_t i = 0; i < _segment_count; ++i) {
        const TrailSegment& s = _segments[i];
        fingerprint = fnvU16(fingerprint, s.length_m);
        fingerprint = fnvByte(fingerprint, static_cast<uint8_t>(s.curve));
        fingerprint = fnvByte(fingerprint, static_cast<uint8_t>(s.grade));
        fingerprint = fnvByte(fingerprint,
                              static_cast<uint8_t>(s.obstacle_lateral));
        fingerprint = fnvByte(fingerprint, s.width);
        fingerprint = fnvByte(fingerprint, static_cast<uint8_t>(s.terrain));
        fingerprint = fnvByte(fingerprint, static_cast<uint8_t>(s.obstacle));
        fingerprint = fnvByte(fingerprint, s.flags);
    }
    _track_fingerprint = fingerprint;
}

const TrailSegment& TrailAssaultGame::segment(uint8_t index) const {
    if (_segment_count == 0u) return _segments[0];
    if (index >= _segment_count) index = static_cast<uint8_t>(_segment_count - 1u);
    return _segments[index];
}

void TrailAssaultGame::_resetRun(TrailStatus status, uint32_t now_ms,
                                 uint32_t session_id) {
    _status = status;
    _terrain = _segments[0].terrain;
    _last_event = TrailEvent::NONE;
    _event_sequence = 0;
    _last_event_tick = 0;
    _last_update_ms = now_ms;
    _sim_now_ms = now_ms;
    _step_ms_remainder = 0;
    _accumulator_units = 0;
    _tick = 0;
    _phase_ticks = 0;
    _elapsed_ticks = 0;
    _remaining_ticks = _config.mode == TrailMode::GATE_RUSH
        ? 18u * FIXED_HZ : 0u;
    _distance_q8 = 0;
    _speed_q8 = 0;
    _lateral = 0;
    _drift = 0;
    _jump_height_q8 = 0;
    _jump_velocity_q8 = 0;
    _temperature_c10 = 650;
    _damage_permille = 0;
    _grip_permille = 900;
    _segment_index = 0;
    _segment_start_cm = 0;
    _checkpoint = 0;
    _gate = 0;
    _score = 0;
    _course_clear = false;
    _obstacle_judged = false;
    _jump_triggered = false;
    _input_seen = false;
    _controller_timed_out = true;
    _input = TrailInput{};
    _input.session_id = session_id ? session_id : 1u;
    _input.received_ms = now_ms;
    _last_gear = 0;
    _last_skid_tick = 0;
    _temperature_warning = false;
    _ghost_sample_stride_ticks = 6u;
    _current_ghost = TrailGhostRecording{};
    _current_ghost.seed = _config.seed;
    _current_ghost.mode = _config.mode;
    _current_ghost.generator_version = GENERATOR_VERSION;
    _audio.clear();
}

void TrailAssaultGame::enterAttract(uint32_t now_ms) {
    _resetRun(TrailStatus::ATTRACT, now_ms, 1u);
}

void TrailAssaultGame::start(uint32_t now_ms, uint32_t session_id) {
    _resetRun(TrailStatus::COUNTDOWN, now_ms, session_id);
    _audio.publish(AudioEventType::COUNTDOWN, 255, 3, _tick);
}

void TrailAssaultGame::stop() {
    _status = TrailStatus::IDLE;
    _input.steering = 0;
    _input.throttle = 0;
    _input.brake = 0;
    _controller_timed_out = true;
    _speed_q8 = 0;
}

void TrailAssaultGame::pause(uint32_t now_ms) {
    if (_status != TrailStatus::RUNNING) return;
    _status = TrailStatus::PAUSED;
    _phase_ticks = 0;
    _last_update_ms = now_ms;
    _accumulator_units = 0;
    _input.steering = 0;
    _input.throttle = 0;
    _input.brake = 0;
    _controller_timed_out = true;
    _audio.publish(AudioEventType::PAUSE, 255, 1, _tick);
}

void TrailAssaultGame::resume(uint32_t now_ms) {
    if (_status != TrailStatus::PAUSED) return;
    _status = TrailStatus::RUNNING;
    _phase_ticks = 0;
    _last_update_ms = now_ms;
    _sim_now_ms = now_ms;
    _step_ms_remainder = 0;
    _accumulator_units = 0;
    _controller_timed_out = true;
    _audio.publish(AudioEventType::PAUSE, 180, 0, _tick);
}

TrailInputResult TrailAssaultGame::submitInput(const TrailInput& source) {
    if (_status != TrailStatus::COUNTDOWN &&
        _status != TrailStatus::RUNNING)
        return TrailInputResult::INACTIVE;
    if (source.session_id != _input.session_id)
        return TrailInputResult::WRONG_SESSION;
    if (_input_seen && !sequenceNewer(source.sequence, _input.sequence))
        return TrailInputResult::STALE_SEQUENCE;

    _input = source;
    _input.steering = static_cast<int8_t>(clampValue<int>(
        _input.steering, -100, 100));
    _input.throttle = static_cast<uint8_t>(clampValue<int>(
        _input.throttle, 0, 100));
    _input.brake = static_cast<uint8_t>(clampValue<int>(
        _input.brake, 0, 100));
    _input_seen = true;
    _controller_timed_out = false;
    return TrailInputResult::ACCEPTED;
}

TrailInputResult TrailAssaultGame::neutralize(uint32_t session_id,
                                              uint16_t sequence,
                                              uint32_t received_ms) {
    TrailInput neutral;
    neutral.session_id = session_id;
    neutral.sequence = sequence;
    neutral.client_ms = received_ms;
    neutral.received_ms = received_ms;
    TrailInputResult result = submitInput(neutral);
    if (result == TrailInputResult::ACCEPTED) _controller_timed_out = false;
    return result;
}

void TrailAssaultGame::update(uint32_t now_ms) {
    if (_status == TrailStatus::IDLE || _status == TrailStatus::ATTRACT ||
        _status == TrailStatus::PAUSED || _status == TrailStatus::FINISHED ||
        _status == TrailStatus::FAILED) {
        _last_update_ms = now_ms;
        return;
    }

    uint32_t delta_ms = now_ms - _last_update_ms;
    _last_update_ms = now_ms;
    // Countdown may legitimately be advanced in one call during startup, but
    // a running race must never replay seconds of physics after a blocked
    // network/filesystem task.  Resynchronise wall time and simulate at most
    // a bounded 500 ms slice.
    if (_status == TrailStatus::RUNNING && delta_ms > MAX_CATCH_UP_MS) {
        delta_ms = MAX_CATCH_UP_MS;
        _sim_now_ms = now_ms - delta_ms;
        _accumulator_units = 0;
    }
    uint64_t units = static_cast<uint64_t>(_accumulator_units) +
                     static_cast<uint64_t>(delta_ms) * FIXED_HZ;
    uint16_t steps = 0;
    const bool entered_as_countdown = _status == TrailStatus::COUNTDOWN;
    const uint16_t max_steps = _status == TrailStatus::RUNNING
        ? static_cast<uint16_t>(MAX_CATCH_UP_MS * FIXED_HZ / 1000u)
        : static_cast<uint16_t>(COUNTDOWN_TICKS - _phase_ticks);
    while (units >= 1000u && steps < max_steps) {
        _step();
        ++steps;
        units -= 1000u;
        if (_status == TrailStatus::FINISHED ||
            _status == TrailStatus::FAILED) {
            units = 0u;
            break;
        }
    }
    if (units >= 1000u) {
        // A long captive-portal or filesystem stall can consume the complete
        // countdown in one call.  Never replay the surplus as race physics,
        // and keep the simulation clock aligned with wall-time input stamps.
        if (entered_as_countdown && _status == TrailStatus::RUNNING) {
            _sim_now_ms = now_ms;
            _step_ms_remainder = 0;
        }
        units %= 1000u;
    }
    _accumulator_units = static_cast<uint32_t>(units);
}

void TrailAssaultGame::_step() {
    ++_tick;
    ++_phase_ticks;
    _step_ms_remainder += 1000u;
    _sim_now_ms += _step_ms_remainder / FIXED_HZ;
    _step_ms_remainder %= FIXED_HZ;

    if (_status == TrailStatus::COUNTDOWN) {
        const int32_t input_age = static_cast<int32_t>(
            _sim_now_ms - _input.received_ms);
        if (!_input_seen ||
            (input_age >= 0 && static_cast<uint32_t>(input_age) >
             INPUT_TIMEOUT_MS)) {
            _controller_timed_out = true;
            _input.steering = 0;
            _input.throttle = 0;
            _input.brake = 0;
        }
        if (_phase_ticks == FIXED_HZ)
            _audio.publish(AudioEventType::COUNTDOWN, 255, 2, _tick);
        else if (_phase_ticks == FIXED_HZ * 2u)
            _audio.publish(AudioEventType::COUNTDOWN, 255, 1, _tick);
        else if (_phase_ticks >= COUNTDOWN_TICKS) {
            _status = TrailStatus::RUNNING;
            _phase_ticks = 0;
            _audio.publish(AudioEventType::COUNTDOWN, 255, 0, _tick);
            _audio.publish(AudioEventType::ENGINE_START, 230, 0, _tick);
        }
        return;
    }

    if (_status == TrailStatus::RUNNING) _stepRunning();
}

void TrailAssaultGame::_stepRunning() {
    ++_elapsed_ticks;
    const int32_t signed_age = static_cast<int32_t>(
        _sim_now_ms - _input.received_ms);
    const bool stale = !_input_seen ||
        (signed_age >= 0 && static_cast<uint32_t>(signed_age) >
         INPUT_TIMEOUT_MS);
    if (stale) {
        if (!_controller_timed_out && _input_seen) {
            _controller_timed_out = true;
            _setEvent(TrailEvent::CONTROLLER_TIMEOUT);
            _audio.publish(AudioEventType::WARNING, 220, 1, _tick);
        }
        _input.steering = 0;
        _input.throttle = 0;
        _input.brake = 0;
    }

    const TrailSegment& current = _segments[_segment_index];
    const TerrainPhysics& physics = physicsFor(current.terrain);
    _grip_permille = static_cast<uint16_t>(clampValue<int>(
        physics.grip_permille - _damage_permille / 5, 300, 1000));

    int32_t acceleration = static_cast<int32_t>(physics.acceleration_cmps2) *
                           _input.throttle / 100;
    acceleration -= static_cast<int32_t>(_input.brake) * 28;
    acceleration -= physics.rolling_drag_cmps2;
    acceleration -= static_cast<int32_t>(current.grade) * 38;
    _speed_q8 += acceleration * 256 / FIXED_HZ;
    if (_speed_q8 < 0) _speed_q8 = 0;
    const int32_t max_speed_q8 =
        static_cast<int32_t>(physics.max_speed_cmps) * 256;
    if (_speed_q8 > max_speed_q8) {
        _speed_q8 -= (_speed_q8 - max_speed_q8) / 10 + 1;
        if (_speed_q8 > max_speed_q8 + 300 * 256)
            _speed_q8 = max_speed_q8 + 300 * 256;
    }

    const int32_t speed_cmps = _speed_q8 / 256;
    const int32_t steer_force = static_cast<int32_t>(_input.steering) *
        (250 + speed_cmps / 7) * _grip_permille / 1000 / 100;
    const int32_t curve_force = static_cast<int32_t>(current.curve) *
                                (45 + speed_cmps / 35);
    const int32_t drift_target =
        static_cast<int32_t>(_input.steering) * speed_cmps *
        (1100 - _grip_permille) / 100000;
    _drift += (drift_target - _drift) / 7;
    _lateral += (steer_force + _drift - curve_force) / FIXED_HZ;
    if (_lateral < -1000 || _lateral > 1000) {
        const int32_t overflow = abs32(_lateral) - 1000;
        _lateral = clampValue<int32_t>(_lateral, -1000, 1000);
        _speed_q8 = _speed_q8 * 97 / 100;
        if (overflow > 20 && (_tick % 12u) == 0u)
            _damage_permille = static_cast<uint16_t>(clampValue<int>(
                _damage_permille + 2, 0, 1000));
    }

    if (_jump_height_q8 > 0 || _jump_velocity_q8 != 0) {
        _jump_height_q8 += _jump_velocity_q8 / FIXED_HZ;
        _jump_velocity_q8 -= 980 * 256 / FIXED_HZ;
        if (_jump_height_q8 <= 0 && _jump_velocity_q8 < 0) {
            const bool hard = _jump_velocity_q8 < -650 * 256;
            _jump_height_q8 = 0;
            _jump_velocity_q8 = 0;
            _setEvent(TrailEvent::LANDING);
            if (hard) {
                _damage_permille = static_cast<uint16_t>(clampValue<int>(
                    _damage_permille + 35, 0, 1000));
                _audio.publish(AudioEventType::COLLISION_SMALL, 150,
                               static_cast<int16_t>(speed_cmps), _tick);
            }
        }
    }

    const uint32_t previous_distance_cm = _distance_q8 >> 8;
    _distance_q8 += static_cast<uint32_t>(_speed_q8 / FIXED_HZ);
    uint32_t distance_cm = _distance_q8 >> 8;
    if (distance_cm > _course_length_cm) {
        distance_cm = _course_length_cm;
        _distance_q8 = distance_cm << 8;
    }
    _advanceSegment(previous_distance_cm, distance_cm);

    if (_input.throttle >= 70u && (_tick % 6u) == 0u)
        ++_temperature_c10;
    else if (_input.throttle < 25u && _temperature_c10 > 600u &&
             (_tick % 60u) == 0u)
        --_temperature_c10;
    if (current.terrain == TrailTerrain::WATER &&
        _temperature_c10 > 550u && (_tick % 3u) == 0u)
        --_temperature_c10;
    if (_temperature_c10 >= 1100u && !_temperature_warning) {
        _temperature_warning = true;
        _setEvent(TrailEvent::OVERHEAT);
        _audio.publish(AudioEventType::WARNING, 255,
                       static_cast<int16_t>(_temperature_c10), _tick);
    } else if (_temperature_c10 < 1040u) {
        _temperature_warning = false;
    }

    if (abs32(_drift) > 85 && _tick - _last_skid_tick >= 15u) {
        _last_skid_tick = _tick;
        _audio.publish(AudioEventType::SKID,
                       static_cast<uint8_t>(clampValue<int32_t>(
                           abs32(_drift), 0, 255)),
                       static_cast<int16_t>(_drift), _tick);
    }
    if ((_tick % 6u) == 0u) {
        const uint16_t kph10 = static_cast<uint16_t>(
            static_cast<uint32_t>(speed_cmps) * 36u / 100u);
        const uint16_t rpm = static_cast<uint16_t>(clampValue<uint32_t>(
            850u + static_cast<uint32_t>(kph10) * 6u, 850u, 7800u));
        _audio.publish(AudioEventType::ENGINE_RPM, 180,
                       static_cast<int16_t>(rpm), _tick);
        const uint8_t gear = static_cast<uint8_t>(clampValue<int>(
            1 + kph10 / 320u, 1, 6));
        if (_last_gear != 0u && gear != _last_gear)
            _audio.publish(AudioEventType::GEAR_CHANGE, 210, gear, _tick);
        _last_gear = gear;
    }

    if ((_elapsed_ticks % _ghost_sample_stride_ticks) == 0u)
        _recordGhostSample();

    if (_config.mode == TrailMode::GATE_RUSH) {
        if (_remaining_ticks > 0u) --_remaining_ticks;
        if (_remaining_ticks == 0u && distance_cm < _course_length_cm) {
            _finish(false);
            return;
        }
    }
    if (_config.mode == TrailMode::HILL_CLIMB &&
        (_temperature_c10 >= 1250u || _damage_permille >= 1000u)) {
        _finish(false);
        return;
    }
    if (distance_cm >= _course_length_cm) {
        const bool clear = _config.mode != TrailMode::GATE_RUSH ||
                           _gate == _total_gates;
        _finish(clear);
    }
}

void TrailAssaultGame::_advanceSegment(uint32_t previous_distance_cm,
                                       uint32_t distance_cm) {
    while (_segment_index < _segment_count) {
        TrailSegment& segment = _segments[_segment_index];
        const uint32_t middle = _segment_start_cm +
            static_cast<uint32_t>(segment.length_m) * 50u;
        const uint32_t end = _segment_start_cm +
            static_cast<uint32_t>(segment.length_m) * 100u;

        if (!_obstacle_judged && previous_distance_cm < middle &&
            distance_cm >= middle) {
            if (segment.flags & TRAIL_SEGMENT_JUMP) {
                _jump_velocity_q8 = 720 * 256;
                _jump_triggered = true;
                _setEvent(TrailEvent::JUMP);
            }
            if (segment.obstacle != TrailObstacleKind::NONE &&
                segment.obstacle != TrailObstacleKind::JUMP_RAMP)
                _judgeObstacle(segment,
                               static_cast<uint32_t>(_speed_q8 / 256));
            _obstacle_judged = true;
        }

        if (distance_cm < end) break;

        if (segment.flags & TRAIL_SEGMENT_CHECKPOINT) {
            ++_checkpoint;
            _setEvent(TrailEvent::CHECKPOINT);
            _audio.publish(AudioEventType::CHECKPOINT, 240, _checkpoint,
                           _tick);
        }
        if (_segment_index + 1u >= _segment_count) break;
        _segment_start_cm = end;
        ++_segment_index;
        _obstacle_judged = false;
        _jump_triggered = false;
        TrailTerrain next = _segments[_segment_index].terrain;
        if (next != _terrain) {
            _terrain = next;
            _setEvent(TrailEvent::TERRAIN_CHANGED);
            if (next == TrailTerrain::MUD)
                _audio.publish(AudioEventType::MUD, 180, 0, _tick);
            else if (next == TrailTerrain::WATER)
                _audio.publish(AudioEventType::WATER, 190, 0, _tick);
        }
        previous_distance_cm = _segment_start_cm;
    }
}

void TrailAssaultGame::_judgeObstacle(const TrailSegment& segment,
                                      uint32_t speed_cmps) {
    if (segment.obstacle == TrailObstacleKind::GATE) {
        if (abs32(_lateral - static_cast<int32_t>(
                  segment.obstacle_lateral) * 10) <= 500) {
            ++_gate;
            _score += 500u;
            _remaining_ticks += 2u * FIXED_HZ;
            _setEvent(TrailEvent::GATE_CLEAR);
            _audio.publish(AudioEventType::CHECKPOINT, 255, _gate, _tick);
        } else {
            _damage_permille = static_cast<uint16_t>(clampValue<int>(
                _damage_permille + 80, 0, 1000));
            _setEvent(TrailEvent::GATE_MISSED);
            _audio.publish(AudioEventType::WARNING, 230, -1, _tick);
        }
        return;
    }

    if (_jump_height_q8 > 0) return;
    const int32_t object_x = static_cast<int32_t>(segment.obstacle_lateral) * 10;
    const int32_t hit_radius =
        static_cast<int32_t>(obstacleRadius(segment.obstacle)) * 10 + 130;
    if (abs32(_lateral - object_x) > hit_radius) {
        _score += 80u;
        return;
    }

    const bool large = speed_cmps >= 3000u ||
                       segment.obstacle == TrailObstacleKind::BOULDER;
    _damage_permille = static_cast<uint16_t>(clampValue<int>(
        _damage_permille + (large ? 145 : 70), 0, 1000));
    _speed_q8 = _speed_q8 * (large ? 46 : 68) / 100;
    _setEvent(large ? TrailEvent::COLLISION_LARGE
                    : TrailEvent::COLLISION_SMALL);
    _audio.publish(large ? AudioEventType::COLLISION_LARGE
                         : AudioEventType::COLLISION_SMALL,
                   large ? 255 : 180, static_cast<int16_t>(speed_cmps),
                   _tick);
}

void TrailAssaultGame::_setEvent(TrailEvent event) {
    _last_event = event;
    _last_event_tick = _tick;
    ++_event_sequence;
    if (_event_sequence == 0u) ++_event_sequence;
}

void TrailAssaultGame::_finish(bool clear) {
    _course_clear = clear;
    _status = clear ? TrailStatus::FINISHED : TrailStatus::FAILED;
    _phase_ticks = 0;
    _speed_q8 = 0;
    _input.steering = 0;
    _input.throttle = 0;
    _input.brake = 0;
    _input_seen = false;
    _controller_timed_out = true;
    _setEvent(TrailEvent::FINISH);
    const uint32_t elapsed_ms = _elapsed_ticks * 1000u / FIXED_HZ;
    if (clear) {
        const uint32_t time_bonus = elapsed_ms < 90000u
            ? (90000u - elapsed_ms) / 20u : 0u;
        _score += 1000u + time_bonus +
                  static_cast<uint32_t>(1000u - _damage_permille) * 2u;
        if (_config.mode == TrailMode::GATE_RUSH)
            _score += static_cast<uint32_t>(_gate) * 400u;
        _finalizeGhost();
    }
    _audio.publish(AudioEventType::FINISH, 255, clear ? 1 : 0, _tick);
}

void TrailAssaultGame::_recordGhostSample() {
    if (_current_ghost.sample_count >= TrailGhostRecording::MAX_SAMPLES) {
        // Keep the entire run rather than silently truncating after 30 s.
        // Time and distance are stored in every sample, so halving the history
        // remains deterministic and the replay does not depend on a fixed
        // array index cadence.
        uint16_t write = 0;
        for (uint16_t read = 0;
             read < _current_ghost.sample_count; read += 2u)
            _current_ghost.samples[write++] = _current_ghost.samples[read];
        _current_ghost.sample_count = write;
        if (_ghost_sample_stride_ticks < 1536u)
            _ghost_sample_stride_ticks *= 2u;
    }
    TrailGhostSample& sample =
        _current_ghost.samples[_current_ghost.sample_count++];
    sample.time_ds = static_cast<uint16_t>(clampValue<uint32_t>(
        _elapsed_ticks / 6u, 0u, 0xFFFFu));
    sample.distance_dm = static_cast<uint16_t>(clampValue<uint32_t>(
        (_distance_q8 >> 8) / 10u, 0u, 0xFFFFu));
    sample.lateral = static_cast<int8_t>(clampValue<int32_t>(
        _lateral / 10, -100, 100));
    const uint32_t speed_cmps = static_cast<uint32_t>(_speed_q8 / 256);
    sample.speed_kph = static_cast<uint8_t>(clampValue<uint32_t>(
        speed_cmps * 36u / 1000u, 0u, 255u));
    sample.steering = _input.steering;
    sample.throttle = _input.throttle;
    sample.state = static_cast<uint8_t>((_jump_height_q8 > 0 ? 1u : 0u) |
        (_controller_timed_out ? 2u : 0u));
}

void TrailAssaultGame::_finalizeGhost() {
    const uint16_t finish_ds = static_cast<uint16_t>(clampValue<uint32_t>(
        _elapsed_ticks / 6u, 0u, 0xFFFFu));
    const uint16_t finish_dm = static_cast<uint16_t>(clampValue<uint32_t>(
        (_distance_q8 >> 8) / 10u, 0u, 0xFFFFu));
    if (_current_ghost.sample_count == 0u ||
        _current_ghost.samples[_current_ghost.sample_count - 1u].time_ds != finish_ds ||
        _current_ghost.samples[_current_ghost.sample_count - 1u].distance_dm != finish_dm)
        _recordGhostSample();
    _current_ghost.finish_ms = _elapsed_ticks * 1000u / FIXED_HZ;
    _current_ghost.crc32 = ghostCrc(_current_ghost);
    const bool replace = !ghostValid(_best_ghost) ||
        _best_ghost.seed != _config.seed ||
        _best_ghost.mode != _config.mode ||
        _current_ghost.finish_ms < _best_ghost.finish_ms;
    if (replace) _best_ghost = _current_ghost;
}

uint32_t TrailAssaultGame::ghostCrc(
    const TrailGhostRecording& recording) {
    uint32_t crc = 0xFFFFFFFFu;
    crc = crcU32(crc, recording.magic);
    crc = crcU32(crc, recording.seed);
    crc = crcU32(crc, recording.finish_ms);
    crc = crcU16(crc, recording.schema_version);
    crc = crcU16(crc, recording.generator_version);
    crc = crcU16(crc, recording.sample_count);
    crc = crcByte(crc, static_cast<uint8_t>(recording.mode));
    crc = crcByte(crc, recording.sample_hz);
    const uint16_t count = recording.sample_count >
        TrailGhostRecording::MAX_SAMPLES
        ? TrailGhostRecording::MAX_SAMPLES : recording.sample_count;
    for (uint16_t i = 0; i < count; ++i) {
        const TrailGhostSample& sample = recording.samples[i];
        crc = crcU16(crc, sample.time_ds);
        crc = crcU16(crc, sample.distance_dm);
        crc = crcByte(crc, static_cast<uint8_t>(sample.lateral));
        crc = crcByte(crc, sample.speed_kph);
        crc = crcByte(crc, static_cast<uint8_t>(sample.steering));
        crc = crcByte(crc, sample.throttle);
        crc = crcByte(crc, sample.state);
        crc = crcByte(crc, sample.reserved);
    }
    return ~crc;
}

bool TrailAssaultGame::ghostValid(const TrailGhostRecording& recording) {
    return recording.magic == TrailGhostRecording::MAGIC &&
           recording.schema_version == TrailGhostRecording::SCHEMA_VERSION &&
           recording.generator_version == GENERATOR_VERSION &&
           recording.sample_hz == TrailGhostRecording::SAMPLE_HZ &&
           recording.sample_count > 0u &&
           recording.sample_count <= TrailGhostRecording::MAX_SAMPLES &&
           recording.finish_ms > 0u &&
           recording.crc32 == ghostCrc(recording);
}

bool TrailAssaultGame::loadBestGhost(
    const TrailGhostRecording& recording) {
    if (!ghostValid(recording)) return false;
    _best_ghost = recording;
    return true;
}

void TrailAssaultGame::clearBestGhost() {
    _best_ghost = TrailGhostRecording{};
}

void TrailAssaultGame::_fillObjects(TrailGameSnapshot& out,
                                    uint32_t distance_cm) const {
    out.visible_objects = 0;
    uint32_t start = _segment_start_cm;
    for (uint8_t index = _segment_index;
         index < _segment_count &&
         out.visible_objects < TrailGameSnapshot::MAX_VISIBLE_OBJECTS;
         ++index) {
        const TrailSegment& segment = _segments[index];
        const uint32_t object_at = start +
            static_cast<uint32_t>(segment.length_m) * 50u;
        if (segment.obstacle != TrailObstacleKind::NONE &&
            object_at >= distance_cm) {
            const uint32_t ahead = object_at - distance_cm;
            if (ahead <= 25000u) {
                TrailObjectView& view = out.objects[out.visible_objects++];
                view.distance_dm = static_cast<uint16_t>(ahead / 10u);
                view.lateral = segment.obstacle_lateral;
                view.radius = obstacleRadius(segment.obstacle);
                view.kind = segment.obstacle;
                view.flags = segment.flags;
            }
        }
        start += static_cast<uint32_t>(segment.length_m) * 100u;
        if (start > distance_cm + 25000u) break;
    }
}

void TrailAssaultGame::_fillPath(TrailGameSnapshot& out) const {
    out.path_samples = 0;
    int center = 0;
    int elevation = 0;
    for (uint8_t index = _segment_index;
         index < _segment_count &&
         out.path_samples < TrailGameSnapshot::MAX_PATH_SAMPLES;
         ++index) {
        const TrailSegment& segment = _segments[index];
        center = clampValue(center + static_cast<int>(segment.curve) * 5,
                            -100, 100);
        elevation = clampValue(
            elevation + static_cast<int>(segment.grade) * 4, -100, 100);
        TrailPathSample& sample = out.path[out.path_samples++];
        sample.center = static_cast<int8_t>(center);
        sample.elevation = static_cast<int8_t>(elevation);
        sample.width = segment.width;
        sample.terrain = segment.terrain;
    }
}

void TrailAssaultGame::_fillRoadbook(TrailGameSnapshot& out,
                                     uint32_t distance_cm) const {
    out.roadbook_notes = 0;
    uint32_t start = _segment_start_cm;
    for (uint8_t index = _segment_index;
         index < _segment_count &&
         out.roadbook_notes < TrailGameSnapshot::MAX_ROADBOOK_NOTES;
         ++index) {
        const TrailSegment& segment = _segments[index];
        const bool noteworthy = abs32(segment.curve) >= 2 ||
            segment.grade >= 3 || segment.grade <= -3 ||
            segment.terrain == TrailTerrain::MUD ||
            segment.terrain == TrailTerrain::WATER ||
            (segment.flags & (TRAIL_SEGMENT_CHECKPOINT |
                              TRAIL_SEGMENT_GATE |
                              TRAIL_SEGMENT_JUMP));
        if (noteworthy) {
            TrailRoadbookNote& note = out.notes[out.roadbook_notes++];
            note.distance_m = static_cast<uint16_t>(clampValue<uint32_t>(
                start > distance_cm ? (start - distance_cm) / 100u : 0u,
                0u, 0xFFFFu));
            note.turn_grade = segment.curve;
            note.grade = segment.grade;
            note.terrain = segment.terrain;
            note.flags = segment.flags;
        }
        start += static_cast<uint32_t>(segment.length_m) * 100u;
    }
}

void TrailAssaultGame::_fillGhost(TrailGameSnapshot& out,
                                  uint32_t distance_cm) const {
    const bool available = ghostValid(_best_ghost) &&
        _best_ghost.seed == _config.seed &&
        _best_ghost.mode == _config.mode;
    out.ghost.available = available;
    if (!available) return;

    const uint16_t distance_dm = static_cast<uint16_t>(clampValue<uint32_t>(
        distance_cm / 10u, 0u, 0xFFFFu));
    uint16_t best_index = 0;
    uint16_t best_error = 0xFFFFu;
    for (uint16_t i = 0; i < _best_ghost.sample_count; ++i) {
        uint16_t error = _best_ghost.samples[i].distance_dm > distance_dm
            ? _best_ghost.samples[i].distance_dm - distance_dm
            : distance_dm - _best_ghost.samples[i].distance_dm;
        if (error < best_error) {
            best_error = error;
            best_index = i;
        }
        if (_best_ghost.samples[i].distance_dm > distance_dm &&
            error > best_error) break;
    }
    const TrailGhostSample& sample = _best_ghost.samples[best_index];
    out.ghost.time_delta_ms = static_cast<int32_t>(
        _elapsed_ticks * 1000u / FIXED_HZ) -
        static_cast<int32_t>(sample.time_ds) * 100;
    int32_t delta = static_cast<int32_t>(distance_dm) - sample.distance_dm;
    out.ghost.distance_delta_dm = static_cast<int16_t>(clampValue<int32_t>(
        delta, -32768, 32767));
    out.ghost.lateral = sample.lateral;
    out.ghost.speed_kph = sample.speed_kph;
    out.ghost.visible = best_error <= 800u;
}

TrailGameSnapshot TrailAssaultGame::snapshot(uint32_t now_ms) const {
    TrailGameSnapshot out;
    out.seed = _config.seed;
    out.tick = _tick;
    out.phase_ms = _phase_ticks * 1000u / FIXED_HZ;
    out.elapsed_ms = _elapsed_ticks * 1000u / FIXED_HZ;
    out.remaining_ms = _remaining_ticks * 1000u / FIXED_HZ;
    out.distance_cm = _distance_q8 >> 8;
    out.course_length_cm = _course_length_cm;
    out.score = _score + out.distance_cm / 100u;
    const uint32_t speed_cmps = static_cast<uint32_t>(
        _speed_q8 > 0 ? _speed_q8 / 256 : 0);
    out.speed_kph10 = static_cast<uint16_t>(clampValue<uint32_t>(
        speed_cmps * 36u / 100u, 0u, 0xFFFFu));
    out.temperature_c10 = _temperature_c10;
    out.damage_permille = _damage_permille;
    out.grip_permille = _grip_permille;
    out.progress_permille = _course_length_cm == 0u ? 0u :
        static_cast<uint16_t>(clampValue<uint32_t>(
            out.distance_cm * 1000u / _course_length_cm, 0u, 1000u));
    if (_last_event != TrailEvent::NONE) {
        const uint32_t age_ms = (_tick - _last_event_tick) * 1000u / FIXED_HZ;
        out.event_age_ms = static_cast<uint16_t>(
            age_ms > 0xFFFEu ? 0xFFFEu : age_ms);
    }
    out.lateral_permille = static_cast<int16_t>(_lateral);
    out.drift_permille = static_cast<int16_t>(clampValue<int32_t>(
        _drift, -1000, 1000));
    out.jump_height_cm = static_cast<int16_t>(clampValue<int32_t>(
        _jump_height_q8 / 256, 0, 32767));
    out.current_curve = _segments[_segment_index].curve;
    out.current_grade = _segments[_segment_index].grade;
    out.mode = _config.mode;
    out.status = _status;
    out.terrain = _terrain;
    out.last_event = _last_event;
    out.event_sequence = _event_sequence;
    out.checkpoint = _checkpoint;
    out.total_checkpoints = _total_checkpoints;
    out.gate = _gate;
    out.total_gates = _total_gates;
    out.segment = _segment_index;
    out.total_segments = _segment_count;
    out.airborne = _jump_height_q8 > 0;
    out.course_clear = _course_clear;
    out.controller.session_id = _input.session_id;
    out.controller.last_sequence = _input.sequence;
    out.controller.steering = _input.steering;
    out.controller.throttle = _input.throttle;
    out.controller.brake = _input.brake;
    out.controller.timed_out = _controller_timed_out;
    out.controller.neutral = _input.steering == 0 &&
        _input.throttle == 0 && _input.brake == 0;
    if (_input_seen) {
        const int32_t signed_age = static_cast<int32_t>(
            now_ms - _input.received_ms);
        const uint32_t age = signed_age < 0
            ? 0u : static_cast<uint32_t>(signed_age);
        out.controller.age_ms = static_cast<uint16_t>(
            age > 0xFFFEu ? 0xFFFEu : age);
    }
    _fillObjects(out, out.distance_cm);
    _fillPath(out);
    _fillRoadbook(out, out.distance_cm);
    _fillGhost(out, out.distance_cm);
    return out;
}
