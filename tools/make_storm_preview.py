#!/usr/bin/env python3
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import subprocess,json
from make_show_preview import mask_pilot,pilot_apertures
root=Path(__file__).resolve().parents[1];out=root/'output/stormDASH';out.mkdir(parents=True,exist_ok=True)
subprocess.run(['c++','-std=c++17','-O2','-I'+str(root/'src'),'-I'+str(root/'include'),str(root/'tools/storm_preview.cpp'),'-o',str(root/'.tools/storm-preview')],check=True)
subprocess.run([str(root/'.tools/storm-preview'),str(root/'evidence/storm-frames')],check=True)
font=lambda n:ImageFont.truetype(str(root/'tools/fonts/BarlowCondensed-Bold.ttf'),n)
for scene,name in enumerate(['PORTAL_LINK','NIGHT_STAGE','PIT_CHECK','TRAIL_BOARD']):
 frames=[]
 for i in range(90 if scene<2 else 1):
  a=Image.open(root/f'evidence/storm-frames/{scene}-{i:03}-a.ppm');b=Image.open(root/f'evidence/storm-frames/{scene}-{i:03}-b.ppm')
  c=Image.new('RGB',(840,1080),'#090914');d=ImageDraw.Draw(c);d.text((42,24),'stormDASH',font=font(66),fill='#f3efff');d.text((44,105),name.replace('_',' ')+' // AFTERDARK',font=font(27),fill='#ad80ff');d.text((45,169),'PILOTA · 160 × 80 · MASCHERA CAD',font=font(24),fill='#b8afc9');c.paste(mask_pilot(a,pilot_apertures(4)),(100,206));d.text((45,552),'COPILOTA · 240 × 135',font=font(24),fill='#b8afc9');c.paste(b.resize((720,405),Image.Resampling.NEAREST),(60,598));d.text((42,1035),'RENDER C++ DEL FIRMWARE · ANTEPRIMA, NON RIPRESA HARDWARE',font=font(21),fill='#b8afc9');frames.append(c)
 frames[len(frames)//2].save(out/f'{name}.png')
 if len(frames)>1:frames[0].save(out/f'{name}.gif',save_all=True,append_images=frames[1:],duration=[round((i+1)*100/15)*10-round(i*100/15)*10 for i in range(len(frames))],loop=0,disposal=2)
(root/'evidence/new-mode-render.json').write_text(json.dumps({'source':'tools/storm_preview.cpp','out_of_bounds_pixels':0,'hardware_capture':False},indent=2))
