#ifndef NATIVE_BUILD
#include <stdio.h>
#include "scene_dashboard.h"
#include "scene_navigator.h"
#include "core/anim.h"

namespace {
constexpr uint16_t BLACK=0x0000, WHITE=0xFFFF, LIME=0xCFE4;
constexpr uint16_t CYAN=0x67BF, MAGENTA=0xF81F, ORANGE=0xFC40;
constexpr uint16_t RED=0xF986, PURPLE=0x8012, NAVY=0x080C, ROAD=0x18E3;

void centered(lgfx::LGFX_Sprite& spr, const char* text, int x, int y,
              const lgfx::IFont* font, uint16_t color) {
    spr.setFont(font); spr.setTextSize(1); spr.setTextDatum(lgfx::MC_DATUM);
    spr.setTextColor(color); spr.drawString(text,x,y); spr.setTextDatum(lgfx::TL_DATUM);
}

void centeredFit(lgfx::LGFX_Sprite& spr, const char* text, int x, int y,
                 int max_width, const lgfx::IFont* primary,
                 const lgfx::IFont* secondary, const lgfx::IFont* compact,
                 uint16_t color) {
    const lgfx::IFont* font=primary;
    spr.setFont(font);
    if(spr.textWidth(text)>max_width){font=secondary;spr.setFont(font);}
    if(spr.textWidth(text)>max_width)font=compact;
    centered(spr,text,x,y,font,color);
}

int crossoverProgress(const RiftFrame& r, bool navigator) {
    uint32_t start=6100,duration=2100;
    if(r.layout==0){start=navigator?7500:6800;duration=navigator?700:700;}
    else if(r.layout==1){start=navigator?6100:7500;duration=700;}
    if(r.elapsed_ms<=start)return 0;
    if(r.elapsed_ms>=start+duration)return 1000;
    return (int)((r.elapsed_ms-start)*1000UL/duration);
}

uint32_t noise32(uint32_t x) {
    x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu;
    return x ^ (x >> 16);
}

const char* resultName(GateResult r) {
    if (r == GateResult::PERFECT) return "PERFECT!";
    if (r == GateResult::CLEAN) return "CLEAN LINE";
    if (r == GateResult::HIT) return "WIPEOUT!";
    return "";
}

char gameRank(const RockGameSnapshot& g) {
    if (g.lives == 3 && g.score >= 3000) return 'S';
    if (g.lives >= 2 && g.score >= 1900) return 'A';
    if (g.lives && g.score >= 900) return 'B';
    return 'C';
}

void drawDashArcadeRoad(lgfx::LGFX_Sprite& s, const RockGameSnapshot& g,
                        uint16_t accent) {
    uint32_t motion=g.status==RockGameStatus::ATTRACT?g.phase_ms/5:g.world_distance;
    s.fillScreen(NAVY);
    for (int i=0;i<12;i++) {
        uint32_t n=noise32((uint32_t)i*71u+17u);
        s.drawPixel(3+(n%154),11+((n>>8)%23),i&1?CYAN:WHITE);
    }
    s.fillCircle(80,30,14,ORANGE); s.fillCircle(80,31,11,MAGENTA);
    for(int y=24;y<39;y+=4)s.drawFastHLine(66,y,29,NAVY);
    s.fillTriangle(0,40,30,25,58,40,BLACK); s.drawLine(0,40,30,25,PURPLE);
    s.fillTriangle(102,40,132,24,159,40,BLACK); s.drawLine(102,40,132,24,CYAN);
    int curve=g.safe_x/6;
    s.fillTriangle(80+curve,37,11,79,149,79,ROAD);
    s.drawLine(80+curve,37,11,79,MAGENTA); s.drawLine(80+curve,37,149,79,CYAN);
    for(int i=0;i<5;i++) {
        int y=42+i*i*2+(motion%3), half=7+i*14;
        s.drawFastHLine(80+curve-half,y,half*2,(i&1)?PURPLE:CYAN);
    }
    int car=80+g.line_x*47/90;
    if(g.status==RockGameStatus::ATTRACT){int wave=(g.phase_ms/35)%80;if(wave>40)wave=80-wave;car+=wave-20;}
    s.fillRoundRect(car-7,64,14,12,3,accent); s.fillRect(car-4,66,8,4,BLACK);
    s.drawPixel(car-5,74,WHITE); s.drawPixel(car+5,74,RED);
}

void drawNavArcadeRoad(lgfx::LGFX_Sprite& s, const RockGameSnapshot& g,
                       uint16_t accent) {
    uint32_t motion=g.status==RockGameStatus::ATTRACT?g.phase_ms/5:g.world_distance;
    s.fillScreen(NAVY);
    for(int i=0;i<20;i++) {
        uint32_t n=noise32((uint32_t)i*131u+motion/8u);
        s.fillCircle(4+(n%232),30+((n>>8)%58),1,i&1?CYAN:WHITE);
    }
    s.fillCircle(120,76,42,ORANGE); s.fillCircle(120,78,35,MAGENTA);
    for(int y=50;y<112;y+=9)s.drawFastHLine(79,y,83,NAVY);
    s.fillTriangle(0,112,48,67,95,112,BLACK); s.drawLine(0,112,48,67,PURPLE);
    s.drawLine(48,67,95,112,MAGENTA);
    s.fillTriangle(145,112,192,64,239,112,BLACK); s.drawLine(145,112,192,64,CYAN);
    s.drawLine(192,64,239,112,PURPLE);
    int curve=g.safe_x/3;
    s.fillTriangle(120+curve,100,8,220,232,220,ROAD);
    s.drawLine(120+curve,100,8,220,MAGENTA); s.drawLine(120+curve,100,232,220,CYAN);
    for(int i=0;i<7;i++) {
        int y=111+i*i*3+(motion%5), half=10+i*16;
        s.drawFastHLine(120+curve-half,y,half*2,(i&1)?PURPLE:CYAN);
    }
    int rem=g.distance_to_gate>300?300:g.distance_to_gate;
    int gy=112+(300-rem)*76/300, half=18+(gy-112);
    int gx=120+g.safe_x*half/100, rock=3+(gy-112)/10;
    s.drawCircle(gx-half/2,gy,rock,MAGENTA); s.drawCircle(gx+half/2,gy,rock,CYAN);
    s.drawLine(gx-half/2-rock,gy,gx-half/2,gy-rock,ORANGE);
    s.drawLine(gx+half/2+rock,gy,gx+half/2,gy-rock,ORANGE);
    int car=120+g.line_x*82/90;
    if(g.status==RockGameStatus::ATTRACT){int wave=(g.phase_ms/35)%100;if(wave>50)wave=100-wave;car+=wave-25;}
    s.fillRoundRect(car-14,185,28,32,5,accent); s.fillRect(car-9,190,18,8,BLACK);
    s.drawLine(car-10,202,car+10,202,WHITE); s.fillCircle(car-11,211,4,BLACK);
    s.fillCircle(car+11,211,4,BLACK); s.fillCircle(car-8,214,2,RED); s.fillCircle(car+8,214,2,RED);
}
}

