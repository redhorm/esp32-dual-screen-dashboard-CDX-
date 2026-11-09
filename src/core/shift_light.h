#pragma once

#include <stdint.h>

struct ShiftLightConfig {
    float minimum_rpm = 700.0f;
    float maximum_rpm = 7200.0f;
    uint8_t led_count = 10;
    float hysteresis_rpm = 90.0f;
    uint16_t smoothing_ms = 90;
    float max_rpm_per_second = 16000.0f;
    uint16_t step_interval_ms = 45;
};
// Stable quantizer for a physical-looking shift strip.  Input filtering keeps
// simulated/real sensor jitter from crossing a boundary every frame;
// hysteresis and the one-step rate limiter make the visible result monotonic.
class ShiftLightController {
public:
    explicit ShiftLightController(const ShiftLightConfig& config = ShiftLightConfig());

    void configure(const ShiftLightConfig& config);
    void reset();
    uint8_t update(float rpm, uint32_t now_ms);

    uint8_t litCount() const { return _lit_count; }
    float filteredRpm() const { return _filtered_rpm; }
    const ShiftLightConfig& config() const { return _config; }

private:
    ShiftLightConfig _config;
    float _filtered_rpm = 0.0f;
    uint8_t _lit_count = 0;
    uint32_t _last_update_ms = 0;
    uint32_t _last_step_ms = 0;
    bool _initialized = false;

    float threshold(uint8_t count) const;
    uint8_t directCount(float rpm) const;
};
