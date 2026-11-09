#!/usr/bin/env python3
"""Rebuild the delivery manifest and curated source/binary ZIP, no private dump."""
from pathlib import Path
import json,hashlib,zipfile,re,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'output/stormDASH';BIN=OUT/'BINARI';BIN.mkdir(parents=True,exist_ok=True)
subprocess.run([sys.executable,str(ROOT/'tools/build_portal.py'),'--check'],check=True)
version=re.search(r'EXPERIENCE_VERSION = "([^"]+)"', (ROOT/'src/core/experience.h').read_text()).group(1);items=[]
for name,offset in [('bootloader.bin',0),('partitions.bin',0x8000),('firmware.bin',0x10000)]:
 b=(ROOT/'.pio/build/esp32-s3-devkitc-1'/name).read_bytes();(BIN/name).write_bytes(b);items.append(dict(file=name,offset=offset,size=len(b),sha256=hashlib.sha256(b).hexdigest()))
if not 0 < items[-1]['size'] < 0x300000:
 raise ValueError('Application exceeds OTA slot')
if version.encode()+b'\0' not in (BIN/'firmware.bin').read_bytes():
 raise ValueError('Source version does not occur in built firmware; rebuild first')
firmware=(BIN/'firmware.bin').read_bytes()
for name,delimiter in [('storm_page.h','STORM'),('settings_page.h','HTML'),('storm_update_page.h','UPDATE')]:
 page=re.search(r'R"'+delimiter+r'\(([\s\S]*)\)'+delimiter+r'";', (ROOT/'src/services'/name).read_text()).group(1)
 if page.encode() not in firmware:
  raise ValueError(f'{name} differs from the compiled firmware; rebuild first')
manifest=dict(product='stormDASH',version=version,chip='esp32s3',module='ESP32-S3-WROOM-1-N16R8',flash_bytes=16777216,psram_bytes=8388608,configuration='qio_opi / octal PSRAM',files=items,preserves=['nvs','otadata','littlefs'],initial_install_only=True,updates='Use /update with firmware.bin after OTA selection exists',date='2026-10-15')
(BIN/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
files=[]
for folder in ['src','include','assets','references','test','web']:
 files +=[p for p in (ROOT/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts]
for name in ['README.md','START_HERE.md','platformio.ini','partitions.csv','.gitignore','.gitattributes']:
 files.append(ROOT/name)
for name in ['WOW.md','ARCHITECTURE.md','API.md','REQUIREMENTS.md','TESTING.md','MIGRATION.md','REEL.md','RESEARCH.md','RIFERIMENTI_GRAFICA.txt','RECOIL_TFT.txt']:
 files.append(ROOT/'docs'/name)
for p in (ROOT/'tools').rglob('*'):
 if p.is_file() and p.suffix in ['.py','.cpp','.h','.ttf','.txt','.js'] and '__pycache__' not in p.parts and p.name not in ['wifi_hardware_test.py','ota_commission.py','flash_dashboard.py','flash_precompiled.py','test_flash_precompiled.py']:
  files.append(p)
for p in OUT.rglob('*'):
 if p.is_file() and p.suffix in ['.bin','.json','.svg','.png','.gif','.html','.js']:files.append(p)
for name in ['flash-id.txt','device-partitions.json','instrument-pixel-comparison.json','new-mode-render.json','native-tests.txt','portal-tests.txt','flash-helper-tests.txt','cad.txt','build.txt','runtime-first.txt','runtime-reboot.txt','runtime-smoke.json','runtime-smoke-log.txt','ota-rollback.json','ota-final.json','soak-summary.json','soak-log.txt','verification-summary.json','baseline-preservation.json','installed-image-verify.txt','installed-build.json']:
 p=ROOT/'evidence'/name
 if p.exists():files.append(p)
files += [p for p in (ROOT/'evidence').glob('wow-*') if p.is_file() and p.suffix in ['.json','.txt']]
files += [p for p in (ROOT/'evidence').glob('portal-design-*') if p.is_file() and p.suffix in ['.json','.txt']]
files.append(ROOT/'evidence/advanced-browser.json')
files.append(ROOT/'evidence/installed-7.1.0-rc.2.json')
zip_path=OUT/f'stormDASH-{version}.zip'
with zipfile.ZipFile(zip_path,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for p in sorted(set(files)):
  if p.exists():
   arcname=str(Path('stormDASH')/p.relative_to(ROOT))
   if p.name in ['ota-rollback.json','ota-final.json']:
    report=json.loads(p.read_text());report.pop('restore',None)
    z.writestr(arcname,json.dumps(report,indent=2)+'\n')
   else:z.write(p,arcname)
check=zipfile.ZipFile(zip_path)
if check.testzip() is not None:
 raise RuntimeError('ZIP integrity check failed')
(OUT/'PACKAGE_SHA256.txt').write_text(hashlib.sha256(zip_path.read_bytes()).hexdigest()+'  '+zip_path.name+'\n')
print(f'{zip_path}\n{zip_path.stat().st_size} bytes; {len(check.namelist())} entries; ZIP integrity PASS')
