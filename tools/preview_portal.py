#!/usr/bin/env python3
"""Local, read-only preview of the embedded AP portal for visual QA."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json
import re
from urllib.parse import parse_qs

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
}
BETA_STATE = {
    "ok": True, "version": "2.0.0-beta.1", "nowMs": 18420,
    "mode": "dashboard",
    "identity": {"callsign": "TRAIL WOLF", "team": "ULTRA4 CREW",
                 "handle": "@TRAILWOLF", "number": "04", "accent": 53220},
    "vehicle": {"speedMph": 68.0, "rpm": 6840, "gear": 4,
                "heading": 287, "batteryV": 13.9, "activeWp": 4},
    "moment": {"name": "MUD SPLASH", "ageMs": 0},
    "callout": {"name": "", "text": "", "ageMs": 0},
    "badges": {"mask": 46, "latest": "CLEAN LINE", "ageMs": 900},
    "game": {"status": "idle", "result": "none", "x": 0, "safeX": 42,
             "speed": 0, "distanceToGate": 300, "progress": 0,
             "score": 0, "combo": 0, "lives": 3, "gate": 0,
             "gateTotal": 12, "phaseMs": 0, "bestScore": 4820, "wins": 2},
}

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
            self._send(BETA_STATE)
        elif self.path.startswith("/api/settings"):
            self._send(SETTINGS)
        elif self.path.startswith("/api/photos"):
            self._send(PHOTOS)
        else:
            self._send(PAGE, "text/html; charset=utf-8")

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length).decode(errors="ignore")
        args = parse_qs(body)
        if self.path.startswith("/api/v2/game/start"):
            BETA_STATE["mode"] = "game"
            BETA_STATE["game"].update(status="countdown", phaseMs=350, score=0,
                                      combo=0, lives=3, gate=0)
        elif self.path.startswith("/api/v2/game/input"):
            if BETA_STATE["mode"] == "game":
                BETA_STATE["game"].update(status="playing", phaseMs=900,
                                          x=int(args.get("steer", ["0"])[0]) // 2,
                                          speed=186, distanceToGate=165,
                                          score=BETA_STATE["game"]["score"] + 12)
        elif self.path.startswith("/api/v2/game/action"):
            BETA_STATE["mode"] = "dashboard"
            BETA_STATE["game"]["status"] = "idle"
        elif self.path.startswith("/api/v2/moment"):
            BETA_STATE["mode"] = "moment" if args.get("action", [""])[0] != "stop" else "dashboard"
            BETA_STATE["moment"]["name"] = args.get("type", ["mud_splash"])[0].replace("_", " ").upper()
        elif self.path.startswith("/api/v2/callout"):
            BETA_STATE["mode"] = "callout"
            BETA_STATE["callout"]["text"] = args.get("type", ["clear"])[0].upper()
        self._send({"ok": True})

    def log_message(self, *_):
        pass

if __name__ == "__main__":
    print("Portal preview: http://127.0.0.1:8765", flush=True)
    ThreadingHTTPServer(("127.0.0.1", 8765), Handler).serve_forever()
