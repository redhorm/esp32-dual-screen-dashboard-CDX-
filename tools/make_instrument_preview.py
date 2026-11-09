#!/usr/bin/env python3
"""Generate a 20 s pixel preview of the two instrument reconstructions.

Requires C++17 and Pillow. Simulates the firmware on the host; not a photograph
or a hardware performance measurement. Includes the actual CAD optical mask.
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from PIL import Image, ImageDraw
from make_show_preview import ROOT, BACKGROUND, CARD, WHITE, CYAN, MUTED, font, pilot_apertures, mask_pilot


def compose(a, b, t, apertures):
    image = Image.new("RGB", (840, 1070), BACKGROUND)
    d = ImageDraw.Draw(image)
    d.text((42, 25), "stormDASH", font=font(50), fill=WHITE)
    d.text((44, 86), "DUE STRUMENTI / UN'UNICA SIMULAZIONE", font=font(25), fill=CYAN)
    d.text((710, 40), f"{t / 1000:.1f}s", font=font(28), fill=CYAN)
    d.rounded_rectangle((30, 133, 810, 523), radius=14, fill=CARD)
    d.text((55, 144), "PILOTA / RICOSTRUZIONE IQ3 / 160 x 80", font=font(24), fill=MUTED)
    image.paste(mask_pilot(a, apertures), (100, 190))
    d.rounded_rectangle((30, 548, 810, 1010), radius=14, fill=CARD)
    d.text((55, 556), "COPILOTA / LOWRANCE HDS-7 LIVE / 240 x 135", font=font(24), fill=MUTED)
    image.paste(b.resize((720, 405), Image.Resampling.NEAREST), (60, 598))
    d.text((42, 1030), "RENDERER REALE DEL FIRMWARE / DATI E TERRENO DEMO", font=font(20), fill=MUTED)
    return image


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("output", type=Path)
    args = ap.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    frames = []
    apertures = pilot_apertures(4)
    with tempfile.TemporaryDirectory(prefix="recoil-instruments-") as folder:
        temp = Path(folder)
        exe = temp / "instrument_preview"
        core = ["simulator.cpp", "roadbook.cpp", "race.cpp", "driver.cpp"]
        command = shlex.split(os.environ.get("CXX", "c++")) + [
            "-std=c++17", "-O2", "-Wall", "-Wextra", "-DNATIVE_BUILD",
            "-I" + str(ROOT / "src"), "-I" + str(ROOT / "include"),
            str(ROOT / "tools/instrument_preview.cpp")]
        command += [str(ROOT / "src/core" / name) for name in core]
        subprocess.run(command + ["-o", str(exe)], check=True)
        subprocess.run([str(exe), str(temp)], check=True)
        for i in range(300):
            with Image.open(temp / f"{i:04d}_pilot.ppm") as a, Image.open(temp / f"{i:04d}_navigator.ppm") as b:
                frames.append(compose(a, b, i * 1000 // 15, apertures))
                if i in (0, 150, 299):
                    a.save(args.output / f"IQ3_pixel_{i:03d}.png")
                    b.save(args.output / f"HDS_pixel_{i:03d}.png")
        durations = [round((i+1)*100/15)*10 - round(i*100/15)*10 for i in range(300)]
        gif = args.output / "STRUMENTI_IQ3_HDS_20s.gif"
        frames[0].save(gif, save_all=True, append_images=frames[1:], duration=durations,
                       loop=0, optimize=False, disposal=2)
        frames[150].save(args.output / "STRUMENTI_IQ3_HDS.png")
    with Image.open(gif) as check:
        duration = 0
        for i in range(check.n_frames):
            check.seek(i); duration += check.info["duration"]
        assert duration == 20000
        metadata = {"hardware_capture": False, "duration_ms": duration, "frames": check.n_frames,
                    "renderers": ["instrument_pilot.h", "instrument_navigator.h"],
                    "vehicle_state": "shared Simulator/RaceController", "timeline_start_ms": 20000,
                    "pilot_mask": "references/recoil/Coordinate_luci_160x80.json"}
    (args.output / "STRUMENTI_verifica.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(f"Saved verified 20 s instrument preview to {args.output}")


if __name__ == "__main__":
    main()
