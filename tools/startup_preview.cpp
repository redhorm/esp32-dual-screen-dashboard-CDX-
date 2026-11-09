// Render the actual firmware startup, not an illustration of its screens.
#include "recoil/host_canvas.h"
#include "app/startup_renderer.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2 || argc > 4) {
        std::fprintf(stderr,"Usage: startup_preview OUTPUT_DIRECTORY [FPS=15] [en|it]\n");
        return 2;
    }
    const int fps=argc>2?std::atoi(argv[2]):15;
    if (fps<1 || fps>15) return 2;
    const bool english=argc>3 && std::string(argv[3])=="en";
    const std::filesystem::path output(argv[1]);
    std::filesystem::create_directories(output);
    StartupSequence sequence;sequence.start(0);
    HostCanvas pilot(160,80), nav(240,135);
    const int count=(StartupSequence::kDurationMs*fps+999U)/1000U;
    for(int i=0;i<count;++i) {
        const uint32_t now=static_cast<uint32_t>(i)*1000U/static_cast<uint32_t>(fps);
        const auto frame=sequence.frame(now);
        if(!StartupRender::pilot(pilot,frame,english)||!StartupRender::navigator(nav,frame,english))return 3;
        char name[64];
        std::snprintf(name,sizeof(name),"%04d_pilot.ppm",i);
        if(!pilot.save((output/name).string().c_str()))return 4;
        std::snprintf(name,sizeof(name),"%04d_navigator.ppm",i);
        if(!nav.save((output/name).string().c_str()))return 4;
    }
    if(pilot.clipped||nav.clipped)return 5;
    std::printf("%d startup frames; %d fps; 5600 ms; no clipped pixels.\n",count,fps);
}
