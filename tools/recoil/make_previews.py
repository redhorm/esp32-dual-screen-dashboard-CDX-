#!/usr/bin/env python3
"""Compile the real C++ renderer and make PNG previews. Requires C++17 + Pillow."""
import argparse, json, subprocess, tempfile
from pathlib import Path
from PIL import Image, ImageDraw
ROOT=Path(__file__).resolve().parents[2]

def main():
    ap=argparse.ArgumentParser();ap.add_argument('output',type=Path);args=ap.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory() as temp:
        exe=Path(temp)/'preview'
        subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-I'+str(ROOT/'src'),str(ROOT/'tools/recoil/preview.cpp'),'-o',str(exe)],check=True)
        subprocess.run([str(exe),str(args.output)],check=True)
    for p in args.output.glob('*.ppm'):
        im=Image.open(p);im.save(p.with_suffix('.png'))
        im.resize((im.width*5,im.height*5),Image.Resampling.NEAREST).save(args.output/(p.stem+'_5x.png'))
    # Orthographic projection through the nominal CAD openings, not a hardware photo.
    d=json.loads((ROOT/'references/recoil/Coordinate_luci_160x80.json').read_text())
    scale=8
    mask=Image.new('L',(160*scale,80*scale));draw=ImageDraw.Draw(mask)
    def xy(x,y):return ((x+10.85)*160*scale/21.7,(5.4-y)*80*scale/10.8)
    draw.polygon([xy(x,y) for x,y in d['main_window_polygon_mm']],fill=255)
    r=d['hole_diameter_mm']/2
    for h in d['holes']:
        x,y=h['x_mm'],h['y_mm'];draw.ellipse([xy(x-r,y+r),xy(x+r,y-r)],fill=255)
    im=Image.open(args.output/'PILOTA_firmware.png').resize(mask.size,Image.Resampling.NEAREST)
    Image.composite(im,Image.new('RGB',mask.size,(24,24,26)),mask).save(args.output/'PILOTA_attraverso_maschera_CAD.png')
if __name__=='__main__':main()
