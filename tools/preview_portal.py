#!/usr/bin/env python3
"""Local, read-only preview of the embedded AP portal for visual QA."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json
import re

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
        if self.path.startswith("/api/settings"):
            self._send(SETTINGS)
        elif self.path.startswith("/api/photos"):
            self._send(PHOTOS)
        else:
            self._send(PAGE, "text/html; charset=utf-8")

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        self.rfile.read(length)
        self._send({"ok": True})

    def log_message(self, *_):
        pass

if __name__ == "__main__":
    print("Portal preview: http://127.0.0.1:8765", flush=True)
    ThreadingHTTPServer(("127.0.0.1", 8765), Handler).serve_forever()
