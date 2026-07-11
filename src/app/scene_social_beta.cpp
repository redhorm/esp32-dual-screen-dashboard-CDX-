#ifndef NATIVE_BUILD
#include <stdio.h>
#include "scene_dashboard.h"
#include "scene_navigator.h"
#include "core/anim.h"

namespace {
constexpr uint16_t BLACK = 0x0000, WHITE = 0xFFFF, LIME = 0xCFE4;
constexpr uint16_t AMBER = 0xFD03, CYAN = 0x67BF, RED = 0xF986;
constexpr uint16_t BLUE = 0x249F, SAND = 0xDE73, OLIVE = 0x3B86;

uint16_t momentColor(ShowProgram p) {
    switch (p) {
        case ShowProgram::WATER_FORD: return CYAN;
        case ShowProgram::NIGHT_RAID: return 0x0410;
        case ShowProgram::WINCH_PULL: return AMBER;
        case ShowProgram::SEND_IT:    return LIME;
        default:                      return SAND;
    }
}
uint16_t momentInk(ShowProgram p) {
    return p == ShowProgram::NIGHT_RAID ? CYAN : BLACK;
}
uint16_t calloutColor(HypeReaction cue, uint16_t accent) {
    if (cue == HypeReaction::STOP) return RED;
    if (cue == HypeReaction::WATER) return CYAN;
    if (cue == HypeReaction::CLEAR) return LIME;
    if (cue == HypeReaction::WINCH) return AMBER;
    return accent;
}
void centered(lgfx::LGFX_Sprite& spr, const char* text, int x, int y,
              const lgfx::IFont* font, uint16_t color) {
    spr.setFont(font); spr.setTextSize(1); spr.setTextDatum(lgfx::MC_DATUM);
    spr.setTextColor(color); spr.drawString(text, x, y);
    spr.setTextDatum(lgfx::TL_DATUM);
}
}

// ---------------------------- 160x80 dashboard -----------------------------

