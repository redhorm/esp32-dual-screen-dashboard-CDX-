#pragma once
#include <stdint.h>
#include "config.h"

// Shared, trivially-copyable UI configuration.  It can safely travel in the
// same critical-section protected frame used by the two display tasks.
enum class UiTheme : uint8_t {
    CLASSIC_RALLY = 0,
    REALISM       = 1,
    STOCK_OFFROAD = 2,
    // ID 3 is reserved by the separate Pro Race branch.
    RECOIL_TFT    = 4,
    RECOIL_ALIGN  = 5,
};

enum class UiLanguage : uint8_t {
    ITALIAN = 0,
    ENGLISH = 1,
};

struct UiSettings {
    UiTheme driver_theme    = UiTheme::RECOIL_TFT;
    UiTheme navigator_theme = UiTheme::RECOIL_TFT;
    UiLanguage language     = UiLanguage::ITALIAN;
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
    return value == static_cast<uint8_t>(UiTheme::RECOIL_ALIGN)
        ? UiTheme::RECOIL_ALIGN : UiTheme::RECOIL_TFT;
}

inline UiLanguage sanitizeLanguage(uint8_t value) {
    return value <= static_cast<uint8_t>(UiLanguage::ENGLISH)
        ? static_cast<UiLanguage>(value)
        : UiLanguage::ITALIAN;
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
        case UiTheme::REALISM:       return "Realism";
        case UiTheme::STOCK_OFFROAD: return "Stock Offroad";
        case UiTheme::RECOIL_TFT:    return "Recoil TFT";
        case UiTheme::RECOIL_ALIGN:  return "Recoil Allineamento";
        default:                     return "Classic Rally";
    }
}

inline const char* languageName(UiLanguage language) {
    return language == UiLanguage::ENGLISH ? "English" : "Italiano";
}