// ---------------------------- 160x80 dashboard -----------------------------

bool SceneDashboard::_renderExperienceTakeover(const UiFx& fx) {
    if (_experience.mode == ExperienceMode::NORMAL) return false;

    if (_experience.mode == ExperienceMode::GAME) {
        const RockGameSnapshot& g=_experience.game;
        drawDashArcadeRoad(_spr,g,_experience.identity.accent);
        _spr.fillRect(0,0,160,10,BLACK); _spr.setFont(&lgfx::fonts::Font0);
        _spr.setTextColor(CYAN,BLACK); _spr.drawString("NEON TRAIL 1986",3,2);
        char b[24]; snprintf(b,sizeof(b),"%06lu",(unsigned long)g.score);
        _spr.setTextDatum(lgfx::TR_DATUM); _spr.setTextColor(LIME,BLACK); _spr.drawString(b,157,2);
        _spr.setTextDatum(lgfx::TL_DATUM);

        if(g.status==RockGameStatus::ATTRACT) {
            if(Anim::pulse(g.phase_ms,450)) centered(_spr,"INSERT COIN",80,49,&lgfx::fonts::Font2,WHITE);
            _spr.setFont(&lgfx::fonts::Font0);_spr.setTextColor(MAGENTA);_spr.drawString("CREDIT 00",4,68);
        } else if(g.status==RockGameStatus::COUNTDOWN) {
            const char* labels[]={"3","2","1","GO!"}; int ix=g.phase_ms<2700?g.phase_ms/900:3;
            centered(_spr,labels[ix],80,47,ix==3?&lgfx::fonts::Font4:&lgfx::fonts::Font6,WHITE);
        } else if(g.status==RockGameStatus::CRASHED) {
            if(Anim::pulse(g.phase_ms,90))_spr.fillScreen(MAGENTA);
            centered(_spr,"WIPEOUT!",80,44,&lgfx::fonts::Font4,WHITE);
        } else if(g.status==RockGameStatus::FINISHED) {
            _spr.fillScreen(BLACK); centered(_spr,g.lives?"COURSE CLEAR":"GAME OVER",80,23,&lgfx::fonts::Font2,g.lives?LIME:RED);
            snprintf(b,sizeof(b),"%06lu  RANK %c",(unsigned long)g.score,gameRank(g));centered(_spr,b,80,49,&lgfx::fonts::Font2,WHITE);
            centered(_spr,"INSERT COIN",80,69,&lgfx::fonts::Font0,MAGENTA);
        } else if(g.result_age_ms<850 && g.last_result!=GateResult::NONE) {
            centered(_spr,resultName(g.last_result),80,45,&lgfx::fonts::Font2,g.last_result==GateResult::HIT?RED:LIME);
        }
        return true;
    }

    const RiftFrame& r=_experience.rift; uint32_t t=r.elapsed_ms;
    if(r.phase==RiftPhase::ARMED) {
        _spr.fillScreen(BLACK);_spr.drawRoundRect(2,2,156,76,6,MAGENTA);
        centered(_spr,"RIFT//RUN",80,27,&lgfx::fonts::Font4,WHITE);
        uint32_t remain=(int32_t)(r.start_at_ms-fx.now_ms)>0?r.start_at_ms-fx.now_ms:0;
        char b[20];snprintf(b,sizeof(b),"SYNC IN %lu.%lus",(unsigned long)(remain/1000),(unsigned long)((remain%1000)/100));
        centered(_spr,b,80,58,&lgfx::fonts::Font2,CYAN);return true;
    }
    if(r.phase==RiftPhase::BLACKOUT) {
        _spr.fillScreen(BLACK);int y=(t/9)%80;_spr.drawFastHLine(0,y,160,CYAN);centered(_spr,"LINK 01",80,40,&lgfx::fonts::Font2,MAGENTA);
    } else if(r.phase==RiftPhase::SCAN) {
        _spr.fillScreen(NAVY);for(int q=8;q<80;q+=13)_spr.drawCircle(80,40,(q+(t/25))%78,CYAN);
        int y=(t/7)%80;_spr.drawFastHLine(0,y,160,LIME);centered(_spr,"TERRAIN SCAN",80,40,&lgfx::fonts::Font2,WHITE);
    } else if(r.phase==RiftPhase::CHARGE) {
        _spr.fillScreen(BLACK);for(int i=0;i<18;i++){int h=6+(int)((t/18+i*17)%48);_spr.fillRect(3+i*9,74-h,6,h,i<13?CYAN:MAGENTA);}
        centered(_spr,"CHARGE",80,19,&lgfx::fonts::Font2,WHITE);
    } else if(r.phase==RiftPhase::CROSSOVER) {
        _spr.fillScreen(NAVY);for(int i=0;i<10;i++){int x=(i*31+t/5)%180-10;_spr.drawFastHLine(x,15+i*6,18,CYAN);}
        int p=crossoverProgress(r,false)*200/1000-20;_spr.fillRoundRect(p-12,31,24,24,5,_experience.identity.accent);_spr.fillCircle(p-9,56,5,BLACK);_spr.fillCircle(p+9,56,5,BLACK);
        centered(_spr,"CROSS SCREEN",80,13,&lgfx::fonts::Font0,WHITE);
    } else if(r.phase==RiftPhase::IMPACT) {
        bool flash=Anim::pulse(t,80);_spr.fillScreen(flash?WHITE:MAGENTA);centered(_spr,"RIFT OPEN",80,40,&lgfx::fonts::Font4,flash?BLACK:WHITE);
    } else if(r.phase==RiftPhase::HERO) {
        _spr.fillScreen(BLACK);_spr.drawRoundRect(2,2,156,76,6,_experience.identity.accent);
        centeredFit(_spr,_experience.identity.number,25,37,42,&lgfx::fonts::Font4,&lgfx::fonts::Font2,&lgfx::fonts::Font0,_experience.identity.accent);
        centeredFit(_spr,_experience.identity.callsign,102,30,104,&lgfx::fonts::Font2,&lgfx::fonts::Font0,&lgfx::fonts::Font0,WHITE);
        centered(_spr,"RIFT//RUN",102,55,&lgfx::fonts::Font0,CYAN);
    } else {
        _spr.fillScreen(BLACK);int w=(int)((RIFT_DURATION_MS-t)*160/1200);if(w>0)_spr.fillRect((160-w)/2,0,w,80,MAGENTA);
    }
    return true;
}

