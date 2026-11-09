#pragma once

#ifndef NATIVE_BUILD

#include <Arduino.h>
#include <stdint.h>

#include "config.h"
#include "core/esc_signal.h"
#include "core/pwm_median.h"

struct EscPwmSample {
    bool enabled = false;
    bool valid = false;
    uint16_t pulse_us = 0;
    uint16_t filtered_us = 0;
    uint32_t age_ms = 0;
    float normalized = 0.0f;
    float forward = 0.0f;
    float reverse = 0.0f;
    EscDriveState state = EscDriveState::NEUTRAL;
    EscType type = EscType::FORWARD_BRAKE_REVERSE;
    uint32_t state_age_ms = 0;
};

// Prepared RC PWM capture.  It is disabled by default.  The ISR only measures
// and lightly filters pulse width; sample() delegates all drive semantics to
// the deterministic EscSignalProcessor in core.
class EscPwmInput {
public:
    EscPwmInput();
    ~EscPwmInput();

    // Uses ESC_PWM_INPUT_ENABLED.  begin(true) explicitly opts in later.
    bool begin();
    bool begin(bool enabled);
    void end();
    EscPwmSample sample();
    void resetFilter();
    void configureSignal(const EscSignalConfig& config);
    void setEscType(EscType type);

    bool enabled() const { return _enabled; }
    EscType escType() const { return _processor.config().type; }
    static bool IRAM_ATTR validPulseWidth(uint32_t pulse_us);

private:
    static EscPwmInput* _instance;

    // The GPIO ISR may execute on a different core from controlIoTick().
    // A shared spinlock is therefore required; noInterrupts() is core-local.
    portMUX_TYPE _capture_mux = portMUX_INITIALIZER_UNLOCKED;

    volatile uint32_t _rise_us = 0;
    volatile uint32_t _last_valid_us = 0;
    volatile uint16_t _pulse_us = 0;
    volatile uint16_t _filtered_us = 0;
    volatile uint16_t _previous_pulse_us = 0;
    volatile uint16_t _older_pulse_us = 0;
    volatile bool _rise_seen = false;
    volatile bool _sample_seen = false;
    bool _enabled = false;
    EscSignalProcessor _processor;

    static void IRAM_ATTR isrThunk();
    void IRAM_ATTR handleEdge();
    static EscSignalConfig defaultSignalConfig();
};

#endif  // NATIVE_BUILD
