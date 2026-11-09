#include "esc_signal.h"

#include <string.h>

namespace {
float minf(float lhs, float rhs) { return lhs < rhs ? lhs : rhs; }
float maxf(float lhs, float rhs) { return lhs > rhs ? lhs : rhs; }
}  // namespace

EscSignalProcessor::EscSignalProcessor(const EscSignalConfig& config) {
    configure(config);
}

void EscSignalProcessor::configure(const EscSignalConfig& config) {
    _config = config;

    // Keep calibration ordered and every timing/threshold meaningful even if
    // settings are restored from an older or corrupted preference record.
    if (_config.neutral_us < 2U || _config.neutral_us > 65533U)
        _config.neutral_us = 1500U;
    if (_config.reverse_us >= _config.neutral_us)
        _config.reverse_us = static_cast<uint16_t>(_config.neutral_us - 1U);
    if (_config.forward_us <= _config.neutral_us)
        _config.forward_us = static_cast<uint16_t>(_config.neutral_us + 1U);
    if (_config.enter_threshold < 0.02f) _config.enter_threshold = 0.02f;
    if (_config.enter_threshold > 0.90f) _config.enter_threshold = 0.90f;
    if (_config.exit_threshold < 0.0f) _config.exit_threshold = 0.0f;
    if (_config.exit_threshold >= _config.enter_threshold)
        _config.exit_threshold = _config.enter_threshold * 0.5f;
    if (_config.forward_enter_threshold <= 0.0f)
        _config.forward_enter_threshold = _config.enter_threshold;
    if (_config.reverse_enter_threshold <= 0.0f)
        _config.reverse_enter_threshold = _config.enter_threshold;
    if (_config.forward_exit_threshold < 0.0f)
        _config.forward_exit_threshold = _config.exit_threshold;
    if (_config.reverse_exit_threshold < 0.0f)
        _config.reverse_exit_threshold = _config.exit_threshold;
    _config.forward_enter_threshold = minf(0.90f,
        maxf(0.005f, _config.forward_enter_threshold));
    _config.reverse_enter_threshold = minf(0.90f,
        maxf(0.005f, _config.reverse_enter_threshold));
    _config.forward_exit_threshold = maxf(0.0f,
        minf(_config.forward_enter_threshold * 0.95f,
             _config.forward_exit_threshold));
    _config.reverse_exit_threshold = maxf(0.0f,
        minf(_config.reverse_enter_threshold * 0.95f,
             _config.reverse_exit_threshold));
    if (_config.max_slew_per_second < 0.1f)
        _config.max_slew_per_second = 0.1f;
    if (_config.reverse_arm_timeout_ms < _config.reverse_confirm_ms)
        _config.reverse_arm_timeout_ms = _config.reverse_confirm_ms;

    reset(0);
}

void EscSignalProcessor::configureDeadbandUs(EscSignalConfig& config,
                                             uint16_t deadband_us) {
    const uint16_t high_span = config.forward_us > config.neutral_us
        ? config.forward_us - config.neutral_us : 1u;
    const uint16_t low_span = config.neutral_us > config.reverse_us
        ? config.neutral_us - config.reverse_us : 1u;
    // Endpoints remain electrically ordered; polarity changes their meaning.
    const uint16_t forward_span = config.inverted ? low_span : high_span;
    const uint16_t reverse_span = config.inverted ? high_span : low_span;
    config.forward_enter_threshold = minf(0.90f, maxf(0.005f,
        static_cast<float>(deadband_us) / static_cast<float>(forward_span)));
    config.reverse_enter_threshold = minf(0.90f, maxf(0.005f,
        static_cast<float>(deadband_us) / static_cast<float>(reverse_span)));
    config.forward_exit_threshold = config.forward_enter_threshold * 0.55f;
    config.reverse_exit_threshold = config.reverse_enter_threshold * 0.55f;
    config.enter_threshold = maxf(config.forward_enter_threshold,
                                  config.reverse_enter_threshold);
    config.exit_threshold = maxf(config.forward_exit_threshold,
                                 config.reverse_exit_threshold);
}

