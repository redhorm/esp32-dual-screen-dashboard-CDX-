
'use strict';
const $=id=>document.getElementById(id),all=s=>document.querySelectorAll(s);
let state=null,online=false,busy=false,pollTask=null,activeTab='home';
let historyRows=null,weightEdited=false;
function msg(text){$('message').textContent=text}
function formatTime(ms){const sec=Math.floor(ms/1000),hours=Math.floor(sec/3600);return(hours?hours+':':'')+String(Math.floor(sec/60)%60).padStart(2,'0')+':'+String(sec%60).padStart(2,'0')}
// Works on captive-portal browsers that do not implement AbortSignal.timeout.
const httpQueue=[];let httpRunning=false;
function pumpHttp(){
  if(httpRunning||!httpQueue.length)return;
  const job=httpQueue.shift();clearTimeout(job.queueTimer);
  // Background tabs may defer timers: check the deadline again at dispatch.
  if(Date.now()>=job.expires){job.reject(Object.assign(Error('queue_timeout'),{code:'queue_timeout',path:job.path}));pumpHttp();return;}
  httpRunning=true;
  performRequest(job.path,job.options,job.timeout).then(job.resolve,job.reject).finally(()=>{httpRunning=false;pumpHttp()});
}
function requestJSON(path,options={},timeout=2500){
  return new Promise((resolve,reject)=>{
    const job={path,options,timeout,resolve,reject,expires:Date.now()+timeout};
    // A command must not wait indefinitely behind another request and execute
    // after its caller has moved on. Queue expiry means it was NEVER sent.
    job.queueTimer=setTimeout(()=>{
      const index=httpQueue.indexOf(job);
      if(index<0)return;
      httpQueue.splice(index,1);
      reject(Object.assign(Error('queue_timeout'),{code:'queue_timeout',path}));
    },timeout);
    httpQueue.push(job);pumpHttp();
  });
}
async function performRequest(path,options,timeoutMs){
  const controller=typeof AbortController!=='undefined'?new AbortController():null;
  let timer;
  const timeout=new Promise((_,reject)=>{timer=setTimeout(()=>{if(controller)controller.abort();reject(Object.assign(Error('timeout'),{code:'timeout',path}))},timeoutMs)});
  try{return await Promise.race([fetch(path,{cache:'no-store',...options,...(controller?{signal:controller.signal}:{})}).then(async response=>{
    let result;try{result=await response.json()}catch(e){throw Object.assign(Error('invalid_response'),{code:'invalid_response',status:response.status,path})}
    if(!response.ok||result.ok===false)throw Object.assign(Error(result.error||'http'),{code:result.error||'http',status:response.status,path});
    return result;
  }),timeout])}catch(e){if(!e.path)e.path=path;throw e}finally{clearTimeout(timer)}
}
window.stormRequest=requestJSON;

