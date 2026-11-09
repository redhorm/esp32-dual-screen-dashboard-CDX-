"""Compile actual portal save/control function bodies with fake WebServer/NVS/GPIO.
Tests domain isolation, failed commits, and PWM lights with instrument ESC off.
No device or receiver is driven by this test.
"""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/services/config_portal.cpp').read_text()
def function(signature):
 start=s.index(signature);a=s.index('{',start);depth=1;i=a+1
 while depth:
  depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[start:i]
scaffold=r'''
#include <cassert>
#include <cstdlib>
#include <map>
#include <string>
#include "core/rtl_logic.h"
using String=std::string;
#define taskENTER_CRITICAL(x) ((void)0)
#define taskEXIT_CRITICAL(x) ((void)0)
static uint32_t now=1000;uint32_t millis(){return now;}
struct WebServer {
 std::map<std::string,std::string> args;int status=0;
 bool hasArg(const char* name){return args.count(name);}
 String arg(const char* name){return args[name];}
 void send(int code,const char*,const char*){status=code;}
};
struct EscPwmSample {bool enabled=true,valid=true;EscDriveState state=EscDriveState::REVERSE;};
struct Input {int starts=0;bool enabled=true;EscPwmSample value;
 void configureSignal(const EscSignalConfig&){} void begin(bool e){starts++;enabled=e;}
 EscPwmSample sample(){value.enabled=enabled;value.valid=enabled;return value;}
};
struct Hardware {uint8_t brightness=96;RtlFrame last;
 void setBrightness(uint8_t b){brightness=b;}void update(const RtlFrame& f){last=f;}
};
struct ConfigPortal {
 enum class ConfigDomain{Combined,Esc,Lights};
 WebServer _server;RtlProfile _rtl_profile=RtlProfile::ULTRA4_4400;uint8_t _rtl_brightness=96;
 EscSignalConfig _esc_config;bool _esc_enabled=true;uint16_t _esc_deadband_us=45;
 bool _manual_rtl_active=false,_io_reconfigure_pending=false,_pwm_reconfigure_pending=false,_config_save_pending=false;
 uint32_t _manual_rtl_at_ms=0,_esc_sample_at_ms=0;RtlCommandState _manual_rtl;EscPwmSample _esc_sample;
 RtlFrame _rtl_frame;RtlLogic _rtl;Input _esc_input;Hardware _rtl_hw;
 bool persist=true;int saves=0,mirrors=0;
 bool _saveVersionedConfigWithRtl(RtlProfile,uint8_t,bool,const EscSignalConfig&,uint16_t){saves++;return persist;}
 bool _saveLegacyMirror(){mirrors++;return true;}
 void _sendRtlState(){_server.status=200;}
 void _saveRtlConfig(ConfigDomain);void controlIoTick();
};
void applyEscDeadband(EscSignalConfig& c,uint16_t d){EscSignalProcessor::configureDeadbandUs(c,d);}
'''
checks=r'''
int main(){
 ConfigPortal p;p._esc_config.inverted=true;p._esc_config.type=EscType::CRAWLER_FORWARD_REVERSE;
 p._server.args={{"brightness","123"},{"profile","rtl_s"}};
 p._saveRtlConfig(ConfigPortal::ConfigDomain::Lights);
 assert(p._server.status==200&&p._rtl_brightness==123&&p._rtl_profile==RtlProfile::RTL_S);
 assert(p._esc_enabled&&p._esc_config.inverted&&p._esc_config.neutral_us==1500&&!p._pwm_reconfigure_pending);
 p.controlIoTick();assert(p._esc_input.starts==0);
 p._server.args={{"escEnabled","0"},{"escInverted","1"},{"escType","crawler_forward_reverse"},{"escNeutralUs","1510"},{"escDeadbandUs","45"},{"escReverseConfirmMs","220"}};
 p._saveRtlConfig(ConfigPortal::ConfigDomain::Esc);
 assert(p._server.status==200&&!p._esc_enabled&&p._esc_config.neutral_us==1510);
 assert(p._rtl_brightness==123&&p._rtl_profile==RtlProfile::RTL_S);
 p.controlIoTick();assert(p._esc_input.enabled&&p._esc_input.starts==1&&p._esc_sample.valid);
 assert(p._rtl_hw.last.primary==RtlFunction::REVERSE_WORK);
 p._server.args={{"profile","ultra4_4400"},{"brightness","99"},{"escEnabled","1"}};
 int saved=p.saves;p._saveRtlConfig(ConfigPortal::ConfigDomain::Lights);
 assert(p._server.status==400&&p.saves==saved&&!p._esc_enabled&&p._rtl_brightness==123);
 p._server.args={{"escEnabled","1"}};p._saveRtlConfig(ConfigPortal::ConfigDomain::Esc);
 assert(p._server.status==400&&p.saves==saved&&!p._esc_enabled);
 p._server.args={{"profile","ultra4_4400"},{"brightness","99"}};p.persist=false;
 p._saveRtlConfig(ConfigPortal::ConfigDomain::Lights);assert(p._server.status==500&&p._rtl_brightness==123&&!p._pwm_reconfigure_pending);
 p.persist=true;p._server.args={{"escEnabled","1"},{"escInverted","1"},{"escType","crawler_forward_reverse"},{"escNeutralUs","1500"},{"escDeadbandUs","45"},{"escReverseConfirmMs","220"}};
 p._saveRtlConfig(ConfigPortal::ConfigDomain::Esc);p.controlIoTick();assert(p._esc_enabled&&p._esc_input.enabled&&p._rtl_brightness==123);
}
'''
with tempfile.TemporaryDirectory() as t:
 cpp=Path(t)/'test.cpp';exe=Path(t)/'test'
 cpp.write_text(scaffold+function('bool parseUnsignedArg(')+function('void ConfigPortal::_saveRtlConfig(')+function('void ConfigPortal::controlIoTick()')+checks)
 subprocess.run(['c++','-std=c++17','-DNATIVE_BUILD','-I'+str(root/'include'),'-I'+str(root/'src'),str(cpp),str(root/'src/core/rtl_logic.cpp'),str(root/'src/core/esc_signal.cpp'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS actual C++ handlers: independent saves, cross-domain rejection, failed NVS commit rollback, PWM remains active and lights show reverse with ESC instruments disabled.')
