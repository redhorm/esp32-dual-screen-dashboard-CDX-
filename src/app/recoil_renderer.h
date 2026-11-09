#pragma once
// Native LCD content shared by LovyanGFX and the host pixel preview.
// Bezels, logos and buttons are physical parts, never drawn on the LCD.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "recoil_geometry.h"

namespace RecoilRender {
constexpr uint16_t rgb(int r,int g,int b) {
    return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);
}
constexpr uint16_t BLACK=0, LCD=rgb(108,135,224), INK=rgb(23,38,85);
constexpr uint16_t MAP=rgb(62,70,61), CONTOUR=rgb(105,112,83);
constexpr uint16_t ROAD=rgb(142,148,137), CYAN=rgb(52,193,225);
constexpr uint16_t WHITE=rgb(213,222,229), AMBER=rgb(244,188,29);
constexpr uint16_t DARK=rgb(17,23,31), RED=rgb(243,66,46);
enum class Screen : uint8_t { BOOT, LIVE, FINISH, ALIGN };

struct PilotData {
    int speed=0, gear=1, lit=0;
    float battery=0;
    bool kmh=true, warning=false;
    // No oil sensor or lap counter exists in the baseline. -1 means unavailable.
    int oil=-1, lap=-1;
    uint32_t stageSeconds=0;
    Screen screen=Screen::LIVE;
};
struct NavData {
    int waypoint=1, satellites=-1;
    float progress=0;
    bool warning=false;
    uint32_t stageSeconds=0;
    Screen screen=Screen::LIVE;
    // True only for the documented CAD reference preview, never vehicle data.
    bool reference=false;
};

