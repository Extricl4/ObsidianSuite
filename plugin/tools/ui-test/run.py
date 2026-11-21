#!/usr/bin/env python3
"""Runs the real plugin UI in headless Chrome against a fake engine (mock-native.js).

    pip install playwright && playwright install chromium
    python plugin/tools/ui-test/run.py            # checks + screenshots in plugin/tools/ui-test/out

It builds the page exactly like the plugin does (ui/obsidian-demo.html with
obsidian-native.css + obsidian-bridge.js injected before </body>), then clicks
and drags controls and checks what reaches the "engine".
"""
import asyncio, json, os, shutil, sys
from pathlib import Path
from playwright.async_api import async_playwright

HERE = Path(__file__).resolve().parent
PLUGIN = HERE.parent.parent
UI = PLUGIN.parent / 'ui'
OUT = HERE / 'out'

def build_page(standalone):
    OUT.mkdir(exist_ok=True)
    html = (UI / 'obsidian-demo.html').read_text(encoding='utf-8')
    inject = '<link rel="stylesheet" href="obsidian-native.css">\n<script src="obsidian-bridge.js"></script>\n'
    i = html.lower().rfind('</body>')
    html = html[:i] + inject + html[i:]
    (OUT / 'page.html').write_text(html, encoding='utf-8')
    shutil.copytree(UI / 'fonts', OUT / 'fonts', dirs_exist_ok=True)
    for f in [UI / 'grain.png', PLUGIN / 'WebUI' / 'obsidian-bridge.js', PLUGIN / 'WebUI' / 'obsidian-native.css']:
        shutil.copy(f, OUT / f.name)
    state = json.loads((HERE / 'state.json').read_text(encoding='utf-8'))
    state['standalone'] = standalone
    if standalone:
        state['audio'] = {'standalone': True, 'types': ['ASIO', 'Windows Audio', 'DirectSound'], 'type': 'ASIO',
            'inputDevices': ['Focusrite USB ASIO'], 'outputDevices': ['Focusrite USB ASIO'], 'inputDevice': 'Focusrite USB ASIO', 'outputDevice': 'Focusrite USB ASIO',
            'deviceName': 'Focusrite USB ASIO', 'sampleRates': [44100, 48000, 96000], 'bufferSizes': [32, 64, 128, 256, 512],
            'inputChannels': ['Analogue 1', 'Analogue 2'], 'outputChannels': ['Playback 1', 'Playback 2'], 'inputsOn': [True, False], 'outputsOn': [True, True],
            'sampleRate': 48000, 'bufferSize': 128, 'bitDepth': 24, 'inputLatencyMs': 3.2, 'outputLatencyMs': 3.9, 'hasControlPanel': True, 'cpu': 0.05,
            'midiInputs': [{'name': 'USB MIDI Pedal', 'id': 'usb1', 'enabled': True}]}
    return state

class Checks:
    def __init__(self): self.n = 0; self.bad = []
    def __call__(self, ok, what):
        self.n += 1
        print(('  ok    ' if ok else '  FAIL  ') + what)
        if not ok: self.bad.append(what)

