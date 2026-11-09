const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {chromium,webkit}=require('playwright');
const root=path.resolve(__dirname,'..');const html=fs.readFileSync(path.join(root,'output/stormDASH/portal.html'),'utf8');
const evidence=path.resolve(process.env.STORM_TEST_OUTPUT||path.join(root,'output/test-evidence'));fs.mkdirSync(evidence,{recursive:true});
(async()=>{const browser=await (process.env.STORM_WEBKIT?webkit.launch({headless:true}):chromium.launch({channel:'chrome',headless:true}));try{
const page=await browser.newPage({viewport:{width:390,height:844}}),errors=[],posts=[],report=[];
page.on('pageerror',e=>errors.push(String(e)));
const settings={driverBrightness:220,navigatorBrightness:220,language:0,useKmh:true,bootImageEnabled:true,bootHoldMs:1800};
const light={ok:true,escValid:true,escEnabled:true,escInverted:true,escPulse:1500,escRawPulse:1500,escAgeMs:12,escState:'NEUTRAL',escType:'crawler_forward_reverse',escNeutralUs:1500,escDeadbandUs:45,escReverseConfirmMs:220,profile:'ultra4_4400',brightness:96,pixels:[],hardwareReady:true};
const storm={mode:0,session:false,running:false,elapsed:0,gates:0,penalties:0,score:0,weight:7,gameMs:0,hits:0,best:0,history:0,done:false,wow:{ms:0,elapsed:0,score:0,best:0,hits:0,seed:1,paused:false,done:false,audioActive:false,exploded:0,bands:[0,0,0,0]}};
let show=false,fail=false,missingState=false,badDiagnostics=false;let inFlight=0,maxInFlight=0;
const routes=new Set([...fs.readFileSync(path.join(root,'src/services/config_portal.cpp'),'utf8').matchAll(/_server\.on\("([^"]+)"/g),...fs.readFileSync(path.join(root,'src/services/storm_service.cpp'),'utf8').matchAll(/server\.on\("([^"]+)"/g)].map(m=>m[1]));
await page.route('http://127.0.0.1/**',async route=>{const req=route.request(),u=new URL(req.url()),p=u.pathname,args=Object.fromEntries(new URLSearchParams(req.postData()||''));
if(p==='/')return route.fulfill({contentType:'text/html',body:html});
if(p==='/advanced')return route.fulfill({status:302,headers:{location:'/#pit'}});
assert.ok(routes.has(p),'Browser requested a route absent from firmware: '+p);
const queued=!(p==='/api/storm'&&args.action==='background');if(queued){inFlight++;maxInFlight=Math.max(maxInFlight,inFlight)}await new Promise(r=>setTimeout(r,25));if(queued)inFlight--;
if(missingState&&p==='/api/v4/state')return route.fulfill({status:404,json:{ok:false,error:'not found'}});
if(badDiagnostics&&p==='/api/v4/diagnostics')return route.fulfill({status:200,contentType:'application/json',body:'{truncated'});

if(req.method()==='POST'){posts.push({path:p,args});if(fail)return route.fulfill({status:503,json:{ok:false,error:'request'}});
if(p==='/api/settings')Object.assign(settings,args);
if(p==='/api/esc/config')Object.assign(light,{escEnabled:args.escEnabled==='1',escInverted:args.escInverted==='1',escNeutralUs:Number(args.escNeutralUs)});
if(p==='/api/lights/config')Object.assign(light,{brightness:Number(args.brightness),profile:args.profile});
if(p==='/api/esc/calibrate-forward'){light.escFullForwardCalibrated=true;light.escFullForwardUs=Number(args.pulseUs)}
if(p==='/api/show')show=args.action==='start';
}
if(req.method()==='POST'&&p==='/api/storm'){
 const modes={exit:0,show:1,game:2,pit:3,board:4,midnight:5,living:6,liquid:7,garage:8,beat:9};
 if(args.action in modes)storm.mode=modes[args.action];
 if(args.action==='start'){storm.session=true;storm.running=true;}
 if(args.action==='pause')storm.running=false;
 if(args.action==='resume')storm.running=true;
 if(args.action==='finish'){storm.session=false;storm.running=false;}
 if(args.action==='gate')storm.gates++;
 if(args.action==='penalty')storm.penalties++;
 if(args.action==='rules')storm.weight=Number(args.value);
 if(args.action==='explode')storm.wow.exploded=Number(args.value);
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

await page.emulateMedia({reducedMotion:'reduce',colorScheme:'dark'});
await page.goto('http://127.0.0.1/');
await page.waitForFunction(()=>document.querySelector('#status').dataset.online==='true');
assert.equal(await page.locator('nav').count(),1,'Exactly one navigation');
assert.equal(await page.locator('#themeToggle').count(),1,'Theme button exists');
assert.equal(await page.evaluate(()=>new Set([...document.querySelectorAll('[id]')].map(e=>e.id)).size===document.querySelectorAll('[id]').length),true,'Unique IDs');
const initialTheme=await page.locator('body').getAttribute('data-theme');
assert.ok(['dark','light'].includes(initialTheme),'Theme is explicit');
await page.locator('#themeToggle').click();
const changedTheme=await page.locator('body').getAttribute('data-theme');
assert.notEqual(changedTheme,initialTheme,'Theme toggle works');
assert.equal(await page.evaluate(()=>localStorage.getItem('storm-theme')),changedTheme,'Theme persisted locally');
await page.reload();await page.waitForFunction(()=>document.querySelector('#status').dataset.online==='true');
assert.equal(await page.locator('body').getAttribute('data-theme'),changedTheme,'Reload preserves theme');
const allIssues=[];
for(const theme of ['dark','light']){
 if(await page.locator('body').getAttribute('data-theme')!==theme)await page.locator('#themeToggle').click();
 for(const width of [320,390,768,1440]){
  await page.setViewportSize({width,height:width>768?1000:844});
  for(const tab of ['home','trail','arcade','wow','pit']){
   await page.locator('[data-tab="'+tab+'"]').click();
   if(tab==='pit')await page.waitForFunction(()=>document.querySelector('#pit-connection').textContent==='DATI AGGIORNATI');
   await page.waitForTimeout(80);
   assert.equal(await page.locator('main>section:not(.hidden)').count(),1);
   assert.equal(await page.locator('[data-tab="'+tab+'"]').getAttribute('aria-current'),'page');
   const audit=await page.evaluate(()=>{
    const visible=e=>{if(!e.getClientRects().length||getComputedStyle(e).visibility==='hidden')return false;let q=e.parentElement;while(q){if(q.tagName==='DETAILS'&&!q.open&&!q.querySelector(':scope > summary')?.contains(e))return false;q=q.parentElement}return true};
    const name=e=>e.getAttribute('aria-label')||e.getAttribute('aria-labelledby')||e.innerText?.trim()||(e.labels&&[...e.labels].map(l=>l.textContent.trim()).join(' '))||e.title;
    const missingNames=[...document.querySelectorAll('button,input:not([type=hidden]),select,summary,[role=slider]')].filter(visible).filter(e=>!name(e)).map(e=>e.id||e.outerHTML.slice(0,100));
    const clipped=[...document.querySelectorAll('main *,header *,nav *')].filter(visible).filter(e=>{const r=e.getBoundingClientRect();return r.right>innerWidth+1||r.left< -1}).filter(e=>!e.closest('.wow-grid')).map(e=>e.id||e.className).filter(x=>typeof x==='string').slice(0,10);
    const tiny=[...document.querySelectorAll('button,input,select,summary')].filter(visible).filter(e=>{const r=(e.type==='checkbox'&&e.labels?.length?e.labels[0]:e).getBoundingClientRect();return !e.disabled&&(r.width<43||r.height<43)}).map(e=>({id:e.id,text:e.innerText.slice(0,30),width:Math.round(e.getBoundingClientRect().width),height:Math.round(e.getBoundingClientRect().height)}));
    const rgba=s=>{const a=s.match(/[\d.]+/g);return a?[+a[0],+a[1],+a[2],a[3]===undefined?1:+a[3]]:[0,0,0,0]};
    const over=(a,b)=>[0,1,2].map(i=>a[i]*a[3]+b[i]*(1-a[3]));
    const lum=c=>c.slice(0,3).map(x=>x/255).map(x=>x<=.04045?x/12.92:((x+.055)/1.055)**2.4).reduce((s,v,i)=>s+v*[.2126,.7152,.0722][i],0);
    const contrast=[];
    for(const e of document.querySelectorAll('main *,header *,nav *,footer *')){
     if(!visible(e)||e.closest(':disabled')||!Array.from(e.childNodes).some(n=>n.nodeType===3&&n.textContent.trim()))continue;
     const chain=[];let q=e,gradient=false;while(q){const c=getComputedStyle(q);if(c.backgroundImage!=='none'||+c.opacity<1)gradient=true;chain.unshift(c);q=q.parentElement;}if(gradient)continue;
     let bg=[255,255,255];for(const c of chain)bg=over(rgba(c.backgroundColor),bg);
     const c=getComputedStyle(e),fg=over(rgba(c.color),bg),a=lum(fg),b=lum(bg),ratio=(Math.max(a,b)+.05)/(Math.min(a,b)+.05);
     const min=parseFloat(c.fontSize)>=24||(parseFloat(c.fontSize)>=18.66&&parseFloat(c.fontWeight)>=700)?3:4.5;
     if(ratio<min-.05)contrast.push({id:e.id,text:e.innerText.slice(0,60),ratio:Number(ratio.toFixed(2)),min});
    }
    return {overflow:document.documentElement.scrollWidth>innerWidth,missingNames,clipped,tiny,contrast};
   });
   report.push({theme,width,tab,...audit});
   if(audit.overflow||audit.missingNames.length)allIssues.push({theme,width,tab,...audit});
   if([390,1440].includes(width))await page.screenshot({path:path.join(evidence,`${tab}-${theme}-${width}.png`),fullPage:true});
  }
 }
}
fs.writeFileSync(path.join(evidence,process.env.STORM_WEBKIT?'redesign-audit-webkit.json':'redesign-audit.json'),JSON.stringify({report,errors,allIssues},null,2));
// Browser keyboard focus must remain visible in both themes.
for(const theme of ['dark','light']){
 if(await page.locator('body').getAttribute('data-theme')!==theme)await page.locator('#themeToggle').click();
 await page.locator('#themeToggle').focus();
 await page.keyboard.press('Tab');
 const focus=await page.evaluate(()=>{const e=document.activeElement,c=getComputedStyle(e);return {tag:e.tagName,visible:e.matches(':focus-visible'),outline:c.outlineStyle,width:parseFloat(c.outlineWidth)}});
 assert.equal(focus.visible,true,'Keyboard focus-visible');assert.notEqual(focus.outline,'none','Keyboard outline present');assert.ok(focus.width>=2,'Focus outline at least 2px');
}
// Use real DOM interactions after the visual audit.
await page.locator('[data-tab="trail"]').click();await page.locator('#start').click();
await page.waitForFunction(()=>document.querySelector('#trailState').textContent==='Sessione in corso');
await page.locator('#gate').click();await page.waitForFunction(()=>document.querySelector('#gates').textContent==='1');
await page.locator('#pause').click();await page.waitForFunction(()=>!document.querySelector('#resume').classList.contains('hidden'));
await page.locator('#finish').click();await page.waitForFunction(()=>!document.querySelector('#start').classList.contains('hidden'));
await page.locator('[data-tab="arcade"]').click();await page.locator('#game').click();await page.waitForFunction(()=>!document.querySelector('#jump').disabled);
await Promise.all([page.waitForResponse(r=>r.request().method()==='POST'&&r.request().postData().includes('action=jump')),page.locator('#jump').click()]);assert.ok(posts.some(p=>p.args.action==='jump'));
await page.locator('[data-tab="wow"]').click();
for(const name of ['midnight','living','liquid','garage','beat']){
 await page.locator('[data-wow="'+name+'"]').click();
 await page.waitForFunction(n=>!document.querySelector('#'+n+'Panel').classList.contains('hidden'),name);
 assert.equal(await page.locator('[data-wow="'+name+'"]').getAttribute('aria-pressed'),'true');
}
await page.locator('[data-tab="pit"]').click();
await page.waitForFunction(()=>document.querySelector('#pit-connection').textContent==='DATI AGGIORNATI');
await page.locator('#pit-lightForm .save').click();await page.waitForFunction(()=>document.querySelector('#pit-lightSaveStatus').textContent.includes('Luci salvate'));
await page.locator('#pit-escForm .save').click();await page.waitForFunction(()=>document.querySelector('#pit-escSaveStatus').textContent.includes('Configurazione ESC salvata'));
assert.equal(maxInFlight,1,'Shared HTTP queue preserved');
assert.deepEqual(errors,[],'No JavaScript errors');
const motion=await page.evaluate(()=>[...document.querySelectorAll('button,nav,.card')].filter(e=>e.getClientRects().length).map(e=>getComputedStyle(e).transitionDuration).filter(d=>d.split(',').some(v=>parseFloat(v)>0.01)));
assert.deepEqual(motion,[],'Reduced motion honored');
fs.writeFileSync(path.join(evidence,process.env.STORM_WEBKIT?'redesign-audit-webkit.json':'redesign-audit.json'),JSON.stringify({report,errors,allIssues},null,2));
assert.deepEqual(allIssues,[],'Responsive layout and accessible names');
console.log('PASS: 40 responsive/theme/page states, clean screenshots, theme persistence, focus visibility, accessible control names, reduced motion, Trail/Arcade/WOW/Pit interactions, ESC/lights separation, serialized HTTP, no JS errors.');
}finally{await browser.close()}})().catch(e=>{console.error(e);process.exitCode=1});
