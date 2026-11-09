#!/usr/bin/env python3
"""Generate original, self-contained SVG illustrations for the local AP portal.

No downloaded imagery, fonts, dependencies, live readings or firmware changes.
The cockpit is a deliberately labelled illustration of the installed displays.
"""
from pathlib import Path
import math

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets/portal"
OUT.mkdir(parents=True, exist_ok=True)


def svg(name, body, view="0 0 240 135"):
    content = f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{view}" aria-hidden="true" focusable="false">{body}</svg>'
    (OUT / f"{name}.svg").write_text(content + "\n")
    return content


def text(x, y, value, size=7, color="#202741", extra=""):
    return f'<text x="{x}" y="{y}" font-family="ui-monospace,monospace" font-size="{size}" fill="{color}" {extra}>{value}</text>'


def line(points, color, width=1, extra=""):
    return f'<polyline points="{points}" fill="none" stroke="{color}" stroke-width="{width}" stroke-linecap="round" stroke-linejoin="round" {extra}/>'


# The pilot keeps the same arch and 16 optical apertures as the physical mask.
dots = [(10,23),(19,19),(28,15),(37,13),(46,10),(55,9),(64,7),(73,7),
        (86,7),(95,7),(104,9),(113,10),(122,13),(131,15),(140,19),(149,23)]
hero_defs = '''<defs>
<linearGradient id="portal-hero-metal" x1="0" y1="0" x2="1" y2="1"><stop stop-color="#545963"/><stop offset=".14" stop-color="#171a21"/><stop offset=".6" stop-color="#080a10"/><stop offset="1" stop-color="#404653"/></linearGradient>
<linearGradient id="portal-hero-glass" x1="0" y1="0" x2=".9" y2="1"><stop stop-color="#282d37"/><stop offset=".35" stop-color="#090c12"/><stop offset="1" stop-color="#06080e"/></linearGradient>
<linearGradient id="portal-hero-lcd" x1="0" y1="0" x2=".8" y2="1"><stop stop-color="#a9b9ff"/><stop offset="1" stop-color="#7e94ec"/></linearGradient>
<linearGradient id="portal-hero-sheen" x1="0" y1="0" x2=".5" y2="1"><stop stop-color="#fff" stop-opacity=".14"/><stop offset="1" stop-color="#fff" stop-opacity="0"/></linearGradient>
<radialGradient id="portal-hero-violet"><stop stop-color="#7159ff" stop-opacity=".25"/><stop offset="1" stop-color="#7159ff" stop-opacity="0"/></radialGradient>
<radialGradient id="portal-hero-blue"><stop stop-color="#49a4ff" stop-opacity=".18"/><stop offset="1" stop-color="#49a4ff" stop-opacity="0"/></radialGradient>
<clipPath id="portal-hero-map"><rect x="0" y="0" width="240" height="135" rx="2"/></clipPath>
</defs>'''
hero = [hero_defs,
        '<ellipse cx="380" cy="250" rx="250" ry="135" fill="url(#portal-hero-violet)"/>',
        '<ellipse cx="173" cy="286" rx="180" ry="108" fill="url(#portal-hero-blue)"/>',
        '<ellipse cx="348" cy="358" rx="244" ry="21" fill="#000" opacity=".19"/>',
        '<g transform="translate(250 61) rotate(-7) scale(1.36)">',
        '<rect x="-11" y="-8" width="268" height="163" rx="14" fill="#06070b"/>',
        '<rect x="-16" y="-16" width="268" height="163" rx="14" fill="url(#portal-hero-metal)" stroke="#5c626e" stroke-width=".6"/>',
        '<rect x="-14" y="-14" width="264" height="159" rx="12" fill="url(#portal-hero-glass)"/>',
        '<rect x="-3" y="-3" width="246" height="141" rx="4" fill="#020306" stroke="#444a50" stroke-width=".45"/>',
        '<g clip-path="url(#portal-hero-map)"><rect width="240" height="135" fill="#eae5ce"/>']
