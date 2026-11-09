#!/usr/bin/env python3
"""Generate and verify WOW previews from the production C++ engine/renderer."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import json
import subprocess
import tempfile
from make_show_preview import mask_pilot, pilot_apertures

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'output/stormDASH/anteprime'
SCENES = ['MIDNIGHT_RUN', 'LIVING_COCKPIT', 'LIQUID_REACTOR', 'GARAGE_3D', 'BEAT_STORM']
DESCRIPTIONS = [
    'Corse, curve e tunnel · comandi simulati dal telefono',
    'Sguardo, battito e reazioni agli eventi Trail',
    '48 particelle · trascinamento e impulso BOOT',
    'Geometria originale · rotazione e vista esplosa',
    'Spettro dimostrativo · quattro bande e sedici punti',
]
COLORS = ['#40ede7', '#40ede7', '#40ede7', '#a067ff', '#ff589f']
FONT = ROOT / 'tools/fonts/BarlowCondensed-Bold.ttf'
def font(size):
    return ImageFont.truetype(str(FONT), size)

def compose(a, b, scene, frame, mask):
    c = Image.new('RGB', (840, 1080), '#050814')
    d = ImageDraw.Draw(c)
    d.text((42, 22), 'stormDASH', font=font(62), fill='#edf6ff')
    d.text((44, 101), 'WOW / ' + SCENES[scene].replace('_', ' '), font=font(30), fill=COLORS[scene])
    d.text((44, 148), DESCRIPTIONS[scene], font=font(23), fill='#a6b6cf')
    d.rounded_rectangle((30, 194, 810, 565), radius=18, fill='#0d1325')
    d.text((49, 205), 'PILOTA · MASCHERA CAD · 16 PUNTI OTTICI', font=font(24), fill='#a6b6cf')
    c.paste(mask_pilot(a, mask), (100, 241))
    d.rounded_rectangle((30, 589, 810, 1040), radius=18, fill='#0d1325')
    d.text((49, 598), 'COPILOTA · 240 × 135', font=font(24), fill='#a6b6cf')
    if scene == 0 and frame >= 72:
        d.text((574, 598), 'TUNNEL · GARA 24 S', font=font(23), fill=COLORS[scene])
    c.paste(b.resize((720, 405), Image.Resampling.NEAREST), (60, 632))
    d.text((42, 1051), 'RENDER C++ DEL FIRMWARE · ANTEPRIMA, NON RIPRESA HARDWARE', font=font(20), fill='#a6b6cf')
    return c

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    mask = pilot_apertures(4)
    evidence = {'hardware_capture': False, 'renderer': 'src/app/wow_renderer.h',
                'engine': 'src/core/wow_state.h', 'inputs': 'Simulated phone controls; synthetic audio band values',
                'midnight_edit': 'At preview 6 s the race timeline cuts from 6 s to 24 s to show the tunnel',
                'preview_duration_ms': 10000, 'preview_checks': []}
    stories = []
    with tempfile.TemporaryDirectory(prefix='stormdash-wow-') as temp:
        temporary = Path(temp)
        executable = temporary / 'wow-preview'
        subprocess.run(['c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-I'+str(ROOT/'src'), '-I'+str(ROOT/'include'),
                        str(ROOT/'tools/wow_preview.cpp'), '-o', str(executable)], check=True)
        run = subprocess.run([str(executable), str(temporary)], check=True, capture_output=True, text=True)
        evidence['render_checks'] = json.loads(run.stdout)
        print(run.stdout.strip())
        durations = [round((i+1)*100/12)*10 - round(i*100/12)*10 for i in range(120)]
        for scene, name in enumerate(SCENES):
            frames = []
            for i in range(120):
                with Image.open(temporary/f'{scene}-{i:03}-a.ppm') as a, Image.open(temporary/f'{scene}-{i:03}-b.ppm') as b:
                    frames.append(compose(a, b, scene, i, mask))
            still = [45, 69, 65, 61, 45][scene]
            frames[still].save(OUT/f'WOW_{name}.png')
            if scene == 3:
                frames[16].save(OUT/'WOW_GARAGE_ASSEMBLATO.png')
            gif = OUT/f'WOW_{name}.gif'
            frames[0].save(gif, save_all=True, append_images=frames[1:], duration=durations,
                           loop=0, disposal=2, optimize=False)
            with Image.open(gif) as check:
                duration = 0
                for i in range(check.n_frames):
                    check.seek(i); duration += check.info['duration']
                if duration != 10000: raise RuntimeError(f'Unexpected duration {gif}: {duration}')
                evidence['preview_checks'].append({'file': gif.name, 'frames': check.n_frames, 'duration_ms': duration, 'size_px': check.size})
            stories.append(frames[still].resize((420, 540), Image.Resampling.NEAREST))
        with Image.open(temporary/'5-000-a.ppm') as a, Image.open(temporary/'5-000-b.ppm') as b:
            extreme = compose(a, b, 3, 61, mask)
            d = ImageDraw.Draw(extreme)
            d.rectangle((40, 144, 800, 180), fill='#050814')
            d.text((44, 148), 'Espanso al 100% · inclinazione -0,45 rad · rotazione 52°', font=font(23), fill='#a6b6cf')
            extreme.save(OUT/'WOW_GARAGE_LIMITE.png')
    sheet = Image.new('RGB', (1260, 1080), '#050814')
    for i, story in enumerate(stories): sheet.paste(story, ((i%3)*420, (i//3)*540))
    d = ImageDraw.Draw(sheet)
    d.text((885, 618), '5 MODALITÀ', font=font(45), fill='#edf6ff')
    d.text((885, 682), 'UN COCKPIT', font=font(45), fill='#40ede7')
    d.text((885, 754), 'Grafica degli strumenti\noriginali invariata.\n\nQueste anteprime mostrano\nle modalità volontarie WOW.', font=font(26), fill='#a6b6cf', spacing=9)
    sheet.save(OUT/'WOW_PANORAMICA.png')
    (ROOT/'evidence/wow-render.json').write_text(json.dumps(evidence, indent=2)+'\n')
    print('Verified five 10-second animations, snapshots and contact sheet:', OUT)

if __name__ == '__main__':
    main()