bool SceneDashboard::_renderBetaTakeover(const UiFx&) {
    if (_beta.mode == BetaMode::NORMAL) return false;

    if (_beta.mode == BetaMode::GAME) {
        const RockGameSnapshot& g = _beta.game;
        _spr.fillScreen(BLACK);
        _spr.fillRect(0, 0, 160, 10, _beta.identity.accent);
        _spr.setFont(&lgfx::fonts::Font0); _spr.setTextColor(BLACK, _beta.identity.accent);
        _spr.drawString("ROCK LINE RUSH", 3, 2);
        char b[20]; snprintf(b, sizeof(b), "%lu", (unsigned long)g.score);
        _spr.setTextDatum(lgfx::TR_DATUM); _spr.drawString(b, 157, 2);
        _spr.setTextDatum(lgfx::TL_DATUM);

        // Top-down trail, gate and Jeep.
        _spr.fillRect(2, 12, 108, 66, 0x2124);
        _spr.drawFastVLine(38, 12, 66, 0x630C);
        _spr.drawFastVLine(74, 12, 66, 0x630C);
        int gate_y = 15 + (int)((300 - (g.distance_to_gate > 300 ? 300 : g.distance_to_gate)) * 45 / 300);
        int gap_x = 56 + g.safe_x * 48 / 90;
        _spr.fillCircle(gap_x - 26, gate_y, 6, 0x8A45);
        _spr.fillCircle(gap_x + 26, gate_y, 6, 0x8A45);
        int car_x = 56 + g.line_x * 48 / 90;
        _spr.fillRoundRect(car_x - 5, 64, 10, 12, 2, _beta.identity.accent);
        _spr.drawRect(car_x - 3, 66, 6, 4, BLACK);

        _spr.fillRect(113, 12, 46, 66, 0x1082);
        _spr.setTextColor(WHITE, 0x1082); _spr.setFont(&lgfx::fonts::Font0);
        snprintf(b, sizeof(b), "GATE %u/%u", g.gate, g.total_gates); _spr.drawString(b, 116, 15);
        snprintf(b, sizeof(b), "LIFE %u", g.lives); _spr.drawString(b, 116, 27);
        snprintf(b, sizeof(b), "COMBO X%u", g.combo); _spr.drawString(b, 116, 39);
        snprintf(b, sizeof(b), "SPD %u", g.speed); _spr.drawString(b, 116, 51);
        _spr.drawRect(116, 65, 39, 6, 0x630C);
        _spr.fillRect(117, 66, g.progress_permille * 37 / 1000, 4, _beta.identity.accent);

        if (g.status == RockGameStatus::COUNTDOWN) {
            int n = 3 - (int)(g.phase_ms / 733);
            snprintf(b, sizeof(b), "%d", n < 1 ? 1 : n);
            centered(_spr, b, 56, 43, &lgfx::fonts::Font4, WHITE);
        } else if (g.status == RockGameStatus::CRASHED) {
            centered(_spr, "ROCK HIT!", 56, 43, &lgfx::fonts::Font2, RED);
        } else if (g.status == RockGameStatus::FINISHED) {
            _spr.fillRect(2, 12, 108, 66, BLACK);
            centered(_spr, g.lives ? "TRAIL CLEAR" : "GAME OVER", 56, 29,
                     &lgfx::fonts::Font2, g.lives ? LIME : RED);
            snprintf(b, sizeof(b), "%lu", (unsigned long)g.score);
            centered(_spr, b, 56, 53, &lgfx::fonts::Font4, WHITE);
        }
        return true;
    }

    if (_beta.mode == BetaMode::HYPE) {
        uint16_t bg = calloutColor(_beta.hype, _beta.identity.accent);
        bool flash = Anim::pulse(_beta.mode_ms, 260);
        _spr.fillScreen(flash ? bg : BLACK);
        uint16_t ink = flash ? BLACK : bg;
        _spr.drawRoundRect(2, 2, 156, 76, 6, ink);
        if (_beta.hype == HypeReaction::LEFT)
            _spr.fillTriangle(8,40,34,20,34,60,ink);
        if (_beta.hype == HypeReaction::RIGHT)
            _spr.fillTriangle(152,40,126,20,126,60,ink);
        centered(_spr, _beta.hype_text, 80, 39, &lgfx::fonts::Font4, ink);
        _spr.setFont(&lgfx::fonts::Font0); _spr.setTextColor(ink);
        _spr.drawString(_beta.identity.callsign, 5, 67);
        return true;
    }

    // Moment Director: a primitive-only animation that stays frame-locked
    // with the 240x240 scene because both use the same mode_ms.
    uint16_t bg = momentColor(_beta.show_program), ink = momentInk(_beta.show_program);
    _spr.fillScreen(bg);
    uint32_t t = _beta.mode_ms;
    if (_beta.show_program == ShowProgram::MUD_SPLASH) {
        for (int i=0;i<20;i++) {
            int x=(i*37+(t/13))%170-5, y=(i*19+(t/21))%86-3;
            _spr.fillCircle(x,y,2+(i%4),i%2?0x5142:0x8A45);
        }
    } else if (_beta.show_program == ShowProgram::WATER_FORD) {
        for(int y=8;y<80;y+=12) for(int x=0;x<160;x+=8)
            _spr.drawLine(x,y+(((x/8)&1)?3:-3),x+8,y+(((x/8)&1)?-3:3),WHITE);
    } else if (_beta.show_program == ShowProgram::NIGHT_RAID) {
        for(int x=0;x<160;x+=10)_spr.drawFastVLine(x,0,80,0x0208);
        _spr.fillTriangle((t/8)%180-20,0,(t/8)%180+5,80,(t/8)%180+30,0,CYAN);
    } else if (_beta.show_program == ShowProgram::WINCH_PULL) {
        for(int x=-20;x<180;x+=14)_spr.drawLine(x+(t/15)%14,0,x-28+(t/15)%14,80,ink);
    } else {
        int r=(t/8)%90; for(int i=0;i<4;i++)_spr.drawCircle(80,40,r+i*14,ink);
    }
    centered(_spr, SocialBetaEngine::showName(_beta.show_program), 80, 36,
             &lgfx::fonts::Font4, ink);
    _spr.fillRect(0,65,160,15,ink); _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextColor(bg,ink); _spr.drawString(_beta.identity.callsign,5,69);
    _spr.setTextDatum(lgfx::TR_DATUM); _spr.drawString(_beta.identity.number,155,69);
    _spr.setTextDatum(lgfx::TL_DATUM);
    return true;
}

