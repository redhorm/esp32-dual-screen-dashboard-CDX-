#pragma once
#include <stdint.h>

// Small, deterministic state machine shared by the displays and web remote.
// It deliberately has no Arduino dependencies or dynamic allocation.
enum class RockGameStatus : uint8_t {
    IDLE,
    ATTRACT,
    COUNTDOWN,
    PLAYING,
    PAUSED,
    CRASHED,
    FINISHED,
};

enum class RockGameEvent : uint8_t {
    NONE,
    DODGED,
    NEAR_MISS,
    HIT,
};

enum class RockObstacleKind : uint8_t {
    BOULDER,
    LOG,
    RUT,
};

struct RockObstacleView {
    int16_t line_x = 0;       // -72..72 across the trail
    uint16_t distance = 0;    // world units ahead of the vehicle
    uint8_t radius = 0;       // collision radius, without vehicle width
    RockObstacleKind kind = RockObstacleKind::BOULDER;
};

struct RockGameSnapshot {
    static constexpr uint8_t MAX_VISIBLE_OBSTACLES = 4;

    RockGameStatus status = RockGameStatus::IDLE;
    RockGameEvent last_event = RockGameEvent::NONE;
    int16_t line_x = 0;             // -88..88, vehicle position
    uint16_t speed = 0;             // world units / second
    uint16_t nearest_distance = 0;  // 0 when no obstacle remains
    uint16_t progress_permille = 0;
    uint32_t score = 0;
    uint8_t combo = 0;
    uint8_t lives = 3;
    uint8_t obstacle = 0;           // obstacles already processed
    uint8_t total_obstacles = 0;
    uint32_t phase_ms = 0;
    int8_t steering = 0;            // echoed web control, -100..100
    uint8_t throttle = 0;
    uint16_t world_distance = 0;
    uint16_t result_age_ms = 0xFFFF;
    uint32_t elapsed_ms = 0;        // excludes countdown and pause
    uint8_t visible_obstacles = 0;
    RockObstacleView obstacles[MAX_VISIBLE_OBSTACLES];
    bool course_clear = false;
};

class RockLineGame {
public:
    static constexpr uint8_t TOTAL_OBSTACLES = 14;
    static constexpr uint8_t MAX_VISIBLE_OBSTACLES =
        RockGameSnapshot::MAX_VISIBLE_OBSTACLES;
    static constexpr uint32_t COUNTDOWN_MS = 2400;
    static constexpr uint32_t CRASH_RECOVERY_MS = 700;
    static constexpr uint32_t INPUT_TIMEOUT_MS = 800;
    static constexpr int16_t ROAD_LIMIT = 88;
    static constexpr int16_t VEHICLE_HALF_WIDTH = 10;

    void enterArcade(uint32_t now_ms);
    void start(uint32_t now_ms);
    void stop();
    void pause(uint32_t now_ms);
    void resume(uint32_t now_ms);
    void control(int steer, bool throttle, uint32_t now_ms);
    void update(uint32_t now_ms);
    RockGameSnapshot snapshot(uint32_t now_ms) const;

    bool isActive() const { return _status != RockGameStatus::IDLE; }
    bool isFinished() const { return _status == RockGameStatus::FINISHED; }
    bool isPaused() const { return _status == RockGameStatus::PAUSED; }

    static int16_t obstacleLine(uint8_t index);
    static uint8_t obstacleRadius(uint8_t index);
    static RockObstacleKind obstacleKind(uint8_t index);
    static bool isCollision(float vehicle_x, int16_t obstacle_x,
                            uint8_t obstacle_radius);

private:
    static constexpr float OBSTACLE_SPACING = 260.0f;
    static constexpr float COURSE_DISTANCE =
        TOTAL_OBSTACLES * OBSTACLE_SPACING;

    RockGameStatus _status = RockGameStatus::IDLE;
    RockGameEvent _last_event = RockGameEvent::NONE;
    uint32_t _phase_t0 = 0;
    uint32_t _last_ms = 0;
    uint32_t _last_input_ms = 0;
    uint32_t _result_t0 = 0;
    uint32_t _elapsed_ms = 0;
    bool _has_result = false;
    bool _course_clear = false;
    float _x = 0.0f;
    float _steer_smoothed = 0.0f;
    float _lateral_velocity = 0.0f;
    float _speed = 0.0f;
    float _distance = 0.0f;
    int8_t _steer = 0;
    bool _throttle = false;
    uint32_t _score = 0;
    uint8_t _combo = 0;
    uint8_t _lives = 3;
    uint8_t _obstacle = 0;

    void _resetRun(RockGameStatus status, uint32_t now_ms);
    void _judgeObstacle(uint32_t now_ms);
    void _finish(uint32_t now_ms, bool clear);
    void _populateObstacles(RockGameSnapshot& out, float distance,
                            uint8_t first) const;
};
