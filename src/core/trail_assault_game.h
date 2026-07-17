#pragma once

#include <stddef.h>
#include <stdint.h>
#include "audio_event_bus.h"

enum class TrailMode : uint8_t {
    TIME_ATTACK = 0,
    GATE_RUSH = 1,
    HILL_CLIMB = 2,
};

enum class TrailStatus : uint8_t {
    IDLE = 0,
    ATTRACT,
    COUNTDOWN,
    RUNNING,
    PAUSED,
    FINISHED,
    FAILED,
};

enum class TrailTerrain : uint8_t {
    DIRT = 0,
    ROCK,
    MUD,
    SAND,
    WATER,
    JUMP,
};

enum class TrailObstacleKind : uint8_t {
    NONE = 0,
    BOULDER,
    LOG,
    RUT,
    GATE,
    JUMP_RAMP,
};

enum class TrailEvent : uint8_t {
    NONE = 0,
    CHECKPOINT,
    GATE_CLEAR,
    GATE_MISSED,
    COLLISION_SMALL,
    COLLISION_LARGE,
    TERRAIN_CHANGED,
    JUMP,
    LANDING,
    CONTROLLER_TIMEOUT,
    OVERHEAT,
    FINISH,
};

enum class TrailInputResult : uint8_t {
    ACCEPTED = 0,
    WRONG_SESSION,
    STALE_SEQUENCE,
    INACTIVE,
};

enum TrailSegmentFlag : uint8_t {
    TRAIL_SEGMENT_NONE = 0,
    TRAIL_SEGMENT_CHECKPOINT = 1u << 0,
    TRAIL_SEGMENT_GATE = 1u << 1,
    TRAIL_SEGMENT_JUMP = 1u << 2,
};

struct TrailGameConfig {
    TrailMode mode = TrailMode::TIME_ATTACK;
    uint32_t seed = 0x4400C0DEu;
};

struct TrailInput {
    uint32_t session_id = 0;
    uint32_t client_ms = 0;
    uint32_t received_ms = 0;
    uint16_t sequence = 0;
    int8_t steering = 0;   // -100..100
    uint8_t throttle = 0;  // 0..100
    uint8_t brake = 0;     // 0..100
};

struct TrailSegment {
    uint16_t length_m = 0;
    int8_t curve = 0;             // -6..6; roadbook turn grade
    int8_t grade = 0;             // signed slope grade
    int8_t obstacle_lateral = 0;  // -80..80 across trail
    uint8_t width = 90;           // relative trail width
    TrailTerrain terrain = TrailTerrain::DIRT;
    TrailObstacleKind obstacle = TrailObstacleKind::NONE;
    uint8_t flags = TRAIL_SEGMENT_NONE;
};

struct TrailPathSample {
    int8_t center = 0;
    int8_t elevation = 0;
    uint8_t width = 90;
    TrailTerrain terrain = TrailTerrain::DIRT;
};

struct TrailObjectView {
    uint16_t distance_dm = 0;
    int8_t lateral = 0;
    uint8_t radius = 0;
    TrailObstacleKind kind = TrailObstacleKind::NONE;
    uint8_t flags = 0;
};

struct TrailRoadbookNote {
    uint16_t distance_m = 0;
    int8_t turn_grade = 0;
    int8_t grade = 0;
    TrailTerrain terrain = TrailTerrain::DIRT;
    uint8_t flags = 0;
};

struct TrailGhostView {
    int32_t time_delta_ms = 0;
    int16_t distance_delta_dm = 0;
    int8_t lateral = 0;
    uint8_t speed_kph = 0;
    bool available = false;
    bool visible = false;
};

struct TrailControllerHealth {
    uint32_t session_id = 0;
    uint16_t last_sequence = 0;
    uint16_t age_ms = 0xFFFFu;
    int8_t steering = 0;
    uint8_t throttle = 0;
    uint8_t brake = 0;
    bool timed_out = true;
    bool neutral = true;
};

struct TrailGameSnapshot {
    static constexpr uint8_t MAX_VISIBLE_OBJECTS = 4;
    static constexpr uint8_t MAX_PATH_SAMPLES = 8;
    static constexpr uint8_t MAX_ROADBOOK_NOTES = 2;

