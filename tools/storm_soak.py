#!/usr/bin/env python3
"""Bounded mixed-mode on-device soak over USB. Does not create trail results."""
import serial,time,re,json
from pathlib import Path
root=Path(__file__).resolve().parents[1];s=serial.Serial('/dev/cu.usbmodem101',115200,timeout=.25);start=time.monotonic();lines=[];last=-1
try:
 while time.monotonic()-start<180:
  elapsed=time.monotonic()-start;phase=int(elapsed//15)
  if phase!=last:
   # Instrument / show / pit / game with a physical-controller-equivalent jump.
   action=[9,8,14,10][phase%4];s.write(f'@command {action} 42\n'.encode());last=phase
   if action==10:s.write(b'@command 11 1\n')
  line=s.readline().decode(errors='replace').strip()
  if line:lines.append(line)
 s.write(b'@command 9\n');time.sleep(.1)
finally:s.close()
(root/'evidence/soak-log.txt').write_text('\n'.join(lines))
heap=[int(x) for l in lines for x in re.findall(r'heap=(\d+)',l)];fps=[list(map(int,m)) for l in lines for m in re.findall(r'A=(\d+) B=(\d+) FPS',l)];errors=[l for l in lines if 'panic' in l.lower() or 'FAIL' in l or ('error=' in l and 'error=none' not in l)]
result=dict(duration_seconds=round(time.monotonic()-start),transport='USB',modes=['instruments','show','pit','game'],samples=len(fps),heap_first=heap[0] if heap else None,heap_last=heap[-1] if heap else None,heap_min_observed=min(heap) if heap else None,errors=errors,pass_=bool(heap) and not errors)
(root/'evidence/soak-summary.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
