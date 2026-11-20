#!/usr/bin/env python3
"""Browser regression using real BrickOS web code + synthetic ECU transport; NOT physical ECU verification."""
from pathlib import Path
import json, re
from playwright.sync_api import sync_playwright

ROOT=Path(__file__).resolve().parents[1]
INI=(ROOT/'tests/fixtures/proteus_f7_generated.ini').read_text(errors='replace')
SIG='rusEFI master.2026.11.09.proteus_f7.2735508644'
core=(ROOT/'web/core.mjs').read_text().replace('export ', '')
app=(ROOT/'web/app.js').read_text()
app=re.sub(r'^import\s+\{[^\n]+\}\s+from\s+["\'][^"\']+["\'];\s*','',app,count=1)
html=(ROOT/'web/index.html').read_text().replace('<link rel="stylesheet" href="/app.css">','<style>'+ (ROOT/'web/app.css').read_text()+'</style>')
mock=r'''
const __ini=__INSERT_INI__;
const __sig=__INSERT_SIG__;
const __prefs={};
Object.defineProperty(window,'localStorage',{configurable:true,value:{getItem(k){return __prefs[k]??null},setItem(k,v){__prefs[k]=String(v)},removeItem(k){delete __prefs[k]}}});
const __pages=[17816,256,2048,1268,48000].map(x=>new Uint8Array(x));
const __mem=(p,o,v)=>new DataView(__pages[p].buffer).setUint16(o,v,true);
for(let i=0;i<256;i++)__mem(0,8964+i*2,85+i%16);
for(let i=0;i<16;i++){__mem(0,9508+i*2,700+i*250);__mem(0,9476+i*2,20+i*10);}
window.__mock={armed:false,writes:[],burns:[],readCount:0,seq:0,runtimeWrites:true,failNextRead:false,failNextWrite:false,backups:[],backupReads:0,downloads:[],backupStorageFail:false};

const __response=(value,ok=true)=>({ok,status:ok?200:403,json:async()=>value,text:async()=>String(value),arrayBuffer:async()=>value.buffer,blob:async()=>new Blob([value],{type:'application/octet-stream'})});
const __toBytes=h=>Uint8Array.from((h.match(/../g)||[]).map(x=>parseInt(x,16)));
const __hex=b=>Array.from(b,x=>x.toString(16).padStart(2,'0')).join('');
const __u16=(b,o)=>new DataView(b.buffer,b.byteOffset,b.byteLength).getUint16(o,true);
window.fetch=async function(path,opts={}){
 let url=String(path);
 if(url==='/api/status')return __response({ok:true,usb_device_ready:true,ecu_session_connected:true,ecu_signature:__sig,ini_cached:true,brick_os_build:__mock.runtimeWrites?'v4.5':'v4.1',write_compiled:__mock.runtimeWrites,write_access_ready:__mock.runtimeWrites,write_armed:__mock.armed,write_mode:'READY — ARM IN EDITOR',live_actual_hz:80.0,live_sample_duration_us:2100,live_sample_bytes:512,storage:'synthetic',ok_requests:90,failed_requests:0});
 if(url==='/api/ini/file')return __response(__ini);
 if(url.startsWith('/api/backup/get?page='))return __response(__mock.backups.at(-1));
 if(url==='/api/backup'){
   const b=JSON.parse(opts.body||'{}'),p=b.page-1;
   if(b.signature!==__sig||!__pages[p]||b.size!==__pages[p].length)return __response({ok:false,error:'Invalid backup request'},false);
   __mock.backupReads++;
   if(__mock.backupStorageFail)return __response({ok:false,error:'ESP32 backup storage full'},false);
   if(__mock.failNextRead){__mock.failNextRead=false;throw new TypeError('Load failed');}
   const snapshot=__pages[p].slice();__mock.backups.push(snapshot);
   return __response({ok:true,page:b.page,length:snapshot.length,crc32:crc32(snapshot),stored:'ESP32_STORAGE'});
 }

 if(url==='/api/logs')return __response({ok:true,files:[]});
 if(url.startsWith('/api/live')){
  const n=Number(new URL('http://mock.test'+url).searchParams.get('length')||384);
  const b=new Uint8Array(16+n),d=new DataView(b.buffer);
  b.set([66,76,86,49]);d.setUint32(4,++__mock.seq,true);d.setUint32(8,Date.now()>>>0,true);d.setUint16(12,n,true);d.setUint16(14,2,true);
  return __response(b);
 }
 let req={};try{req=JSON.parse(opts.body||'{}')}catch{}
 if(url==='/api/arm'){
  if(req.signature!==__sig)return __response({ok:false,error:'Wrong ECU'},false);
  __mock.armed=true;return __response({ok:true,armed_seconds:60});
 }
 if(url==='/api/disarm'){__mock.armed=false;return __response({ok:true});}
 if(url==='/api/ts'){
  const a=__toBytes(req.args||''),cmd=req.cmd;
  if(cmd==='S')return __response({ok:true,hex:__hex(new TextEncoder().encode(__sig)),status:0});
  if(cmd==='V')return __response({ok:true,hex:__hex(new TextEncoder().encode('bench simulator')),status:0});
  if(cmd==='e')return __response({ok:true,hex:'',status:0});
  if(cmd==='O'){const n=__u16(a,2);return __response({ok:true,hex:__hex(new Uint8Array(n)),status:0});}
  if(cmd==='R'){
   const p=__u16(a,0)>>8,o=__u16(a,2),n=__u16(a,4);
   __mock.readCount++;
   if(__mock.failNextRead){__mock.failNextRead=false;throw new TypeError('Load failed');}
   if(!__pages[p]||o+n>__pages[p].length)return __response({ok:false,error:'Bad mock read'},false);
   return __response({ok:true,hex:__hex(__pages[p].subarray(o,o+n)),status:0});
  }
  if(cmd==='C'){
   if(__mock.failNextWrite){__mock.failNextWrite=false;throw new TypeError('Load failed');}
   if(!__mock.armed||req.signature!==__sig)return __response({ok:false,error:'Not armed'},false);
   const p=__u16(a,0)>>8,o=__u16(a,2),n=__u16(a,4);
   if(n!==a.length-6)return __response({ok:false,error:'Bad mock length'},false);
   __pages[p].set(a.subarray(6),o);__mock.writes.push([p,o,n]);
   return __response({ok:true,hex:'',status:0});
  }
  if(cmd==='B'){
   if(!__mock.armed||req.signature!==__sig)return __response({ok:false,error:'Not armed'},false);
   __mock.burns.push(__u16(a,0)>>8);
   return __response({ok:true,hex:'',status:4});
  }
  return __response({ok:true,hex:'',status:0});
 }
 return __response({ok:false,error:'Unsupported path: '+url},false);
};
'''
mock=mock.replace('__INSERT_INI__',json.dumps(INI).replace('<','\\u003c')).replace('__INSERT_SIG__',json.dumps(SIG))
html=html.replace('<script type="module" src="/app.js"></script>','<script>\n'+mock+'\n'+core+'\n'+app+'\n</script>')