function validSnapshot(s){return s&&Number.isInteger(s.mode)&&s.mode>=0&&s.mode<=9&&typeof s.session==='boolean'&&typeof s.running==='boolean'&&['elapsed','gates','penalties','score','weight','gameMs','hits','best','history'].every(k=>Number.isFinite(s[k]))&&(s.mode<5&&!s.wow||validWow(s.wow))}
function validWow(w){return w&&['ms','elapsed','score','best','hits','seed'].every(k=>Number.isInteger(w[k])&&w[k]>=0)&&w.elapsed<=60000&&['paused','done','audioActive'].every(k=>typeof w[k]==='boolean')&&Number.isFinite(w.exploded)&&w.exploded>=0&&w.exploded<=1&&Array.isArray(w.bands)&&w.bands.length===4&&w.bands.every(v=>Number.isInteger(v)&&v>=0&&v<=255)}
function controls(){
  all('[data-action],#game,#rules').forEach(b=>b.disabled=!online||busy);
  for(const id of ['gate','penalty','undo','finish'])$(id).disabled=!online||busy||!state||!state.session;
  if(state){$('rules').disabled=!online||busy||state.session;$('undo').disabled=!online||busy||!state.session||state.gates+state.penalties===0;
    if(state.gates+state.penalties>=128){$('gate').disabled=true;$('penalty').disabled=true}}
  $('jump').disabled=!online||busy||!state||state.mode!==2||state.done;
  $('card').disabled=!online&&(!historyRows||!historyRows.length);
  wowControls();
}
function connected(value){if(!value)haltWow(false);online=value;$('status').textContent=value?'● Online':'○ Offline';$('status').setAttribute('data-online',String(value));$('status').setAttribute('aria-label',value?'Collegato al cruscotto':'Collegamento assente');$('homeConnection').textContent=value?'Collegato al tuo cockpit':'Offline · collega il Wi-Fi stormDASH';$('networkHelp').classList.toggle('hidden',value);controls();if(window.stormPit)window.stormPit.connectionChanged()}
function render(s){
  $('time').textContent=formatTime(s.elapsed);for(const id of ['gates','penalties','score'])$(id).textContent=s[id];
  $('trailState').textContent=s.session?(s.running?'Sessione in corso':'Sessione in pausa'):(s.elapsed||s.gates||s.penalties?'Ultima sessione chiusa':'Pronto per partire');
  $('start').classList.toggle('hidden',s.session);$('pause').classList.toggle('hidden',!s.session||!s.running);$('resume').classList.toggle('hidden',!s.session||s.running);
  if(!weightEdited&&document.activeElement!==$('weight'))$('weight').value=s.weight;
  $('history').textContent=s.history+' risultati nell’archivio. '+(s.saved?'Archivio salvato e riletto.':'Archivio nuovo o salvataggio non disponibile: esporta i risultati prima di spegnere.');
  $('gameState').textContent=(s.done?'ARRIVO':s.paused?'PAUSA · tocca SALTA per riprendere':'NIGHT STAGE')+' • '+formatTime(s.gameMs)+' / 00:45 • '+s.hits+' urti • '+(s.best?'record '+s.best+' punti':'nessun record');
  $('jump').textContent=s.paused?'RIPRENDI + SALTA':'SALTA';
  $('displayMode').textContent='TFT: '+[(window.stormPit?window.stormPit.instrumentLabel():'Strumenti'),'SHOW · AFTERDARK','NIGHT STAGE · gioco','PIT CHECK · osserva i display','TRAIL · tabellone manuale','MIDNIGHT RUN · gioco','LIVING COCKPIT','LIQUID REACTOR','GARAGE 3D','BEAT STORM'][s.mode];
  renderHome(s);
  renderWow(s);
  controls();
}
function poll(){
  if(pollTask)return pollTask;
  pollTask=(async()=>{try{const next=await requestJSON('/api/storm');if(!validSnapshot(next))throw Error('invalid_state');state=next;connected(true);render(next);return next}catch(e){if(e.code!=='queue_timeout')connected(false);return null}finally{pollTask=null}})();
  return pollTask;
}
function errorMessage(error,fallback){if(error.code==='queue_timeout')return 'Cruscotto occupato: richiesta non inviata. Riprova.';if(error.status===400)return 'Valore non valido: controlla i campi.';if(error.status===503)return 'Cruscotto occupato: attendi un momento e verifica lo stato.';return fallback}
async function send(action,value){
  if(!online){msg('Ricollegati al cruscotto prima di inviare comandi.');return false}
  if(busy)return false;
  if(['exit','start','show','game','pit','board','midnight','living','liquid','garage','beat'].includes(action))haltWow(true);
  busy=true;controls();let accepted=false;
  try{await requestJSON('/api/storm',{method:'POST',body:new URLSearchParams({action,...(value===undefined?{}:{value})})});accepted=true;if(action!=='jump')msg('Comando acquisito dal cruscotto.')}
  catch(e){if(!e.status&&e.code!=='queue_timeout')connected(false);msg(errorMessage(e,'Risposta non ricevuta: verifica lo stato prima di ripetere il comando.'))}
  finally{busy=false;controls()}
  if(accepted&&action!=='jump')await poll();return accepted;
}
function integerValue(id,min,max){const field=$(id),raw=field.value.trim();if(!/^\d+$/.test(raw)||Number(raw)<min||Number(raw)>max){field.focus();msg('Inserisci un numero intero tra '+min+' e '+max+'.');return null}return Number(raw)}
function background(){
  haltWow(false);
  if(!online||!state||!([2,5,9].includes(state.mode))||(state.mode===2&&state.done)||(state.mode===5&&state.wow.done))return;
  const body=new URLSearchParams({action:'background'});
  if(navigator.sendBeacon&&navigator.sendBeacon('/api/storm',body))return;
  fetch('/api/storm',{method:'POST',body,keepalive:true}).catch(()=>{});
}
function navigate(tab){
  if(!['home','trail','arcade','wow','pit'].includes(tab))return;
  if((activeTab==='arcade'||activeTab==='wow')&&tab!==activeTab)background();
  activeTab=tab;all('section').forEach(s=>s.classList.toggle('hidden',s.id!==activeTab));
  all('[data-tab]').forEach(b=>{const selected=b.dataset.tab===tab;b.classList.toggle('active',selected);b.setAttribute('aria-pressed',String(selected));b.setAttribute('aria-current',selected?'page':'false')});
  $('appMain').setAttribute('data-page',tab);
  if(window.stormPit)window.stormPit.setActive(activeTab==='pit');
  wowControls();
  if(typeof window.scrollTo==='function')window.scrollTo({top:0,behavior:'auto'});
}
all('[data-tab]').forEach(button=>button.onclick=()=>navigate(button.dataset.tab));
all('[data-go]').forEach(button=>button.onclick=()=>navigate(button.dataset.go));
function renderHome(s){
  $('homeTimer').textContent=s.session?formatTime(s.elapsed):'Pronto.';
  $('homeSession').textContent=s.session?(s.running?'Il tuo giro è in corso':'Il tuo giro è in pausa'):'Il prossimo giro comincia qui';
  $('homeDisplay').textContent=['Strumenti','Portal Link','Night Stage','Pit check','Tabellone Trail','Midnight Run','Living Cockpit','Liquid Reactor','Garage 3D','Beat Storm'][s.mode];
  $('homeTrailAction').textContent=s.session?'Apri la sessione':'Entra in Trail';
}
all('[data-action]').forEach(b=>b.onclick=()=>send(b.dataset.action));
$('weight').oninput=()=>{weightEdited=true};
$('rules').onclick=async()=>{const value=integerValue('weight',0,100);if(value!==null&&await send('rules',value))weightEdited=false};
$('game').onclick=()=>{const value=integerValue('seed',1,4294967295);if(value!==null)send('game',value)};
$('jump').onclick=()=>send('jump');
document.addEventListener('visibilitychange',()=>{if(document.hidden)background();else poll()});
window.addEventListener('pagehide',background);
async function cycle(){
  if(!document.hidden){await poll();if(online&&state&&((state.mode===2&&activeTab==='arcade'&&!state.paused&&!state.done)||(state.mode===5&&activeTab==='wow'&&!state.wow.paused&&!state.wow.done)||(state.mode===9&&activeTab==='wow'&&beatPlaying))&&!busy&&!document.hidden){try{await requestJSON('/api/storm',{method:'POST',body:new URLSearchParams({action:'heartbeat'})})}catch(e){if(!e.status&&e.code!=='queue_timeout')connected(false)}}}
  setTimeout(cycle,800);
}
// One bounded, coalescing lane for phone gestures and spectral frames. Controls
// expire on the device too; no queued gesture is replayed after reconnection.
let controlPending=null,controlBusy=false,heldSteer=null,steerPointer=null,steerGeneration=0;
let beatFile=null,beatAudio=null,beatContext=null,beatSource=null,beatAnalyser=null,beatBins=null,beatUrl=null,beatPlaying=false,beatStarting=false,beatGeneration=0;
const wowPanels=['midnight','living','liquid','garage','beat'];
function wowAvailable(mode){return online&&!busy&&!document.hidden&&activeTab==='wow'&&state&&state.mode===mode}
function wowControls(){
  all('[data-wow]').forEach(b=>b.disabled=!online||busy);
  $('midnightStart').disabled=!online||busy;
  $('midnightResume').disabled=!wowAvailable(5)||!state.wow.paused||state.wow.done;
  for(const id of ['steerLeft','steerRight'])$(id).disabled=!wowAvailable(5)||state.wow.paused||state.wow.done;
  for(const [id,mode] of [['livingBlink',6],['liquidWave',7],['garageExplode',8],['garageAssemble',8]])$(id).disabled=!wowAvailable(mode);
  for(const [id,mode] of [['livingPad',6],['liquidPad',7],['garagePad',8]])$(id).setAttribute('aria-disabled',wowAvailable(mode)?'false':'true');
  $('beatPlay').disabled=!wowAvailable(9)||!beatFile||beatPlaying||beatStarting;
  $('beatStop').disabled=!beatPlaying&&!beatStarting;
}
function renderWow(s){
  const index=s.mode-5;
  $('wowGallery').classList.toggle('has-selection',index>=0);
  wowPanels.forEach((name,i)=>$(name+'Panel').classList.toggle('hidden',i!==index));
  $('wowIdle').classList.toggle('hidden',index>=0);
  all('[data-wow]').forEach(b=>b.setAttribute('aria-pressed',Number(b.dataset.mode)===s.mode?'true':'false'));
  if(s.mode!==5||s.wow.paused||s.wow.done)clearSteer();
  if(s.mode!==9&&(beatAudio||beatContext))stopBeat(false);
  if(s.mode===5){const w=s.wow;$('midnightState').textContent=(w.done?'ARRIVO':w.paused?'PAUSA · tocca Riprendi':'IN CORSA')+' • '+formatTime(w.elapsed)+' / 01:00 • '+w.score+' punti • '+w.hits+' urti • record '+w.best+(w.saved===false?' • salvataggio record non confermato':'')}
}
function beaconControl(action,value){
  if(!online)return;
  const body=new URLSearchParams({action,...(value===undefined?{}:{value})});
  if(navigator.sendBeacon&&navigator.sendBeacon('/api/storm',body))return;
  fetch('/api/storm',{method:'POST',body,keepalive:true}).catch(()=>{});
}
function controlValid(action,value){
  if(!Number.isInteger(value)||value<0)return false;
  if(action==='steer')return value<=2&&wowAvailable(5)&&!state.wow.paused&&!state.wow.done;
  if(action==='touch')return value<=65535&&(wowAvailable(6)||wowAvailable(7));
  if(action==='orbit')return value<=65535&&wowAvailable(8);
  return action==='audio'&&value<=4294967295&&wowAvailable(9);
}
function offerControl(action,value){if(controlValid(action,value))controlPending={action,value,mode:state.mode}}
async function flushControl(){
  if(controlBusy||!controlPending)return;
  const next=controlPending;controlPending=null;
  if(!state||next.mode!==state.mode||!controlValid(next.action,next.value))return;
  controlBusy=true;const audioEpoch=beatGeneration,steerEpoch=steerGeneration;
  try{await requestJSON('/api/storm',{method:'POST',body:new URLSearchParams({action:next.action,value:next.value})})}
  catch(e){controlPending=null;if(!e.status&&e.code!=='queue_timeout')connected(false);else{haltWow(false);msg(errorMessage(e,'Controllo interrotto: verifica il collegamento.'))}}
  finally{
    controlBusy=false;
    // A request accepted after Stop must be followed by silence/neutral, while
    // a new deliberate play or press keeps ownership of its current controls.
    if(online&&state&&state.mode===next.mode){
      if(next.action==='audio'&&audioEpoch!==beatGeneration&&!beatAudio&&!beatPlaying&&!beatStarting)beaconControl('background');
      if(next.action==='steer'&&next.value!==1&&steerEpoch!==steerGeneration&&heldSteer===null)beaconControl('steer',1);
    }
  }
}
function paintSteer(){for(const [id,value] of [['steerLeft',0],['steerRight',2]]){$(id).classList.toggle('held',heldSteer===value);$(id).setAttribute('aria-pressed',heldSteer===value?'true':'false')}}
function clearSteer(){++steerGeneration;heldSteer=null;steerPointer=null;if(controlPending&&controlPending.action==='steer')controlPending=null;paintSteer()}
function releaseSteer(){const held=heldSteer!==null;clearSteer();if(held)offerControl('steer',1)}
function haltWow(notify){
  controlPending=null;
  if(notify&&heldSteer!==null&&state&&state.mode===5)beaconControl('steer',1);
  clearSteer();stopBeat(notify);
}
for(const [id,direction] of [['steerLeft',0],['steerRight',2]]){
  const button=$(id);
  button.onpointerdown=e=>{if(button.disabled)return;e.preventDefault();heldSteer=direction;steerPointer=e.pointerId;try{button.setPointerCapture(e.pointerId)}catch(_){}offerControl('steer',direction);paintSteer()};
  button.onpointerup=button.onpointercancel=button.onlostpointercapture=e=>{if(steerPointer===e.pointerId)releaseSteer()};
  button.onkeydown=e=>{if(button.disabled||![' ','Enter','ArrowLeft','ArrowRight'].includes(e.key))return;e.preventDefault();heldSteer=direction;steerPointer='keyboard';offerControl('steer',direction);paintSteer()};
  button.onkeyup=e=>{if([' ','Enter','ArrowLeft','ArrowRight'].includes(e.key)){e.preventDefault();releaseSteer()}};
  button.onblur=()=>{if(heldSteer===direction)releaseSteer()};
}
function bindPad(id,mode,action){
  const pad=$(id);let pointer=null,x=128,y=128;
  const emit=()=>{pad.setAttribute('aria-valuenow',String(x));pad.setAttribute('aria-valuetext','Orizzontale '+x+', verticale '+y);offerControl(action,(x<<8)|y)};
  const move=e=>{const rect=pad.getBoundingClientRect();if(!rect.width||!rect.height)return;x=Math.max(0,Math.min(255,Math.round((e.clientX-rect.left)/rect.width*255)));y=Math.max(0,Math.min(255,Math.round((e.clientY-rect.top)/rect.height*255)));emit()};
  pad.onpointerdown=e=>{if(!wowAvailable(mode))return;e.preventDefault();pointer=e.pointerId;try{pad.setPointerCapture(pointer)}catch(_){}move(e)};
  pad.onpointermove=e=>{if(pointer===e.pointerId&&wowAvailable(mode)){e.preventDefault();move(e)}};
  pad.onpointerup=pad.onpointercancel=pad.onlostpointercapture=e=>{if(pointer===e.pointerId)pointer=null};
  pad.onkeydown=e=>{if(!wowAvailable(mode)||!['ArrowLeft','ArrowRight','ArrowUp','ArrowDown','Home'].includes(e.key))return;e.preventDefault();if(e.key==='Home'){x=128;y=128}else{x=Math.max(0,Math.min(255,x+(e.key==='ArrowLeft'?-16:e.key==='ArrowRight'?16:0)));y=Math.max(0,Math.min(255,y+(e.key==='ArrowUp'?-16:e.key==='ArrowDown'?16:0)))}emit()};
}
bindPad('livingPad',6,'touch');bindPad('liquidPad',7,'touch');bindPad('garagePad',8,'orbit');
all('[data-wow]').forEach(b=>b.onclick=async()=>{
  const name=b.dataset.wow;
  const seed=name==='midnight'?integerValue('midnightSeed',1,4294967295):undefined;
  if(seed===null)return false;
  const accepted=await send(name,seed);
  // Only an explicit selection moves the viewport; background polling never does.
  if(accepted&&online&&activeTab==='wow'&&state&&state.mode===Number(b.dataset.mode)){
    const panel=$(name+'Panel');
    if(typeof panel.scrollIntoView==='function')panel.scrollIntoView({block:'start',behavior:'auto'});
  }
  return accepted;
});
$('midnightStart').onclick=()=>{const seed=integerValue('midnightSeed',1,4294967295);if(seed!==null)return send('midnight',seed)};
$('midnightResume').onclick=()=>send('heartbeat',1);
$('livingBlink').onclick=$('liquidWave').onclick=()=>send('impulse',0);
$('garageExplode').onclick=()=>send('explode',1);$('garageAssemble').onclick=()=>send('explode',0);
function setBeatMeters(bands){bands.forEach((v,i)=>$('beatBand'+i).style.transform='scaleY('+Math.max(.03,v/255)+')')}
function packBands(bands){return bands.reduce((word,value,index)=>(word|(value<<(8*index)))>>>0,0)}
function analyseBands(){
  beatAnalyser.getByteFrequencyData(beatBins);
  const hz=beatContext.sampleRate/beatAnalyser.fftSize,edges=[40,180,800,3000,Math.min(12000,beatContext.sampleRate/2)];
  return edges.slice(0,4).map((low,i)=>{const first=Math.min(beatBins.length-1,Math.max(0,Math.floor(low/hz))),last=Math.min(beatBins.length,Math.max(first+1,Math.ceil(edges[i+1]/hz)));let sum=0;for(let j=first;j<last;j++)sum+=beatBins[j]*beatBins[j];return Math.min(255,Math.round(Math.sqrt(sum/(last-first))))});
}
function stopBeat(notify=false){
  ++beatGeneration;beatPlaying=false;beatStarting=false;
  if(controlPending&&controlPending.action==='audio')controlPending=null;
  const audio=beatAudio,context=beatContext;beatAudio=null;beatContext=null;
  if(audio){audio.onended=null;audio.onerror=null;audio.pause();audio.removeAttribute('src');audio.load()}
  if(beatSource){try{beatSource.disconnect()}catch(_){}beatSource=null}
  if(beatAnalyser){try{beatAnalyser.disconnect()}catch(_){}beatAnalyser=null}
  beatBins=null;if(context){try{Promise.resolve(context.close()).catch(()=>{})}catch(_){}}
  if(beatUrl){URL.revokeObjectURL(beatUrl);beatUrl=null}
  if(notify&&state&&state.mode===9)beaconControl('background');
  setBeatMeters([0,0,0,0]);$('beatState').textContent=beatFile?'Pronto: tocca Riproduci per iniziare dal principio.':'Scegli un file, poi tocca Riproduci.';wowControls();
}
$('beatFile').onchange=()=>{
  stopBeat(true);const file=$('beatFile').files&&$('beatFile').files[0];beatFile=null;
  if(!file){$('beatFileName').textContent='Nessun brano selezionato.';wowControls();return}
  if(!file.size||file.size>30*1024*1024){$('beatFileName').textContent='File non accettato.';$('beatState').textContent='Scegli un file audio non vuoto, al massimo 30 MB.';wowControls();return}
  if(!(file.type&&file.type.startsWith('audio/'))&&!/\.(mp3|wav|m4a|ogg|aac|flac|opus|webm)$/i.test(file.name)){$('beatFileName').textContent='Formato non riconosciuto.';$('beatState').textContent='Scegli un file audio supportato dal browser.';wowControls();return}
  beatFile=file;$('beatFileName').textContent=file.name+' · '+(file.size/1048576).toFixed(1)+' MB';$('beatState').textContent='Tocca Riproduci: il file resta sul telefono.';wowControls();
};
async function playBeat(){
  if(!wowAvailable(9)||!beatFile||beatPlaying||beatStarting)return;
  const Context=window.AudioContext||window.webkitAudioContext;
  if(!Context){$('beatState').textContent='Questo browser non supporta l’analisi audio. Apri il portale in Safari o Chrome.';return}
  stopBeat(false);beatStarting=true;const generation=beatGeneration;wowControls();
  try{
    beatContext=new Context();beatAudio=document.createElement('audio');beatAudio.preload='auto';beatAudio.playsInline=true;
    beatUrl=URL.createObjectURL(beatFile);beatAudio.src=beatUrl;
    beatAnalyser=beatContext.createAnalyser();beatAnalyser.fftSize=512;beatAnalyser.smoothingTimeConstant=.65;
    beatBins=new Uint8Array(beatAnalyser.frequencyBinCount);beatSource=beatContext.createMediaElementSource(beatAudio);beatSource.connect(beatAnalyser);beatAnalyser.connect(beatContext.destination);
    beatAudio.onended=()=>{stopBeat(true);$('beatState').textContent='Brano terminato. Tocca Riproduci per riascoltarlo.'};
    beatAudio.onerror=()=>{stopBeat(true);$('beatState').textContent='Il browser non riesce a riprodurre questo file. Prova un MP3 o WAV.'};
    // Both calls occur in the explicit button gesture, before yielding.
    const resume=beatContext.resume(),play=beatAudio.play();await Promise.all([resume,play]);
    if(generation!==beatGeneration)return;
    if(!wowAvailable(9)||beatContext.state!=='running'){stopBeat(true);$('beatState').textContent='Riproduzione non avviata. Tocca di nuovo Riproduci nel browser.';return}
    beatPlaying=true;beatStarting=false;$('beatState').textContent='In riproduzione sul telefono · spettro inviato al cockpit.';wowControls();
  }catch(e){if(generation!==beatGeneration)return;stopBeat(true);$('beatState').textContent='Audio non avviato. Verifica il formato e tocca di nuovo Riproduci.'}
}
$('beatPlay').onclick=playBeat;$('beatStop').onclick=()=>stopBeat(true);
function wowTick(){
  if(document.hidden||!online||activeTab!=='wow'){if(heldSteer!==null||beatAudio)haltWow(false);controlPending=null}
  else if(!busy){
    if(heldSteer!==null)offerControl('steer',heldSteer);
    if(beatPlaying&&wowAvailable(9)){
      if(!beatContext||beatContext.state!=='running'){stopBeat(true);$('beatState').textContent='Audio sospeso dal telefono. Tocca Riproduci per riavviare.'}
      else try{const bands=analyseBands();setBeatMeters(bands);offerControl('audio',packBands(bands))}catch(e){stopBeat(true);$('beatState').textContent='Analisi audio interrotta. Tocca Riproduci per riprovare.'}
    }
    flushControl();
  }
  setTimeout(wowTick,100);
}

