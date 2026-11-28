#!/usr/bin/env python3
"""ORPHEUS: builds the UI (WebUI/index.html, its fonts, grain.png and fx-worklet.js) from ui/src. The plugin compiles WebUI/ in;
the same index.html also runs in a browser on its own (Web Audio sound). Run after changing anything in ui/src.

Builds the SYNTH-only preview: the real SIREN and AETHER (from the repo sources) plus ORPHEUS,
on their own device, without the rest of the Obsidian app (amp, pedals, presets bar, footer)."""
import os, subprocess, shutil
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # the orpheus/ folder
OUT = os.path.join(HERE, 'WebUI')
os.makedirs(OUT, exist_ok=True)
subprocess.run(['python3', 'build.py'], cwd=os.path.join(HERE, 'ui/src'), check=True, env=dict(os.environ, OBS_OUT=os.path.join(OUT, 'index.html')))
for d in ('fonts',):
    shutil.rmtree(os.path.join(OUT, d), ignore_errors=True); shutil.copytree(os.path.join(HERE, 'ui', d), os.path.join(OUT, d))
shutil.copy(os.path.join(HERE, 'ui/grain.png'), OUT)

p = os.path.join(OUT, 'index.html'); h = open(p).read()
h = h.replace('<title>Obsidian Amp Sim Demo</title>', '<title>Obsidian SYNTH · SIREN &amp; ORPHEUS</title>', 1)
shell = r"""<style>
/* SYNTH only: the app stays (it runs the synth page) but off screen; the device sits alone in the window */
#app { position: fixed !important; left: -40000px !important; top: 0 !important; opacity: 0 !important; pointer-events: none !important; }
#syn { position: fixed; inset: 0; z-index: 10; display: flex; align-items: center; justify-content: center; overflow: hidden; user-select: none;
  background: radial-gradient(120% 90% at 50% 0%, #222226 0%, #111113 55%, #08080a 100%); color: #e9e6df; font-family: 'Saira', 'Segoe UI', system-ui, sans-serif; }
#syn .wrap { position: relative; width: 1320px; height: 572px; flex: none; transform-origin: center center; }
/* no SIREN POWER button in this preview: SIREN is always on (its PRESETS are the standalone's own, siren_presets.js) */
/* no AETHER scenes: the scene name under the badge */
.ae-bot .ae-badge .pn { display: none !important; }
.ae-bot .ae-badge { top: 18px; }
#syn .grain2 { position: absolute; inset: 0; border-radius: 22px; background: url(grain.png) 0 0 / 256px 256px; mix-blend-mode: overlay; opacity: .2; pointer-events: none; z-index: 40; }
</style>
<script>
setTimeout(() => {
  const tab = [...document.querySelectorAll('#app button')].find(x => x.textContent.trim() === 'SYNTH' && x.querySelector('svg'));
  if (tab) tab.click();                                     // the SYNTH page runs (SIREN and AETHER check that it shows)
  const dev = document.querySelector('.sy-dev'); if (!dev) return;
  const syn = document.createElement('div'); syn.id = 'syn';
  const wrap = document.createElement('div'); wrap.className = 'wrap';
  const grain = document.createElement('div'); grain.className = 'grain2'; grain.setAttribute('aria-hidden', 'true');
  wrap.append(dev, grain); syn.append(wrap); document.body.append(syn);
  const K = window.__sirenKit;
  document.querySelectorAll('.sy-top.sy-siren .sy-pb[aria-label="POWER"]').forEach(b => { const w = b.closest('.sy-hw'); if (w) w.style.display = 'none'; });
  if (K) {
    if (!K.on('syn.power')) K.set('syn.power', 1, 'ui');            // SIREN has no POWER button here: always on (AETHER keeps its own)
    K.onChange((id, v) => { if (id === 'syn.power' && !v) setTimeout(() => K.set(id, 1, 'ui'), 0); });
  }
  const fit = () => { const s = Math.min(innerWidth / 1368, innerHeight / 620); wrap.style.transform = 'scale(' + s + ')'; };
  addEventListener('resize', fit); fit();
}, 0);
</script>
"""
i = h.rfind('</body>'); h = h[:i] + shell + h[i:]
# SHIMMER's AudioWorklet: shipped beside the page (fx-worklet.js) and inlined too, for a blob: fallback; it must come before SIREN's scripts
wsrc = open(os.path.join(HERE, 'ui/src/fx-worklet.js')).read()
shutil.copy(os.path.join(HERE, 'ui/src/fx-worklet.js'), OUT)
import json as _json
inj = '<script>window.OBS_FX_WORKLET = ' + _json.dumps(wsrc).replace('</', '<\\/') + ';</script>'
j = h.find('<script'); h = h[:j] + inj + h[j:]
open(p, 'w').write(h)
print('ok', len(h))
