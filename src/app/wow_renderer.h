#pragma once
// Original stormDASH WOW graphics. No heap allocation and no external assets.
// All primitives pass through the physical canvas/mask clipping contract.
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "core/wow_state.h"
#include "instrument_pilot.h"

namespace WowRender {
using RecoilRender::rgb;
using RecoilRender::text;
constexpr float kPi = 3.14159265358979323846f;
constexpr uint16_t BG = rgb(5, 8, 20), PANEL = rgb(13, 19, 37),
                   DIM = rgb(34, 45, 67), CYAN = rgb(64, 237, 231),
                   VIOLET = rgb(160, 103, 255), PINK = rgb(255, 88, 159),
                   WHITE = rgb(237, 246, 255), AMBER = rgb(255, 190, 77);
inline float clamp(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
inline int imin(int a, int b) { return a < b ? a : b; }
inline int imax(int a, int b) { return a > b ? a : b; }
inline uint16_t fade(uint16_t color, float value) {
  value = clamp(value, 0.f, 1.f);
  return rgb(int(((color >> 11) & 31) * 255 / 31 * value),
             int(((color >> 5) & 63) * 255 / 63 * value),
             int((color & 31) * 255 / 31 * value));
}
inline uint16_t accent(uint8_t palette, int index = 0) {
  const uint16_t colors[4][4] = {{CYAN, VIOLET, PINK, AMBER},
                               {AMBER, PINK, VIOLET, CYAN},
                               {VIOLET, CYAN, AMBER, PINK},
                               {PINK, AMBER, CYAN, VIOLET}};
  return colors[palette % 4][index & 3];
}
template <class C> struct Clip {
  C &raw; int l, t, r, b;
  Clip(C &canvas, int left, int top, int right, int bottom)
      : raw(canvas), l(left), t(top), r(right), b(bottom) {}
  void fillRect(int x, int y, int w, int h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    int right = imin(x + w, r), bottom = imin(y + h, b);
    x = imax(x, l); y = imax(y, t);
    if (right > x && bottom > y) raw.fillRect(x, y, right - x, bottom - y, color);
  }
  void fillScreen(uint16_t color) { fillRect(l, t, r - l, b - t, color); }
};
template <class C> void line(C &c, int x, int y, int ex, int ey, uint16_t color) {
  int dx = abs(ex - x), sx = x < ex ? 1 : -1;
  int dy = -abs(ey - y), sy = y < ey ? 1 : -1, error = dx + dy;
  for (;;) {
    c.fillRect(x, y, 1, 1, color);
    if (x == ex && y == ey) break;
    int twice = 2 * error;
    if (twice >= dy) { error += dy; x += sx; }
    if (twice <= dx) { error += dx; y += sy; }
  }
}
template <class C> void box(C &c, int x, int y, int w, int h, uint16_t color) {
  c.fillRect(x, y, w, 1, color); c.fillRect(x, y + h - 1, w, 1, color);
  c.fillRect(x, y, 1, h, color); c.fillRect(x + w - 1, y, 1, h, color);
}
template <class C> void disk(C &c, int x, int y, int r, uint16_t color) {
  RecoilRender::disk(c, x, y, r, color);
}
template <class C> void ellipse(C &c, int x, int y, int rx, int ry, uint16_t color) {
  if (ry <= 0 || rx <= 0) return;
  for (int dy = -ry; dy <= ry; ++dy) {
    int span = int(rx * sqrtf(clamp(1.f - float(dy * dy) / float(ry * ry), 0, 1)));
    c.fillRect(x - span, y + dy, span * 2 + 1, 1, color);
  }
}
struct P2 { int x, y; };
struct V3 { float x, y, z; };
template <class C> void triangle(C &c, P2 a, P2 b, P2 d, uint16_t color) {
  // Scan conversion uses bounded local triangles, no framebuffer or allocations.
  if (a.y > b.y) { P2 p = a; a = b; b = p; }
  if (b.y > d.y) { P2 p = b; b = d; d = p; }
  if (a.y > b.y) { P2 p = a; a = b; b = p; }
  if (a.y == d.y) { c.fillRect(imin(a.x, imin(b.x, d.x)), a.y,
      imax(a.x, imax(b.x, d.x)) - imin(a.x, imin(b.x, d.x)) + 1, 1, color); return; }
  for (int y = a.y; y <= d.y; ++y) {
    int x1 = a.x + (d.x - a.x) * (y - a.y) / (d.y - a.y);
    int x2;
    if (y < b.y && b.y != a.y) x2 = a.x + (b.x - a.x) * (y - a.y) / (b.y - a.y);
    else if (d.y != b.y) x2 = b.x + (d.x - b.x) * (y - b.y) / (d.y - b.y);
    else x2 = b.x;
    c.fillRect(imin(x1, x2), y, abs(x2 - x1) + 1, 1, color);
  }
}
template <class C> void header(C &c, const char *title, const char *right, uint16_t color) {
  c.fillRect(0, 0, 240, 19, BG);
  text(c, title, 84, 5, color, 2);
  text(c, right, 207, 8, WHITE);
  c.fillRect(0, 18, 240, 1, DIM);
}
template <class C> void footer(C &c, const char *label, uint16_t color = VIOLET) {
  c.fillRect(0, 123, 240, 12, BG); text(c, label, 120, 127, color);
}
template <class C> void stars(C &c, uint32_t ms) {
  for (int i = 0; i < 27; ++i) {
    int x = (i * 73 + 11) % 240, y = 22 + (i * 29) % 78;
    uint16_t col = (ms / 420 + i) % 7 == 0 ? VIOLET : DIM;
    c.fillRect(x, y, 1, 1, col);
  }
}

inline float roadCenter(const Wow::Snapshot &s, float perspective) {
  return 120 + s.curve * 60 * (1 - perspective) * (1 - perspective) - s.car * 5;
}
template <class C> void raceCar(C &c, int x, int y, int size, uint16_t color, bool rear) {
  int half = imax(size / 2, 2), height = imax(size * 2 / 3, 3);
  c.fillRect(x - half - 2, y + height / 3, 3, height * 2 / 3, BG);
  c.fillRect(x + half - 1, y + height / 3, 3, height * 2 / 3, BG);
  triangle(c, {x - half, y + height}, {x - half + size / 5, y}, {x + half, y + height}, color);
  triangle(c, {x + half, y + height}, {x - half + size / 5, y}, {x + half - size / 5, y}, color);
  c.fillRect(x - half + size / 4, y + 2, size / 2, imax(height / 3, 1), PANEL);
  c.fillRect(x - half, y + height - 2, size, 2, fade(color, .6f));
  c.fillRect(x - half + 1, y + height - 3, imax(size / 4, 1), 2, rear ? PINK : WHITE);
  c.fillRect(x + half - imax(size / 4, 1), y + height - 3, imax(size / 4, 1), 2, rear ? PINK : WHITE);
}
template <class C> void midnight(C &c, const Wow::Snapshot &s) {
  const float time = s.ms * .001f;
  const int horizon = 49 + int(sinf(time * .39f) * 6);
  const bool tunnel = (s.elapsed / 12000) % 4 == 2;
  Clip<C> scene(c, 0, 19, 240, 123);
  for (int y = 19; y < horizon; ++y) {
    int t = (y - 19) * 100 / imax(horizon - 19, 1);
    scene.fillRect(0, y, 240, 1, rgb(29 + t, 12 + t / 4, 64 + t / 3));
  }
  stars(scene, s.ms);
  disk(scene, 178 - int(s.curve * 22), horizon - 8, 19, PINK);
  for (int y = horizon - 4; y < horizon + 15; y += 4) scene.fillRect(145, y, 68, 2, rgb(92, 29, 74));
  for (int i = 0; i < 8; ++i) {
    int x = i * 39 - int(s.curve * 14) - 18;
    int h = 7 + (i * 13) % 17;
    triangle(scene, {x, horizon}, {x + 24, horizon - h}, {x + 48, horizon}, rgb(39, 26, 65));
  }
  for (int y = horizon; y < 123; ++y) {
    float p = float(y - horizon) / (123 - horizon);
    int center = int(roadCenter(s, p)), half = 3 + int(p * p * 112);
    int stripe = int((1 - p) * (1 - p) * 70 + s.distance * .13f) & 1;
    scene.fillRect(0, y, 240, 1, stripe ? rgb(20, 22, 46) : rgb(27, 25, 52));
    scene.fillRect(center - half - 3, y, 2 * half + 6, 1, stripe ? PINK : VIOLET);
    scene.fillRect(center - half, y, half * 2, 1, stripe ? rgb(38, 41, 61) : rgb(34, 37, 57));
    if (stripe && p > .11f) for (int j = -1; j <= 1; j += 2)
      scene.fillRect(center + j * half / 3, y, imax(int(p * 2), 1), 1, WHITE);
  }
  if (tunnel) {
    // Perspective ribs move toward the viewer; the centre remains readable.
    for (int r = 0; r < 6; ++r) {
      float phase = fmodf(r / 6.f + time * .22f, 1.f);
      float p = phase * phase;
      int half = 17 + int(p * 139), top = horizon - 7 - int(p * 38), base = horizon + int(p * 82);
      int center = int(roadCenter(s, 1 - phase));
      line(scene, center - half, base, center - half, top + 11, fade(CYAN, .18f + p * .75f));
      line(scene, center - half, top + 11, center - half + 13, top, fade(CYAN, .18f + p * .75f));
      line(scene, center - half + 13, top, center + half - 13, top, fade(VIOLET, .25f + p * .65f));
      line(scene, center + half - 13, top, center + half, top + 11, fade(CYAN, .18f + p * .75f));
      line(scene, center + half, top + 11, center + half, base, fade(CYAN, .18f + p * .75f));
    }
  }
  float op = 1.f - clamp(s.obstacleZ, 0, 1);
  int oy = horizon + int(op * (119 - horizon));
  int oh = 3 + int(op * op * 112);
  int ox = int(roadCenter(s, op) + s.obstacleLane * oh * .78f);
  raceCar(scene, ox, oy - 5 - int(op * 13), 4 + int(op * 16), AMBER, true);
  int carx = 120 + int(s.car * 68), cary = 101 + int(sinf(time * 31) * (s.paused ? 0 : 1));
  ellipse(scene, carx, 119, 18, 3, BG);
  raceCar(scene, carx, cary, 27, CYAN, true);
  line(scene, carx - 10, cary - 1, carx + 10, cary - 1, WHITE);
  if (s.energy > .5f) { box(scene, 1, 20, 238, 102, PINK); box(scene, 2, 21, 236, 100, PINK); }
  char b[24]; snprintf(b, sizeof b, "%02lu S", (unsigned long)((60000 - (s.elapsed > 60000 ? 60000 : s.elapsed) + 999) / 1000));
  header(c, "MIDNIGHT RUN", b, CYAN);
  snprintf(b, sizeof b, "URTI %u  PT %lu  ARCADE", s.hits, (unsigned long)s.score);
  footer(c, b, WHITE);
  if (s.paused || s.done) {
    c.fillRect(28, 53, 184, 41, BG); box(c, 28, 53, 184, 41, s.done ? AMBER : CYAN);
    text(c, s.done ? "ARRIVO!" : "PAUSA", 120, 61, WHITE, 2);
    text(c, s.done ? "RIAVVIA DAL TELEFONO" : "COMANDI SUL TELEFONO", 120, 81, VIOLET);
  }
}

template <class C> void living(C &c, const Wow::Snapshot &s) {
  stars(c, s.ms);
  const float time = s.ms * .001f;
  const uint16_t color = s.mood == 2 ? AMBER : s.mood >= 3 ? PINK : CYAN;
  for (int i = 0; i < 4; ++i) {
    int y = 33 + i * 23;
    c.fillRect(18, y, 5, 1, DIM); c.fillRect(217, y, 5, 1, DIM);
  }
  int bob = int(sinf(time * 2.4f) * 2);
  int open = imax(2, int(23 * (1 - s.blink)));
  for (int side = 0; side < 2; ++side) {
    int x = 80 + side * 80, y = 69 + bob + (s.mood == 2 ? side * 2 : 0);
    ellipse(c, x, y, 31, open + 4, fade(color, .12f));
    ellipse(c, x, y, 26, open + 1, fade(color, .35f));
    ellipse(c, x, y, 22, open, color);
    if (open > 6) {
      int gx = int(s.gazeX * 9), gy = int(s.gazeY * 7);
      ellipse(c, x + gx, y + gy, 9, imax(3, open - 6), BG);
      disk(c, x + gx - 4, y + gy - 6, 3, WHITE);
      c.fillRect(x + gx + 4, y + gy + 4, 2, 2, fade(WHITE, .6f));
    }
    if (s.mood == 1 || s.mood >= 3) {
      line(c, x - 17, y - 31, x - 5, y - 36, color);
      line(c, x - 5, y - 36, x + 12, y - 33, color);
    } else if (s.mood == 2) {
      line(c, x - 16, y - 29 - side * 4, x + 16, y - 33 + side * 4, AMBER);
    }
  }
  if (s.mood == 2) {
    line(c, 111, 104, 120, 101, color); line(c, 120, 101, 129, 104, color);
  } else {
    line(c, 108, 99 + bob, 114, 104 + bob, color);
    line(c, 114, 104 + bob, 126, 104 + bob, color);
    line(c, 126, 104 + bob, 132, 99 + bob, color);
    if (s.mood >= 3) ellipse(c, 120, 106 + bob, 7, 3, PINK);
  }
  if (s.mood == 1 || s.mood >= 3) {
    ellipse(c, 48, 87 + bob, 7, 2, fade(PINK, .65f));
    ellipse(c, 192, 87 + bob, 7, 2, fade(PINK, .65f));
  }
  if (s.mood == 1 || s.mood >= 3) for (int i = 0; i < 12; ++i) {
    float p = fmodf(time * .4f + i * .13f, 1.f);
    int x = 20 + (i * 47) % 200, y = 113 - int(p * 80);
    c.fillRect(x, y, 2, 3, accent(i));
  }
  const char *moods[] = {"CIAO PILOTA", "BEL GATE!", "SI RIPARTE", "TRAIL FINITO", "NUOVO RECORD"};
  header(c, "LIVING COCKPIT", "LIVE", color);
  footer(c, moods[s.mood < 5 ? s.mood : 0], color);
}

template <class C> void liquid(C &c, const Wow::Snapshot &s) {
  const float time = s.ms * .001f;
  const uint16_t color = accent(s.palette);
  stars(c, s.ms);
  Clip<C> tank(c, 14, 26, 226, 116);
  for (int y = 35; y < 116; y += 16) line(tank, 14, y, 225, y, rgb(10, 30, 45));
  for (int x = 17; x < 226; x += 16) line(tank, x, 26, x, 115, rgb(10, 30, 45));
  for (int x = 14; x < 226; x += 2) {
    float wave = sinf(x * .043f + time * 1.7f) * 5 + sinf(x * .073f - time * 1.1f) * 3;
    int top = 91 + int(wave * (1 + s.energy));
    tank.fillRect(x, top, 2, 116 - top, fade(color, .13f));
    tank.fillRect(x, top, 2, 2, fade(color, .7f));
  }
  // Forty-eight moving particles with a luminous core, connected only locally.
  for (int i = 0; i < 48; ++i) {
    int x = 20 + int(clamp(s.particles[i].x, 0, 1) * 198), y = 31 + int(clamp(s.particles[i].y, 0, 1) * 78);
    if (i > 0) {
      int px = 20 + int(s.particles[i - 1].x * 198), py = 31 + int(s.particles[i - 1].y * 78);
      int dx = x - px, dy = y - py;
      if (dx * dx + dy * dy < 325) line(tank, x, y, px, py, fade(color, .28f));
    }
    disk(tank, x, y, 5, fade(color, .16f));
    disk(tank, x, y, 3, fade(color, .50f));
    disk(tank, x, y, 1 + (i % 7 == 0), i % 7 ? color : WHITE);
  }
  if (s.energy > .1f) {
    float radius = (1 - s.energy) * 94;
    int ox = 120 + int(s.gazeX * 90), oy = 71 + int(s.gazeY * 34);
    P2 old{ox + int(radius), oy};
    for (int i = 1; i <= 32; ++i) {
      float a = i * kPi / 16;
      P2 p{ox + int(cosf(a) * radius), oy + int(sinf(a) * radius * .55f)};
      line(tank, old.x, old.y, p.x, p.y, fade(WHITE, s.energy * .7f)); old = p;
    }
  }
  box(c, 13, 25, 214, 92, fade(color, .42f));
  c.fillRect(13, 25, 21, 2, color); c.fillRect(205, 115, 22, 2, color);
  header(c, "LIQUID REACTOR", "48 PT", color);
  footer(c, "TRASCINA / BOOT = ONDA", color);
}

struct Camera {
  float cy, sy, ct, st, scale; int x, y;
  Camera(float yaw, float tilt, float size, int cx, int yy)
      : cy(cosf(yaw)), sy(sinf(yaw)), ct(cosf(tilt)), st(sinf(tilt)), scale(size), x(cx), y(yy) {}
  P2 project(V3 p) const {
    float xx = p.x * cy + p.z * sy, zz = -p.x * sy + p.z * cy;
    float yy = p.y * ct - zz * st;
    float depth = 4.2f + p.y * st + zz * ct;
    float k = scale * 4.2f / (depth < 1 ? 1 : depth);
    return {x + int(xx * k), y - int(yy * k)};
  }
};
template <class C> void edge3(C &c, const Camera &cam, V3 a, V3 b, uint16_t color) {
  P2 p = cam.project(a), q = cam.project(b); line(c, p.x, p.y, q.x, q.y, color);
}
template <class C> void buggy3d(C &c, float yaw, float tilt, float explode, float size, int cx, int top, int bottom, bool detail) {
  const float e = clamp(explode, 0, 1);
  // Fit conservative wheel/cage envelopes before drawing. Full explosion and
  // every orbit angle remain inside their own view, clear of all labels.
  const float sineYaw = sinf(yaw), cosineYaw = cosf(yaw);
  const float sineTilt = sinf(tilt), cosineTilt = cosf(tilt);
  float minY = 1000, maxY = -1000, maxX = 0;
  for (int group = 0; group < 3; ++group) {
    float halfX = group == 2 ? .52f : group ? .45f : .73f + e * .48f;
    float nearZ = group == 2 ? -.89f : group ? -.30f : -.80f;
    float farZ = group == 2 ? .66f : group ? .45f : .75f;
    float lowY = group ? .02f : -.34f - e * .12f;
    float highY = group == 2 ? .28f + e * .12f : group ? .77f + e * .65f : .14f - e * .12f;
    for (int i = 0; i < 8; ++i) {
      float x = i & 1 ? halfX : -halfX, y = i & 2 ? highY : lowY;
      float z = i & 4 ? farZ : nearZ;
      float xx = x * cosineYaw + z * sineYaw, zz = -x * sineYaw + z * cosineYaw;
      float yy = y * cosineTilt - zz * sineTilt;
      float depth = 4.2f + y * sineTilt + zz * cosineTilt;
      float py = yy * 4.2f / depth, px = fabsf(xx * 4.2f / depth);
      minY = py < minY ? py : minY; maxY = py > maxY ? py : maxY;
      maxX = px > maxX ? px : maxX;
    }
  }
  float fit = float(bottom - top) / (maxY - minY);
  if (fit < size) size = fit;
  float widthFit = (detail ? 109.f : 61.f) / maxX;
  if (widthFit < size) size = widthFit;
  int cy = int((top + bottom) * .5f + (minY + maxY) * size * .5f);
  Camera cam(yaw, tilt, size, cx, cy);
  const V3 body[] = {{-.45f, .02f + e * .08f, -.88f}, {.45f, .02f + e * .08f, -.88f},
      {.52f, .02f + e * .08f, .65f}, {-.52f, .02f + e * .08f, .65f},
      {-.32f, .28f + e * .12f, -.65f}, {.32f, .28f + e * .12f, -.65f},
      {.45f, .25f + e * .12f, .46f}, {-.45f, .25f + e * .12f, .46f}};
  // A tiny original low-poly shell. Sort its six quads by camera depth.
  const uint8_t faces[6][4] = {{0, 1, 2, 3}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}, {4, 5, 6, 7}};
  uint8_t order[6] = {0, 1, 2, 3, 4, 5}; float depth[6];
  for (int i = 0; i < 6; ++i) {
    depth[i] = 0;
    for (int j = 0; j < 4; ++j) { const V3 &p = body[faces[i][j]]; depth[i] += (-p.x * cam.sy + p.z * cam.cy) * cam.ct + p.y * cam.st; }
  }
  for (int i = 0; i < 6; ++i) for (int j = i + 1; j < 6; ++j)
    if (depth[order[i]] < depth[order[j]]) { uint8_t a = order[i]; order[i] = order[j]; order[j] = a; }
  for (int k = 0; k < 6; ++k) {
    int face = order[k]; const uint8_t *f = faces[face];
    P2 a = cam.project(body[f[0]]), b = cam.project(body[f[1]]), d = cam.project(body[f[2]]), q = cam.project(body[f[3]]);
    uint16_t color = face == 5 ? rgb(50, 66, 100) : rgb(17 + face * 4, 25 + face * 5, 48 + face * 6);
    triangle(c, a, b, d, color); triangle(c, a, d, q, color);
    line(c, a.x, a.y, b.x, b.y, CYAN); line(c, b.x, b.y, d.x, d.y, CYAN);
    line(c, d.x, d.y, q.x, q.y, CYAN); line(c, q.x, q.y, a.x, a.y, CYAN);
  }
  float roof = .67f + e * .65f, post = .24f + e * .34f;
  const V3 cage[] = {{-.36f, post, -.30f}, {.36f, post, -.30f}, {.36f, post, .45f}, {-.36f, post, .45f},
      {-.31f, roof, -.05f}, {.31f, roof, -.05f}, {.31f, roof, .35f}, {-.31f, roof, .35f}};
  for (int i = 0; i < 4; ++i) {
    edge3(c, cam, cage[i], cage[i + 4], WHITE);
    edge3(c, cam, cage[i + 4], cage[((i + 1) % 4) + 4], WHITE);
  }
  edge3(c, cam, cage[4], cage[6], fade(VIOLET, .8f));
  edge3(c, cam, cage[5], cage[7], fade(VIOLET, .8f));
  for (int side = -1; side <= 1; side += 2) for (int end = -1; end <= 1; end += 2) {
    float wx = side * (.59f + e * .48f), wz = end < 0 ? -.56f : .51f;
    float wy = -.10f - e * .12f;
    edge3(c, cam, {side * .25f, .03f, wz}, {wx, wy, wz}, AMBER);
    edge3(c, cam, {side * .43f, .23f, wz}, {wx, wy, wz}, AMBER);
    for (int ring = 0; ring < 2; ++ring) {
      float xx = wx + side * ring * .14f;
      for (int i = 0; i < 10; ++i) {
        float a = i * kPi / 5, b = (i + 1) * kPi / 5;
        V3 p{xx, wy + cosf(a) * .23f, wz + sinf(a) * .23f};
        V3 q{xx, wy + cosf(b) * .23f, wz + sinf(b) * .23f};
        edge3(c, cam, p, q, ring ? VIOLET : DIM);
        if (ring && i % 2 == 0) edge3(c, cam, {xx, wy, wz}, p, fade(WHITE, .65f));
        if (detail && ring) edge3(c, cam, p, {wx, p.y, p.z}, DIM);
      }
    }
  }
  edge3(c, cam, {-.36f, .11f, -.9f}, {-.20f, .11f, -.9f}, WHITE);
  edge3(c, cam, {.20f, .11f, -.9f}, {.36f, .11f, -.9f}, WHITE);
  if (e > .1f && detail) for (int side = -1; side <= 1; side += 2)
    edge3(c, cam, {side * .44f, .27f, .3f}, {side * .44f, roof + .1f, .3f}, fade(PINK, .6f));
}
template <class C> void garage(C &c, const Wow::Snapshot &s) {
  Clip<C> scene(c, 0, 19, 240, 123);
  stars(scene, s.ms);
  Camera grid(s.yaw, .36f, 49, 120, 94);
  for (int i = -4; i <= 4; ++i) {
    float offset = i * .4f;
    edge3(scene, grid, {offset, -.39f, -1.6f}, {offset, -.39f, 1.6f}, rgb(18, 33, 46));
    edge3(scene, grid, {-1.6f, -.39f, offset}, {1.6f, -.39f, offset}, rgb(18, 33, 46));
  }
  buggy3d(scene, s.yaw, s.tilt, s.exploded, 44, 120, 24, 119, true);
  c.fillRect(9, 27, 18, 1, CYAN); c.fillRect(9, 27, 1, 16, CYAN);
  c.fillRect(212, 112, 18, 1, CYAN); c.fillRect(229, 97, 1, 16, CYAN);
  header(c, "GARAGE 3D", s.exploded > .5f ? "ESPLOSO" : "BUGGY", CYAN);
  footer(c, "TRASCINA / BOOT = ESPLOSO", VIOLET);
}

template <class C> void beat(C &c, const Wow::Snapshot &s) {
  Clip<C> scene(c, 0, 19, 240, 123);
  stars(scene, s.ms);
  const float time = s.ms * .001f;
  const uint16_t color = accent(s.palette);
  for (int y = 38; y < 119; y += 20) scene.fillRect(10, y, 220, 1, rgb(17, 26, 43));
  for (int i = 0; i < 16; ++i) {
    int band = i / 4;
    float level = s.bands[band] / 255.f;
    int h = imax(2, int(level * (70 + 10 * sinf(i * .8f + time * 3))));
    int x = 10 + i * 14;
    uint16_t col = accent(s.palette, band);
    for (int row = 0; row < h; row += 4)
      scene.fillRect(x, 116 - row, 10, 3, fade(col, .30f + row / 100.f));
    scene.fillRect(x, 114 - h, 10, 2, col);
  }
  float radius = 17 + s.energy * 12;
  P2 last{0, 0};
  for (int i = 0; i <= 32; ++i) {
    float a = i * kPi / 16 + time * .28f;
    float r = radius + s.bands[(i / 8) & 3] / 255.f * (7 + sinf(i * 1.5f + time * 5) * 5);
    P2 p{120 + int(cosf(a) * r), 66 + int(sinf(a) * r * .70f)};
    if (i) line(scene, last.x, last.y, p.x, p.y, fade(color, .9f));
    last = p;
  }
  if (s.audioActive) {
    disk(scene, 120, 66, 11, BG); line(scene, 116, 61, 116, 72, WHITE); line(scene, 123, 58, 123, 69, WHITE);
    line(scene, 116, 61, 123, 58, WHITE); disk(scene, 114, 72, 2, WHITE); disk(scene, 121, 69, 2, WHITE);
  } else {
    c.fillRect(36, 48, 168, 38, BG); box(c, 36, 48, 168, 38, DIM);
    text(c, "SCEGLI UN BRANO", 120, 56, WHITE, 2);
    text(c, "AUDIO DAL TELEFONO", 120, 73, color);
  }
  header(c, "BEAT STORM", s.audioActive ? "AUDIO" : "ATTESA", color);
  footer(c, "4 BANDE / 16 PUNTI OTTICI", color);
}

template <class C> void navigator(C &raw, const Wow::Snapshot &s) {
  Clip<C> c(raw, 0, 0, 240, 135); c.fillScreen(BG);
  switch (s.kind) {
  case Wow::Kind::Midnight: midnight(c, s); break;
  case Wow::Kind::Living: living(c, s); break;
  case Wow::Kind::Liquid: liquid(c, s); break;
  case Wow::Kind::Garage: garage(c, s); break;
  case Wow::Kind::Beat: beat(c, s); break;
  }
}

template <class C> void pilot(C &raw, const Wow::Snapshot &s) {
  raw.fillScreen(0);
  Iq3Render::Window<C> mask(raw);
  Clip<Iq3Render::Window<C>> c(mask, 0, 0, 160, 80); c.fillScreen(BG);
  uint16_t color = s.kind == Wow::Kind::Living ? (s.mood == 2 ? AMBER : s.mood >= 3 ? PINK : CYAN) : accent(s.palette);
  char b[28];
  switch (s.kind) {
  case Wow::Kind::Midnight: {
    text(c, "MIDNIGHT / ARCADE", 80, 37, CYAN);
    snprintf(b, sizeof b, "%02lu", (unsigned long)((60000 - (s.elapsed > 60000 ? 60000 : s.elapsed) + 999) / 1000));
    text(c, b, 80, 48, WHITE, 4);
    snprintf(b, sizeof b, "%u URTI", s.hits); text(c, b, 32, 56, PINK);
    snprintf(b, sizeof b, "%lu PT", (unsigned long)s.score); text(c, b, 128, 56, CYAN);
    text(c, s.done ? "ARRIVO" : s.paused ? "PAUSA" : "STERZA DAL TELEFONO", 80, 71, WHITE);
    break;
  }
  case Wow::Kind::Living: {
    text(c, "LIVING COCKPIT", 80, 37, color);
    float t = s.ms * .001f; int phase = int(t * 38) % 120;
    for (int x = 15; x < 146; ++x) {
      int p = (x - phase + 240) % 120;
      int wave = p < 5 ? p * 2 : p < 9 ? (9 - p) * 3 : p < 13 ? -(p - 9) * 3 : p < 17 ? -(17 - p) * 3 : 0;
      c.fillRect(x, 58 - wave, 1, 2, fade(color, .3f + .7f * float(x) / 145));
    }
    disk(c, 80, 57, 3 + int((1 + sinf(t * 6)) * 2), PINK);
    text(c, s.mood == 2 ? "VAI TRANQUILLO" : s.mood ? "BELLA, PILOTA!" : "SONO QUI", 80, 71, WHITE);
    break;
  }
  case Wow::Kind::Liquid: {
    text(c, "REACTOR / 48", 80, 37, color);
    for (int x = 19; x < 143; ++x) {
      int top = 57 + int(sinf(x * .09f + s.ms * .003f) * (3 + s.energy * 6));
      c.fillRect(x, top, 1, 70 - top, fade(color, .25f)); c.fillRect(x, top, 1, 2, color);
    }
    for (int i = 0; i < 48; i += 3) disk(c, 22 + int(s.particles[i].x * 116), 49 + int(s.particles[i].y * 18), 1, WHITE);
    text(c, "BOOT = ONDA", 80, 71, WHITE); break;
  }
  case Wow::Kind::Garage: {
    text(c, "GARAGE / VISTA 2", 80, 34, CYAN);
    Clip<Clip<Iq3Render::Window<C>>> model(c, 9, 42, 151, 69);
    buggy3d(model, s.yaw + kPi * .5f, s.tilt, s.exploded, 18, 80, 43, 68, false);
    text(c, s.exploded > .5f ? "ESPLOSO" : "BUGGY 3D", 80, 71, WHITE); break;
  }
  case Wow::Kind::Beat: {
    text(c, "BEAT STORM", 80, 36, color);
    for (int i = 0; i < 16; ++i) {
      int band = i / 4, h = imax(1, s.bands[band] * (18 + i % 4) / 255);
      c.fillRect(19 + i * 8, 68 - h, 5, h, accent(s.palette, band));
      c.fillRect(19 + i * 8, 66 - h, 5, 1, WHITE);
    }
    text(c, s.audioActive ? "AUDIO TELEFONO" : "IN ATTESA AUDIO", 80, 71, WHITE); break;
  }
  }
  // Each emitted light is restricted to the original sixteen CAD apertures.
  // These are TFT optical points; no external WS2812 strip is assumed.
  for (int i = 0; i < 16; ++i) {
    const auto &p = RecoilGeometry::dots[i];
    float level = 0; uint16_t col = color;
    if (s.kind == Wow::Kind::Beat) {
      int band = i / 4; col = accent(s.palette, band);
      level = s.audioActive ? s.bands[band] / 255.f : 0;
    } else if (s.kind == Wow::Kind::Midnight) {
      level = s.energy > .5f ? .9f : (!s.paused && !s.done && (i < 8 ? i : 15 - i) < s.speed * 8) ? .75f : .06f;
      col = s.energy > .5f ? PINK : CYAN;
    } else {
      level = .1f + .65f * (.5f + .5f * sinf(s.ms * .003f - i * .45f)) + s.energy * .2f;
    }
    disk(raw, p.x, p.y, RecoilGeometry::DOT_RADIUS, fade(col, level));
  }
}
} // namespace WowRender
