#include "experience.h"

void ExperienceEngine::reset(uint32_t now_ms) {
    TaskGuard guard(_mutex);
    (void)now_ms;
    _config_dirty = false;
}

void ExperienceEngine::update(uint32_t now_ms, const VehicleState& state,
                              const UiFx&) {
    TaskGuard guard(_mutex);
    (void)now_ms;
    _vehicle = state;
}

ExperienceFrame ExperienceEngine::frame(uint32_t now_ms) const {
    TaskGuard guard(_mutex);
    (void)now_ms;
    ExperienceFrame out;
    out.identity = _config;
    out.speed_mph = _vehicle.speed_mph;
    out.rpm = _vehicle.rpm;
    out.heading_deg = _vehicle.heading_deg;
    out.battery_v = _vehicle.battery_v;
    out.gear = static_cast<int8_t>(_vehicle.gear);
    out.esc_controlled = _vehicle.esc_controlled;
    out.esc_signal_valid = _vehicle.esc_signal_valid;
    out.active_wp = static_cast<uint8_t>(_vehicle.active_wp);
    return out;
}

bool ExperienceEngine::consumeConfigDirty() {
    TaskGuard guard(_mutex);
    const bool dirty = _config_dirty;
    _config_dirty = false;
    return dirty;
}
