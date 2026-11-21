import sys
from playwright.sync_api import sync_playwright
files=sys.argv[1:]
with sync_playwright() as p:
    b=p.chromium.launch(executable_path='/usr/bin/google-chrome',args=['--no-sandbox'])
    pg=b.new_page(viewport={'width':1280,'height':900})
    errs=[]; pg.on('pageerror',lambda e:errs.append(str(e))); pg.on('console',lambda m:errs.append(m.text) if m.type=='error' else None)
    pg.goto('file://' + __import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), 'site', 'index.html')); pg.wait_for_timeout(800)
    st=pg.locator('#eidolon .stage'); st.scroll_into_view_if_needed(); st.screenshot(path='/tmp/e_empty.png')
    print(pg.locator('#eidolon .read').inner_text())
    pg.locator('#eidolon .filein').set_input_files(files); pg.wait_for_timeout(500)
    st.screenshot(path='/tmp/e_loaded.png'); print(pg.locator('#eidolon .read').inner_text())
    pg.locator('#eidolon .hot.btn').nth(1).click(); pg.wait_for_timeout(200)
    k=pg.locator('#eidolon .hot[data-k="0"]'); bb=k.bounding_box()
    pg.mouse.move(bb['x']+bb['width']/2,bb['y']+bb['height']/2); pg.mouse.down(); pg.mouse.move(bb['x']+bb['width']/2,bb['y']-60,steps=5); pg.mouse.up()
    st.screenshot(path='/tmp/e_next.png'); print(pg.locator('#eidolon .read').inner_text())
    pg.locator('#eidolon .hot.pw').click(); pg.wait_for_timeout(1500); st.screenshot(path='/tmp/e_off.png')
    print('ERRORS',errs); b.close()
