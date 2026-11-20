"""BrickOS v3.8 browser smoke/regression with synthetic transport responses only.
This test does not claim physical ECU throughput or successful ESP-IDF flashing.
"""
from pathlib import Path
import asyncio
import threading
import http.server
import functools
from playwright.async_api import async_playwright

ROOT=Path(__file__).resolve().parents[1]
PREVIEW=ROOT.parent/'brickos_v38_preview.html'
html=PREVIEW.read_text()
MOCK=r'''
let testSequence=0;
window.fetch=async function(path,opts={}){
 if(path==='/api/status')return {ok:true,status:200,json:async()=>({ok:true,usb_device_ready:true,ecu_session_connected:true,ecu_signature:'rusEFI master.2026.11.09.proteus_f7.2735508644',ini_cached:true,live_actual_hz:48.1,live_sample_duration_us:2410,live_sample_age_ms:17,live_sample_bytes:512,live_samples_failed:0,ok_requests:100,failed_requests:0,storage:'/logs',storage_error:'OK',write_compiled:false})};
 if(path==='/api/ini/file')return {ok:false,status:404,json:async()=>({ok:false,error:'No cache'})};
 if(path==='/api/logs')return {ok:true,status:200,json:async()=>({ok:true,files:[]})};
 if(String(path).startsWith('/api/live')){
   const length=Number(new URL('http://127.0.0.1'+path).searchParams.get('length'));
   const bytes=new Uint8Array(16+length),dv=new DataView(bytes.buffer);
   bytes.set([66,76,86,49]);dv.setUint32(4,++testSequence,true);dv.setUint32(8,Date.now()>>>0,true);dv.setUint16(12,length,true);dv.setUint16(14,3,true);
   return {ok:true,status:200,arrayBuffer:async()=>bytes.buffer};
 }
 if(path==='/api/ts')return {ok:true,status:200,json:async()=>({ok:true,hex:'727573454649206d61737465722e323032362e31312e30392e70726f746575735f66372e32373335353038363434',status:0})};
 return {ok:true,status:200,json:async()=>({ok:true})};
};
'''
LOCAL_STORE=r'''const __prefs={};Object.defineProperty(window,'localStorage',{configurable:true,value:{getItem(k){return __prefs[k]??null},setItem(k,v){__prefs[k]=String(v)},removeItem(k){delete __prefs[k]}}});'''
html=html.replace('<script>\n','<script>\n'+LOCAL_STORE+'\n'+MOCK+'\n</script>\n<script>\n',1)
ini=(ROOT/'tests/fixtures/rusefi_proteus_f7.ini').read_text(errors='replace')
async def run():
 await smoke()
