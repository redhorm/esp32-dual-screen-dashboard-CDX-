// Host preview of the actual instrument renderer, not a mock-up.
// c++ -std=c++17 -I src tools/recoil/nav_real_preview.cpp -o /tmp/nav-real-preview
// /tmp/nav-real-preview /tmp/nav-real [odometer_metres]
// Produces <prefix>.ppm, <prefix>-turn.ppm, <prefix>-finish.ppm.
#include "host_canvas.h"
#include "app/instrument_navigator.h"
#include <string>
int main(int argc,char**argv) {
    if(argc<2)return 2;
    HostCanvas canvas(240,135);HdsRender::Data d;d.speed_mps=18.4f;
    d.odo_m=argc>2?static_cast<float>(atof(argv[2])):310;
    HdsRender::render(canvas,d);canvas.save((std::string(argv[1])+".ppm").c_str());
    d.odo_m=HdsRender::waypointMetres(2)+220;d.speed_mps=12;d.warning=true;
    HdsRender::render(canvas,d);canvas.save((std::string(argv[1])+"-turn.ppm").c_str());
    d.screen=HdsRender::Screen::FINISH;d.warning=false;
    HdsRender::render(canvas,d);canvas.save((std::string(argv[1])+"-finish.ppm").c_str());
    return canvas.clipped?1:0;
}
