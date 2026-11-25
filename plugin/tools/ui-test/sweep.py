#!/usr/bin/env python3
"""Control sweep: tries every control on every page of the real UI and reports the ones that do nothing.

    python3 plugin/tools/ui-test/sweep.py [--standalone] [--page NAME]     # report in out/sweep.json

For every visible control bound to a plugin parameter (data-param) on every page (and sub-page):
  drag it up and down, scroll it, double-click it (knobs) or click it (buttons, switches)
and checks that it moves on screen, shows its new value, and that the value reaches the engine. It also
clicks every other visible button and reports page errors, and samples every canvas / meter twice to see which
displays move. Uses the same page and fake engine as run.py.
"""
import asyncio, json, os, sys
from pathlib import Path
from playwright.async_api import async_playwright

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import run  # noqa: E402

OUT = HERE / 'out'

# JS helpers living in the page while the sweep runs
HELPERS = r"""
window.__sw = {
  vis(el) { if (!el || !el.isConnected) return false; const r = el.getBoundingClientRect(); if (r.width < 4 || r.height < 4) return false;
    if (r.bottom < 0 || r.right < 0 || r.top > innerHeight || r.left > innerWidth) return false;
    for (let e = el; e; e = e.parentElement) { const s = getComputedStyle(e); if (s.display === 'none' || s.visibility === 'hidden' || +s.opacity === 0) return false; }
    const x = r.left + r.width / 2, y = r.top + r.height / 2, t = document.elementFromPoint(x, y); return !!t && (el === t || el.contains(t) || t.contains(el)); },
  label(el) {
    const own = el.getAttribute('aria-label') || el.title || '';
    let box = el.parentElement, t = '';
    for (let i = 0; i < 3 && box && !t; i++, box = box.parentElement) {
      const sp = [...box.querySelectorAll('span,b,label')].filter(s => !el.contains(s) && /^[A-Z0-9][A-Z0-9 .\/&+'-]{1,24}$/.test(s.textContent.trim()));
      if (sp.length) t = sp[0].textContent.trim(); }
    return (t || own || el.textContent.trim().slice(0, 24)).replace(/\s+/g, ' '); },
  sig(el) {          // what the control looks like: its markup + the inline styles / classes below it + nearby value text
    let box = el.parentElement; for (let i = 0; i < 2 && box && box.parentElement; i++) box = box.parentElement;
    return el.outerHTML.length + ':' + el.outerHTML + '|' + (box ? box.innerText : ''); },
  text(el) { let box = el.parentElement; for (let i = 0; i < 2 && box && box.parentElement; i++) box = box.parentElement; return box ? box.innerText.replace(/\s+/g, ' ').slice(0, 200) : ''; },
  page() { const p = [...document.querySelectorAll('.page')].find(p => getComputedStyle(p).display !== 'none'); return p; },
  controls() {
    const pg = this.page(); const roots = [pg, ...document.querySelectorAll('.ov')].filter(Boolean);
    const out = []; const seen = new Set();
    roots.forEach(root => root.querySelectorAll('[data-param], .kn, .hm-kn, .sy-kn, .sy-mk, .sy-sel .dial, .ff-hot[data-k], .sy-pill').forEach(el => {
      if (el.closest('[data-param]') && el.closest('[data-param]') !== el) return;
      if (seen.has(el) || !this.vis(el)) return; seen.add(el); out.push(el); }));
    window.__swEls = out;
    return out.map((el, i) => { const r = el.getBoundingClientRect();
      return { i, id: el.dataset.param || '', choose: !!el.querySelector(':scope > button[data-i], :scope > button'), tag: el.tagName, cls: el.className && el.className.baseVal === undefined ? String(el.className) : '', label: this.label(el),
               x: r.left + r.width / 2, y: r.top + r.height / 2, w: r.width, h: r.height }; });
  },
  buttons() {
    const pg = this.page(); const out = [];
    pg.querySelectorAll('button').forEach(b => { if (b.closest('[data-param]') || !this.vis(b)) return; out.push(b); });
    window.__swBtns = out;
    return out.map((b, i) => { const r = b.getBoundingClientRect(); return { i, label: this.label(b), text: b.textContent.trim().slice(0, 30), x: r.left + r.width / 2, y: r.top + r.height / 2 }; });
  },
  sent(id) { return window.__mock.log.filter(x => id ? (x[0] === 'setParam' && x[1] === id) : !/^(ready|state|meters|libraries|beginGesture|endGesture)$/.test(x[0])).map(x => id ? x[2] : x[0]); },
  chooseBtn(el) { const bs = [...el.querySelectorAll(':scope > button')].filter(b => this.vis(b)); const b = bs.find(b => !b.classList.contains('on')) || bs[0];
    if (!b) return null; const r = b.getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2, bs.findIndex(x => x.classList.contains('on'))]; },
  meta(id) { const p = (window.__mock.state.params || []).find(p => p.id === id); if (!p) return null;
    const m = Object.assign({}, p), v = p.def;
    let n;
    if (p.type === 'bool') n = v >= .5 ? 1 : 0;
    else if (p.log) n = Math.log(v / p.min) / Math.log(p.max / p.min);
    else { n = Math.min(1, Math.max(0, (v - p.min) / (p.max - p.min))); if (p.type !== 'int' && p.type !== 'choice' && p.skew !== 1 && n > 0) n = Math.pow(n, p.skew); }
    m.defNorm = n; return m; },
  live() {          // a fingerprint of every display that should be moving
    const pg = this.page(); const out = {};
    [...pg.querySelectorAll('canvas')].filter(c => this.vis(c)).forEach((c, i) => { try { const d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data; let h = 0;
      for (let k = 0; k < d.length; k += 97) h = (h * 31 + d[k]) | 0; out['canvas#' + i + ' ' + (c.className || c.parentElement.className || '')] = h; } catch (e) {} });
    [...pg.querySelectorAll('svg')].filter(s => this.vis(s) && s.getBoundingClientRect().width > 60).forEach((s, i) => { out['svg#' + i + ' ' + (s.getAttribute('class') || s.parentElement.className || '')] = s.innerHTML.length + ':' + s.innerHTML.slice(0, 4000); });
    return out;
  },
};
"""


