#!/usr/bin/env node
'use strict';
// Full portal + real controller, with recorded Web Audio and local HTTP fixtures.
// DSP waveform checks live separately in test_engine_synth.js.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {chromium,webkit}=require('playwright');
const root=path.resolve(__dirname,'..');
const html=fs.readFileSync(path.join(root,'output/stormDASH/portal.html'),'utf8');
const tests=[];const test=(name,run)=>tests.push({name,run});
let browser;
async function fixture(){
 const page=await browser.newPage({viewport:{width:390,height:844}});
 const errors=[],calls=[];page.on('pageerror',e=>errors.push(String(e)));
 let clock=1000,inflight=0,maxInFlight=0;
 const state={mode:0,session:false,running:false,elapsed:0,gates:0,penalties:0,score:0,weight:7,gameMs:0,hits:0,best:0,history:0,done:false,wow:{ms:0,elapsed:0,score:0,best:0,hits:0,seed:1,paused:false,done:false,audioActive:false,exploded:0,bands:[0,0,0,0]}};
 const telemetry={ok:true,nowMs:1000,telemetrySource:'esc_pwm_scenic',escSignalValid:true,vehicle:{rpm:800,gear:0}};
 const control={frozen:false,fail:false,badJson:false,hold:null};
 await page.addInitScript(()=>{
  window.__audio={contexts:[],events:[],resumeHeld:false,resumeFail:false};
  class Context{
   constructor(){this.state='suspended';this.currentTime=0;this.destination={};window.__audio.contexts.push(this)}
   resume(){window.__audio.events.push({kind:'resume',gesture:navigator.userActivation?.isActive});if(window.__audio.resumeFail)return Promise.reject(Error('resume failed'));if(window.__audio.resumeHeld)return new Promise(resolve=>{this.release=()=>{this.state='running';resolve()}});this.state='running';return Promise.resolve()}
   close(){this.state='closed';window.__audio.events.push({kind:'close'});return Promise.resolve()}
  }
  window.AudioContext=window.webkitAudioContext=Context;
 });
 await page.route('http://127.0.0.1/**',async route=>{
  const req=route.request(),p=new URL(req.url()).pathname;
  if(p==='/')return route.fulfill({contentType:'text/html',body:html});
  calls.push({path:p,method:req.method()});inflight++;maxInFlight=Math.max(maxInFlight,inflight);
  try{
   await new Promise(r=>setTimeout(r,12));
   if(control.hold&&p===control.hold.path)await control.hold.promise;
   if(p==='/api/v4/state'){
    if(control.fail)return await route.abort('failed');
    if(control.badJson)return await route.fulfill({contentType:'application/json',body:'{broken'});
    if(!control.frozen)telemetry.nowMs=clock+=200;
    return await route.fulfill({json:telemetry});
   }
   let data={ok:true};
   if(p==='/api/storm')data=state;
   else if(p==='/api/settings')data={driverBrightness:220,navigatorBrightness:220,language:0,useKmh:true,bootImageEnabled:true,bootHoldMs:1800};
   else if(p==='/api/v3/state')data={identity:{vehicle:'RECOIL',number:'4400',driver:'LUCA'}};
   else if(p==='/api/esc/state'||p==='/api/lights/state')data={ok:true,escValid:true,escEnabled:true,escPulse:1500,escRawPulse:1500,escAgeMs:12,escState:'NEUTRAL',escType:'crawler_forward_reverse',escNeutralUs:1500,escDeadbandUs:45,escReverseConfirmMs:220,profile:'ultra4_4400',brightness:96,pixels:[],hardwareReady:true};
   else if(p==='/api/copilot-display')data={profile:'240x135_landscape',logicalWidth:240,logicalHeight:135};
   else if(p==='/api/show')data={active:false,elapsedMs:0};
   return await route.fulfill({json:data});
  }finally{inflight--}
 });
 await page.goto('http://127.0.0.1/');
 await page.waitForFunction(()=>document.getElementById('status').dataset.online==='true');
 await page.locator('[data-tab="pit"]').click();
 await page.waitForFunction(()=>document.getElementById('pit-connection').textContent==='DATI AGGIORNATI');
 await page.evaluate(()=>{
  window.StormEngineSynth={create(context){const events=window.__audio.events;events.push({kind:'create'});return{
   start(){events.push({kind:'start'})},
   update(sample,volume){events.push({kind:'update',sample,volume})},
   setVolume(volume){events.push({kind:'volume',volume})},
   stop(){events.push({kind:'stop'})}
  }}};
 });
 const f={page,errors,calls,state,telemetry,control,maxInFlight:()=>maxInFlight,
  events:()=>page.evaluate(()=>window.__audio.events),
  active:()=>page.evaluate(()=>window.stormEngineAudio.isActive()),
  start:async()=>{await page.locator('#engine-start').click();await page.waitForFunction(()=>window.__audio.events.some(e=>e.kind==='update'))},
  stopped:async()=>{await page.waitForFunction(()=>!window.stormEngineAudio.isActive());await page.waitForFunction(()=>window.__audio.contexts.every(c=>c.state==='closed'))},
  close:async()=>{assert.deepEqual(errors,[]);await page.close()}
 };
 return f;
}
async function withFixture(run){const f=await fixture();try{await run(f)}finally{await f.close()}}
const updates=events=>events.filter(e=>e.kind==='update');

