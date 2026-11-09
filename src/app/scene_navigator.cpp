#ifndef NATIVE_BUILD
#include "scene_navigator.h"
#include "recoil_renderer.h"
#include <stdio.h>
bool SceneNavigator::init(){
    auto display=copilotDisplayConfig(_profile);
    _spr.setColorDepth(16);_spr.setPsram(false);
    return _ready=_spr.createSprite(display.logical_width,display.logical_height)!=nullptr;
}
void SceneNavigator::setPortalCredentials(bool visible,const char* ssid,const char* password){
    _portalCredentialsVisible=visible;
    snprintf(_portalSsid,sizeof(_portalSsid),"%s",ssid?ssid:"");
    snprintf(_portalPassword,sizeof(_portalPassword),"%s",password?password:"");
}
void SceneNavigator::render(const VehicleState& state,const UiFx& fx,const RunStats& stats){
    if(!_ready)return;
    _renderRecoil(state,fx,stats);
    if(_portalCredentialsVisible){
        using namespace RecoilRender;
        _spr.fillScreen(DARK);
        int cx=_spr.width()/2;
        text(_spr,"RECOIL WI-FI",cx,15,CYAN,2);
        text(_spr,"SSID",cx,40,WHITE);
        text(_spr,_portalSsid,cx,52,WHITE);
        text(_spr,"PASSWORD",cx,72,WHITE);
        text(_spr,_portalPassword,cx,84,AMBER,2);
        text(_spr,"192.168.4.1",cx,112,CYAN,2);
    }
}
#endif
