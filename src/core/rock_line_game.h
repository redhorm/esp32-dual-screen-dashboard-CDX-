#pragma once
#include <stdint.h>

enum class RockGameStatus : uint8_t {
    IDLE,
    COUNTDOWN,
    PLAYING,
    CRASHED,
    FINISHED,
};

enum class GateResult : uint8_t {
    NONE,
    PERFECT,
    CLEAN,
    HIT,
};

struct RockGameSnapshot {
    RockGameStatus status = RockGameStatus::IDLE;
    GateResult last_result = GateResult::NONE;
    int16_t line_x = 0;             // -90..90, vehicle position across trail
    int16_t safe_x = 0;             // center of the upcoming gate
    uint16_t speed = 0;             // game world units / second
    uint16_t distance_to_gate = 0;
    uint16_t progress_permille = 0;
    uint32_t score = 0;
    uint8_t combo = 0;
    uint8_t lives = 3;
    uint8_t gate = 0;
    uint8_t total_gates = 12;
    uint32_t phase_ms = 0;
};

// Deterministic, allocation-free off-road line game. It has no Arduino
// dependencies and is covered by the native host test suite.
class RockLineGame {
public:
    void start(uint32_t now_ms);
    void stop();
    void control(int steer, bool throttle, uint32_t now_ms);
    void update(uint32_t now_ms);
    RockGameSnapshot snapshot(uint32_t now_ms) const;

    bool isActive() const { return _status != RockGameStatus::IDLE; }
    bool isFinished() const { return _status == RockGameStatus::FINISHED; }
    static int16_t safeLine(uint8_t gate);

private:
    static constexpr uint8_t TOTAL_GATES = 12;
    static constexpr float GATE_SPACING = 300.0f;

    RockGameStatus _status = RockGameStatus::IDLE;
    GateResult _last_result = GateResult::NONE;
    uint32_t _started_ms = 0;
    uint32_t _phase_t0 = 0;
    uint32_t _last_ms = 0;
    uint32_t _last_input_ms = 0;
    float _x = 0.0f;
    float _speed = 0.0f;
    float _distance = 0.0f;
    float _next_gate = 300.0f;
    int8_t _steer = 0;
    bool _throttle = false;
    uint32_t _score = 0;
    uint8_t _combo = 0;
    uint8_t _lives = 3;
    uint8_t _gate = 0;

    void _judgeGate(uint32_t now_ms);
};
