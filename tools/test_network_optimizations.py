#!/usr/bin/env python3
"""Compile actual HTTP handler bodies with a recording transport, ASan and UBSan.

Checks the complete JSON contract, capacity errors and streamed OTA HTML bytes.
No physical ESP32, upload, or simulated throughput claims are involved.
"""
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
portal = (ROOT / 'src/services/config_portal.cpp').read_text()
ota = (ROOT / 'src/services/storm_ota.cpp').read_text()
helper = portal[portal.index('bool appendFormat('):portal.index('void applyEscDeadband(')]
handler = portal[portal.index('void ConfigPortal::_sendRtlState()'):portal.index('void ConfigPortal::_calibrateForward()')]
page_handler = ota.split('s.on("/update", HTTP_GET, [&s] {', 1)[1].split('\n  });', 1)[0]
source = r'''
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <cassert>
#include <iostream>
#include "core/rtl_logic.h"
#include "core/esc_display_calibration.h"
#define PROGMEM
#include "services/storm_update_page.h"
#define taskENTER_CRITICAL(x) ((void)0)
#define taskEXIT_CRITICAL(x) ((void)0)
struct EscPwmSample {
 bool enabled=true, valid=true;
 uint16_t pulse_us=1042, filtered_us=1040;
 uint32_t age_ms=4294967295u;
 float normalized=-1.0f;
 EscDriveState state=EscDriveState::REVERSE;
};
struct WebServer {
 int status=0; std::string body, cache; size_t length=0;
 void sendHeader(const char*,const char* value){cache=value;}
 void send(int code,const char*,const char* data){status=code;body=data;}
 void send_P(int code,const char*,const char* data,size_t size){status=code;body.assign(data,size);assert(size==strlen(data));}
 void setContentLength(size_t size){length=size;}
 void sendContent_P(const char* data,size_t size){body.append(data,size);}
 void sendContent(const char* data,size_t size){body.append(data,size);}
};
struct ConfigPortal {
 RtlFrame _rtl_frame; EscPwmSample _esc_sample; EscSignalConfig _esc_config;
 RtlProfile _rtl_profile=RtlProfile::ULTRA4_4400;
 uint16_t _esc_deadband_us=45, _esc_full_forward_us=1040;
 uint8_t _rtl_brightness=255; bool _esc_enabled=false;
 struct {bool ready(){return true;}} _rtl_hw;
 char _json_buffer[BUFFER_SIZE]; WebServer _server;
 void _sendRtlState();
};
''' + helper + handler + '''
void servePage(WebServer& s) {
 const char token[]="0123456789abcdef0123456789abcdef";
''' + page_handler + r'''
}
int main() {
 for(unsigned count: {0u,16u,64u,65u}) {
  ConfigPortal p; p._rtl_frame.count=count;
  for(unsigned i=0;i<64;i++)p._rtl_frame.pixels[i]=RtlColor(255,uint8_t(i),128);
  p._esc_config.inverted=true;p._esc_config.type=EscType::CRAWLER_FORWARD_REVERSE;
  p._sendRtlState();assert(p._server.cache=="no-store");
  std::cout<<count<<"\t"<<p._server.status<<"\t"<<p._server.body<<"\n";
 }
 WebServer page;servePage(page);
 std::string expected=STORM_UPDATE_PAGE;
 const auto at=expected.find("{{TOKEN}}");assert(at!=std::string::npos);
 expected.replace(at,9,"0123456789abcdef0123456789abcdef");
 assert(page.status==200 && page.cache=="no-store");
 assert(page.body==expected && page.length==expected.size());
 assert(page.body.find("{{TOKEN}}") == std::string::npos);
 std::cout<<"OTA_BYTES\t"<<page.length<<"\n";
}
'''

with tempfile.TemporaryDirectory(prefix='storm-http-') as temp:
    cpp = Path(temp) / 'http.cpp'
    cpp.write_text(source)
    for size in (4096, 64):
        binary = Path(temp) / f'http-{size}'
        subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                        '-DNATIVE_BUILD', f'-DBUFFER_SIZE={size}',
                        '-I', str(ROOT / 'src'), '-I', str(ROOT / 'include'),
                        str(cpp), str(ROOT / 'src/core/rtl_logic.cpp'),
                        str(ROOT / 'src/core/esc_signal.cpp'), '-o', str(binary)], check=True)
        lines = subprocess.check_output([str(binary)], text=True).splitlines()
        for line in lines[:-1]:
            count, code, raw = line.split('\t', 2)
            count, code = int(count), int(code)
            actual = json.loads(raw)
            if size == 64 or count > 64:
                assert code == 500 and actual == {'ok': False, 'error': 'state_overflow'}
                continue
            assert code == 200
            assert actual == {
                'ok': True, 'profile': 'ultra4_4400', 'function': 'running',
                'brightness': 255, 'hardwareReady': True, 'timedOut': True,
                'escEnabled': False, 'pwmEnabled': True, 'lightsFollowPwm': True,
                'escValid': True, 'escPulse': 1040, 'escFullForwardUs': 1040,
                'escFullForwardCalibrated': True, 'escRawPulse': 1042,
                'escAgeMs': 4294967295, 'escInverted': True,
                'escType': 'crawler_forward_reverse', 'escState': 'REVERSE',
                'escNormalized': -100, 'escNeutralUs': 1500, 'escDeadbandUs': 45,
                'escReverseConfirmMs': 220,
                'pixels': [[255, i, 128] for i in range(count)]}
        print(f'PASS buffer={size}: four JSON boundary cases; identical streamed OTA page; ASan/UBSan')
