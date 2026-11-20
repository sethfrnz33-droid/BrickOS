"""In-browser smoke test for the responsive PCLink-inspired workbench.
Uses synthetic data; never asserts real hardware throughput.
"""
from pathlib import Path
import asyncio
from playwright.async_api import async_playwright

ROOT=Path(__file__).resolve().parents[1]
html=(ROOT.parent/'brickos_brick_preview.html').read_text()
mock=r'''
let testSequence=0;
window.fetch=async function(path,opts={}){
 if(path==='/api/status')return {ok:true,status:200,json:async()=>({ok:true,usb_device_ready:true,ecu_session_connected:true,ecu_signature:'rusEFI master.2026.11.09.proteus_f7.2735508644',ini_cached:false,live_actual_hz:48.1,live_sample_duration_us:2410,live_sample_age_ms:17,live_sample_bytes:512,live_samples_failed:0,ok_requests:100,failed_requests:0,storage:'/logs',storage_error:'OK',write_compiled:false})};
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
html=html.replace('<script>\n','<script>\n'+mock+'\n</script>\n<script>\n',1)
async def go():
 async with async_playwright() as pw:
  browser=await pw.chromium.launch(executable_path='/usr/bin/chromium',headless=True,args=['--no-sandbox','--disable-dev-shm-usage'])
  context=await browser.new_context(viewport={'width':1440,'height':900},device_scale_factor=1)
  page=await context.new_page();errors=[]
  page.on('pageerror',lambda e:errors.append(str(e)))
  await page.set_content(html,wait_until='domcontentloaded')
  await page.wait_for_timeout(200)
  ini=(ROOT/'tests/fixtures/rusefi_proteus_f7.ini').read_text(errors='replace')
  await page.evaluate('(ini)=>{S.rawIni=ini;S.definition=parseIni(ini);S.ecuSignature=S.definition.signature;S.connected=true;S.signatureMismatch=false;S.status={ok:true,usb_device_ready:true,ecu_session_connected:true,live_actual_hz:48.1,live_sample_duration_us:2410,live_sample_age_ms:17,live_sample_bytes:512,live_samples_failed:0,ok_requests:100,failed_requests:0};rebuildTs();render()}',ini)
  await page.wait_for_timeout(350)
  assert await page.locator('#live-scope').count()==1
  assert await page.locator('[data-gauge]').count()>=4
  assert await page.locator('[data-live-actual]').count()==1
  await page.screenshot(path=str(ROOT/'preview_v37_desktop.png'),full_page=True)
  # High sample target can be changed without losing the GUI state.
  await page.select_option('#live-rate','100')
  assert await page.evaluate('S.liveHz')==100
  await page.select_option('#live-mode','full')
  assert await page.evaluate('liveCaptureLength()')==2248
  await page.locator('[data-route="gauges"]').click()
  assert await page.locator('[data-gauge]').count()>10
  await page.locator('[data-route="overview"]').click()
  await page.set_viewport_size({'width':390,'height':844})
  await page.wait_for_timeout(150)
  await page.screenshot(path=str(ROOT/'preview_v37_mobile.png'),full_page=True)
  assert await page.locator('#live-scope').count()==1
  assert not errors,errors
  await browser.close()
 print('PASS browser 3.7: real generated INI, desktop/mobile multi-pane dashboard, gauges, live scope, rate selector; no JS errors')
if __name__=='__main__':asyncio.run(go())