inline uint16_t glyph(char c) {
    // Original 3x5 bitmap alphabet, MSB-first rows; no runtime font allocation.
    switch(c) {
    case '0':return 0b111101101101111;
    case '1':return 0b010110010010111;
    case '2':return 0b110001010100111;
    case '3':return 0b110001010001110;
    case '4':return 0b101101111001001;
    case '5':return 0b111100110001110;
    case '6':return 0b011100111101111;
    case '7':return 0b111001010010010;
    case '8':return 0b111101111101111;
    case '9':return 0b111101111001110;
    case 'A':return 0b010101111101101;
    case 'B':return 0b110101110101110;
    case 'C':return 0b011100100100011;
    case 'D':return 0b110101101101110;
    case 'E':return 0b111100110100111;
    case 'F':return 0b111100110100100;
    case 'G':return 0b011100101101011;
    case 'H':return 0b101101111101101;
    case 'I':return 0b111010010010111;
    case 'J':return 0b001001001101010;
    case 'K':return 0b101101110101101;
    case 'L':return 0b100100100100111;
    case 'M':return 0b101111111101101;
    case 'N':return 0b101111111111101;
    case 'O':return 0b010101101101010;
    case 'P':return 0b110101110100100;
    case 'Q':return 0b010101101111011;
    case 'R':return 0b110101110101101;
    case 'S':return 0b011100010001110;
    case 'T':return 0b111010010010010;
    case 'U':return 0b101101101101111;
    case 'V':return 0b101101101101010;
    case 'W':return 0b101101111111101;
    case 'X':return 0b101101010101101;
    case 'Y':return 0b101101010010010;
    case 'Z':return 0b111001010100111;
    case '-':return 0b000000111000000;
    case '.':return 0b000000000000010;
    case ':':return 0b000010000010000;
    case '/':return 0b001001010100100;
    case '+':return 0b000010111010000;
    default:return 0;
    }
}
template<class C> void text(C& c,const char* s,int center,int y,uint16_t color,int scale=1) {
    const int length=static_cast<int>(strlen(s));
    int x=center-((length*4-1)*scale)/2;
    for (;*s;++s,x+=4*scale) {
        uint16_t bits=glyph(*s);
        for(int row=0;row<5;++row) for(int col=0;col<3;++col)
            if(bits & (1U<<(14-row*3-col)))
                c.fillRect(x+col*scale,y+row*scale,scale,scale,color);
    }
}
template<class C> void disk(C& c,int x,int y,int r,uint16_t color) {
    for(int dy=-r;dy<=r;++dy) {
        int dx=r;while(dx*dx+dy*dy>r*r)--dx;
        c.fillRect(x-dx,y+dy,2*dx+1,1,color);
    }
}
template<class C> void segmentDigit(C& c,char ch,int x,int y,int w,int h,int t,uint16_t color) {
    static const uint8_t segments[10]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
    if(ch<'0'||ch>'9') {text(c,"-",x+w/2,y+h/2,color);return;}
    const uint8_t s=segments[ch-'0'];const int half=h/2;
    if(s&1)c.fillRect(x+t,y,w-2*t,t,color);
    if(s&2)c.fillRect(x+w-t,y+t,t,half-t-1,color);
    if(s&4)c.fillRect(x+w-t,y+half+1,t,h-half-t-1,color);
    if(s&8)c.fillRect(x+t,y+h-t,w-2*t,t,color);
    if(s&16)c.fillRect(x,y+half+1,t,h-half-t-1,color);
    if(s&32)c.fillRect(x,y+t,t,half-t-1,color);
    if(s&64)c.fillRect(x+t,y+half-t/2,w-2*t,t,color);
}
template<class C> void number(C& c,int value,int center,int y,int w,int h,int t,uint16_t color) {
    char b[12];snprintf(b,sizeof(b),"%d",value);
    const int n=strlen(b);int x=center-(n*(w+2)-2)/2;
    for(int i=0;i<n;++i,x+=w+2)segmentDigit(c,b[i],x,y,w,h,t,color);
}
template<class C> void lamps(C& c,int count) {
    for(int i=0;i<RecoilGeometry::DOT_COUNT;++i) {
        const auto& p=RecoilGeometry::dots[i];
        disk(c,p.x,p.y,RecoilGeometry::DOT_RADIUS,i<count?p.color:BLACK);
    }
}
template<class C> bool pilot(C& c,const PilotData& d) {
    c.fillScreen(BLACK);
    if(c.width()!=160||c.height()!=80)return false;
    for(int x=0;x<160;++x) {
        const int y=RecoilGeometry::windowTop[x];
        if(y!=255)c.fillRect(x,y,1,RecoilGeometry::WINDOW_BOTTOM-y+1,LCD);
    }
    if(d.screen==Screen::ALIGN) {
        text(c,"16 PUNTI",80,40,INK,2);
        text(c,"160X80  1:1",80,57,INK);
        text(c,"FRONTE",80,68,INK);
        lamps(c,16);return true;
    }
    if(d.screen==Screen::BOOT) {
        text(c,"RECOIL",80,40,INK,2);text(c,"PULSE",80,57,INK);text(c,"DEMO",80,69,INK);
        lamps(c,d.lit);return true;
    }
    if(d.screen==Screen::FINISH) {
        text(c,"FINISH",80,38,INK,2);text(c,"DEMO",80,69,INK);
        char b[16];snprintf(b,sizeof(b),"%02lu:%02lu",(unsigned long)((d.stageSeconds/60)%100),(unsigned long)(d.stageSeconds%60));
        text(c,b,80,57,INK,2);lamps(c,16);return true;
    }
    constexpr int tx[7]={24,42,61,80,98,116,135},ty[7]={33,27,23,21,23,27,33};
    for(int i=0;i<7;++i) {char b[2]={char('1'+i),0};text(c,b,tx[i],ty[i],INK);}
    text(c,d.kmh?"KM/H":"MPH",72,27,INK);
    const int speed=d.speed<0?0:(d.speed>999?999:d.speed);
    number(c,speed,69,33,speed<10?20:(speed<100?14:10),32,2,INK);
    text(c,"OIL",26,41,INK);
    if(d.oil<0)text(c,"--",26,53,INK,2);
    else number(c,d.oil>999?999:d.oil,26,48,d.oil>99?6:8,15,1,INK);
    text(c,"GEAR",122,40,INK);
    if(d.gear<0)text(c,"R",122,48,INK,3);
    else if(d.gear==0)text(c,"N",122,48,INK,3);
    else number(c,d.gear>9?9:d.gear,122,48,10,16,1,INK);
    char b[16];snprintf(b,sizeof(b),"%.1fV",d.battery);text(c,"DEMO",34,70,INK);
    if(d.lap<0)text(c,"LAP --",121,70,INK);
    else {snprintf(b,sizeof(b),"LAP %02d",d.lap%100);text(c,b,121,70,INK);}
    if(d.warning) {c.fillRect(59,68,31,7,RED);text(c,"ALERT",74,69,WHITE);}
    // Last operation: no wipe, text, smoothing, alert or animation moves/overpaints dots.
    lamps(c,d.lit);return true;
}

