#!/usr/bin/env python3
"""Render real startup pixels to a 5.6-second GIF and PNG storyboard.

Usage: python3 tools/make_startup_preview.py OUTPUT_DIRECTORY [--fps 15]
Requires Pillow and a C++17 compiler. No ESP32 or installed image assets needed.
"""
import argparse
import json
import math
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

from PIL import Image, ImageDraw
from make_show_preview import ROOT, BACKGROUND, CARD, WHITE, CYAN, MUTED, font, pilot_apertures, mask_pilot

DURATION_MS = 5600


def phases(t, english):
    if english:
        pilot = "LCD SEGMENT CHECK" if t < 1000 else "MODEL / FIRMWARE" if t < 4000 else "NEUTRAL / DEMO"
        nav = "HDS LIVE INTRO" if t < 3000 else "CHART DRAWING" if t < 4600 else "STATIONARY CHART"
    else:
        pilot = "TEST SEGMENTI LCD" if t < 1000 else "MODELLO / FIRMWARE" if t < 4000 else "FOLLE / DEMO"
        nav = "INTRO HDS LIVE" if t < 3000 else "DISEGNO CARTA" if t < 4600 else "CARTA FERMA"
    return pilot, nav


def compose(pilot, navigator, t, english, mask_info):
    result = Image.new("RGB", (840, 1070), BACKGROUND)
    d = ImageDraw.Draw(result)
    a, b = phases(t, english)
    d.text((42, 25), "RECOIL / POWER ON", font=font(50), fill=WHITE)
    d.text((44, 88), "STARTUP ADAPTATION" if english else "ACCENSIONE ADATTATA", font=font(26), fill=CYAN)
    d.text((695, 42), f"{t / 1000:.1f} s", font=font(30), fill=CYAN)
    d.rounded_rectangle((30, 133, 810, 523), radius=14, fill=CARD)
    d.text((55, 144), "PILOT / 160 x 80" if english else "PILOTA / 160 x 80", font=font(25), fill=MUTED)
    d.text((470, 144), a, font=font(25), fill=CYAN)
    result.paste(mask_pilot(pilot, mask_info), (100, 190))
    d.rounded_rectangle((30, 548, 810, 1010), radius=14, fill=CARD)
    d.text((55, 556), "NAVIGATOR / 240 x 135" if english else "COPILOTA / 240 x 135", font=font(25), fill=MUTED)
    d.text((505, 556), b, font=font(25), fill=CYAN)
    result.paste(navigator.resize((720, 405), Image.Resampling.NEAREST), (60, 598))
    caption = "REAL FIRMWARE PIXELS / CAD APERTURES / DEMO" if english else "PIXEL DEL FIRMWARE / APERTURE CAD / DEMO"
    d.text((42, 1030), caption, font=font(20), fill=MUTED)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--fps", type=int, choices=range(1, 16), default=15)
    parser.add_argument("--language", choices=("it", "en"), default="it")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    mask_info = pilot_apertures(4)
    english = args.language == "en"
    count = math.ceil(DURATION_MS * args.fps / 1000)
    boundaries = [min(DURATION_MS, round(i * 100 / args.fps) * 10) for i in range(count)] + [DURATION_MS]
    durations = [b-a for a, b in zip(boundaries, boundaries[1:])]
    frames = []
    with tempfile.TemporaryDirectory(prefix="recoil-startup-") as directory:
        temp = Path(directory)
        executable = temp / "startup_preview"
        compiler = shlex.split(os.environ.get("CXX", "c++"))
        subprocess.run(compiler + ["-std=c++17", "-DNATIVE_BUILD", "-O2", "-Wall", "-Wextra",
                                  "-I"+str(ROOT/"src"), str(ROOT/"tools/startup_preview.cpp"),
                                  "-o", str(executable)], check=True)
        subprocess.run([str(executable), str(temp), str(args.fps), args.language], check=True)
        for i in range(count):
            with Image.open(temp/f"{i:04d}_pilot.ppm") as pilot, Image.open(temp/f"{i:04d}_navigator.ppm") as navigator:
                frames.append(compose(pilot, navigator, i*1000//args.fps, english, mask_info))
        gif = args.output / "RECOIL_ACCENSIONE_5_6s.gif"
        frames[0].save(gif, save_all=True, append_images=frames[1:], duration=durations,
                       loop=0, optimize=False, disposal=2)
        snapshots = (0, 1500, 3500, 5000)
        story = Image.new("RGB", (840, 1070), BACKGROUND)
        for i, at in enumerate(snapshots):
            rendered = frames[min(at*args.fps//1000, count-1)]
            rendered.save(args.output/f"RECOIL_ACCENSIONE_{at:04d}ms.png")
            story.paste(rendered.resize((420, 535), Image.Resampling.NEAREST), ((i%2)*420, (i//2)*535))
        story.save(args.output/"RECOIL_ACCENSIONE_storyboard.png")
    with Image.open(gif) as check:
        actual_duration = 0
        for i in range(check.n_frames):
            check.seek(i)
            actual_duration += check.info["duration"]
        if actual_duration != DURATION_MS or check.width > 1200:
            raise RuntimeError(f"Unexpected encoded preview: {actual_duration} ms, width {check.width}")
        metadata = {"duration_ms": actual_duration, "frames": check.n_frames, "fps_requested": args.fps,
                    "size_px": check.size, "language": args.language, "renderer": "src/app/startup_renderer.h",
                    "pilot_apertures": "references/recoil/Coordinate_luci_160x80.json",
                    "pixel_scaling": "nearest", "hardware_capture": False,
                    "authentic_boot_duration_claim": False}
    (args.output/"ACCENSIONE_anteprima_verifica.json").write_text(json.dumps(metadata, indent=2)+"\n")
    print(f"Saved verified 5.6-second startup GIF, frames and storyboard to {args.output}")


if __name__ == "__main__":
    main()
