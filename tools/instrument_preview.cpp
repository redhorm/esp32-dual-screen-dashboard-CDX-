// Host preview of the firmware's simulator, data conversion and LCD renderers.
#include "recoil/host_canvas.h"
#include "app/instrument_data.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    std::filesystem::path out(argv[1]);
    std::filesystem::create_directories(out);
    Simulator simulator;
    Roadbook roadbook;
    RaceController race;
    UiSettings settings;
    settings.use_kmh = true;
    simulator.reset(); roadbook.reset(); race.reset(0);
    HostCanvas a(160, 80), b(240, 135);
    uint32_t now = 0;
    for (int i = 0; i < 300; ++i) {
        const uint32_t target = 20000U + static_cast<uint32_t>(i) * 1000U / 15U;
        while (now < target) {
            now += target - now < 20U ? target - now : 20U;
            race.update(now, simulator, roadbook);
        }
        const auto p = InstrumentData::pilot(simulator.state, race.fx(), race.stats(), settings);
        const auto n = InstrumentData::navigator(simulator.state, race.fx(), settings);
        if (!Iq3Render::pilot(a, p) || !HdsRender::render(b, n)) return 3;
        char name[64];
        std::snprintf(name, sizeof(name), "%04d_pilot.ppm", i);
        if (!a.save((out / name).string().c_str())) return 4;
        std::snprintf(name, sizeof(name), "%04d_navigator.ppm", i);
        if (!b.save((out / name).string().c_str())) return 4;
    }
    if (a.clipped || b.clipped) {
        std::fprintf(stderr, "Unexpected clipping: A=%d B=%d\n", a.clipped, b.clipped);
        return 5;
    }
    std::puts("300 native frames / 20 seconds / shared simulator and renderers / no clipped pixels");
}
