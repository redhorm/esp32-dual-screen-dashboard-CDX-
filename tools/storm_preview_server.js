#!/usr/bin/env node
'use strict';
// Local UI preview: actual native firmware state, simulated peripheral APIs.
// It binds only to loopback and never connects to, updates or flashes a board.
const fs=require('node:fs'),path=require('node:path'),os=require('node:os'),http=require('node:http');
const {spawn,execFileSync}=require('node:child_process'),{createInterface}=require('node:readline');
const root=path.resolve(__dirname,'..');
const names=['start','pause','resume','finish','gate','penalty','undo','rules','show','exit','game','jump','heartbeat','background','pit','board','midnight','living','liquid','garage','beat','steer','touch','impulse','explode','orbit','audio'];
function validValue(index,value){
  if(index===7)return value<=100;
  if(index===21)return value<=2;
  if(index===22||index===25)return value<=65535;
  if(index===11||index===12||index===24)return value<=1;
  if(index===10||index===16||index===26)return true;
  return value===0;
}
async function createPreview({port=0}={}){
  const tmp=fs.mkdtempSync(path.join(os.tmpdir(),'storm-preview-'));
  let engine,server,tick;
  const commands=[],errors=[],waiting=[],history=[];
  const lights={escValid:false,escPulse:0,escState:'neutral',escType:'forward_brake_reverse',escNeutralUs:1500,escDeadbandUs:45,brightness:96,profile:'ultra4_4400',escEnabled:false,escReverseConfirmMs:220,function:'running',hardwareReady:false,timedOut:false,escNormalized:0,pixels:Array.from({length:16},()=>[0,0,0])};
  let manualAt=0,showStarted=0;
  const settings={displayTheme:4,driverTheme:4,navigatorTheme:4,language:0,setupComplete:true,useKmh:true,useCelsius:true,driverBrightness:220,navigatorBrightness:220,bootImageEnabled:false,bootHoldMs:1800};
  const identity={vehicle:'STORM GARAGE',vehicleName:'STORM GARAGE',number:'047',driver:'PILOTA'};
  try{
    execFileSync('c++',['-std=c++11','-O2','-I'+path.join(root,'src'),path.join(root,'tools/wow_host.cpp'),'-o',path.join(tmp,'host')]);
    engine=spawn(path.join(tmp,'host'));const started=performance.now();let closing=false,unavailable=null;
    createInterface({input:engine.stdout}).on('line',line=>{const next=waiting.shift();if(next){try{next.resolve(JSON.parse(line))}catch(e){next.reject(e)}}});
    const fail=e=>{unavailable=e;for(const p of waiting.splice(0))p.reject(e)};
    engine.on('exit',code=>fail(Error('Native state process exited '+code)));
    engine.on('error',fail);engine.stdin.on('error',fail);
    function state(action=-1,value=0){return new Promise((resolve,reject)=>{if(closing||unavailable)return reject(unavailable||Error('Preview closed'));waiting.push({resolve,reject});engine.stdin.write(Math.floor(performance.now()-started)+' '+action+' '+value+'\n')})}
    const badge='<aside id="previewNotice" role="note" style="position:relative;z-index:101;padding:9px 16px;background:#f0eaff;color:#543b89;text-align:center;font:600 11px/1.5 -apple-system,BlinkMacSystemFont,sans-serif;letter-spacing:.08em">ANTEPRIMA LOCALE · COMANDI SIMULATI · NESSUNA SCHEDA COLLEGATA</aside>';
    function marked(page){const out=/<body\b/.test(page)?page.replace(/<body\b[^>]*>/,body=>body+badge):page.replace(/<header\b/,badge+'<header');if(!out.includes('id="previewNotice"'))throw Error('Cannot display the required preview disclosure');return out}
    const mainPage=()=>marked(fs.readFileSync(path.join(root,'src/services/storm_page.h'),'utf8').match(/R"STORM\(([\s\S]*)\)STORM";/)[1]);
    const advancedPage=()=>marked(fs.readFileSync(path.join(root,'src/services/settings_page.h'),'utf8').match(/R"HTML\(([\s\S]*)\)HTML";/)[1]);
    mainPage();advancedPage();
    const unsupported='<!doctype html><html lang="it"><meta name="viewport" content="width=device-width"><title>Anteprima stormDASH</title><body style="font:18px/1.5 system-ui;max-width:600px;margin:10vh auto;padding:24px;background:#f5f5f7;color:#1d1d1f"><h1>Solo anteprima.</h1><p>Questa funzione richiede il cruscotto. L’anteprima sul computer non accede a hardware, file o rete della scheda.</p><p><a href="/">Torna a stormDASH</a></p></body></html>';
    server=http.createServer(async(req,res)=>{try{
      const reply=(data,status=200,kind='application/json')=>{res.writeHead(status,{'Content-Type':kind,'Cache-Control':'no-store'});res.end(kind==='application/json'?JSON.stringify(data):data)};
      const route=new URL(req.url,'http://localhost').pathname;
      let body='';if(req.method==='POST'){for await(const chunk of req){body+=chunk;if(body.length>4096)return reply({error:'too_large'},413)}}
      const p=new URLSearchParams(body);
      if(route==='/api/storm'){
        if(req.method==='POST'){
          const index=names.indexOf(p.get('action')),raw=p.get('value')||'0',value=Number(raw);
          if(index<0||!/^\d{1,10}$/.test(raw)||!Number.isInteger(value)||value>0xffffffff||!validValue(index,value))return reply({error:'invalid'},400);
          const previous=await state();if(index===8)showStarted=Date.now();commands.push({action:names[index],value,at:Date.now()});
          if(index===3&&previous.session){history.unshift({duration:previous.elapsed,gates:previous.gates,penalties:previous.penalties,score:previous.score,penaltyWeight:previous.weight,test:true});history.splice(64)}
          await state(index,value);return reply({accepted:true},202);
        }
        const s=await state();s.history=history.length;return reply(s);
      }
      if(route==='/api/storm/results')return reply(history);
      if(route==='/api/storm/history')return reply('origin,duration_ms,gates,penalties,score,penalty_weight\n'+history.map(r=>['COMMISSIONING',r.duration,r.gates,r.penalties,r.score,r.penaltyWeight].join(',')).join('\n'),200,'text/csv; charset=utf-8');
      if(route==='/api/v3/rtl/state'){if(Date.now()-manualAt>1500){lights.function='running';lights.pixels=Array.from({length:16},()=>[0,0,0])}return reply(lights)}
      if(route==='/api/settings'){
        if(req.method==='POST'){
          for(const key of ['displayTheme','driverTheme','navigatorTheme','language','driverBrightness','navigatorBrightness','bootHoldMs'])if(p.has(key))settings[key]=Number(p.get(key));
          if(p.has('units'))settings.useKmh=p.get('units')==='metric';if(p.has('temperature'))settings.useCelsius=p.get('temperature')==='c';
          if(p.has('bootImageEnabled'))settings.bootImageEnabled=['1','true','on'].includes(p.get('bootImageEnabled'));
          if(p.has('displayTheme'))settings.driverTheme=settings.navigatorTheme=settings.displayTheme;
        }
        return reply(settings);
      }
      if(route==='/api/v3/state'||route==='/api/v3/profile'){
        if(req.method==='POST'){for(const key of ['vehicle','number','driver'])if(p.has(key))identity[key]=p.get(key).toUpperCase().replace(/[^A-Z0-9 ._-]/g,'').slice(0,key==='number'?6:24);identity.vehicleName=identity.vehicle}
        return reply({ok:true,preview:true,telemetrySource:'demo',identity});
      }
      if(route==='/api/copilot-display')return reply({ok:true,preview:true,profile:'240x135_landscape',logicalWidth:240,logicalHeight:135,controller:'ST7789',rotation:1});
      if(route==='/api/photos')return reply({ok:true,preview:true,fsReady:false,slots:[],bootDriver:false,bootNavigator:false,storageState:'unavailable'});
      if(route==='/api/show'){
        if(req.method==='POST'){if(!['start','stop'].includes(p.get('action')))return reply({error:'invalid_show_action'},400);if(p.get('action')==='start')showStarted=Date.now();await state(p.get('action')==='start'?8:9)}
        const current=await state(),active=current.mode===1;
        return reply({ok:true,preview:true,active,elapsedMs:active?Math.min(6000,Date.now()-showStarted):0,durationMs:6000,pending:false,requestedActive:active});
      }
      if(route==='/api/v3/rtl/config'&&req.method==='POST'){
        for(const key of ['escNeutralUs','escDeadbandUs','escReverseConfirmMs','brightness'])if(p.has(key))lights[key]=Number(p.get(key));
        for(const key of ['escType','profile'])if(p.has(key))lights[key]=p.get(key);
        lights.escEnabled=p.get('escEnabled')==='1';return reply({...lights,ok:true,preview:true});
      }
      if(route==='/api/v3/rtl/command'&&req.method==='POST'){
        const fn=p.get('function');if(!['left','right','brake','hazard','dust_chase','reverse_work','running','clear','pit_test'].includes(fn))return reply({error:'invalid_function'},400);
        if(fn==='pit_test')return reply({error:'esc_not_neutral'},409);
        manualAt=Date.now();lights.function=fn==='clear'?'running':fn;
        const color=fn==='brake'?[255,0,0]:fn==='reverse_work'?[240,240,220]:['left','right','hazard','dust_chase'].includes(fn)?[255,145,0]:[0,0,0];
        lights.pixels=Array.from({length:16},(_,i)=>fn==='left'&&i>7||fn==='right'&&i<8?[0,0,0]:color);
        return reply({...lights,ok:true,preview:true});
      }
      if(route==='/api/storm/hardware'||route==='/api/v4/diagnostics')return reply({preview:true,hardwareConnected:false,firmware:'ANTEPRIMA · nessuna scheda collegata',mode:'Modello firmware su computer',note:'Nessuna misura hardware disponibile in questa anteprima.'});
      if(route.startsWith('/api/'))return reply({error:'preview_not_available'},404);
      if(route==='/advanced')return reply(advancedPage(),200,'text/html; charset=utf-8');
      if(route==='/update'){const file=path.join(root,'output/stormDASH/update.html');return reply(fs.existsSync(file)?marked(fs.readFileSync(file,'utf8')):unsupported,200,'text/html; charset=utf-8')}
      if(route!=='/'&&route!=='/portal.html'&&route!=='/favicon.ico')return reply({error:'not_found'},404);
      if(route==='/favicon.ico'){res.writeHead(204);return res.end()}
      reply(mainPage(),200,'text/html; charset=utf-8');
    }catch(e){errors.push(String(e));if(!res.headersSent)res.writeHead(500);res.end()}});
    await new Promise((resolve,reject)=>{server.once('error',reject);server.listen(port,'127.0.0.1',resolve)});
    let ticking=false;
    tick=setInterval(async()=>{if(ticking||closing||unavailable)return;ticking=true;try{await state()}catch(e){if(!closing)errors.push(String(e))}finally{ticking=false}},20);
    async function close(){closing=true;clearInterval(tick);if(server){server.closeAllConnections();await new Promise(resolve=>server.close(resolve))}engine.kill();fs.rmSync(tmp,{recursive:true,force:true})}
    return{url:'http://127.0.0.1:'+server.address().port,state,commands,errors,close};
  }catch(error){clearInterval(tick);if(server)server.close();if(engine)engine.kill();fs.rmSync(tmp,{recursive:true,force:true});throw error}
}
module.exports={createPreview};
if(require.main===module){
  const i=process.argv.indexOf('--port'),port=i<0?8766:Number(process.argv[i+1]);
  if(!Number.isInteger(port)||port<1024||port>65535)throw Error('Use --port 1024…65535');
  createPreview({port}).then(preview=>{console.log('stormDASH local UI preview: '+preview.url+' · no hardware writes');for(const signal of ['SIGINT','SIGTERM'])process.once(signal,()=>preview.close().then(()=>process.exit(0)))}).catch(error=>{console.error(error);process.exitCode=1});
}
