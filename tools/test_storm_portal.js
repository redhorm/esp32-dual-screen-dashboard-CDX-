#!/usr/bin/env node
// Pit controls are covered by test_unified_portal.js against the complete embedded HTML.
'use strict';
// Runs the actual embedded portal against a bounded DOM/API fixture. No ESP32,
// Wi-Fi changes, network calls, or generated session records are involved.
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const path=require('node:path');
const root=path.resolve(__dirname,'..');
const source=fs.readFileSync(path.join(root,'src/services/storm_page.h'),'utf8');
const html=source.match(/R"STORM\(([\s\S]*)\)STORM";/)[1];
const script=html.match(/<script>([\s\S]*?)<\/script>/)[1];
class Element{
  constructor(tag='div',attrs={}){this.tagName=tag;this.attrs=attrs;this.id=attrs.id;this.value=attrs.value||'';this.dataset={};this.children=[];this.disabled=false;this.checked=false;this.textContent='';this.className=attrs.class||'';this.focused=false;this.style={};this.files=[];for(const [k,v] of Object.entries(attrs))if(k.startsWith('data-'))this.dataset[k.slice(5)]=v;this.classes=new Set(this.className.split(' '));this.classList={toggle:(key,force)=>{const next=force===undefined?!this.classes.has(key):force;next?this.classes.add(key):this.classes.delete(key)},contains:key=>this.classes.has(key),add:(...keys)=>keys.forEach(key=>this.classes.add(key)),remove:(...keys)=>keys.forEach(key=>this.classes.delete(key))}}
  setAttribute(k,v){this.attrs[k]=v}
  getAttribute(k){return this.attrs[k]??null}
  removeAttribute(k){delete this.attrs[k]}
  setPointerCapture(id){this.pointerCapture=id}
  getBoundingClientRect(){return{left:10,top:20,width:200,height:100}}
  append(...items){this.children.push(...items)}
  appendChild(item){this.append(item)}
  replaceChildren(...items){this.children=[...items]}
  focus(){this.focused=true}
  click(){if(this.onclick)return this.onclick()}
  remove(){}
}
const defaultWow=()=>({ms:0,elapsed:0,score:0,best:0,hits:0,paused:false,done:false,audioActive:false,seed:1,exploded:0,bands:[0,0,0,0]});
const defaultState=()=>({mode:0,session:false,running:false,elapsed:0,gates:0,penalties:0,score:0,weight:7,gameMs:0,hits:0,paused:false,done:false,best:0,history:0,saved:false,wow:defaultWow()});
async function fixture(){
  const elements=[];const byId={};
  for(const tag of html.matchAll(/<(\w+)\b([^>]*)>/g)){const attrs={};for(const attr of tag[2].matchAll(/([\w-]+)="([^"]*)"/g))attrs[attr[1]]=attr[2];const element=new Element(tag[1],attrs);elements.push(element);if(element.id)byId[element.id]=element}
  const calls=[],beacons=[],canvases=[],audios=[],audioContexts=[],revoked=[],timers=new Map(),listeners={};let nextTimer=1;
  const api={state:defaultState(),results:[],lights:{escValid:true,escPulse:1500,escState:'neutral',escType:'forward_brake_reverse',escNeutralUs:1500,escDeadbandUs:45,brightness:96,profile:'ultra4_4400',escEnabled:false,escReverseConfirmMs:880},fail:null,hold:null,audioFail:false,audioResumeFail:false,audioPlayHold:null,audioValues:null};
  const document={hidden:false,activeElement:null,body:new Element('body'),getElementById:id=>{assert.ok(byId[id],`Missing element ${id}`);return byId[id]},querySelectorAll:selector=>elements.filter(e=>selector.split(',').some(part=>part.startsWith('#')?e.id===part.slice(1):part.startsWith('[')?Object.hasOwn(e.attrs,part.slice(1,-1)):e.tagName===part)),addEventListener:(name,fn)=>{listeners[name]=fn},createElement:tag=>{
    const e=new Element(tag);if(tag==='canvas'){const texts=[];e.getContext=()=>({fillStyle:'',font:'',fillRect(){},fillText(text){texts.push(String(text))},measureText(text){return{width:text.length*35}}});e.toBlob=fn=>fn({png:true});canvases.push(texts)}if(tag==='audio'){e.paused=true;e.play=()=>{e.paused=false;if(api.audioFail)return Promise.reject(Error('unsupported'));return api.audioPlayHold||Promise.resolve()};e.pause=()=>{e.paused=true};e.load=()=>{e.loaded=true};audios.push(e)}return e;
  }};
  class FakeAudioContext{
    constructor(){this.state='suspended';this.sampleRate=48000;this.destination={};this.closed=false;audioContexts.push(this)}
    createAnalyser(){return{fftSize:512,frequencyBinCount:256,smoothingTimeConstant:0,connect(){},disconnect(){this.disconnected=true},getByteFrequencyData(buffer){for(let i=0;i<buffer.length;i++)buffer[i]=api.audioValues?api.audioValues[i]||0:128}}}
    createMediaElementSource(){return{connect(){},disconnect(){this.disconnected=true}}}
    resume(){if(api.audioResumeFail)return Promise.reject(Error('suspended'));this.state='running';return Promise.resolve()}
    close(){this.state='closed';this.closed=true;return Promise.resolve()}
  }
  const context=vm.createContext({document,window:{scrollTo(){},AudioContext:FakeAudioContext,addEventListener:(name,fn)=>{listeners[name]=fn}},navigator:{sendBeacon:(url,body)=>{beacons.push({url,action:body.get('action'),value:body.get('value')});return true}},URLSearchParams,URL:{createObjectURL:()=> 'blob:fixture',revokeObjectURL:url=>revoked.push(url)},setTimeout:(fn,delay)=>{const id=nextTimer++;timers.set(id,{fn,delay});return id},clearTimeout:id=>timers.delete(id),console,fetch:async(url,options={})=>{
    const method=options.method||'GET',body=options.body;calls.push({url,method,body});if(api.hold&&url===api.hold.path&&method===(api.hold.method||'GET'))await api.hold.promise;
    if(api.fail&&(!api.fail.path||api.fail.path===url)){if(api.fail.status)return{ok:false,status:api.fail.status,json:async()=>({error:'fixture'})};throw Error('fixture network failure')}
    let result;if(method==='POST')result={accepted:true};else if(url==='/api/storm')result=api.state;else if(url==='/api/storm/results')result=api.results;else if(url==='/api/v3/rtl/state')result=api.lights;else result={fixture:true};return{ok:true,status:200,json:async()=>JSON.parse(JSON.stringify(result))};
  }});
  vm.runInContext(script,context,{filename:'storm_page.h embedded JavaScript'});
  const settle=async()=>{for(let i=0;i<8;i++)await new Promise(resolve=>setImmediate(resolve))};
  await settle();
  return {api,document,$:id=>byId[id],elements,calls,beacons,canvases,audios,audioContexts,revoked,timers,listeners,context,settle,run:s=>vm.runInContext(s,context)};
}
const tests=[];
function test(name,fn){tests.push({name,fn})}

test('Embedded source and deliverable previews are identical; no CDN',async()=>{
  assert.equal(fs.readFileSync(path.join(root,'output/stormDASH/portal.html'),'utf8'),html);
  assert.equal(fs.readFileSync(path.join(root,'output/stormDASH/portal.js'),'utf8'),script);
  assert.ok(!/<(?:script|link)[^>]+(?:src|href)="https?:/.test(html));
});
test('Initial connection loads saved rules and never invents a card',async()=>{const f=await fixture();assert.equal(f.$('start').disabled,false);assert.equal(f.$('undo').disabled,true);assert.equal(f.$('jump').disabled,true);assert.equal(f.$('weight').value,7);await f.$('card').click();assert.equal(f.canvases.length,0);assert.match(f.$('message').textContent,/Non ci sono ancora risultati/)});
test('Session controls follow state, undo and event capacity',async()=>{const f=await fixture();Object.assign(f.api.state,{session:true,running:true});await f.run('poll()');assert.equal(f.$('gate').disabled,false);assert.equal(f.$('undo').disabled,true);assert.equal(f.$('rules').disabled,true);assert.equal(f.$('pause').classList.contains('hidden'),false);Object.assign(f.api.state,{running:false,gates:128});await f.run('poll()');assert.equal(f.$('gate').disabled,true);assert.equal(f.$('penalty').disabled,true);assert.equal(f.$('undo').disabled,false);assert.equal(f.$('resume').classList.contains('hidden'),false);assert.equal(f.run('formatTime(86400000)'),'24:00:00')});
test('Integer input bounds reject invalid seeds and weights before sending',async()=>{const f=await fixture();const posts=()=>f.calls.filter(c=>c.method==='POST').length;let before=posts();for(const input of ['0','4294967296','1e2','1.5','']){f.$('seed').value=input;f.$('game').click()}f.$('weight').value='-1';await f.$('rules').click();assert.equal(posts(),before);f.$('seed').value='4294967295';f.$('game').click();await f.settle();assert.equal(posts(),before+1);assert.equal(f.calls.find(c=>c.method==='POST').body.get('value'),'4294967295')});
test('Card uses immutable closed result, original weight and real origin',async()=>{const f=await fixture();Object.assign(f.api.state,{elapsed:0,score:999,weight:99});f.api.results=[{duration:61000,gates:2,penalties:1,score:3,penaltyWeight:5},{duration:1,gates:99,penalties:99,score:999,test:true}];await f.$('card').click();assert.equal(f.canvases.length,1);assert.ok(f.canvases[0].includes('01:01'));assert.ok(f.canvases[0].includes('3 punti'));assert.ok(f.canvases[0].includes('Peso penalità 5'));assert.ok(!f.canvases[0].includes('999 punti'));assert.ok(f.canvases[0].includes('Risultati inseriti manualmente'))});
test('Test-only archive is never exported as a real outing',async()=>{const f=await fixture();f.api.results=[{duration:1250,gates:1,penalties:0,score:-1,penaltyWeight:5,test:true}];await f.$('card').click();assert.equal(f.canvases.length,0);assert.match(f.$('message').textContent,/solo collaudi/);await f.$('historyButton').click();const item=f.$('results').children[0];assert.match(item.children[0].textContent,/COLLAUDO/);item.children[1].click();assert.ok(f.canvases[0].includes('COLLAUDO · TRAIL SESSION'));assert.ok(f.canvases[0].includes('Dati di collaudo · non è un’uscita reale'))});
test('Malformed archive cannot become an exported result',async()=>{const f=await fixture();f.api.results=[{duration:2000,gates:-1,penalties:0,score:0}];await f.$('card').click();assert.equal(f.canvases.length,0);assert.match(f.$('message').textContent,/nessuna card/)});
test('Network loss freezes controls and discloses stale displayed values',async()=>{const f=await fixture();f.api.fail={path:'/api/storm'};await f.run('poll()');assert.equal(f.$('start').disabled,true);assert.equal(f.$('networkHelp').classList.contains('hidden'),false);assert.match(f.$('status').textContent,/Offline|assente/);f.api.fail=null;await f.run('poll()');assert.equal(f.$('start').disabled,false);assert.equal(f.$('networkHelp').classList.contains('hidden'),true)});
test('Concurrent poll requests share a single HTTP request',async()=>{const f=await fixture();let release;f.api.hold={path:'/api/storm',promise:new Promise(resolve=>{release=resolve})};const before=f.calls.length;const one=f.run('poll()'),two=f.run('poll()');assert.equal(one,two);assert.equal(f.calls.length,before+1);release();await one;f.api.hold=null});
test('Controller heartbeat is foreground-only; tab/page leaving pauses game',async()=>{const f=await fixture();f.api.state.mode=2;await f.run('poll()');await f.run('cycle()');assert.equal(f.calls.filter(c=>c.body&&c.body.get('action')==='heartbeat').length,0);f.elements.find(e=>e.dataset.tab==='arcade').click();await f.run('cycle()');assert.equal(f.calls.filter(c=>c.body&&c.body.get('action')==='heartbeat').length,1);f.elements.find(e=>e.dataset.tab==='trail').click();assert.equal(f.beacons.at(-1).action,'background');f.document.hidden=true;f.listeners.visibilitychange();assert.equal(f.beacons.length,2);const before=f.calls.length;await f.run('cycle()');assert.equal(f.calls.length,before)});
test('Jump sends immediately without waiting for a state read',async()=>{const f=await fixture();f.api.state.mode=2;await f.run('poll()');const before=f.calls.length;await f.$('jump').click();assert.equal(f.calls.length,before+1);assert.equal(f.calls.at(-1).body.get('action'),'jump');assert.equal(f.$('jump').disabled,false)});
test('HTTP rejection is distinct from disconnection',async()=>{const f=await fixture();f.api.fail={path:'/api/storm',status:400};await f.run("send('start')");assert.match(f.$('message').textContent,/Valore non valido/);assert.equal(f.run('online'),true)});
test('Request timeout releases pending commands without automatic replay',async()=>{const f=await fixture();f.context.fetch=()=>new Promise(()=>{});const pending=f.run("send('start')");const timer=[...f.timers.values()].find(t=>t.delay===2500);assert.ok(timer);timer.fn();await pending;assert.equal(f.run('busy'),false);assert.equal(f.run('online'),false);assert.match(f.$('message').textContent,/verifica lo stato prima di ripetere/)});
const pointer=(x=110,y=70,id=1)=>({clientX:x,clientY:y,pointerId:id,preventDefault(){}});
const key=k=>({key:k,preventDefault(){}});
const posts=f=>f.calls.filter(c=>c.method==='POST'&&c.url==='/api/storm');
async function activate(f,mode){f.elements.find(e=>e.dataset.tab==='wow').click();f.api.state.mode=mode;await f.run('poll()')}
async function selectAudio(f,file={name:'local.mp3',type:'audio/mpeg',size:1048576}){f.$('beatFile').files=[file];f.$('beatFile').onchange();await f.settle()}

test('WOW validates five mode IDs and bounded spectral payloads',async()=>{const f=await fixture();for(let mode=5;mode<=9;mode++){f.api.state.mode=mode;await f.run('poll()');assert.equal(f.run('online'),true);assert.equal(f.$(['midnight','living','liquid','garage','beat'][mode-5]+'Panel').classList.contains('hidden'),false)}for(const invalid of [{bands:[0,0,256,0]},{bands:[0,0,0]},{paused:1},{elapsed:60001},{exploded:1.01}]){Object.assign(f.api.state.wow,invalid);await f.run('poll()');assert.equal(f.run('online'),false);f.api.state.wow=defaultWow()}f.api.state.wow.exploded=.5;await f.run('poll()');assert.equal(f.run('online'),true);f.api.state.mode=10;await f.run('poll()');assert.equal(f.run('online'),false)});
test('All five WOW selectors send the intended mode; Midnight seed is bounded',async()=>{const f=await fixture();await activate(f,5);f.$('midnightSeed').value='0';await f.$('midnightStart').click();assert.equal(posts(f).length,0);f.$('midnightSeed').value='4294967295';for(const name of ['midnight','living','liquid','garage','beat']){await f.elements.find(e=>e.dataset.wow===name).click();assert.equal(posts(f).at(-1).body.get('action'),name)}assert.equal(posts(f)[0].body.get('value'),'4294967295')});
test('Steering holds refresh at the bounded cadence and cancel releases neutral',async()=>{const f=await fixture();await activate(f,5);f.$('steerLeft').onpointerdown(pointer());for(let i=0;i<20;i++)f.$('steerLeft').onpointerdown(pointer());assert.equal(posts(f).length,0);f.run('wowTick()');await f.settle();assert.equal(posts(f).length,1);assert.equal(posts(f)[0].body.get('value'),'0');f.run('wowTick()');await f.settle();assert.equal(posts(f).length,2);f.$('steerLeft').onpointercancel(pointer());f.run('wowTick()');await f.settle();assert.equal(posts(f).at(-1).body.get('value'),'1');const before=posts(f).length;f.run('wowTick()');await f.settle();assert.equal(posts(f).length,before);assert.equal(f.$('steerLeft').attrs['aria-pressed'],'false')});
test('Steering keyboard, focus loss and paused state cannot stick',async()=>{const f=await fixture();await activate(f,5);f.$('steerRight').onkeydown(key('Enter'));f.run('wowTick()');await f.settle();assert.equal(posts(f).at(-1).body.get('value'),'2');f.$('steerRight').onblur();f.run('wowTick()');await f.settle();assert.equal(posts(f).at(-1).body.get('value'),'1');f.api.state.wow.paused=true;await f.run('poll()');assert.equal(f.$('steerLeft').disabled,true);assert.equal(f.$('midnightResume').disabled,false);await f.$('midnightResume').click();assert.equal(posts(f).at(-1).body.get('action'),'heartbeat');assert.equal(posts(f).at(-1).body.get('value'),'1')});
test('WOW heartbeat follows visibility and explicit Midnight resume',async()=>{const f=await fixture();await activate(f,5);await f.run('cycle()');assert.equal(posts(f).filter(c=>c.body.get('action')==='heartbeat').length,1);f.api.state.wow.paused=true;await f.run('cycle()');assert.equal(posts(f).filter(c=>c.body.get('action')==='heartbeat').length,1);f.elements.find(e=>e.dataset.tab==='trail').click();assert.equal(f.beacons.at(-1).action,'background');f.document.hidden=true;f.listeners.visibilitychange();const before=f.calls.length;f.run('wowTick()');await f.run('cycle()');assert.equal(f.calls.length,before)});
test('Touch pads clamp pointer bounds and provide keyboard controls',async()=>{const f=await fixture();for(const [mode,id,action] of [[6,'livingPad','touch'],[7,'liquidPad','touch'],[8,'garagePad','orbit']]){await activate(f,mode);f.$(id).onpointerdown(pointer(-200,500));f.run('wowTick()');await f.settle();assert.equal(posts(f).at(-1).body.get('action'),action);assert.equal(posts(f).at(-1).body.get('value'),'255');f.$(id).onpointermove(pointer(999,-300));f.run('wowTick()');await f.settle();assert.equal(posts(f).at(-1).body.get('value'),'65280');f.$(id).onpointerup(pointer());f.$(id).onkeydown(key('Home'));f.run('wowTick()');await f.settle();assert.equal(posts(f).at(-1).body.get('value'),'32896')}assert.equal(f.run("controlValid('orbit',65536)"),false);assert.equal(f.run("controlValid('touch',65535)"),false)});
test('One in-flight control coalesces movement to the latest gesture',async()=>{const f=await fixture();await activate(f,6);let release;f.api.hold={path:'/api/storm',method:'POST',promise:new Promise(resolve=>{release=resolve})};f.$('livingPad').onpointerdown(pointer(10,20));f.run('wowTick()');for(let i=0;i<20;i++){f.$('livingPad').onpointermove(pointer(210,120));f.run('wowTick()')}assert.equal(posts(f).length,1);assert.equal(f.run('controlBusy'),true);release();await f.settle();f.api.hold=null;f.run('wowTick()');await f.settle();assert.equal(posts(f).length,2);assert.equal(posts(f).at(-1).body.get('value'),'65535')});
test('Control timeout discards pending movement and never replays it',async()=>{const f=await fixture();await activate(f,6);f.context.fetch=()=>new Promise(()=>{});f.$('livingPad').onpointerdown(pointer());f.run('wowTick()');f.$('livingPad').onpointermove(pointer(200,100));const timer=[...f.timers.values()].find(t=>t.delay===2500);timer.fn();await f.settle();assert.equal(f.run('online'),false);assert.equal(f.run('controlBusy'),false);assert.equal(f.run('controlPending'),null);f.run('wowTick()');assert.equal(f.run('controlPending'),null)});
test('Audio files stay local and reject empty, oversized and unrelated files',async()=>{const f=await fixture();await activate(f,9);for(const file of [{name:'a.mp3',type:'audio/mpeg',size:0},{name:'a.mp3',type:'audio/mpeg',size:30*1024*1024+1},{name:'a.pdf',type:'application/pdf',size:100}]){await selectAudio(f,file);assert.equal(f.run('beatFile'),null);assert.equal(f.$('beatPlay').disabled,true)}await selectAudio(f);assert.equal(f.$('beatPlay').disabled,false);assert.equal(f.audios.length,0);assert.ok(f.calls.every(c=>!(c.body&&c.body.get('file'))))});
test('Local audio starts only on gesture and packs four bands without sign loss',async()=>{const f=await fixture();await activate(f,9);await selectAudio(f);assert.equal(f.run('beatPlaying'),false);assert.equal(f.run('packBands([1,2,3,255])'),4278387201);await f.$('beatPlay').click();assert.equal(f.run('beatPlaying'),true);assert.equal(f.audioContexts[0].state,'running');f.run('wowTick()');await f.settle();const post=posts(f).at(-1);assert.equal(post.body.get('action'),'audio');assert.equal(post.body.get('value'),'2155905152');assert.equal(f.audios.length,1);assert.equal(f.audios[0].src,'blob:fixture');assert.equal(f.$('beatPlay').disabled,true)});
test('Audio stop, page hiding and mode exit release every audio resource',async()=>{for(const reason of ['stop','hidden','tab','mode','ended','offline']){const f=await fixture();await activate(f,9);await selectAudio(f);await f.$('beatPlay').click();if(reason==='stop')f.$('beatStop').click();if(reason==='hidden'){f.document.hidden=true;f.listeners.visibilitychange()}if(reason==='tab')f.elements.find(e=>e.dataset.tab==='trail').click();if(reason==='mode'){f.api.state.mode=0;await f.run('poll()')}if(reason==='ended')f.audios[0].onended();if(reason==='offline'){f.api.fail={path:'/api/storm'};await f.run('poll()')}assert.equal(f.run('beatPlaying'),false,reason);assert.equal(f.run('beatAudio'),null,reason);assert.equal(f.audioContexts[0].closed,true,reason);assert.equal(f.audios[0].paused,true,reason);assert.ok(f.revoked.includes('blob:fixture'),reason);assert.equal(f.run('controlPending'),null,reason)}});
test('Audio cannot restart after hiding during a pending play promise',async()=>{const f=await fixture();await activate(f,9);await selectAudio(f);let release;f.api.audioPlayHold=new Promise(resolve=>{release=resolve});const starting=f.$('beatPlay').click();assert.equal(f.run('beatStarting'),true);f.document.hidden=true;f.listeners.visibilitychange();release();await starting;assert.equal(f.run('beatPlaying'),false);assert.equal(f.run('beatStarting'),false);assert.equal(f.run('beatAudio'),null);assert.equal(f.audioContexts[0].closed,true)});
test('Unsupported audio and suspended context report errors without streaming',async()=>{for(const kind of ['play','resume','runtime','unsupported']){const f=await fixture();await activate(f,9);await selectAudio(f);if(kind==='play')f.api.audioFail=true;if(kind==='resume')f.api.audioResumeFail=true;if(kind==='unsupported')delete f.context.window.AudioContext;await f.$('beatPlay').click();if(kind==='runtime'){f.audioContexts[0].state='suspended';f.run('wowTick()')}assert.equal(f.run('beatPlaying'),false);assert.equal(f.run('beatAudio'),null);assert.equal(posts(f).filter(c=>c.body.get('action')==='audio').length,0);assert.match(f.$('beatState').textContent,/non avviato|sospeso|non supporta/);assert.equal(f.$('beatPlay').disabled,false)}});


test('BOOT pause and arrival clear a held pointer before explicit resume',async()=>{for(const reason of ['paused','done']){const f=await fixture();await activate(f,5);f.$('steerLeft').onpointerdown(pointer());assert.equal(f.run('heldSteer'),0);assert.ok(f.run('controlPending'));f.api.state.wow[reason]=true;await f.run('poll()');assert.equal(f.run('heldSteer'),null);assert.equal(f.run('steerPointer'),null);assert.equal(f.run('controlPending'),null);assert.equal(f.$('steerLeft').attrs['aria-pressed'],'false');f.api.state.wow[reason]=false;await f.run('poll()');f.run('wowTick()');await f.settle();assert.equal(posts(f).length,0)}});
test('Late steering completion neutralizes a released control without cancelling a new press',async()=>{for(const newPress of [false,true]){const f=await fixture();await activate(f,5);let release;f.api.hold={path:'/api/storm',method:'POST',promise:new Promise(resolve=>{release=resolve})};f.$('steerLeft').onpointerdown(pointer());f.run('wowTick()');f.$('steerLeft').onpointercancel(pointer());if(newPress)f.$('steerRight').onpointerdown(pointer(110,70,2));release();await f.settle();assert.equal(f.beacons.filter(b=>b.action==='steer'&&b.value==='1').length,newPress?0:1);assert.equal(f.run('heldSteer'),newPress?2:null)}});
test('Late spectral completion repeats stop without stopping a newer playback',async()=>{for(const newPlay of [false,true]){const f=await fixture();await activate(f,9);await selectAudio(f);await f.$('beatPlay').click();let release;f.api.hold={path:'/api/storm',method:'POST',promise:new Promise(resolve=>{release=resolve})};f.run('wowTick()');assert.equal(f.run('controlBusy'),true);f.$('beatStop').click();const before=f.beacons.filter(b=>b.action==='background').length;assert.ok(before);const restarting=newPlay?f.$('beatPlay').click():null;release();if(restarting)await restarting;await f.settle();assert.equal(f.beacons.filter(b=>b.action==='background').length,before+(newPlay?0:1));assert.equal(f.run('beatPlaying'),newPlay)}});

test('Home is the default; shortcuts navigate without sending device commands',async()=>{const f=await fixture();assert.equal(f.run('activeTab'),'home');assert.equal(f.$('home').classList.contains('hidden'),false);const before=posts(f).length;for(const tab of ['trail','arcade','wow','pit','home']){await f.run(`navigate('${tab}')`);assert.equal(f.run('activeTab'),tab);for(const other of ['home','trail','arcade','wow','pit'])assert.equal(f.$(other).classList.contains('hidden'),other!==tab);const button=f.elements.find(e=>e.dataset.tab===tab);assert.ok(['true','page'].includes(button.attrs['aria-current'])||button.attrs['aria-pressed']==='true')}for(const shortcut of f.elements.filter(e=>e.dataset.go)){shortcut.click();assert.equal(f.run('activeTab'),shortcut.dataset.go)}assert.equal(posts(f).length,before);assert.equal(f.beacons.length,0)});
test('Home mirrors confirmed session and display state, then clearly goes offline',async()=>{const f=await fixture();Object.assign(f.api.state,{session:true,running:true,elapsed:125000,mode:6});await f.run('poll()');assert.equal(f.$('homeTimer').textContent,'02:05');assert.match(f.$('homeSession').textContent,/corso|attiva|Sessione/);assert.match(f.$('homeDisplay').textContent,/Living/i);const last=f.$('homeTimer').textContent;f.api.fail={path:'/api/storm'};await f.run('poll()');assert.match(f.$('homeConnection').textContent,/Offline|assente|collegamento|Collega il Wi-Fi/i);assert.equal(f.$('homeTimer').textContent,last);for(const shortcut of f.elements.filter(e=>e.dataset.go))assert.equal(shortcut.disabled,false)});
test('Returning to a selected WOW controller enables it immediately without a new poll',async()=>{const f=await fixture();await activate(f,6);await f.run("navigate('home')");const before=f.calls.length;await f.run("navigate('wow')");assert.equal(f.$('livingPad').attrs['aria-disabled'],'false');assert.equal(f.$('livingPanel').classList.contains('hidden'),false);assert.equal(f.calls.length,before)});


test('Queue expiry removes an unsent command without reporting Wi-Fi loss',async()=>{
  const f=await fixture();let release;
  f.api.hold={path:'/api/storm',promise:new Promise(resolve=>{release=resolve})};
  const held=f.run('poll()');
  const queued=f.run("send('start')");
  assert.equal(f.run('httpQueue.length'),1);
  const timer=f.run('httpQueue[0].queueTimer');f.timers.get(timer).fn();
  await queued;assert.equal(f.run('online'),true);assert.equal(f.run('busy'),false);
  assert.match(f.$('message').textContent,/richiesta non inviata/);
  release();await held;await f.settle();
  assert.equal(f.calls.filter(c=>c.method==='POST').length,0);
  assert.equal(f.run('httpQueue.length'),0);
  f.api.hold=null;await f.run("send('start')");
  assert.equal(f.calls.filter(c=>c.method==='POST').length,1);
});
test('A delayed queue timer cannot dispatch an expired request',async()=>{
  const f=await fixture();let release;
  f.api.hold={path:'/api/storm',promise:new Promise(resolve=>{release=resolve})};
  const held=f.run('poll()');const queued=f.run("send('start')");
  f.run('httpQueue[0].expires=0');release();await held;await queued;
  assert.equal(f.calls.filter(c=>c.method==='POST').length,0);
  assert.equal(f.run('online'),true);
});
test('Expiring a queued poll does not mark a healthy transport offline',async()=>{
  const f=await fixture();let release;
  f.api.hold={path:'/api/storm/results',promise:new Promise(resolve=>{release=resolve})};
  const held=f.run("requestJSON('/api/storm/results')");const poll=f.run('poll()');
  f.timers.get(f.run('httpQueue[0].queueTimer')).fn();await poll;
  assert.equal(f.run('online'),true);assert.equal(f.run('pollTask'),null);
  release();await held;await f.settle();
});
test('Dispatch clears the queue timer and preserves ordered successful requests',async()=>{
  const f=await fixture();let release;
  f.api.hold={path:'/api/storm/results',promise:new Promise(resolve=>{release=resolve})};
  const held=f.run("requestJSON('/api/storm/results')");
  const queued=f.run("requestJSON('/api/storm',{method:'POST',body:new URLSearchParams({action:'start'})})");
  const timer=f.run('httpQueue[0].queueTimer');assert.ok(f.timers.has(timer));
  release();await held;await queued;await f.settle();
  assert.equal(f.timers.has(timer),false);assert.equal(f.run('httpQueue.length'),0);
  assert.equal(f.calls.filter(c=>c.method==='POST').length,1);
});

const suiteDeadline=setTimeout(()=>{console.error('Test suite stalled');process.exit(1)},15000);
(async()=>{let passed=0;for(const {name,fn} of tests){try{await fn();console.log('PASS '+name);passed++}catch(error){console.error('FAIL '+name);throw error}}console.log(`\n${passed}/${tests.length} portal behavior checks passed. Fixture tests only; no physical latency/FPS claims.`)})().catch(error=>{console.error(error);process.exitCode=1}).finally(()=>clearTimeout(suiteDeadline));
