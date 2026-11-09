#!/usr/bin/env python3
"""Actual-device core/display test over the documented USB console, not a Wi-Fi test."""
import serial,time,json
from pathlib import Path
root=Path(__file__).resolve().parents[1];log=[];s=serial.Serial('/dev/cu.usbmodem101',115200,timeout=.25)
def read_until(prefix,timeout=3):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  line=s.readline().decode(errors='replace').strip()
  if line:log.append(line)
  if line.startswith(prefix):return line[len(prefix):]
 raise RuntimeError('No response '+prefix)
def state():
 s.write(b'@state\n');return json.loads(read_until('@state '))
def cmd(a,v=0):
 s.write(f'@command {a} {v}\n'.encode());assert read_until('@accepted ')=='1';time.sleep(.08)
def wait(seconds):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  line=s.readline().decode(errors='replace').strip()
  if line:log.append(line)
results={}
try:
 cmd(9);cmd(0);wait(2);x=state();assert x['session'] and x['elapsed']>=1800
 cmd(4);cmd(5);cmd(6);x=state();assert x['gates']==1 and x['penalties']==0
 cmd(1);a=state();wait(1);b=state();assert a['elapsed']==b['elapsed'];cmd(2);wait(1);cmd(3);x=state();assert not x['session'] and x['saved'];results['session']=x
 cmd(8);wait(1);assert state()['mode']==1;wait(5.5);assert state()['mode']==0;results['show']='start and automatic exit'
 cmd(10,42);wait(3);x=state();assert x['paused'];results['controller_loss']=x
 cmd(11,1);wait(46);x=state();assert x['done'] and x['gameMs']==45000;results['game']=x
 cmd(14);wait(2);assert state()['mode']==3;cmd(9);assert state()['mode']==0;results['pit']='enter and restore instruments'
 results['transport']='USB console; firmware executes game and renders actual displays';results['test_record']='The first archived session is an automated commissioning test (1 gate, 0 penalties).'
 print(json.dumps(results,indent=2))
finally:
 s.close();(root/'evidence/runtime-smoke-log.txt').write_text('\n'.join(log));(root/'evidence/runtime-smoke.json').write_text(json.dumps(results,indent=2))
