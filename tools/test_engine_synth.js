#!/usr/bin/env node
'use strict';
// Render actual Web Audio into memory: no loudspeaker, microphone or device.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {chromium,webkit}=require('playwright');
const root=path.resolve(__dirname,'..');
(async()=>{
 const browser=await(process.env.STORM_WEBKIT?webkit.launch({headless:true}):chromium.launch({channel:'chrome',headless:true}));
 try{
  const page=await browser.newPage();await page.setContent('<!doctype html><title>Offline engine render</title>');
  await page.addScriptTag({content:fs.readFileSync(path.join(root,'web/engine_synth.js'),'utf8')});
  const report=await page.evaluate(async()=>{
   const Context=window.OfflineAudioContext||window.webkitOfflineAudioContext;
   if(!Context)throw Error('OfflineAudioContext missing');
   const rate=24000;
   async function render(initial,events=[],duration=2.2){
    const context=new Context(1,Math.ceil(rate*duration),rate),synth=window.StormEngineSynth.create(context);
    synth.start();synth.start();
    if(initial)synth.update(initial,.7);
    const suspensions=events.map(e=>context.suspend(e.at));
    const done=context.startRendering();
    for(let i=0;i<events.length;i++){
     await suspensions[i];const e=events[i];
     if(e.sample)synth.update(e.sample);
     if(e.volume!==undefined)synth.setVolume(e.volume);
     if(e.stop){synth.stop();synth.stop();}
     if(e.invalid){
      for(const bad of [null,{rpm:NaN,gear:1},{rpm:Infinity,gear:1},{rpm:-1,gear:1},{rpm:2000,gear:7}])
       if(synth.update(bad)!==false)throw Error('Malformed sample accepted');
     }
     await context.resume();
    }
    const pcm=(await done).getChannelData(0);synth.stop();
    if(!pcm.every(Number.isFinite))throw Error('Nonfinite audio');
    return pcm;
   }
   function rms(data,start,end){let sum=0,count=0;for(let i=Math.floor(start*rate);i<Math.floor(end*rate);i++){sum+=data[i]*data[i];count++;}return Math.sqrt(sum/count);}
   function power(data,hz,start,end){let re=0,im=0,count=0;for(let i=Math.floor(start*rate);i<Math.floor(end*rate);i++){const phase=2*Math.PI*hz*i/rate;re+=data[i]*Math.cos(phase);im+=data[i]*Math.sin(phase);count++;}return(re*re+im*im)/(count*count);}
   const silence=await render(null);
   const idle=await render({rpm:0,gear:0});
   const high=await render({rpm:6000,gear:3});
   const shifted=await render({rpm:4000,gear:1},[{at:.75,sample:{rpm:4000,gear:1}},{at:1,sample:{rpm:4000,gear:2}}]);
   const steady=await render({rpm:4000,gear:1},[{at:.75,sample:{rpm:4000,gear:1}},{at:1,sample:{rpm:4000,gear:1}}]);
   const reverse=await render({rpm:4000,gear:1},[{at:.75,sample:{rpm:4000,gear:1}},{at:1,sample:{rpm:4000,gear:-1}}]);
   const limiter=await render({rpm:7100,gear:5});
   const below=await render({rpm:7100,gear:4});
   const stale=await render({rpm:6000,gear:3},[{at:1,volume:1,invalid:true}]);
   const stopped=await render({rpm:3000,gear:2},[{at:.8,stop:true}]);
   const muted=await render({rpm:3000,gear:2},[{at:.7,volume:0}]);
   let maxPeak=0;for(const a of [idle,high,shifted,limiter])for(const n of a)maxPeak=Math.max(maxPeak,Math.abs(n));
   return {
    silence:rms(silence,0,2),idle:rms(idle,.65,.8),high:rms(high,.65,.8),maxPeak,
    idleFundamental:power(idle,800/60*4,.6,.85),idleWrongPitch:power(idle,400,.6,.85),
    highFundamental:power(high,400,.6,.85),highWrongPitch:power(high,800/60*4,.6,.85),
    shiftCut:rms(shifted,1.025,1.065),steadyCutWindow:rms(steady,1.025,1.065),
    reverseCutWindow:rms(reverse,1.025,1.065),
    limiter:rms(limiter,.65,.8),unlimited:rms(below,.65,.8),
    staleTail:rms(stale,1.25,2),stopTail:rms(stopped,.85,2),muteTail:rms(muted,.92,1.05),
    recoveredShift:rms(shifted,1.25,1.45),steadyAfterShift:rms(steady,1.25,1.45)
   };
  });
  assert.equal(report.silence,0,'Start without telemetry remains silent');
  assert.ok(report.idle>.001&&report.high>.001,'Idle and acceleration produce audible PCM');
  assert.ok(report.maxPeak<.95,'Headroom prevents sample clipping');
  assert.ok(report.idleFundamental>report.idleWrongPitch*20,'Idle firing frequency matches V8 RPM');
  assert.ok(report.highFundamental>report.highWrongPitch*20,'Acceleration raises firing frequency');
  assert.ok(report.shiftCut<report.steadyCutWindow*.75,'Forward gear change produces a short torque cut');
  assert.ok(Math.abs(report.reverseCutWindow-report.steadyCutWindow)<.00001,'Reverse transition does not invent forward shift');
  assert.ok(report.recoveredShift>report.steadyAfterShift*.8,'Shift recovers normal volume');
  assert.ok(report.limiter<report.unlimited*.9,'Top gear high RPM pulses limiter');
  assert.equal(report.staleTail,0,'Watchdog expires even after volume changes/malformed samples');
  assert.equal(report.stopTail,0,'Idempotent stop disconnects every source');
  assert.ok(report.muteTail<.0001,'Volume zero mutes output');
  console.log('PASS actual OfflineAudioContext: idle/acceleration frequency, startup, clipping headroom, gear cut/recovery, reverse, top-gear limiter, stale-data silence, mute and stop. No physical audio playback.');
  console.log(JSON.stringify(report,null,2));
 }finally{await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1});
