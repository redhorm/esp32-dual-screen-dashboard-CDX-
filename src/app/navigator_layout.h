#pragma once

#include "config.h"
#include "hal/copilot_display_profile.h"

struct CopilotRect {
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
};

struct CopilotLayoutDefinition {
    CopilotDisplayProfile profile;
    uint16_t width;
    uint16_t height;
    CopilotRect header;
    CopilotRect content;
    CopilotRect telemetry;
    CopilotRect footer;
};

// These rectangles describe the native composition contract. The landscape
// renderer below keeps its established constants; square renderers use this
// table to avoid a second, implicit scaling system.
static constexpr CopilotLayoutDefinition COPILOT_LAYOUTS[] = {
    {CopilotDisplayProfile::DISPLAY_240X240, 240, 240,
     {0, 0, 240, 29}, {0, 29, 168, 180}, {170, 29, 70, 180},
     {0, 209, 240, 31}},
    {CopilotDisplayProfile::DISPLAY_240X135_LANDSCAPE, 240, 135,
     {0, 0, 240, 18}, {0, 19, 168, 93}, {168, 19, 72, 93},
     {0, 112, 240, 23}},
    {CopilotDisplayProfile::DISPLAY_128X128, 128, 128,
     {0, 0, 128, 15}, {4, 23, 120, 57}, {0, 80, 128, 15},
     {0, 95, 128, 33}},
};

static constexpr size_t COPILOT_LAYOUT_COUNT =
    sizeof(COPILOT_LAYOUTS) / sizeof(COPILOT_LAYOUTS[0]);

inline const CopilotLayoutDefinition& copilotLayout(
    CopilotDisplayProfile profile) {
    const uint8_t index = static_cast<uint8_t>(profile);
    return index < COPILOT_LAYOUT_COUNT ? COPILOT_LAYOUTS[index]
        : COPILOT_LAYOUTS[1];
}

static_assert(COPILOT_LAYOUTS[0].content.x + COPILOT_LAYOUTS[0].content.width <= 240,
              "240x240 content out of bounds");
static_assert(COPILOT_LAYOUTS[0].footer.y + COPILOT_LAYOUTS[0].footer.height <= 240,
              "240x240 footer out of bounds");
static_assert(COPILOT_LAYOUTS[2].content.x + COPILOT_LAYOUTS[2].content.width <= 128,
              "128x128 content out of bounds");
static_assert(COPILOT_LAYOUTS[2].footer.y + COPILOT_LAYOUTS[2].footer.height <= 128,
              "128x128 footer out of bounds");

// Single source of truth for every code-native surface rendered on Display B.
// Coordinates are native 240×135 landscape pixels: no global scaling, crop or
// hidden 240×240 canvas is allowed.
namespace CopilotLayout {

static constexpr int WIDTH       = COPILOT_SCREEN_WIDTH;
static constexpr int HEIGHT      = COPILOT_SCREEN_HEIGHT;
static constexpr int MARGIN      = 4;
static constexpr int HEADER_H    = 18;
static constexpr int FOOTER_Y    = 112;
static constexpr int FOOTER_H    = HEIGHT - FOOTER_Y;

namespace Rally {
static constexpr int TABLE_HEADER_Y = 19;
static constexpr int TABLE_HEADER_H = 12;
static constexpr int BODY_Y         = 31;
static constexpr int ROW_H          = 27;
static constexpr int ROWS           = 3;
static constexpr int TABLE_W        = 168;
static constexpr int SIDE_X         = TABLE_W;
static constexpr int SIDE_CX        = 204;
static constexpr int COL_ARROW_X    = 39;
static constexpr int COL_INFO_X     = 70;
static constexpr int COL_TOTAL_X    = 137;
static constexpr int CENTER_DIST    = 19;
static constexpr int CENTER_ARROW   = 54;
static constexpr int CENTER_INFO    = 103;
static constexpr int CENTER_TOTAL   = 152;
}

namespace Map {
static constexpr int TOP_X       = 5;
static constexpr int TOP_Y       = 4;
static constexpr int TOP_W       = WIDTH - 10;
static constexpr int TOP_H       = 21;
static constexpr int CARD_X      = 5;
static constexpr int CARD_Y      = 88;
static constexpr int CARD_W      = WIDTH - 10;
static constexpr int CARD_H      = 43;
}

namespace Stock {
static constexpr int FRAME_X     = 2;
static constexpr int FRAME_Y     = 2;
static constexpr int FRAME_W     = WIDTH - 4;
static constexpr int FRAME_H     = HEIGHT - 4;
static constexpr int TOP_X       = 6;
static constexpr int TOP_Y       = 5;
static constexpr int TOP_W       = WIDTH - 12;
static constexpr int TOP_H       = 23;
static constexpr int MAP_Y       = 31;
static constexpr int MAP_H       = 72;
static constexpr int BOTTOM_Y    = 105;
static constexpr int BOTTOM_H    = 25;
}

static_assert(WIDTH == 240 && HEIGHT == 135,
              "Copilot renderer is engineered for native 240x135");
static_assert(Rally::BODY_Y + Rally::ROWS * Rally::ROW_H <= FOOTER_Y,
              "Rally rows must end before the footer");
static_assert(Map::CARD_Y + Map::CARD_H <= HEIGHT - MARGIN,
              "Map card must stay inside the visible panel");
static_assert(Stock::BOTTOM_Y + Stock::BOTTOM_H <= HEIGHT - MARGIN,
              "Stock footer must stay inside the visible panel");

}  // namespace CopilotLayout
