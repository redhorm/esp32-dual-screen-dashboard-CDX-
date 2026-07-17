#!/usr/bin/env python3
"""Local API-compatible preview of the embedded captive portal."""
from copy import deepcopy
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse
import base64
import json
import re
import time

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/services/settings_page.h").read_text(encoding="utf-8")
PAGE = re.search(r'R"HTML\((.*)\)HTML";', SOURCE, re.S).group(1).encode()
QA_PNG = base64.b64decode(
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAusB9Wl2k3sAAAAASUVORK5CYII="
)
QA_SCRIPT = b'''<script>
(async()=>{if(new URLSearchParams(location.search).get('qa')!=='1')return;
const blob=await(await fetch('/qa-sample.png')).blob();
const file=new File([blob],'TRAIL #4400 + TEST.PNG',{type:'image/png'});
for(const id of ['bootFile','galleryFiles']){const input=document.getElementById(id);
const transfer=new DataTransfer();transfer.items.add(file);input.files=transfer.files;
input.dispatchEvent(new Event('change',{bubbles:true}));}})();
</script>'''

SETTINGS = {
    "displayTheme": 1, "driverTheme": 1, "navigatorTheme": 1,
    "language": 0,
    "setupComplete": False, "useKmh": False, "useCelsius": True,
    "driverBrightness": 220, "navigatorBrightness": 220,
    "bootImageEnabled": True, "bootHoldMs": 1800,
    "photoModeEnabled": False, "photoTarget": 3,
    "photoIntervalS": 12, "photoTransition": 1,
    "photoRandom": False, "photoOverlay": True,
}
PHOTOS = {
    "ok": True, "schemaVersion": 3, "slots": [0, 1, 2], "maxSlots": 24,
    "bootDriver": True, "bootNavigator": True, "usedBytes": 612_400,
    "totalBytes": 5_177_344, "freeBytes": 4_564_944, "fsReady": True,
    "storageState": "ready", "partitionLabel": "littlefs",
    "formattedOnBegin": False, "recoveredOnBegin": False,
    "degraded": False, "uploadActive": False, "lastError": "none",
    "driverMaxBytes": 65_536, "navigatorMaxBytes": 196_608,
    "reserveBytes": 32_768,
    "systemAssetBytes": 118_400, "userPhotoBytes": 372_000,
    "gameAssetBytes": 122_000,
    "breakdown": {"systemAssetBytes": 118_400, "userPhotoBytes": 372_000,
                  "gameAssetBytes": 122_000, "transactionBytes": 0},
}
STATE = {
    "ok": True, "version": "5.0.0-beta.1", "nowMs": 18_420,
    "mode": "dashboard",
    "identity": {
        "number": "4400", "vehicle": "TRAIL WOLF",
        "vehicleName": "TRAIL WOLF", "driver": "L. TEMPESTINI",
        "accent": 64576,
    },
    "vehicle": {"speedMph": 68.0, "rpm": 6840, "gear": 4,
                "heading": 287, "batteryV": 13.9, "activeWp": 4},
    "game": {
        "status": "idle", "event": "none", "x": 0, "lineX": 0,
        "speed": 0, "nearestDistance": 210, "progress": 0, "score": 0,
        "combo": 0, "lives": 3, "obstacle": 0, "totalObstacles": 14,
        "phaseMs": 0, "steer": 0, "throttle": 0, "worldDistance": 0,
        "resultAgeMs": 65535, "elapsedMs": 0, "visibleObstacles": 3,
        "courseClear": False, "bestScore": 4820, "wins": 2,
        "obstacles": [
            {"lineX": -35, "distance": 120, "radius": 15,
             "kind": 0, "kindName": "boulder"},
            {"lineX": 47, "distance": 230, "radius": 12,
             "kind": 1, "kindName": "log"},
            {"lineX": 5, "distance": 340, "radius": 17,
             "kind": 2, "kindName": "rut"},
        ],
    },
}
RTL = {
    "ok": True, "profile": "ultra4_4400", "function": "running",
    "brightness": 96, "hardwareReady": True, "timedOut": False,
    "escEnabled": False, "escValid": False, "escPulse": 0,
    "escType": "forward_brake_reverse", "escState": "neutral",
    "escNormalized": 0, "escNeutralUs": 1500, "escDeadbandUs": 45,
    "escReverseConfirmMs": 220, "pixels": [],
}

