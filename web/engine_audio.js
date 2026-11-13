(()=>{
'use strict';
const $=id=>document.getElementById('engine-'+id);
let ctx=null,synth=null,epoch=0,active=false,pending=false,timer=0,guard=0,last=0,stamp=null,volume=.25;
const clock=()=>performance.now();
const eligible=()=>online&&activeTab==='pit'&&!document.hidden&&state&&state.mode===0;
function paint(){
 $('start').disabled=active||pending||!eligible();$('stop').disabled=!active&&!pending;
 $('status').dataset.state=pending?'starting':active?'running':'off';
 $('panel').classList.toggle('engine-running',active);$('volume-value').textContent=Math.round(volume*100)+'%';
}
function status(it,en){$('status').textContent=document.documentElement.lang==='en'?en:it;}
function stop(reason='stop'){
 ++epoch;active=pending=false;clearTimeout(timer);clearTimeout(guard);stamp=null;
 const s=synth,c=ctx;synth=ctx=null;
 if(c)c.onstatechange=null;
 if(s)try{s.stop();}catch(_){}
 if(c)try{Promise.resolve(c.close()).catch(()=>{});}catch(_){}
 $('rpm').textContent='—';$('gear').textContent='—';$('meter').style.setProperty('--engine-level','0');
 const messages={
 stop:['Motore spento. Tocca Avvia per ripartire.','Engine off. Tap Start to restart.'],
 signal:['Audio fermo: attiva gli strumenti ESC e verifica il PWM. Poi premi Avvia.','Audio stopped: enable ESC instruments and check PWM. Then tap Start.'],
 stale:['Audio fermo: dati del gas non aggiornati. Verifica il collegamento e premi Avvia.','Audio stopped: throttle data is stale. Check connection and tap Start.'],
 network:['Audio fermo: risposta non ricevuta. Verifica il Wi-Fi e premi Avvia.','Audio stopped: no response. Check Wi-Fi and tap Start.'],
 busy:['Audio fermo: cruscotto occupato. Riprova con Avvia.','Audio stopped: dashboard busy. Tap Start to retry.'],
 invalid:['Audio fermo: dati strumenti non validi. Aggiorna il firmware e riprova.','Audio stopped: invalid instrument data. Update firmware and retry.'],
 interrupted:['Audio sospeso dal telefono. Tocca Avvia per riprendere.','Audio interrupted by the phone. Tap Start to resume.'],
 hidden:['Audio fermo uscendo dalla schermata. Tocca Avvia per riprendere.','Audio stopped on leaving the screen. Tap Start to resume.'],
 mode:['Torna agli strumenti e premi Avvia motore.','Return to instruments and tap Start engine.']};
 status(...(messages[reason]||messages.stop));paint();
}
function sync(){if((active||pending)&&!eligible())stop(!online?'network':state&&state.mode!==0?'mode':'hidden');else paint();}
function watch(e){if(e!==epoch)return;if(!eligible()){sync();return;}if(clock()-last>=1200){stop('stale');return;}guard=setTimeout(()=>watch(e),100);}
async function tick(e){
 if(e!==epoch||!active)return;if(!eligible()){sync();return;}
 const begin=clock();
 try{
  const data=await window.stormRequest('/api/v4/state',{},800);
  if(e!==epoch)return;if(!eligible()){sync();return;}
  if(ctx.state!=='running')throw {code:'interrupted'};
  if(!data||data.telemetrySource!=='esc_pwm_scenic'||data.escSignalValid!==true)throw {code:'signal'};
  const v=data.vehicle;
  if(!v||!Number.isFinite(v.rpm)||v.rpm<0||v.rpm>8000||!Number.isInteger(v.gear)||v.gear < -1||v.gear>5||!Number.isInteger(data.nowMs)||data.nowMs<0||data.nowMs>4294967295)throw {code:'invalid'};
  if(clock()-begin>=1000)throw {code:'stale'};
  const advance=stamp===null?1:(data.nowMs-stamp)>>>0;
  if(advance>=0x80000000)throw {code:'stale'};
  if(advance){stamp=data.nowMs;last=clock();synth.update(v,volume);
   $('rpm').textContent=String(Math.round(Math.max(800,v.rpm)));$('gear').textContent=v.gear<0?'R':v.gear===0?'N':String(v.gear);
   $('meter').style.setProperty('--engine-level',String(Math.min(1,Math.max(0,(v.rpm-800)/6400))));
   status('Motore attivo · segue RPM e marce del cruscotto.','Engine on · following dashboard RPM and gears.');paint();
  }
 }catch(err){if(e===epoch)stop(err.code==='queue_timeout'?'busy':['signal','invalid','stale','interrupted'].includes(err.code)?err.code:'network');return;}
 if(e===epoch)timer=setTimeout(()=>tick(e),Math.max(40,200-(clock()-begin)));
}
async function start(){
 if(active||pending||!eligible())return;
 const Context=window.AudioContext||window.webkitAudioContext;
 if(!Context){status('Audio non supportato. Apri in Safari o Chrome.','Audio unsupported. Open in Safari or Chrome.');return;}
 stop();pending=true;const e=epoch;last=clock();status('Avviamento · lettura del gas…','Starting · reading throttle…');paint();
 try{
  ctx=new Context();const own=ctx;
  // Unlock in the explicit tap before awaiting telemetry (iPhone autoplay).
  const resumed=own.resume();synth=window.StormEngineSynth.create(own);synth.start();guard=setTimeout(()=>watch(e),100);
  await resumed;if(e!==epoch)return;if(!eligible()){sync();return;}
  if(own.state!=='running')throw Error('interrupted');
  own.onstatechange=()=>{if(e===epoch&&own.state!=='running')stop('interrupted');};
  pending=false;active=true;paint();await tick(e);
 }catch(_){if(e===epoch)stop('interrupted');}
}
$('start').onclick=start;$('stop').onclick=()=>stop();
$('volume').oninput=()=>{const v=Number($('volume').value);volume=Number.isFinite(v)?Math.max(0,Math.min(1,v/100)):0;if(synth)synth.setVolume(volume);paint();};
document.addEventListener('visibilitychange',()=>{if(document.hidden&&(active||pending))stop('hidden');else sync();});
window.addEventListener('pagehide',()=>{if(active||pending)stop('hidden');});
window.stormEngineAudio={stop,sync,isActive:()=>active||pending};paint();
})();