async def try_controls(pg, ctrls, errors, label, mod=None):
    """Generic: drag / scroll / double-click knobs, click buttons; returns the controls with their problems."""
    res = []
    for c in ctrls:
        errors.clear()
        el = f"window.__swEls[{c['i']}]"
        if not await pg.evaluate(f"__sw.vis({el})"): continue
        sig0 = await pg.evaluate(f"__sw.sig({el})")
        await pg.evaluate("window.__mock.log.length=0")
        r = dict(c); r['problems'] = []
        for dy in (-45, 90):
            await pg.mouse.move(c['x'], c['y']); await pg.mouse.down(); await pg.mouse.move(c['x'], c['y'] + dy, steps=6); await pg.mouse.up(); await pg.wait_for_timeout(100)
            sig1 = await pg.evaluate(f"__sw.sig({el})"); sent = await pg.evaluate(f"__sw.sent({json.dumps(c['id'])})")
            if sig1 != sig0: break
        if sig1 == sig0: r['problems'].append('drag: nothing moves')
        if not sent: r['problems'].append('drag: nothing sent')
        await pg.evaluate("window.__mock.log.length=0")
        await pg.mouse.move(c['x'], c['y']); await pg.mouse.wheel(0, 300); await pg.wait_for_timeout(100)
        await pg.mouse.dblclick(c['x'], c['y']); await pg.wait_for_timeout(120)
        sent3 = await pg.evaluate(f"__sw.sent({json.dumps(c['id'])})")
        meta = await pg.evaluate(f"__sw.meta({json.dumps(c['id'])})") if c['id'] else None
        if mod and meta:          # a pedal's knob: its own default, not the slot parameter's
            k = int(c['id'].split('.p')[-1]) - 1; mp = (mod.get('params') or [])[k] if k < len(mod.get('params') or []) else None
            meta = dict(meta, defNorm=mp['defNorm'], type='choice' if (mp.get('choices') or mp.get('toggle') or mp.get('momentary')) else 'float') if mp else None
        if meta and sent3 and meta.get('defNorm') is not None and abs(sent3[-1] - meta['defNorm']) > 0.02 and meta.get('type') == 'float':
            r['problems'].append(f"double-click: went to {sent3[-1]:.3f}, default is {meta['defNorm']:.3f}")
        if errors: r['problems'].append('errors: ' + ' / '.join(errors[:3]))
        res.append(r)
        if r['problems']: print(f"  BAD {label:<16} {c['id']:<16} {c['label'][:22]:<22} {'; '.join(r['problems'])}", flush=True)
    return res


