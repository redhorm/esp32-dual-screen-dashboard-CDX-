#pragma once
#ifndef NATIVE_BUILD

#include <LovyanGFX.hpp>
#include "assets/font_speed44.h"
#include "assets/font_hero30.h"
#include "assets/font_med20.h"
#include "assets/font_small12.h"

// Smooth (anti-aliased) VLW fonts, embedded in flash.
//
// Each scene owns its OWN UiFonts instance: a VLWfont reads glyph
// bitmaps through a DataWrapper with a mutable cursor, so sharing one
// instance between the two render tasks (different cores) would race.
// Cost per instance: just the metric tables (~30 B/glyph in heap).
struct UiFonts {
    lgfx::VLWfont speed;   // 44 px condensed bold — big numbers, titles
    lgfx::VLWfont hero;    // 30 px — dash RPM hero, gear box
    lgfx::VLWfont med;     // 20 px — gear, footer, roadbook DIST
    lgfx::VLWfont small;   // 12 px — roadbook info/note

    bool init() {
        _w_speed.set(font_speed44_vlw, sizeof(font_speed44_vlw));
        _w_hero.set(font_hero30_vlw, sizeof(font_hero30_vlw));
        _w_med.set(font_med20_vlw, sizeof(font_med20_vlw));
        _w_small.set(font_small12_vlw, sizeof(font_small12_vlw));
        return speed.loadFont(&_w_speed)
            && hero.loadFont(&_w_hero)
            && med.loadFont(&_w_med)
            && small.loadFont(&_w_small);
    }

private:
    lgfx::PointerWrapper _w_speed, _w_hero, _w_med, _w_small;
};

#endif // NATIVE_BUILD
