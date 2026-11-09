#pragma once
// HDS-style off-road chart, adapted to the actual 240x135 LCD.
// All navigation is explicitly DEMO: there is no GPS receiver in this rig.
// The chart uses metres in a fixed East/North world, never screen-space drift.
#include <math.h>
#include <stdlib.h>
#include "recoil_renderer.h"

namespace HdsRender {
using Screen = RecoilRender::Screen;
constexpr float kMetresPerMile = 1609.344f;
constexpr float kRouteLengthM = 12.05f * kMetresPerMile;
constexpr float kMetersPerPixel = 12.5f;
constexpr uint8_t kWaypointCount = 9;
constexpr uint16_t PAPER = RecoilRender::rgb(239,233,207);
constexpr uint16_t CONTOUR = RecoilRender::rgb(209,196,154);
constexpr uint16_t CONTOUR_MAJOR = RecoilRender::rgb(189,174,131);
constexpr uint16_t INK = RecoilRender::rgb(25,30,29);
constexpr uint16_t ROUTE = RecoilRender::rgb(191,41,139);
constexpr uint16_t TRAIL = RecoilRender::rgb(0,119,156);
constexpr uint16_t WHITE = RecoilRender::rgb(255,254,246);
constexpr uint16_t AMBER = RecoilRender::rgb(248,184,50);
constexpr uint16_t SECONDARY_TRAIL = RecoilRender::rgb(162,168,158);
constexpr uint16_t DRY_WASH = RecoilRender::rgb(155,189,191);

struct Data {
    Screen screen = Screen::LIVE;
    float odo_m = 0;
    float speed_mps = 0;
    bool kmh = true;
    uint32_t now_ms = 0;
    bool warning = false;
};
struct Point { float east_m, north_m; };
struct RouteSample {
    float east_m, north_m, heading_deg, dir_east, dir_north;
    float odo_m, distance_m;
    uint8_t nextWaypoint; // Zero-based roadbook index: 1 at START, 8 at FINISH.
    bool finished;
};
// Cumulative distances match Roadbook::init. Turns are clockwise from North.
// The left hairpin turns 160 degrees, leaving two distinct legs on the chart.
constexpr float kWaypointMiles[kWaypointCount] = {0,.45f,1.23f,2.34f,3.21f,5.67f,8.34f,9.78f,12.05f};
constexpr float kHeading[kWaypointCount-1] = {0,45,315,315,0,200,290,245};
constexpr Point kDirection[kWaypointCount-1] = {
    {0,1},{.7071067812f,.7071067812f},{-.7071067812f,.7071067812f},
    {-.7071067812f,.7071067812f},{0,1},{-.3420201433f,-.9396926208f},
    {-.9396926208f,.3420201433f},{-.9063077870f,-.4226182617f}
};
inline float waypointMetres(uint8_t i) {
    return kWaypointMiles[i < kWaypointCount ? i : kWaypointCount-1] * kMetresPerMile;
}
inline Point waypointPosition(uint8_t i) {
    Point p = {0,0};
    for(uint8_t leg=0;leg<i && leg<kWaypointCount-1;++leg) {
        const float length = waypointMetres(leg+1)-waypointMetres(leg);
        p.east_m += length*kDirection[leg].east_m;
        p.north_m += length*kDirection[leg].north_m;
    }
    return p;
}
inline RouteSample sampleRoute(float odo_m) {
    // NaN/negative input returns START; +infinity and overruns clamp to FINISH.
    if(!(odo_m >= 0))odo_m=0;
    if(odo_m > kRouteLengthM)odo_m=kRouteLengthM;
    uint8_t leg=0;
    while(leg<kWaypointCount-2 && odo_m>=waypointMetres(leg+1))++leg;
    const Point origin=waypointPosition(leg);
    const float travelled=odo_m-waypointMetres(leg);
    return {origin.east_m+travelled*kDirection[leg].east_m,
            origin.north_m+travelled*kDirection[leg].north_m,
            kHeading[leg],kDirection[leg].east_m,kDirection[leg].north_m,
            odo_m,waypointMetres(leg+1)-odo_m,static_cast<uint8_t>(leg+1),odo_m>=kRouteLengthM};
}

// Canvas adapter clips BEFORE sending any primitive to LovyanGFX. This also
// protects the overlay area when long off-screen route/contour segments pan.
template<class C> class Clip {
    C& c; int left,top,right,bottom;
    static bool boundary(float p,float q,float& a,float& b) {
        if(p==0)return q>=0;
        const float r=q/p;
        if(p<0) {
            if(r>b)return false;
            if(r>a)a=r;
        } else {
            if(r<a)return false;
            if(r<b)b=r;
        }
        return true;
    }
public:
    Clip(C& canvas,int x0,int y0,int x1,int y1):c(canvas),left(x0),top(y0),right(x1),bottom(y1) {}
    void fillRect(int x,int y,int w,int h,uint16_t color) {
        if(w<=0||h<=0)return;
        int x1=x+w-1,y1=y+h-1;
        if(x<left)x=left;
        if(y<top)y=top;
        if(x1>right)x1=right;
        if(y1>bottom)y1=bottom;
        if(x<=x1&&y<=y1)c.fillRect(x,y,x1-x+1,y1-y+1,color);
    }
    void drawLine(int x,int y,int bx,int by,uint16_t color) {
        float a=0,b=1,dx=static_cast<float>(bx-x),dy=static_cast<float>(by-y);
        if(!boundary(-dx,x-left,a,b)||!boundary(dx,right-x,a,b)||
           !boundary(-dy,y-top,a,b)||!boundary(dy,bottom-y,a,b))return;
        int ax=static_cast<int>(lroundf(x+a*dx)),ay=static_cast<int>(lroundf(y+a*dy));
        int ex=static_cast<int>(lroundf(x+b*dx)),ey=static_cast<int>(lroundf(y+b*dy));
        // Float rounding at a boundary must never escape the native canvas.
        if(ax<left)ax=left;
        if(ax>right)ax=right;
        if(ex<left)ex=left;
        if(ex>right)ex=right;
        if(ay<top)ay=top;
        if(ay>bottom)ay=bottom;
        if(ey<top)ey=top;
        if(ey>bottom)ey=bottom;
        c.drawLine(ax,ay,ex,ey,color);
    }
};
struct Chart {
    RouteSample position;
    int x(float east_m) const {return 120+static_cast<int>(lroundf((east_m-position.east_m)/kMetersPerPixel));}
    int y(float north_m) const {return 94-static_cast<int>(lroundf((north_m-position.north_m)/kMetersPerPixel));}
    template<class C> void line(C& c,Point a,Point b,uint16_t color,int width=1) const {
        int ax=x(a.east_m),ay=y(a.north_m),bx=x(b.east_m),by=y(b.north_m);
        for(int offset=0;offset<width;++offset) {
            int k=offset-width/2;
            if(abs(bx-ax)>=abs(by-ay))c.drawLine(ax,ay+k,bx,by+k,color);
            else c.drawLine(ax+k,ay,bx+k,by,color);
        }
    }
};

template<class C> void chartLabel(C& c,const Chart& chart,Point position,const char* label,uint16_t color=INK) {
    const int x=chart.x(position.east_m),y=chart.y(position.north_m);
    const int width=static_cast<int>(strlen(label))*4-1;
    // Avoid labels cut by the header, scale and compass. Their world position
    // remains fixed while the map moves; there is no screen-space decoration.
    if(x-width/2<3||x+width/2>236||y<32||y>105)return;
    if(x+width/2>219&&y<56)return;
    c.fillRect(x-width/2-1,y-1,width+2,7,PAPER);
    RecoilRender::text(c,label,x,y,color);
}
inline uint32_t hillSeed(int col,int row) {
    // Defined unsigned wraparound keeps the same terrain on host and ESP32,
    // including negative world coordinates. No runtime random state is used.
    uint32_t seed=static_cast<uint32_t>(col)*73856093u ^ static_cast<uint32_t>(row)*19349663u ^ 0x9e3779b9u;
    seed^=seed>>13;seed*=0x85ebca6bu;seed^=seed>>16;
    return seed;
}
template<class C> void terrain(C& c,const Chart& chart) {
    // Fixed synthetic topography, not downloaded cartography. Unequal ridge
    // stretches, rotation, centre drift and shape variation avoid repeating
    // circles. Still bounded to 9 hills x 4 contours x 12 line segments.
    constexpr Point radial[12]={{1,0},{.8660254f,.5f},{.5f,.8660254f},{0,1},
        {-.5f,.8660254f},{-.8660254f,.5f},{-1,0},{-.8660254f,-.5f},
        {-.5f,-.8660254f},{0,-1},{.5f,-.8660254f},{.8660254f,-.5f}};
    constexpr float irregular[12]={1,.94f,1.06f,.98f,.9f,1.04f,1.1f,1,.93f,1.08f,1.03f,.96f};
    const int cx=static_cast<int>(floorf(chart.position.east_m/2400));
    const int cy=static_cast<int>(floorf(chart.position.north_m/2200));
    for(int row=cy-1;row<=cy+1;++row)for(int col=cx-1;col<=cx+1;++col) {
        const uint32_t seed=hillSeed(col,row);
        const float east=col*2400.0f+400+(seed&255)*2;
        const float north=row*2200.0f+550+((seed>>8)&255)*2;
        const Point rotation=radial[seed%12];
        const float stretch=1.05f+((seed>>18)&3)*.17f;
        const float height=.53f+((seed>>20)&3)*.1f;
        const int shape=(seed>>23)%12;
        for(int ring=1;ring<=4;++ring) {
            const float radius=ring*(170.0f+((seed>>12)&63));
            Point points[12];
            for(int i=0;i<12;++i) {
                const float uneven=irregular[(i+shape)%12];
                const float x=radial[i].east_m*radius*stretch*uneven+(4-ring)*45;
                const float y=radial[i].north_m*radius*height*uneven+(4-ring)*18;
                points[i]={east+x*rotation.east_m-y*rotation.north_m,
                           north+x*rotation.north_m+y*rotation.east_m};
            }
            for(int i=0;i<12;++i)chart.line(c,points[i],points[(i+1)%12],ring==4?CONTOUR_MAJOR:CONTOUR);
        }
        if((seed&3)!=0)continue; // Sparse nominal contour elevations in metres.
        char elevation[8];snprintf(elevation,sizeof(elevation),"%u",static_cast<unsigned>(340+(seed%14)*40));
        chartLabel(c,chart,{east+rotation.east_m*105,north+rotation.north_m*105},elevation,CONTOUR_MAJOR);
    }
}
template<class C> void mapFeatures(C& c,const Chart& chart) {
    // Three static desert service trails, in the same metre coordinate system
    // as the route. They provide context while the vehicle crosses the stage.
    constexpr Point trails[3][8]={
        {{-1400,-600},{-450,150},{-260,550},{0,725},{360,1160},{250,1620},{400,1940},{900,2600}},
        {{650,2000},{100,2550},{-375,2875},{-900,3360},{-2100,3680},{-2300,4680},{-2300,5850},{-1700,7100}},
        {{-8300,3300},{-6800,3650},{-5250,4820},{-4270,5320},{-3980,6280},{-2480,7540},{-1450,8050},{-430,8400}}
    };
    for(int trail=0;trail<3;++trail)for(int i=1;i<8;++i)
        chart.line(c,trails[trail][i-1],trails[trail][i],SECONDARY_TRAIL);
    // A dry wash actually crosses the route at WATER/WP03. Offsets are fixed
    // local survey-like geometry, not values animated by the frame clock.
    constexpr Point washOffsets[13]={{-1350,-260},{-1120,-145},{-860,-200},{-655,-75},
        {-380,-115},{-210,-45},{0,0},{185,40},{310,155},{510,125},{730,290},{1050,390},{1350,320}};
    const Point water=waypointPosition(3);
    Point previous={water.east_m+washOffsets[0].east_m,water.north_m+washOffsets[0].north_m};
    for(int i=1;i<13;++i) {
        Point next={water.east_m+washOffsets[i].east_m,water.north_m+washOffsets[i].north_m};
        chart.line(c,previous,next,DRY_WASH,2);previous=next;
    }
    chartLabel(c,chart,{-780,350},"SAND FLATS");
    chartLabel(c,chart,{750,1050},"RIDGE");
    chartLabel(c,chart,{550,2270},"NORTH RIDGE");
    chartLabel(c,chart,{water.east_m-850,water.north_m-140},"DRY WASH",INK);
    chartLabel(c,chart,{-2100,6550},"HIGH PASS");
    chartLabel(c,chart,{-6500,3900},"WEST FLATS");
}
template<class C> void triangle(C& c,int ax,int ay,int bx,int by,int dx,int dy,uint16_t color) {
    int minx=ax,maxx=ax,miny=ay,maxy=ay;
    if(bx<minx)minx=bx;if(dx<minx)minx=dx;if(bx>maxx)maxx=bx;if(dx>maxx)maxx=dx;
    if(by<miny)miny=by;if(dy<miny)miny=dy;if(by>maxy)maxy=by;if(dy>maxy)maxy=dy;
    for(int y=miny;y<=maxy;++y)for(int x=minx;x<=maxx;++x) {
        int a=(bx-ax)*(y-ay)-(by-ay)*(x-ax);
        int b=(dx-bx)*(y-by)-(dy-by)*(x-bx);
        int d=(ax-dx)*(y-dy)-(ay-dy)*(x-dx);
        if((a>=0&&b>=0&&d>=0)||(a<=0&&b<=0&&d<=0))c.fillRect(x,y,1,1,color);
    }
}
template<class C> void vehicle(C& c,const RouteSample& s) {
    // White cursor with black outline, fixed lower in the north-up viewport.
    const float dx=s.dir_east,dy=-s.dir_north;
    for(int layer=0;layer<2;++layer) {
        const float front=layer?7:10,back=layer?3:5,wide=layer?3:6;
        triangle(c,120+lroundf(dx*front),94+lroundf(dy*front),
            120+lroundf(-dx*back-dy*wide),94+lroundf(-dy*back+dx*wide),
            120+lroundf(-dx*back+dy*wide),94+lroundf(-dy*back-dx*wide),layer?WHITE:INK);
    }
}
inline const char* waypointName(uint8_t i) {
    switch(i) {
        case 0:return "START";case 1:return "R45";case 2:return "L90";
        case 3:return "WATER";case 4:return "R45";case 5:return "HAIRPIN";
        case 6:return "R90";case 7:return "L45";default:return "FINISH";
    }
}
template<class C> void overlays(C& c,const Data& d,const RouteSample& s) {
    using RecoilRender::text;
    c.fillRect(0,0,240,27,WHITE);
    c.drawLine(0,26,239,26,CONTOUR_MAJOR);
    c.drawLine(79,2,79,23,CONTOUR);c.drawLine(159,2,159,23,CONTOUR);
    text(c,d.kmh?"SOG KM/H":"SOG MPH",40,3,INK);
    text(c,"COG",120,3,INK);text(c,d.kmh?"DTW KM":"DTW MI",200,3,INK);
    float speed=::isfinite(d.speed_mps)&&d.speed_mps>0?d.speed_mps:0;
    if(speed>150)speed=150;
    if(s.finished||d.screen==Screen::BOOT)speed=0;
    char b[20];snprintf(b,sizeof(b),"%.1f",speed*(d.kmh?3.6f:2.2369363f));text(c,b,40,12,INK,2);
    snprintf(b,sizeof(b),"%03d",static_cast<int>(s.heading_deg+.5f)%360);text(c,b,118,12,INK,2);
    c.drawLine(133,10,135,10,INK);c.drawLine(133,12,135,12,INK);c.fillRect(133,11,1,1,INK);c.fillRect(135,11,1,1,INK);
    snprintf(b,sizeof(b),"%.2f",s.distance_m/(d.kmh?1000:kMetresPerMile));text(c,b,200,12,INK,2);
    // North-up compass and real map scale. Labels live above the chart layer.
    c.fillRect(221,31,15,23,PAPER);text(c,"N",228,32,INK);
    c.drawLine(228,40,228,51,INK);c.drawLine(224,45,228,40,INK);c.drawLine(232,45,228,40,INK);
    const float scale_m=d.kmh?500.0f:528.0f*.3048f; // 500 m or 0.1 mile.
    const int length=static_cast<int>(lroundf(scale_m/kMetersPerPixel));
    c.fillRect(4,108,length+8,16,PAPER);text(c,d.kmh?"500 M":"0.1 MI",7+length/2,110,INK);
    c.drawLine(7,120,7+length,120,INK);c.drawLine(7,117,7,121,INK);c.drawLine(7+length,117,7+length,121,INK);
    c.fillRect(0,125,240,10,WHITE);c.drawLine(0,125,239,125,CONTOUR_MAJOR);
    if(s.finished)snprintf(b,sizeof(b),"FINISH - ARRIVED");
    else snprintf(b,sizeof(b),"WP%02u %s",static_cast<unsigned>(s.nextWaypoint),waypointName(s.nextWaypoint));
    text(c,b,88,128,INK);
    c.fillRect(208,127,28,7,AMBER);text(c,"DEMO",222,128,INK);
    if(d.warning) {
        c.fillRect(164,113,71,10,AMBER);text(c,"CAUTION",199,115,INK);
    }
}
template<class C> bool render(C& c,const Data& d) {
    if(c.width()!=240||c.height()!=135) {c.fillScreen(RecoilRender::BLACK);return false;}
    if(d.screen==Screen::ALIGN) {
        RecoilRender::NavData old;old.screen=Screen::ALIGN;
        return RecoilRender::navigator(c,old);
    }
    float odo=d.screen==Screen::FINISH?kRouteLengthM:d.screen==Screen::BOOT?0:d.odo_m;
    const RouteSample s=sampleRoute(odo);
    c.fillScreen(PAPER);
    Clip<C> map(c,0,27,239,124),full(c,0,0,239,134);
    Chart chart={s};terrain(map,chart);mapFeatures(map,chart);
    Point last=waypointPosition(0);
    for(uint8_t i=1;i<kWaypointCount;++i) {
        Point next=waypointPosition(i);
        chart.line(map,last,next,ROUTE,2);
        if(s.odo_m>=waypointMetres(i))chart.line(map,last,next,TRAIL,2);
        else if(s.odo_m>waypointMetres(i-1))chart.line(map,last,{s.east_m,s.north_m},TRAIL,2);
        last=next;
    }
    for(uint8_t i=0;i<kWaypointCount;++i) {
        Point p=waypointPosition(i);const int x=chart.x(p.east_m),y=chart.y(p.north_m);
        // Do not emit map labels partly hidden under overlays, or at our cursor.
        if(x<9||x>230||y<33||y>116||(abs(x-120)<15&&abs(y-94)<15))continue;
        RecoilRender::disk(map,x,y,3,INK);RecoilRender::disk(map,x,y,2,WHITE);
        char b[12];if(i==0||i==8)snprintf(b,sizeof(b),"%s",waypointName(i));else snprintf(b,sizeof(b),"WP%02u",static_cast<unsigned>(i));
        const int labelWidth=static_cast<int>(strlen(b))*4;
        int labelCenter=x+labelWidth/2+5;if(labelCenter+labelWidth/2>237)labelCenter=x-labelWidth/2-5;
        map.fillRect(labelCenter-labelWidth/2-1,y-3,labelWidth+2,7,PAPER);
        RecoilRender::text(map,b,labelCenter,y-2,INK);
    }
    vehicle(map,s);overlays(full,d,s);
    if(d.screen==Screen::BOOT) {
        full.fillRect(62,51,116,35,INK);full.fillRect(63,52,114,33,WHITE);
        RecoilRender::text(full,"CHART",120,57,INK,2);
        RecoilRender::text(full,"SIMULATOR",120,72,INK);
        for(int i=0;i<3;++i)full.fillRect(111+i*8,80,3,2,(d.now_ms/180)%3==static_cast<unsigned>(i)?ROUTE:CONTOUR);
    } else if(d.screen==Screen::FINISH) {
        full.fillRect(74,34,92,19,INK);full.fillRect(75,35,90,17,WHITE);
        RecoilRender::text(full,"ARRIVED",120,39,INK,2);
    }
    return true;
}
} // namespace HdsRender
