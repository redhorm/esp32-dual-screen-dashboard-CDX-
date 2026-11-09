#include "recoil/host_canvas.h"
#include "app/wow_renderer.h"
#include <chrono>
#include <filesystem>
#include <iostream>

// Frame inputs simulate phone controls; pixels come from the production engine
// and production TFT renderer. This is a host preview, never a hardware capture.
static uint32_t xy(float x, float y) {
  unsigned a = unsigned(WowRender::clamp(x, 0, 1) * 255);
  unsigned b = unsigned(WowRender::clamp(y, 0, 1) * 255);
  return (a << 8) | b;
}
static bool apertureOnly(const HostCanvas &c) {
  for (int y = 0; y < 80; ++y) for (int x = 0; x < 160; ++x) {
    if (!c.at(x, y)) continue;
    if (RecoilGeometry::windowTop[x] != 255 && y >= RecoilGeometry::windowTop[x] && y <= RecoilGeometry::WINDOW_BOTTOM) continue;
    bool hole = false;
    for (const auto &p : RecoilGeometry::dots) {
      int dx = x - p.x, dy = y - p.y;
      if (dx * dx + dy * dy <= RecoilGeometry::DOT_RADIUS * RecoilGeometry::DOT_RADIUS) hole = true;
    }
    if (!hole) return false;
  }
  return true;
}
static void input(Wow::State &state, int scene, uint32_t t) {
  auto &s = state.s;
  if (scene == 0 && t % 80 == 0) {
    state.control(Wow::Control::Heartbeat, 0, t);
    float target = s.obstacleLane >= 0 ? -.62f : .62f;
    state.control(Wow::Control::Steer, s.car < target - .08f ? 2 : s.car > target + .08f ? 0 : 1, t);
  } else if (scene == 1) {
    if (t % 80 == 0) state.control(Wow::Control::Touch, xy(.5f + .38f * sinf(t * .0013f), .5f + .3f * cosf(t * .0009f)), t);
    if (t == 1500) state.react(1, t);
    if (t == 3500) state.react(2, t);
    if (t == 5500) state.react(3, t);
    if (t == 7500) state.react(4, t);
    if (t == 9000) state.control(Wow::Control::Impulse, 0, t);
  } else if (scene == 2) {
    if (t == 1800 || t == 5200 || t == 8200) state.control(Wow::Control::Impulse, 0, t);
    if (t > 2800 && t < 7600 && t % 80 == 0)
      state.control(Wow::Control::Touch, xy(.5f + .32f * sinf(t * .0014f), .5f + .27f * cosf(t * .0013f)), t);
  } else if (scene == 3) {
    if (t % 80 == 0) state.control(Wow::Control::Orbit, xy(.5f + .45f * sinf(t * .0004f), .65f), t);
    if (t == 2400) state.control(Wow::Control::Explode, 1, t);
    if (t == 7000) state.control(Wow::Control::Explode, 0, t);
  } else if (scene == 4) {
    if (t < 9000 && t % 80 == 0) {
      uint32_t bands = 0;
      for (int i = 0; i < 4; ++i) {
        float wave = .5f + .5f * sinf(t * (.006f + i * .0011f) + i * 1.4f);
        uint32_t amplitude = uint32_t(35 + 210 * wave * wave);
        bands |= amplitude << (8 * i);
      }
      state.control(Wow::Control::Audio, bands, t);
    }
    if (t == 4000 || t == 7000) state.control(Wow::Control::Impulse, 0, t);
  }
}
int main(int argc, char **argv) {
  if (argc != 2) return 2;
  std::filesystem::create_directories(argv[1]);
  HostCanvas a(160, 80), b(240, 135);
  unsigned rendered = 0, checked = 0, boundsChecked = 0;
  auto start = std::chrono::steady_clock::now();
  for (int scene = 0; scene < 5; ++scene) {
    Wow::State state; state.enter(Wow::Kind(scene), 0, 42);
    uint32_t now = 0;
    for (int frame = 0; frame < 120; ++frame) {
      // Midnight's last four seconds show the tunnel section at race 24 s.
      uint32_t target = uint32_t(frame * 1000 / 12);
      if (scene == 0 && frame >= 72) target += 18000;
      while (now < target) {
        now += 20; input(state, scene, now); state.tick(now);
      }
      WowRender::pilot(a, state.s); WowRender::navigator(b, state.s);
      if (!apertureOnly(a)) { std::cerr << "Pilot aperture leak scene " << scene << " frame " << frame << '\n'; return 4; }
      char path[1024];
      snprintf(path, sizeof path, "%s/%d-%03d-a.ppm", argv[1], scene, frame); if (!a.save(path)) return 5;
      snprintf(path, sizeof path, "%s/%d-%03d-b.ppm", argv[1], scene, frame); if (!b.save(path)) return 5;
      ++rendered; ++checked;
    }
    // Broad engine-driven render coverage: timeout, all events/palettes, orbit
    // extremes, pause/resume and the full 60-second race including completion.
    for (unsigned frame = 0; frame < 2000; ++frame) {
      now += 40;
      if (scene == 0) {
        state.control(Wow::Control::Heartbeat, 0, now);
        state.control(Wow::Control::Steer, (frame / 40) % 3, now);
        if (frame == 20 || frame == 60) state.control(Wow::Control::Impulse, 0, now);
      } else {
        if (frame % 31 == 0) state.control(Wow::Control::Impulse, 0, now);
        if (frame % 47 == 0) state.react((frame / 47) % 5, now);
        if (scene == 1 || scene == 2) state.control(Wow::Control::Touch, xy(float(frame % 129) / 128, float(frame % 173) / 172), now);
        if (scene == 3) state.control(Wow::Control::Orbit, xy(float(frame % 129) / 128, float(frame % 173) / 172), now);
        if (scene == 4) state.control(Wow::Control::Audio, frame * 293749u, now);
      }
      state.tick(now);
      WowRender::pilot(a, state.s); WowRender::navigator(b, state.s);
      if (!apertureOnly(a)) { std::cerr << "Stress aperture leak scene " << scene << '\n'; return 4; }
      ++checked;
    }
  }
  for (int yaw = 0; yaw < 360; yaw += 10) for (int tilt = -1; tilt <= 1; ++tilt) for (int explosion = 0; explosion <= 2; ++explosion) {
    a.fillScreen(0); b.fillScreen(0);
    WowRender::buggy3d(a, yaw * WowRender::kPi / 180, tilt * .45f, explosion * .5f, 18, 80, 43, 68, false);
    WowRender::buggy3d(b, yaw * WowRender::kPi / 180, tilt * .45f, explosion * .5f, 44, 120, 24, 119, true);
    for (int y = 0; y < 80; ++y) if (y < 43 || y > 68) for (int x = 0; x < 160; ++x)
      if (a.at(x, y)) { std::cerr << "Pilot model overlaps labels " << yaw << " " << tilt << " " << explosion; return 6; }
    for (int y = 0; y < 135; ++y) if (y < 24 || y > 119) for (int x = 0; x < 240; ++x)
      if (b.at(x, y)) { std::cerr << "Navigator model overlaps labels " << yaw << " " << tilt << " " << explosion; return 6; }
    ++boundsChecked;
  }
  Wow::State extreme; extreme.enter(Wow::Kind::Garage, 0);
  extreme.s.yaw = 52 * WowRender::kPi / 180; extreme.s.tilt = -.45f; extreme.s.exploded = 1;
  WowRender::pilot(a, extreme.s); WowRender::navigator(b, extreme.s);
  char path[1024]; snprintf(path, sizeof path, "%s/5-000-a.ppm", argv[1]); if (!a.save(path)) return 5;
  snprintf(path, sizeof path, "%s/5-000-b.ppm", argv[1]); if (!b.save(path)) return 5;
  double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  std::cout << "{\"preview_pairs\":" << rendered << ",\"checked_pairs\":" << checked
            << ",\"pilot_out_of_bounds\":" << a.clipped << ",\"navigator_out_of_bounds\":" << b.clipped
            << ",\"pilot_aperture_leaks\":0,\"garage_label_bounds_pairs\":" << boundsChecked << ",\"host_elapsed_seconds\":" << seconds << "}\n";
  return a.clipped || b.clipped ? 3 : 0;
}
