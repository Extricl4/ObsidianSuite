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
    inject = '<link rel="stylesheet" href="obsidian-native.css">\n<script src="obsidian-pedals.js"></script>\n<script src="obsidian-bridge.js"></script>\n'
    i = html.lower().rfind('</body>')
    html = html[:i] + inject + html[i:]
    (OUT / 'page.html').write_text(html, encoding='utf-8')
    shutil.copytree(UI / 'fonts', OUT / 'fonts', dirs_exist_ok=True)
    for f in [UI / 'grain.png', PLUGIN / 'WebUI' / 'obsidian-bridge.js', PLUGIN / 'WebUI' / 'obsidian-native.css', PLUGIN / 'WebUI' / 'obsidian-pedals.js', *sorted((PLUGIN / 'WebUI' / 'pedals').glob('*.webp'))]:
        shutil.copy(f, OUT / f.name)
    state = json.loads((HERE / 'state.json').read_text(encoding='utf-8'))
    state['standalone'] = standalone
    # three amps from the amp pack, with their faces (images served from Content/Faces like the plugin does)
    state['amps'] += [capture_entry(k) for k in ('umbra', 'moirai', 'kratos')]
    # your presets: the dump's own ones, two of them in a folder of yours
    mine = [dict(e, id='user:' + e['name']) for e in state['presets'] if not e['factory']]
    for e in mine[:2]: e['dir'] = 'Drop D'; e['id'] = 'user:Drop D/' + e['name']
    state['presets'] = factory_presets() + mine
    state['presetFolders'] = ['Drop D', 'Live set']
    faces = OUT / 'faces'
    if faces.is_symlink() or faces.exists():
        faces.unlink() if faces.is_symlink() else shutil.rmtree(faces)
    faces.symlink_to(CONTENT / 'Faces', target_is_directory=True)
    if standalone:
        state['audio'] = {'standalone': True, 'types': ['ASIO', 'Windows Audio', 'DirectSound'], 'type': 'ASIO',
            'inputDevices': ['Focusrite USB ASIO'], 'outputDevices': ['Focusrite USB ASIO'], 'inputDevice': 'Focusrite USB ASIO', 'outputDevice': 'Focusrite USB ASIO',
            'deviceName': 'Focusrite USB ASIO', 'sampleRates': [44100, 48000, 96000], 'bufferSizes': [32, 64, 128, 256, 512],
            'inputChannels': ['Analogue 1', 'Analogue 2'], 'outputChannels': ['Playback 1', 'Playback 2'], 'inputsOn': [True, False], 'outputsOn': [True, True],
            'sampleRate': 48000, 'bufferSize': 128, 'bitDepth': 24, 'inputLatencyMs': 3.2, 'outputLatencyMs': 3.9, 'hasControlPanel': True, 'cpu': 0.05,
            'midiInputs': [{'name': 'USB MIDI Pedal', 'id': 'usb1', 'enabled': True}]}
    return state

CONTENT = PLUGIN / 'Content'

def factory_presets():
    """The built-in presets (plugin/Presets/**), listed like PresetManager::listJson does."""
    out = []
    for f in (PLUGIN / 'Presets').rglob('*.json'):
        v = json.loads(f.read_text(encoding='utf-8'))
        e = {'id': 'factory:' + v['name'], 'name': v['name'], 'category': v.get('category', ''), 'factory': True}
        if v.get('type') and v.get('type') != e['category']: e['type'] = v['type']
        if v.get('note'): e['note'] = v['note']
        out.append(e)
    key = lambda e: (('1' if e['category'].startswith('Artists/') else '0') + e['category'] + '\n' + e['name']).lower()
    return sorted(out, key=key)
PARAM_OF = {'gain': 'amp.gain', 'bass': 'amp.bass', 'mid': 'amp.mid', 'treble': 'amp.treble', 'presence': 'amp.presence', 'depth': 'amp.depth', 'master': 'amp.master'}

