#ifndef NATIVE_BUILD
#include "scene_dashboard.h"
#include "recoil_renderer.h"
#include "show_renderer.h"
#include <stdio.h>
bool SceneDashboard::init(){
    _spr.setColorDepth(16);_spr.setPsram(false);
    return _ready=_spr.createSprite(160,80)!=nullptr;
}
void SceneDashboard::setPortalCredentials(bool visible,const char* ssid,const char* password){
    _portalCredentialsVisible=visible;
    snprintf(_portalSsid,sizeof(_portalSsid),"%s",ssid?ssid:"");
    snprintf(_portalPassword,sizeof(_portalPassword),"%s",password?password:"");
}
void SceneDashboard::render(const VehicleState& state,const UiFx& fx,const RunStats& stats){
    if(!_ready)return;
    _renderRecoil(state,fx,stats);
    if(_portalCredentialsVisible){
        // All text stays inside the physical instrument window.
        RecoilRender::PilotData base;base.screen=RecoilRender::Screen::BOOT;
        RecoilRender::pilot(_spr,base);
        ShowRender::PilotWindow<LGFX_Sprite> window(_spr);
        window.fillRect(0,20,160,60,RecoilRender::LCD);
        RecoilRender::text(window,"WI-FI",80,28,RecoilRender::INK);
        RecoilRender::text(window,_portalSsid,80,39,RecoilRender::INK);
        RecoilRender::text(window,_portalPassword,80,51,RecoilRender::INK,2);
        RecoilRender::text(window,"192.168.4.1",80,68,RecoilRender::INK);
    }
}
#endif
