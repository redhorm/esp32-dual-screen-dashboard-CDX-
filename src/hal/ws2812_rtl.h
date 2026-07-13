#pragma once

#ifndef NATIVE_BUILD

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <stdint.h>

#include "config.h"
#include "core/rtl_logic.h"

// Hardware adapter for the pure RtlLogic frame.  update() only queues pixels;
// a small higher-priority FreeRTOS task owns the strip and enforces a second,
// hardware-level timeout.  A slow multipart upload therefore cannot freeze a
// transient BRAKE/HAZARD/PIT frame on the physical LEDs.
class Ws2812Rtl {
public:
    Ws2812Rtl();

    void begin();
    bool update(const RtlFrame& frame);
    bool service();
    void setBrightness(uint8_t brightness);

    bool ready() const { return _ready; }
    uint8_t brightness() const { return _brightness; }
    uint16_t ledCount() const { return RTL_LED_COUNT; }

private:
    Adafruit_NeoPixel _strip;
    RtlColor _pending[RTL_LED_COUNT]{};
    RtlColor _failsafe[RTL_LED_COUNT]{};
    RtlColor _sent[RTL_LED_COUNT]{};
    portMUX_TYPE _mux;
    TaskHandle_t _task = nullptr;
    volatile bool _ready = false;
    bool _pending_valid = false;
    bool _failsafe_valid = false;
    bool _sent_valid = false;
    uint8_t _brightness = RTL_LED_BRIGHTNESS;
    uint8_t _applied_brightness = RTL_LED_BRIGHTNESS;
    RtlProfile _failsafe_profile = RtlProfile::RTL_S;
    uint32_t _last_update_ms = 0;

    static bool samePixels(const RtlColor* lhs, const RtlColor* rhs);
    static void copyPixels(RtlColor* dst, const RtlColor* src);
    static void taskThunk(void* context);
    void taskLoop();
};

#endif  // NATIVE_BUILD
