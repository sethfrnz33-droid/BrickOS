import asyncio
from pathlib import Path
from playwright.async_api import async_playwright
R=Path(__file__).resolve().parents[1]
async def go():
    async with async_playwright() as p:
        b=await p.chromium.launch(executable_path='/usr/bin/chromium',headless=True,args=['--no-sandbox'])
        page=await b.new_page(viewport={'width':1440,'height':900})
        errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
        await page.set_content((R.parent/'brickos_brick_preview.html').read_text(),wait_until='domcontentloaded')
        ini=(R/'tests/fixtures/proteus_f7_generated.ini').read_text(encoding='latin1')
        await page.evaluate('''async ini=>{S.ecuSignature='rusEFI master.2026.11.09.proteus_f7.2735508644';S.connected=true;await activateIni(ini);S.route='tuner';render()}''',ini)
        print('menus',await page.locator('.ts-cat-header').all_text_contents())
        await page.locator('[data-ts-item="veTableDialog"]').click()
        print('tuning title',await page.locator('h1').first.inner_text())
        print('map links',await page.locator('.ts-map-card').all_text_contents())
        await page.screenshot(path=str(R.parent/'BrickOS_v36_TunerStudio_UI_preview.png'),full_page=False)
        print('errors',errors)
        assert 'VE' in await page.locator('h1').first.inner_text()
        assert await page.locator('.ts-map-card').count()>=1
        assert not errors,errors
        await b.close()
asyncio.run(go())
