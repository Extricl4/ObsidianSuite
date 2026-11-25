#!/usr/bin/env python3
"""Level-matches the GLITCH presets (KERAUNOS + KHAOS, ui/src/fxlab.js) to the dry guitar, like a pedal at unity gain.

    python3 plugin/tools/presets/glitch_levels.py [--write]

Reads every preset's full settings from the real page (the UI test's page and fake engine, headless Chrome), renders
each on the test guitar riff with the plugin (--render-sounds, amp off, SFX on) next to the dry riff, and measures
loudness (ITU BS.1770). A preset more than 3 dB off the dry guitar gets a LEVEL that brings it back: KERAUNOS's
(ker.level) when KERAUNOS is in it, otherwise KHAOS's (glt.level). --write puts those into the presets in fxlab.js
(as `level` in the preset's k: / g: block). Needs playwright, numpy, soundfile, pyloudnorm.
"""
import asyncio, json, math, os, re, subprocess, sys, tempfile
from pathlib import Path
import numpy as np
import soundfile as sf

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
FX = ROOT / 'ui' / 'src' / 'fxlab.js'
BIN = os.environ.get('OBS_BIN', '/home/claude/build/ObsidianSuite_artefacts/Release/Standalone/Obsidian Amp Suite')
sys.path.insert(0, str(ROOT / 'plugin' / 'tools' / 'ui-test'))
import run  # noqa: E402


def real(meta, id_, n):
    m = meta[id_]
    if m['type'] == 'bool': return 1 if n >= .5 else 0
    if m['type'] in ('int', 'choice'): return round(m['min'] + (m['max'] - m['min']) * n)
    if m.get('log'): return m['min'] * (m['max'] / m['min']) ** n
    if m['skew'] != 1 and n > 0: n = math.exp(math.log(n) / m['skew'])
    return m['min'] + (m['max'] - m['min']) * n


async def read_presets():
    from playwright.async_api import async_playwright
    state = run.build_page(True)
    async with async_playwright() as p:
        b = await p.chromium.launch(executable_path='/usr/bin/google-chrome' if os.path.exists('/usr/bin/google-chrome') else None,
                                    args=['--no-sandbox', '--allow-file-access-from-files'])
        pg = await b.new_page(viewport={'width': 1376, 'height': 836})
        await pg.add_init_script(path=str(run.HERE / 'mock-native.js'))
        await pg.add_init_script(script='window.__mock.load(' + json.dumps(state) + ');')
        await pg.goto((run.OUT / 'page.html').as_uri()); await pg.wait_for_timeout(800)
        n = await pg.evaluate('obsFxLab.presets.length')
        out = [await pg.evaluate(f"(()=>{{ obsFxLab.loadPreset({i}); const pr=obsFxLab.presets[{i}]; return {{name: pr.name, cat: pr.cat, k: !!pr.k, g: !!(pr.g||pr.q),"
                                 f" level: (pr.k&&pr.k.level)||(pr.g&&pr.g.level)||0, vals: obsFxLab.read()}}; }})()") for i in range(n)]
        await b.close()
    return state, out


def main():
    state, pre = asyncio.run(read_presets())
    meta = {p['id']: p for p in state['params']}
    spec = [{'name': 'dry', 'seconds': 8, 'params': {}, 'guitar': True}]
    for i, r in enumerate(pre):
        params = {'sfx.on': 1}; params.update({k: real(meta, k, v) for k, v in r['vals'].items() if k in meta})
        spec.append({'name': f'p{i:02d}', 'seconds': 8, 'params': params, 'guitar': True})
    import pyloudnorm as pyln
    M = pyln.Meter(48000)
    with tempfile.TemporaryDirectory() as tmp:
        sp = Path(tmp) / 'spec.json'; sp.write_text(json.dumps(spec))
        subprocess.run([BIN, '--render-sounds', tmp, str(sp)], capture_output=True, text=True, timeout=1800)
        dry = M.integrated_loudness(sf.read(Path(tmp) / 'dry.wav')[0])
        changes = {}
        for i, r in enumerate(pre):
            x = sf.read(Path(tmp) / f'p{i:02d}.wav')[0]
            l = M.integrated_loudness(x); pk = 20 * np.log10(np.abs(x).max() + 1e-9)
            off = l - dry
            new = r['level']
            if abs(off) > 3: new = max(-12, min(12, round((r['level'] - off) * 2) / 2))
            if new != r['level']: changes[r['name']] = (new, 'k' if r['k'] else 'g')
            print(f"{r['name'][:24]:<25}{r['cat'][:10]:<11}{off:+6.1f} dB vs dry  peak {pk:5.1f}   level {r['level']:+5.1f}" + (f' -> {new:+.1f}' if new != r['level'] else ''))
    print(f'dry guitar {dry:.1f} LUFS; {len(changes)} presets to change')
    if '--write' in sys.argv and changes:
        s = FX.read_text(encoding='utf-8')
        for name, (lv, blk) in changes.items():
            m = re.search(r"\{ name: '" + re.escape(name) + r"'.*?\n", s)
            line = m.group(0)
            b = re.search(blk + r': \{', line)
            if re.search(blk + r': \{[^}]*level: [-\d.]+', line): new = re.sub(r'(' + blk + r': \{[^}]*?)level: [-\d.]+', lambda q: q.group(1) + f'level: {lv:g}', line)
            else: new = line[:b.end()] + f' level: {lv:g},' + line[b.end():]
            s = s.replace(line, new, 1)
        FX.write_text(s, encoding='utf-8'); print('wrote', FX)


if __name__ == '__main__':
    main()
