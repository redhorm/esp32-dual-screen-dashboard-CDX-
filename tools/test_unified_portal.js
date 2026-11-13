const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {chromium,webkit}=require('playwright');
const root=path.resolve(__dirname,'..');const html=fs.readFileSync(path.join(root,'output/stormDASH/portal.html'),'utf8');
const evidence=path.resolve(process.env.STORM_TEST_OUTPUT||path.join(root,'output/test-evidence'));fs.mkdirSync(evidence,{recursive:true});
(async()=>{const browser=await (process.env.STORM_WEBKIT?webkit.launch({headless:true}):chromium.launch({channel:'chrome',headless:true}));try{
const page=await browser.newPage({viewport:{width:390,height:844}}),errors=[],posts=[];
page.on('pageerror',e=>errors.push(String(e)));
const settings={driverBrightness:220,navigatorBrightness:220,language:0,useKmh:true,bootImageEnabled:true,bootHoldMs:1800};
const light={ok:true,escValid:true,escEnabled:true,escInverted:true,escPulse:1500,escRawPulse:1500,escAgeMs:12,escState:'NEUTRAL',escType:'crawler_forward_reverse',escNeutralUs:1500,escDeadbandUs:45,escReverseConfirmMs:220,profile:'ultra4_4400',brightness:96,pixels:[],hardwareReady:true};
const storm={mode:0,session:false,running:false,elapsed:0,gates:0,penalties:0,score:0,weight:7,gameMs:0,hits:0,best:0,history:0,done:false};
let show=false,fail=false,missingState=false,badDiagnostics=false;let inFlight=0,maxInFlight=0;
const routes=new Set([...fs.readFileSync(path.join(root,'src/services/config_portal.cpp'),'utf8').matchAll(/_server\.on\("([^"]+)"/g),...fs.readFileSync(path.join(root,'src/services/storm_service.cpp'),'utf8').matchAll(/server\.on\("([^"]+)"/g)].map(m=>m[1]));
await page.route('http://127.0.0.1/**',async route=>{const req=route.request(),u=new URL(req.url()),p=u.pathname,args=Object.fromEntries(new URLSearchParams(req.postData()||''));
if(p==='/')return route.fulfill({contentType:'text/html',body:html});
if(p==='/advanced')return route.fulfill({status:302,headers:{location:'/#pit'}});
assert.ok(routes.has(p),'Browser requested a route absent from firmware: '+p);
inFlight++;maxInFlight=Math.max(maxInFlight,inFlight);await new Promise(r=>setTimeout(r,25));inFlight--;
if(missingState&&p==='/api/v4/state')return route.fulfill({status:404,json:{ok:false,error:'not found'}});
if(badDiagnostics&&p==='/api/v4/diagnostics')return route.fulfill({status:200,contentType:'application/json',body:'{truncated'});

if(req.method()==='POST'){posts.push({path:p,args});if(fail)return route.fulfill({status:503,json:{ok:false,error:'request'}});
if(p==='/api/settings')Object.assign(settings,args);
if(p==='/api/esc/config')Object.assign(light,{escEnabled:args.escEnabled==='1',escInverted:args.escInverted==='1',escNeutralUs:Number(args.escNeutralUs)});
if(p==='/api/lights/config')Object.assign(light,{brightness:Number(args.brightness),profile:args.profile});
if(p==='/api/esc/calibrate-forward'){light.escFullForwardCalibrated=true;light.escFullForwardUs=Number(args.pulseUs)}
if(p==='/api/show')show=args.action==='start';
}
let data={ok:true};
if(p==='/api/storm')data=storm;
else if(p==='/api/storm/results')data=[];
else if(p==='/api/settings')data=settings;
else if(p==='/api/v3/state')data={identity:{vehicle:'RECOIL',number:'4400',driver:'LUCA'}};
else if(p.startsWith('/api/esc/')||p.startsWith('/api/lights/'))data=light;
else if(p==='/api/v4/state')data={telemetrySource:light.escEnabled?'esc_pwm_scenic':'demo',escSignalValid:true};
else if(p==='/api/copilot-display')data={profile:'240x135_landscape',logicalWidth:240,logicalHeight:135};
else if(p==='/api/show')data={active:show,elapsedMs:0};
else if(p==='/api/photos')data={fsReady:true,bootDriver:false,bootNavigator:false};
else if(p==='/api/v4/diagnostics')data={firmware:'7.1.0-rc.7-esc-lights',displayA:{ok:true,fps:30},displayB:{ok:true,fps:15},storage:{ready:true},memory:{heapFreeBytes:120000},lastError:'none'};
return route.fulfill({json:data});});
await page.goto('http://127.0.0.1/');
await page.waitForFunction(()=>document.querySelector('#status').dataset.online==='true');
assert.equal(await page.locator('nav').count(),1);assert.equal(await page.locator('iframe,[role=tablist],a[href="/advanced"]').count(),0);
assert.equal(await page.evaluate(()=>{const ids=[...document.querySelectorAll('[id]')].map(e=>e.id);return ids.length===new Set(ids).size}),true);
for(const tab of ['home','trail','arcade','wow','pit']){await page.locator('[data-tab="'+tab+'"]').click();assert.equal(await page.locator('main>section:not(.hidden)').count(),1)}
await page.waitForFunction(()=>document.querySelector('#pit-connection').textContent==='DATI AGGIORNATI');
// Both summaries share the transport state, even with previously loaded data.
await page.evaluate(()=>{
  connected(false);
  const link=document.getElementById('pit-connection');
  if(!link.textContent.includes('CONNESSIONE PERSA')||link.classList.contains('online')||document.getElementById('pit-retryButton').hidden||!document.getElementById('pit-showButton').disabled)throw Error('Pit retained a stale online status');
  connected(true);
  if(link.textContent!=='DATI AGGIORNATI'||!link.classList.contains('online'))throw Error('Pit did not recover with the main connection');
});

assert.equal(await page.locator('#pit-escInverted').isChecked(),true);
await page.locator('#pit-escForm details>summary').click();
light.escValid=false;
await page.locator('#pit-captureNeutral').click();
await page.waitForFunction(()=>document.querySelector('#pit-escSaveStatus').classList.contains('error'));
assert.equal(await page.locator('#pit-escForm [name=escNeutralUs]').inputValue(),'1500');
light.escValid=true;light.escPulse=1508;
await page.locator('#pit-captureNeutral').click();
await page.waitForFunction(()=>document.querySelector('#pit-escForm [name=escNeutralUs]').value==='1508');
await page.locator('#pit-escForm [name=escNeutralUs]').fill('1500');
await page.locator('#pit-escForm details>summary').click();

await page.locator('#pit-readFullThrottle').click();light.escPulse=1040;light.escState='FORWARD';
await page.locator('#pit-captureFullThrottle').click();await page.waitForFunction(()=>document.querySelector('#pit-escSaveStatus').textContent.includes('calibrato e salvato'));
assert.ok(posts.some(p=>p.path==='/api/esc/calibrate-forward'&&p.args.pulseUs==='1040'));
await page.locator('#pit-escForm .save').click();await page.waitForFunction(()=>document.querySelector('#pit-escSaveStatus').textContent.includes('Configurazione ESC salvata'));
assert.ok(posts.some(p=>p.path==='/api/esc/config'&&p.args.escInverted==='1'&&p.args.escType==='crawler_forward_reverse'&&p.args.escNeutralUs==='1500'));

// Lights form sends no ESC fields, even when unsaved throttle edits exist.
await page.locator('#pit-escForm details>summary').click();
await page.locator('#pit-escForm [name=escNeutralUs]').fill('1510');
await page.locator('#pit-rtlBrightness').fill('123');
await page.locator('#pit-lightForm .save').click();
await page.waitForFunction(()=>document.querySelector('#pit-lightSaveStatus').textContent.includes('Luci salvate'));
const lp=posts.filter(p=>p.path==='/api/lights/config').at(-1);assert.deepEqual(Object.keys(lp.args).sort(),['brightness','profile']);assert.equal(light.escNeutralUs,1500);
assert.equal(await page.locator('#pit-escForm [name=escNeutralUs]').inputValue(),'1510');
await page.locator('#pit-escForm [name=escNeutralUs]').fill('1500');
await page.locator('#pit-escForm details>summary').click();
await page.locator('#pit-escEnabled').uncheck();await page.locator('#pit-escForm .save').click();
await page.waitForFunction(()=>document.querySelector('#pit-escSummary').textContent.includes('DEMO'));
assert.equal(light.brightness,123);assert.equal(light.profile,'ultra4_4400');
const ep=posts.filter(p=>p.path==='/api/esc/config').at(-1);assert.ok(!('brightness' in ep.args)&&!('profile' in ep.args));
assert.ok((await page.locator('#pit-pwmStatus').textContent()).includes('1040'));
await page.locator('#pit-escEnabled').check();await page.locator('#pit-escForm .save').click();await page.waitForFunction(()=>document.querySelector('#pit-escSummary').textContent.includes('attivi'));
// A missing service is an HTTP error, not Wi-Fi loss; diagnostics stays usable.
missingState=true;await page.locator('#pit-hardwareDetails summary').click();
await page.waitForFunction(()=>document.querySelector('#pit-diagnosticOutput').textContent.includes('fps'));
await page.waitForFunction(()=>document.querySelector('#pit-globalStatus').textContent.includes('HTTP 404'));
assert.equal(await page.locator('#status').getAttribute('data-online'),'true');
assert.ok(!(await page.locator('#pit-connection').textContent()).includes('CONNESSIONE PERSA'));
missingState=false;await page.locator('#pit-retryButton').click();await page.waitForFunction(()=>document.querySelector('#pit-connection').textContent==='DATI AGGIORNATI');
badDiagnostics=true;await page.locator('#pit-diagnosticButton').click();
await page.waitForFunction(()=>document.querySelector('#pit-diagnosticStatus').textContent.includes('risposta non valida'));
assert.equal(await page.locator('#status').getAttribute('data-online'),'true');
badDiagnostics=false;await page.locator('#pit-diagnosticButton').click();await page.waitForFunction(()=>document.querySelector('#pit-diagnosticStatus').textContent==='Aggiornato');
assert.equal(maxInFlight,1,'Portal and Pit must not overlap HTTP requests');

await page.locator('#pit-driverBrightness').fill('180');await page.locator('#pit-displayForm .save').click();await page.waitForFunction(()=>document.querySelector('#pit-displayStatus').textContent.includes('aggiornati'));
assert.ok(posts.some(p=>p.path==='/api/settings'&&p.args.driverBrightness==='180'));
await page.locator('[data-rtl="left"]').click();await page.waitForFunction(()=>document.querySelector('[data-rtl="left"]').classList.contains('active'));
await page.locator('[data-tab="home"]').click();await page.waitForTimeout(100);assert.ok(posts.some(p=>p.path==='/api/lights/command'&&p.args.function==='clear'));
await page.locator('[data-tab="pit"]').click();fail=true;await page.locator('#pit-displayForm .save').click();await page.waitForFunction(()=>document.querySelector('#pit-displayStatus').classList.contains('error'));fail=false;
for(const width of [320,390,1120]){await page.setViewportSize({width,height:844});assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));await page.screenshot({path:path.join(evidence,'pit-'+width+'.png'),fullPage:true});}
assert.ok(fs.readFileSync(path.join(root,'src/services/config_portal.cpp'),'utf8').includes('_server.sendHeader("Location", "/#pit")'));await page.goto('http://127.0.0.1/#pit');await page.reload();await page.waitForFunction(()=>!document.querySelector('#pit').classList.contains('hidden'));
assert.deepEqual(errors,[]);console.log('PASS: firmware route contract, shared HTTP queue, independent ESC/lights payloads and drafts, diagnostic recovery, 404 is not Wi-Fi loss, one menu, five sections, no duplicate IDs/iframe, ESC calibration, direction preserved, settings save, error feedback, clear manual lights on exit, 320/390/1120px, legacy redirect, zero JS errors.');
}finally{await browser.close()}})().catch(e=>{console.error(e);process.exitCode=1});
