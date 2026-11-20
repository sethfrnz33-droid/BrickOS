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
 ctx=browser.new_context(viewport={'width':390,'height':844},accept_downloads=True)
 page=ctx.new_page();errors=[];dialogs=[]
 page.on('pageerror',lambda e:errors.append(str(e)))
 def dialog(d):
  dialogs.append(d.message)
  d.dismiss() if len(dialogs)==1 else d.accept()
 page.on('dialog',dialog)
 page.set_content(html,wait_until='domcontentloaded')
 page.wait_for_selector('[data-instrument]')
 page.evaluate('()=>{S.route="tables";render()}')
 page.locator('[data-load-field="veTable"]').first.click()
 page.wait_for_selector('[data-cell]')
 page.locator('[data-cell="0"]').first.click()
 page.locator('[data-action="inc5"]').click()
 assert 'NOT SAVED' in page.locator('.burn-dock').inner_text()
 assert page.locator('[data-action="burn"]').is_enabled()
 # Navigation warning can be dismissed to remain in editor.
 page.locator('[data-action="close-editor"]').click()
 assert page.locator('[data-action="burn"]').is_visible()
 assert len(dialogs)==1 and 'NOT saved' in dialogs[0],dialogs
 # Reattempt leaving; confirmation retains the staged edit.
 page.locator('[data-action="close-editor"]').click()
 assert not page.locator('[data-action="burn"]').count()
 assert len(page.evaluate('staged()'))==1
 assert page.locator('.has-staged').count()==1
 assert 'unburned tuning changes' in page.locator('body').inner_text() or page.locator('.has-staged').count()==1
 # Re-open the setting and verify no more prompt is needed to burn.
 page.locator('[data-load-field="veTable"]').first.click()
 page.wait_for_selector('[data-action="burn"]')
 initial_dialogs=len(dialogs)
 # Simulate a delayed but acknowledged ECU B command, to verify real stage visibility.
 page.evaluate("""()=>{const original=window.fetch;window.fetch=async (url,opt)=>{
   if(String(url)==='/api/ts' && JSON.parse(opt?.body||'{}').cmd==='B')await new Promise(r=>setTimeout(r,950));
   return original(url,opt);
  };}""")
 page.locator('[data-action="burn"]').click()
 page.wait_for_function("S.burnState.active && S.burnState.step===4",timeout=15000)
 assert 'Burning ECU flash' in page.locator('#burn-flow').inner_text()
 assert 'ECU communication active' in page.locator('#burn-flow').inner_text()
 assert len(dialogs)==initial_dialogs,'Burn should not ask for additional confirmations'
 page.wait_for_function('S.working===false',timeout=15000)
 assert page.evaluate('__mock.burns.length')==1
 assert len(page.evaluate('staged()'))==0
 assert page.evaluate('S.burnState.success') is True
 page.wait_for_function('document.querySelector("#burn-flow").hidden',timeout=6000)
 assert page.locator('[data-action="burn"]').is_disabled()
 # Scalar settings are staged directly from the numeric input; no extra Stage button.
 page.evaluate('()=>loadField("launchRpm",1)')
 page.wait_for_selector('#scalar-value')
 page.locator('#scalar-value').fill('2500')
 assert page.locator('[data-action="burn"]').is_enabled(),'Scalar edit did not stage automatically'
 assert len(page.evaluate('staged()'))==1
 assert not page.locator('[data-action="set-scalar"]').count(),'Redundant scalar Stage button returned'
 page.screenshot(path=str(ROOT/'preview_v45_mobile_saved.png'),full_page=True)
 assert page.evaluate('document.documentElement.scrollWidth<=window.innerWidth+2'),'Phone UI should fit viewport'
 assert not errors,errors
 print('PASS v4.5 UX: cancelable leave warning, staged-change badge, single-tap burn, live flash stage, one B, auto-dismiss, 390px mobile')
 browser.close()
