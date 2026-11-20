"""Runs BrickOS browser flows with a deterministic in-page ECU emulator.
No localhost network or physical ECU is needed. Requires Playwright + Chromium.
"""
import asyncio
from pathlib import Path
from playwright.async_api import async_playwright
ROOT=Path(__file__).resolve().parents[1]
JS_MOCK = r'''
const fake={armed:false,auto:false,samples:25,page:new Uint8Array(8192)};
fake.page.set(new Uint8Array([150,0,4,1,104,1,220,0,44,1,254,1]),200);
function h(bytes){return Array.from(bytes,b=>b.toString(16).padStart(2,'0')).join('')}
function unhex(s){return Uint8Array.from(s.match(/../g)||[],x=>parseInt(x,16))}
window.fetch=async function(path,opts={}){
 let obj={ok:true};let responseBytes=null;
 const body=opts?.body?JSON.parse(opts.body):{};
 if(path==='/api/status')obj={ok:true,product:'BrickOS',transport:'test ISO-TP',can_tx_id:256,can_rx_id:258,tx_gpio:43,rx_gpio:44,
 last_ok_ms:4000,ok_requests:100,failed_requests:0,last_error:'OK',write_compiled:true,write_armed:fake.armed,physical_arm:true,
 logging:false,samples:fake.samples,log_name:'',storage:'/logs',storage_error:'OK',log_actual_hz:10,
 log_failures:0,auto_log:fake.auto,auto_log_offset:0,auto_log_length:256,auto_log_interval_ms:100};
 else if(path==='/api/logs')obj={ok:true,files:[]};
 else if(path==='/api/arm')fake.armed=true;
 else if(path==='/api/disarm')fake.armed=false;
 else if(path==='/api/log/auto')fake.auto=body.enabled;
 else if(path==='/api/ts'){
   const args=unhex(body.args);let result=new Uint8Array(0);
   if(body.cmd==='S')result=new TextEncoder().encode('rusEFI sample-123\0');
   else if(body.cmd==='O'){let o=args[0]|(args[1]<<8),n=args[2]|(args[3]<<8);result=new Uint8Array(n);if(o===0){result[0]=0;result[1]=0;}}
   else if(body.cmd==='R'){let off=args[2]|(args[3]<<8),n=args[4]|(args[5]<<8);result=fake.page.slice(off,off+n)}
   else if(body.cmd==='C'){if(!fake.armed)throw Error('Write not armed');let off=args[2]|(args[3]<<8),n=args[4]|(args[5]<<8);fake.page.set(args.slice(6,6+n),off)}
   obj={ok:true,status:body.cmd==='B'?4:0,length:result.length,hex:h(result)};
 }
 return {ok:true,status:200,json:async()=>obj,arrayBuffer:async()=>responseBytes};
};
'''
async def test():
    html=(ROOT.parent/'brickos_brick_preview.html').read_text()
    html=html.replace('<script>\n','<script>\n'+JS_MOCK+'\n</script>\n<script>\n',1)
    async with async_playwright() as playwright:
        browser=await playwright.chromium.launch(executable_path='/usr/bin/chromium',headless=True,
            args=['--no-sandbox','--disable-dev-shm-usage'])
        context=await browser.new_context(accept_downloads=True)
        page=await context.new_page()
        errors=[]
        page.on('pageerror',lambda e:errors.append(str(e)))
        page.on('dialog',lambda d:asyncio.create_task(d.accept()))
        await page.set_content(html,wait_until='domcontentloaded')
        await page.wait_for_timeout(400)
        assert 'ECU CONNECTED' in await page.locator('body').inner_text()
        ini=(ROOT/'tests/sample_generated.ini').read_bytes()
        await page.locator('#ini-upload').set_input_files({'name':'rusefi.ini','mimeType':'text/plain','buffer':ini})
        await page.wait_for_timeout(150)
        await page.locator('[data-route="tables"]').first.click()
        await page.locator('[data-load-field="fuelTable"]').first.click()
        await page.wait_for_timeout(300)
        assert await page.locator('#map-surface').count()==1, '3D map not displayed'
        await page.screenshot(path='/mnt/data/brickos_brick_map.png')
        await page.locator('[data-cell="0"]').first.click()
        await page.locator('[data-action="inc1"]').first.click()
        assert 'UNSAVED CHANGES' in await page.locator('#page').inner_text()
        async with page.expect_download() as dl:
            await page.locator('[data-action="export-patch"]').click()
        download=await dl.value
        assert download.suggested_filename.endswith('.btune')
        async with page.expect_download() as dl:
            await page.locator('[data-action="backup"]').click()
        backup=await dl.value
        assert backup.suggested_filename.endswith('.bin')
        await page.locator('[data-action="arm"]').click()
        await page.wait_for_timeout(150)
        assert await page.evaluate('S.status.write_armed')
        await page.locator('[data-action="apply"]').click()
        await page.wait_for_timeout(800)
        assert 'UNSAVED CHANGES' not in await page.locator('#page').inner_text()
        await page.locator('[data-route="settings"]').first.click()
        await page.locator('[data-load-field="tempCompCurve"]').first.click()
        assert await page.locator('#curve-chart').count()==1
        await page.screenshot(path='/mnt/data/brickos_brick_curve.png')
        await page.locator('[data-route="logs"]').first.click()
        assert await page.locator('[data-action="save-auto-log"]').count()==1
        await page.locator('#auto-log').check()
        await page.locator('[data-action="save-auto-log"]').click()
        await page.wait_for_timeout(100)
        assert 'Automatic recording saved' in await page.locator('#toast').inner_text()
        assert not errors, errors
        await browser.close()
    print('PASS: connect, INI, live table, 3D preview, edit, patch, full-page backup, arm/write/readback/burn, 1D curve, autonomous logger; no JS errors')
if __name__=='__main__':asyncio.run(test())