void EscSignalProcessor::reset(uint32_t now_ms) {
    _frame = EscSignalFrame{};
    _band = Band::NEUTRAL;
    _last_update_ms = now_ms;
    _state_started_ms = now_ms;
    _neutral_started_ms = now_ms;
    _reverse_started_ms = now_ms;
    _has_update = false;
    _neutral_timer_active = false;
    _reverse_timer_active = false;
}

float EscSignalProcessor::clampUnit(float value) {
    if (value < -1.0f) return -1.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

float EscSignalProcessor::normalize(uint16_t pulse_us) const {
    float value;
    if (pulse_us >= _config.neutral_us) {
        const float span = static_cast<float>(_config.forward_us - _config.neutral_us);
        value = clampUnit(static_cast<float>(pulse_us - _config.neutral_us) / span);
    } else {
        const float span = static_cast<float>(_config.neutral_us - _config.reverse_us);
        value = clampUnit(-static_cast<float>(_config.neutral_us - pulse_us) / span);
    }
    return _config.inverted ? -value : value;
}

EscSignalProcessor::Band EscSignalProcessor::classify(float value) {
    // Direction changes cross the larger entry threshold; an already-latched
    // direction releases at the smaller exit threshold.  This is the key
    // anti-chatter boundary around neutral.
    if (_band == Band::FORWARD) {
        if (value <= -_config.reverse_enter_threshold) _band = Band::REVERSE;
        else if (value < _config.forward_exit_threshold) _band = Band::NEUTRAL;
    } else if (_band == Band::REVERSE) {
        if (value >= _config.forward_enter_threshold) _band = Band::FORWARD;
        else if (value > -_config.reverse_exit_threshold) _band = Band::NEUTRAL;
    } else {
        if (value >= _config.forward_enter_threshold) _band = Band::FORWARD;
        else if (value <= -_config.reverse_enter_threshold) _band = Band::REVERSE;
    }
    return _band;
}

void EscSignalProcessor::transition(EscDriveState state, uint32_t now_ms) {
    if (_frame.state == state) return;
    _frame.state = state;
    _frame.state_changed = true;
    _state_started_ms = now_ms;
    _neutral_timer_active = false;
    _reverse_timer_active = false;
}

void EscSignalProcessor::invalidate(uint32_t now_ms) {
    const bool changed = _frame.state != EscDriveState::NEUTRAL;
    _frame = EscSignalFrame{};
    _frame.state_changed = changed;
    _band = Band::NEUTRAL;
    _has_update = false;
    _last_update_ms = now_ms;
    _state_started_ms = now_ms;
    _neutral_timer_active = false;
    _reverse_timer_active = false;
}

void EscSignalProcessor::updateState(Band band, uint32_t now_ms) {
    const EscType type = _config.type;

    if (band == Band::FORWARD) {
        transition(EscDriveState::FORWARD, now_ms);
        return;
    }

    if (type == EscType::FORWARD_BRAKE_ONLY) {
        transition(band == Band::REVERSE ? EscDriveState::BRAKING
                                         : EscDriveState::NEUTRAL,
                   now_ms);
        return;
    }

    if (type == EscType::CRAWLER_FORWARD_REVERSE) {
        // Input capture already rejects isolated spikes; preserve the actual
        // crawler direction without a race-ESC brake/confirmation sequence.
        transition(band == Band::REVERSE ? EscDriveState::REVERSE
                                         : EscDriveState::NEUTRAL, now_ms);
        return;
    }

    // Race/road ESC: the first reverse command is always braking.  Reverse is
    // possible only after releasing to neutral for the configured dwell and
    // issuing a second, continuously confirmed reverse command.
    switch (_frame.state) {
        case EscDriveState::FORWARD:
        case EscDriveState::NEUTRAL:
            if (band == Band::REVERSE)
                transition(EscDriveState::BRAKING, now_ms);
            else
                transition(EscDriveState::NEUTRAL, now_ms);
            break;

        case EscDriveState::BRAKING:
            if (band == Band::REVERSE) {
                _neutral_timer_active = false;
            } else {
                if (!_neutral_timer_active) {
                    _neutral_started_ms = now_ms;
                    _neutral_timer_active = true;
                }
                if (static_cast<uint32_t>(now_ms - _neutral_started_ms) >=
                    _config.neutral_dwell_ms)
                    transition(EscDriveState::REVERSE_ARMED, now_ms);
            }
            break;

        case EscDriveState::REVERSE_ARMED:
            if (static_cast<uint32_t>(now_ms - _state_started_ms) >
                _config.reverse_arm_timeout_ms) {
                transition(EscDriveState::NEUTRAL, now_ms);
            } else if (band == Band::REVERSE) {
                if (!_reverse_timer_active) {
                    _reverse_started_ms = now_ms;
                    _reverse_timer_active = true;
                }
                if (static_cast<uint32_t>(now_ms - _reverse_started_ms) >=
                    _config.reverse_confirm_ms)
                    transition(EscDriveState::REVERSE, now_ms);
            } else {
                _reverse_timer_active = false;
            }
            break;

        case EscDriveState::REVERSE:
            if (band != Band::REVERSE)
                transition(EscDriveState::NEUTRAL, now_ms);
            break;
    }
}

EscSignalFrame EscSignalProcessor::update(uint16_t pulse_us, bool valid,
                                          uint32_t now_ms) {
    if (!valid) {
        invalidate(now_ms);
        return _frame;
    }

    _frame.state_changed = false;
    _frame.valid = true;
    _frame.pulse_us = pulse_us;
    _frame.raw_normalized = normalize(pulse_us);

    if (_config.type == EscType::CRAWLER_FORWARD_REVERSE) {
        // Median capture + Schmitt deadband are sufficient for crawler input.
        // A smoothing tail would keep the old direction after a direct reversal
        // or keep moving after release to neutral.
        _frame.normalized = _frame.raw_normalized;
        _last_update_ms = now_ms;
        _has_update = true;
    } else if (!_has_update) {
        _frame.normalized = _frame.raw_normalized;
        _last_update_ms = now_ms;
        _has_update = true;
    } else {
        const uint32_t dt_ms = static_cast<uint32_t>(now_ms - _last_update_ms);
        _last_update_ms = now_ms;
        if (dt_ms != 0) {
            const float alpha = _config.smoothing_ms == 0
                ? 1.0f
                : static_cast<float>(dt_ms) /
                  static_cast<float>(dt_ms + _config.smoothing_ms);
            const float desired = _frame.normalized +
                (_frame.raw_normalized - _frame.normalized) * alpha;
            const float max_delta = _config.max_slew_per_second *
                                    static_cast<float>(dt_ms) * 0.001f;
            const float delta = desired - _frame.normalized;
            _frame.normalized += maxf(-max_delta, minf(max_delta, delta));
            _frame.normalized = clampUnit(_frame.normalized);
        }
    }

    _frame.forward = maxf(0.0f, _frame.normalized);
    _frame.reverse = maxf(0.0f, -_frame.normalized);
    updateState(classify(_frame.normalized), now_ms);
    _frame.state_age_ms = static_cast<uint32_t>(now_ms - _state_started_ms);
    return _frame;
}

const char* EscSignalProcessor::stateName(EscDriveState state) {
    switch (state) {
        case EscDriveState::FORWARD:       return "FORWARD";
        case EscDriveState::BRAKING:       return "BRAKING";
        case EscDriveState::REVERSE_ARMED: return "REVERSE_ARMED";
        case EscDriveState::REVERSE:       return "REVERSE";
        default:                           return "NEUTRAL";
    }
}

const char* EscSignalProcessor::typeName(EscType type) {
    switch (type) {
        case EscType::CRAWLER_FORWARD_REVERSE: return "crawler_forward_reverse";
        case EscType::FORWARD_BRAKE_ONLY:      return "forward_brake_only";
        default:                               return "forward_brake_reverse";
    }
}

EscType EscSignalProcessor::typeFromName(const char* name) {
    if (name == nullptr) return EscType::FORWARD_BRAKE_REVERSE;
    if (strcmp(name, "crawler_forward_reverse") == 0)
        return EscType::CRAWLER_FORWARD_REVERSE;
    if (strcmp(name, "forward_brake_only") == 0)
        return EscType::FORWARD_BRAKE_ONLY;
    return EscType::FORWARD_BRAKE_REVERSE;
}