    uint32_t seed = 0;
    uint32_t tick = 0;
    uint32_t phase_ms = 0;
    uint32_t elapsed_ms = 0;
    uint32_t remaining_ms = 0;
    uint32_t distance_cm = 0;
    uint32_t course_length_cm = 0;
    uint32_t score = 0;
    uint16_t speed_kph10 = 0;
    uint16_t temperature_c10 = 0;
    uint16_t damage_permille = 0;
    uint16_t grip_permille = 0;
    uint16_t progress_permille = 0;
    uint16_t event_age_ms = 0xFFFFu;
    int16_t lateral_permille = 0;
    int16_t drift_permille = 0;
    int16_t jump_height_cm = 0;
    int8_t current_curve = 0;
    int8_t current_grade = 0;
    TrailMode mode = TrailMode::TIME_ATTACK;
    TrailStatus status = TrailStatus::IDLE;
    TrailTerrain terrain = TrailTerrain::DIRT;
    TrailEvent last_event = TrailEvent::NONE;
    uint8_t event_sequence = 0;
    uint8_t checkpoint = 0;
    uint8_t total_checkpoints = 0;
    uint8_t gate = 0;
    uint8_t total_gates = 0;
    uint8_t segment = 0;
    uint8_t total_segments = 0;
    uint8_t visible_objects = 0;
    uint8_t path_samples = 0;
    uint8_t roadbook_notes = 0;
    bool airborne = false;
    bool course_clear = false;
    TrailControllerHealth controller;
    TrailGhostView ghost;
    TrailObjectView objects[MAX_VISIBLE_OBJECTS];
    TrailPathSample path[MAX_PATH_SAMPLES];
    TrailRoadbookNote notes[MAX_ROADBOOK_NOTES];
};

struct TrailGhostSample {
    uint16_t time_ds = 0;
    uint16_t distance_dm = 0;
    int8_t lateral = 0;
    uint8_t speed_kph = 0;
    int8_t steering = 0;
    uint8_t throttle = 0;
    uint8_t state = 0;
    uint8_t reserved = 0;
};

struct TrailGhostRecording {
    static constexpr uint16_t MAX_SAMPLES = 300;
    static constexpr uint32_t MAGIC = 0x55444748u;  // "UDGH"
    static constexpr uint16_t SCHEMA_VERSION = 1;
    static constexpr uint8_t SAMPLE_HZ = 10;

    uint32_t magic = MAGIC;
    uint32_t seed = 0;
    uint32_t finish_ms = 0;
    uint32_t crc32 = 0;
    uint16_t schema_version = SCHEMA_VERSION;
    uint16_t generator_version = 1;
    uint16_t sample_count = 0;
    TrailMode mode = TrailMode::TIME_ATTACK;
    uint8_t sample_hz = SAMPLE_HZ;
    TrailGhostSample samples[MAX_SAMPLES] = {};
};

class TrailAssaultGame {
public:
    static constexpr uint8_t MAX_SEGMENTS = 80;
    static constexpr uint16_t GENERATOR_VERSION = 1;
    static constexpr uint16_t FIXED_HZ = 60;
    static constexpr uint16_t INPUT_TIMEOUT_MS = 300;
    static constexpr uint16_t COUNTDOWN_TICKS = FIXED_HZ * 3;
    static constexpr uint16_t MAX_CATCH_UP_MS = 500;

    TrailAssaultGame();

    void configure(const TrailGameConfig& config);
    const TrailGameConfig& config() const { return _config; }
    void enterAttract(uint32_t now_ms);
    void start(uint32_t now_ms, uint32_t session_id);
    void stop();
    void pause(uint32_t now_ms);
    void resume(uint32_t now_ms);
    void update(uint32_t now_ms);

    TrailInputResult submitInput(const TrailInput& input);
    TrailInputResult neutralize(uint32_t session_id, uint16_t sequence,
                                uint32_t received_ms);

