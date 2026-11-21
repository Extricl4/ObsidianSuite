import sys, asyncio
from playwright.sync_api import sync_playwright
key=sys.argv[1]; out=sys.argv[2]
with sync_playwright() as p:
    b=p.chromium.launch(executable_path='/usr/bin/google-chrome',args=['--no-sandbox'])
    pg=b.new_page(viewport={'width':1280,'height':900},device_scale_factor=1)
    errs=[]; pg.on('pageerror',lambda e:errs.append(str(e))); pg.on('console',lambda m:errs.append(m.text) if m.type=='error' else None)
    pg.goto('file://' + __import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), 'site', 'index.html')); pg.wait_for_timeout(800)
    sec=pg.locator('#'+key); sec.scroll_into_view_if_needed()
    sec.screenshot(path=out+'_a.png')
    # interact: drag first knob up, press each button, then power off
    k=pg.locator(f'#{key} .hot[data-k="0"]'); bb=k.bounding_box()
    pg.mouse.move(bb['x']+bb['width']/2,bb['y']+bb['height']/2); pg.mouse.down(); pg.mouse.move(bb['x']+bb['width']/2,bb['y']-80,steps=5)
    pg.wait_for_timeout(100); pg.locator(f'#{key} .stage').screenshot(path=out+'_drag.png'); pg.mouse.up()
    for btn in pg.locator(f'#{key} .hot.btn').all(): btn.click(); pg.wait_for_timeout(100)
    pg.wait_for_timeout(600); pg.locator(f'#{key} .stage').screenshot(path=out+'_b.png')
    print(pg.locator(f'#{key} .read').inner_text())
    pg.locator(f'#{key} .hot.pw').click(); pg.wait_for_timeout(1200); pg.locator(f'#{key} .stage').screenshot(path=out+'_off.png')
    print(pg.locator(f'#{key} .read').inner_text())
    print('ERRORS',errs)
    b.close()
