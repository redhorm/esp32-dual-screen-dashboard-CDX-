#!/usr/bin/env python3
"""Render the actual SHOW firmware to an 18 s GIF and a static storyboard.

Usage: python3 tools/make_show_preview.py OUTPUT_DIRECTORY [--fps 15] [--language it|en]
Requires a C++17 compiler on PATH (or CXX) and Pillow. No device is required.
The pilot inset is projected through the supplied optical CAD contract; no
image-generation model, photographs, or approximate redrawn LCD text are used.
"""

import argparse
import json
import os
import shlex
import subprocess
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
BACKGROUND = (10, 20, 27)
CARD = (18, 31, 39)
WHITE = (236, 245, 247)
CYAN = (52, 193, 225)
MUTED = (146, 169, 179)
MASK = (22, 24, 27)
EDGE = (75, 86, 90)


def font(size):
    bundled = ROOT / "tools/fonts/BarlowCondensed-Bold.ttf"
    return ImageFont.truetype(str(bundled), size) if bundled.exists() else ImageFont.load_default()


def phase(t, english):
    if t < 2000:
        return "01  /  16 LIGHT POINTS" if english else "01  /  16 PUNTI DI LUCE"
    if t < 8000:
        return "02  /  ROUTE IN MOTION" if english else "02  /  PERCORSO IN MOVIMENTO"
    if t < 14500:
        return "03  /  TWO SCREENS, ONE BEAT" if english else "03  /  DUE SCHERMI, UN RITMO"
    return "04  /  RECOIL PULSE"


def pilot_apertures(scale):
    contract = json.loads((ROOT / "references/recoil/Coordinate_luci_160x80.json").read_text())
    width, height = contract["display_px"]
    active_width, active_height = contract["active_mm"]
    aperture = Image.new("L", (width * scale, height * scale))
    draw = ImageDraw.Draw(aperture)

    def xy(x, y):
        return ((x + active_width / 2) * width * scale / active_width,
                (active_height / 2 - y) * height * scale / active_height)

    polygon = [xy(x, y) for x, y in contract["main_window_polygon_mm"]]
    draw.polygon(polygon, fill=255)
    radius = contract["hole_diameter_mm"] / 2
    circles = []
    for hole in contract["holes"]:
        x, y = hole["x_mm"], hole["y_mm"]
        p1, p2 = xy(x - radius, y + radius), xy(x + radius, y - radius)
        box = (*p1, *p2)
        draw.ellipse(box, fill=255)
        circles.append(box)
    return aperture, polygon, circles


def mask_pilot(image, mask_info):
    aperture, polygon, circles = mask_info
    lcd = image.resize(aperture.size, Image.Resampling.NEAREST)
    masked = Image.composite(lcd, Image.new("RGB", lcd.size, MASK), aperture)
    draw = ImageDraw.Draw(masked)
    # Outlines are outside the optical opening: the live light pixels are not
    # painted over. This is a front projection of apertures, not the full bezel.
    outlines = Image.new("RGB", lcd.size, MASK)
    edge = ImageDraw.Draw(outlines)
    edge.line(polygon + [polygon[0]], fill=EDGE, width=2)
    for x0, y0, x1, y1 in circles:
        edge.ellipse((x0 - 1, y0 - 1, x1 + 1, y1 + 1), outline=EDGE, width=2)
    return Image.composite(masked, outlines, aperture)


