#ifndef NATIVE_BUILD
#include <stdio.h>
#include "scene_dashboard.h"
#include "scene_navigator.h"
#include "core/anim.h"
#include "core/ui_i18n.h"

namespace {
constexpr uint16_t BLACK=0x0000, WHITE=0xFFFF, NIGHT=0x080D;
constexpr uint16_t CYAN=0x67BF, MAGENTA=0xF81F, PURPLE=0x8012;
constexpr uint16_t ORANGE=0xFC40, RED=0xF986, LIME=0xCFE4;
constexpr uint16_t SAND=0xDE73, DUST=0x8A45, ROCK=0x632C;
constexpr uint16_t ROCK_EDGE=0xEF5D, ROAD=0x18E3, STEEL=0x3186;

void centered(lgfx::LGFX_Sprite& spr, const char* text, int x, int y,
              const lgfx::IFont* font, uint16_t color) {
    spr.setFont(font);
    spr.setTextSize(1);
    spr.setTextDatum(lgfx::MC_DATUM);
    spr.setTextColor(color);
    spr.drawString(text, x, y);
    spr.setTextDatum(lgfx::TL_DATUM);
}

uint32_t noise32(uint32_t x) {
    x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu;
    return x ^ (x >> 16);
}

const char* eventName(RockGameEvent event, UiLanguage language) {
    switch (event) {
        case RockGameEvent::DODGED:
            return uiText(language, "SUPERATO!", "CLEAR!");
        case RockGameEvent::NEAR_MISS:
            return uiText(language, "QUASI!", "NEAR MISS!");
        case RockGameEvent::HIT:
            return uiText(language, "IMPATTO!", "IMPACT!");
        default:                       return "";
    }
}

uint16_t eventColor(RockGameEvent event) {
    if (event == RockGameEvent::HIT) return RED;
    if (event == RockGameEvent::NEAR_MISS) return ORANGE;
    return LIME;
}

char gameRank(const RockGameSnapshot& game) {
    if (game.course_clear && game.lives == 3 && game.score >= 3300) return 'S';
    if (game.course_clear && game.lives >= 2 && game.score >= 2300) return 'A';
    if (game.course_clear) return 'B';
    return 'C';
}

void drawObstacle(lgfx::LGFX_Sprite& spr, const RockObstacleView& obstacle,
                  int x, int y, int scale) {
    if (scale < 2) scale = 2;
    switch (obstacle.kind) {
        case RockObstacleKind::LOG:
            spr.fillRoundRect(x-scale*2, y-scale/2, scale*4, scale,
                              scale/2, 0xA2C4);
            spr.drawRoundRect(x-scale*2, y-scale/2, scale*4, scale,
                              scale/2, ROCK_EDGE);
            spr.drawLine(x-scale, y-scale/2, x-scale, y+scale/2, BLACK);
            spr.drawLine(x+scale, y-scale/2, x+scale, y+scale/2, BLACK);
            break;
        case RockObstacleKind::RUT:
            spr.fillTriangle(x-scale*2, y+scale, x-scale, y-scale,
                             x, y+scale, ROCK);
            spr.fillTriangle(x, y+scale, x+scale, y-scale,
                             x+scale*2, y+scale, ROCK);
            spr.drawLine(x-scale*2, y+scale, x-scale, y-scale, MAGENTA);
            spr.drawLine(x-scale, y-scale, x, y+scale, MAGENTA);
            spr.drawLine(x, y+scale, x+scale, y-scale, CYAN);
            spr.drawLine(x+scale, y-scale, x+scale*2, y+scale, CYAN);
            break;
        default:
            spr.fillTriangle(x-scale*2, y+scale, x-scale, y-scale,
                             x+scale, y-scale/2, ROCK);
            spr.fillTriangle(x-scale*2, y+scale, x+scale, y-scale/2,
                             x+scale*2, y+scale, ROCK);
            spr.drawTriangle(x-scale*2, y+scale, x-scale, y-scale,
                             x+scale, y-scale/2, ROCK_EDGE);
            spr.drawTriangle(x-scale*2, y+scale, x+scale, y-scale/2,
                             x+scale*2, y+scale, ROCK_EDGE);
            spr.drawLine(x-scale, y-scale, x, y+scale, SAND);
            break;
    }
}

void projectObstacle(const RockObstacleView& obstacle, int center,
                     int horizon_y, int bottom_y, int road_half,
                     int& x, int& y, int& scale) {
    int distance = obstacle.distance > 1000 ? 1000 : obstacle.distance;
    int near = 1000 - distance;
    y = horizon_y + (bottom_y - horizon_y) * near / 1000;
    int half = 7 + (road_half - 7) * near / 1000;
    x = center + obstacle.line_x * half / 88;
    scale = 2 + near * 8 / 1000;
}

void drawDashWorld(lgfx::LGFX_Sprite& spr, const RockGameSnapshot& game,
                   uint16_t accent) {
    const uint32_t motion = game.world_distance;
    spr.fillScreen(NIGHT);

    // Pixel-sunset and desert silhouettes keep the arcade identity readable
    // even on the 160x80 pilot panel.
    spr.fillCircle(80, 27, 12, ORANGE);
    spr.fillCircle(80, 28, 9, MAGENTA);
    for (int y=20; y<34; y+=4) spr.drawFastHLine(69, y, 23, NIGHT);
    spr.fillTriangle(0, 39, 28, 24, 58, 39, BLACK);
    spr.fillTriangle(101, 39, 132, 23, 159, 39, BLACK);
    spr.drawLine(0, 39, 28, 24, PURPLE);
    spr.drawLine(101, 39, 132, 23, CYAN);
    spr.fillTriangle(80, 35, 9, 79, 151, 79, ROAD);
    spr.drawLine(80, 35, 9, 79, MAGENTA);
    spr.drawLine(80, 35, 151, 79, CYAN);

    for (int stripe=0; stripe<4; ++stripe) {
        int y = 42 + stripe*stripe*3 + (motion % 3u);
        int half = 6 + stripe*12;
        spr.drawFastHLine(80-half, y, half*2,
                          stripe&1 ? PURPLE : STEEL);
    }

    for (int i=static_cast<int>(game.visible_obstacles)-1; i>=0; --i) {
        int x, y, scale;
        projectObstacle(game.obstacles[i], 80, 36, 72, 59, x, y, scale);
        drawObstacle(spr, game.obstacles[i], x, y, scale);
    }

    const int car_x = 80 + game.line_x * 49 / RockLineGame::ROAD_LIMIT;
    spr.fillRoundRect(car_x-7, 65, 14, 11, 3, accent);
    spr.fillRect(car_x-4, 67, 8, 4, BLACK);
    spr.fillRect(car_x-8, 69, 2, 5, BLACK);
    spr.fillRect(car_x+6, 69, 2, 5, BLACK);
    spr.drawPixel(car_x-4, 75, WHITE);
    spr.drawPixel(car_x+4, 75, RED);
    if (game.throttle) {
        uint32_t n = noise32(motion + 17u);
        spr.drawPixel(car_x-3-(n&3u), 78, ORANGE);
        spr.drawPixel(car_x+2+((n>>3)&3u), 78, SAND);
    }
}

void drawNavWorld(lgfx::LGFX_Sprite& spr, const RockGameSnapshot& game,
                  uint16_t accent) {
    const uint32_t motion = game.world_distance;
    spr.fillScreen(NIGHT);
    for (int i=0; i<18; ++i) {
        uint32_t n = noise32(static_cast<uint32_t>(i)*113u + motion/7u);
        spr.drawPixel(3+(n%234u), 30+((n>>8)%54u), i&1 ? CYAN : WHITE);
    }
    spr.fillCircle(120, 78, 38, ORANGE);
    spr.fillCircle(120, 80, 31, MAGENTA);
    for (int y=52; y<111; y+=8) spr.drawFastHLine(83, y, 75, NIGHT);
    spr.fillTriangle(0, 112, 50, 63, 99, 112, BLACK);
    spr.fillTriangle(141, 112, 191, 61, 239, 112, BLACK);
    spr.drawLine(0, 112, 50, 63, PURPLE);
    spr.drawLine(141, 112, 191, 61, CYAN);
    spr.fillTriangle(120, 96, 7, 220, 233, 220, ROAD);
    spr.drawWideLine(120, 96, 7, 220, 3, MAGENTA);
    spr.drawWideLine(120, 96, 233, 220, 3, CYAN);

    for (int stripe=0; stripe<7; ++stripe) {
        int y = 108 + stripe*stripe*3 + (motion % 5u);
        int half = 8 + stripe*16;
        spr.drawFastHLine(120-half, y, half*2,
                          stripe&1 ? PURPLE : STEEL);
    }

    for (int i=static_cast<int>(game.visible_obstacles)-1; i>=0; --i) {
        int x, y, scale;
        projectObstacle(game.obstacles[i], 120, 98, 211, 105, x, y, scale);
        scale = 3 + (scale-2)*2;
        drawObstacle(spr, game.obstacles[i], x, y, scale);
        if (i == 0 && game.obstacles[i].distance < 180) {
            spr.drawCircle(x, y, scale*3, RED);
            spr.drawCircle(x, y, scale*3+2, ORANGE);
        }
    }

    const int car_x = 120 + game.line_x * 88 / RockLineGame::ROAD_LIMIT;
    spr.fillRoundRect(car_x-14, 184, 28, 32, 5, accent);
    spr.fillRect(car_x-9, 189, 18, 9, BLACK);
    spr.drawLine(car_x-10, 202, car_x+10, 202, WHITE);
    spr.fillCircle(car_x-12, 210, 4, BLACK);
    spr.fillCircle(car_x+12, 210, 4, BLACK);
    spr.fillCircle(car_x-8, 213, 2, RED);
    spr.fillCircle(car_x+8, 213, 2, RED);
    if (game.throttle) {
        for (int i=0; i<4; ++i) {
            uint32_t n=noise32(motion+static_cast<uint32_t>(i)*31u);
            spr.fillCircle(car_x-14+static_cast<int>(n%29u),
                           219+static_cast<int>((n>>8)%12u), 1, DUST);
        }
    }
}

void drawDashOverlay(lgfx::LGFX_Sprite& spr, const RockGameSnapshot& game,
                     UiLanguage language) {
    char text[28];
    if (game.status == RockGameStatus::ATTRACT ||
        game.status == RockGameStatus::IDLE) {
        spr.fillRoundRect(24, 36, 112, 27, 4, BLACK);
        centered(spr, "DESERT BLITZ", 80, 43, &lgfx::fonts::Font2, MAGENTA);
        if (Anim::pulse(game.phase_ms, 420))
            centered(spr, uiText(language, "PREMI START", "PRESS START"),
                     80, 56, &lgfx::fonts::Font0, WHITE);
    } else if (game.status == RockGameStatus::COUNTDOWN) {
        int index = game.phase_ms < 1800 ? game.phase_ms / 600 : 3;
        const char* label = index == 3 ? uiText(language, "VIA!", "GO!")
                                       : (index == 0 ? "3" :
                                          (index == 1 ? "2" : "1"));
        spr.fillRoundRect(58, 25, 44, 42, 5, BLACK);
        centered(spr, label, 80, 46,
                 index == 3 ? &lgfx::fonts::Font4 : &lgfx::fonts::Font6,
                 index == 3 ? LIME : WHITE);
    } else if (game.status == RockGameStatus::PAUSED) {
        spr.fillRoundRect(35, 29, 90, 35, 5, BLACK);
        centered(spr, uiText(language, "PAUSA", "PAUSED"),
                 80, 43, &lgfx::fonts::Font4, ORANGE);
        centered(spr, uiText(language, "RIPRENDI", "RESUME"),
                 80, 57, &lgfx::fonts::Font0, WHITE);
    } else if (game.status == RockGameStatus::CRASHED) {
        if (Anim::pulse(game.phase_ms, 85))
            spr.drawRoundRect(1, 12, 158, 67, 5, RED);
        centered(spr, uiText(language, "IMPATTO!", "IMPACT!"),
                 80, 45, &lgfx::fonts::Font4, WHITE);
    } else if (game.status == RockGameStatus::FINISHED) {
        spr.fillScreen(BLACK);
        centered(spr, game.course_clear
                 ? uiText(language, "TRACCIA OK", "TRAIL CLEAR")
                 : uiText(language, "FINE GIOCO", "GAME OVER"),
                 80, 22, &lgfx::fonts::Font2,
                 game.course_clear ? LIME : RED);
        snprintf(text, sizeof(text), "%06lu  %s %c",
                 static_cast<unsigned long>(game.score),
                 uiText(language, "GRADO", "RANK"), gameRank(game));
        centered(spr, text, 80, 46, &lgfx::fonts::Font2, WHITE);
        centered(spr, uiText(language, "PREMI START", "PRESS START"),
                 80, 68, &lgfx::fonts::Font0, MAGENTA);
    } else if (game.result_age_ms < 650u &&
               game.last_event != RockGameEvent::NONE) {
        centered(spr, eventName(game.last_event, language), 80, 45,
                 &lgfx::fonts::Font2, eventColor(game.last_event));
    }
}

void drawNavOverlay(lgfx::LGFX_Sprite& spr, const RockGameSnapshot& game,
                    const ExperienceFrame& experience,
                    const lgfx::IFont* medium,
                    const lgfx::IFont* speed,
                    UiLanguage language) {
    char text[36];
    if (game.status == RockGameStatus::ATTRACT ||
        game.status == RockGameStatus::IDLE) {
        spr.fillRoundRect(23, 84, 194, 91, 12, BLACK);
        spr.drawRoundRect(23, 84, 194, 91, 12, MAGENTA);
        centered(spr, "DESERT BLITZ", 120, 110, speed, MAGENTA);
        centered(spr, "OFFROAD ARCADE", 120, 134, medium, CYAN);
        if (Anim::pulse(game.phase_ms, 420))
            centered(spr, uiText(language, "PREMI START", "PRESS START"),
                     120, 158, medium, WHITE);
    } else if (game.status == RockGameStatus::COUNTDOWN) {
        int index = game.phase_ms < 1800 ? game.phase_ms / 600 : 3;
        const char* label = index == 3 ? uiText(language, "VIA!", "GO!")
                                       : (index == 0 ? "3" :
                                          (index == 1 ? "2" : "1"));
        spr.fillRoundRect(74, 76, 92, 105, 14, BLACK);
        spr.drawRoundRect(74, 76, 92, 105, 14, CYAN);
        centered(spr, label, 120, 126, speed,
                 index == 3 ? LIME : WHITE);
    } else if (game.status == RockGameStatus::PAUSED) {
        spr.fillRoundRect(35, 86, 170, 82, 12, BLACK);
        spr.drawRoundRect(35, 86, 170, 82, 12, ORANGE);
        centered(spr, uiText(language, "PAUSA", "PAUSED"),
                 120, 114, speed, ORANGE);
        centered(spr, uiText(language, "TOCCA RIPRENDI", "TAP RESUME"),
                 120, 146, medium, WHITE);
    } else if (game.status == RockGameStatus::CRASHED) {
        if (Anim::pulse(game.phase_ms, 85)) {
            spr.drawRoundRect(5, 33, 230, 181, 12, RED);
            spr.drawRoundRect(9, 37, 222, 173, 10, MAGENTA);
        }
        centered(spr, uiText(language, "IMPATTO!", "IMPACT!"),
                 120, 128, speed, WHITE);
    } else if (game.status == RockGameStatus::FINISHED) {
        spr.fillRoundRect(20, 58, 200, 132, 14, BLACK);
        spr.drawRoundRect(20, 58, 200, 132, 14,
                          game.course_clear ? LIME : RED);
        centered(spr, game.course_clear
                 ? uiText(language, "TRACCIA COMPLETA", "TRAIL CLEAR")
                 : uiText(language, "FINE GIOCO", "GAME OVER"),
                 120, 84, medium, game.course_clear ? LIME : RED);
        snprintf(text, sizeof(text), "%06lu",
                 static_cast<unsigned long>(game.score));
        centered(spr, text, 120, 123, speed, WHITE);
        snprintf(text, sizeof(text), "%s %c  %s %06lu",
                 uiText(language, "GRADO", "RANK"), gameRank(game),
                 uiText(language, "RECORD", "BEST"),
                 static_cast<unsigned long>(experience.identity.best_game_score));
        centered(spr, text, 120, 157, medium, MAGENTA);
        centered(spr, uiText(language, "PREMI START", "PRESS START"), 120, 178,
                 &lgfx::fonts::Font0, CYAN);
    } else if (game.result_age_ms < 700u &&
               game.last_event != RockGameEvent::NONE) {
        centered(spr, eventName(game.last_event, language), 120, 126, speed,
                 eventColor(game.last_event));
    }
}
}