    TrailGameSnapshot snapshot(uint32_t now_ms) const;
    const TrailSegment& segment(uint8_t index) const;
    uint8_t segmentCount() const { return _segment_count; }
    uint32_t courseLengthCm() const { return _course_length_cm; }
    uint32_t trackFingerprint() const { return _track_fingerprint; }

    bool isActive() const { return _status != TrailStatus::IDLE; }
    bool isFinished() const {
        return _status == TrailStatus::FINISHED ||
               _status == TrailStatus::FAILED;
    }
    bool isPaused() const { return _status == TrailStatus::PAUSED; }

    const AudioEventBus& audioEvents() const { return _audio; }
    AudioEventBus& audioEvents() { return _audio; }

    const TrailGhostRecording& currentGhost() const { return _current_ghost; }
    const TrailGhostRecording& bestGhost() const { return _best_ghost; }
    bool loadBestGhost(const TrailGhostRecording& recording);
    void clearBestGhost();
    static uint32_t ghostCrc(const TrailGhostRecording& recording);
    static bool ghostValid(const TrailGhostRecording& recording);

private:
    TrailGameConfig _config;
    TrailSegment _segments[MAX_SEGMENTS] = {};
    uint8_t _segment_count = 0;
    uint8_t _segment_index = 0;
    uint32_t _segment_start_cm = 0;
    uint32_t _course_length_cm = 0;
    uint32_t _track_fingerprint = 0;
    TrailStatus _status = TrailStatus::IDLE;
    TrailTerrain _terrain = TrailTerrain::DIRT;
    TrailEvent _last_event = TrailEvent::NONE;
    uint8_t _event_sequence = 0;
    uint32_t _last_event_tick = 0;
    uint32_t _last_update_ms = 0;
    uint32_t _sim_now_ms = 0;
    uint32_t _step_ms_remainder = 0;
    uint32_t _accumulator_units = 0;
    uint32_t _tick = 0;
    uint32_t _phase_ticks = 0;
    uint32_t _elapsed_ticks = 0;
    uint32_t _remaining_ticks = 0;
    uint32_t _distance_q8 = 0;   // centimetres * 256
    int32_t _speed_q8 = 0;       // centimetres/second * 256
    int32_t _lateral = 0;        // -1000..1000
    int32_t _drift = 0;
    int32_t _jump_height_q8 = 0;
    int32_t _jump_velocity_q8 = 0;
    uint16_t _temperature_c10 = 650;
    uint16_t _damage_permille = 0;
    uint16_t _grip_permille = 900;
    uint8_t _checkpoint = 0;
    uint8_t _total_checkpoints = 0;
    uint8_t _gate = 0;
    uint8_t _total_gates = 0;
    uint32_t _score = 0;
    bool _course_clear = false;
    bool _obstacle_judged = false;
    bool _jump_triggered = false;
    bool _input_seen = false;
    bool _controller_timed_out = true;
    TrailInput _input;
    uint8_t _last_gear = 0;
    uint32_t _last_skid_tick = 0;
    bool _temperature_warning = false;
    AudioEventBus _audio;
    TrailGhostRecording _current_ghost;
    TrailGhostRecording _best_ghost;
    uint16_t _ghost_sample_stride_ticks = 6;

    void _generateTrack();
    void _resetRun(TrailStatus status, uint32_t now_ms, uint32_t session_id);
    void _step();
    void _stepRunning();
    void _advanceSegment(uint32_t previous_distance_cm,
                         uint32_t distance_cm);
    void _judgeObstacle(const TrailSegment& segment, uint32_t speed_cmps);
    void _finish(bool clear);
    void _setEvent(TrailEvent event);
    void _recordGhostSample();
    void _finalizeGhost();
    void _fillObjects(TrailGameSnapshot& out, uint32_t distance_cm) const;
    void _fillPath(TrailGameSnapshot& out) const;
    void _fillRoadbook(TrailGameSnapshot& out, uint32_t distance_cm) const;
    void _fillGhost(TrailGameSnapshot& out, uint32_t distance_cm) const;
};

static_assert(sizeof(TrailGameSnapshot) <= 192,
              "Trail snapshot must stay cheap to copy between display tasks");