def compose(pilot, navigator, t, english, mask_info):
    image = Image.new("RGB", (840, 1070), BACKGROUND)
    d = ImageDraw.Draw(image)
    d.text((42, 25), "RECOIL PULSE", font=font(50), fill=WHITE)
    d.text((44, 89), phase(t, english), font=font(26), fill=CYAN)
    d.text((690, 40), f"{t / 1000:04.1f} s", font=font(30), fill=CYAN)
    d.rounded_rectangle((30, 133, 810, 523), radius=14, fill=CARD)
    d.text((55, 144), "PILOT / 160 x 80" if english else "PILOTA / 160 x 80", font=font(25), fill=MUTED)
    image.paste(mask_pilot(pilot, mask_info), (100, 190))
    d.rounded_rectangle((30, 548, 810, 1010), radius=14, fill=CARD)
    d.text((55, 556), "NAVIGATOR / 240 x 135" if english else "COPILOTA / 240 x 135", font=font(25), fill=MUTED)
    image.paste(navigator.resize((720, 405), Image.Resampling.NEAREST), (60, 598))
    caption = "REAL FIRMWARE PIXELS / CAD APERTURES / DEMONSTRATION" if english else "PIXEL DEL FIRMWARE / APERTURE CAD / DATI DIMOSTRATIVI"
    d.text((42, 1030), caption, font=font(20), fill=MUTED)
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--fps", type=int, choices=range(1, 16), default=15)
    parser.add_argument("--language", choices=("it", "en"), default="it")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    english = args.language == "en"
    mask_info = pilot_apertures(4)
    count = args.fps * 18
    frames = []
    # GIF timestamps are multiples of 10 ms. Alternating frame durations keeps
    # the playback at exactly 18 seconds, rather than truncating 15 fps to 16.2 s.
    boundaries = [round(i * 100 / args.fps) * 10 for i in range(count + 1)]
    durations = [b - a for a, b in zip(boundaries, boundaries[1:])]
    with tempfile.TemporaryDirectory(prefix="recoil-show-") as directory:
        temp = Path(directory)
        executable = temp / "show_preview"
        compiler = shlex.split(os.environ.get("CXX", "c++"))
        subprocess.run(compiler + ["-std=c++17", "-O2", "-Wall", "-Wextra", "-I" + str(ROOT / "src"),
                                  str(ROOT / "tools/show_preview.cpp"), str(ROOT / "src/core/show_sequence.cpp"),
                                  "-o", str(executable)], check=True)
        subprocess.run([str(executable), str(temp), str(args.fps), args.language], check=True)
        for i in range(count):
            with Image.open(temp / f"{i:04d}_pilot.ppm") as pilot, Image.open(temp / f"{i:04d}_navigator.ppm") as navigator:
                frames.append(compose(pilot, navigator, i * 1000 // args.fps, english, mask_info))
        gif = args.output / "RECOIL_PULSE_SHOW_18s.gif"
        frames[0].save(gif, save_all=True, append_images=frames[1:], duration=durations, loop=0,
                       optimize=False, disposal=2)
        # Four readable moments, two columns. Downsizing uses exact integer
        # nearest-neighbour sampling; the original single frames are kept too.
        snapshots = (0, 5000, 8000, 16000)
        story = Image.new("RGB", (840, 1070), BACKGROUND)
        for i, at in enumerate(snapshots):
            rendered = frames[min(at * args.fps // 1000, count - 1)]
            rendered.save(args.output / f"RECOIL_PULSE_{at // 1000:02d}s.png")
            story.paste(rendered.resize((420, 535), Image.Resampling.NEAREST), ((i % 2) * 420, (i // 2) * 535))
        story.save(args.output / "RECOIL_PULSE_storyboard.png")
    # Verify the encoded file, not merely the requested source duration.
    with Image.open(gif) as check:
        actual_duration = 0
        for i in range(check.n_frames):
            check.seek(i)
            actual_duration += check.info["duration"]
        if actual_duration != 18000 or check.width > 1200:
            raise RuntimeError(f"Unexpected GIF duration or width: {actual_duration}, {check.width}")
        metadata = {"duration_ms": actual_duration, "frames": check.n_frames, "fps_requested": args.fps,
                    "size_px": check.size, "language": args.language, "renderer": "src/app/show_renderer.h",
                    "pixel_scaling": "nearest", "pilot_apertures": "references/recoil/Coordinate_luci_160x80.json",
                    "hardware_capture": False}
    (args.output / "SHOW_anteprima_verifica.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(f"Saved verified 18-second GIF, four frames and storyboard to {args.output}")


if __name__ == "__main__":
    main()