# Original synthetic contour map. There is no implied real GPS position.
for cx, cy, rx, ry in [(53,70,47,38),(195,89,53,42),(131,148,48,22)]:
    for ring in range(1,5):
        rr = ring / 4
        points = " ".join(f"{cx+math.cos(i*math.pi/6)*rx*rr*(1+.08*math.sin(i*2)):.1f},{cy+math.sin(i*math.pi/6)*ry*rr:.1f}" for i in range(13))
        hero.append(line(points, "#c5b98e", .65))
hero += [line("-4,116 30,100 65,103 89,82 113,70 143,54 180,58 211,39 248,43", "#f9f5e5", 5),
         line("-4,116 30,100 65,103 89,82 113,70 143,54 180,58 211,39 248,43", "#acb6a2", .7),
         line("76,138 92,112 112,96 120,69 144,50 171,58 190,37", "#bc368d", 2.1),
         line("92,112 112,96 120,69", "#07829b", 1.5),
         '<circle cx="144" cy="50" r="3.2" fill="#f7f0dc" stroke="#bc368d" stroke-width="1.5"/>',
         '<path d="m120 63 5 12-5-3-5 3z" fill="#007b99" stroke="#fcf8e9" stroke-width="1.2"/>',
         '<rect width="240" height="24" fill="#151b23"/>',
         text(8,10,"LOWRANCE",7,"#f6f7fa", 'font-weight="700" letter-spacing="1.5"'),
         text(8,20,"HDS · TRAIL DEMO",4.7,"#cbd0da", 'letter-spacing=".7"'),
         text(224,16,"N",9,"#fff", 'text-anchor="middle"'),
         '<path d="m224 29 3 8h-6z" fill="#475348"/>',
         '<rect y="117" width="240" height="18" fill="#fffdf3"/>',
         text(8,125,"DISTANZA",4,"#6a6c61", 'letter-spacing=".5"'),
         text(8,132,"1.23 mi",6,"#202820", 'font-weight="700"'),
         text(92,125,"PROSSIMO",4,"#6a6c61", 'letter-spacing=".5"'),
         text(92,132,"WAYPOINT 03",6,"#202820", 'font-weight="700"'),
         text(231,128,"DEMO",6,"#a34277", 'text-anchor="end" font-weight="700"'),
         '</g><path d="M-11-11H245V22L-11 85Z" fill="url(#portal-hero-sheen)" opacity=".5"/>',
         '<path d="M-4 144h238" stroke="#798597" stroke-width=".35" opacity=".5"/>',
         '</g>',
         '<g transform="translate(53 209) rotate(5) scale(1.77)">',
         '<rect x="-10" y="-4" width="183" height="105" rx="13" fill="#05070d"/>',
         '<rect x="-14" y="-12" width="184" height="105" rx="13" fill="url(#portal-hero-metal)" stroke="#5b6270" stroke-width=".6"/>',
         '<rect x="-12" y="-10" width="180" height="101" rx="11" fill="url(#portal-hero-glass)"/>',
         '<rect x="-3" y="-3" width="166" height="86" rx="4" fill="#020409"/>',
         '<path d="M5 38Q80 0 155 38V75H5Z" fill="url(#portal-hero-lcd)"/>']
for i, (x, y) in enumerate(dots):
    # A static demo of all 16 apertures: subdued red centre, amber, then green.
    pair = min(i, 15-i)
    color = "#63e697" if pair < 4 else ("#f7c15e" if pair < 6 else "#f16f83")
    hero += [f'<circle cx="{x}" cy="{y}" r="3.3" fill="{color}" opacity=".88"/>',
             f'<circle cx="{x-.65}" cy="{y-.8}" r="1.2" fill="#fff" opacity=".22"/>']
for i in range(34):
    x = 13+i*134/33
    y = 22 + .00295*(x-80)**2
    hero.append(f'<path d="M{x:.1f} {y:.1f}v3" stroke="#263b78" stroke-width="1.4" opacity="{1 if i<21 else .2}"/>')
