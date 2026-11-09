#pragma once
#ifndef NATIVE_BUILD
#include <LovyanGFX.hpp>
#include "core/race.h"
#include "core/ui_settings.h"
#include "services/photo_store.h"
#include "hal/copilot_display_profile.h"
class SceneNavigator {
public:
    SceneNavigator(lgfx::LGFX_Device& d,const Roadbook& r):_spr(&d),_disp(d),_rb(r){}
    bool init();
    bool ready()const{return _ready;}
    bool setDisplayProfile(CopilotDisplayProfile p){if(_ready)return false;_profile=p;return true;}
    void setSettings(const UiSettings& s){_settings=s;}
    void setPortalCredentials(bool visible,const char* ssid,const char* password);
    void render(const VehicleState&,const UiFx&,const RunStats&);
    void push(){if(_ready)_spr.pushSprite(&_disp,0,0);}
    LGFX_Sprite& surface(){return _spr;}
private:
    LGFX_Sprite _spr;
    lgfx::LGFX_Device& _disp;
    const Roadbook& _rb;
    UiSettings _settings;
    bool _ready=false,_portalCredentialsVisible=false;
    char _portalSsid[24]={},_portalPassword[16]={};
    CopilotDisplayProfile _profile=copilotDisplayDefaultProfile();
    void _renderRecoil(const VehicleState&,const UiFx&,const RunStats&);
};
#endif
