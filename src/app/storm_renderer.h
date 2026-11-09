#pragma once
#include "core/storm_state.h"
#include "instrument_pilot.h"
#include "wow_renderer.h"
namespace StormRender {
using RecoilRender::rgb;
using RecoilRender::text;
template <class C> struct Bounds {
  C &c;
  int left, top, right, bottom;
  Bounds(C &raw, int l, int t, int r, int b)
      : c(raw), left(l), top(t), right(r), bottom(b) {}
  void fillRect(int x, int y, int w, int h, uint16_t color) {
    int end = x + w, rightY = y + h;
    if (x < left)
      x = left;
    if (y < top)
      y = top;
    if (end > right)
      end = right;
    if (rightY > bottom)
      rightY = bottom;
    if (end > x && rightY > y)
      c.fillRect(x, y, end - x, rightY - y, color);
  }
  void fillScreen(uint16_t color) {
    fillRect(left, top, right - left, bottom - top, color);
  }
};
constexpr uint16_t BG = rgb(7, 7, 17), PURPLE = rgb(147, 79, 255),
                   PINK = rgb(239, 66, 154), WHITE = rgb(239, 235, 255),
                   DIM = rgb(54, 38, 80);
template <class C> void buggy(C &c, int x, int y, uint16_t color = PURPLE) {
  c.fillRect(x + 3, y - 9, 12, 5, color);
  c.fillRect(x, y - 5, 24, 6, color);
  c.fillRect(x + 6, y - 8, 7, 3, BG);
  c.fillRect(x + 2, y, 6, 5, WHITE);
  c.fillRect(x + 17, y, 6, 5, WHITE);
  c.fillRect(x + 4, y + 1, 2, 3, BG);
  c.fillRect(x + 19, y + 1, 2, 3, BG);
}
template <class C> void symbol(C &c, int x, int y, int k = 1) {
  const uint8_t rows[] = {0x18, 0x38, 0x70, 0xfe, 0x1c, 0x38, 0x30, 0x20};
  for (int r = 0; r < 8; r++)
    for (int b = 0; b < 8; b++)
      if (rows[r] & (128 >> b))
        c.fillRect(x + b * k, y + r * k, k, k, PURPLE);
  c.fillRect(x, y + 9 * k, 3 * k, k, WHITE);
  c.fillRect(x + 5 * k, y + 9 * k, 3 * k, k, WHITE);
}
// Original 3x5 lowercase forms keep the exact product casing at tiny sizes.
template <class C> void logo(C &c, int center, int y, int scale = 1) {
  const char *word = "stormDASH";
  const uint16_t lower[] = {0b000011100010110, 0b010111010010011,
                            0b000000111101111, 0b000000110100100,
                            0b000000111111101};
  int x = center - (35 * scale) / 2;
  for (int i = 0; i < 9; i++, x += 4 * scale) {
    uint16_t bits = i < 5 ? lower[i] : RecoilRender::glyph(word[i]);
    for (int r = 0; r < 5; r++)
      for (int col = 0; col < 3; col++)
        if (bits & (1 << (14 - r * 3 - col)))
          c.fillRect(x + col * scale, y + r * scale, scale, scale, WHITE);
  }
}
template <class C> void pilot(C &c, const Storm::Snapshot &s) {
  if (Storm::isWow(s.mode)) {
    WowRender::pilot(c, s.wow);
    return;
  }
  if (s.mode == Storm::Mode::Game && s.gameDone && s.showMs < 3000) {
    auto end = s;
    end.mode = Storm::Mode::Show;
    pilot(c, end);
    return;
  }
  c.fillScreen(0);
  Iq3Render::Window<C> w(c);
  w.fillRect(0, 0, 160, 80, BG);
  char b[32];
  if (s.mode == Storm::Mode::Show) {
    int t = s.showMs % 3000;
    w.fillRect(137, 44, 3, 27, PURPLE);
    if (t < 1200) {
      Bounds<Iq3Render::Window<C>> portal(w, 0, 0, 137, 80);
      buggy(portal, 15 + t * 120 / 1000, 61);
    } else {
      symbol(w, 70, 44, 2);
      logo(w, 80, 66);
    }
  } else if (s.mode == Storm::Mode::Game) {
    text(w, "NIGHT STAGE", 80, 40, PURPLE);
    snprintf(b, sizeof(b), "%02lu SEC",
             (unsigned long)((45000 - s.gameMs) / 1000));
    text(w, b, 80, 53, WHITE, 2);
    text(w,
         s.gamePaused ? "PAUSA / SALTA"
         : s.gameDone ? "FINISH"
                      : "BOOT = SALTO",
         80, 69, WHITE);
  } else if (s.mode == Storm::Mode::Board) {
    text(w, "TRAIL / MANUALE", 80, 42, PURPLE);
    snprintf(b, sizeof(b), "%lu:%02lu", (unsigned long)(s.elapsed / 60000),
             (unsigned long)(s.elapsed / 1000 % 60));
    text(w, b, 80, 55, WHITE, 2);
  } else {
    text(w, "PIT CHECK", 80, 42, WHITE);
    text(w, "OSSERVA 16 PUNTI", 80, 56, PURPLE);
    text(w, "BOOT = ESCI", 80, 67, WHITE);
  }
  // Optical points always use the baseline CAD geometry.
  RecoilRender::lamps(c, s.mode == Storm::Mode::Pit ? 16 : 0);
}
template <class C> void navigator(C &raw, const Storm::Snapshot &s) {
  if (Storm::isWow(s.mode)) {
    WowRender::navigator(raw, s.wow);
    return;
  }
  if (s.mode == Storm::Mode::Game && s.gameDone && s.showMs < 3000) {
    auto end = s;
    end.mode = Storm::Mode::Show;
    navigator(raw, end);
    return;
  }
  Bounds<C> c(raw, 0, 0, 240, 135);
  c.fillScreen(BG);
  char b[40];
  if (s.mode == Storm::Mode::Show) {
    int t = s.showMs % 3000;
    c.fillRect(8, 48, 3, 45, PURPLE);
    if (t >= 1100 && t < 2200) {
      Bounds<Bounds<C>> portal(c, 11, 0, 240, 135);
      buggy(portal, -24 + (t - 1100) * 120 / 1000, 80);
    }
    if (t >= 2200) {
      symbol(c, 110, 25, 3);
      logo(c, 120, 76, 3);
      text(c, "AFTERDARK / DEMO", 120, 110, PINK);
    } else {
      text(c, "AFTERDARK", 120, 20, PURPLE);
      text(c, "PORTAL LINK / DEMO", 120, 116, WHITE);
    }
  } else if (s.mode == Storm::Mode::Game) {
    // Sharp horizon, deterministic parallax, generous contrast around hazards.
    for (int y = 23; y < 86; y++)
      c.fillRect(0, y, 240, 1, rgb(24 + (y - 23), 13, 46 + (y - 23) / 2));
    RecoilRender::disk(c, 176, 58, 23, PINK);
    for (int i = 0; i < 9; i++) {
      int x = (i * 40 - int(s.distance / 4) % 40);
      for (int y = 0; y < 20; y++)
        c.fillRect(x + y, 86 - y, 40 - 2 * y, 1, DIM);
    }
    c.fillRect(0, 99, 240, 36, DIM);
    c.fillRect(0, 99, 240, 2, PURPLE);
    for (int i = 0; i < 12; i++)
      c.fillRect((i * 24 + 240 - int(s.distance) % 24) % 240, 115 + (i % 3) * 4,
                 6, 1, PURPLE);
    c.fillRect(s.obstacle, 87, 12, 12, WHITE);
    c.fillRect(s.obstacle + 3, 84, 7, 3, WHITE);
    if (s.ghostJump >= 0)
      buggy(c, 36, 94 - s.ghostJump, DIM);
    buggy(c, 36, 94 - int(s.jump));
    if (s.jump == 0)
      for (int i = 0; i < 3; i++)
        c.fillRect(27 - i * 6, 96 + (i % 2) * 2, 3, 1, PINK);
    snprintf(b, sizeof(b), "%02lu S   URTI %u",
             (unsigned long)((45000 - s.gameMs) / 1000), s.hits);
    text(c, b, 120, 8, WHITE, 2);
    if (s.gamePaused || s.gameDone) {
      c.fillRect(18, 38, 204, 40, BG);
      text(c, s.gameDone ? "ARRIVO!" : "PAUSA", 120, 44, WHITE, 2);
      text(c,
           s.gameDone ? "RIAVVIA DAL TELEFONO" : "BOOT O SALTA PER RIPRENDERE",
           120, 65, PURPLE);
    }
  } else if (s.mode == Storm::Mode::Board) {
    text(c, "TRAIL SESSION", 120, 20, PURPLE, 2);
    snprintf(b, sizeof(b), "%lu:%02lu", (unsigned long)(s.elapsed / 60000),
             (unsigned long)(s.elapsed / 1000 % 60));
    text(c, b, 120, 47, WHITE, 4);
    snprintf(b, sizeof(b), "GATE %u  PEN %u  PT %d", s.gates, s.penalties,
             s.score);
    text(c, b, 120, 87, WHITE);
    text(c, "INSERIMENTI MANUALI", 120, 116, PINK);
  } else {
    text(c, "PIT CHECK", 120, 14, WHITE, 2);
    for (int i = 0; i < 6; i++)
      c.fillRect(10 + i * 37, 36, 34, 35,
                 rgb(i & 1 ? 255 : 0, i & 2 ? 255 : 0, i & 4 ? 255 : 0));
    text(c, "COLORI / MASCHERA / 16 PUNTI", 120, 84, WHITE);
    text(c, "CONFERMA VISIVA UMANA", 120, 100, PURPLE);
    text(c, "DETTAGLI NEL PORTALE", 120, 118, WHITE);
  }
}
} // namespace StormRender
