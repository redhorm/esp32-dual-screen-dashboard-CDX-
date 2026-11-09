#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

// The copilot connector uses the same SPI pins for the three supported
// modules.  Only the panel/controller geometry changes, so the active profile
// is selected once before display initialisation and never changed live.
enum class CopilotDisplayProfile : uint8_t {
    DISPLAY_240X240 = 0,
    DISPLAY_240X135_LANDSCAPE = 1,
    DISPLAY_128X128 = 2,
};

enum class CopilotDisplayController : uint8_t {
    ST7789V = 0,
    ST7735 = 1,
};

struct CopilotDisplayConfig {
    CopilotDisplayProfile profile;
    CopilotDisplayController controller;
    const char* id;
    const char* label;
    uint16_t physical_width;
    uint16_t physical_height;
    uint16_t logical_width;
    uint16_t logical_height;
    uint16_t memory_width;
    uint16_t memory_height;
    uint16_t offset_x;
    uint16_t offset_y;
    uint8_t rotation;
    bool invert;
    bool rgb_order;
};

// ST7735 128×128 modules use the 132×132 address window with a 2,1 visible
// offset.  The MRD 135×240 module is the known-good ST7789V profile from the
// previous beta.  The original square ST7789 panel remains available at its
// original rotation and offsets.
static constexpr CopilotDisplayConfig COPILOT_DISPLAY_CONFIGS[] = {
    {CopilotDisplayProfile::DISPLAY_240X240,
     CopilotDisplayController::ST7789V,
     "240x240", "240×240 quadrato", 240, 240, 240, 240,
     240, 320, 0, 0, 0, true, false},
    {CopilotDisplayProfile::DISPLAY_240X135_LANDSCAPE,
     CopilotDisplayController::ST7789V,
     "240x135_landscape", "135×240 orizzontale · area 240×135",
     135, 240, 240, 135, 240, 320, 52, 40, 1, true, false},
    {CopilotDisplayProfile::DISPLAY_128X128,
     CopilotDisplayController::ST7735,
     "128x128", "128×128 quadrato", 128, 128, 128, 128,
     132, 132, 2, 1, 0, false, false},
};

static constexpr size_t COPILOT_DISPLAY_CONFIG_COUNT =
    sizeof(COPILOT_DISPLAY_CONFIGS) / sizeof(COPILOT_DISPLAY_CONFIGS[0]);

inline constexpr CopilotDisplayProfile copilotDisplayDefaultProfile() {
    return CopilotDisplayProfile::DISPLAY_240X135_LANDSCAPE;
}

inline const CopilotDisplayConfig& copilotDisplayConfig(
    CopilotDisplayProfile profile) {
    const uint8_t index = static_cast<uint8_t>(profile);
    return index < COPILOT_DISPLAY_CONFIG_COUNT
        ? COPILOT_DISPLAY_CONFIGS[index]
        : COPILOT_DISPLAY_CONFIGS[static_cast<uint8_t>(
              copilotDisplayDefaultProfile())];
}

inline bool copilotDisplayProfileFromId(
    const char* id, CopilotDisplayProfile& profile) {
    if (!id || !id[0]) return false;
    for (size_t index = 0; index < COPILOT_DISPLAY_CONFIG_COUNT; ++index) {
        if (strcmp(id, COPILOT_DISPLAY_CONFIGS[index].id) == 0) {
            profile = COPILOT_DISPLAY_CONFIGS[index].profile;
            return true;
        }
    }
    return false;
}

inline const char* copilotDisplayControllerName(
    CopilotDisplayController controller) {
    return controller == CopilotDisplayController::ST7735
        ? "ST7735" : "ST7789V";
}