test('No autoplay; explicit gesture starts engine; RPM, reverse, volume and stop are read-only',()=>withFixture(async f=>{
 assert.equal(await f.active(),false);assert.equal((await f.events()).length,0);
 assert.equal(await f.page.locator('#engine-volume').inputValue(),'25');
 await f.start();assert.ok((await f.events()).find(e=>e.kind==='resume').gesture);
 assert.equal(updates(await f.events())[0].sample.rpm,800);
 f.telemetry.vehicle={rpm:4600,gear:3};await f.page.waitForFunction(()=>window.__audio.events.some(e=>e.kind==='update'&&e.sample.rpm===4600&&e.sample.gear===3));
 if(process.env.STORM_AUDIO_SCREENSHOTS){
  const dir=path.resolve(process.env.STORM_AUDIO_SCREENSHOTS);fs.mkdirSync(dir,{recursive:true});
  for(const theme of ['dark','light']){
   if(await f.page.locator('body').getAttribute('data-theme')!==theme)await f.page.locator('#themeToggle').click();
   await f.page.locator('#engine-panel').screenshot({path:path.join(dir,'engine-'+theme+'.png')});
  }
 }
 f.telemetry.vehicle={rpm:2200,gear:-1};await f.page.waitForFunction(()=>window.__audio.events.some(e=>e.kind==='update'&&e.sample.gear===-1));
 await f.page.locator('#engine-volume').fill('0');await f.page.locator('#engine-volume').dispatchEvent('input');
 await f.page.waitForFunction(()=>window.__audio.events.some(e=>e.kind==='volume'&&e.volume===0));
 await f.page.locator('#engine-stop').click();await f.stopped();
 assert.equal(f.calls.filter(c=>c.method==='POST').length,0);
 assert.equal(f.maxInFlight(),1);
}));

test('Invalid, absent or demo telemetry stops sound and cannot auto-restart',async()=>{
 for(const invalid of [{escSignalValid:false},{telemetrySource:'demo'},{vehicle:{rpm:null,gear:1}},{vehicle:{rpm:8100,gear:1}},{vehicle:{rpm:1000,gear:6}},{vehicle:{rpm:1000,gear:1.5}}])await withFixture(async f=>{
  await f.start();Object.assign(f.telemetry,invalid);await f.stopped();
  Object.assign(f.telemetry,{telemetrySource:'esc_pwm_scenic',escSignalValid:true,vehicle:{rpm:800,gear:0}});
  const count=(await f.events()).filter(e=>e.kind==='create').length;await f.page.waitForTimeout(1300);
  assert.equal(await f.active(),false);assert.equal((await f.events()).filter(e=>e.kind==='create').length,count);
 });
});

test('Frozen timestamps, invalid JSON and network failure terminate playback',async()=>{
 for(const kind of ['frozen','badJson','fail'])await withFixture(async f=>{await f.start();f.control[kind]=true;await f.stopped();assert.equal(await f.active(),false)});
});