void SceneDashboard::_drawBetaBadge() {
    if (_beta.latest_badge == TrailBadge::NONE || _beta.badge_age_ms > 2300) return;
    int y = 79 - (int)(Ease::outCubic(Anim::clamp01(_beta.badge_age_ms / 260.0f)) * 17);
    _spr.fillRoundRect(2,y,156,16,4,_beta.identity.accent);
    centered(_spr, SocialBetaEngine::badgeName(_beta.latest_badge), 80, y+8,
             &lgfx::fonts::Font2, BLACK);
}

// --------------------------- 240x240 navigator -----------------------------

bool SceneNavigator::_renderBetaTakeover(const UiFx&) {
    if (_beta.mode == BetaMode::NORMAL) return false;

    if (_beta.mode == BetaMode::GAME) {
        const RockGameSnapshot& g = _beta.game;
        _spr.fillScreen(0x0841);
        _spr.fillRect(0,0,240,28,BLACK);
        _spr.setFont(&_fonts.med); _spr.setTextColor(_beta.identity.accent,BLACK);
        _spr.drawString("ROCK LINE RUSH",7,5);
        char b[24]; snprintf(b,sizeof(b),"%lu",(unsigned long)g.score);
        _spr.setTextDatum(lgfx::TR_DATUM);_spr.setTextColor(WHITE);_spr.drawString(b,233,5);_spr.setTextDatum(lgfx::TL_DATUM);
        // Perspective trail.
        _spr.fillTriangle(120,32,18,220,222,220,0x31A5);
        for(int i=0;i<7;i++){int y=45+i*i*4;int half=8+i*14;_spr.drawFastHLine(120-half,y,half*2,0x73AE);}
        _spr.drawLine(120,32,18,220,0x9CD3);_spr.drawLine(120,32,222,220,0x9CD3);
        int rem=g.distance_to_gate>300?300:g.distance_to_gate;
        int gate_y=45+(300-rem)*150/300;
        int half=12+(gate_y-45)*90/150;
        int gate_x=120+g.safe_x*half/95;
        int rock=4+(gate_y-45)*10/150;
        _spr.fillSmoothCircle(gate_x-half/2,gate_y,rock,0x8A45);
        _spr.fillSmoothCircle(gate_x+half/2,gate_y,rock,0x8A45);
        int car_x=120+g.line_x*88/90;
        _spr.fillRoundRect(car_x-13,183,26,34,5,_beta.identity.accent);
        _spr.fillRect(car_x-9,189,18,9,BLACK);
        _spr.fillCircle(car_x-14,191,4,BLACK);_spr.fillCircle(car_x+14,191,4,BLACK);
        _spr.fillCircle(car_x-14,210,4,BLACK);_spr.fillCircle(car_x+14,210,4,BLACK);
        _spr.fillRect(0,220,240,20,BLACK);_spr.setFont(&lgfx::fonts::Font0);_spr.setTextColor(WHITE,BLACK);
        snprintf(b,sizeof(b),"LIVES %u   COMBO X%u   GATE %u/%u",g.lives,g.combo,g.gate,g.total_gates);centered(_spr,b,120,230,&lgfx::fonts::Font0,WHITE);
        if(g.status==RockGameStatus::COUNTDOWN){int n=3-(int)(g.phase_ms/733);snprintf(b,sizeof(b),"%d",n<1?1:n);centered(_spr,b,120,112,&_fonts.speed,WHITE);}
        else if(g.status==RockGameStatus::CRASHED)centered(_spr,"ROCK HIT!",120,112,&_fonts.speed,RED);
        else if(g.status==RockGameStatus::FINISHED){_spr.fillRoundRect(24,70,192,100,12,BLACK);centered(_spr,g.lives?"TRAIL CLEAR":"GAME OVER",120,94,&_fonts.med,g.lives?LIME:RED);snprintf(b,sizeof(b),"%lu",(unsigned long)g.score);centered(_spr,b,120,132,&_fonts.speed,WHITE);}
        return true;
    }

    if (_beta.mode == BetaMode::HYPE) {
        uint16_t bg=calloutColor(_beta.hype,_beta.identity.accent);
        bool flash=Anim::pulse(_beta.mode_ms,260);_spr.fillScreen(flash?bg:BLACK);uint16_t ink=flash?BLACK:bg;
        for(int r=104;r>30;r-=18)_spr.drawCircle(120,120,r,ink);
        if(_beta.hype==HypeReaction::LEFT)_spr.fillTriangle(18,120,83,64,83,176,ink);
        if(_beta.hype==HypeReaction::RIGHT)_spr.fillTriangle(222,120,157,64,157,176,ink);
        centered(_spr,_beta.hype_text,120,112,&_fonts.speed,ink);
        centered(_spr,_beta.identity.callsign,120,163,&_fonts.med,ink);
        return true;
    }

    uint16_t bg=momentColor(_beta.show_program),ink=momentInk(_beta.show_program);_spr.fillScreen(bg);uint32_t t=_beta.mode_ms;
    if(_beta.show_program==ShowProgram::MUD_SPLASH){for(int i=0;i<42;i++){int x=(i*71+t/11)%260-10,y=(i*43+t/17)%260-10;_spr.fillCircle(x,y,3+i%8,i%2?0x5142:0x8A45);}}
    else if(_beta.show_program==ShowProgram::WATER_FORD){for(int y=8;y<240;y+=16)for(int x=0;x<240;x+=10)_spr.drawLine(x,y+(((x/10)&1)?4:-4),x+10,y+(((x/10)&1)?-4:4),WHITE);}
    else if(_beta.show_program==ShowProgram::NIGHT_RAID){for(int x=0;x<240;x+=12)_spr.drawFastVLine(x,0,240,0x0208);for(int y=0;y<240;y+=12)_spr.drawFastHLine(0,y,240,0x0208);int x=(t/7)%300-30;_spr.fillTriangle(x,0,x+40,240,x+80,0,CYAN);}
    else if(_beta.show_program==ShowProgram::WINCH_PULL){for(int x=-80;x<300;x+=18)_spr.drawLine(x+(t/13)%18,0,x-90+(t/13)%18,240,ink);_spr.drawCircle(120,120,72,ink);_spr.drawCircle(120,120,58,ink);}
    else{int r=(t/7)%140;for(int i=0;i<6;i++)_spr.drawCircle(120,120,r+i*22,ink);}
    centered(_spr,SocialBetaEngine::showName(_beta.show_program),120,103,&_fonts.speed,ink);
    centered(_spr,_beta.identity.team,120,146,&_fonts.med,ink);
    _spr.fillRect(0,207,240,33,ink);_spr.setTextColor(bg,ink);_spr.setFont(&_fonts.med);_spr.drawString(_beta.identity.callsign,7,214);_spr.setTextDatum(lgfx::TR_DATUM);_spr.drawString(_beta.identity.number,233,214);_spr.setTextDatum(lgfx::TL_DATUM);
    return true;
}

void SceneNavigator::_drawBetaBadge() {
    if(_beta.latest_badge==TrailBadge::NONE||_beta.badge_age_ms>2300)return;
    float q=Ease::outCubic(Anim::clamp01(_beta.badge_age_ms/300.0f));int y=-34+(int)(q*60);
    _spr.fillRoundRect(20,y,200,32,8,_beta.identity.accent);
    centered(_spr,SocialBetaEngine::badgeName(_beta.latest_badge),120,y+16,&_fonts.med,BLACK);
}
#endif
