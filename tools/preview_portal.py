#!/usr/bin/env python3
"""Browser preview with a fake ESP32 API; never connects to or flashes hardware.

Usage: python3 tools/preview_portal.py --port 8765
Scenarios: /?scenario=normal, /?scenario=fs-offline, /?scenario=loss
Fixture endpoints cover settings/profile, RTL, /api/show (18-second timeline),
boot-image status/upload and diagnostics. SHOW start/stop are idempotent;
settings support validated partial updates; stale RTL commands time out.
The banner, display FPS/RAM/PWM and image responses are simulated fixtures.
This is a UI fixture, not an ESP32/emulator or firmware validation.
"""
from copy import deepcopy
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse
import argparse
import base64
import json
import re
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
PAGE = re.search(r'R"HTML\((.*)\)HTML";', (ROOT / 'src/services/settings_page.h').read_text(), re.S).group(1)
PAGE = PAGE.replace('<body>', '<body><div style="background:#ffb956;color:#111;text-align:center;padding:6px;font:700 11px monospace">ANTEPRIMA LOCALE · API SIMULATA · NESSUN HARDWARE COLLEGATO</div>').encode()
LOCK = threading.RLock()
START = time.monotonic()
SHOW_START = None
SCENARIO = 'normal'
REQUESTS = 0
RTL_AT = 0.0
SETTINGS = dict(displayTheme=4, driverTheme=4, navigatorTheme=4, language=0,
                setupComplete=True, useKmh=True, useCelsius=True,
                driverBrightness=220, navigatorBrightness=220,
                bootImageEnabled=True, bootHoldMs=1800, photoModeEnabled=False,
                photoTarget=3, photoIntervalS=12, photoTransition=1,
                photoRandom=False, photoOverlay=True)
IDENTITY = dict(number='4400', vehicle='RECOIL ULTRA4', vehicleName='RECOIL ULTRA4', driver='LUCA', accent=64576)
RTL = dict(ok=True, profile='ultra4_4400', function='running', brightness=96,
           hardwareReady=True, timedOut=False, escEnabled=False, escValid=False,
           escPulse=0, escType='forward_brake_reverse', escState='neutral',
           escNormalized=0, escNeutralUs=1500, escDeadbandUs=45,
           escReverseConfirmMs=220, pixels=[])
PROFILE = '240x135_landscape'
BOOT_PRESENT = True
SAMPLE = base64.b64decode('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAusB9Wl2k3sAAAAASUVORK5CYII=')


def show():
    global SHOW_START
    elapsed = int((time.monotonic() - SHOW_START) * 1000) if SHOW_START is not None else 0
    if elapsed >= 18000:
        SHOW_START = None
        elapsed = 0
    active = SHOW_START is not None
    return dict(ok=True, active=active, elapsedMs=elapsed, durationMs=18000, pending=False, requestedActive=active)


def lights():
    if RTL['function'] != 'running' and time.monotonic() - RTL_AT > 1.5:
        RTL.update(function='running', timedOut=True)
    colors = [[70, 0, 0]] * 4 + [[160, 50, 0]] * 2 + [[0, 0, 0]] * 4 + [[160, 50, 0]] * 2 + [[70, 0, 0]] * 4
    fn = RTL['function']
    if fn == 'brake': colors[:4] = colors[-4:] = [[255, 0, 0]] * 4
    if fn == 'reverse_work': colors[6:10] = [[255, 255, 200]] * 4
    if fn in ('left', 'right', 'hazard', 'dust_chase') and int(time.monotonic() * 3) % 2:
        if fn in ('left', 'hazard', 'dust_chase'): colors[4:6] = [[255, 170, 0]] * 2
        if fn in ('right', 'hazard', 'dust_chase'): colors[10:12] = [[255, 170, 0]] * 2
    RTL['pixels'] = colors
    return deepcopy(RTL)