test('Tab exit, page hiding and changing instrument mode release audio',async()=>{
 for(const kind of ['tab','hidden','pagehide','mode','offline'])await withFixture(async f=>{
  await f.start();
  if(kind==='tab')await f.page.locator('[data-tab="wow"]').click();
  if(kind==='hidden')await f.page.evaluate(()=>{Object.defineProperty(document,'hidden',{configurable:true,value:true});document.dispatchEvent(new Event('visibilitychange'))});
  if(kind==='pagehide')await f.page.evaluate(()=>window.dispatchEvent(new Event('pagehide')));
  if(kind==='mode'){f.state.mode=9;await f.page.evaluate(()=>poll())}
  if(kind==='offline')await f.page.evaluate(()=>connected(false));
  await f.stopped();
  await f.page.evaluate(()=>{Object.defineProperty(document,'hidden',{configurable:true,value:false});document.dispatchEvent(new Event('visibilitychange'))});
  await f.page.waitForTimeout(300);assert.equal(await f.active(),false);
 });
});

test('Stop during deferred browser resume cannot resurrect the old audio session',()=>withFixture(async f=>{
 await f.page.evaluate(()=>window.__audio.resumeHeld=true);
 await f.page.locator('#engine-start').click();
 await f.page.waitForFunction(()=>window.__audio.contexts.length===1);
 await f.page.locator('#engine-stop').click();
 await f.page.evaluate(()=>window.__audio.contexts[0].release());await f.page.waitForTimeout(250);
 assert.equal(await f.active(),false);assert.equal(updates(await f.events()).length,0);
 assert.ok((await f.events()).some(e=>e.kind==='close'));
}));

test('Stop during pending telemetry discards late data and permits a deliberate restart',()=>withFixture(async f=>{
 let release;f.control.hold={path:'/api/v4/state',promise:new Promise(r=>release=r)};
 await f.page.locator('#engine-start').click();await f.page.waitForFunction(()=>window.__audio.contexts.length===1);
 await f.page.locator('#engine-stop').click();f.control.hold=null;release();await f.page.waitForTimeout(300);
 assert.equal(await f.active(),false);assert.equal(updates(await f.events()).length,0);
 await f.start();assert.equal(await f.active(),true);await f.page.locator('#engine-stop').click();await f.stopped();
}));

test('Pending transport times out without leaving audio or replaying old data',()=>withFixture(async f=>{
 await f.start();let release;f.control.hold={path:'/api/v4/state',promise:new Promise(r=>release=r)};
 await f.stopped();f.control.hold=null;release();await f.page.waitForTimeout(300);assert.equal(await f.active(),false);
}));

test('A blocked shared HTTP lane expires audio reads without replay after release',()=>withFixture(async f=>{
 await f.start();let release;
 f.control.hold={path:'/api/storm/results',promise:new Promise(r=>release=r)};
 await f.page.evaluate(()=>{window.__blocked=window.stormRequest('/api/storm/results',{},5000).catch(()=>{})});
 await f.page.waitForTimeout(50);
 const before=f.calls.filter(c=>c.path==='/api/v4/state').length;
 await f.stopped();f.control.hold=null;release();await f.page.evaluate(()=>window.__blocked);await f.page.waitForTimeout(100);
 assert.equal(await f.active(),false);
 // The audio service never revives from a queued response; unrelated Pit polls
 // may still legitimately read the same endpoint after the lane is released.
 assert.ok(f.calls.filter(c=>c.path==='/api/v4/state').length>=before);
 const stopped=(await f.events()).findLastIndex(e=>e.kind==='stop');
 assert.equal((await f.events()).slice(stopped+1).filter(e=>e.kind==='update').length,0);
}));

test('Unavailable or rejected browser audio reports an actionable stopped state',async()=>{
 for(const kind of ['missing','reject'])await withFixture(async f=>{
  await f.page.evaluate(kind=>{if(kind==='missing'){window.AudioContext=undefined;window.webkitAudioContext=undefined}else window.__audio.resumeFail=true},kind);
  await f.page.locator('#engine-start').click();await f.page.waitForTimeout(100);
  assert.equal(await f.active(),false);assert.equal(updates(await f.events()).length,0);
  assert.ok((await f.page.locator('#engine-status').textContent()).length>0);
 });
});

(async()=>{
 browser=process.env.STORM_WEBKIT?await webkit.launch({headless:true}):await chromium.launch({channel:'chrome',headless:true});
 try{for(const {name,run} of tests){await run();console.log('PASS '+name)}console.log(`${tests.length} engine audio integration groups passed; HTTP/Web Audio fixtures only, no receiver or motor commands.`)}finally{await browser.close()}
})().catch(e=>{console.error(e);process.exitCode=1});