with sync_playwright() as p:
 browser=p.chromium.launch(executable_path='/usr/bin/chromium',headless=True,args=['--no-sandbox','--disable-dev-shm-usage'])
 context=browser.new_context(viewport={'width':1440,'height':900},accept_downloads=True)
 page=context.new_page();errors=[];downloads=[];page.on('pageerror',lambda e:errors.append(str(e)));page.on('download',lambda d:downloads.append(d.suggested_filename))
 page.on('dialog',lambda d:d.accept())
 page.set_content(html,wait_until='domcontentloaded')
 page.wait_for_selector('[data-instrument]',timeout=15000)
 page.locator('#nav [data-route="tuning"]').click()
 page.wait_for_selector('.tune-category',timeout=15000)
 # Open exact generated table directly via real UI route, not a fake editor.
 page.evaluate('()=>{S.route="tables";render()}')
 page.wait_for_selector('[data-load-field="veTable"]',timeout=10000)
 page.locator('[data-load-field="veTable"]').first.click()
 page.wait_for_selector('[data-axis-cell="x"]',timeout=15000)
 assert page.locator('[data-axis-cell="x"]').count()==16,'X axis was not resolved'
 assert page.locator('[data-axis-cell="y"]').count()==16,'Y axis was not resolved'
 page.locator('[data-axis-cell="x"]').first.click();page.locator('[data-action="inc1"]').click()
 page.locator('[data-axis-cell="y"]').first.click();page.locator('[data-action="inc1"]').click()
 page.locator('[data-cell="0"]').first.click();page.locator('[data-action="inc5"]').click()
 assert page.locator('[data-action="burn"]').is_enabled(),'Changes were not staged'
 # Open/close a different map without losing any of the three staged changes.
 page.locator('[data-action="close-editor"]').click()
 page.wait_for_selector('[data-load-field="veTable"]')
 page.locator('[data-load-field="veTable"]').first.click()
 page.wait_for_selector('[data-axis-cell="x"]')
 assert len(page.evaluate('staged()'))==3,'Staged fields were lost upon map navigation'
 # Regression: display polling can be stale/unavailable while the ECU is still online.
 # The direct USB preflight must control write authorization, not cached dashboard data.
 page.evaluate('()=>{S.latest=null; S.latestMs=0; S.liveBusy=true;}')
 assert page.evaluate('S.latest===null && S.latestMs===0')
 page.screenshot(path=str(ROOT/'preview_v39_tuning.png'),full_page=True)
 # Safari-safe: persist backup first, without opening a download dialog mid-write.
 # Inject a real iOS Safari-style transient Load failed on an R request;
 # only idempotent R/O are retried, never C or B.
 page.evaluate('()=>{__mock.failNextRead=true;localStorage.setItem=()=>{throw new Error("QuotaExceededError")};}')
 page.locator('[data-action="burn"]').click()
 page.wait_for_function('window.__mock.backups.length>=1',timeout=12000)
 page.wait_for_function('window.__mock.burns.length===1',timeout=15000)
 page.wait_for_function('S.working===false',timeout=15000)
 assert not downloads,'Unexpected iOS backup download prompt during write'
 assert page.evaluate('__mock.backupReads')>=1,'Server-side ECU backup did not run'
 assert page.evaluate('Object.keys(__prefs).every(x=>!x.startsWith("brickos.calbackup"))'),'Browser-local ECU backup mistakenly saved'
 backup=page.evaluate('savedBackup(1)')
 assert backup['length']==17816 and backup['page']==1,'Full-page prewrite snapshot was not stored'
 assert page.locator('[data-action="download-saved-backup"]').is_enabled(),'Persisted backup cannot be exported'
 with page.expect_download(timeout=20000) as dl:page.locator('[data-action="download-saved-backup"]').click()
 assert dl.value.suggested_filename.endswith('_prewrite.bin')
 writes,burns,reads=page.evaluate('()=>[__mock.writes,__mock.burns,__mock.readCount]')
 assert len(writes)>=3,f'Expected X/Y/map writes, got {writes}'
 assert burns==[0],f'Expected one flash burn, got {burns}'
 assert not page.locator('[data-action="burn"]').is_enabled(),'Edits not cleared after verification'
 # A full ESP32 storage error must abort before sending any C/B.
 page.locator('[data-cell="0"]').first.click();page.locator('[data-action="inc5"]').click()
 page.evaluate('()=>{__mock.backupStorageFail=true}')
 page.locator('[data-action="burn"]').click()
 page.wait_for_function("S.working===false && S.lastWritePhase.includes('backup')",timeout=15000)
 assert len(page.evaluate('__mock.writes'))==len(writes),'Wrote ECU despite failed backup'
 assert len(page.evaluate('__mock.burns'))==1,'Burned ECU despite failed backup'
 assert 'storage full' in page.evaluate('S.lastWriteError')
 page.locator('[data-action="burn-dismiss"]').click()
 page.evaluate('()=>{__mock.backupStorageFail=false}')
 # A failed C network response may be ambiguous and MUST NOT be silently retried.

 page.evaluate('()=>{__mock.failNextWrite=true}')
 page.locator('[data-action="burn"]').click()
 page.wait_for_function("S.working===false && S.lastWritePhase.startsWith('writing page')",timeout=15000)
 assert 'might have reached the ECU' in page.evaluate('S.lastWriteError')
 assert len(page.evaluate('__mock.writes'))==len(writes),'Ambiguous C command was retried'
 assert len(page.evaluate('__mock.burns'))==1,'Unexpected burn after failed C'
 assert len(page.evaluate('staged()'))==1,'Staged edit was lost on failed write'
 page.locator('[data-action="burn-dismiss"]').click()
 # If another writer changed bytes behind our back, reject the entire new plan before any C commands.
 page.evaluate('()=>{__pages[0][8964]^=1}')
 page.locator('[data-action="burn"]').click()
 page.wait_for_function("document.querySelector('#toast')?.textContent?.includes('data conflict')",timeout=14000)
 assert len(page.evaluate('__mock.writes'))==len(writes),'Stale edit was transmitted to ECU'
 assert len(page.evaluate('__mock.burns'))==1,'Stale edit triggered an extra burn'
 page.locator('[data-action="burn-dismiss"]').click()
 # The exact original complaint must now identify old/disabled installed firmware.
 page.evaluate('()=>{__mock.runtimeWrites=false;}')
 page.locator('[data-action="burn"]').click()
 page.wait_for_function("document.querySelector('#toast')?.textContent?.includes('v4.1 with writes DISABLED')",timeout=12000)
 assert len(page.evaluate('__mock.writes'))==len(writes),'Disabled firmware still received write commands'
 assert len(page.evaluate('__mock.burns'))==1,'Disabled firmware still received burn commands'
 assert not errors,f'Uncaught JS errors: {errors}'
 page.locator('#nav [data-route="overview"]').first.click()
 page.screenshot(path=str(ROOT/'preview_v39_dashboard.png'),full_page=True)
 page.set_viewport_size({'width':390,'height':844})
 page.screenshot(path=str(ROOT/'preview_v39_mobile.png'),full_page=True)
 assert page.evaluate('document.documentElement.scrollWidth<=window.innerWidth+2'),'Mobile layout overflows'
 print('PASS browser mock: INI handshake, axes, stage edits, durable on-device page backup, manual download, Safari read retry, verified C/B, stale conflict, mobile')
 print(f'Mock: {reads} reads; C writes={writes}; B burns={burns}; old disabled firmware detected correctly; no page errors')
 browser.close()