hero += [text(32,47,"KM/H",4.2,"#293777",'text-anchor="middle" letter-spacing=".6"'),
         text(32,62,"00",16,"#24326c",'text-anchor="middle" font-weight="500" font-style="italic"'),
         text(80,36,"GEAR",4.2,"#293777",'text-anchor="middle" letter-spacing=".6"'),
         '<path d="M72 60V40h2l10 20h2V40" fill="none" stroke="#26336f" stroke-width="2.4" stroke-linejoin="bevel"/>',
         text(125,47,"RUN",4.2,"#293777",'text-anchor="middle" letter-spacing=".6"'),
         text(125,61,"00:00",11,"#24326c",'text-anchor="middle" font-style="italic"'),
         text(32,72,"RPM  0000",4.7,"#293777",'text-anchor="middle"'),
         text(80,72,"DEMO",4.5,"#293777",'text-anchor="middle" letter-spacing="1"'),
         text(126,72,"12.6 V",4.7,"#293777",'text-anchor="middle"'),
         '<path d="M-8-7H163V18L-8 54Z" fill="url(#portal-hero-sheen)" opacity=".4"/>',
         '<path d="M-2 90h157" stroke="#91a8c4" stroke-width=".4" opacity=".6"/>',
         '</g>']
arts = {"cockpit": svg("cockpit", "".join(hero), "0 0 640 400")}

# MIDNIGHT RUN — asymmetric road perspective and luminous horizon.
midnight = '''<defs><linearGradient id="portal-midnight-sky" x2="0" y2="1"><stop stop-color="#101127"/><stop offset=".65" stop-color="#49305f"/><stop offset="1" stop-color="#b64c83"/></linearGradient><linearGradient id="portal-midnight-sun" x2="0" y2="1"><stop stop-color="#ffc5b4"/><stop offset="1" stop-color="#ea5b91"/></linearGradient></defs>
<rect width="240" height="135" fill="url(#portal-midnight-sky)"/><circle cx="151" cy="55" r="25" fill="url(#portal-midnight-sun)"/>
<path d="M0 77 27 49 48 62 68 41 99 68 128 61 159 81 195 56 224 65 240 53V135H0Z" fill="#201a36"/><path d="m0 85 26-12 36 9 23-7 33 8 33-2 35 11 54-12v55H0Z" fill="#10101e"/>
<path d="M141 82c-22 12-32 25-89 53h152c-30-31-54-42-59-53Z" fill="#29243a"/><path d="M141 82c-22 12-32 25-89 53M145 82c5 11 29 22 59 53" fill="none" stroke="#a574d7" stroke-width="1.2"/>
<path d="m143 84-3 4m0 4-3 7m1 6 3 12m4 6 6 12" stroke="#faf0df" stroke-width="2"/>
<path d="m104 116 7-13h30l10 13 2 15h-51z" fill="#a79de8"/><path d="m115 106-5 8h33l-6-8z" fill="#181827"/><path d="M104 121h12m25 0h10" stroke="#ff6f9d" stroke-width="3"/><path d="M107 132h42" stroke="#0c0d18" stroke-width="5"/>
<circle cx="39" cy="24" r=".8" fill="#fff" opacity=".8"/><circle cx="93" cy="15" r=".7" fill="#fff" opacity=".6"/><circle cx="207" cy="30" r="1" fill="#fff" opacity=".7"/>'''
arts["midnight"] = svg("midnight", midnight)