def display():
    w, h = {'240x135_landscape': (240, 135), '240x240': (240, 240), '128x128': (128, 128)}[PROFILE]
    return dict(ok=True, profile=PROFILE, logicalWidth=w, logicalHeight=h,
                controller='ST7735' if w == 128 else 'ST7789', rotation=1,
                physicalWidth=h, physicalHeight=w)


def photos():
    ready = SCENARIO != 'fs-offline'
    return dict(ok=True, fsReady=ready, slots=[], maxSlots=24, bootDriver=ready and BOOT_PRESENT,
                bootNavigator=ready and BOOT_PRESENT, totalBytes=5177344 if ready else 0,
                usedBytes=74000 if ready else 0, freeBytes=5103344 if ready else 0,
                storageState='ready' if ready else 'unavailable')


class Handler(BaseHTTPRequestHandler):
    def send(self, payload, status=200, kind='application/json'):
        body = payload if isinstance(payload, bytes) else json.dumps(payload).encode()
        self.send_response(status)
        self.send_header('Content-Type', kind)
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        try: self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError): pass

    def error(self, code, status=400):
        self.send(dict(ok=False, error=code), status)

    def do_GET(self):
        global SCENARIO, REQUESTS
        with LOCK:
            parsed = urlparse(self.path); args = parse_qs(parsed.query); path = parsed.path
            if path == '/' and 'scenario' in args: SCENARIO = args['scenario'][0]
            REQUESTS += 1
            if SCENARIO == 'loss' and path.startswith('/api/') and REQUESTS % 6 == 0:
                self.error('simulated_link_loss', 503); return
            if path == '/api/show': self.send(show())
            elif path == '/api/settings': self.send(SETTINGS)
            elif path in ('/api/v3/state', '/api/v3/profile'):
                self.send(dict(ok=True, version='6.0.0-recoil-pulse-preview', telemetrySource='demo', identity=IDENTITY))
            elif path == '/api/v3/rtl/state': self.send(lights())
            elif path == '/api/copilot-display': self.send(display())
            elif path == '/api/photos': self.send(photos())
            elif path == '/api/v4/diagnostics':
                self.send(dict(ok=True, firmware='6.0.0-recoil-pulse · PREVIEW', uptimeMs=int((time.monotonic()-START)*1000),
                               displayA=dict(ok=True, fps=30), displayB=dict(ok=True, fps=15),
                               memory=dict(heapFreeBytes=221000, heapLargestBytes=106000),
                               storage=dict(ready=SCENARIO != 'fs-offline'),
                               lastError='littlefs_unavailable' if SCENARIO == 'fs-offline' else 'none',
                               restartCount=1, resetReason='power_on'))
            elif path == '/api/media/boot':
                self.send(b'<svg xmlns="http://www.w3.org/2000/svg" width="240" height="135"><rect width="240" height="135" fill="#101417"/><text x="120" y="70" text-anchor="middle" fill="#57e1df" font-family="sans-serif" font-size="24">RECOIL PULSE</text><text x="120" y="93" text-anchor="middle" fill="#ffb956" font-size="10">ANTEPRIMA SIMULATA</text></svg>', kind='image/svg+xml')
            elif path == '/qa-sample.png': self.send(SAMPLE, kind='image/png')
            elif path.startswith('/api/'): self.error('not_found', 404)
            else: self.send(PAGE, kind='text/html; charset=utf-8')

    def do_POST(self):
        global SHOW_START, RTL_AT, BOOT_PRESENT, PROFILE
        parsed = urlparse(self.path); path = parsed.path
        raw = self.rfile.read(min(1048576, int(self.headers.get('Content-Length', '0'))))
        args = parse_qs(parsed.query, keep_blank_values=True)
        if 'multipart/form-data' not in self.headers.get('Content-Type', ''):
            args.update(parse_qs(raw.decode(errors='replace'), keep_blank_values=True))
        args = {k: v[-1] for k, v in args.items()}
        with LOCK:
            if path == '/api/show':
                if args.get('action') not in ('start', 'stop'): self.error('invalid_show_action'); return
                show()
                if args['action'] == 'start' and SHOW_START is None: SHOW_START = time.monotonic()
                elif args['action'] == 'stop': SHOW_START = None
                self.send(show()); return
            if path == '/api/settings':
                candidate = deepcopy(SETTINGS); known = False
                ranges = dict(displayTheme=(4,5),driverTheme=(4,5),navigatorTheme=(4,5),language=(0,1),driverBrightness=(32,255),navigatorBrightness=(32,255),bootHoldMs=(500,3500))
                for key, (lo, hi) in ranges.items():
                    if key not in args: continue
                    known = True
                    if not args[key].isdigit() or not lo <= int(args[key]) <= hi: self.error('invalid_settings'); return
                    candidate[key] = int(args[key])
                for key, values, dest in [('units',['imperial','metric'],'useKmh'),('temperature',['f','c'],'useCelsius')]:
                    if key not in args: continue
                    known = True
                    if args[key] not in values: self.error('invalid_settings'); return
                    candidate[dest] = args[key] == values[1]
                for key in ('bootImageEnabled','photoModeEnabled'):
                    if key not in args: continue
                    known = True
                    if args[key] not in ('','0','1','on','off','true','false'): self.error('invalid_settings'); return
                    candidate[key] = args[key] in ('1','on','true')
                    if key == 'photoModeEnabled' and candidate[key]: self.error('invalid_settings'); return
                if not known: self.error('invalid_settings'); return
                if 'displayTheme' in args: candidate['driverTheme'] = candidate['navigatorTheme'] = candidate['displayTheme']
                SETTINGS.update(candidate)
            elif path == '/api/v3/profile':
                for key in ('number','vehicle','driver'):
                    if key in args: IDENTITY[key] = re.sub(r'[^A-Z0-9 ._-]', '', args[key].upper())[:6 if key=='number' else 24]
                IDENTITY['vehicleName'] = IDENTITY['vehicle']
            elif path == '/api/copilot-display':
                if args.get('profile') != '240x135_landscape': self.error('invalid_profile'); return
                PROFILE = args['profile']; self.send(dict(ok=True,restarting=True,restartAfterMs=900)); return
            elif path == '/api/v3/rtl/config':
                for key in ('profile','escType'):
                    if key in args: RTL[key] = args[key]
                for key in ('brightness','escNeutralUs','escDeadbandUs','escReverseConfirmMs'):
                    if key in args: RTL[key] = int(args[key])
                RTL['escEnabled'] = args.get('escEnabled') == '1'
                RTL['function'] = 'running'; self.send(lights()); return
            elif path == '/api/v3/rtl/command':
                fn = args.get('function')
                if fn == 'pit_test' and not RTL['escEnabled']: self.error('esc_required_for_pit',409); return
                if fn == 'pit_test' and not RTL['escValid']: self.error('esc_not_neutral',409); return
                if fn not in ('left','right','brake','hazard','dust_chase','reverse_work','running','clear','pit_test'): self.error('invalid_function'); return
                RTL['function'] = 'running' if fn == 'clear' else fn
                RTL_AT = time.monotonic(); RTL['timedOut'] = False
                self.send(lights()); return
            elif path.startswith('/api/upload/') or path.startswith('/api/boot/'):
                if SCENARIO == 'fs-offline': self.error('filesystem_unavailable',503); return
                if path == '/api/boot/commit': BOOT_PRESENT = True
                if path == '/api/boot/delete': BOOT_PRESENT = False
            else: self.error('not_found',404); return
            self.send(dict(ok=True))

    def log_message(self, *_): pass


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',type=int,default=8765)
    arguments = parser.parse_args()
    print(f'Fake API / UI preview only: http://127.0.0.1:{arguments.port}',flush=True)
    ThreadingHTTPServer(('127.0.0.1',arguments.port),Handler).serve_forever()
