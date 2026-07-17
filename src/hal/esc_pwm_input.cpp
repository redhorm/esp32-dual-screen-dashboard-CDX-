#include "esc_pwm_input.h"

#ifndef NATIVE_BUILD

EscPwmInput* EscPwmInput::_instance = nullptr;

EscPwmInput::EscPwmInput() : _processor(defaultSignalConfig()) {}

EscPwmInput::~EscPwmInput() { end(); }

EscSignalConfig EscPwmInput::defaultSignalConfig() {
    EscSignalConfig config;
    config.reverse_us = ESC_PWM_REVERSE_US;
    config.neutral_us = ESC_PWM_NEUTRAL_US;
    config.forward_us = ESC_PWM_FORWARD_US;
    config.enter_threshold = ESC_PWM_ENTER;
    config.exit_threshold = ESC_PWM_EXIT;
    config.smoothing_ms = ESC_PWM_SMOOTH_MS;
    config.max_slew_per_second = ESC_PWM_MAX_SLEW;
    config.neutral_dwell_ms = ESC_NEUTRAL_DWELL_MS;
    config.reverse_confirm_ms = ESC_REVERSE_CONFIRM_MS;
    config.reverse_arm_timeout_ms = ESC_REVERSE_ARM_MS;
    return config;
}

bool EscPwmInput::begin() {
    return begin(ESC_PWM_INPUT_ENABLED != 0);
}

bool EscPwmInput::begin(bool enabled) {
    end();
    resetFilter();
    if (!enabled) return false;

    // Never leave the interrupt input floating: a disconnected lead in an
    // automotive/RC environment can otherwise generate a continuous CHANGE
    // interrupt storm.  The external conditioned input should still include
    // its own defined bias and protection; this is the last-resort guard.
    pinMode(ESC_PWM_SIGNAL_PIN, INPUT_PULLDOWN);
    _instance = this;
    _enabled = true;
    attachInterrupt(digitalPinToInterrupt(ESC_PWM_SIGNAL_PIN), isrThunk, CHANGE);
    return true;
}

void EscPwmInput::end() {
    if (_enabled) {
        detachInterrupt(digitalPinToInterrupt(ESC_PWM_SIGNAL_PIN));
        pinMode(ESC_PWM_SIGNAL_PIN, INPUT_PULLDOWN);
    }
    if (_instance == this) _instance = nullptr;
    _enabled = false;
}

void EscPwmInput::resetFilter() {
    portENTER_CRITICAL(&_capture_mux);
    _rise_us = 0;
    _last_valid_us = 0;
    _pulse_us = 0;
    _filtered_us = 0;
    _rise_seen = false;
    _sample_seen = false;
    portEXIT_CRITICAL(&_capture_mux);
    _processor.reset(millis());
}

void EscPwmInput::configureSignal(const EscSignalConfig& config) {
    _processor.configure(config);
}

void EscPwmInput::setEscType(EscType type) {
    EscSignalConfig config = _processor.config();
    config.type = type;
    _processor.configure(config);
}

EscPwmSample EscPwmInput::sample() {
    EscPwmSample out;
    out.enabled = _enabled;
    out.type = _processor.config().type;
    if (!_enabled) return out;

    uint32_t last_valid_us;
    uint16_t pulse_us;
    uint16_t filtered_us;
    bool sample_seen;
    portENTER_CRITICAL(&_capture_mux);
    last_valid_us = _last_valid_us;
    pulse_us = _pulse_us;
    filtered_us = _filtered_us;
    sample_seen = _sample_seen;
    portEXIT_CRITICAL(&_capture_mux);

    if (!sample_seen) return out;
    const uint32_t age_us = static_cast<uint32_t>(micros() - last_valid_us);
    out.age_ms = age_us / 1000U;
    out.pulse_us = pulse_us;
    out.filtered_us = filtered_us;
    out.valid = age_us <= ESC_PWM_TIMEOUT_US && validPulseWidth(pulse_us);
    const EscSignalFrame drive =
        _processor.update(filtered_us, out.valid, millis());
    out.normalized = drive.normalized;
    out.forward = drive.forward;
    out.reverse = drive.reverse;
    out.state = drive.state;
    out.state_age_ms = drive.state_age_ms;
    return out;
}

bool IRAM_ATTR EscPwmInput::validPulseWidth(uint32_t pulse_us) {
    return pulse_us >= ESC_PWM_MIN_US && pulse_us <= ESC_PWM_MAX_US;
}

void IRAM_ATTR EscPwmInput::isrThunk() {
    if (_instance != nullptr) _instance->handleEdge();
}

void IRAM_ATTR EscPwmInput::handleEdge() {
    const uint32_t now_us = micros();
    const bool high = digitalRead(ESC_PWM_SIGNAL_PIN) == HIGH;
    portENTER_CRITICAL_ISR(&_capture_mux);
    if (high) {
        _rise_us = now_us;
        _rise_seen = true;
        portEXIT_CRITICAL_ISR(&_capture_mux);
        return;
    }

    if (!_rise_seen) {
        portEXIT_CRITICAL_ISR(&_capture_mux);
        return;
    }
    _rise_seen = false;
    const uint32_t width_us = static_cast<uint32_t>(now_us - _rise_us);
    if (!validPulseWidth(width_us)) {
        portEXIT_CRITICAL_ISR(&_capture_mux);
        return;
    }

    const uint16_t width = static_cast<uint16_t>(width_us);
    _pulse_us = width;
    if (!_sample_seen) {
        _filtered_us = width;
        _sample_seen = true;
    } else {
        // Integer IIR: 75% previous sample + 25% newest valid pulse.
        _filtered_us = static_cast<uint16_t>(
            (static_cast<uint32_t>(_filtered_us) * 3U + width + 2U) / 4U);
    }
    _last_valid_us = now_us;
    portEXIT_CRITICAL_ISR(&_capture_mux);
}

#endif  // NATIVE_BUILD