living = '''<defs><radialGradient id="portal-living-glow"><stop stop-color="#293b70"/><stop offset="1" stop-color="#0c1125"/></radialGradient><linearGradient id="portal-living-eyes" x2="0" y2="1"><stop stop-color="#c9ecff"/><stop offset="1" stop-color="#6e91ff"/></linearGradient></defs>
<rect width="240" height="135" fill="url(#portal-living-glow)"/><path d="M39 50c9-9 25-13 40-6m81 0c16-7 32-3 41 6" stroke="#a3c5ff" stroke-width="2" fill="none" opacity=".35"/>
<rect x="49" y="48" width="46" height="45" rx="20" fill="url(#portal-living-eyes)"/><rect x="145" y="48" width="46" height="45" rx="20" fill="url(#portal-living-eyes)"/><ellipse cx="78" cy="70" rx="6" ry="12" fill="#182644"/><ellipse cx="174" cy="70" rx="6" ry="12" fill="#182644"/><circle cx="80" cy="65" r="2" fill="#f2f9ff"/><circle cx="176" cy="65" r="2" fill="#f2f9ff"/>
<path d="M109 96q11 12 22 0" stroke="#b8caff" stroke-width="2.5" fill="none" stroke-linecap="round"/><ellipse cx="43" cy="94" rx="12" ry="4" fill="#cd9aee" opacity=".2"/><ellipse cx="198" cy="94" rx="12" ry="4" fill="#cd9aee" opacity=".2"/>
<path d="m213 23 2 5 5 2-5 2-2 5-2-5-5-2 5-2z" fill="#c2ceff" opacity=".7"/>'''
arts["living"] = svg("living", living)

liquid = '''<defs><linearGradient id="portal-liquid-bg" x2="1" y2="1"><stop stop-color="#0c1930"/><stop offset="1" stop-color="#091c29"/></linearGradient><linearGradient id="portal-liquid-fill" x2=".7" y2="1"><stop stop-color="#6df7e8"/><stop offset=".5" stop-color="#267ab4"/><stop offset="1" stop-color="#304288"/></linearGradient></defs>
<rect width="240" height="135" fill="url(#portal-liquid-bg)"/><path d="M-8 108c35-4 32-64 70-66s36 55 65 48 23-66 55-62 40 68 66 64v46H-8Z" fill="url(#portal-liquid-fill)" opacity=".32"/>
<path d="M-8 108c35-4 32-64 70-66s36 55 65 48 23-66 55-62 40 68 66 64" fill="none" stroke="#7cf0ed" stroke-width="1.8" opacity=".8"/>
<path d="M-3 117c37-4 37-43 65-38s42 39 75 28 38-62 67-43 22 33 40 28v45H-3Z" fill="url(#portal-liquid-fill)" opacity=".45"/><path d="M-3 117c37-4 37-43 65-38s42 39 75 28 38-62 67-43 22 33 40 28" stroke="#7de7eb" stroke-width=".7" fill="none"/>
<circle cx="62" cy="34" r="6" fill="#95fff3" opacity=".7"/><circle cx="84" cy="26" r="2" fill="#95fff3"/><circle cx="175" cy="16" r="2.5" fill="#95fff3" opacity=".5"/><circle cx="21" cy="65" r="2.7" fill="#77e4ee" opacity=".45"/><circle cx="137" cy="57" r="3" fill="#77e4ee" opacity=".7"/><circle cx="202" cy="35" r="2" fill="#77e4ee" opacity=".5"/>'''
arts["liquid"] = svg("liquid", liquid)