GAME_V4 = {
    "ok": True, "version": "5.0.0-beta.1", "protocol": 4,
    "experienceMode": "dashboard", "status": "idle",
    "mode": "time_attack", "seed": 4400,
    "ackSeq": 0, "serverMs": 18_420, "controllerAlive": False,
    "inputRttMs": 18, "rpm": 900, "speed": 0, "steer": 0,
    "throttle": 0, "brake": 0,
    "score": 0, "checkpoint": 0, "totalCheckpoints": 12,
    "timeMs": 0, "bestMs": 48_620, "damage": 0, "tempC": 72,
    "grip": 1000, "distanceCm": 0, "courseLengthCm": 342_000,
    "objects": [], "audio": [],
}
AUDIO_EVENTS = []
AUDIO_SEQ = 0
INPUT_RECEIVED = 0
INPUT_LOST = 0
LAST_INPUT_AT = 0
SCENARIO = "normal"
REQUEST_COUNT = 0

DIAGNOSTICS = {
    "ok": True, "firmware": "5.0.0-beta.1", "protocol": 4,
    "uptimeMs": 18_420,
    "displayA": {"ok": True, "fps": 30, "frames": 12_860,
                 "heartbeatAgeMs": 12, "stackFreeBytes": 2_912,
                 "frameAvgUs": 33_210, "frameMaxUs": 39_500,
                 "renderAvgUs": 3_380, "renderMaxUs": 7_920,
                 "pushAvgUs": 27_200, "pushMaxUs": 30_100},
    "displayB": {"ok": True, "fps": 15, "frames": 6_430,
                 "heartbeatAgeMs": 26, "stackFreeBytes": 4_736,
                 "frameAvgUs": 66_420, "frameMaxUs": 72_800,
                 "renderAvgUs": 4_820, "renderMaxUs": 9_210,
                 "pushAvgUs": 54_100, "pushMaxUs": 58_300},
    "memory": {"heapFreeBytes": 241_664, "heapMinBytes": 212_480,
               "heapLargestBytes": 131_072, "psramPresent": False,
               "psramFreeBytes": 0, "psramTotalBytes": 0},
    "storage": {"ready": True, "totalBytes": 5_177_344,
                "usedBytes": 612_400, "freeBytes": 4_564_944,
                "systemAssetBytes": 118_400,
                "userPhotoBytes": 372_000, "gameAssetBytes": 122_000,
                "transactionBytes": 0},
    "wifi": {"online": True, "clients": 1},
    "input": {"packets": 0, "accepted": 0, "lost": 0, "stale": 0,
              "timeouts": 0, "latencyMs": 18, "latencyAvgMs": 18,
              "latencyMaxMs": 31, "controllerAlive": False},
    "rtl": {"ok": True, "controlAlive": True, "controlAgeMs": 4,
            "controlStackFreeBytes": 2_176},
    "esc": {"enabled": False, "valid": False, "pulseUs": 0},
    "lastError": "none",
    "restartCount": 3, "resetReason": "power_on",
    "resetReasonCode": 1,
}

BOOT_CLOCK = time.monotonic()
GAME_STARTED = None
GAME_PAUSED_AT = None
RTL_COMMAND_AT = time.monotonic()


def clock_ms():
    return (18_420 + int((time.monotonic() - BOOT_CLOCK) * 1000)) & 0xFFFFFFFF


def emit_audio(kind, value=1.0):
    global AUDIO_SEQ
    AUDIO_SEQ += 1
    AUDIO_EVENTS.append({"seq": AUDIO_SEQ, "type": kind, "value": value})
    del AUDIO_EVENTS[:-24]