// --------------------------- 240x240 navigator -----------------------------

bool SceneNavigator::_renderExperienceTakeover(const UiFx& fx) {
    if(_experience.mode==ExperienceMode::NORMAL)return false;

    if(_experience.mode==ExperienceMode::GAME) {
        const RockGameSnapshot& g=_experience.game;
        drawNavArcadeRoad(_spr,g,_experience.identity.accent);
        _spr.fillRect(0,0,240,28,BLACK);_spr.setFont(&_fonts.med);_spr.setTextColor(CYAN,BLACK);_spr.drawString("NEON TRAIL",7,4);
        char b[32];snprintf(b,sizeof(b),"%06lu",(unsigned long)g.score);_spr.setTextDatum(lgfx::TR_DATUM);_spr.setTextColor(LIME);_spr.drawString(b,233,4);_spr.setTextDatum(lgfx::TL_DATUM);
        _spr.fillRect(0,220,240,20,BLACK);snprintf(b,sizeof(b),"LIFE %u   X%u   GATE %u/%u",g.lives,g.combo,g.gate,g.total_gates);centered(_spr,b,120,230,&lgfx::fonts::Font0,WHITE);
        if(g.status==RockGameStatus::ATTRACT) {
            centered(_spr,"OFFROAD ARCADE 1986",120,118,&_fonts.med,MAGENTA);
            if(Anim::pulse(g.phase_ms,450))centered(_spr,"INSERT COIN",120,151,&_fonts.speed,WHITE);
            centered(_spr,"CREDIT 00",120,180,&_fonts.med,CYAN);
        } else if(g.status==RockGameStatus::COUNTDOWN) {
            const char* labels[]={"3","2","1","GO!"};int ix=g.phase_ms<2700?g.phase_ms/900:3;centered(_spr,labels[ix],120,145,&_fonts.speed,WHITE);
        } else if(g.status==RockGameStatus::CRASHED) {
            if(Anim::pulse(g.phase_ms,90)){_spr.fillRect(0,28,240,192,MAGENTA);for(int i=0;i<8;i++)_spr.drawLine(0,i*27,(i*47)%240,220,WHITE);}
            centered(_spr,"WIPEOUT!",120,145,&_fonts.speed,WHITE);
        } else if(g.status==RockGameStatus::FINISHED) {
            _spr.fillRoundRect(18,62,204,126,14,BLACK);centered(_spr,g.lives?"COURSE CLEAR":"GAME OVER",120,90,&_fonts.med,g.lives?LIME:RED);
            snprintf(b,sizeof(b),"%06lu",(unsigned long)g.score);centered(_spr,b,120,130,&_fonts.speed,WHITE);
            snprintf(b,sizeof(b),"RANK %c  HI %06lu",gameRank(g),(unsigned long)_experience.identity.best_game_score);centered(_spr,b,120,164,&_fonts.med,MAGENTA);
        } else if(g.result_age_ms<900&&g.last_result!=GateResult::NONE) {
            centered(_spr,resultName(g.last_result),120,145,&_fonts.speed,g.last_result==GateResult::HIT?RED:LIME);
        }
        return true;
    }

    const RiftFrame& r=_experience.rift;uint32_t t=r.elapsed_ms;
    if(r.phase==RiftPhase::ARMED){_spr.fillScreen(BLACK);for(int q=22;q<116;q+=18)_spr.drawCircle(120,120,q,MAGENTA);centered(_spr,"RIFT//RUN",120,87,&_fonts.speed,WHITE);uint32_t remain=(int32_t)(r.start_at_ms-fx.now_ms)>0?r.start_at_ms-fx.now_ms:0;char b[24];snprintf(b,sizeof(b),"PORTAL IN %lu.%lu",(unsigned long)(remain/1000),(unsigned long)((remain%1000)/100));centered(_spr,b,120,142,&_fonts.med,CYAN);return true;}
    _spr.fillScreen(NAVY);
    for(int i=0;i<24;i++){uint32_t n=noise32(r.seed+(uint32_t)i*97u);_spr.drawPixel(n%240,(n>>8)%210,i&1?CYAN:WHITE);}
    if(r.phase==RiftPhase::BLACKOUT){_spr.fillScreen(BLACK);int y=(t/6)%240;_spr.drawFastHLine(0,y,240,CYAN);centered(_spr,"TRIPTYCH LINK",120,120,&_fonts.med,MAGENTA);}
    else if(r.phase==RiftPhase::SCAN){for(int q=15;q<130;q+=15)_spr.drawCircle(120,120,(q+t/18)%125,q&1?CYAN:PURPLE);for(int i=0;i<9;i++){int y=30+i*23+(t/13)%23;_spr.drawLine(0,y,240,y-28,MAGENTA);}centered(_spr,"TOPO LOCK",120,115,&_fonts.speed,WHITE);}
    else if(r.phase==RiftPhase::CHARGE){_spr.fillCircle(120,82,54,ORANGE);_spr.fillCircle(120,84,45,MAGENTA);for(int y=48;y<125;y+=10)_spr.drawFastHLine(67,y,107,NAVY);int p=(t-3200)*100/2900;_spr.fillTriangle(120,93,24,220,216,220,ROAD);for(int i=0;i<7;i++){int y=112+i*i*3;int half=10+i*16;_spr.drawFastHLine(120-half,y,half*2,i&1?CYAN:PURPLE);}char b[18];snprintf(b,sizeof(b),"ENERGY %03d%%",p);centered(_spr,b,120,209,&_fonts.med,LIME);}
    else if(r.phase==RiftPhase::CROSSOVER){for(int i=0;i<20;i++){int x=(i*53+t/4)%280-20;_spr.drawFastHLine(x,18+i*10,28,i&1?CYAN:MAGENTA);}int p=crossoverProgress(r,true)*300/1000-30;_spr.fillRoundRect(p-22,94,44,56,8,_experience.identity.accent);_spr.fillRect(p-14,102,28,15,BLACK);_spr.fillCircle(p-20,145,8,BLACK);_spr.fillCircle(p+20,145,8,BLACK);for(int i=0;i<10;i++)_spr.fillCircle(p-28-i*9,144+(i%3)*4,2+i%4,0x8A45);centered(_spr,"OBJECT TRANSFER",120,27,&_fonts.med,WHITE);}
    else if(r.phase==RiftPhase::IMPACT){bool flash=Anim::pulse(t,80);_spr.fillScreen(flash?WHITE:MAGENTA);for(int q=12;q<130;q+=18)_spr.drawCircle(120,120,q,flash?BLACK:WHITE);centered(_spr,"IMPACT",120,118,&_fonts.speed,flash?BLACK:WHITE);}
    else if(r.phase==RiftPhase::HERO){for(int q=22;q<125;q+=17)_spr.drawCircle(120,120,(q+t/30)%120,PURPLE);centered(_spr,"RIFT//RUN",120,48,&_fonts.med,CYAN);centeredFit(_spr,_experience.identity.callsign,120,91,220,&_fonts.speed,&_fonts.med,&_fonts.small,WHITE);centeredFit(_spr,_experience.identity.number,120,147,210,&_fonts.speed,&_fonts.med,&_fonts.small,_experience.identity.accent);centeredFit(_spr,_experience.identity.team,120,195,220,&_fonts.med,&_fonts.small,&lgfx::fonts::Font0,MAGENTA);}
    else{int w=(int)((RIFT_DURATION_MS-t)*240/1200);_spr.fillScreen(BLACK);if(w>0)_spr.fillRect((240-w)/2,0,w,240,MAGENTA);centered(_spr,"PORTAL CLOSED",120,120,&_fonts.med,WHITE);}
    return true;
}
#endif