inline int nx(float x) {return static_cast<int>((x+12.456f)*240.0f/24.912f);}
inline int ny(float y) {return static_cast<int>((7.432f-y)*135.0f/14.864f);}
template<class C> void mapLine(C& c,float x0,float y0,float x1,float y1,uint16_t color,int width=1) {
    const int ax=nx(x0),ay=ny(y0),bx=nx(x1),by=ny(y1);
    const int dx=bx-ax,dy=by-ay;
    for(int k=-(width/2);k<=width/2;++k) {
        if(dx*dx>=dy*dy)c.drawLine(ax,ay+k,bx,by+k,color);
        else c.drawLine(ax+k,ay,bx+k,by,color);
    }
}
template<class C> bool navigator(C& c,const NavData& d) {
    c.fillScreen(BLACK);
    if(c.width()!=240||c.height()!=135) {
        text(c,"USE 240X135",c.width()/2,c.height()/2-3,WHITE);return false;
    }
    c.fillScreen(MAP);
    for(int j=0;j<7;++j) {
        const float p[6][2]={{-12,-6.4f+j*1.6f},{-8,-5+j*1.3f},{-3,-6.1f+j*1.4f},{2,-5+j*1.4f},{7,-5.8f+j*1.45f},{12,-4.6f+j*1.2f}};
        for(int i=1;i<6;++i)mapLine(c,p[i-1][0],p[i-1][1],p[i][0],p[i][1],CONTOUR);
    }
    const float roads[3][4][2]={{{-10,-6},{-7,-2},{-8,2},{-5,5}},{{-2,-6},{0,-2},{4,1},{3,5}},{{9,-6},{7,-1},{10,4},{10,4}}};
    for(int r=0;r<3;++r)for(int i=1;i<4;++i)mapLine(c,roads[r][i-1][0],roads[r][i-1][1],roads[r][i][0],roads[r][i][1],ROAD);
    const float route[8][2]={{-10,-4.5},{-6.5,-3.2},{-4.4,.1},{-.5,1.1},{1,-.6},{5,-.6},{7,3.3},{9.4,2.6}};
    for(int i=1;i<8;++i)mapLine(c,route[i-1][0],route[i-1][1],route[i][0],route[i][1],CYAN,3);
    float progress=d.progress<0?0:(d.progress>1?1:d.progress);
    const float t=progress*7;int i=static_cast<int>(t);if(i>6)i=6;
    const float f=t-i;
    disk(c,nx(route[i][0]+f*(route[i+1][0]-route[i][0])),ny(route[i][1]+f*(route[i+1][1]-route[i][1])),4,AMBER);
    c.fillRect(0,0,240,16,DARK);char b[20];
    snprintf(b,sizeof(b),d.reference?"BAJA %02d":"WP %02d",d.waypoint%100);text(c,b,55,4,WHITE,2);
    if(d.satellites<0)text(c,"DEMO",196,4,CYAN,2);
    else {snprintf(b,sizeof(b),"GPS %02d",d.satellites%100);text(c,b,196,4,CYAN,2);}
    mapLine(c,-10.5,-5.9,-6.5,-5.9,WHITE);text(c,d.reference?"50 M":"DEMO",38,126,WHITE);
    mapLine(c,9.15,3.8,9.65,4.6,WHITE);mapLine(c,9.65,4.6,10.15,3.8,WHITE);text(c,"N",227,24,WHITE);
    if(d.screen==Screen::ALIGN) {
        c.drawLine(0,0,239,0,WHITE);c.drawLine(239,0,239,134,WHITE);
        c.drawLine(239,134,0,134,WHITE);c.drawLine(0,134,0,0,WHITE);
        c.fillRect(62,53,116,22,DARK);text(c,"240X135  1:1",120,59,WHITE,2);
    } else if(d.screen==Screen::BOOT) {
        c.fillRect(65,53,110,22,DARK);text(c,"RECOIL",120,59,WHITE,2);
    } else if(d.screen==Screen::FINISH) {
        c.fillRect(65,46,110,39,DARK);text(c,"FINISH",120,51,WHITE,2);
        snprintf(b,sizeof(b),"%02lu:%02lu",(unsigned long)((d.stageSeconds/60)%100),(unsigned long)(d.stageSeconds%60));text(c,b,120,69,CYAN,2);
    } else if(d.warning) {
        c.fillRect(144,124,90,10,RED);text(c,"ALERT",189,126,WHITE);
    }
    return true;
}
} // namespace RecoilRender