async def smoke():
 async with async_playwright() as pw:
  browser=await pw.chromium.launch(executable_path='/usr/bin/chromium',headless=True,args=['--no-sandbox','--disable-dev-shm-usage'])
  context=await browser.new_context(viewport={'width':1440,'height':900},device_scale_factor=1)
  page=await context.new_page();errors=[]
  page.on('pageerror',lambda e:errors.append(str(e)))
  await page.set_content(html,wait_until='domcontentloaded')
  await page.wait_for_timeout(250)
  await page.evaluate('(ini)=>{S.rawIni=ini;S.definition=parseIni(ini);S.ecuSignature=S.definition.signature;S.connected=true;S.signatureMismatch=false;S.status={ok:true,usb_device_ready:true,ecu_session_connected:true,live_actual_hz:48.1,live_sample_duration_us:2410,live_sample_age_ms:17,live_sample_bytes:512,live_samples_failed:0,ok_requests:100,failed_requests:0};rebuildTs();render()}',ini)
  await page.wait_for_timeout(150)
  assert await page.locator('#live-scope').count()==1,'Scope lost'
  assert await page.locator('[data-instrument]').count()>=4,'Missing live gauge instruments'
  assert await page.locator('.dial-pointer').count()>0,'No actual dial pointers'
  assert await page.locator('#nav [data-route]').count()==5
  # Command palette: global menu search opens a real INI-driven tuning screen.
  await page.locator('[data-action="open-command"]').click()
  assert await page.locator('#command-search').count()==1
  await page.fill('#command-search','Injector')
  assert await page.locator('[data-command-item]').count()>0
  await page.locator('[data-action="close-command"]').click()
  assert await page.locator('#command-search').count()==0
  await page.screenshot(path=str(ROOT/'preview_v38_desktop.png'),full_page=True)
  # User can open the real INI menu from Tune without a permanent dashboard tree.
  await page.locator('#nav [data-route="tuning"]').click()
  assert await page.locator('.tune-shell').count()==1
  assert await page.locator('.tune-category').count()>=7
  assert await page.locator('.tune-result').count()>0
  await page.screenshot(path=str(ROOT/'preview_v38_tuning.png'),full_page=True)
  await page.locator('#tune-search').fill('VE')
  assert await page.locator('.tune-result').count()>0
  await page.locator('[data-ts-item]').first.click()
  assert await page.evaluate('S.route')=='tuning'
  assert (await page.evaluate('S.tsSelected'))!=''
  # Favoriting a menu persists locally and doesn't touch ECU calibration.
  await page.locator('[data-tune-star]').first.click()
  assert len(await page.evaluate('S.tuneFavorites'))==1
  # Gauges can be added and edited, and preference is kept across navigation.
  await page.locator('#nav [data-route="gauges"]').click()
  await page.locator('[data-action="add-gauge"]').click()
  assert await page.locator('#gauge-editor-title').count()==1
  await page.select_option('#gauge-kind','digital')
  await page.fill('#gauge-min','0')
  await page.fill('#gauge-max','6000')
  await page.locator('[data-action="gauge-save"]').click()
  assert await page.evaluate('S.gaugeSlots.at(-1).kind')=='digital'
  assert await page.evaluate('JSON.parse(localStorage.getItem("brickos.workspace.v38")).gauges.length')>=4
  await page.locator('#nav [data-route="overview"]').click()
  assert await page.locator('.instrument-digital').count()==1
  assert await page.locator('#live-scope').count()==1
  await page.select_option('#live-rate','100')
  await page.select_option('#dashboard-layout','scope')
  assert await page.evaluate('S.dashboardLayout')=='scope'
  assert await page.locator('.cockpit-layout-scope').count()==1
  # Update the gauge's display from actual INI offsets, not simulated gauge values.
  await page.evaluate("""()=>{const b=new Uint8Array(768);const f=visibleChannels().find(x=>x.name==='RPMValue');const dv=new DataView(b.buffer);if(f?.type==='U16')dv.setUint16(f.offset,3000,true);else if(f?.type==='F32')dv.setFloat32(f.offset,3000,true);else if(f?.type==='U32')dv.setUint32(f.offset,3000,true);S.latest=b;renderLiveValues();drawLiveScope()}""")
  assert await page.locator('[data-instrument="RPMValue"] .instrument-reading').inner_text()!='0.0 RPM'
  assert not errors,errors
  # Narrow phone: only bottom app rail shows; Tune browser can be opened as drawer.
  await page.set_viewport_size({'width':390,'height':844})
  await page.wait_for_timeout(150)
  assert await page.locator('.app-rail').is_visible()
  await page.screenshot(path=str(ROOT/'preview_v38_mobile.png'),full_page=True)
  await page.locator('#nav [data-route="tuning"]').click()
  assert await page.locator('.tune-browser').is_visible()
  await page.locator('[data-ts-item]').first.click()
  assert not await page.locator('.tune-browser').is_visible()
  assert await page.locator('[data-action="tune-browse"]').count()==1
  await page.locator('[data-action="tune-browse"]').click()
  assert await page.locator('.tune-browser').is_visible()
  # No horizontal spill beyond viewport from the persistent application chrome.
  assert await page.evaluate('document.documentElement.scrollWidth<=window.innerWidth+2')
  assert not errors,errors
  await browser.close()
 print('PASS browser v3.8: dashboard dials, Tune menu, search, favorites, gauge editor, persistence, graph, mobile navigation; no JS errors')
if __name__=='__main__':asyncio.run(run())
