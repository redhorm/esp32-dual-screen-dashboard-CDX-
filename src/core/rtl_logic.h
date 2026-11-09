#pragma once

#include <stdint.h>

#include "config.h"
#include "core/esc_signal.h"

// Pure, host-testable state and rendering logic for a segmented rear light
// bar.  This layer knows nothing about Arduino, GPIO or a specific LED driver.

constexpr uint16_t RTL_LOGIC_DEFAULT_PIXELS = RTL_LED_COUNT;
constexpr uint16_t RTL_LOGIC_MIN_PIXELS = 5;
constexpr uint16_t RTL_LOGIC_MAX_PIXELS = 64;
constexpr uint32_t RTL_LOGIC_DEFAULT_TIMEOUT_MS = RTL_COMMAND_TIMEOUT_MS;

enum class RtlProfile : uint8_t {
    RTL_S,
    ULTRA4_4400,
};

enum class RtlFunction : uint8_t {
    RUNNING,
    BRAKE,
    LEFT,
    RIGHT,
    HAZARD,
    REVERSE_WORK,
    DUST_CHASE,
    PIT_TEST,
};

struct RtlColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;

    // Keep this type usable in constant expressions on the ESP32 toolchain,
    // which still compiles the firmware as C++11.  The desktop tests use a
    // newer standard and would otherwise hide this constructor requirement.
    constexpr RtlColor(uint8_t red = 0, uint8_t green = 0,
                       uint8_t blue = 0)
        : r(red), g(green), b(blue) {}

    constexpr bool operator==(const RtlColor& rhs) const {
        return r == rhs.r && g == rhs.g && b == rhs.b;
    }
    constexpr bool operator!=(const RtlColor& rhs) const {
        return !(*this == rhs);
    }
};

// Signals may be combined.  For example RUNNING + BRAKE + LEFT keeps the
// tail light active, raises the red brake zones and flashes the left amber
// zone.  The optional ESC interpreter maps an explicit drive state through
// stateForEsc(); it never mistakes the first braking pulse for reverse.
struct RtlCommandState {
    bool running = true;
    bool brake = false;
    bool left = false;
    bool right = false;
    bool hazard = false;
    bool reverse_work = false;
    bool dust_chase = false;
    bool pit_test = false;

    constexpr bool operator==(const RtlCommandState& rhs) const {
        return running == rhs.running && brake == rhs.brake &&
               left == rhs.left && right == rhs.right &&
               hazard == rhs.hazard &&
               reverse_work == rhs.reverse_work &&
               dust_chase == rhs.dust_chase && pit_test == rhs.pit_test;
    }
    constexpr bool operator!=(const RtlCommandState& rhs) const {
        return !(*this == rhs);
    }
};

struct RtlFrame {
    uint16_t count = 0;
    RtlColor pixels[RTL_LOGIC_MAX_PIXELS]{};
    RtlProfile profile = RtlProfile::RTL_S;
    RtlFunction primary = RtlFunction::RUNNING;
    bool command_timed_out = true;

    bool operator==(const RtlFrame& rhs) const;
    bool operator!=(const RtlFrame& rhs) const { return !(*this == rhs); }
};

class RtlLogic {
public:
    explicit RtlLogic(
        RtlProfile profile = RtlProfile::RTL_S,
        uint16_t led_count = RTL_LOGIC_DEFAULT_PIXELS,
        uint32_t command_timeout_ms = RTL_LOGIC_DEFAULT_TIMEOUT_MS);

    void configure(RtlProfile profile, uint16_t led_count,
                   uint32_t command_timeout_ms);

    // A submit is also the command keepalive.  If it is not refreshed before
    // the timeout, rendering falls back to a conservative running-light frame.
    void submit(const RtlCommandState& state, uint32_t now_ms);
    void activate(RtlFunction function, uint32_t now_ms);
    void clearCommand();

    RtlFrame frame(uint32_t now_ms) const;
    bool commandTimedOut(uint32_t now_ms) const;

    RtlProfile profile() const { return _profile; }
    uint16_t ledCount() const { return _led_count; }
    uint32_t commandTimeoutMs() const { return _command_timeout_ms; }
    const RtlCommandState& commandState() const { return _command; }

    static RtlCommandState stateFor(RtlFunction function);
    static RtlCommandState stateForEsc(EscDriveState state);
    static RtlCommandState mergeEsc(const RtlCommandState& base,
                                    EscDriveState state);
    static const char* functionName(RtlFunction function);
    static const char* profileName(RtlProfile profile);

private:
    RtlProfile _profile = RtlProfile::RTL_S;
    uint16_t _led_count = RTL_LOGIC_DEFAULT_PIXELS;
    uint32_t _command_timeout_ms = RTL_LOGIC_DEFAULT_TIMEOUT_MS;
    RtlCommandState _command;
    uint32_t _last_command_ms = 0;
    uint32_t _command_started_ms = 0;
    bool _has_command = false;

    static uint16_t clampLedCount(uint16_t count);
    static RtlCommandState failSafeState();
};
