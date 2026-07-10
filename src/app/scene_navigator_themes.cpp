#ifndef NATIVE_BUILD
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "scene_navigator.h"
#include "core/formatter.h"
#include "core/anim.h"
#include "config.h"

namespace {
constexpr uint16_t rgbn(uint8_t r,uint8_t g,uint8_t b){return((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3);}
struct NavColors { uint16_t bg,panel,ink,dim,accent,danger,good,line; };
constexpr NavColors BAJA={rgbn(13,9,5),rgbn(245,221,177),rgbn(45,27,12),rgbn(170,110,48),rgbn(255,157,25),rgbn(224,49,35),rgbn(117,151,50),rgbn(106,67,28)};
constexpr NavColors NIGHT={rgbn(1,7,10),rgbn(4,22,28),rgbn(96,243,255),rgbn(40,104,116),rgbn(198,255,44),rgbn(255,55,62),rgbn(54,224,164),rgbn(14,112,126)};
constexpr NavColors TOPO={rgbn(31,37,23),rgbn(224,207,158),rgbn(28,37,20),rgbn(116,128,78),rgbn(225,171,54),rgbn(191,64,41),rgbn(95,139,59),rgbn(136,146,91)};
const NavColors& pal(UiTheme t){return t==UiTheme::NIGHT_OPS?NIGHT:(t==UiTheme::TRAIL_TOPO?TOPO:BAJA);}
const char* title(UiTheme t){return t==UiTheme::NIGHT_OPS?"NIGHT OPS":(t==UiTheme::TRAIL_TOPO?"TRAIL TOPO":"BAJA ROADBOOK");}
constexpr int TW=166,SX=168,SC=204,RY=38,RH=42,FY=207;
}

void SceneNavigator::_renderThemedBoot(const UiFx& fx){
    const auto& c=pal(_settings.navigator_theme);_spr.fillScreen(c.bg);
    for(int r=92;r>18;r-=12)_spr.drawCircle(120,127,r,c.line);
    float q=Ease::outCubic(Anim::progress(fx.phase_ms,0,BOOT_TOTAL_MS));
    int sweep=(int)(q*360);_spr.drawArc(120,127,76,72,-90,sweep-90,c.accent);
    _spr.setTextDatum(lgfx::MC_DATUM);_spr.setFont(&_fonts.speed);_spr.setTextColor(c.ink);_spr.drawString(title(_settings.navigator_theme),120,104);
    _spr.setFont(&_fonts.small);_spr.setTextColor(c.accent);_spr.drawString("GPS + ROADBOOK INITIALIZING",120,150);
    _spr.setFont(&lgfx::fonts::Font0);_spr.setTextColor(c.dim);char b[20];snprintf(b,sizeof(b),"SYSTEM %03d%%",(int)(q*100));_spr.drawString(b,120,178);_spr.setTextDatum(lgfx::TL_DATUM);
}

void SceneNavigator::_drawThemedRow(int y,const Waypoint& wp,bool active,bool alert_on){
    const auto& c=pal(_settings.navigator_theme);int h=RH-2;
    uint16_t bg=active?c.accent:c.panel,ink=c.ink;
    if(wp.danger&&!active){bg=alert_on?c.panel:c.danger;ink=alert_on?c.danger:0xFFFF;}
    _spr.fillRoundRect(2,y,TW-4,h,3,bg);_spr.drawRoundRect(2,y,TW-4,h,3,active?c.ink:c.line);
    _spr.drawFastVLine(45,y+2,h-4,c.line);_spr.drawFastVLine(77,y+2,h-4,c.line);_spr.drawFastVLine(133,y+2,h-4,c.line);
    int cy=y+h/2;char b[10];Fmt::distNum(b,wp.dist_mi,_settings.use_kmh);
    _spr.setTextDatum(lgfx::MC_DATUM);_spr.setFont(&_fonts.med);_spr.setTextColor(ink);_spr.drawString(b,23,cy);
    _drawArrow(61,y+19,wp);
    bool note=wp.note&&wp.note[0];_spr.setFont(strlen(wp.info)<=7?&_fonts.med:&_fonts.small);_spr.setTextColor(ink);_spr.drawString(wp.info,105,note?y+13:cy);
    if(note){_spr.setFont(&_fonts.small);_spr.setTextColor(wp.danger?c.danger:c.good);_spr.drawString(wp.note,105,y+29);}
    Fmt::distNum(b,wp.total_mi,_settings.use_kmh);_spr.setFont(&lgfx::fonts::Font0);_spr.setTextColor(ink);_spr.drawString(b,149,cy);_spr.setTextDatum(lgfx::TL_DATUM);
}

void SceneNavigator::_renderThemedLive(const VehicleState& s,const UiFx& fx){
    const auto& c=pal(_settings.navigator_theme);_spr.fillScreen(c.bg);
    if(_settings.navigator_theme==UiTheme::NIGHT_OPS){for(int x=0;x<240;x+=16)_spr.drawFastVLine(x,0,240,c.panel);for(int y=0;y<240;y+=16)_spr.drawFastHLine(0,y,240,c.panel);}
    if(_settings.navigator_theme==UiTheme::TRAIL_TOPO){for(int r=20;r<130;r+=18)_spr.drawCircle(210,90,r,c.line);}
    _spr.fillRect(0,0,240,22,c.bg);_spr.drawFastHLine(0,22,240,c.accent);_spr.fillSmoothCircle(8,11,3,c.good);
    _spr.setFont(&lgfx::fonts::Font0);_spr.setTextColor(c.good,c.bg);_spr.drawString("GPS",15,7);
    _spr.setTextDatum(lgfx::TC_DATUM);_spr.setFont(&lgfx::fonts::Font2);_spr.setTextColor(c.ink);_spr.drawString(title(_settings.navigator_theme),120,3);
    _spr.setTextDatum(lgfx::TR_DATUM);_spr.setFont(&lgfx::fonts::Font0);char b[18];Fmt::clockHMS(b,fx.now_ms/1000);_spr.drawString(b,236,7);_spr.setTextDatum(lgfx::TL_DATUM);
    _spr.fillRect(0,24,TW,12,c.accent);_spr.setTextColor(c.bg,c.accent);_spr.setFont(&lgfx::fonts::Font0);_spr.setTextDatum(lgfx::TC_DATUM);
    _spr.drawString(_settings.use_kmh?"KM":"MI",23,26);_spr.drawString("CAP",61,26);_spr.drawString("NOTE",105,26);_spr.drawString("TOT",149,26);_spr.setTextDatum(lgfx::TL_DATUM);
    int first=s.active_wp-1,maxFirst=_rb.getCount()-4;if(first>maxFirst)first=maxFirst;if(first<0)first=0;_scroll.setTarget(first,fx.now_ms,SCROLL_MS);float pos=_scroll.pos(fx.now_ms);bool pulse=Anim::pulse(fx.now_ms,ALERT_PULSE_MS);
    _spr.setClipRect(0,RY,TW,4*RH);int i0=(int)floorf(pos);for(int i=i0;i<=i0+4&&i<_rb.getCount();i++){if(i<0)continue;int y=RY+(int)((i-pos)*RH+.5f);if(y>=RY+4*RH)break;_drawThemedRow(y,_rb.get(i),i==s.active_wp,fx.alert&&i==fx.alert_wp&&pulse);}_spr.clearClipRect();
    _spr.fillRoundRect(SX+3,27,67,174,5,c.panel);_spr.drawRoundRect(SX+3,27,67,174,5,c.line);float sp=_spdF.update(s.speed_mph,fx.now_ms,NAV_VALUE_SMOOTH_MS,NAV_VALUE_SMOOTH_MS);
    _spr.setTextDatum(lgfx::TC_DATUM);_spr.setFont(&lgfx::fonts::Font0);_spr.setTextColor(c.dim,c.panel);_spr.drawString("SPEED",SC,34);Fmt::speed(b,sp,_settings.use_kmh);_spr.setFont(&_fonts.speed);_spr.setTextColor(c.ink);_spr.drawString(b,SC,43);_spr.setFont(&lgfx::fonts::Font0);_spr.setTextColor(c.dim);_spr.drawString(_settings.use_kmh?"KM/H":"MPH",SC,86);
    _spr.drawFastHLine(SX+8,100,57,c.line);_spr.drawString("GEAR",SC,106);Fmt::gear(b,s.gear,false);_spr.setFont(&_fonts.speed);_spr.setTextColor(c.accent);_spr.drawString(b,SC,113);
    _spr.drawFastHLine(SX+8,168,57,c.line);snprintf(b,sizeof(b),"%d RPM",(int)s.rpm);_spr.setFont(&_fonts.small);_spr.setTextColor(c.ink);_spr.drawString(b,SC,177);_spr.setFont(&lgfx::fonts::Font0);_spr.setTextColor(s.four_wd_lock?c.good:c.dim);_spr.drawString(s.four_wd_lock?"4WD LOCK":"4WD AUTO",SC,190);_spr.setTextDatum(lgfx::TL_DATUM);
    _spr.fillRect(0,FY,240,33,c.bg);_spr.drawFastHLine(0,FY,240,fx.alert?c.danger:c.accent);char hd[12];Fmt::headingFull(hd,s.heading_deg);_spr.setFont(&_fonts.med);_spr.setTextColor(c.ink);_spr.drawString(hd,5,215);float d=_distF.update(fx.alert?fx.alert_dist:s.dist_to_next,fx.now_ms,NAV_VALUE_SMOOTH_MS,NAV_VALUE_SMOOTH_MS);Fmt::distNum(b,d,_settings.use_kmh);char db[18];snprintf(db,sizeof(db),"%s %s",b,_settings.use_kmh?"KM":"MI");_spr.setTextDatum(lgfx::TR_DATUM);_spr.setTextColor(fx.alert?c.danger:c.accent);_spr.drawString(db,235,215);_spr.setTextDatum(lgfx::TL_DATUM);
}

void SceneNavigator::_renderThemedFinish(const UiFx& fx,const RunStats& stats){
    const auto& c=pal(_settings.navigator_theme);_spr.fillScreen(c.bg);for(int r=100;r>25;r-=15)_spr.drawCircle(120,112,r,c.line);
    _spr.setTextDatum(lgfx::MC_DATUM);_spr.setFont(&_fonts.speed);_spr.setTextColor(c.ink);_spr.drawString("FINISH",120,72);char b[20];Fmt::raceTime(b,stats.stage_ms);_spr.setTextColor(c.accent);_spr.drawString(b,120,121);char s[10];Fmt::speed(s,stats.max_speed_mph,_settings.use_kmh);snprintf(b,sizeof(b),"MAX %s %s",s,_settings.use_kmh?"KM/H":"MPH");_spr.setFont(&_fonts.med);_spr.setTextColor(c.ink);_spr.drawString(b,120,169);_spr.setFont(&_fonts.small);_spr.setTextColor(c.dim);_spr.drawString(title(_settings.navigator_theme),120,199);_spr.setTextDatum(lgfx::TL_DATUM);(void)fx;
}
#endif