garage = '''<defs><radialGradient id="portal-garage-bg"><stop stop-color="#29233b"/><stop offset="1" stop-color="#0e0d1a"/></radialGradient><linearGradient id="portal-garage-wire"><stop stop-color="#dfc3ff"/><stop offset="1" stop-color="#8c87ff"/></linearGradient></defs>
<rect width="240" height="135" fill="url(#portal-garage-bg)"/><g fill="none" stroke="#4d4263" stroke-width=".5" opacity=".5"><path d="m0 109 135-62 105 53M0 132l157-73M51 135 180 72M104 135l99-52M158 135l68-38M12 87l107 48M46 72l140 63M78 57l162 72"/></g>
<g stroke="#a899cb" stroke-width="1.1" fill="#1e1d2d"><path d="M45 83v19l15 9 13-7V85l-15-8zM171 65v19l15 9 13-7V67l-15-8zM153 107v19l15 7 13-9v-17l-15-8zM87 63v19l15 7 13-9V63l-15-8z"/></g>
<g fill="#484057" stroke="url(#portal-garage-wire)" stroke-width="1.5" stroke-linejoin="round"><path d="m57 84 55-31 77 39-24 24-67-7z"/><path d="m81 76 16-35 38-16 36 19 6 40-33 22z" fill="#151623" fill-opacity=".8"/><path d="m97 41 37 19 37-16M134 60l10 46M97 41l-1 43M135 25l-1 35M81 76l53-16M96 84l48 22 33-22" fill="none"/><path d="m144 106 21 10 24-24-12-8z" fill="#676079"/></g>
<path d="m174 95 8-5m-17 18 7-6" stroke="#f1c0ff" stroke-width="3" stroke-linecap="round"/><path d="M103 22v-9m-5 4 5-4 5 4" fill="none" stroke="#cfb6ff" opacity=".65"/>'''
arts["garage"] = svg("garage", garage)

beat = ['''<defs><linearGradient id="portal-beat-bg" x2="1" y2="1"><stop stop-color="#211128"/><stop offset="1" stop-color="#100f21"/></linearGradient><linearGradient id="portal-beat-bars" x2="0" y2="1"><stop stop-color="#f9b6dd"/><stop offset=".45" stop-color="#d16fcb"/><stop offset="1" stop-color="#5750b5"/></linearGradient></defs><rect width="240" height="135" fill="url(#portal-beat-bg)"/><ellipse cx="120" cy="116" rx="95" ry="9" fill="#a150b6" opacity=".1"/>''']
for i, h in enumerate([20,32,52,39,71,90,56,75,47,82,61,95,69,44,32,19]):
    beat.append(f'<rect x="{17+i*13}" y="{115-h}" width="7" height="{h}" rx="3.5" fill="url(#portal-beat-bars)"/>')
beat.append('<path d="M0 72q17-10 30 0t30 0 30 0 30 0 30 0 30 0 30 0 30 0" fill="none" stroke="#f2c5f9" stroke-width=".8" opacity=".4"/>')
arts["beat"] = svg("beat", "".join(beat))

# A self-contained comparison sheet is local working output, not a web dependency.
page = '''<!doctype html><html lang="it"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>stormDASH · illustrazioni del portale</title><style>*{box-sizing:border-box}body{margin:0;padding:40px;background:#0b0b10;color:#f6f5fa;font:14px -apple-system,BlinkMacSystemFont,sans-serif}main{max-width:1000px;margin:auto}h1{font-size:24px;letter-spacing:-1px;margin:0}p{color:#9996a9}.hero{max-width:720px;margin:0 auto 25px}.hero svg{width:100%;display:block}.grid{display:grid;grid-template-columns:repeat(5,1fr);gap:14px}.card{overflow:hidden;border-radius:18px;border:1px solid #ffffff14;background:#17161f}.card svg{width:100%;display:block}.card b{display:block;padding:14px;font-size:11px;letter-spacing:.2px}@media(max-width:600px){body{padding:20px}.grid{grid-template-columns:repeat(2,1fr)}}</style><main><h1>stormDASH</h1><p>Illustrazioni originali · solo anteprima · dati DEMO</p><div class="hero">'''
page += arts["cockpit"] + '</div><div class="grid">'
for name, label in [("midnight","MIDNIGHT RUN"),("living","LIVING COCKPIT"),("liquid","LIQUID REACTOR"),("garage","GARAGE 3D"),("beat","BEAT STORM")]:
    page += f'<div class="card">{arts[name]}<b>{label}</b></div>'
page += '</div></main></html>'
(ROOT / ".tools" / "portal-art.html").write_text(page)
print("\n".join(f"{(OUT / (name+'.svg')).stat().st_size:5} bytes  assets/portal/{name}.svg" for name in arts))
print("Preview: .tools/portal-art.html")