async def sweep_modules(pg, tab, report, errors):
    """Every pedal / effect of this page's racks: put it in slot 1, open its editor (or face), try its knobs."""
    rack = {'PEDALS': 0, 'FX': 1}.get(tab, 2)
    mods = await pg.evaluate(f"(window.__mock.state.modules||[]).filter(m=>(m.racks||[]).includes({rack}) || (m.racks||[]).includes(['pre','post','sfx'][{rack}])).map(m=>m.key)")
    if not mods: mods = await pg.evaluate("(window.__mock.state.modules||[]).map(m=>m.key)")
    print(f"  {len(mods)} modules", flush=True)
    out = {}
    for key in mods:
        await pg.evaluate(f"""(()=>{{const r=JSON.parse(JSON.stringify(window.__mock.state.rig)); r.racks[{rack}][0]={{key:{json.dumps(key)},name:{json.dumps(key)},file:'',fileName:''}};
            window.__mock.state.rig=r; window.__mock.emit('obs:rig', r);}})()""")
        await pg.wait_for_timeout(150)
        # open its editor
        page = f'.page[data-page="{tab}"]'
        await pg.evaluate(f"""(()=>{{const s=document.querySelector('{page} .obs-slot'); if(s && !document.querySelector('{page} .obs-editor .kn:not([style*="display: none"])')) s.click();}})()""")
        await pg.wait_for_timeout(200)
        ctrls = await pg.evaluate(f"""(()=>{{ const pg=document.querySelector('{page}'); const els=[...pg.querySelectorAll('[data-param]')].filter(e=>/^(pre|post|sfx)\\.1\\./.test(e.dataset.param) && __sw.vis(e) && !/\\.on$/.test(e.dataset.param));
            window.__swEls=els; return els.map((el,i)=>{{const r=el.getBoundingClientRect(); return {{i,id:el.dataset.param,label:__sw.label(el),x:r.left+r.width/2,y:r.top+r.height/2}};}}); }})()""")
        n_def = await pg.evaluate(f"((window.__mock.state.modules||[]).find(m=>m.key==={json.dumps(key)})||{{params:[]}}).params.length")
        mod = await pg.evaluate(f"(window.__mock.state.modules||[]).find(m=>m.key==={json.dumps(key)})")
        res = await try_controls(pg, ctrls, errors, key, mod)
        # its switches and choice buttons
        btns = await pg.evaluate(f"""(()=>{{ const ed=[...document.querySelectorAll('{page} .obs-editor')].find(e=>__sw.vis(e)); if(!ed) return [];
            const bs=[...ed.querySelectorAll('button')].filter(b=>__sw.vis(b) && !/RESET|LOAD/i.test(b.textContent)); window.__swBtns=bs;
            return bs.map((b,i)=>{{const r=b.getBoundingClientRect(); return {{i, t:b.textContent.trim(), x:r.left+r.width/2, y:r.top+r.height/2}};}}); }})()""")
        for bt in btns:
            await pg.evaluate("window.__mock.log.length=0"); errors.clear()
            await pg.mouse.click(bt['x'], bt['y']); await pg.wait_for_timeout(120)
            got = await pg.evaluate("__sw.sent('')")
            if not got or errors: print(f"  BAD {key:<16} button {bt['t']!r}: " + ('nothing sent ' if not got else '') + ' / '.join(errors[:2]), flush=True); res.append({'id': key, 'label': 'button ' + bt['t'], 'problems': ['button: nothing sent' if not got else 'errors']})
        n_knobs = sum(1 for q in (mod or {}).get('params', []) if not (q.get('choices') or q.get('toggle') or q.get('momentary')))
        prob = [] if len(ctrls) >= min(n_knobs, 8) else [f'{len(ctrls)} knobs on screen, module has {n_knobs}']
        if prob: print(f"  BAD {key:<16} {prob[0]}", flush=True)
        out[key] = {'controls': res, 'problems': prob}
        if key == mods[0]: await pg.screenshot(path=str(OUT / f'sweep-{tab}-module.png'))
    report['pages'][tab] = {'controls': [dict(c, label=k + ' ' + c['label']) for k, v in out.items() for c in v['controls']] + [{'id': k, 'label': k, 'problems': v['problems']} for k, v in out.items() if v['problems']],
                            'displaysStill': [], 'displaysMoving': []}
    await pg.evaluate("window.__mock.emit('obs:rig', window.__mock.state.rig)")


