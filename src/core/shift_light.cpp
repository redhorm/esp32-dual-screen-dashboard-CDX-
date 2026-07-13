#include "shift_light.h"

namespace {
float minf(float lhs, float rhs) { return lhs < rhs ? lhs : rhs; }
float maxf(float lhs, float rhs) { return lhs > rhs ? lhs : rhs; }
}  // namespace

ShiftLightController::ShiftLightController(const ShiftLightConfig& config) {
    configure(config);
}
void ShiftLightController::configure(const ShiftLightConfig& config) {
    _config = config;
    if (_config.led_count == 0) _config.led_count = 1;
    if (_config.led_count > 32) _config.led_count = 32;
    if (_config.maximum_rpm <= _config.minimum_rpm)
        _config.maximum_rpm = _config.minimum_rpm + 1.0f;
    if (_config.hysteresis_rpm < 0.0f) _config.hysteresis_rpm = 0.0f;
    if (_config.max_rpm_per_second < 100.0f)
        _config.max_rpm_per_second = 100.0f;
    reset();
}

void ShiftLightController::reset() {
    _filtered_rpm = 0.0f;
    _lit_count = 0;
    _last_update_ms = 0;
    _last_step_ms = 0;
    _initialized = false;
}

float ShiftLightController::threshold(uint8_t count) const {
    return _config.minimum_rpm +
           (_config.maximum_rpm - _config.minimum_rpm) *
           static_cast<float>(count) / static_cast<float>(_config.led_count);
}

uint8_t ShiftLightController::directCount(float rpm) const {
    if (rpm <= _config.minimum_rpm) return 0;
    if (rpm >= _config.maximum_rpm) return _config.led_count;
    const float progress = (rpm - _config.minimum_rpm) /
                           (_config.maximum_rpm - _config.minimum_rpm);
    uint8_t result = static_cast<uint8_t>(progress * _config.led_count);
    if (result > _config.led_count) result = _config.led_count;
    return result;
}

uint8_t ShiftLightController::update(float rpm, uint32_t now_ms) {
    if (!_initialized) {
        _filtered_rpm = rpm;
        _lit_count = directCount(rpm);
        _last_update_ms = now_ms;
        _last_step_ms = now_ms;
        _initialized = true;
        return _lit_count;
    }

    const uint32_t dt_ms = static_cast<uint32_t>(now_ms - _last_update_ms);
    _last_update_ms = now_ms;
    if (dt_ms != 0) {
        const float alpha = _config.smoothing_ms == 0
            ? 1.0f
            : static_cast<float>(dt_ms) /
              static_cast<float>(dt_ms + _config.smoothing_ms);
        const float desired = _filtered_rpm + (rpm - _filtered_rpm) * alpha;
        const float max_delta = _config.max_rpm_per_second *
                                static_cast<float>(dt_ms) * 0.001f;
        const float delta = desired - _filtered_rpm;
        _filtered_rpm += maxf(-max_delta, minf(max_delta, delta));
    }

    if (static_cast<uint32_t>(now_ms - _last_step_ms) <
        _config.step_interval_ms)
        return _lit_count;

    if (_lit_count < _config.led_count &&
        _filtered_rpm >= threshold(static_cast<uint8_t>(_lit_count + 1U)) +
                         _config.hysteresis_rpm) {
        ++_lit_count;
        _last_step_ms = now_ms;
    } else if (_lit_count > 0 &&
               _filtered_rpm <= threshold(_lit_count) -
                                _config.hysteresis_rpm) {
        --_lit_count;
        _last_step_ms = now_ms;
    }
    return _lit_count;
}
