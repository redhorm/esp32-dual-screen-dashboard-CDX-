(()=>{

'use strict';
const root=document.getElementById('pitControls');
const $=id=>document.getElementById('pit-'+id);
let active=false,initialized=false;
let language=0,settingsLoaded=false,identityLoaded=false,lightLoaded=false,escLoaded=false,telemetryLoaded=false,displayLoaded=false,bootLoaded=false;
let showState={active:false,elapsedMs:0,durationMs:6000},lightState={},displayState={logicalWidth:240,logicalHeight:135};
let telemetryState={};
let manual='clear',mutationBusy=false,pollBusy=false,pollTimer=0,lastSlow=0,connected=false;
const readIssues=new Map();
const tr=(it,en)=>language?en:it;
function message(id,text,error=false){const e=$(id);e.textContent=text;e.className='status '+(error?'error':'ok');}
function translate(){document.documentElement.lang=language?'en':'it';root.querySelectorAll('[data-it]').forEach(e=>{const value=language?e.dataset.en:e.dataset.it;if(e.hasAttribute('data-html'))e.innerHTML=value;else e.textContent=value;});root.querySelectorAll('[data-label-it]').forEach(e=>e.setAttribute('aria-label',language?e.dataset.labelEn:e.dataset.labelIt));renderShow();renderLights();renderTelemetry();if(window.stormEngineAudio)window.stormEngineAudio.sync();}
function connection(){
 const ready=settingsLoaded&&identityLoaded&&lightLoaded&&escLoaded&&telemetryLoaded&&displayLoaded;
 connected=ready&&online;
 $('connection').textContent=!online?tr('CONNESSIONE PERSA · DATI NON AGGIORNATI','OFFLINE · DATA NOT UPDATED'):readIssues.size?tr('DATI PARZIALI · RIPROVA','PARTIAL DATA · RETRY'):ready?tr('DATI AGGIORNATI','DATA UP TO DATE'):tr('CARICAMENTO DATI…','LOADING DATA…');
 $('connection').className='link '+(connected&&!readIssues.size?'online':'');
 $('retryButton').hidden=connected&&!readIssues.size;
 message('globalStatus',readIssues.size?[...readIssues.values()].map(errorText).join(' · '):'',!!readIssues.size);
}
function errorText(e){const errors={neutral_unavailable:tr('Segnale neutro non disponibile: rilascia il gas e riprova.','Neutral signal unavailable: release throttle and retry.'),config_domain_mismatch:tr('Comando non valido per questa sezione. Ricarica la pagina.','Wrong settings section. Reload the page.'),invalid_calibration:tr('Tieni tutto gas AVANTI e riprova la lettura.','Hold full FORWARD throttle and retry the reading.'),calibration_unstable:tr('Il gas non era stabile: tienilo al massimo e riprova.','Throttle was not stable: hold it at full and retry.'),calibration_commit_failed:tr('Taratura non salvata. Rilascia il gas e riprova.','Calibration was not saved. Release throttle and retry.'),invalid_settings:tr('Valori non validi. Controlla i campi.','Invalid values. Check the fields.'),settings_commit_failed:tr('Impostazioni non salvate. Riprova.','Settings were not saved. Try again.'),profile_commit_failed:tr('Nome non salvato. Riprova.','Name was not saved. Try again.'),invalid_rtl_config:tr('Controlla luminosità e taratura PWM.','Check brightness and PWM calibration.'),rtl_commit_failed:tr('Configurazione luci non salvata.','Light configuration was not saved.'),filesystem_unavailable:tr('Archivio non disponibile. La memoria viene inizializzata solo se verificata vuota: i dati esistenti restano protetti. Controlla Hardware e diagnostica. Guida e SHOW funzionano.','Storage is unavailable. Only verified empty memory is initialized; existing data is protected. Check Hardware and diagnostics. Drive and SHOW still work.'),no_space:tr('Spazio immagini insufficiente.','Not enough image storage.'),invalid_image:tr('Immagine non supportata: usa JPG, PNG o WebP.','Unsupported image: use JPG, PNG or WebP.'),too_large:tr('Immagine troppo grande. Massimo 10 MB.','Image is too large. Maximum 10 MB.'),esc_required_for_pit:tr('Test box: abilita e collega il PWM.','Pit test: enable and connect PWM.'),esc_not_neutral:tr('Test box: il ricevitore deve essere in neutro stabile.','Pit test: the receiver must be at stable neutral.'),invalid_transaction:tr('Caricamento interrotto. Seleziona e invia di nuovo.','Upload interrupted. Select and upload again.')};if(errors[e.code])return errors[e.code];
 const names={'/api/v4/state':'Stato strumenti','/api/v3/state':'Identità','/api/esc/state':'Segnale PWM','/api/lights/state':'Luci','/api/v4/diagnostics':'Diagnostica','/api/copilot-display':'Display copilota','/api/settings':'Preferenze'};
 const name=names[e.path]||tr('Comando','Command');
 if(e.code==='queue_timeout')return name+tr(': richiesta non inviata; cruscotto occupato. Riprova.',': request not sent; dashboard busy. Retry.');
 if(e.code==='invalid_response')return name+tr(': risposta non valida dal firmware. Riprova.',': invalid firmware response. Retry.');
 if(e.status)return name+' · HTTP '+e.status+tr('. Il cruscotto risponde; questo servizio ha restituito un errore.','. Dashboard reached; this service returned an error.');
 return name+tr(': richiesta scaduta o rete non raggiungibile. Riprova.',': request timed out or network unreachable. Retry.');}
async function request(path,data,timeout=4000){
 const options=data===undefined?{}:{method:'POST',body:data instanceof FormData?data:new URLSearchParams(data)};
 try{const result=await window.stormRequest(path,options,timeout);if(data===undefined)readIssues.delete(path);return result;}
 catch(e){if(data===undefined)readIssues.set(path,e);throw e;}
 finally{connection();}
}
async function action(button,status,fn){if(mutationBusy)return;mutationBusy=true;button.disabled=true;message(status,tr('Un momento…','One moment…'));try{await fn();connection(true);}catch(e){message(status,errorText(e),true);}finally{mutationBusy=false;button.disabled=false;renderShow();}}
function formValues(form){return Object.fromEntries(new FormData(form));}
function percent(id){$(id+'Value').textContent=Math.round(Number($(id).value)/255*100)+'%';}
['driverBrightness','navigatorBrightness','rtlBrightness'].forEach(id=>{$(id).oninput=()=>percent(id);percent(id);});
for(let i=0;i<16;i++){$('pulseLine').append(document.createElement('i'));$('rtlPixels').append(document.createElement('i'));}
function renderShow(){const active=showState.pending?showState.requestedActive:showState.active,elapsed=showState.active?Math.min(6000,Number(showState.elapsedMs)||0):0;$('showButton').textContent=active?tr('INTERROMPI SHOW','STOP SHOW'):tr('AVVIA SHOW · 6 s','START SHOW · 6 s');$('showButton').classList.toggle('running',active);$('showButton').disabled=mutationBusy||!connected;$('showProgress').style.width=(elapsed/6000*100)+'%';$('showTime').textContent=String(Math.floor(elapsed/1000)).padStart(2,'0')+' / 6 s';$('showPhase').textContent=active?tr('SHOW in corso · DEMO','SHOW running · DEMO'):tr('Pronto quando vuoi','Ready when you are');Array.from($('pulseLine').children).forEach((e,i)=>{e.className=active&&(i<Math.floor(elapsed/6000*17))?(i>11?'peak':'lit'):'';});}
function renderEscFields(){const f=$('escForm'),crawler=f.elements.escType.value==='crawler_forward_reverse';$('escReverseField').hidden=crawler;$('crawlerHint').hidden=!crawler;f.elements.escReverseConfirmMs.readOnly=crawler;}
function renderLights(){$('throttleCalibrationState').textContent=lightState.escFullForwardCalibrated?tr('Gas massimo calibrato. Avanti, neutro e retro restano invariati.','Full throttle calibrated. Forward, neutral and reverse remain unchanged.'):tr('Per raggiungere il limitatore, calibra il gas massimo: non servono numeri.','To reach the limiter, calibrate full throttle: no numbers needed.');const s=lightState,pwm=s.escValid?String(s.escPulse||0)+' µs':tr('ASSENTE','NO SIGNAL');const states={FORWARD:tr('AVANTI','FORWARD'),NEUTRAL:tr('NEUTRO','NEUTRAL'),BRAKING:tr('FRENO','BRAKING'),REVERSE_ARMED:tr('ATTESA RETRO','REVERSE ARMED'),REVERSE:tr('RETRO','REVERSE')},drive=s.escValid?(states[s.escState]||'—'):'';$('pwmStatus').textContent=pwm+(drive?' · '+drive:'');Array.from($('rtlPixels').children).forEach((e,i)=>{const p=s.pixels&&s.pixels[i]||[24,28,30];e.style.background='rgb('+p.join(',')+')';});root.querySelectorAll('[data-rtl]').forEach(b=>b.classList.toggle('active',manual!=='clear'&&b.dataset.rtl===manual));const raw=s.escValid&&Number.isFinite(s.escRawPulse)?tr(' · grezzo ',' · raw ')+s.escRawPulse+' µs':'',age=s.escValid&&Number.isFinite(s.escAgeMs)?tr(' · età ',' · age ')+s.escAgeMs+' ms':'';$('escSummary').textContent=s.escEnabled?tr('Strumenti dal gas attivi. Luci PWM indipendenti.','Throttle instruments active. PWM lights are independent.'):tr('Strumenti in DEMO. Luci PWM ancora attive.','Instruments in DEMO. PWM lights still active.');if(!mutationBusy)$('lightStatus').textContent=s.hardwareReady===false?tr('Uscita luci non pronta. Controlla la diagnostica.','Light output is not ready. Check diagnostics.'):tr('PWM: ','PWM: ')+pwm+(drive?' · '+drive:'')+raw+age+' · '+(manual==='clear'?tr('AUTOMATICO','AUTOMATIC'):tr('MANUALE','MANUAL'));}
function renderTelemetry(){const esc=telemetryState.telemetrySource==='esc_pwm_scenic',known=!!telemetryState.telemetrySource;$('telemetryStatus').textContent=showState.active?'SHOW · DEMO':!known?'—':esc?(telemetryState.escSignalValid?'ESC':tr('ESC · ATTESA','ESC · WAIT')):'DEMO';}
async function refreshTelemetry(){telemetryState=await request('/api/v4/state');telemetryLoaded=true;renderTelemetry();}
async function loadSettings(){const s=await request('/api/settings');const f=$('displayForm');for(const key of ['driverBrightness','navigatorBrightness','language'])f.elements[key].value=s[key];f.elements.units.value=s.useKmh?'metric':'imperial';$('bootImageEnabled').checked=!!s.bootImageEnabled;$('bootHoldMs').value=s.bootHoldMs||1800;language=Number(s.language)===1?1:0;settingsLoaded=true;$('displayFields').disabled=false;['driverBrightness','navigatorBrightness'].forEach(percent);translate();}
async function loadIdentity(){const s=await request('/api/v3/state');const identity=s.identity||{};$('rigTitle').textContent=identity.vehicle||identity.vehicleName||'stormDASH';if(!identityLoaded){const f=$('identityForm');f.elements.vehicle.value=identity.vehicle||identity.vehicleName||'';f.elements.number.value=identity.number||'';f.elements.driver.value=identity.driver||'';identityLoaded=true;$('identityFields').disabled=false;}}
async function refreshLights(fillDomain=''){
 lightState=await request('/api/esc/state');
 if(!escLoaded||fillDomain==='esc'){
  const f=$('escForm');['escType','escNeutralUs','escDeadbandUs','escReverseConfirmMs'].forEach(key=>{if(lightState[key]!==undefined)f.elements[key].value=lightState[key]});
  $('escEnabled').checked=!!lightState.escEnabled;$('escInverted').checked=!!lightState.escInverted;
  renderEscFields();escLoaded=true;$('escFields').disabled=false;
 }
 if(!lightLoaded||fillDomain==='lights'){
  const f=$('lightForm');f.elements.profile.value=lightState.profile;f.elements.brightness.value=lightState.brightness;
  lightLoaded=true;$('lightFields').disabled=false;percent('rtlBrightness');
 }
 renderLights();
}
async function loadDisplay(){displayState=await request('/api/copilot-display');$('displayProfile').value='240x135_landscape';$('profileButton').hidden=displayState.profile==='240x135_landscape';displayLoaded=true;}
async function diagnostics(){const d=await request('/api/v4/diagnostics');$('diagnosticOutput').textContent=[d.firmware||'stormDASH',tr('Pilota: ','Driver: ')+(d.displayA?.ok?'OK':'—')+' · '+(d.displayA?.fps||0)+' fps',tr('Copilota: ','Co-driver: ')+(d.displayB?.ok?'OK':'—')+' · '+(d.displayB?.fps||0)+' fps','RAM '+Math.round((d.memory?.heapFreeBytes||0)/1024)+' KB · '+tr('blocco ','block ')+Math.round((d.memory?.heapLargestBytes||0)/1024)+' KB',tr('Archivio: ','Storage: ')+(d.storage?.ready?'OK':tr('non disponibile','unavailable')),tr('Ultimo errore: ','Last error: ')+(d.lastError||'—'),tr('Avvii registrati: ','Recorded boots: ')+(d.restartCount||0)+' · '+(d.resetReason||'—')].join('\n');message('diagnosticStatus',tr('Aggiornato','Updated'));}
async function bootInfo(){const b=await request('/api/photos');bootLoaded=true;$('bootAvailable').hidden=!b.fsReady;$('deleteBoot').disabled=!b.bootDriver&&!b.bootNavigator;$('bootPreview').hidden=!b.bootNavigator;if(b.bootNavigator)$('bootPreview').src='/api/media/boot?display=b&t='+Date.now();message('bootStatus',b.fsReady?(b.formattedOnBegin?tr('Archivio inizializzato su memoria vuota. JPG, PNG o WebP fino a 10 MB.','Storage initialized on empty memory. JPG, PNG or WebP up to 10 MB.'):tr('Archivio pronto. JPG, PNG o WebP fino a 10 MB.','Storage ready. JPG, PNG or WebP up to 10 MB.')):errorText({code:'filesystem_unavailable'}),!b.fsReady);}
function schedulePoll(delay=650){clearTimeout(pollTimer);if(!document.hidden&&active)pollTimer=setTimeout(poll,delay);}
async function poll(){
 if(document.hidden||!active)return;
 if(pollBusy||mutationBusy){schedulePoll();return;}
 pollBusy=true;
 try{
  if(manual!=='clear')try{await request('/api/lights/command',{function:manual},1600)}catch(e){manual='clear';message('lightStatus',errorText(e),true)}
  const tasks=[async()=>{showState=await request('/api/show');renderShow()},refreshLights];
  if(Date.now()-lastSlow>3000){tasks.push(refreshTelemetry);if(!settingsLoaded)tasks.push(loadSettings);if(!identityLoaded)tasks.push(loadIdentity);if(!displayLoaded)tasks.push(loadDisplay);lastSlow=Date.now()}
  for(const task of tasks){if(!active||document.hidden)break;try{await task()}catch(e){/* request records a service-specific error; other reads still run */}}
 }finally{pollBusy=false;connection();renderShow();schedulePoll(showState.active||manual!=='clear'?500:1100)}
}
function clearManual(){const had=manual!=='clear';manual='clear';renderLights();if(had){const data=new URLSearchParams({function:'clear'});if(document.hidden&&navigator.sendBeacon)navigator.sendBeacon('/api/lights/command',data);else request('/api/lights/command',{function:'clear'},1200).catch(()=>{});}}
$('showButton').onclick=()=>{if(window.stormEngineAudio)window.stormEngineAudio.stop('mode');return action($('showButton'),'showStatus',async()=>{const active=showState.pending?showState.requestedActive:showState.active;showState=await request('/api/show',{action:active?'stop':'start'});message('showStatus',active?tr('Ritorno alla guida.','Returning to drive.'):tr('SHOW avviato. Guarda i due display.','SHOW started. Watch both displays.'));renderShow();});};
$('identityForm').onsubmit=e=>{e.preventDefault();const f=e.currentTarget;action(f.querySelector('button'),'identityStatus',async()=>{await request('/api/v3/profile',formValues(f));identityLoaded=false;await loadIdentity();message('identityStatus',tr('Nome salvato.','Name saved.'));});};
$('displayForm').onsubmit=e=>{e.preventDefault();const f=e.currentTarget;action(f.querySelector('button'),'displayStatus',async()=>{await request('/api/settings',formValues(f));language=Number(f.elements.language.value)===1?1:0;translate();message('displayStatus',tr('Display aggiornati.','Displays updated.'));});};
$('escForm').elements.escType.onchange=renderEscFields;
$('captureNeutral').onclick=()=>action($('captureNeutral'),'escSaveStatus',async()=>{
 const sample=await request('/api/esc/state');
 const pulse=Number(sample.escPulse);
 if(!sample.escValid||!Number.isFinite(pulse)||pulse<1300||pulse>1700||!Number.isFinite(Number(sample.escAgeMs))||Number(sample.escAgeMs)>100)throw {code:'neutral_unavailable'};
 $('escForm').elements.escNeutralUs.value=Math.round(pulse);
 message('escSaveStatus',tr('Neutro acquisito. Premi SALVA CONFIGURAZIONE ESC.','Neutral captured. Press SAVE ESC CONFIGURATION.'));
});
$('readFullThrottle').onclick=()=>{if(!lightState.escEnabled){message('escSaveStatus','Attiva e salva prima «Strumenti dal gas (ESC)».',true);return;}$('fullThrottleCalibration').hidden=false;message('escSaveStatus',tr('Tieni tutto gas avanti, poi premi LEGGI E SALVA.','Hold full forward throttle, then press READ AND SAVE.'));};
$('cancelFullThrottle').onclick=()=>{$('fullThrottleCalibration').hidden=true;message('escSaveStatus',tr('Taratura annullata.','Calibration cancelled.'));};
$('captureFullThrottle').onclick=()=>action($('captureFullThrottle'),'escSaveStatus',async()=>{$('cancelFullThrottle').disabled=true;try{const readings=[];for(let i=0;i<5;i++){const state=await request('/api/esc/state');const pulse=Number(state.escPulse),neutral=Number(state.escNeutralUs);if(!state.escValid||state.escState!=='FORWARD'||!Number.isFinite(pulse)||!Number.isFinite(neutral)||pulse<800||pulse>2200||Math.abs(pulse-neutral)<200||!Number.isFinite(Number(state.escAgeMs))||Number(state.escAgeMs)>100){message('escSaveStatus',tr('Segnale non valido: tieni tutto gas AVANTI con le ruote libere e riprova.','Invalid signal: hold full FORWARD throttle with wheels free and retry.'),true);return;}readings.push(pulse);if(i<4)await new Promise(resolve=>setTimeout(resolve,100));}readings.sort((a,b)=>a-b);if(readings[4]-readings[0]>15){message('escSaveStatus',tr('Il gas si è mosso: tienilo al massimo e riprova.','Throttle moved: hold it at full and retry.'),true);return;}await request('/api/esc/calibrate-forward',{pulseUs:Math.round(readings[2])});await refreshLights('esc');$('fullThrottleCalibration').hidden=true;message('escSaveStatus',tr('Gas massimo calibrato e salvato. Ora rilascia il gas.','Full throttle calibrated and saved. Release throttle now.'));}finally{$('cancelFullThrottle').disabled=false;}});
$('escForm').onsubmit=e=>{e.preventDefault();const f=e.currentTarget;action(f.querySelector('.save'),'escSaveStatus',async()=>{
 const values=formValues(f);values.escEnabled=$('escEnabled').checked?'1':'0';values.escInverted=$('escInverted').checked?'1':'0';
 await request('/api/esc/config',values);manual='clear';await refreshLights('esc');await refreshTelemetry();
 message('escSaveStatus',tr('Configurazione ESC salvata. Profilo e luminosità luci invariati.','ESC configuration saved. Light profile and brightness unchanged.'));
});};
$('lightForm').onsubmit=e=>{e.preventDefault();const f=e.currentTarget;action(f.querySelector('.save'),'lightSaveStatus',async()=>{
 await request('/api/lights/config',formValues(f));manual='clear';await refreshLights('lights');
 message('lightSaveStatus',tr('Luci salvate. Configurazione ESC e calibrazione invariate.','Lights saved. ESC configuration and calibration unchanged.'));
});};
root.querySelectorAll('[data-rtl]').forEach(b=>b.onclick=()=>action(b,'lightStatus',async()=>{try{lightState=await request('/api/lights/command',{function:b.dataset.rtl});manual=b.dataset.rtl;renderLights();message('lightStatus',tr('Comando applicato.','Command applied.'));}catch(e){manual='clear';renderLights();throw e;}}));
for(const [id,theme]of [['alignButton','5'],['normalButton','4']])$(id).onclick=()=>action($(id),'alignmentStatus',async()=>{await request('/api/show',{action:'stop'});await request('/api/settings',{displayTheme:theme});message('alignmentStatus',theme==='5'?tr('16 punti accesi: verifica la maschera.','16 points lit: check the mask.'):tr('Grafica strumenti ripristinata.','Instrument display restored.'));});
$('profileButton').onclick=()=>action($('profileButton'),'hardwareStatus',async()=>{await request('/api/copilot-display',{profile:$('displayProfile').value});message('hardwareStatus',tr('Riavvio in corso. Riconnettiti alla rete stormDASH.','Restarting. Reconnect to the stormDASH network.'));});
$('diagnosticButton').onclick=()=>action($('diagnosticButton'),'diagnosticStatus',diagnostics);
$('hardwareDetails').ontoggle=()=>{if($('hardwareDetails').open&&!mutationBusy)loadDisplay().then(diagnostics).catch(e=>message('diagnosticStatus',errorText(e),true));};
$('bootDetails').ontoggle=()=>{if($('bootDetails').open&&!bootLoaded&&!mutationBusy)bootInfo().catch(e=>message('bootStatus',errorText(e),true));};
$('bootForm').onsubmit=e=>{e.preventDefault();action(e.currentTarget.querySelector('button'),'bootStatus',async()=>{await request('/api/settings',{bootImageEnabled:$('bootImageEnabled').checked?'1':'0',bootHoldMs:$('bootHoldMs').value});message('bootStatus',tr('Avvio aggiornato.','Startup updated.'));});};
function jpeg(image,w,h){const canvas=document.createElement('canvas');canvas.width=w;canvas.height=h;const c=canvas.getContext('2d');if(!c)throw Error('canvas');c.fillStyle='#000';c.fillRect(0,0,w,h);const factor=Math.min(w/image.width,h/image.height),dw=image.width*factor,dh=image.height*factor;c.drawImage(image,(w-dw)/2,(h-dh)/2,dw,dh);return new Promise((resolve,reject)=>canvas.toBlob(blob=>blob?resolve(blob):reject(Error('jpeg')),'image/jpeg',.88));}
async function prepareImage(file){if(!file||!['image/jpeg','image/png','image/webp'].includes(file.type))throw{code:'invalid_image'};if(file.size>10*1024*1024)throw{code:'too_large'};const url=URL.createObjectURL(file);try{const image=new Image();image.src=url;await image.decode();if(image.width*image.height>32000000)throw{code:'too_large'};return{a:await jpeg(image,160,80),b:await jpeg(image,displayState.logicalWidth,displayState.logicalHeight)};}finally{URL.revokeObjectURL(url);}}
$('uploadBoot').onclick=()=>action($('uploadBoot'),'bootStatus',async()=>{const file=$('bootFile').files[0];await loadDisplay();const pair=await prepareImage(file),tx=Array.from(crypto.getRandomValues(new Uint8Array(8)),x=>x.toString(16).padStart(2,'0')).join('');try{for(const side of ['a','b']){message('bootStatus',tr('Invio immagine ','Uploading image ')+side.toUpperCase()+' / 2');const form=new FormData();form.append('file',pair[side],'boot.jpg');await request('/api/upload/boot-'+side+'?tx='+tx,form,9000);}await request('/api/boot/commit',{tx});$('bootFile').value='';await bootInfo();message('bootStatus',tr('Immagine salvata per entrambi gli schermi.','Image saved for both screens.'));}catch(e){await request('/api/boot/abort',{tx},1500).catch(()=>{});throw e;}});
$('deleteBoot').onclick=()=>action($('deleteBoot'),'bootStatus',async()=>{await request('/api/boot/delete',{});await bootInfo();message('bootStatus',tr('Immagine rimossa.','Image removed.'));});
async function initialize(){
 if(mutationBusy)return;mutationBusy=true;
 for(const task of [loadSettings,loadIdentity,refreshLights,refreshTelemetry,loadDisplay])try{await task()}catch(e){}
 if($('hardwareDetails').open)try{await diagnostics()}catch(e){message('diagnosticStatus',errorText(e),true)}
 mutationBusy=false;connection();renderShow();schedulePoll(0);
}
$('retryButton').onclick=initialize;
document.addEventListener('visibilitychange',()=>{if(document.hidden){clearTimeout(pollTimer);clearManual();}else schedulePoll(0);});
window.addEventListener('pagehide',()=>{clearTimeout(pollTimer);clearManual();});
window.stormPit={connectionChanged(){connection();renderShow();},instrumentLabel(){return !telemetryLoaded?'Strumenti':telemetryState.telemetrySource==='esc_pwm_scenic'?'Strumenti · ESC':'Strumenti · DEMO'},setActive(value){
 active=value;
 if(!active){clearTimeout(pollTimer);clearManual();return;}
 if(!initialized){initialized=true;initialize();}else schedulePoll(0);
}};
if(typeof activeTab!=='undefined'&&activeTab==='pit')window.stormPit.setActive(true);

})();
