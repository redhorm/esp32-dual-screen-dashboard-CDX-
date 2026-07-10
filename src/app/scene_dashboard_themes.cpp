#ifndef NATIVE_BUILD
#include <stdio.h>
#include "scene_dashboard.h"
#include "core/formatter.h"
#include "core/anim.h"
#include "config.h"

namespace {
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
struct Colors { uint16_t bg, panel, ink, dim, accent, danger, good, line; };
constexpr Colors BAJA = { rgb(16,10,5), rgb(48,29,12), rgb(255,229,184), rgb(184,116,45), rgb(255,154,20), rgb(242,55,38), rgb(170,210,65), rgb(119,72,25) };
constexpr Colors NIGHT= { rgb(1,7,10), rgb(5,24,30), rgb(96,243,255), rgb(42,104,116), rgb(198,255,44), rgb(255,55,62), rgb(54,224,164), rgb(15,105,117) };
constexpr Colors TOPO = { rgb(31,37,23), rgb(225,205,151), rgb(25,34,19), rgb(123,135,83), rgb(226,169,53), rgb(194,66,42), rgb(104,145,63), rgb(137,146,91) };
const Colors& palette(UiTheme t) {
    return t == UiTheme::NIGHT_OPS ? NIGHT : (t == UiTheme::TRAIL_TOPO ? TOPO : BAJA);
}
}

void SceneDashboard::_renderThemedBoot(const VehicleState&, const UiFx& fx) {
    const auto& c = palette(_settings.driver_theme);
    _spr.fillScreen(c.bg);
    _spr.drawRoundRect(1, 1, 158, 78, 5, c.line);
    float q = Ease::outCubic(Anim::progress(fx.phase_ms, 0, BOOT_LOGO_MS));
    for (int i = 0; i < 5; i++)
        _spr.drawRoundRect(28-i*3, 19-i*2, 104+i*6, 39+i*4, 5, c.line);
    const char* title = _settings.driver_theme == UiTheme::BAJA_AMBER ? "BAJA" :
                        _settings.driver_theme == UiTheme::NIGHT_OPS ? "OPS" : "TRAIL";
    _spr.setTextDatum(lgfx::MC_DATUM);
    _spr.setFont(&lgfx::fonts::Font4);
    _spr.setTextColor(Anim::lerp565(c.bg, c.accent, q));
    _spr.drawString(title, 80, 33);
    _spr.setFont(&lgfx::fonts::Font0);
    _spr.setTextColor(c.ink);
    _spr.drawString("ULTRA4 // SYSTEM CHECK", 80, 58);
    _spr.setTextDatum(lgfx::TL_DATUM);
}

