#!/usr/bin/env python3
"""Local, read-only preview of the embedded AP portal for visual QA."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json
import re
import time
from copy import deepcopy
from urllib.parse import parse_qs, urlparse

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/services/settings_page.h").read_text(encoding="utf-8")
PAGE = re.search(r'R"HTML\((.*)\)HTML";', SOURCE, re.S).group(1).encode()

SETTINGS = {
    "driverTheme": 0, "navigatorTheme": 0, "useKmh": False,
    "useCelsius": True, "driverBrightness": 220,
    "navigatorBrightness": 220, "bootImageEnabled": True,
    "bootHoldMs": 1800, "photoModeEnabled": False, "photoTarget": 3,
    "photoIntervalS": 12, "photoTransition": 1,
    "photoRandom": False, "photoOverlay": True,
}
PHOTOS = {
    "slots": [0, 1, 2], "maxSlots": 24, "bootDriver": True,
    "bootNavigator": True, "usedBytes": 612_400, "totalBytes": 5_177_344,
    "freeBytes": 4_564_944, "fsReady": True,
}
STATE = {
    "ok": True, "version": "3.0.0-beta.2", "nowMs": 18420,
    "mode": "dashboard",
    "identity": {"callsign": "TRAIL WOLF", "team": "ULTRA4 CREW",
                 "handle": "@TRAILWOLF", "number": "04", "accent": 53220},
    "vehicle": {"speedMph": 68.0, "rpm": 6840, "gear": 4,
                "heading": 287, "batteryV": 13.9, "activeWp": 4},
    "rift": {"phase": "idle", "runId": 0, "seed": 294811,
             "startAtMs": 0, "elapsedMs": 0, "progress": 0,
             "layout": 0, "durationMs": 13800},
    "game": {"status": "idle", "result": "none", "x": 0, "safeX": 42,
             "speed": 0, "distanceToGate": 300, "progress": 0,
             "score": 0, "combo": 0, "lives": 3, "gate": 0,
             "gateTotal": 12, "phaseMs": 0, "steer": 0, "throttle": 0,
             "worldDistance": 0, "resultAgeMs": 65535, "elapsedMs": 0,
             "bestScore": 4820, "wins": 2},
}
BOOT_CLOCK = time.monotonic()
RIFT_START_MS = None
GAME_START_MS = None

def clock_ms():
    return (18_420 + int((time.monotonic() - BOOT_CLOCK) * 1000)) & 0xFFFFFFFF

def rift_phase(elapsed):
    if elapsed < 1200: return "blackout"
    if elapsed < 3200: return "scan"
    if elapsed < 6100: return "charge"
    if elapsed < 8200: return "crossover"
    if elapsed < 8800: return "impact"
    if elapsed < 12600: return "hero"
    if elapsed < 13800: return "exit"
    return "idle"

def current_state():
    global RIFT_START_MS, GAME_START_MS
    now = clock_ms()
    STATE["nowMs"] = now
    if STATE["mode"] == "rift" and RIFT_START_MS is not None:
        delta = now - RIFT_START_MS
        if delta < 0:
            STATE["rift"].update(phase="armed", elapsedMs=0, progress=0)
        elif delta >= 13800:
            STATE["mode"] = "dashboard"
            STATE["rift"].update(phase="idle", elapsedMs=0, progress=0)
            RIFT_START_MS = None
        else:
            STATE["rift"].update(phase=rift_phase(delta), elapsedMs=delta,
                                 progress=delta * 1000 // 13800)
    if STATE["mode"] == "game" and GAME_START_MS is not None:
        elapsed = now - GAME_START_MS
        game = STATE["game"]
        if game["status"] == "countdown":
            game["phaseMs"] = max(0, elapsed)
            if elapsed >= 3200:
                game.update(status="playing", phaseMs=elapsed - 3200)
        elif game["status"] == "playing":
            game["phaseMs"] = max(0, elapsed - 3200)
            game["elapsedMs"] = game["phaseMs"]
            game["worldDistance"] = int(game["phaseMs"] * game["speed"] / 1000) & 0xFFFF
            game["distanceToGate"] = max(0, 300 - game["worldDistance"] % 300)
    return deepcopy(STATE)

class Handler(BaseHTTPRequestHandler):
    def _send(self, body, content_type="application/json"):
        if not isinstance(body, bytes):
            body = json.dumps(body).encode()
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path.startswith("/api/v2"):
            self._send(current_state())
        elif self.path.startswith("/api/settings"):
            self._send(SETTINGS)
        elif self.path.startswith("/api/photos"):
            self._send(PHOTOS)
        else:
            self._send(PAGE, "text/html; charset=utf-8")

    def do_POST(self):
        global RIFT_START_MS, GAME_START_MS
        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length).decode(errors="ignore")
        parsed = urlparse(self.path)
        args = parse_qs(parsed.query)
        args.update(parse_qs(body))
        path = parsed.path
        if path == "/api/v2/game/start":
            STATE["mode"] = "game"
            GAME_START_MS = clock_ms()
            STATE["game"].update(status="countdown", phaseMs=0, score=0,
                                 combo=0, lives=3, gate=0, x=0, speed=0,
                                 throttle=0, steer=0, worldDistance=0)
        elif path == "/api/v2/game/input":
            if STATE["mode"] == "game":
                steer = max(-100, min(100, int(args.get("steer", ["0"])[0])))
                throttle = int(args.get("throttle", ["0"])[0]) > 0
                STATE["game"].update(x=steer * 9 // 10, steer=steer,
                                     throttle=1 if throttle else 0,
                                     speed=226 if throttle else 82)
                if STATE["game"]["status"] == "playing":
                    STATE["game"]["score"] += 3
        elif path == "/api/v2/game/action":
            action = args.get("action", ["quit"])[0]
            if action == "enter":
                STATE["mode"] = "game"
                GAME_START_MS = None
                STATE["game"].update(status="attract", phaseMs=0, x=0, speed=0)
            elif action == "restart":
                STATE["mode"] = "game"
                GAME_START_MS = clock_ms()
                STATE["game"].update(status="countdown", phaseMs=0, score=0,
                                     combo=0, lives=3, gate=0)
            else:
                STATE["mode"] = "dashboard"
                GAME_START_MS = None
                STATE["game"]["status"] = "idle"
        elif path == "/api/v2/rift/start":
            STATE["mode"] = "rift"
            RIFT_START_MS = (clock_ms() + 2500) & 0xFFFFFFFF
            STATE["rift"].update(phase="armed", runId=STATE["rift"]["runId"] + 1,
                                 seed=(clock_ms() * 2654435761) & 0xFFFFFFFF,
                                 startAtMs=RIFT_START_MS, elapsedMs=0, progress=0,
                                 layout=int(args.get("layout", ["0"])[0]))
        elif path == "/api/v2/rift/stop":
            STATE["mode"] = "dashboard"
            STATE["rift"]["phase"] = "idle"
            RIFT_START_MS = None
        elif path == "/api/v2/profile":
            for key in ("callsign", "team", "handle", "number"):
                if key in args: STATE["identity"][key] = args[key][0].upper()
            accents = {"lime": 53220, "amber": 64771, "cyan": 26559, "sand": 56947}
            STATE["identity"]["accent"] = accents.get(args.get("accent", ["lime"])[0], 53220)
        elif path == "/api/photos/commit":
            slot = int(args.get("slot", ["0"])[0])
            if slot not in PHOTOS["slots"]: PHOTOS["slots"].append(slot)
        elif path == "/api/photos/delete":
            slot = int(args.get("slot", ["-1"])[0])
            if slot in PHOTOS["slots"]: PHOTOS["slots"].remove(slot)
        elif path == "/api/boot/commit":
            PHOTOS.update(bootDriver=True, bootNavigator=True)
        elif path == "/api/boot/delete":
            PHOTOS.update(bootDriver=False, bootNavigator=False)
        self._send(current_state() if path.startswith("/api/v2/") else {"ok": True})

    def log_message(self, *_):
        pass

if __name__ == "__main__":
    print("Portal preview: http://127.0.0.1:8765", flush=True)
    ThreadingHTTPServer(("127.0.0.1", 8765), Handler).serve_forever()
