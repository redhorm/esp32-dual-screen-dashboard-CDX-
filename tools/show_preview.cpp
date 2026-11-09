// Host entry point for the exact firmware renderer; no screen reconstruction.
#include "recoil/host_canvas.h"
#include "app/show_renderer.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2 || argc > 4) {
        std::fprintf(stderr, "Usage: show_preview OUTPUT_DIRECTORY [FPS=15] [en|it]\n");
        return 2;
    }
    const int fps = argc > 2 ? std::atoi(argv[2]) : 15;
    if (fps < 1 || fps > 15) { std::fputs("FPS must be 1..15\n", stderr); return 2; }
    const bool english = argc > 3 && std::string(argv[3]) == "en";
    const std::filesystem::path output(argv[1]);
    std::filesystem::create_directories(output);
    ShowSequence sequence; sequence.start(0);
    HostCanvas pilot(160, 80), navigator(240, 135);
    const int count = static_cast<int>(ShowSequence::kDurationMs / 1000U) * fps;
    for (int i = 0; i < count; ++i) {
        const uint32_t now = static_cast<uint32_t>(i) * 1000U / static_cast<uint32_t>(fps);
        const ShowFrame frame = sequence.frame(now);
        if (!ShowRender::pilot(pilot, frame, english) || !ShowRender::navigator(navigator, frame, english)) return 3;
        char name[64];
        std::snprintf(name, sizeof(name), "%04d_pilot.ppm", i);
        if (!pilot.save((output / name).string().c_str())) return 4;
        std::snprintf(name, sizeof(name), "%04d_navigator.ppm", i);
        if (!navigator.save((output / name).string().c_str())) return 4;
    }
    if (pilot.clipped || navigator.clipped) {
        std::fprintf(stderr, "Unexpected clipped pixels: pilot=%d navigator=%d\n", pilot.clipped, navigator.clipped);
        return 5;
    }
    std::printf("%d frames from shared C++ renderer; %d fps; 18 seconds; no clipped pixels.\n", count, fps);
}