async def sweep(standalone, only=None):
    state = run.build_page(standalone)
    report = {'mode': 'standalone' if standalone else 'daw', 'pages': {}, 'errors': []}
    async with async_playwright() as p:
        exe = '/usr/bin/google-chrome' if os.path.exists('/usr/bin/google-chrome') else None
        b = await p.chromium.launch(executable_path=exe, args=['--no-sandbox', '--allow-file-access-from-files'])
        pg = await b.new_page(viewport={'width': 1376, 'height': 836})
        errors = []
        pg.on('pageerror', lambda e: errors.append(str(e) + ' @ ' + ' | '.join(l.strip() for l in (getattr(e, 'stack', '') or '').split('\n')[1:3])))
        pg.on('console', lambda m: errors.append(m.text) if m.type == 'error' else None)
        await pg.add_init_script(path=str(HERE / 'mock-native.js'))
        await pg.add_init_script(script='window.__mock.load(' + json.dumps(state) + ');')
        await pg.goto((OUT / 'page.html').as_uri())
        await pg.wait_for_timeout(800)
        await pg.evaluate(HELPERS)
        tabs = await pg.evaluate("tabBtns.map(b=>b.textContent.trim())")
        # sub-pages: SYNTH (SIREN | AETHER), SFX (RACK | GLITCH)
        views = []
        for t in tabs:
            if t == 'SYNTH': views += [(t, 'siren', "obsSynth.showPage('siren')"), (t, 'aether', "obsSynth.showPage('aether')")]
            elif t in ('SFX', 'SPECIAL FX'): views += [(t, p_, "document.querySelector('.fl-pagesw button[data-p=" + p_ + "]').click()") for p_ in ('rack', 'glitch')]
            else: views.append((t, '', ''))
        views.insert(0, ('HEADER', '', ''))
        for tab, subname, sub in views:
            name = tab + (' / ' + subname if subname else '')
            if only and only.lower() not in name.lower(): continue
            print('\n==', name, flush=True)
            if tab != 'HEADER': await pg.evaluate(f"setTab({json.dumps(tab)})")
            if sub: await pg.evaluate(sub)
            await pg.wait_for_timeout(350)
            if tab in ('PEDALS', 'FX') or subname == 'rack':
                await sweep_modules(pg, tab, report, errors)
                continue
            await pg.mouse.move(2, 2)
            if tab == 'HEADER':
                ctrls = await pg.evaluate("""(()=>{ const out=[]; const els=[...document.querySelectorAll('[data-param]')].filter(e=>!e.closest('.page')&&!e.closest('.ov')&&__sw.vis(e)); window.__swEls=els;
                  return els.map((el,i)=>{const r=el.getBoundingClientRect(); return {i,id:el.dataset.param,tag:el.tagName,cls:String(el.className||''),label:__sw.label(el),x:r.left+r.width/2,y:r.top+r.height/2,w:r.width,h:r.height};}); })()""")
            else:
                ctrls = await pg.evaluate("__sw.controls()")
            res = []
            await pg.screenshot(path=str(OUT / ('sweep-' + name.replace(' / ', '-').replace(' ', '_') + '.png')))
            for c in ctrls:
                errors.clear()
                el = f"window.__swEls[{c['i']}]"
                still = await pg.evaluate(f"__sw.vis({el})")
                if not still: continue
                sig0 = await pg.evaluate(f"__sw.sig({el})"); txt0 = await pg.evaluate(f"__sw.text({el})")
                await pg.evaluate("window.__mock.log.length=0")
                kind = 'choose' if c.get('choose') and 'sy-stepper' not in c['cls'] else 'click' if c['tag'] == 'BUTTON' or 'tog' in c['cls'] or 'sy-chip' in c['cls'] or 'sy-stepper' in c['cls'] else 'drag'
                r = dict(c); r['kind'] = kind; r['problems'] = []
                if kind == 'drag':
                    for dy in (-45, 90):          # up; if it was already at the top, down
                        await pg.mouse.move(c['x'], c['y']); await pg.mouse.down()
                        await pg.mouse.move(c['x'], c['y'] + dy, steps=6); await pg.mouse.up(); await pg.wait_for_timeout(120)
                        sig1 = await pg.evaluate(f"__sw.sig({el})"); txt1 = await pg.evaluate(f"__sw.text({el})")
                        sent = await pg.evaluate(f"__sw.sent({json.dumps(c['id'])})")
                        if sig1 != sig0: break
                    if sig1 == sig0: r['problems'].append('drag: nothing moves')
                    elif txt1 == txt0 and not any(x in c['cls'] for x in ('sy-sel', 'dial')): r['problems'].append('drag: value text unchanged')
                    if not sent: r['problems'].append('drag: nothing sent to the engine')
                    # scroll
                    await pg.evaluate("window.__mock.log.length=0")
                    for wd in (300, -300):
                        await pg.mouse.move(c['x'], c['y']); await pg.mouse.wheel(0, wd); await pg.wait_for_timeout(120)
                        sig2 = await pg.evaluate(f"__sw.sig({el})"); sent2 = await pg.evaluate(f"__sw.sent({json.dumps(c['id'])})")
                        if sig2 != sig1: break
                    if sig2 == sig1: r['problems'].append('wheel: nothing moves')
                    if not sent2: r['problems'].append('wheel: nothing sent')
                    # double-click: back to the parameter's default
                    await pg.evaluate("window.__mock.log.length=0")
                    await pg.mouse.dblclick(c['x'], c['y']); await pg.wait_for_timeout(150)
                    sent3 = await pg.evaluate(f"__sw.sent({json.dumps(c['id'])})")
                    meta = await pg.evaluate(f"__sw.meta({json.dumps(c['id'])})") if c['id'] else None
                    if meta and sent3:
                        want = meta.get('defNorm', None)
                        if want is not None and abs(sent3[-1] - want) > 0.02: r['problems'].append(f'double-click: went to {sent3[-1]:.3f}, default is {want:.3f}')
                    elif meta and not sent3 and meta.get('type') not in ('choice', 'int', 'bool'):
                        r['problems'].append('double-click: no reset')
                    r['sent'] = (sent or [None])[-1]
                elif kind == 'choose':
                    pt = await pg.evaluate(f"__sw.chooseBtn({el})")
                    await pg.mouse.click(pt[0], pt[1]); await pg.wait_for_timeout(150)
                    sig1 = await pg.evaluate(f"__sw.sig({el})"); sent = await pg.evaluate(f"__sw.sent({json.dumps(c['id'])})")
                    if sig1 == sig0: r['problems'].append('choose: nothing changes')
                    if not sent: r['problems'].append('choose: nothing sent')
                    if pt[2] >= 0: await pg.evaluate(f"[...{el}.querySelectorAll(':scope > button')][{pt[2]}].click()"); await pg.wait_for_timeout(100)
                else:
                    await pg.mouse.click(c['x'], c['y']); await pg.wait_for_timeout(150)
                    sig1 = await pg.evaluate(f"__sw.sig({el})")
                    sent = await pg.evaluate(f"__sw.sent({json.dumps(c['id'])})")
                    if sig1 == sig0: r['problems'].append('click: nothing changes')
                    if not sent: r['problems'].append('click: nothing sent')
                    await pg.mouse.click(c['x'], c['y']); await pg.wait_for_timeout(120)     # put it back
                if errors: r['problems'].append('errors: ' + ' / '.join(errors[:3]))
                res.append(r)
                flag = '  ok  ' if not r['problems'] else '  BAD '
                print(f"{flag}{c['id']:<22} {r['label'][:24]:<24} {kind:<5} {'; '.join(r['problems'])}", flush=True)
            # displays: do they move?
            await pg.evaluate("window.__mock.meterExtra = null")
            l0 = await pg.evaluate("__sw.live()"); await pg.wait_for_timeout(900); l1 = await pg.evaluate("__sw.live()")
            still = [k for k in l0 if k in l1 and l0[k] == l1[k]]
            moving = [k for k in l0 if k in l1 and l0[k] != l1[k]]
            print(f"  displays: {len(moving)} moving, {len(still)} still: " + ', '.join(k[:40] for k in still), flush=True)
            report['pages'][name] = {'controls': res, 'displaysStill': still, 'displaysMoving': moving}
        await b.close()
    (OUT / ('sweep-' + report['mode'] + '.json')).write_text(json.dumps(report, indent=1))
    bad = [(n, c['id'], c['label'], c['problems']) for n, pgr in report['pages'].items() for c in pgr['controls'] if c['problems']]
    print(f"\n{sum(len(v['controls']) for v in report['pages'].values())} controls tried, {len(bad)} with problems")
    return report


if __name__ == '__main__':
    only = sys.argv[sys.argv.index('--page') + 1] if '--page' in sys.argv else None
    asyncio.run(sweep('--standalone' in sys.argv, only))
