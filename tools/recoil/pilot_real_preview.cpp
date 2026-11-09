#include "host_canvas.h"
#include "app/instrument_pilot.h"
#include <filesystem>
#include <string>
#include <cstdio>

int main(int argc,char** argv) {
    if(argc!=2){std::fprintf(stderr,"Usage: pilot_real_preview OUTPUT_DIRECTORY\n");return 2;}
    std::filesystem::create_directories(argv[1]);
    HostCanvas c(160,80); Iq3Render::Data data;
    data.speed=74;data.gear=3;data.rpm=5600;data.battery_x10=126;data.temp=93;data.elapsed_ms=142000;
    auto save=[&](const char* name){
        Iq3Render::pilot(c,data);
        const auto path=std::filesystem::path(argv[1])/(std::string(name)+".ppm");
        if(!c.save(path.string().c_str()))std::exit(3);
    };
    save("IQ3_live");
    data.speed=105;data.gear=5;data.rpm=7200;data.elapsed_ms=166000;save("IQ3_shift");
    data.speed=0;data.gear=0;data.rpm=800;data.elapsed_ms=0;save("IQ3_neutral");
    data.kmh=false;data.gear=-1;data.speed=4;data.rpm=2200;save("IQ3_reverse_mph");
    data.warning=true;save("IQ3_warning");
    data.screen=RecoilRender::Screen::BOOT;data.shift_pairs=8;save("IQ3_startup");
    return c.clipped?4:0;
}