async def run(standalone):
    state = build_page(standalone)
    check = Checks()
    print(f'\n== UI test ({"standalone" if standalone else "plugin in a DAW"})')
    async with async_playwright() as p:
        exe = '/usr/bin/google-chrome' if os.path.exists('/usr/bin/google-chrome') else None
        b = await p.chromium.launch(executable_path=exe, args=['--no-sandbox', '--allow-file-access-from-files'])
        pg = await b.new_page(viewport={'width': 1376, 'height': 836})
        errors = []
        pg.on('pageerror', lambda e: errors.append(str(e)))
        pg.on('console', lambda m: errors.append(m.text) if m.type == 'error' else None)
        await pg.add_init_script(path=str(HERE / 'mock-native.js'))
        await pg.add_init_script(script='window.__mock.load(' + json.dumps(state) + ');')
        await pg.goto((OUT / 'page.html').as_uri())
        await pg.wait_for_timeout(600)
        log = lambda: pg.evaluate('window.__mock.log.map(x=>x[0])')

        check('ready' in await log(), 'bridge started and asked the engine for its state')
        check(await pg.evaluate("!!document.querySelector('[data-param=\"in.gain\"]')"), 'header INPUT knob is bound')
        n = await pg.evaluate("Object.keys(window.__mock.params).filter(id=>document.querySelector('[data-param=\"'+id+'\"]')).length")
        check(n > 40, f'{n} controls on screen are bound to plugin parameters')

        # ---- drag the INPUT knob: gesture + values reach the engine
        await pg.evaluate("window.__mock.log.length=0")
        k = pg.locator('[data-param="in.gain"]')
        box = await k.bounding_box()
        await pg.mouse.move(box['x'] + box['width'] / 2, box['y'] + box['height'] / 2)
        await pg.mouse.down(); await pg.mouse.move(box['x'] + box['width'] / 2, box['y'] - 60, steps=8); await pg.mouse.up()
        await pg.wait_for_timeout(100)
        L = await pg.evaluate('window.__mock.log')
        cmds = [x[0] for x in L]
        check('beginGesture' in cmds and 'endGesture' in cmds and cmds.index('beginGesture') < cmds.index('endGesture'), 'dragging a knob sends begin/end gesture (DAW automation)')
        v = await pg.evaluate("window.__mock.sent('in.gain')")
        check(v is not None and v > 0.6, f'INPUT knob value reached the engine ({v})')

        # ---- engine -> UI, without echo
        await pg.evaluate("window.__mock.log.length=0; window.__mock.emit('obs:params', {'in.gain': 0.25, 'syn.flt.cutoff': 0.5, 'eq.b3': 0.75, 'cab.a.level': 0.5})")
        await pg.wait_for_timeout(900)
        t = await pg.evaluate("document.querySelector('[data-param=\"in.gain\"]').nextElementSibling.lastElementChild.textContent")
        check(t.strip() == '−6.0 dB', f'INPUT shows a value from the engine ({t.strip()})')
        echo = await pg.evaluate("window.__mock.log.filter(x=>x[0]==='setParam').map(x=>x[1])")
        check(not echo, 'values from the engine are not echoed back' + (f' (echoed {echo})' if echo else ''))
        fd = await pg.evaluate("document.querySelectorAll('.fd')[2].dataset.g")
        check(fd == '6', f'EQ band 3 fader follows the engine (+{fd} dB)')

        # ---- synth page
        await pg.evaluate("setTab('SYNTH')")
        defs = await pg.evaluate("obsSynth.defs")
        meta = {q['id']: q for q in state['params'] if q['id'].startswith('syn.')}
        bad = []
        TYPES = {'f': 'float', 'log': 'float', 'int': 'int', 'bool': 'bool', 'choice': 'choice'}
        for d in defs:
            q = meta.get(d[0])
            if not q: bad.append(d[0] + ' missing in engine'); continue
            if TYPES[d[1]] != q['type'] or abs(d[2] - q['min']) > 1e-4 or abs(d[3] - q['max']) > 1e-4 or abs(d[4] - q['def']) > 1e-3: bad.append(d[0])
            if d[1] == 'log' and not q['log']: bad.append(d[0] + ' log')
            if d[1] == 'f' and abs((d[6] if len(d) > 6 else 1) - q['skew']) > 1e-4: bad.append(d[0] + ' skew')
            if d[1] == 'choice' and q.get('choices') and d[5].split('|') != q['choices']: bad.append(d[0] + ' choices')
        extra = sorted(set(meta) - {d[0] for d in defs})
        check(not bad and not extra, f'synth page and engine agree on all {len(defs)} synth parameters' + (f' (mismatch: {bad[:6]} extra: {extra[:6]})' if bad or extra else ''))
        shown = await pg.evaluate("document.querySelector('.sy-dev').classList.contains('off')")
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.sy-bat').first.click(); await pg.wait_for_timeout(400)
        hidden = not await pg.evaluate("document.querySelector('.sy-dev').classList.contains('off')")
        check(shown and hidden and await pg.evaluate("window.__mock.sent('synth.on')") == 1, 'synth page shows when the synth is off, and the POWER switch turns it on')
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.sy-seg[data-param="syn.a.wave"] button[data-i="1"]').click(); await pg.wait_for_timeout(120)
        v = await pg.evaluate("window.__mock.sent('syn.a.wave')")
        check(v is not None and abs(v - 0.25) < 1e-6, f'SYNTH osc A pulse wave -> syn.a.wave = {v}')
        k = pg.locator('.sy-kn[data-param="syn.flt.cutoff"]'); box = await k.bounding_box()
        await pg.evaluate("window.__mock.log.length=0")
        await pg.mouse.move(box['x'] + box['width'] / 2, box['y'] + box['height'] / 2); await pg.mouse.down()
        await pg.mouse.move(box['x'] + box['width'] / 2, box['y'] - 40, steps=6); await pg.mouse.up(); await pg.wait_for_timeout(120)
        cmds = await pg.evaluate("window.__mock.log.map(x=>x[0]+':'+(x[1]||''))")
        check('beginGesture:syn.flt.cutoff' in cmds and 'endGesture:syn.flt.cutoff' in cmds, 'dragging CUTOFF sends the value with a gesture')
        await pg.evaluate("window.__mock.log.length=0; window.__mock.emit('obs:params', {'syn.b.wave': 0, 'syn.flt.type': 1, 'syn.arp.on': 1, 'syn.chord': 0.5})")
        await pg.wait_for_timeout(150)
        r = await pg.evaluate("obsSynth.read()")
        on_vowel = await pg.evaluate("document.querySelector('.sy-seg[data-param=\"syn.flt.type\"] button.on').textContent")
        chord = await pg.evaluate("document.querySelector('.sy-sel[data-param=\"syn.chord\"] .opt.on').textContent")
        echo = await pg.evaluate("window.__mock.log.filter(x=>x[0]==='setParam').length")
        check(abs(r['syn.b.wave']) < 1e-6 and r['syn.arp.on'] == 1 and on_vowel == 'VOWEL' and chord == 'TRIAD' and not echo, f'SYNTH follows the engine (vowel filter, arp on, chord {chord}) without echo')
        # arpeggiator steps travel with the rig
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.sy-st').nth(1).click(); await pg.wait_for_timeout(100)
        sent = await pg.evaluate("(window.__mock.log.find(x=>x[0]==='setSynthSteps')||[])[1]")
        check(isinstance(sent, list) and len(sent) == 16 and (sent[1] & 1) == 1, 'clicking a step sends all 16 steps to the engine')
        await pg.evaluate("const r=JSON.parse(JSON.stringify(window.__mock.state.rig)); r.misc=Object.assign({},r.misc,{synSteps:'1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0'}); window.__mock.emit('obs:rig', r)")
        await pg.wait_for_timeout(150)
        lit = await pg.evaluate("[...document.querySelectorAll('.sy-st .cell > i')].map(i=>parseFloat(i.style.height)>50?1:0).join('')")
        check(lit == '1000100010001000', f'steps from a loaded preset show in the lane ({lit})')
        # live note + step from the engine
        await pg.evaluate("window.__mock.meterExtra = {synNote:45, synStep:4, synSrc:0, synth:.5}")
        await pg.wait_for_timeout(150)
        note = await pg.evaluate("document.querySelector('.sy-led .n').textContent")
        playing = await pg.evaluate("[...document.querySelectorAll('.sy-st')].findIndex(s=>s.classList.contains('play'))")
        check(note == 'A2' and playing == 4, f'note display and arp lane follow the engine ({note}, step {playing})')
        await pg.evaluate("window.__mock.meterExtra = null")
        # AUDITION plays the engine (and switches the synth on)
        await pg.evaluate("window.__mock.log.length=0; window.__mock.engineNotes = true")
        await pg.locator('.sy-bot .sy-btn').first.click(); await pg.wait_for_timeout(200)
        L = await pg.evaluate("window.__mock.log.filter(x=>x[0]==='synthNote'||(x[0]==='setParam'&&x[1]==='synth.on'))")
        check(any(x[0] == 'synthNote' and x[2] for x in L), 'AUDITION sends notes to the engine')
        await pg.locator('.sy-bot .sy-btn').first.click(); await pg.wait_for_timeout(150)
        offs = await pg.evaluate("window.__mock.log.filter(x=>x[0]==='synthNote'&&!x[2]).length")
        check(offs >= 1, 'stopping AUDITION releases the note')
        # DICE rolls a new sound and UNDO brings the old one back
        before = await pg.evaluate("JSON.stringify(obsSynth.read())")
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.sy-btn.r').click(); await pg.wait_for_timeout(150)
        n_sent = await pg.evaluate("window.__mock.log.filter(x=>x[0]==='setParam'&&x[1].startsWith('syn.')).length")
        kept = await pg.evaluate("window.__mock.log.filter(x=>x[0]==='setParam'&&['syn.mix','syn.level','syn.key','syn.tune'].includes(x[1])).length")
        await pg.locator('.sy-undo').click(); await pg.wait_for_timeout(150)
        after = await pg.evaluate("JSON.stringify(obsSynth.read())")
        check(n_sent > 15 and kept == 0 and before == after, f'DICE changes {n_sent} settings (not mix/level/key), UNDO restores them')
        await pg.screenshot(path=str(OUT / 'synth.png'))

        # ---- pedals: add, edit, footswitch, remove
        await pg.evaluate("setTab('PEDALS')")
        await pg.locator('.page[data-page="PEDALS"] .obs-slot').nth(1).click()
        await pg.wait_for_timeout(150)
        tiles = await pg.locator('.page[data-page="PEDALS"] .obs-tile').count()
        check(tiles >= 8, f'ADD PEDAL lists the real pedals ({tiles})')
        await pg.screenshot(path=str(OUT / 'pedal-library.png'))
        await pg.locator('.page[data-page="PEDALS"] .obs-tile[data-mod="overdrive"]').click()
        await pg.wait_for_timeout(300)
        L = await pg.evaluate("window.__mock.log.filter(x=>x[0]==='setModule')")
        check(L and L[-1][1:] == [0, 1, 'overdrive'], f'clicking Overdrive puts it in slot 2 ({L[-1] if L else None})')
        knobs = await pg.locator('.page[data-page="PEDALS"] .obs-editor .kn').count()
        check(knobs == 4, f'pedal editor shows its 4 knobs ({knobs})')
        drive = await pg.evaluate("document.querySelector('.obs-editor .kn').parentElement.querySelector('.kn-val').textContent")
        check(drive == '3.0', f'Overdrive DRIVE shows its default ({drive})')
        await pg.screenshot(path=str(OUT / 'pedals.png'))
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.page[data-page="PEDALS"] .obs-slot').nth(1).locator('.obs-foot').click()
        await pg.wait_for_timeout(100)
        check(await pg.evaluate("window.__mock.sent('pre.2.on')") == 0, 'footswitch turns the pedal off')
        # drag slot 2 onto slot 4
        a = await pg.locator('.page[data-page="PEDALS"] .obs-slot').nth(1).bounding_box()
        c = await pg.locator('.page[data-page="PEDALS"] .obs-slot').nth(3).bounding_box()
        await pg.mouse.move(a['x'] + 75, a['y'] + 60); await pg.mouse.down()
        await pg.mouse.move(c['x'] + 75, c['y'] + 60, steps=10); await pg.mouse.up()
        await pg.wait_for_timeout(200)
        L = await pg.evaluate("window.__mock.log.filter(x=>x[0]==='moveModule')")
        check(L and L[-1][1:] == [0, 1, 3], f'dragging a pedal reorders it ({L[-1] if L else None})')

        # ---- FX + special FX libraries
        for page, want in (('FX', 'crypt'), ('SPECIAL FX', 'seraph')):
            await pg.evaluate(f"setTab('{page}')")
            await pg.locator(f'.page[data-page="{page}"] .obs-slot').first.click()
            await pg.wait_for_timeout(120)
            ok = await pg.locator(f'.page[data-page="{page}"] .obs-tile[data-mod="{want}"]').count()
            check(ok == 1, f'{page} library lists {want}')
            await pg.locator(f'.page[data-page="{page}"] .obs-tile[data-mod="{want}"]').click()
            await pg.wait_for_timeout(250)
        await pg.screenshot(path=str(OUT / 'special-fx.png'))

        # ---- amp + cab
        await pg.evaluate("setTab('AMP')")
        await pg.wait_for_timeout(150)
        check(await pg.locator('.obs-amp:not(.obs-hidden)').count() == 1, 'AMP page shows the loaded amp')
        based = await pg.evaluate("document.querySelector('.obs-amp .obs-based').textContent")
        check(len(based.strip()) > 12, f'amp says what it is based on ({based.strip()})')
        tint = await pg.evaluate("getComputedStyle(document.querySelector('.obs-amp')).backgroundColor")
        check(tint not in ('', 'rgba(0, 0, 0, 0)', 'transparent'), f'amp panel has its own tint ({tint})')
        await pg.screenshot(path=str(OUT / 'amp.png'))
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.obs-ampchange').click(); await pg.wait_for_timeout(150)
        inhouse = [a for a in state['amps'] if a.get('folder') == 'OBSIDIAN AMPS']
        top = await pg.locator('.obs-tile[data-amp]:visible').count()
        check(await pg.locator('.obs-ampfolder[data-amp-folder="OBSIDIAN AMPS"]:visible').count() == 1 and top == len(state['amps']) - len(inhouse),
              f'amp chooser: in-house amps sit in an OBSIDIAN AMPS folder ({top} loose)')
        await pg.screenshot(path=str(OUT / 'amp-library.png'))
        await pg.locator('.obs-ampfolder[data-amp-folder="OBSIDIAN AMPS"]:visible').click(); await pg.wait_for_timeout(150)
        inside = await pg.locator('.obs-tile[data-amp]:visible').count()
        check(inside == len(inhouse) == 9, f'OBSIDIAN AMPS folder opens with all {inside} amps')
        await pg.screenshot(path=str(OUT / 'amp-folder.png'))
        await pg.locator('.obs-crumb-back:visible').click(); await pg.wait_for_timeout(150)
        check(await pg.locator('.obs-ampfolder:visible').count() == 1, 'back button returns to all amps')
        await pg.locator('.pop:visible input').fill('jcm'); await pg.wait_for_timeout(150)
        check(await pg.locator('.obs-tile[data-amp]:visible').count() == 1, 'search finds amps inside folders (jcm)')
        await pg.locator('.pop:visible input').fill(''); await pg.wait_for_timeout(100)
        await pg.locator('.obs-ampfolder:visible').click(); await pg.wait_for_timeout(150)
        await pg.locator('.obs-tile[data-amp="brit800"]:visible').click(); await pg.wait_for_timeout(250)
        check(await pg.evaluate("window.__mock.log.some(x=>x[0]==='setAmp'&&x[1]==='brit800')"), 'choosing BRIT 800 in the amp list')
        check('JCM800' in await pg.evaluate("document.querySelector('.obs-amp .obs-based').textContent"), 'panel switches to BRIT 800')
        await pg.screenshot(path=str(OUT / 'amp-brit.png'))
        await pg.locator('.obs-ampchange').click(); await pg.wait_for_timeout(150)
        await pg.locator('.obs-tile[data-amp="nam-loader"]').click(); await pg.wait_for_timeout(250)
        check(await pg.evaluate("window.__mock.log.some(x=>x[0]==='setAmp'&&x[1]==='nam-loader')"), 'choosing NAM Loader in the amp list')
        chans = await pg.locator('.obs-chan').count()
        check(chans == 4, f'NAM amp shows 4 capture channels ({chans})')
        await pg.screenshot(path=str(OUT / 'amp-nam.png'))
        await pg.evaluate("setTab('CAB')"); await pg.wait_for_timeout(150)
        await pg.screenshot(path=str(OUT / 'cab.png'))
        txtpos = await pg.evaluate("document.querySelector('[data-param=\"cab.a.level\"]').parentElement.querySelector('.kn-val').textContent")
        check('dB' in txtpos, f'cab mic level shows dB ({txtpos})')

        # ---- presets
        await pg.locator('[aria-label="Previous preset"]').evaluate('b=>b.nextElementSibling.click()')
        await pg.wait_for_timeout(200)
        rows = await pg.locator('.obs-prow').count()
        check(rows == len(state['presets']), f'preset browser lists {rows} presets')
        await pg.screenshot(path=str(OUT / 'presets.png'))
        await pg.locator('.obs-prow').nth(2).dblclick(); await pg.wait_for_timeout(200)
        name = await pg.evaluate("document.querySelector('[aria-label=\"Previous preset\"]').nextElementSibling.textContent.trim()")
        check(name == state['presets'][2]['name'], f'double-click loads a preset ({name})')
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('[aria-label="Next preset"]').click(); await pg.wait_for_timeout(150)
        check(await pg.evaluate("window.__mock.log.some(x=>x[0]==='presetNext'&&x[1]===1)"), 'next-preset arrow')
        await pg.locator('button', has_text='SAVE AS').first.click(); await pg.wait_for_timeout(100)
        check(await pg.locator('.obs-dialog input').count() == 1, 'SAVE AS asks for a name')
        await pg.locator('.obs-dialog input').fill('My Wall'); await pg.locator('.obs-dialog [data-ok]').click(); await pg.wait_for_timeout(150)
        check(await pg.evaluate("window.__mock.log.some(x=>x[0]==='presetSaveAs'&&x[1]==='My Wall')"), 'SAVE AS saves under the new name')

        # ---- settings
        await pg.evaluate("openOv('settings')")
        for panel in ('Device', 'Audio', 'MIDI', 'Interface'):
            await pg.locator('.setnav button', has_text=panel).click(); await pg.wait_for_timeout(120)
            await pg.screenshot(path=str(OUT / f'settings-{panel.lower()}.png'))
        # ---- themes: every theme applies cleanly, is saved, and its moving layer can be switched off
        await pg.evaluate("window.__mock.log.length=0")
        ids = await pg.evaluate("window.obsThemes")
        for tid in ids:
            await pg.locator(f'.thc[data-t="{tid}"]').click(); await pg.wait_for_timeout(120)
        saved = await pg.evaluate("window.__mock.log.filter(x=>x[0]==='setSetting'&&x[1]==='theme').map(x=>x[2])")
        check(len(ids) >= 10 and saved[-len(ids):] == ids, f'all {len(ids)} themes apply and are saved ({", ".join(ids)})')
        await pg.locator('.thc[data-t="norse"]').click(); await pg.wait_for_timeout(300)
        band = await pg.evaluate("getComputedStyle(document.querySelector('.thband')).display")
        fx = await pg.evaluate("getComputedStyle(document.getElementById('themefx')).display")
        font = await pg.evaluate("getComputedStyle(document.querySelector('.thlogo')).fontFamily")
        loaded = await pg.evaluate("document.fonts.check(\"16px 'Uncial Antiqua'\")")
        await pg.screenshot(path=str(OUT / 'theme-norse.png'))
        await pg.locator('[aria-label="Moving theme details"]').click(); await pg.wait_for_timeout(150)
        fx_off = await pg.evaluate("getComputedStyle(document.getElementById('themefx')).display")
        sent_fx = await pg.evaluate("window.__mock.log.some(x=>x[0]==='setSetting'&&x[1]==='themeFx'&&x[2]===false)")
        check(band == 'block' and fx == 'block' and 'Uncial' in font and loaded and fx_off == 'none' and sent_fx, f'Norse: rune bands, snow and its own font; moving details switch off ({band}, {fx}->{fx_off}, font loaded {loaded})')
        await pg.locator('[aria-label="Moving theme details"]').click(); await pg.locator('.thc[data-t="obsidian"]').click(); await pg.wait_for_timeout(150)
        check(await pg.evaluate("getComputedStyle(document.querySelector('.thband')).display") == 'none', 'back to Obsidian: no bands')
        await pg.locator('.setnav button', has_text='Audio').click()
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.spanel[data-panel="Audio"] .seg button', has_text='HQ').click(); await pg.wait_for_timeout(120)
        check(await pg.evaluate("window.__mock.sent('transpose.hq')") == 1, 'Settings > Audio: transpose HQ switch')
        await pg.locator('.spanel[data-panel="Audio"] .seg button', has_text='4×').click(); await pg.wait_for_timeout(120)
        check(await pg.evaluate("window.__mock.log.some(x=>x[0]==='setSetting'&&x[1]==='oversampling'&&x[2]===4)"), 'Settings > Audio: oversampling 4×')
        if standalone:
            await pg.locator('.setnav button', has_text='Device').click(); await pg.wait_for_timeout(100)
            dd = await pg.evaluate("[...document.querySelectorAll('.spanel[data-panel=Device] button')].map(b=>b.textContent.trim()).filter(Boolean)")
            check(any('128 samples' in x for x in dd) and any('Focusrite' in x for x in dd), 'Device panel shows the real interface + buffer')
            check(await pg.locator('.obs-chk-in').count() == 2, 'Device panel lists the real input channels')
        else:
            check(await pg.locator('#f0 button', has_text='STUDIO').is_hidden(), 'STUDIO is hidden inside a DAW')
        await pg.evaluate("closeOv()")

        # ---- tuner + meters
        await pg.evaluate("openOv('tuner')"); await pg.wait_for_timeout(250)
        note = await pg.evaluate("[...document.querySelectorAll('.ov[data-ov=tuner] span')].find(s=>s.style.fontSize==='110px').firstChild.textContent")
        check(note == 'E', f'tuner shows the note from the engine ({note})')
        check(await pg.evaluate("window.__mock.log.some(x=>x[0]==='tunerOpen'&&x[1]===true)"), 'opening the tuner tells the engine')
        await pg.screenshot(path=str(OUT / 'tuner.png'))
        # the tuner box must not move when notes change (or when nothing is played)
        geo = "(()=>{const ov=document.querySelector('.ov[data-ov=tuner]'); const big=[...ov.querySelectorAll('span')].find(s=>s.style.fontSize==='110px'); const box=big.closest('div[style*=\"width: 700px\"]'); const r=e=>{const b=e.getBoundingClientRect(); return [Math.round(b.left*2)/2,Math.round(b.top*2)/2,Math.round(b.width*2)/2,Math.round(b.height*2)/2].join(',');}; return [r(box), r(big), r(big.previousElementSibling), r(big.nextElementSibling), r(big.parentElement.nextElementSibling)].join(' | '); })()"
        shapes = []
        for tun in [{'hz': 82.41, 'conf': .9, 'level': .3}, {'hz': 185.0, 'conf': .9, 'level': .3}, {'hz': 0, 'conf': 0, 'level': 0}, {'hz': 466.16 * 0.973, 'conf': .9, 'level': .3}, {'hz': 1046.5, 'conf': .9, 'level': .3}]:
            await pg.evaluate("window.__mock.meterExtra = {tuner: " + json.dumps(tun) + "}"); await pg.wait_for_timeout(120)
            shapes.append(await pg.evaluate(geo))
        await pg.evaluate("window.__mock.meterExtra = null")
        check(len(set(shapes)) == 1, 'tuner stays still across notes, strings and silence' + ('' if len(set(shapes)) == 1 else ' (' + ' / '.join(sorted(set(shapes))) + ')'))
        await pg.evaluate("closeOv(); setTab('AMP')")
        await pg.screenshot(path=str(OUT / 'main.png'))

        check(not errors, 'no script errors' + ('' if not errors else ': ' + ' | '.join(errors[:5])))
        await b.close()
    print(f'  {check.n - len(check.bad)} / {check.n} passed')
    return not check.bad

if __name__ == '__main__':
    ok = asyncio.run(run(False)) & asyncio.run(run(True))
    sys.exit(0 if ok else 1)