def capture_entry(key):
    """A capture amp as Rig::ampLibrary() lists it (roles as in CaptureAmp.h assignRoles, simplified)."""
    amp = json.loads((CONTENT / 'Amps' / (key + '.json')).read_text(encoding='utf-8'))
    face = json.loads((CONTENT / 'Faces' / (key + '.json')).read_text(encoding='utf-8'))
    m = amp['map']; labels = [l.lower() for l in m['knobs']]
    has_master = 'master' in labels; taken = has_master; knobs = []
    sel = m.get('selector', {}).get('knob', -1)
    for j, l in enumerate(labels):
        r = 'plain'
        if j == m['gainKnob']: r = 'gain'
        elif j == sel: r = 'selector'
        elif l in ('bass', 'low'): r = 'bass'
        elif l in ('middle', 'mid'): r = 'mid'
        elif l in ('treble', 'high'): r = 'treble'
        elif l == 'presence': r = 'presence'
        elif l in ('depth', 'resonance'): r = 'depth'
        elif l == 'master': r = 'master'
        elif l in ('volume', 'output', 'level'):
            if not taken: r = 'master'; taken = True
            else: r = 'level'
        fk = face['knobs'][j]
        knobs.append({'label': m['knobs'][j], 'role': r, 'param': PARAM_OF.get(r, ''), 'def': fk['v'], 'steps': fk.get('steps', 0)})
    bank = []
    def add(f):
        if f and f not in bank: bank.append(f)
    for v in m['settings'].values():
        for f in v['files']:
            for x in (f if isinstance(f, list) else [f]): add(x)
    for f in m.get('selector', {}).get('files', []): add(f)
    return {'key': key, 'name': amp['name'], 'type': 'capture', 'role': amp.get('role', ''), 'description': amp.get('description', ''), 'basedOn': amp.get('basedOn', ''),
            'folder': 'CAPTURE AMPS', 'builtIn': False, 'channels': len(m['buttons'][0]['names']) if m['buttons'] and m['buttons'][0].get('names') else 1,
            'knobs': knobs, 'bank': [Path(b).stem for b in bank], 'face': face}

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
        # ---- EQ: double-click a fader resets it; low / high cut switches; RESET puts everything back
        await pg.evaluate("setTab('EQ')"); await pg.wait_for_timeout(150)
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.fd').nth(2).dblclick(); await pg.wait_for_timeout(200)
        g3 = await pg.evaluate("document.querySelectorAll('.fd')[2].dataset.g")
        check(g3 == '0' and await pg.evaluate("window.__mock.sent('eq.b3')") == 0.5, f'double-clicking an EQ fader puts it back to 0 dB ({g3})')
        cuts = await pg.evaluate("[...document.querySelectorAll('.obs-cutsw .tog')].map(t=>[t.getAttribute('aria-label'), t.dataset.on])")
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.obs-cutsw .tog').first.click(); await pg.wait_for_timeout(200)
        lc_off = await pg.evaluate("window.__mock.sent('eq.lc.on')"); dim = await pg.evaluate("document.querySelector('.obs-cutsw').parentElement.classList.contains('obs-cutoff')")
        check(cuts == [['low cut on', '1'], ['high cut on', '1']] and lc_off == 0 and dim, f'LOW CUT / HIGH CUT have on/off switches ({cuts}; low cut off sends {lc_off})')
        await pg.evaluate("setFader(document.querySelectorAll('.fd')[5], 8); document.dispatchEvent(new Event('change'))"); await pg.wait_for_timeout(200)
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.eqreset').click(); await pg.wait_for_timeout(250)
        check(await pg.evaluate("window.__mock.sent('eq.b6')") == 0.5 and await pg.evaluate("window.__mock.sent('eq.lc.on')") == 1
              and await pg.evaluate("document.querySelector('.obs-cutsw .tog').dataset.on") == '1', 'EQ RESET: every band flat and both cut switches back on')
        await pg.screenshot(path=str(OUT / 'eq.png'))

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
        await pg.locator('.sy-pb').first.click(); await pg.wait_for_timeout(400)
        hidden = not await pg.evaluate("document.querySelector('.sy-dev').classList.contains('off')")
        check(shown and hidden and await pg.evaluate("window.__mock.sent('synth.on')") == 1, 'synth page shows when the synth is off, and the POWER button turns it on')
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
        # computer keys play the engine (and switch the synth on); no AUDITION button any more
        await pg.evaluate("window.__mock.log.length=0; window.__mock.engineNotes = true")
        await pg.keyboard.down('a'); await pg.wait_for_timeout(120); await pg.keyboard.up('a'); await pg.wait_for_timeout(120)
        L = await pg.evaluate("window.__mock.log.filter(x=>x[0]==='synthNote')")
        check(any(x[2] for x in L) and any(not x[2] for x in L), 'computer key A plays and releases a note in the engine')
        check(await pg.evaluate("![...document.querySelectorAll('.sy-dev .tx b')].some(b=>/AUDITION/.test(b.textContent))"), 'no AUDITION button on the synth')
        # presets: the browser lists them, loading one sends the sound (not mix / output / key)
        n_pr = await pg.evaluate("obsSynth.presets.length")
        await pg.locator('.sy-prbtn').click(); await pg.wait_for_timeout(150)
        tiles = await pg.locator('.sy-pr').count()
        await pg.locator('.sy-prs-cats button[data-c="ARCADE"]').click(); await pg.wait_for_timeout(100)
        arcade = await pg.locator('.sy-pr').count()
        check(n_pr >= 50 and tiles == n_pr and 0 < arcade < n_pr, f'synth preset browser: {n_pr} presets, ARCADE shows {arcade}')
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.sy-pr').first.click(); await pg.wait_for_timeout(200)
        sent = await pg.evaluate("Object.fromEntries(window.__mock.log.filter(x=>x[0]==='setParam'&&x[1].startsWith('syn.')).map(x=>[x[1],x[2]]))")
        name = await pg.evaluate("document.querySelector('.sy-prname').textContent")
        r = await pg.evaluate("obsSynth.read()")
        check(name == 'Waka Chomp' and abs(sent.get('syn.char', -1) - 2 / 3) < 1e-6 and r['syn.arp.on'] == 1 and abs(r['syn.arp.rate'] - 1) < 1e-6 and len(sent) > 10 and not any(k in sent for k in ('syn.mix', 'syn.level', 'syn.key')),
              f'loading "Waka Chomp" sends its sound to the engine (8-bit, arp at 1/32), keeps mix / output / key ({name}, {len(sent)} settings)')
        steps_sent = await pg.evaluate("(window.__mock.log.filter(x=>x[0]==='setSynthSteps').pop()||[])[1]")
        check(isinstance(steps_sent, list) and len(steps_sent) == 16 and (steps_sent[8] & 1) == 0, 'the preset\'s arpeggiator steps go to the engine too')
        bad = await pg.evaluate("""() => { const D = Object.fromEntries(obsSynth.defs.map(d => [d[0], d])), out = [];
          obsSynth.presets.forEach(p => { for (const k in p.v) { const d = D['syn.' + k], v = p.v[k];
            if (!d) out.push(p.name + ': ' + k); else if (typeof v === 'string' ? !(d[5] || '').split('|').some(o => o.toLowerCase() === v.toLowerCase()) : (v < d[2] || v > d[3])) out.push(p.name + ': ' + k + '=' + v); } });
          return out; }""")
        check(not bad, 'every synth preset uses real settings and choices' + (f' ({bad[:4]})' if bad else ''))
        await pg.locator('.sy-prnav button').nth(1).click(); await pg.wait_for_timeout(150)
        name2 = await pg.evaluate("document.querySelector('.sy-prname').textContent")
        check(name2 == 'Ghost Siren', f'the next-preset arrow loads the next one ({name2})')
        await pg.locator('.sy-prs .x').click(); await pg.wait_for_timeout(100)
        # RESET: everything back to default (key and tuning stay), UNDO brings the preset back
        await pg.evaluate("window.__mock.emit('obs:params', {'syn.key': 9/11, 'syn.mix': 0.9})"); await pg.wait_for_timeout(150)
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.sy-reset').click(); await pg.wait_for_timeout(200)
        r = await pg.evaluate("obsSynth.read()")
        check(abs(r['syn.mix'] - 0.5) < 1e-6 and abs(r['syn.key'] - 9 / 11) < 1e-6 and r['syn.arp.on'] == 0 and abs(r['syn.char'] - 1 / 3) < 1e-6
              and await pg.evaluate("document.querySelector('.sy-prname').textContent") == 'Init', 'RESET puts the synth back to default (key stays)')
        await pg.locator('.sy-undo').click(); await pg.wait_for_timeout(150)
        r = await pg.evaluate("obsSynth.read()")
        check(abs(r['syn.mix'] - 0.9) < 1e-6 and abs(r['syn.char'] - 2 / 3) < 1e-6 and abs(r['syn.lfo.depth'] - 1) < 1e-6, 'UNDO brings back the sound from before RESET')
        # every number and label of the synth stays inside its box, at default, lowest and highest values
        fit = await pg.evaluate("""async () => {
          const out = [], dev = document.querySelector('.sy-dev'), wait = () => new Promise(r => setTimeout(r, 30));
          const views = [() => document.querySelectorAll('.sy-tab')[0].click(), () => document.querySelectorAll('.sy-tab')[1].click(), () => document.querySelectorAll('.sy-tab')[2].click(),
                         () => document.querySelectorAll('.sy-tab')[4].click(), () => document.querySelectorAll('.sy-tab')[3].click()];
          for (const v of views) { v();
            for (const n of [null, 0, 1, .37]) {
              if (n !== null) obsSynth.write(Object.fromEntries(obsSynth.ids.map(id => [id, n]))); else obsSynth.reset();
              await wait();
              dev.querySelectorAll('.sy-kv,.sy-kl,.sy-val,.sy-eng,.sy-pbv,.sy-seg button,.sy-tab,.sy-chip,.sy-hint,.sy-lbl,.sy-tg,.sy-sh h3').forEach(e => {
                if (!e.offsetParent) return; const box = (e.closest('.sy-sec') || dev).getBoundingClientRect(), r = e.getBoundingClientRect();
                if (r.left < box.left + 3 || r.right > box.right - 3 || r.top < box.top + 2 || r.bottom > box.bottom - 3) out.push((e.textContent || '').trim() + ' @' + n);
              }); } }
          obsSynth.reset(); return [...new Set(out)]; }""")
        check(not fit, 'every synth number and label stays inside its box (all settings low, high, default)' + (f' ({fit[:5]})' if fit else ''))
        # DICE rolls a new sound and UNDO brings the old one back
        await pg.wait_for_timeout(400)        # let the UNDO above reach the engine first
        before = await pg.evaluate("JSON.stringify(obsSynth.read())")
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.sy-btn.r:not(.sy-reset)').click(); await pg.wait_for_timeout(150)
        n_sent = await pg.evaluate("window.__mock.log.filter(x=>x[0]==='setParam'&&x[1].startsWith('syn.')).length")
        rolled = await pg.evaluate("obsSynth.read()")
        kept = sum(1 for k in ('syn.mix', 'syn.level', 'syn.key', 'syn.tune') if abs(rolled[k] - json.loads(before)[k]) > 1e-6)
        await pg.locator('.sy-undo').click(); await pg.wait_for_timeout(150)
        after = await pg.evaluate("JSON.stringify(obsSynth.read())")
        diff = [k for k, v in json.loads(before).items() if abs(v - json.loads(after)[k]) > 1e-6]
        check(n_sent > 15 and kept == 0 and not diff, f'DICE changes {n_sent} settings (not mix/level/key), UNDO restores them' + (f' (kept {kept}, differ {diff[:5]})' if kept or diff else ''))
        await pg.screenshot(path=str(OUT / 'synth.png'))

        # ---- HARMONY page (HYDRA)
        await pg.evaluate("setTab('HARMONY')"); await pg.wait_for_timeout(150)
        ids = await pg.evaluate("obsHarmony.ids")
        meta = {q['id']: q for q in state['params'] if q['id'].startswith('harm.')}
        ui = await pg.evaluate("obsHarmony.read()")
        bad = [i for i in ids if i not in meta or abs(ui[i] - (meta[i]['def'] - meta[i]['min']) / (meta[i]['max'] - meta[i]['min'])) > 1e-3]
        extra = sorted(set(meta) - set(ids) - {'harm.on'})
        ids = ids + ['harm.on']
        check(len(ids) == 58 and not bad and not extra, f'HARMONY page and engine agree on all {len(ids)} harmony parameters and their defaults' + (f' (mismatch: {bad[:6]} extra: {extra[:6]})' if bad or extra else ''))
        js_modes = await pg.evaluate("obsHarmony.modes.map(m=>m.name.replace(/♮/g,'nat').replace(/♭/g,'b').replace(/♯/g,'#'))")
        check(js_modes == meta['harm.mode'].get('choices'), f'all {len(js_modes)} modes match the engine, in order')
        await pg.evaluate("window.__mock.log.length=0")
        was = await pg.evaluate("obsHarmony.state.on ? 1 : 0")
        await pg.locator('.hm-pw').click(); await pg.wait_for_timeout(150)
        first = await pg.evaluate("window.__mock.sent('harm.on')")
        await pg.locator('.hm-pw').click(); await pg.wait_for_timeout(150)
        check(first == 1 - was and await pg.evaluate("window.__mock.sent('harm.on')") == was, f'POWER switches HYDRA on and off (harm.on {was} -> {first} -> {was})')
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.hm-seg button', has_text='CHORDS').click(); await pg.wait_for_timeout(150)
        await pg.locator('.hm-seg button', has_text='BEFORE AMP').click(); await pg.wait_for_timeout(150)
        check(await pg.evaluate("window.__mock.sent('harm.track')") == 1 and await pg.evaluate("window.__mock.sent('harm.place')") == 1, 'TRACKING CHORDS and PLACE BEFORE AMP reach the engine')
        await pg.locator('.hm-seg button', has_text='LEAD').click(); await pg.locator('.hm-seg button', has_text='AFTER AMP').click(); await pg.wait_for_timeout(150)
        k = pg.locator('.hm-v').nth(0).locator('.hm-vk .hm-kn').first; box = await k.bounding_box()
        await pg.evaluate("window.__mock.log.length=0")
        await pg.mouse.move(box['x'] + box['width'] / 2, box['y'] + box['height'] / 2); await pg.mouse.down()
        await pg.mouse.move(box['x'] + box['width'] / 2, box['y'] - 30, steps=6); await pg.mouse.up(); await pg.wait_for_timeout(150)
        cmds = await pg.evaluate("window.__mock.log.map(x=>x[0]+':'+(x[1]||''))")
        lv = await pg.evaluate("window.__mock.sent('harm.v1.level')")
        check('beginGesture:harm.v1.level' in cmds and 'endGesture:harm.v1.level' in cmds and lv is not None and lv > 0.8, f'dragging VOICE 1 LEVEL moves harm.v1.level with a gesture ({lv})')
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.hm-dd').nth(1).click(); await pg.wait_for_timeout(100)
        await pg.locator('.hm-menu.show .hm-modes button', has_text='Dorian').first.click(); await pg.wait_for_timeout(150)
        mv = await pg.evaluate("window.__mock.sent('harm.mode')")
        check(mv is not None and abs(mv - 4 / 44) < 1e-6, f'the MODE menu picks Dorian (harm.mode = {mv})')
        await pg.evaluate("window.__mock.log.length=0; window.__mock.emit('obs:params', {'harm.key': 9/11, 'harm.v2.step': 19/28, 'harm.v3.on': 1})")
        await pg.wait_for_timeout(150)
        keytxt = await pg.evaluate("document.querySelectorAll('.hm-dd b')[0].textContent")
        v2 = await pg.evaluate("document.querySelectorAll('.hm-v')[1].querySelector('.hm-int b').textContent")
        v3on = await pg.evaluate("!document.querySelectorAll('.hm-v')[2].classList.contains('mute')")
        echo = await pg.evaluate("window.__mock.log.filter(x=>x[0]==='setParam').length")
        check(keytxt == 'A' and v2 == '6TH' and v3on and not echo, f'HARMONY follows the engine (key {keytxt}, voice 2 {v2}, voice 3 on) without echo')
        if not await pg.evaluate("obsHarmony.state.on"): await pg.locator('.hm-pw').click(); await pg.wait_for_timeout(200)
        await pg.evaluate("window.__mock.meterExtra = {hmNote: 64, hmChord: []}"); await pg.wait_for_timeout(200)
        led = await pg.evaluate("document.querySelector('.hm-led .n').textContent")
        dots = await pg.evaluate("[...document.querySelectorAll('.hm-kb .dot')].filter(d=>d.style.display!=='none').length")
        son = await pg.evaluate("[obsHarmony.state.on, obsHarmony.state.voices.filter(v=>v.on).length]")
        check(led == 'E4' and dots >= 2, f'the note you play shows live, with where each voice lands ({led}, {dots} dots, power/voices {son})')
        await pg.evaluate("window.__mock.meterExtra = null"); await pg.wait_for_timeout(200)
        await pg.evaluate("window.__mock.log.length=0")
        await pg.evaluate("obsHarmony.loadPreset(obsHarmony.presets.findIndex(p=>p[0]==='Queen Choir'))"); await pg.wait_for_timeout(150)
        await pg.evaluate("document.dispatchEvent(new Event('change'))"); await pg.wait_for_timeout(200)
        q4 = await pg.evaluate("window.__mock.sent('harm.v4.on')"); qm = await pg.evaluate("window.__mock.sent('harm.mode')")
        check(q4 == 1 and qm is not None and abs(qm) < 1e-6, f'a HYDRA preset (Queen Choir) sets the voices and the mode in the engine ({q4}, {qm})')
        # double-clicking the HARMONY tab switches the section, once per double-click (no flicker)
        htab = pg.locator('button', has_text='HARMONY').first
        on0 = await pg.evaluate("obsHarmony.state.on")
        flips = []
        for _ in range(2):
            await pg.evaluate("window.__mock.log.length=0")
            await htab.dblclick(); await pg.wait_for_timeout(450)
            flips.append([await pg.evaluate("JSON.stringify(window.__mock.log.filter(x=>x[0]==='setParam'&&x[1]==='harm.on').map(x=>x[2]))"), await pg.evaluate("obsHarmony.state.on")])
        want = [[json.dumps([0 if on0 else 1]).replace(' ', ''), not on0], [json.dumps([1 if on0 else 0]).replace(' ', ''), on0]]
        check(flips == want, f'double-clicking the HARMONY tab turns it off and on, once each ({flips})')
        # a smart voice's screen shows its semitones, and they change with the mode
        live1 = "document.querySelector('.hm-int .live').textContent"
        await pg.evaluate("(()=>{const S=obsHarmony.state; S.voices[0].smart=1; S.voices[0].step=2; S.key=0; S.mode=obsHarmony.modes.findIndex(m=>/^(major|ionian)/i.test(m.name)); obsHarmony.redraw();})()")
        a = await pg.evaluate(live1)
        await pg.evaluate("(()=>{obsHarmony.state.mode=obsHarmony.modes.findIndex(m=>/^(natural )?minor|aeolian/i.test(m.name)); obsHarmony.redraw();})()")
        b2 = await pg.evaluate(live1)
        check('+4' in a and '+3' in b2, f'a smart voice shows its semitones and they change with the mode ({a} -> {b2})')
        tab = await pg.evaluate("[...document.querySelectorAll('nav span, .tab span, button span')].some(e=>e.textContent.trim()==='HARMONY')")
        check(tab, 'HARMONY tab in the chain')

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
        # pedals with a design show their own face, lit when on
        await pg.locator('.page[data-page="PEDALS"] .obs-slot').nth(2).click(); await pg.wait_for_timeout(150)
        await pg.locator('.page[data-page="PEDALS"] .obs-tile[data-mod="ember"]').click(); await pg.wait_for_timeout(400)
        face = pg.locator('.page[data-page="PEDALS"] .obs-slot').nth(2)
        imgs = await face.evaluate("b=>[...b.querySelectorAll('.obs-pface img')].filter(i=>i.complete&&i.naturalWidth>0).length")
        lit = await face.evaluate("b=>getComputedStyle(b.querySelector('.pf-on')).opacity")
        await face.locator('.pf-foot').click(); await pg.wait_for_timeout(700)
        unlit = await face.evaluate("b=>getComputedStyle(b.querySelector('.pf-on')).opacity")
        check(imgs == 2 and lit == '1' and unlit == '0' and await pg.evaluate("window.__mock.sent('pre.3.on')") == 0, f'EMBER shows its own face, lit when on; its drawn footswitch turns it off ({imgs} images, {lit} -> {unlit})')
        # a pedal that arrives while another tab is showing (a preset loaded from the AMP page) is full size too
        w_vis = await face.evaluate("b=>b.querySelector('.obs-pface').offsetWidth")
        await pg.evaluate("setTab('AMP')"); await pg.wait_for_timeout(100)
        await pg.evaluate("(()=>{const r=JSON.parse(JSON.stringify(window.__mock.state.rig)); r.racks[0][3]={key:'ember',name:'Ember Fuzz',file:'',fileName:''}; window.__mock.emit('obs:rig', r);})()")
        await pg.wait_for_timeout(150); await pg.evaluate("setTab('PEDALS')"); await pg.wait_for_timeout(200)
        w_hid = await pg.locator('.page[data-page="PEDALS"] .obs-slot').nth(3).evaluate("b=>(b.querySelector('.obs-pface')||{}).offsetWidth")
        check(w_vis >= 100 and w_hid == w_vis, f'a pedal loaded while another tab shows is full size ({w_hid} px, like {w_vis} px)')
        await pg.evaluate("window.__mock.emit('obs:rig', window.__mock.state.rig)"); await pg.wait_for_timeout(100)
        await face.locator('.pf-foot').click(); await pg.wait_for_timeout(200)
        await pg.screenshot(path=str(OUT / 'pedals-faces.png'))
        # drag slot 2 onto slot 4
        a = await pg.locator('.page[data-page="PEDALS"] .obs-slot').nth(1).bounding_box()
        c = await pg.locator('.page[data-page="PEDALS"] .obs-slot').nth(3).bounding_box()
        await pg.mouse.move(a['x'] + 75, a['y'] + 60); await pg.mouse.down()
        await pg.mouse.move(c['x'] + 75, c['y'] + 60, steps=10); await pg.mouse.up()
        await pg.wait_for_timeout(200)
        L = await pg.evaluate("window.__mock.log.filter(x=>x[0]==='moveModule')")
        check(L and L[-1][1:] == [0, 1, 3], f'dragging a pedal reorders it ({L[-1] if L else None})')

        # ---- FX + special FX libraries
        for page, want in (('FX', 'crypt'), ('SFX', 'seraph')):
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
        cab_on_amp = await pg.evaluate("[...document.querySelectorAll('.page[data-page=\"AMP\"] button')].filter(b=>b.offsetParent && /ADD CAB|CAB/.test(b.textContent) && !/CHANGE AMP/.test(b.textContent)).length")
        check(cab_on_amp == 0, f'AMP page has no cab section (cabs are picked on the CAB page)')
        based = await pg.evaluate("document.querySelector('.obs-amp .obs-based').textContent")
        check(len(based.strip()) > 12, f'amp says what it is based on ({based.strip()})')
        tint = await pg.evaluate("getComputedStyle(document.querySelector('.obs-amp')).backgroundColor")
        check(tint not in ('', 'rgba(0, 0, 0, 0)', 'transparent'), f'amp panel has its own tint ({tint})')
        await pg.screenshot(path=str(OUT / 'amp.png'))
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.obs-ampchange').click(); await pg.wait_for_timeout(150)
        inhouse = [a for a in state['amps'] if a.get('folder') == 'OBSIDIAN AMPS']
        folders = await pg.evaluate("[...document.querySelectorAll('.obs-ampfolder')].filter(b=>b.offsetParent).map(b=>b.dataset.ampFolder)")
        top = await pg.evaluate("[...document.querySelectorAll('.obs-tile[data-amp]')].filter(b=>b.offsetParent).map(b=>b.dataset.amp)")
        check(folders[:4] == ['CLEAN', 'CRUNCH', 'RHYTHM', 'LEAD'] and 'ALL AMPS' in folders and top == ['nam-loader'],
              f'amp chooser opens on folders by sound ({folders}), the NAM loader on its own')
        await pg.screenshot(path=str(OUT / 'amp-library.png'))
        await pg.locator('.obs-ampfolder[data-amp-folder="CLEAN"]:visible').click(); await pg.wait_for_timeout(150)
        clean = await pg.evaluate("[...document.querySelectorAll('.obs-tile[data-amp]')].filter(b=>b.offsetParent).map(b=>b.dataset.amp+':'+b.querySelector('i').textContent)")
        check(any(x.startswith('glass65') for x in clean) and any(x.startswith('moirai:') and 'clean' in x.lower() for x in clean) and not any(x.startswith('kratos') for x in clean),
              f'CLEAN holds the clean amps and the clean channels of bigger amps ({len(clean)}: {clean[:4]}...)')
        await pg.screenshot(path=str(OUT / 'amp-folder.png'))
        await pg.locator('.obs-tile[data-amp="moirai"]:visible').click(); await pg.wait_for_timeout(300)
        log = await pg.evaluate("window.__mock.log.filter(x=>['setAmp','ampDim'].includes(x[0])).map(x=>x.join(':'))")
        check(log[-2:] == ['setAmp:moirai', 'ampDim:0:0'], f'picking MOIRAI from CLEAN starts it on its clean channel ({log[-2:]})')
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.obs-fchange').click(); await pg.wait_for_timeout(150)
        await pg.locator('.obs-ampfolder[data-amp-folder="LEAD"]:visible').click(); await pg.wait_for_timeout(150)
        await pg.locator('.obs-tile[data-amp="moirai"]:visible').click(); await pg.wait_for_timeout(300)
        log = await pg.evaluate("window.__mock.log.filter(x=>['setAmp','ampDim','setParam'].includes(x[0])).map(x=>x.slice(0,3).join(':'))")
        check('setAmp:moirai' in log and 'ampDim:0:2' in log and any(x.startswith('setParam:amp.gain') for x in log), f'from LEAD it starts on the lead channel with more gain ({log})')
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.obs-fchange').click(); await pg.wait_for_timeout(150)
        await pg.locator('.pop:visible input').fill('jcm'); await pg.wait_for_timeout(150)
        check(await pg.locator('.obs-tile[data-amp]:visible').count() == 1, 'search finds an amp once, whatever its folders (jcm)')
        await pg.locator('.pop:visible input').fill(''); await pg.wait_for_timeout(100)
        await pg.locator('.obs-ampfolder[data-amp-folder="ALL AMPS"]:visible').click(); await pg.wait_for_timeout(150)
        allin = await pg.locator('.obs-tile[data-amp]:visible').count()
        check(allin == len(state['amps']), f'ALL AMPS lists every amp once ({allin})')
        await pg.locator('.obs-crumb-back:visible').click(); await pg.wait_for_timeout(150)
        check(await pg.locator('.obs-ampfolder:visible').count() == len(folders), 'the back button returns to the folders')
        await pg.locator('.obs-ampfolder[data-amp-folder="ALL AMPS"]:visible').click(); await pg.wait_for_timeout(150)
        await pg.locator('.obs-tile[data-amp="brit800"]:visible').click(); await pg.wait_for_timeout(250)
        check(await pg.evaluate("window.__mock.log.some(x=>x[0]==='setAmp'&&x[1]==='brit800')"), 'choosing BRIT 800 in the amp list')
        check('JCM800' in await pg.evaluate("document.querySelector('.obs-amp .obs-based').textContent"), 'panel switches to BRIT 800')
        await pg.screenshot(path=str(OUT / 'amp-brit.png'))
        await pg.locator('.obs-ampchange').click(); await pg.wait_for_timeout(150)
        await pg.locator('.obs-tile[data-amp="nam-loader"]').click(); await pg.wait_for_timeout(250)
        check(await pg.evaluate("window.__mock.log.some(x=>x[0]==='setAmp'&&x[1]==='nam-loader')"), 'choosing Eidolon (the NAM loader) in the amp list')
        await pg.wait_for_timeout(400)
        scr = lambda: pg.evaluate("[...document.querySelectorAll('.ff-screen .sc-top > *')].map(e=>e.textContent)")
        imgs = await pg.evaluate("[...document.querySelectorAll('.obs-face img')].filter(i=>i.complete&&i.naturalWidth>0).length")
        s0 = await scr()
        check(await pg.locator('.obs-face:visible').count() == 1 and imgs >= 8 and s0[1] == 'Empty slot', f'the NAM loader is Eidolon: its own face and screen ({imgs} layers, {s0})')
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.obs-face .ff-btn[data-b="load"]').click(); await pg.wait_for_timeout(300)
        s1 = await scr()
        check(await pg.evaluate("window.__mock.log.some(x=>x[0]==='loadCapture'&&x[1]===0)") and s1[1] == '5150 Red Lead' and 'Peavey 5150' in s1[2] and s1[3] == 'AMP ONLY',
              f'LOAD opens a capture into slot 1; the screen shows its name, gear and type ({s1})')
        await pg.locator('.obs-face .ff-btn[data-b="load"]').click(); await pg.wait_for_timeout(300)
        s2 = await scr()
        check(await pg.evaluate("window.__mock.log.some(x=>x[0]==='loadCapture'&&x[1]===1)") and s2[0].startswith('SLOT 2 / 4') and s2[1] == 'Plexi Crunch', f'LOAD again fills the next empty slot ({s2[:2]})')
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.obs-face .ff-btn[data-b="prev"]').click(); await pg.wait_for_timeout(250)
        s3 = await scr()
        check(s3[0].startswith('SLOT 1') and await pg.evaluate("window.__mock.sent('amp.channel')") == 0, f'PREV steps back to the capture before ({s3[:2]})')
        await pg.locator('.obs-face .ff-btn[data-b="next"]').click(); await pg.wait_for_timeout(250)
        check((await scr())[0].startswith('SLOT 2'), 'NEXT steps forward')
        await pg.evaluate("window.__mock.log.length=0")
        k = pg.locator('.obs-face .ff-hot[data-k="0"]'); box = await k.bounding_box()
        await pg.mouse.move(box['x'] + box['width'] / 2, box['y'] + box['height'] / 2); await pg.mouse.down()
        await pg.mouse.move(box['x'] + box['width'] / 2, box['y'] - 30, steps=5)
        hot = await pg.evaluate("[...document.querySelectorAll('.ff-screen .sc-vals span.hot')].map(e=>e.textContent)")
        await pg.mouse.up(); await pg.wait_for_timeout(150)
        check(await pg.evaluate("window.__mock.log.some(x=>x[0]==='setParam'&&x[1]==='amp.gain')") and hot and hot[0].startswith('GAIN'), f'GAIN drives the amp, and its value lights up on the screen ({hot})')
        k = pg.locator('.obs-face .ff-hot[data-k="6"]'); box = await k.bounding_box()
        await pg.mouse.move(box['x'] + box['width'] / 2, box['y'] + box['height'] / 2); await pg.mouse.down()
        await pg.mouse.move(box['x'] + box['width'] / 2, box['y'] + 200, steps=6); await pg.mouse.up(); await pg.wait_for_timeout(250)
        check((await scr())[0].startswith('SLOT 1'), 'BROWSE sweeps through the slots')
        await pg.screenshot(path=str(OUT / 'amp-nam.png'))
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.obs-fslots [data-clear="1"]').click(); await pg.wait_for_timeout(200)
        check(await pg.evaluate("window.__mock.log.some(x=>x[0]==='clearCapture'&&x[1]===1)"), 'a slot under the amp can be emptied (✕)')

        # ---- capture amps: the real faces
        await pg.locator('.obs-fchange').click(); await pg.wait_for_timeout(150)
        await pg.locator('.obs-ampfolder[data-amp-folder="ALL AMPS"]:visible').click(); await pg.wait_for_timeout(300)
        pics = await pg.evaluate("[...document.querySelectorAll('.obs-tile[data-amp] .obs-tilepic')].filter(e=>e.offsetParent).length")
        check(pics == 4, f'capture amps and Eidolon show a picture of their face ({pics})')
        await pg.screenshot(path=str(OUT / 'amp-capture-folder.png'))
        await pg.locator('.obs-tile[data-amp="umbra"]:visible').click(); await pg.wait_for_timeout(600)
        loaded = await pg.evaluate("[...document.querySelectorAll('.obs-face img')].filter(i=>i.complete&&i.naturalWidth>0).length")
        total = await pg.evaluate("document.querySelectorAll('.obs-face img').length")
        check(await pg.locator('.obs-face:visible').count() == 1 and loaded == total and total > 10, f'UMBRA shows its real face ({loaded}/{total} layers loaded)')
        await pg.evaluate("window.__mock.log.length=0")
        k = pg.locator('.obs-face .ff-hot[data-k="0"]'); box = await k.bounding_box()
        await pg.mouse.move(box['x'] + box['width'] / 2, box['y'] + box['height'] / 2); await pg.mouse.down()
        await pg.mouse.move(box['x'] + box['width'] / 2, box['y'] - 30, steps=5); await pg.mouse.up(); await pg.wait_for_timeout(150)
        cmds = await pg.evaluate("window.__mock.log.map(x=>x[0]+':'+(x[1]||''))")
        check('beginGesture:amp.gain' in cmds and 'setParam:amp.gain' in cmds, 'dragging the Gain knob on the face moves amp.gain (with a gesture)')
        rot = await pg.evaluate("document.querySelector('.obs-face .ff-ptr[data-k=\"0\"][data-v=\"on\"]').style.transform")
        check('rotate' in rot, f'its pointer turns ({rot})')
        await pg.evaluate("window.__mock.log.length=0")
        k = pg.locator('.obs-face .ff-hot[data-k="6"]'); box = await k.bounding_box()
        await pg.mouse.move(box['x'] + box['width'] / 2, box['y'] + box['height'] / 2); await pg.mouse.down()
        await pg.mouse.move(box['x'] + box['width'] / 2, box['y'] - 20, steps=4); await pg.mouse.up(); await pg.wait_for_timeout(150)
        check(await pg.evaluate("window.__mock.log.some(x=>x[0]==='ampKnob'&&x[1]===6)"), 'the Output knob (not a plugin knob) goes to the amp (ampKnob)')
        await pg.evaluate("window.__mock.log.length=0")
        leds0 = await pg.evaluate("[...document.querySelectorAll('.obs-face .ff-led')].filter(i=>i.style.opacity==='1').map(i=>i.dataset.l).join(',')")
        await pg.locator('.obs-face .ff-btn[data-b="channel"]').click(); await pg.wait_for_timeout(200)
        leds1 = await pg.evaluate("[...document.querySelectorAll('.obs-face .ff-led')].filter(i=>i.style.opacity==='1').map(i=>i.dataset.l).join(',')")
        check(await pg.evaluate("window.__mock.log.some(x=>x[0]==='ampDim'&&x[1]===0)") and leds0 != leds1, f'CHANNEL button switches the channel and its LED ({leds0} -> {leds1})')
        await pg.locator('.obs-face .ff-btn[data-b="mode"]').click(); await pg.wait_for_timeout(200)
        check(await pg.evaluate("window.__mock.log.some(x=>x[0]==='ampDim'&&x[1]===1)"), 'MODE button switches the mode')
        await pg.evaluate("window.__mock.meterExtra = {ampA: 3, ampB: 4, ampF: .4, cabIn: false}"); await pg.wait_for_timeout(300)
        rd = await pg.evaluate("document.querySelector('.obs-faceread').textContent")
        check('Channel' in rd and 'Mesa' not in rd and '.nam' not in rd and '[18dBu]' not in rd, f'readout shows the channel and mode, no file names ({rd[:90]})')
        await pg.screenshot(path=str(OUT / 'amp-face.png'))
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.obs-face .ff-pw').click(); await pg.wait_for_timeout(300)
        offop = await pg.evaluate("document.querySelector('.obs-face .ff-base.on[data-s=\"0\"]').style.opacity")
        check(await pg.evaluate("window.__mock.sent('amp.on')") == 0 and offop == '0', 'the power lamp switches the amp off (amp.on = 0, face goes dark)')
        await pg.locator('.obs-face .ff-pw').click(); await pg.wait_for_timeout(200)
        await pg.evaluate("window.__mock.meterExtra = null")
        await pg.locator('.obs-fchange').click(); await pg.wait_for_timeout(150)
        await pg.locator('.obs-ampfolder[data-amp-folder="ALL AMPS"]:visible').click(); await pg.wait_for_timeout(200)
        await pg.locator('.obs-tile[data-amp="moirai"]:visible').click(); await pg.wait_for_timeout(600)
        skin0 = await pg.evaluate("[...document.querySelectorAll('.obs-face .ff-base.off')].findIndex(i=>i.style.opacity==='1')")
        await pg.locator('.obs-face .ff-btn[data-b="ch1"]').click(); await pg.wait_for_timeout(250)
        skin1 = await pg.evaluate("[...document.querySelectorAll('.obs-face .ff-base.off')].findIndex(i=>i.style.opacity==='1')")
        check(skin0 == 2 and skin1 == 0, f'MOIRAI changes colour with the channel (red -> green: skin {skin0} -> {skin1})')
        await pg.screenshot(path=str(OUT / 'amp-face-moirai.png'))
        await pg.locator('.obs-fchange').click(); await pg.wait_for_timeout(150)
        await pg.locator('.obs-ampfolder[data-amp-folder="ALL AMPS"]:visible').click(); await pg.wait_for_timeout(200)
        await pg.locator('.obs-tile[data-amp="kratos"]:visible').click(); await pg.wait_for_timeout(600)
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.obs-face .ff-btn[data-b="crunch"]').click(); await pg.wait_for_timeout(200)
        lit = await pg.evaluate("document.querySelector('.obs-face .ff-led[data-l=\"crunch_led\"]').style.opacity")
        check(await pg.evaluate("window.__mock.log.some(x=>x[0]==='ampSwitch'&&x[1]===0&&x[2]===true)") and lit == '1', 'KRATOS: the CRUNCH switch toggles and lights')
        await pg.screenshot(path=str(OUT / 'amp-face-kratos.png'))
        await pg.evaluate("setTab('CAB')"); await pg.wait_for_timeout(150)
        await pg.screenshot(path=str(OUT / 'cab.png'))
        txtpos = await pg.evaluate("document.querySelector('[data-param=\"cab.a.level\"]').parentElement.querySelector('.kn-val').textContent")
        check('dB' in txtpos, f'cab mic level shows dB ({txtpos})')

        # the level meters sit inside the top bar, both sides the same distance from its edge
        mt = await pg.evaluate("""(()=>{const bar=[...document.querySelectorAll('.kn')].find(k=>!k.closest('.page')&&!k.closest('.ov')).closest('div[style*="height: 100px"]').getBoundingClientRect();
          const ms=meters.map(m=>m.d.parentElement.getBoundingClientRect()); return [Math.round(ms[0].left-bar.left), Math.round(bar.right-ms[ms.length-1].right)]})()""")
        check(mt[0] >= 8 and mt[1] >= 8 and abs(mt[0] - mt[1]) <= 3, f'the in/out meters are inside the top bar ({mt[0]} px / {mt[1]} px from its edges)')
        # ---- presets
        await pg.locator('[aria-label="Previous preset"]').evaluate('b=>b.nextElementSibling.click()')
        await pg.wait_for_timeout(200)
        rows = await pg.locator('.obs-prow').count()
        n_fac = sum(1 for e in state['presets'] if e['factory'] and not e['category'].startswith('Artists/'))
        n_art = sum(1 for e in state['presets'] if e['category'].startswith('Artists/'))
        folders = await pg.evaluate("[...document.querySelectorAll('.obs-folder')].map(f=>f.dataset.folder)")
        # the browser opens on the loaded preset's folder; go to the top of Factory
        await pg.locator('.obs-folder[data-folder="FACTORY"]').click(); await pg.wait_for_timeout(150)
        if await pg.locator('.obs-pdir').count() == 0: await pg.locator('.obs-folder[data-folder="FACTORY"]').click(); await pg.wait_for_timeout(150)
        dirs = await pg.evaluate("[...document.querySelectorAll('.obs-pdir')].map(d=>d.querySelector('b').textContent)")
        rows0 = await pg.locator('.obs-prow').count()
        check(rows0 == 0 and dirs[:4] == ['Clean', 'Crunch', 'Rhythm', 'Lead'] and len(dirs) == 8 and 'All presets' not in folders and folders[:1] == ['FACTORY'] and 'ARTISTS' in folders and 'MY' in folders,
              f'Factory shows its folders, not every preset ({dirs})')
        await pg.locator('.obs-pdir[data-folder="FACTORY/Clean"]').click(); await pg.wait_for_timeout(150)
        clean = await pg.locator('.obs-prow').count(); up = await pg.locator('.obs-pup').count()
        n_clean = sum(1 for e in state['presets'] if e['factory'] and e['category'] == 'Clean')
        check(clean == n_clean and up == 1, f'going into Clean lists its {clean} presets, with a way back')
        await pg.locator('.obs-pup').click(); await pg.wait_for_timeout(150)
        check(await pg.locator('.obs-pdir').count() == 8 and await pg.locator('.obs-prow').count() == 0, 'the back row goes up to Factory again')
        await pg.locator('.obs-folder[data-folder="ARTISTS"]').click(); await pg.wait_for_timeout(150)
        arts = await pg.locator('.obs-pdir').count()
        rows_all = await pg.locator('.obs-prow').count()
        await pg.locator('.obs-pdir[data-folder="ARTISTS/Metallica"]').click(); await pg.wait_for_timeout(150)
        met = await pg.evaluate("[...document.querySelectorAll('.obs-prow')].map(r=>r.children[1].textContent)")
        check(arts >= 40 and rows_all == 0 and 'Enter Sandman' in met and 'Master of Puppets' in met and len(met) >= 10,
              f'ARTISTS shows {arts} artist folders (no presets until you open one); Metallica lists {len(met)} (Enter Sandman, Master of Puppets...)')
        await pg.screenshot(path=str(OUT / 'presets.png'))
        await pg.locator('#preset-search').fill('sandman'); await pg.wait_for_timeout(120)
        hits = await pg.evaluate("[...document.querySelectorAll('.obs-prow')].map(r=>r.children[1].textContent)")
        check(hits == ['Enter Sandman'], f'search looks in every folder ({hits})')
        await pg.locator('#preset-search').fill(''); await pg.wait_for_timeout(80)
        await pg.locator('.obs-folder[data-folder="ARTISTS/Metallica"]').click(); await pg.wait_for_timeout(100)
        want = met[2]
        await pg.locator('.obs-prow').nth(2).dblclick(); await pg.wait_for_timeout(200)
        name = await pg.evaluate("document.querySelector('[aria-label=\"Previous preset\"]').nextElementSibling.textContent.trim()")
        check(name == want, f'double-click loads a preset ({name})')
        locked = await pg.evaluate("[...document.querySelectorAll('button')].find(b=>b.textContent.trim()==='DELETE'&&!b.closest('.ov')).classList.contains('obs-locked')")
        await pg.evaluate("window.__mock.log.length=0")
        await pg.evaluate("[...document.querySelectorAll('button')].find(b=>b.textContent.trim()==='DELETE'&&!b.closest('.ov')).click()"); await pg.wait_for_timeout(120)
        check(locked and await pg.locator('.obs-dialog').count() == 0 and not await pg.evaluate("window.__mock.log.some(x=>x[0]==='presetDelete')"),
              'artist and factory presets are locked: DELETE is greyed out and does nothing')
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('[aria-label="Next preset"]').click(); await pg.wait_for_timeout(150)
        check(await pg.evaluate("window.__mock.log.some(x=>x[0]==='presetNext'&&x[1]===1)"), 'next-preset arrow')
        await pg.locator('button', has_text='SAVE AS').first.click(); await pg.wait_for_timeout(100)
        check(await pg.locator('.obs-dialog input').count() == 1, 'SAVE AS asks for a name')
        await pg.locator('.obs-dialog input').fill('My Wall'); await pg.locator('.obs-dialog [data-ok]').click(); await pg.wait_for_timeout(150)
        check(await pg.evaluate("window.__mock.log.some(x=>x[0]==='presetSaveAs'&&x[1]==='My Wall')"), 'SAVE AS saves under the new name')
        # ---- your own folders
        await pg.wait_for_timeout(150)
        unlocked = not await pg.evaluate("[...document.querySelectorAll('button')].find(b=>b.textContent.trim()==='DELETE'&&!b.closest('.ov')).classList.contains('obs-locked')")
        await pg.evaluate("openOv('presets')"); await pg.wait_for_timeout(200)
        names = lambda: pg.evaluate("[...document.querySelectorAll('.obs-prow')].map(r=>r.children[1].textContent)")
        dirs = await pg.evaluate("[...document.querySelectorAll('.obs-pdir')].map(d=>d.dataset.folder)")
        rows = await names()
        check(unlocked and dirs == ['MY/Drop D', 'MY/Live set'] and 'My Wall' in rows and len(rows) == 11,
              f'My presets shows your folders ({dirs}), then your loose presets ({len(rows)})')
        await pg.locator('.obs-prow', has_text='My Wall').click(); await pg.wait_for_timeout(80)
        mine_btns = await pg.evaluate("['.obs-pdel','.obs-pmove'].every(c=>document.querySelector(c).offsetParent!==null)")
        await pg.locator('button.obs-newfolder').click(); await pg.wait_for_timeout(100)
        await pg.locator('.obs-dialog input').fill('Solos'); await pg.locator('.obs-dialog [data-ok]').click(); await pg.wait_for_timeout(250)
        here = await pg.evaluate("(document.querySelector('.obs-pup b')||{}).textContent")
        side = await pg.evaluate("[...document.querySelectorAll('.obs-folder')].map(f=>f.dataset.folder)")
        check(mine_btns and here == 'Solos' and 'MY/Solos' in side and await pg.locator('.obs-pfren').count() == 1 and await pg.locator('.obs-empty').count() == 1,
              'NEW FOLDER makes a folder and opens it (empty, with RENAME / DELETE for it)')
        await pg.locator('.obs-pup').click(); await pg.wait_for_timeout(150)
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.obs-prow', has_text='My Wall').drag_to(pg.locator('.obs-pdir[data-folder="MY/Solos"]')); await pg.wait_for_timeout(200)
        moved = await pg.evaluate("window.__mock.log.some(x=>x[0]==='presetMove'&&x[1]==='user:My Wall'&&x[2]==='Solos')")
        await pg.locator('.obs-pdir[data-folder="MY/Solos"]').click(); await pg.wait_for_timeout(150)
        check(moved and await names() == ['My Wall'], 'drag a preset onto a folder to move it there')
        await pg.screenshot(path=str(OUT / 'presets-mine.png'))
        await pg.locator('.obs-prow', has_text='My Wall').click(); await pg.wait_for_timeout(60)
        await pg.locator('.obs-pmove').click(); await pg.wait_for_timeout(100)
        await pg.locator('.obs-dialog select').select_option('Drop D'); await pg.locator('.obs-dialog [data-ok]').click(); await pg.wait_for_timeout(200)
        await pg.locator('.obs-pup').click(); await pg.wait_for_timeout(120)
        await pg.locator('.obs-pdir[data-folder="MY/Drop D"]').click(); await pg.wait_for_timeout(150)
        dd = await names()
        check('My Wall' in dd and len(dd) == 3, f'MOVE TO FOLDER moves the selected preset ({dd})')
        await pg.locator('.obs-pfren').click(); await pg.wait_for_timeout(100)
        await pg.locator('.obs-dialog input').fill('Drop C'); await pg.locator('.obs-dialog [data-ok]').click(); await pg.wait_for_timeout(250)
        here = await pg.evaluate("(document.querySelector('.obs-pup b')||{}).textContent")
        check(here == 'Drop C' and len(await names()) == 3, 'rename a folder (it stays open, presets and all)')
        await pg.locator('.obs-pfdel').click(); await pg.wait_for_timeout(100)
        warn = await pg.locator('.obs-dialog p').text_content()
        await pg.locator('.obs-dialog [data-ok]').click(); await pg.wait_for_timeout(250)
        dirs = await pg.evaluate("[...document.querySelectorAll('.obs-pdir')].map(d=>d.dataset.folder)")
        check('3 presets' in warn and dirs == ['MY/Live set', 'MY/Solos'] and await pg.locator('.obs-pup').count() == 0,
              'deleting a folder warns how many presets go with it, then goes back to My presets')
        await pg.locator('#preset-search').fill('sandman'); await pg.wait_for_timeout(120)
        await pg.locator('.obs-prow').first.click(); await pg.wait_for_timeout(80)
        hidden = await pg.evaluate("['.obs-pdel','.obs-pmove'].every(c=>document.querySelector(c).offsetParent===null)")
        lockln = await pg.evaluate("[...document.querySelectorAll('.obs-lockline')].map(e=>e.offsetParent?e.textContent:'').join('')")
        await pg.evaluate("window.__mock.log.length=0")
        check(hidden and lockln == 'Built in · locked' and not await pg.evaluate("document.querySelector('.obs-prow').classList.contains('obs-pmine')"),
              'a factory/artist preset has no DELETE / MOVE, says it is locked, and can\'t be dragged')
        await pg.locator('#preset-search').fill(''); await pg.wait_for_timeout(60)
        await pg.evaluate("closeOv && closeOv()"); await pg.wait_for_timeout(100)

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
        sc = await pg.evaluate("[getComputedStyle(document.getElementById('thscene')).display, document.querySelectorAll('#thscene svg *').length, document.documentElement.classList.contains('thstill')]")
        check(sc[0] == 'block' and sc[1] > 50 and sc[2], f'Norse draws its scene behind the pages ({sc[1]} shapes), and it stops moving with the details off')
        counts = {}
        for tid in ids:
            await pg.evaluate(f"applyTheme('{tid}')")
            counts[tid] = await pg.evaluate("document.documentElement.classList.contains('thscene') ? document.querySelectorAll('#thscene svg *').length : 0")
        await pg.evaluate("applyTheme('norse')")
        check(all(counts[t] > 50 for t in ids if t not in ('obsidian', 'matte')) and counts['obsidian'] == 0 and counts['matte'] == 0, f'every theme but Obsidian and Matte has a scene ({counts})')
        await pg.locator('[aria-label="Moving theme details"]').click(); await pg.locator('.thc[data-t="obsidian"]').click(); await pg.wait_for_timeout(150)
        check(await pg.evaluate("getComputedStyle(document.querySelector('.thband')).display") == 'none', 'back to Obsidian: no bands')
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.obs-livetog').click(); await pg.wait_for_timeout(120)
        check(await pg.evaluate("window.__mock.log.some(x=>x[0]==='setSetting'&&x[1]==='liveUi'&&x[2]===true)") and await pg.locator('.obs-livereload').count() == 1,
              'Settings > Interface: Live UI switch (+ RELOAD) is saved')
        await pg.locator('.obs-livetog').click(); await pg.wait_for_timeout(80)
        await pg.locator('.setnav button', has_text='Audio').click()
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.spanel[data-panel="Audio"] .seg button', has_text='HQ').click(); await pg.wait_for_timeout(120)
        check(await pg.evaluate("window.__mock.sent('transpose.hq')") == 1, 'Settings > Audio: transpose HQ switch')
        await pg.locator('.spanel[data-panel="Audio"] .seg button', has_text='4×').click(); await pg.wait_for_timeout(120)
        check(await pg.evaluate("window.__mock.log.some(x=>x[0]==='setSetting'&&x[1]==='oversampling'&&x[2]===4)"), 'Settings > Audio: oversampling 4×')
        # ---- Settings > About: the update button
        await pg.locator('.setnav button', has_text='About').click(); await pg.wait_for_timeout(120)
        about = await pg.evaluate("[...document.querySelectorAll('.spanel[data-panel=About] span')].map(s=>s.textContent.trim())")
        msg = lambda: pg.evaluate("document.querySelector('.obs-upd-msg').textContent")
        go = lambda: pg.evaluate("document.querySelector('.obs-upd-go').textContent")
        check('Build' in about and 'made by hand' in about and await pg.locator('.obs-upd').is_visible() and 'made by hand' in await msg(),
              f'Settings > About shows the build and an UPDATES box ({await msg()})')
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.obs-upd-check').click(); await pg.wait_for_timeout(450)
        m1, g1 = await msg(), await go()
        check(await pg.evaluate("window.__mock.log.some(x=>x[0]==='updateCheck')") and 'Build 1 is ready' in m1 and g1 == 'UPDATE'
              and await pg.locator('.obs-upd-notes').is_visible(), f'CHECK NOW finds a newer build and shows what changed ({m1})')
        await pg.screenshot(path=str(OUT / 'settings-about.png'))
        await pg.locator('.obs-upd-go').click(); await pg.wait_for_timeout(200)
        m2, bar = await msg(), await pg.locator('.obs-upd-bar').is_visible()
        await pg.wait_for_timeout(700)
        check('Downloading' in m2 and bar and await pg.evaluate("window.__mock.log.some(x=>x[0]==='updateInstall')"),
              f'UPDATE downloads it with a progress bar, then installs ({m2})')
        await pg.evaluate("window.__mock.log.length=0")
        await pg.locator('.obs-upd-tog').click(); await pg.wait_for_timeout(100)
        check(await pg.evaluate("window.__mock.log.some(x=>x[0]==='setSetting'&&x[1]==='checkUpdates'&&x[2]===false)"), '"Check when it opens" can be switched off')
        await pg.locator('.obs-upd-tog').click(); await pg.wait_for_timeout(60)
        if standalone:
            await pg.locator('.setnav button', has_text='Device').click(); await pg.wait_for_timeout(100)
            dd = await pg.evaluate("[...document.querySelectorAll('.spanel[data-panel=Device] button')].map(b=>b.textContent.trim()).filter(Boolean)")
            check(any('128 samples' in x for x in dd) and any('Focusrite' in x for x in dd), 'Device panel shows the real interface + buffer')
            check(await pg.locator('.obs-chk-in').count() == 2, 'Device panel lists the real input channels')
        else:
            check(await pg.locator('#f0 button', has_text='STUDIO').is_hidden(), 'STUDIO is hidden inside a DAW')
        await pg.evaluate("closeOv()")
        if standalone:
            # ---- STUDIO: each track shows its pan; a double-click on the pan slider centres it
            await pg.locator('#f0 button', has_text='STUDIO').click(); await pg.wait_for_timeout(300)
            pan = pg.locator('#studio .st-head').first.locator('.st-pan'); box = await pan.bounding_box()
            await pg.mouse.click(box['x'] + box['width'] * .9, box['y'] + box['height'] / 2); await pg.wait_for_timeout(500)
            v1 = await pg.locator('#studio .st-head').first.locator('.st-panv').text_content()
            await pg.mouse.dblclick(box['x'] + box['width'] * .9, box['y'] + box['height'] / 2); await pg.wait_for_timeout(200)
            v2 = await pg.locator('#studio .st-head').first.locator('.st-panv').text_content()
            nm = await pg.locator('#studio .st-head').first.locator('.st-name').text_content()
            check(v1.startswith('R') and v2 == 'C' and not nm.startswith('Pan'), f'STUDIO: the pan shows next to the track name ({v1}), double-click centres it ({v2})')
            await pg.screenshot(path=str(OUT / 'studio.png'))
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