void SceneDashboard::_renderThemedLive(const VehicleState& s, const UiFx& fx,
                                       const RunStats&) {
    const auto& c = palette(_settings.driver_theme);
    float rpm = _rpmF.update(s.rpm, fx.now_ms, RPM_ATTACK_MS, RPM_RELEASE_MS);
    float speed = _speedF.update(s.speed_mph, fx.now_ms,
                                 SPEED_SMOOTH_MS, SPEED_SMOOTH_MS);
    bool limiter = fx.redline && Anim::pulse(fx.now_ms, SHIFT_FLASH_MS);
    char buf[18];
    _spr.fillScreen(c.bg);

    if (_settings.driver_theme == UiTheme::BAJA_AMBER) {
        // Chunky desert tach and back-lit switch panel.
        _spr.drawRoundRect(1,1,158,78,4,limiter?c.danger:c.line);
        int lit=(int)(rpm/SIM_MAX_RPM*19);
        for(int i=0;i<19;i++){
            int h=4+(i%3==1?2:0);
            _spr.fillRect(4+i*8,4,6,h,i<lit?(i>16?c.danger:c.accent):c.panel);
        }
        _spr.setFont(&lgfx::fonts::Font0); _spr.setTextColor(c.dim);
        _spr.drawString("BAJA // STAGE",5,13);
        Fmt::speed(buf,speed,_settings.use_kmh);
        _spr.setFont(&lgfx::fonts::Font4); _spr.setTextColor(c.ink);
        _spr.drawString(buf,5,24);
        _spr.setFont(&lgfx::fonts::Font0); _spr.setTextColor(c.accent);
        _spr.drawString(_settings.use_kmh?"KMH":"MPH",7,49);
        _spr.fillRoundRect(109,17,45,38,5,c.panel);
        _spr.drawRoundRect(109,17,45,38,5,c.accent);
        Fmt::gear(buf,s.gear,false);
        _spr.setFont(&lgfx::fonts::Font4); _spr.setTextDatum(lgfx::MC_DATUM);
        _spr.setTextColor(c.accent); _spr.drawString(buf,131,36);
        _spr.setTextDatum(lgfx::TL_DATUM);
        snprintf(buf,sizeof(buf),"%d RPM",(int)s.rpm);
        _spr.setFont(&lgfx::fonts::Font0); _spr.setTextColor(c.ink);
        _spr.drawString(buf,59,47);
    } else if (_settings.driver_theme == UiTheme::NIGHT_OPS) {
        // NVG scan grid, central hero speed and a cyan RPM rail.
        for(int x=0;x<160;x+=10)_spr.drawFastVLine(x,0,80,c.panel);
        for(int y=0;y<80;y+=10)_spr.drawFastHLine(0,y,160,c.panel);
        _spr.drawRect(1,1,158,78,limiter?c.danger:c.line);
        _spr.setFont(&lgfx::fonts::Font0); _spr.setTextColor(c.good);
        _spr.drawString("NIGHT OPS / LIVE",5,5);
        Fmt::speed(buf,speed,_settings.use_kmh);
        _spr.setFont(&lgfx::fonts::Font4); _spr.setTextColor(c.ink);
        _spr.setTextDatum(lgfx::MC_DATUM); _spr.drawString(buf,70,34);
        _spr.setFont(&lgfx::fonts::Font0); _spr.setTextColor(c.dim);
        _spr.drawString(_settings.use_kmh?"KM/H":"MPH",70,52);
        _spr.fillRoundRect(124,17,30,36,3,c.panel);
        Fmt::gear(buf,s.gear,false);
        _spr.setFont(&lgfx::fonts::Font4); _spr.setTextColor(c.accent);
        _spr.drawString(buf,139,34); _spr.setTextDatum(lgfx::TL_DATUM);
        int lit=(int)(rpm/SIM_MAX_RPM*24);
        for(int i=0;i<24;i++)
            _spr.fillRect(5+i*6,59,4,5,i<lit?(i>20?c.danger:c.ink):c.panel);
    } else {
        // Expedition panel: compass ribbon, sand cards and topo accents.
        _spr.drawRoundRect(1,1,158,78,5,c.line);
        _spr.fillRect(3,3,154,14,c.panel);
        char heading[12]; Fmt::headingFull(heading,s.heading_deg);
        _spr.setFont(&lgfx::fonts::Font0); _spr.setTextColor(c.ink,c.panel);
        _spr.setTextDatum(lgfx::MC_DATUM); _spr.drawString(heading,80,10);
        for(int x=8;x<153;x+=12)_spr.drawFastVLine(x,4,3,c.dim);
        _spr.fillTriangle(77,16,83,16,80,12,c.danger);
        _spr.fillRoundRect(5,21,91,35,5,c.panel);
        Fmt::speed(buf,speed,_settings.use_kmh);
        _spr.setFont(&lgfx::fonts::Font4); _spr.drawString(buf,43,36);
        _spr.setFont(&lgfx::fonts::Font0);
        _spr.drawString(_settings.use_kmh?"KMH":"MPH",80,38);
        _spr.fillRoundRect(104,21,50,35,5,c.panel);
        Fmt::gear(buf,s.gear,false);
        _spr.setFont(&lgfx::fonts::Font4); _spr.setTextColor(c.danger);
        _spr.drawString(buf,129,36); _spr.setTextDatum(lgfx::TL_DATUM);
        snprintf(buf,sizeof(buf),"ODO %.1f",s.odo_mi);
        _spr.setFont(&lgfx::fonts::Font0); _spr.setTextColor(c.accent);
        _spr.drawString(buf,6,59);
        snprintf(buf,sizeof(buf),"%d RPM",(int)s.rpm); _spr.drawString(buf,102,59);
    }

    float mt=_settings.use_celsius?(s.engine_temp_f-32)*5/9:s.engine_temp_f;
    float et=_settings.use_celsius?(s.esc_temp_f-32)*5/9:s.esc_temp_f;
    _spr.fillRect(3,68,154,9,c.panel);
    _spr.setFont(&lgfx::fonts::Font0); _spr.setTextColor(c.ink,c.panel);
    snprintf(buf,sizeof(buf),"M%d%c",(int)mt,_settings.use_celsius?'C':'F'); _spr.drawString(buf,6,69);
    snprintf(buf,sizeof(buf),"E%d%c",(int)et,_settings.use_celsius?'C':'F'); _spr.drawString(buf,57,69);
    snprintf(buf,sizeof(buf),"%.1fV",s.battery_v); _spr.drawString(buf,120,69);

    if(fx.alert){
        uint16_t a=Anim::pulse(fx.now_ms,ALERT_PULSE_MS)?c.danger:c.panel;
        _spr.fillRect(2,57,156,21,a);
        snprintf(buf,sizeof(buf),"%s %s",fx.alert_info,fx.alert_note);
        _spr.setFont(&lgfx::fonts::Font2); _spr.setTextDatum(lgfx::MC_DATUM);
        _spr.setTextColor(c.ink); _spr.drawString(buf,80,67);
        _spr.setTextDatum(lgfx::TL_DATUM);
    }
}

void SceneDashboard::_renderThemedFinish(const UiFx& fx, const RunStats& stats) {
    const auto& c=palette(_settings.driver_theme);
    _spr.fillScreen(c.bg); _spr.drawRoundRect(1,1,158,78,5,c.line);
    for(int x=-8;x<160;x+=16)
        if(((x/16)&1)==0)_spr.fillRect(x+(fx.now_ms/90)%16,3,8,5,c.accent);
    _spr.setTextDatum(lgfx::MC_DATUM); _spr.setFont(&lgfx::fonts::Font4);
    _spr.setTextColor(c.ink); _spr.drawString("FINISH",80,29);
    char spd[10],tm[12];
    Fmt::speed(spd,stats.max_speed_mph,_settings.use_kmh);
    Fmt::raceTime(tm,stats.stage_ms);
    _spr.setFont(&lgfx::fonts::Font2); _spr.setTextColor(c.accent);
    _spr.drawString(tm,48,56); _spr.drawString(spd,116,56);
    _spr.setTextDatum(lgfx::TL_DATUM);
}
#endif