def update_game_v4():
    now = clock_ms()
    GAME_V4["serverMs"] = now
    GAME_V4["nowMs"] = now
    GAME_V4["controllerAlive"] = bool(
        LAST_INPUT_AT and now - LAST_INPUT_AT <= 350)
    if GAME_V4["status"] == "countdown" and GAME_STARTED is not None:
        elapsed = max(0, now - GAME_STARTED)
        if elapsed >= 2400:
            GAME_V4["status"] = "running"
            emit_audio("engine_start")
    if GAME_V4["status"] != "running" or GAME_STARTED is None:
        return
    elapsed = max(0, now - GAME_STARTED - 2400)
    throttle = GAME_V4["throttle"] / 100
    brake = GAME_V4["brake"] / 100
    speed = max(0, int(38 + throttle * 182 - brake * 150))
    distance = elapsed * speed / 1000
    checkpoint = min(GAME_V4["totalCheckpoints"], int(distance // 285))
    old_checkpoint = GAME_V4["checkpoint"]
    if checkpoint > old_checkpoint:
        emit_audio("checkpoint")
    lanes = (-80, 74, -20, 96, -110, 42, 8, -64, 118, -35, 55, 0)
    objects = []
    for index in range(checkpoint, min(12, checkpoint + 4)):
        ahead = max(0, int((index + 1) * 285 - distance))
        objects.append({"distanceDm": ahead * 5, "lateral": lanes[index],
                        "radius": 13 + index % 7,
                        "kind": ("boulder", "gate", "rut")[index % 3]})
    GAME_V4.update(timeMs=elapsed, speed=speed, rpm=900 + speed * 29,
                   distanceCm=int(distance * 100), worldDistance=int(distance),
                   lineX=int(GAME_V4["steer"] * .65), checkpoint=checkpoint,
                   score=elapsed // 10 + checkpoint * 350,
                   tempC=min(112, 72 + int(throttle * 28)),
                   grip=max(350, 1000 - abs(GAME_V4["steer"]) // 2),
                   objects=objects)
    if checkpoint >= GAME_V4["totalCheckpoints"]:
        GAME_V4["status"] = "finished"
        GAME_V4["speed"] = 0
        emit_audio("finish")


def game_payload(audio_after=0):
    update_game_v4()
    out = deepcopy(GAME_V4)
    out["audio"] = [event for event in AUDIO_EVENTS
                    if event["seq"] > audio_after]
    return out


def photos_payload():
    out = deepcopy(PHOTOS)
    if SCENARIO == "storage-full":
        out.update(usedBytes=out["totalBytes"] - 12_000, freeBytes=12_000)
        out["userPhotoBytes"] = out["usedBytes"] - out["systemAssetBytes"] - out["gameAssetBytes"]
        out["breakdown"]["userPhotoBytes"] = out["userPhotoBytes"]
    elif SCENARIO == "fs-offline":
        out.update(fsReady=False, storageState="unavailable", usedBytes=0,
                   totalBytes=0, freeBytes=0, degraded=True,
                   lastError="filesystem_unavailable", bootDriver=False,
                   bootNavigator=False, slots=[])
    return out


def adjust_photo_storage(delta):
    PHOTOS["userPhotoBytes"] = max(0, PHOTOS["userPhotoBytes"] + delta)
    PHOTOS["breakdown"]["userPhotoBytes"] = PHOTOS["userPhotoBytes"]
    PHOTOS["usedBytes"] = (PHOTOS["systemAssetBytes"] +
                           PHOTOS["gameAssetBytes"] +
                           PHOTOS["userPhotoBytes"])
    PHOTOS["freeBytes"] = max(0, PHOTOS["totalBytes"] - PHOTOS["usedBytes"])


def obstacle_views(distance):
    lanes = (-35, 47, 5, -58, 30, -12, 63, -43, 17, 52, -25, 2, 39, -60)
    views = []
    processed = min(14, max(0, int(distance // 260)))
    for index in range(processed, min(14, processed + 4)):
        ahead = max(0, int((index + 1) * 260 - distance))
        views.append({"lineX": lanes[index], "distance": ahead,
                      "radius": 12 + index % 6, "kind": index % 3,
                      "kindName": ("boulder", "log", "rut")[index % 3]})
    return processed, views


def update_game():
    now = clock_ms()
    game = STATE["game"]
    if STATE["mode"] != "game" or GAME_STARTED is None:
        return
    elapsed = max(0, now - GAME_STARTED)
    if game["status"] == "countdown":
        game["phaseMs"] = elapsed
        if elapsed >= 2400:
            game["status"] = "playing"
    if game["status"] != "playing":
        return
    play = max(0, elapsed - 2400)
    speed = 215 if game["throttle"] else 88
    distance = play * speed / 1000
    processed, views = obstacle_views(distance)
    game.update(phaseMs=play, elapsedMs=play, speed=speed,
                worldDistance=int(distance) & 0xFFFF, score=play // 12,
                obstacle=processed, progress=min(1000, int(distance / 3640 * 1000)),
                nearestDistance=views[0]["distance"] if views else 0,
                visibleObstacles=len(views), obstacles=views)
    if processed >= 14:
        game.update(status="finished", courseClear=True)


def update_rtl_pixels(now=None):
    now = now or time.monotonic()
    low = RTL["function"] in ("left", "right", "hazard") and int(
        (now - RTL_COMMAND_AT) / .3) % 2
    red, bright_red = [64, 0, 0], [255, 0, 0]
    amber, bright_amber, white = [176, 44, 0], [255, 92, 0], [255, 255, 220]
    pixels = [red] * 4 + [amber] * 2 + [[0, 0, 0]] * 4 + [amber] * 2 + [red] * 4
    fn = RTL["function"]
    if fn == "brake": pixels[:4] = [bright_red] * 4; pixels[-4:] = [bright_red] * 4
    if fn in ("left", "hazard"): pixels[4:6] = ([amber] if low else [bright_amber]) * 2
    if fn in ("right", "hazard"): pixels[10:12] = ([amber] if low else [bright_amber]) * 2
    if fn == "reverse_work": pixels[6:10] = [white] * 4
    if fn == "dust_chase":
        k = int((now - RTL_COMMAND_AT) / .12) % 2
        pixels[4 + k] = bright_amber; pixels[11 - k] = bright_amber
    if fn == "pit_test":
        phase = int((now - RTL_COMMAND_AT) / .4) % 5
        pixels = [[0, 0, 0]] * 16
        zones = ((0, 4, bright_red), (4, 6, bright_amber),
                 (6, 10, white), (10, 12, bright_amber))
        if phase == 4:
            for a, b, color in zones: pixels[a:b] = [color] * (b - a)
            pixels[12:] = [bright_red] * 4
        elif phase < 4:
            a, b, color = zones[phase]; pixels[a:b] = [color] * (b - a)
    RTL["pixels"] = pixels


def current_state():
    STATE["nowMs"] = clock_ms()
    update_game()
    update_rtl_pixels()
    return deepcopy(STATE)


def preview_svg(label):
    safe = re.sub(r"[^A-Z0-9 #]", "", label.upper())[:24]
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="480" height="300">
<defs><linearGradient id="g" x2="1" y2="1"><stop stop-color="#f29b18"/><stop offset="1" stop-color="#281509"/></linearGradient></defs>
<rect width="480" height="300" fill="#0d0b09"/><circle cx="380" cy="80" r="95" fill="url(#g)"/>
<path d="M0 230L90 140l70 64 90-110 90 104 80-78 60 90v90H0z" fill="#34241a"/>
<path d="M210 300l35-150 45 150" fill="#17120e" stroke="#f29b18" stroke-width="5"/>
<text x="24" y="46" fill="#ffc14a" font-family="monospace" font-size="22" font-weight="bold">ULTRA4 MEDIA</text>
<text x="24" y="275" fill="white" font-family="sans-serif" font-size="25" font-weight="bold">{safe}</text></svg>'''.encode()


class Handler(BaseHTTPRequestHandler):
    def _send(self, body, content_type="application/json", status=200):
        if not isinstance(body, bytes): body = json.dumps(body).encode()
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if body: self.wfile.write(body)

    def do_GET(self):
        global SCENARIO, REQUEST_COUNT
        parsed = urlparse(self.path)
        path = parsed.path
        args = parse_qs(parsed.query)
        REQUEST_COUNT += 1
        if path == "/" and "scenario" in args:
            SCENARIO = args["scenario"][0]
        if SCENARIO == "loss" and path.startswith("/api/") and REQUEST_COUNT % 6 == 0:
            self._send({"ok": False, "error": "simulated_link_loss"}, status=503)
        elif path == "/api/v4/game/state":
            after = int(args.get("audioAfter", ["0"])[0] or 0)
            self._send(game_payload(after))
        elif path == "/api/v4/diagnostics":
            photo_state = photos_payload()
            DIAGNOSTICS["uptimeMs"] = clock_ms()
            DIAGNOSTICS["input"].update(packets=INPUT_RECEIVED,
                                        accepted=INPUT_RECEIVED,
                                        lost=INPUT_LOST,
                                        latencyMs=GAME_V4["inputRttMs"],
                                        controllerAlive=GAME_V4["controllerAlive"])
            DIAGNOSTICS["storage"].update(
                ready=photo_state["fsReady"],
                totalBytes=photo_state["totalBytes"],
                usedBytes=photo_state["usedBytes"],
                freeBytes=photo_state["freeBytes"],
                systemAssetBytes=photo_state["systemAssetBytes"],
                userPhotoBytes=photo_state["userPhotoBytes"],
                gameAssetBytes=photo_state["gameAssetBytes"])
            DIAGNOSTICS["lastError"] = "none"
            if SCENARIO == "fs-offline":
                DIAGNOSTICS["storage"]["ready"] = False
                DIAGNOSTICS["lastError"] = "filesystem_unavailable"
            self._send(DIAGNOSTICS)
        elif path in ("/api/v2/state", "/api/v3/state", "/api/v2/profile", "/api/v3/profile"):
            self._send(current_state())
        elif path in ("/api/v2/capabilities", "/api/v3/capabilities"):
            self._send({"ok": True, "protocol": 4, "version": "5.0.0-beta.1",
                        "features": ["trailAssault", "rtlController", "escStateMachine",
                                     "realismDash", "stockOffroadDash", "bilingualUi",
                                     "photoStorageV3", "mediaPreview",
                                     "webAudio", "diagnosticsV4", "analogController",
                                     "uniqueApCredentials", "rtlOutputWatchdog"],
                        "maxClients": 2, "stateHz": 10, "inputHz": 20})
        elif path == "/api/settings": self._send(SETTINGS)
        elif path == "/api/photos": self._send(photos_payload())
        elif path == "/api/v3/rtl/state":
            update_rtl_pixels(); self._send(RTL)
        elif path in ("/api/media/boot", "/api/media/photo"):
            if SCENARIO == "fs-offline":
                self._send({"ok": False, "error": "filesystem_unavailable"},
                           status=503)
            else:
                slot = args.get("slot", ["BOOT"])[0]
                self._send(preview_svg(f"PHOTO {slot}"), "image/svg+xml")
        elif path == "/qa-sample.png":
            self._send(QA_PNG, "image/png")
        else:
            page = PAGE.replace(b"</body>", QA_SCRIPT + b"</body>") if args.get("qa") == ["1"] else PAGE
            self._send(page, "text/html; charset=utf-8")

    def do_POST(self):
        global GAME_STARTED, GAME_PAUSED_AT, RTL_COMMAND_AT
        global INPUT_RECEIVED, INPUT_LOST, LAST_INPUT_AT, REQUEST_COUNT
        length = int(self.headers.get("Content-Length", 0))
        raw = self.rfile.read(length)
        parsed = urlparse(self.path)
        content_type = self.headers.get("Content-Type", "")
        body = raw.decode(errors="ignore") if "multipart/form-data" not in content_type else ""
        args = parse_qs(parsed.query); args.update(parse_qs(body))
        path = parsed.path
        REQUEST_COUNT += 1

        if SCENARIO == "loss" and path.startswith("/api/") and REQUEST_COUNT % 5 == 0:
            self._send({"ok": False, "error": "simulated_link_loss"}, status=503)
            return
        if SCENARIO == "fs-offline" and path.startswith("/api/upload/"):
            self._send({"ok": False, "error": "filesystem_unavailable"}, status=503)
            return
        if SCENARIO == "storage-full" and path.startswith("/api/upload/"):
            self._send({"ok": False, "error": "no_space"}, status=507)
            return

        if path == "/api/v4/game/config":
            mode = args.get("mode", ["time_attack"])[0]
            if mode not in ("time_attack", "gate_rush", "hill_climb"):
                mode = "time_attack"
            try: seed = max(1, int(args.get("seed", ["4400"])[0]))
            except ValueError: seed = 4400
            GAME_V4.update(mode=mode, seed=seed, status="attract",
                           experienceMode="game")
        elif path == "/api/v4/game/input":
            seq = int(args.get("seq", ["0"])[0] or 0)
            if GAME_V4["ackSeq"] and seq > GAME_V4["ackSeq"] + 1:
                INPUT_LOST += seq - GAME_V4["ackSeq"] - 1
            INPUT_RECEIVED += 1
            LAST_INPUT_AT = clock_ms()
            steer = max(-100, min(100, int(args.get("steer", ["0"])[0])))
            throttle = max(0, min(100, int(args.get("throttle", ["0"])[0])))
            brake = max(0, min(100, int(args.get("brake", ["0"])[0])))
            GAME_V4.update(ackSeq=seq, controllerAlive=True, inputRttMs=18,
                           steer=steer, throttle=throttle, brake=brake)
        elif path == "/api/v4/game/neutral":
            LAST_INPUT_AT = 0
            seq = int(args.get("seq", [str(GAME_V4["ackSeq"] + 1)])[0])
            GAME_V4.update(ackSeq=seq, controllerAlive=False, steer=0,
                           throttle=0, brake=0)
        elif path == "/api/v4/game/action":
            action = args.get("action", ["quit"])[0]
            if action in ("start", "restart"):
                GAME_STARTED = clock_ms(); GAME_PAUSED_AT = None
                GAME_V4.update(status="countdown", experienceMode="game",
                               score=0, checkpoint=0, timeMs=0, damage=0,
                               distanceCm=0, objects=[])
                emit_audio("countdown")
            elif action == "pause" and GAME_V4["status"] == "running":
                GAME_PAUSED_AT = clock_ms(); GAME_V4["status"] = "paused"
            elif action == "resume" and GAME_V4["status"] == "paused":
                if GAME_STARTED is not None and GAME_PAUSED_AT is not None:
                    GAME_STARTED += clock_ms() - GAME_PAUSED_AT
                GAME_PAUSED_AT = None; GAME_V4["status"] = "running"
            elif action in ("exit", "quit"):
                GAME_STARTED = None; GAME_PAUSED_AT = None
                GAME_V4.update(status="idle", experienceMode="dashboard",
                               speed=0, rpm=900,
                               controllerAlive=False, steer=0, throttle=0,
                               brake=0)
        elif path in ("/api/v2/profile", "/api/v3/profile"):
            for key in ("number", "vehicle", "driver"):
                if key in args:
                    target = "vehicleName" if key == "vehicle" else key
                    STATE["identity"][target] = args[key][0].upper()
                    if key == "vehicle": STATE["identity"]["vehicle"] = args[key][0].upper()
            SETTINGS["setupComplete"] = True
        elif path == "/api/settings":
            if "displayTheme" in args:
                SETTINGS["displayTheme"] = int(args["displayTheme"][0])
            if "language" in args:
                SETTINGS["language"] = int(args["language"][0])
            for key in ("driverBrightness", "navigatorBrightness", "bootHoldMs",
                        "photoTarget", "photoIntervalS", "photoTransition"):
                if key in args: SETTINGS[key] = int(args[key][0])
            SETTINGS["useKmh"] = args.get("units", ["imperial"])[0] == "metric"
            SETTINGS["useCelsius"] = args.get("temperature", ["f"])[0] == "c"
            for key in ("bootImageEnabled", "photoModeEnabled", "photoRandom", "photoOverlay"):
                SETTINGS[key] = args.get(key, [""])[0] == "on"
        elif path == "/api/v2/game/start":
            STATE["mode"] = "game"; GAME_STARTED = clock_ms(); GAME_PAUSED_AT = None
            STATE["game"].update(status="countdown", phaseMs=0, score=0, combo=0,
                                 lives=3, obstacle=0, x=0, lineX=0, speed=0,
                                 throttle=0, steer=0, worldDistance=0,
                                 courseClear=False)
        elif path == "/api/v2/game/input":
            if STATE["mode"] == "game":
                steer = max(-100, min(100, int(args.get("steer", ["0"])[0])))
                throttle = int(args.get("throttle", ["0"])[0]) > 0
                x = max(-88, min(88, STATE["game"]["x"] + steer // 20))
                STATE["game"].update(x=x, lineX=x, steer=steer,
                                     throttle=1 if throttle else 0)
        elif path == "/api/v2/game/action":
            action = args.get("action", ["quit"])[0]
            if action in ("start", "restart"):
                STATE["mode"] = "game"; GAME_STARTED = clock_ms(); GAME_PAUSED_AT = None
                STATE["game"].update(status="countdown", score=0, lives=3,
                                     obstacle=0, courseClear=False)
            elif action == "enter":
                STATE["mode"] = "game"; GAME_STARTED = None
                STATE["game"]["status"] = "attract"
            elif action == "pause" and STATE["game"]["status"] == "playing":
                GAME_PAUSED_AT = clock_ms(); STATE["game"]["status"] = "paused"
            elif action == "resume" and STATE["game"]["status"] == "paused":
                if GAME_STARTED is not None and GAME_PAUSED_AT is not None:
                    GAME_STARTED += clock_ms() - GAME_PAUSED_AT
                GAME_PAUSED_AT = None; STATE["game"]["status"] = "playing"
            elif action == "quit":
                STATE["mode"] = "dashboard"; GAME_STARTED = None; GAME_PAUSED_AT = None
                STATE["game"]["status"] = "idle"
        elif path == "/api/v3/rtl/config":
            RTL["profile"] = args.get("profile", [RTL["profile"]])[0]
            RTL["brightness"] = int(args.get("brightness", [RTL["brightness"]])[0])
            RTL["escEnabled"] = args.get("escEnabled", ["0"])[0] == "1"
            RTL["escType"] = args.get("escType", [RTL["escType"]])[0]
            RTL["escNeutralUs"] = int(args.get("escNeutralUs", [RTL["escNeutralUs"]])[0])
            RTL["escDeadbandUs"] = int(args.get("escDeadbandUs", [RTL["escDeadbandUs"]])[0])
            RTL["escReverseConfirmMs"] = int(args.get("escReverseConfirmMs", [RTL["escReverseConfirmMs"]])[0])
        elif path == "/api/v3/rtl/command":
            RTL["function"] = args.get("function", ["running"])[0]
            if RTL["function"] == "clear": RTL["function"] = "running"
            RTL_COMMAND_AT = time.monotonic(); RTL["timedOut"] = False
        elif path == "/api/photos/commit":
            slot = int(args.get("slot", ["0"])[0])
            if slot not in PHOTOS["slots"]:
                PHOTOS["slots"].append(slot); adjust_photo_storage(92_000)
        elif path == "/api/photos/delete":
            slot = int(args.get("slot", ["-1"])[0])
            if slot in PHOTOS["slots"]:
                PHOTOS["slots"].remove(slot); adjust_photo_storage(-92_000)
        elif path == "/api/boot/commit":
            if not (PHOTOS["bootDriver"] and PHOTOS["bootNavigator"]):
                adjust_photo_storage(74_000)
            PHOTOS.update(bootDriver=True, bootNavigator=True)
        elif path == "/api/boot/delete":
            if PHOTOS["bootDriver"] or PHOTOS["bootNavigator"]:
                adjust_photo_storage(-74_000)
            PHOTOS.update(bootDriver=False, bootNavigator=False)

        if path == "/api/v4/game/input":
            self._send({"ok": True, "accepted": True,
                        "seq": GAME_V4["ackSeq"],
                        "serverMs": clock_ms(),
                        "result": "accepted",
                        "inputRttMs": GAME_V4["inputRttMs"]})
        elif path == "/api/v4/game/neutral":
            self._send({"ok": True, "seq": GAME_V4["ackSeq"],
                        "serverMs": clock_ms(), "result": "accepted"})
        elif path.startswith("/api/v4/game/"):
            self._send(game_payload())
        elif path == "/api/v2/game/input": self._send(b"", "text/plain", 204)
        elif path.startswith("/api/v3/rtl/"):
            update_rtl_pixels(); self._send(RTL)
        elif path.startswith("/api/v2/game/"): self._send(current_state())
        else: self._send({"ok": True})

    def log_message(self, *_):
        pass


if __name__ == "__main__":
    print("Portal preview: http://127.0.0.1:8765", flush=True)
    ThreadingHTTPServer(("127.0.0.1", 8765), Handler).serve_forever()
