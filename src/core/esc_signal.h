#pragma once

#include <stdint.h>

// Pure, host-testable interpretation of a conditioned RC/ESC PWM signal.
// Hardware capture only supplies a pulse and its validity; every semantic
// decision (neutral, brake and reverse confirmation) lives here.

enum class EscType : uint8_t {
    FORWARD_BRAKE_REVERSE = 0,
    CRAWLER_FORWARD_REVERSE = 1,
    FORWARD_BRAKE_ONLY = 2,
};

enum class EscDriveState : uint8_t {
    FORWARD = 0,
    NEUTRAL = 1,
    BRAKING = 2,
    REVERSE_ARMED = 3,
    REVERSE = 4,
};

struct EscSignalConfig {
    EscType type = EscType::FORWARD_BRAKE_REVERSE;
    uint16_t reverse_us = 1000;
    uint16_t neutral_us = 1500;
    uint16_t forward_us = 2000;

    // Schmitt thresholds in normalized command units.  A direction starts at
    // enter_threshold and remains latched until it crosses exit_threshold.
    float enter_threshold = 0.12f;
    float exit_threshold = 0.06f;
    // Optional directional thresholds preserve a deadband expressed in real
    // microseconds when neutral is not exactly centered between endpoints.
    // Negative values inherit the generic threshold for backward compatibility.
    float forward_enter_threshold = -1.0f;
    float reverse_enter_threshold = -1.0f;
    float forward_exit_threshold = -1.0f;
    float reverse_exit_threshold = -1.0f;

    // Input conditioning outside the ISR.  Smoothing removes PWM jitter while
    // the slew limit rejects implausible command steps without adding delay to
    // normal pedal/trigger motion.
    uint16_t smoothing_ms = 55;
    float max_slew_per_second = 8.0f;

    // Reverse safety timing.  A road/race ESC requires brake -> neutral dwell
    // -> a second reverse command.  Crawler mode skips braking but still uses
    // a short confirmation dwell before energising reverse lights.
    uint16_t neutral_dwell_ms = 180;
    uint16_t reverse_confirm_ms = 220;
    uint16_t reverse_arm_timeout_ms = 1600;
};

struct EscSignalFrame {
    bool valid = false;
    uint16_t pulse_us = 0;
    float raw_normalized = 0.0f;       // -1 reverse .. 0 neutral .. +1 forward
    float normalized = 0.0f;           // conditioned value
    float forward = 0.0f;              // 0 .. 1
    float reverse = 0.0f;              // 0 .. 1 (command, not permission)
    EscDriveState state = EscDriveState::NEUTRAL;
    bool state_changed = false;
    uint32_t state_age_ms = 0;
};

class EscSignalProcessor {
public:
    explicit EscSignalProcessor(const EscSignalConfig& config = EscSignalConfig());

    void configure(const EscSignalConfig& config);
    void reset(uint32_t now_ms = 0);
    EscSignalFrame update(uint16_t pulse_us, bool valid, uint32_t now_ms);

    const EscSignalConfig& config() const { return _config; }
    const EscSignalFrame& frame() const { return _frame; }

    static const char* stateName(EscDriveState state);
    static const char* typeName(EscType type);
    static EscType typeFromName(const char* name);
    // Converts a symmetric neutral band expressed in real PWM microseconds
    // into direction-specific normalized thresholds.
    static void configureDeadbandUs(EscSignalConfig& config,
                                    uint16_t deadband_us);

private:
    enum class Band : int8_t { REVERSE = -1, NEUTRAL = 0, FORWARD = 1 };

    EscSignalConfig _config;
    EscSignalFrame _frame;
    Band _band = Band::NEUTRAL;
    uint32_t _last_update_ms = 0;
    uint32_t _state_started_ms = 0;
    uint32_t _neutral_started_ms = 0;
    uint32_t _reverse_started_ms = 0;
    bool _has_update = false;
    bool _neutral_timer_active = false;
    bool _reverse_timer_active = false;

    static float clampUnit(float value);
    float normalize(uint16_t pulse_us) const;
    Band classify(float value);
    void transition(EscDriveState state, uint32_t now_ms);
    void updateState(Band band, uint32_t now_ms);
    void invalidate(uint32_t now_ms);
};
