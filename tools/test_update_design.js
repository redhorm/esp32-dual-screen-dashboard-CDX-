// Fixture-only verification: never connects to or uploads to a physical ESP32.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),{chromium}=require('playwright');
const root=path.resolve(__dirname,'..'),html=fs.readFileSync(path.join(root,'output/stormDASH/update.html'),'utf8');
const evidence=path.resolve(process.env.STORM_TEST_OUTPUT||path.join(root,'output/test-evidence'));fs.mkdirSync(evidence,{recursive:true});
(async()=>{const browser=await chromium.launch({channel:'chrome',headless:true});try{
 const page=await browser.newPage(),errors=[];page.on('pageerror',e=>errors.push(String(e)));
 await page.route('http://127.0.0.1/**',r=>r.fulfill({contentType:'text/html',body:html.replaceAll('{{TOKEN}}','fixture-token')}));
 for(const theme of ['dark','light'])for(const width of [320,390,768,1440]){
  await page.setViewportSize({width,height:844});await page.goto('http://127.0.0.1/update');
  if(await page.locator('body').getAttribute('data-theme')!==theme)await page.locator('#themeToggle').click();
  assert.equal(await page.locator('body').getAttribute('data-theme'),theme);
  assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'update width '+width);
  const form=page.locator('form');assert.equal(await form.getAttribute('action'),'/api/storm/update?token=fixture-token');assert.equal(await form.getAttribute('method'),'post');assert.equal(await form.getAttribute('enctype'),'multipart/form-data');
  await page.locator('#firmware').setInputFiles({name:'firmware.bin',mimeType:'application/octet-stream',buffer:Buffer.from('fixture')});assert.equal(await page.locator('#firmware').evaluate(e=>e.files[0].name),'firmware.bin');
  if(width===390||width===1440)await page.screenshot({path:path.join(evidence,'update-'+theme+'-'+width+'.png'),fullPage:true});
 }
 assert.deepEqual(errors,[]);console.log('PASS: updater8viewport/theme states, native upload form+CSRF token unchanged, selected file, no JS errors. No physical upload.');
}finally{await browser.close()}})().catch(e=>{console.error(e);process.exitCode=1});
