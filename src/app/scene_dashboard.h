#pragma once
#ifndef NATIVE_BUILD
#include <LovyanGFX.hpp>
#include "core/race.h"
#include "core/ui_settings.h"
#include "services/photo_store.h"
class SceneDashboard {
public:
    explicit SceneDashboard(lgfx::LGFX_Device& d):_spr(&d),_disp(d){}
    bool init();
    bool ready()const{return _ready;}
    void setSettings(const UiSettings& s){_settings=s;}
    void setPortalCredentials(bool visible,const char* ssid,const char* password);
    void render(const VehicleState&,const UiFx&,const RunStats&);
    void push(){if(_ready)_spr.pushSprite(&_disp,0,0);}
    LGFX_Sprite& surface(){return _spr;}
private:
    LGFX_Sprite _spr;
    lgfx::LGFX_Device& _disp;
    UiSettings _settings;
    bool _ready=false,_portalCredentialsVisible=false;
    char _portalSsid[24]={},_portalPassword[16]={};
    void _renderRecoil(const VehicleState&,const UiFx&,const RunStats&);
};
#endif
