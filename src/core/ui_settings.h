#pragma once
#include <stdint.h>
#include "config.h"

// Shared, trivially-copyable UI configuration.  It can safely travel in the
// same critical-section protected frame used by the two display tasks.
enum class UiTheme : uint8_t {
    CLASSIC_RALLY = 0,
    BAJA_AMBER    = 1,
    NIGHT_OPS     = 2,
    TRAIL_TOPO    = 3,
};

struct UiSettings {
    UiTheme driver_theme    = UiTheme::CLASSIC_RALLY;
    UiTheme navigator_theme = UiTheme::CLASSIC_RALLY;
    bool    use_kmh         = USE_KMH;
    bool    use_celsius     = USE_CELSIUS;
    uint8_t driver_brightness    = 220;
    uint8_t navigator_brightness = 220;
    bool     boot_image_enabled  = true;
    uint16_t boot_hold_ms        = 1800;
    bool     photo_mode_enabled  = false;
    uint8_t  photo_target        = 3;     // bit 0=driver, bit 1=navigator
    uint16_t photo_interval_s    = 12;
    uint8_t  photo_transition    = 1;     // 0=cut, 1=dissolve
    bool     photo_random        = false;
    bool     photo_overlay       = true;
};

inline UiTheme sanitizeTheme(uint8_t value) {
    return value <= static_cast<uint8_t>(UiTheme::TRAIL_TOPO)
        ? static_cast<UiTheme>(value)
        : UiTheme::CLASSIC_RALLY;
}

inline uint8_t sanitizeBrightness(int value) {
    if (value < 32)  return 32;
    if (value > 255) return 255;
    return static_cast<uint8_t>(value);
}

inline uint16_t clampU16(int value, int lo, int hi) {
    if (value < lo) return static_cast<uint16_t>(lo);
    if (value > hi) return static_cast<uint16_t>(hi);
    return static_cast<uint16_t>(value);
}

inline const char* themeName(UiTheme theme) {
    switch (theme) {
        case UiTheme::BAJA_AMBER: return "Baja Amber";
        case UiTheme::NIGHT_OPS:  return "Night Ops";
        case UiTheme::TRAIL_TOPO: return "Trail Topo";
        default:                  return "Classic Rally";
    }
}
