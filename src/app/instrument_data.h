#pragma once

#include <math.h>
#include "core/race.h"
#include "core/ui_settings.h"
#include "instrument_pilot.h"
#include "instrument_navigator.h"

// One conversion path for hardware and pixel previews. Both displays use the
// same simulated vehicle state; no invented GPS lock or sensor acquisition.
namespace InstrumentData {
inline RecoilRender::Screen screen(Phase phase) {
    if (phase == Phase::BOOT) return RecoilRender::Screen::BOOT;
    if (phase == Phase::FINISH) return RecoilRender::Screen::FINISH;
    return RecoilRender::Screen::LIVE;
}

inline Iq3Render::Data pilot(const VehicleState& state, const UiFx& fx,
                            const RunStats& stats, const UiSettings& settings) {
    Iq3Render::Data data;
    data.screen = settings.driver_theme == UiTheme::RECOIL_ALIGN
        ? RecoilRender::Screen::ALIGN : screen(fx.phase);
    const float speed = isfinite(state.speed_mph) ? state.speed_mph : 0.0f;
    data.speed = lroundf(speed * (settings.use_kmh ? 1.609344f : 1.0f));
    data.gear = speed < 0.3f ? 0 : state.gear;
    // Twenty RPM resolution prevents insignificant simulator noise from making
    // the segmented numeric field shimmer while retaining gear-change response.
    data.rpm = isfinite(state.rpm) ? lroundf(state.rpm / 20.0f) * 20 : 0;
    data.battery_x10 = isfinite(state.battery_v) ? lroundf(state.battery_v * 10) : 0;
    data.esc_controlled = state.esc_controlled;
    data.esc_signal_valid = state.esc_signal_valid;
    data.kmh = settings.use_kmh;
    data.celsius = settings.use_celsius;
    data.warning = state.engine_temp_f > 250.0f || state.battery_v < 11.5f;
    data.elapsed_ms = stats.stage_ms;
    data.now_ms = fx.now_ms;
    if (fx.phase == Phase::BOOT) data.shift_pairs = fx.phase_ms < 300 ? 8 : -1;
    return data;
}

inline HdsRender::Data navigator(const VehicleState& state, const UiFx& fx,
                                const UiSettings& settings) {
    HdsRender::Data data;
    data.screen = settings.navigator_theme == UiTheme::RECOIL_ALIGN
        ? RecoilRender::Screen::ALIGN : screen(fx.phase);
    data.odo_m = isfinite(state.odo_mi) ? state.odo_mi * 1609.344f : 0.0f;
    data.speed_mps = isfinite(state.speed_mph) ? state.speed_mph * 0.44704f : 0.0f;
    data.kmh = settings.use_kmh;
    data.now_ms = fx.now_ms;
    data.warning = fx.alert;
    return data;
}
}  // namespace InstrumentData