// Export only closed results returned by the ESP32 archive, never the live counters.
function validResult(r){return r&&['duration','gates','penalties','score'].every(k=>Number.isFinite(r[k]))&&r.duration>=0&&r.gates>=0&&r.penalties>=0}
async function loadResults(){const rows=await requestJSON('/api/storm/results');if(!Array.isArray(rows)||!rows.every(validResult))throw Error('invalid_archive');historyRows=rows;return rows}
function drawCard(result){
  const canvas=document.createElement('canvas');canvas.width=1080;canvas.height=1920;const context=canvas.getContext('2d');
  if(!context){msg('Questo browser non può creare la card. Usa l’esportazione CSV.');return}
  context.fillStyle='#090914';context.fillRect(0,0,1080,1920);context.fillStyle='#a578ff';context.fillRect(80,230,920,5);
  context.font='bold 82px sans-serif';context.fillText('stormDASH',80,160);context.fillStyle='#eee8ff';context.font='48px sans-serif';context.fillText(result.test?'COLLAUDO · TRAIL SESSION':'TRAIL SESSION',80,390);
  let name=$('model').value.trim().slice(0,32)||'Il mio crawler';context.font='60px sans-serif';while(context.measureText(name).width>920&&name.length>1)name=name.slice(0,-1);context.fillText(name,80,520);
  const time=formatTime(result.duration);context.font='bold '+(time.length>5?120:160)+'px sans-serif';context.fillText(time,80,820);
  context.font='62px sans-serif';context.fillText(result.gates+' gate · '+result.penalties+' penalità',80,1050);context.fillText(result.score+' punti',80,1180);
  context.font='36px sans-serif';context.fillText(result.test?'Dati di collaudo · non è un’uscita reale':'Risultati inseriti manualmente',80,1580);
  context.fillText(Number.isFinite(result.penaltyWeight)?'Peso penalità '+result.penaltyWeight:'Peso penalità non disponibile',80,1650);context.fillStyle='#b48aff';context.fillText('stormDASH // AFTERDARK',80,1780);
  canvas.toBlob(blob=>{if(!blob){msg('Esportazione immagine non disponibile. Usa il CSV.');return}const link=document.createElement('a'),url=URL.createObjectURL(blob);link.href=url;link.download=result.test?'stormDASH-collaudo.png':'stormDASH-session.png';document.body.appendChild(link);link.click();link.remove();setTimeout(()=>URL.revokeObjectURL(url),30000);msg(result.test?'Card di collaudo scaricata.':'Card del risultato chiuso scaricata.')},'image/png');
}
$('card').onclick=async()=>{
  try{const rows=online?await loadResults():historyRows;if(!rows||!rows.length){msg('Non ci sono ancora risultati chiusi da esportare.');return}const result=rows.slice().reverse().find(r=>!r.test);if(!result){msg('L’archivio contiene solo collaudi: apri i risultati per esportare una card contrassegnata.');return}drawCard(result)}catch(e){msg('Archivio non disponibile: nessuna card è stata creata.')}
};
$('historyButton').onclick=async()=>{
  try{const rows=await loadResults();$('results').replaceChildren();if(!rows.length){$('results').textContent='Nessuna sessione chiusa. Il primo risultato comparirà qui.';return}
    rows.slice().reverse().forEach((r,i)=>{const item=document.createElement('div'),text=document.createElement('p'),button=document.createElement('button');item.className='result';text.textContent=(r.test?'COLLAUDO · ':'')+'#'+(rows.length-i)+' · '+formatTime(r.duration)+' · '+r.gates+' gate · '+r.penalties+' penalità · '+r.score+' punti';button.textContent=r.test?'Card collaudo':'Card risultato';button.onclick=()=>drawCard(r);item.append(text,button);$('results').append(item)})
  }catch(e){msg('Storico non disponibile: verifica il collegamento.')}
};
controls();cycle();wowTick();
if(typeof location!=='undefined'&&location.hash==='#pit')navigate('pit');
