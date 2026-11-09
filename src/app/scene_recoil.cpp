#ifndef NATIVE_BUILD
#include "scene_dashboard.h"
#include "scene_navigator.h"
#include "instrument_data.h"

void SceneDashboard::_renderRecoil(const VehicleState& state, const UiFx& fx,
                                  const RunStats& stats) {
    Iq3Render::pilot(_spr, InstrumentData::pilot(state, fx, stats, _settings));
}

void SceneNavigator::_renderRecoil(const VehicleState& state, const UiFx& fx,
                                  const RunStats&) {
    HdsRender::render(_spr, InstrumentData::navigator(state, fx, _settings));
}
#endif
