// Still previews of production renderers plus unchanged optical calibration.
#include "host_canvas.h"
#include "app/instrument_pilot.h"
#include "app/instrument_navigator.h"
#include <filesystem>
#include <string>
#include <cstdio>
int main(int argc,char** argv) {
    if(argc!=2) {fprintf(stderr,"Usage: preview OUTPUT_DIRECTORY\n");return 2;}
    std::filesystem::create_directories(argv[1]);
    HostCanvas a(160,80),b(240,135);
    Iq3Render::Data p;p.speed=76;p.gear=3;p.rpm=5600;p.battery_x10=126;p.elapsed_ms=142000;
    HdsRender::Data n;n.odo_m=620;n.speed_mps=76.0f/3.6f;
    auto save=[&](const char* name,HostCanvas& c){
        std::string path=std::string(argv[1])+"/"+name+".ppm";
        if(!c.save(path.c_str()))std::exit(3);
    };
    Iq3Render::pilot(a,p);save("PILOTA_firmware",a);
    HdsRender::render(b,n);save("COPILOTA_firmware",b);
    p.screen=RecoilRender::Screen::ALIGN;n.screen=p.screen;
    Iq3Render::pilot(a,p);save("PILOTA_allineamento",a);
    HdsRender::render(b,n);save("COPILOTA_allineamento",b);
    p.screen=RecoilRender::Screen::LIVE;p.speed=123;p.warning=true;p.gear=5;p.rpm=7200;
    Iq3Render::pilot(a,p);save("PILOTA_prova_avviso",a);
    if(a.clipped||b.clipped)return 4;
    puts("Five previews from production renderers; no out-of-bounds pixels.");
}
