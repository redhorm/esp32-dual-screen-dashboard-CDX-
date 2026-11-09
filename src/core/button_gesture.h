#pragma once
#include <stdint.h>
enum class ButtonAction : uint8_t { NONE, TOGGLE_SHOW, REVEAL_WIFI };
// The on-board BOOT button is active-low; caller supplies pressed=true.
// Long press fires once. Releasing it can never trigger a SHOW afterwards.
class ButtonGesture {
    bool _raw=false, _stable=false, _long=false;
    uint32_t _changed=0, _pressed=0;
public:
    ButtonAction update(bool pressed, uint32_t now) {
        if(pressed!=_raw){_raw=pressed;_changed=now;}
        if(_raw!=_stable && uint32_t(now-_changed)>=40){
            _stable=_raw;
            if(_stable){_pressed=now;_long=false;}
            else if(!_long)return ButtonAction::TOGGLE_SHOW;
        }
        if(_stable && !_long && uint32_t(now-_pressed)>=2000){
            _long=true;return ButtonAction::REVEAL_WIFI;
        }
        return ButtonAction::NONE;
    }
};