// ---------------------------- 160x80 dashboard -----------------------------

bool SceneDashboard::_renderExperienceTakeover(const UiFx& fx) {
    (void)fx;
    if (_experience.mode == ExperienceMode::NORMAL) return false;

    const RockGameSnapshot& game = _experience.game;
    drawDashWorld(_spr, game, _experience.identity.accent);
    _spr.fillRect(0, 0, 160, 11, BLACK);
    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextColor(CYAN, BLACK);
    _spr.drawString("DESERT BLITZ", 3, 2);
    char text[20];
    snprintf(text, sizeof(text), "%06lu",
             static_cast<unsigned long>(game.score));
    _spr.setTextDatum(lgfx::TR_DATUM);
    _spr.setTextColor(LIME, BLACK);
    _spr.drawString(text, 157, 2);
    _spr.setTextDatum(lgfx::TL_DATUM);
    drawDashOverlay(_spr, game, _settings.language);
    return true;
}

// --------------------------- 240x240 navigator -----------------------------

bool SceneNavigator::_renderExperienceTakeover(const UiFx& fx) {
    (void)fx;
    if (_experience.mode == ExperienceMode::NORMAL) return false;

    const RockGameSnapshot& game = _experience.game;
    drawNavWorld(_spr, game, _experience.identity.accent);

    _spr.fillRect(0, 0, 240, 29, BLACK);
    _spr.setFont(&_fonts.med);
    _spr.setTextColor(CYAN, BLACK);
    _spr.drawString("DESERT BLITZ", 6, 4);
    char text[34];
    snprintf(text, sizeof(text), "%06lu",
             static_cast<unsigned long>(game.score));
    _spr.setTextDatum(lgfx::TR_DATUM);
    _spr.setTextColor(LIME, BLACK);
    _spr.drawString(text, 234, 4);
    _spr.setTextDatum(lgfx::TL_DATUM);

    _spr.fillRect(0, 220, 240, 20, BLACK);
    // Keep the status line short enough for the 240 px panel.
    if (_settings.language == UiLanguage::ITALIAN)
        snprintf(text, sizeof(text), "VITE %u  X%u  ROCCE %u/%u",
                 game.lives, game.combo, game.obstacle, game.total_obstacles);
    else
        snprintf(text, sizeof(text), "LIVES %u  X%u  ROCKS %u/%u",
                 game.lives, game.combo, game.obstacle, game.total_obstacles);
    centered(_spr, text, 120, 230, &lgfx::fonts::Font0, WHITE);
    drawNavOverlay(_spr, game, _experience, &_fonts.med, &_fonts.speed,
                   _settings.language);
    return true;
}
#endif
