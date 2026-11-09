#pragma once

#include <stdint.h>
#include <math.h>
#include "core/show_sequence.h"
#include "recoil_renderer.h"
#include "instrument_pilot.h"
#include "instrument_navigator.h"

// One deterministic physical demonstration drives both actual instruments.
// Existing sprites are reused; there are no framebuffers or runtime allocations.
namespace ShowRender {
constexpr uint16_t BLACK = RecoilRender::BLACK;
constexpr uint16_t WHITE = 0xffff;
constexpr uint16_t CYAN = RecoilRender::CYAN;
constexpr uint16_t DARK = RecoilRender::rgb(8,18,27);
constexpr uint16_t DIM = RecoilRender::rgb(26,61,71);
constexpr uint16_t AMBER = RecoilRender::AMBER;

// Kept for the normal scene's clipped status overlays as well as SHOW cards.
template<class C> using PilotWindow = Iq3Render::Window<C>;

struct DemoState {
    float speed_kmh = 0;
    float odo_m = 620;
    int gear = 0;
    int rpm = 800;
};

inline bool instrumentPhase(const ShowFrame& frame) {
    return frame.active && (frame.phase == ShowPhase::ROUTE || frame.phase == ShowPhase::LINK);
}

inline DemoState demoState(const ShowFrame& frame) {
    DemoState state;
    if (!frame.active || frame.elapsed_ms < 2000U) return state;
    const uint32_t now = frame.elapsed_ms > 14500U ? 14500U : frame.elapsed_ms;
    if (now < 8000U) {
        const float elapsed = static_cast<float>(now - 2000U);
        state.speed_kmh = elapsed * (88.0f / 6000.0f);
        // Integral of the linear 0..88 km/h acceleration: 73.333 m in 6 s.
        state.odo_m += 88.0f * elapsed * elapsed / 43200000.0f;
    } else {
        const uint32_t elapsed = now - 8000U;
        const uint32_t cycles = elapsed / 1600U;
        const float q = static_cast<float>(elapsed % 1600U);
        float integral;
        if (q <= 800.0f) {
            state.speed_kmh = 88.0f - q * .02f;
            integral = 88.0f * q - .01f * q * q;
        } else {
            const float rising = q - 800.0f;
            state.speed_kmh = 72.0f + rising * .02f;
            integral = 64000.0f + 72.0f * rising + .01f * rising * rising;
        }
        // Complete cycles average 80 km/h. Integrating instead of inventing
        // cursor motion keeps the HDS speed, distance and first turn coherent.
        state.odo_m += 73.3333333f + (cycles * 128000.0f + integral) / 3600.0f;
    }
    if (state.speed_kmh < .8f) return state;
    state.gear = state.speed_kmh < 23 ? 1 : state.speed_kmh < 46 ? 2 : state.speed_kmh < 72 ? 3 : 4;
    constexpr int ratios[4] = {280,160,110,82};
    state.rpm = static_cast<int>(lroundf(state.speed_kmh * ratios[state.gear - 1] / 20.0f)) * 20;
    state.rpm = Iq3Render::bounded(state.rpm,800,7200);
    return state;
}

inline Iq3Render::Data pilotData(const ShowFrame& frame) {
    const auto state = demoState(frame);
    Iq3Render::Data data;
    data.speed = static_cast<int>(lroundf(state.speed_kmh));
    data.gear = state.gear;
    data.rpm = state.rpm;
    data.battery_x10 = 126;
    data.elapsed_ms = frame.elapsed_ms >= 2000U ? frame.elapsed_ms - 2000U : 0;
    data.now_ms = frame.elapsed_ms;
    return data;
}

inline HdsRender::Data navigatorData(const ShowFrame& frame) {
    const auto state = demoState(frame);
    HdsRender::Data data;
    data.odo_m = state.odo_m;
    data.speed_mps = state.speed_kmh / 3.6f;
    data.now_ms = frame.elapsed_ms;
    return data;
}

template<class C> bool pilot(C& canvas, const ShowFrame& frame, bool english = false) {
    canvas.fillScreen(BLACK);
    if (canvas.width() != 160 || canvas.height() != 80) return false;
    if (!frame.active) return true;
    if (instrumentPhase(frame)) return Iq3Render::pilot(canvas,pilotData(frame));
    Iq3Render::Window<C> c(canvas);
    c.fillRect(0,0,160,80,DARK);
    if (frame.phase == ShowPhase::ACQUIRE) {
        RecoilRender::text(c,"PULSE",80,34,WHITE,3);
        RecoilRender::text(c,english ? "16 POINTS" : "16 PUNTI",80,55,CYAN);
    } else if (frame.phase == ShowPhase::FINISH) {
        RecoilRender::text(c,"RECOIL",80,34,WHITE,3);
        RecoilRender::text(c,"PULSE",80,54,CYAN,2);
    }
    RecoilRender::text(c,"DEMO",80,70,AMBER);
    // Opening/closing signature retains the delivered sequential lamp sweep.
    // During the instrument demonstration the real IQ3 symmetric pairs are
    // driven by RPM through Iq3Render, not by a separate decorative animation.
    RecoilRender::lamps(canvas,frame.lit);
    return true;
}

template<class C> bool navigator(C& c, const ShowFrame& frame, bool english = false) {
    c.fillScreen(BLACK);
    if (c.width() != 240 || c.height() != 135) {
        if (c.width() >= 80 && c.height() >= 40) {
            RecoilRender::text(c,"RECOIL PULSE",c.width()/2,c.height()/2-12,WHITE);
            RecoilRender::text(c,"SHOW DEMO",c.width()/2,c.height()/2+2,AMBER);
        }
        return false;
    }
    if (!frame.active) return true;
    if (instrumentPhase(frame)) {
        HdsRender::render(c,navigatorData(frame));
        // A small annotation occupies only the empty area below the cursor.
        // SOG/COG/DTW, north arrow, scale, waypoint and DEMO stay unobstructed.
        if (frame.phase == ShowPhase::LINK) {
            c.fillRect(71,113,97,10,HdsRender::INK);
            RecoilRender::text(c,"PULSE LINK",120,116,HdsRender::WHITE);
        } else {
            c.fillRect(96,113,48,10,HdsRender::PAPER);
            RecoilRender::text(c,"SHOW",120,116,HdsRender::INK);
        }
        c.fillRect(0,134,static_cast<int>(240U*frame.progress_permille/1000U),1,HdsRender::ROUTE);
        return true;
    }
    c.fillScreen(DARK);
    // The short title cards frame the actual demonstration; no imaginary map
    // or GPS data is shown before the paired instrument demonstration starts.
    c.fillRect(0,0,240,19,BLACK);
    RecoilRender::text(c,"RECOIL PULSE",75,5,WHITE,2);
    RecoilRender::text(c,"DEMO",210,5,AMBER,2);
    if (frame.phase == ShowPhase::ACQUIRE) {
        RecoilRender::text(c,"PULSE",120,47,WHITE,5);
        RecoilRender::text(c,english ? "16 POINTS . 2 SCREENS" : "16 PUNTI . 2 SCHERMI",120,80,CYAN,2);
        for(int i=0;i<16;++i)c.fillRect(57+i*8,105,5,3,i<frame.lit?CYAN:DIM);
    } else if (frame.phase == ShowPhase::FINISH) {
        RecoilRender::text(c,"RECOIL",120,47,WHITE,5);
        RecoilRender::text(c,"PULSE",120,80,CYAN,3);
    }
    c.fillRect(0,114,240,21,BLACK);
    RecoilRender::text(c,"SHOW",24,119,CYAN);
    char label[24];
    snprintf(label,sizeof(label),"%02u / 18 S",static_cast<unsigned>(frame.elapsed_ms/1000U));
    RecoilRender::text(c,label,194,119,WHITE);
    c.fillRect(8,130,224,2,DIM);
    c.fillRect(8,130,static_cast<int>(224U*frame.progress_permille/1000U),2,CYAN);
    return true;
}
} // namespace ShowRender
