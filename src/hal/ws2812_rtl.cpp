#include "ws2812_rtl.h"

#ifndef NATIVE_BUILD

Ws2812Rtl::Ws2812Rtl()
    : _strip(RTL_LED_COUNT, RTL_WS2812_PIN, NEO_GRB + NEO_KHZ800) {
    portMUX_INITIALIZE(&_mux);
}

void Ws2812Rtl::begin() {
    _strip.begin();
    _strip.setBrightness(_brightness);
    _strip.clear();
    _strip.show();
    _pending_valid = false;
    _failsafe_valid = false;
    _sent_valid = true;  // begin() has physically transmitted an all-off frame.
    _applied_brightness = _brightness;
    _last_update_ms = millis();
    for (uint16_t i = 0; i < RTL_LED_COUNT; ++i) _sent[i] = RtlColor{};

    BaseType_t created = xTaskCreatePinnedToCore(
        taskThunk, "rtl-watchdog", 3072, this, 2, &_task,
        ARDUINO_RUNNING_CORE);
    _ready = created == pdPASS;
    if (!_ready) {
        _task = nullptr;
        _strip.clear();
        _strip.show();
        Serial.println("[rtl] watchdog task failed; strip held off");
    }
}

bool Ws2812Rtl::update(const RtlFrame& frame) {
    if (!_ready) return false;

    RtlColor desired[RTL_LED_COUNT]{};
    const uint16_t copy_count = frame.count < RTL_LED_COUNT
        ? frame.count : RTL_LED_COUNT;
    for (uint16_t i = 0; i < copy_count; ++i) desired[i] = frame.pixels[i];

    RtlColor safe_pixels[RTL_LED_COUNT]{};
    const bool refresh_failsafe = !_failsafe_valid ||
                                  _failsafe_profile != frame.profile;
    if (refresh_failsafe) {
        RtlLogic safe_logic(frame.profile, RTL_LED_COUNT,
                            RTL_COMMAND_TIMEOUT_MS);
        const RtlFrame safe_frame = safe_logic.frame(0);
        for (uint16_t i = 0; i < RTL_LED_COUNT; ++i)
            safe_pixels[i] = safe_frame.pixels[i];
    }

    bool changed;
    taskENTER_CRITICAL(&_mux);
    changed = !_pending_valid || !samePixels(desired, _pending);
    if (changed) {
        copyPixels(_pending, desired);
        _pending_valid = true;
    }
    if (refresh_failsafe) {
        copyPixels(_failsafe, safe_pixels);
        _failsafe_profile = frame.profile;
        _failsafe_valid = true;
        changed = true;
    }
    _last_update_ms = millis();
    taskEXIT_CRITICAL(&_mux);

    if (changed && _task != nullptr) xTaskNotifyGive(_task);
    return changed;
}

bool Ws2812Rtl::service() {
    // Kept for source compatibility with the previous cooperative adapter.
    // The watchdog task wakes by itself every 20 ms; notifying it from every
    // Arduino loop iteration would waste CPU and could starve lower priorities.
    return false;
}

void Ws2812Rtl::setBrightness(uint8_t brightness) {
    bool changed = false;
    taskENTER_CRITICAL(&_mux);
    if (_brightness != brightness) {
        changed = true;
    }
    _brightness = brightness;
    taskEXIT_CRITICAL(&_mux);
    if (changed && _task != nullptr) xTaskNotifyGive(_task);
}

void Ws2812Rtl::taskThunk(void* context) {
    static_cast<Ws2812Rtl*>(context)->taskLoop();
}

void Ws2812Rtl::taskLoop() {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));

        RtlColor desired[RTL_LED_COUNT]{};
        uint8_t brightness;
        bool frame_valid = false;
        const uint32_t now = millis();

        taskENTER_CRITICAL(&_mux);
        const bool hardware_timed_out = _pending_valid && _failsafe_valid &&
            static_cast<uint32_t>(now - _last_update_ms) >
                RTL_COMMAND_TIMEOUT_MS;
        if (_pending_valid) {
            copyPixels(desired, hardware_timed_out ? _failsafe : _pending);
            frame_valid = true;
        }
        brightness = _brightness;
        taskEXIT_CRITICAL(&_mux);

        if (!frame_valid) continue;
        if (brightness != _applied_brightness) {
            _strip.setBrightness(brightness);
            _applied_brightness = brightness;
            // NeoPixel brightness mutates its internal buffer; rewrite the
            // next frame from the logical, unscaled RGB values.
            _sent_valid = false;
        }
        if (_sent_valid && samePixels(desired, _sent)) continue;
        if (!_strip.canShow()) continue;

        for (uint16_t i = 0; i < RTL_LED_COUNT; ++i) {
            const RtlColor& color = desired[i];
            _strip.setPixelColor(i, color.r, color.g, color.b);
        }
        _strip.show();
        copyPixels(_sent, desired);
        _sent_valid = true;
    }
}

bool Ws2812Rtl::samePixels(const RtlColor* lhs, const RtlColor* rhs) {
    for (uint16_t i = 0; i < RTL_LED_COUNT; ++i) {
        if (lhs[i] != rhs[i]) return false;
    }
    return true;
}

void Ws2812Rtl::copyPixels(RtlColor* dst, const RtlColor* src) {
    for (uint16_t i = 0; i < RTL_LED_COUNT; ++i) dst[i] = src[i];
}

#endif  // NATIVE_BUILD
