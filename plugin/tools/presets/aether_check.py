#!/usr/bin/env python3
"""Loudness check for AETHER's scenes (ui/src/synth_presets.js, OBS_AETHER_SCENES).

    python -I plugin/tools/presets/aether_check.py <render folder> [--fix] [--only NAME]

Every scene is rendered by the plugin on its own (16 s, nothing else playing, the SYNTH tab on:
"Obsidian Amp Suite" --render-sounds), measured as integrated loudness (ITU-R BS.1770:
K-weighting, 400 ms blocks, gated) and compared with its folder's target. --fix writes
each scene's LEVEL so it lands on the target. The HORROR scenes are the atmospheres
of SIREN's HORROR presets (the `A` blocks in the same file), so --fix sets those too.
Needs node (to read the preset file), numpy and scipy. OBS_BIN: the standalone to use.
"""
import json, os, re, subprocess, sys, tempfile
from pathlib import Path
import numpy as np
from scipy.io import wavfile
from scipy.signal import lfilter

HERE = Path(__file__).resolve().parent
PRESETS = HERE.parents[2] / 'ui' / 'src' / 'synth_presets.js'
EXE = os.environ.get('OBS_BIN', '/home/claude/build/ObsidianSuite_artefacts/Release/Standalone/Obsidian Amp Suite')
TARGET = {'NATURE': -31.0, 'DREAM': -29.5, 'DARK': -30.0, 'HORROR': -28.0}      # LUFS, under a guitar at about -21
LEVEL_RANGE = (-24.0, 4.0)

READ = """const fs = require('fs'); const window = {};
eval(fs.readFileSync(process.argv[1], 'utf8').replace(/<\\/?script>/g, ''));
process.stdout.write(JSON.stringify(window.OBS_AETHER_SCENES));"""


def scenes():
    out = subprocess.run(['node', '-e', READ, str(PRESETS)], check=True, capture_output=True, text=True).stdout
    return json.loads(out)


def safe(name): return re.sub(r'[^A-Za-z0-9]+', '_', name)


def lufs(path):
    sr, x = wavfile.read(path)
    x = x.astype(np.float64) / (2 ** 31 if x.dtype == np.int32 else 1)
    if x.ndim == 1: x = x[:, None]
    if sr != 48000: raise SystemExit('expects 48 kHz renders')
    y = lfilter([1.53512485958697, -2.69169618940638, 1.19839281085285], [1.0, -1.69065929318241, 0.73248077421585], x, axis=0)
    y = lfilter([1.0, -2.0, 1.0], [1.0, -1.99004745483398, 0.99007225036621], y, axis=0)
    n = int(.4 * sr); hop = n // 4
    z = np.array([np.sum(np.mean(y[i:i + n] ** 2, axis=0)) for i in range(0, len(y) - n, hop)])
    l = -0.691 + 10 * np.log10(z + 1e-20)
    g = z[l > -70]
    if not len(g): return -99.0
    rel = -0.691 + 10 * np.log10(np.mean(g)) - 10
    return float(-0.691 + 10 * np.log10(np.mean(z[(l > -70) & (l > rel)])))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    if not args: raise SystemExit(__doc__)
    out = Path(args[0]); out.mkdir(parents=True, exist_ok=True)
    fix = '--fix' in sys.argv
    only = sys.argv[sys.argv.index('--only') + 1] if '--only' in sys.argv else None
    sc = [s for s in scenes() if not only or only.lower() in s['name'].lower()]
    spec = [{'name': s['cat'] + '_' + safe(s['name']), 'seconds': 16,
             'params': dict({'synth.on': 1, 'syn.mix': 0, 'atm.on': 1}, **{'atm.' + k: v for k, v in s['a'].items()})} for s in sc]
    with tempfile.NamedTemporaryFile('w', suffix='.json', delete=False) as f: json.dump(spec, f); sp = f.name
    subprocess.run([EXE, '--render-sounds', str(out.resolve()), sp], check=True, capture_output=True, timeout=1800)
    text = PRESETS.read_text(encoding='utf-8'); worst = 0.0
    for s, e in zip(sc, spec):
        L = lufs(out / (e['name'] + '.wav')); cur = float(s['a'].get('level', -6)); t = TARGET[s['cat']]
        new = min(LEVEL_RANGE[1], max(LEVEL_RANGE[0], round((cur + t - L) * 2) / 2))
        worst = max(worst, abs(L - t))
        print(f"{s['cat']:7s} {s['name']:26s} {L:6.1f} LUFS (target {t:5.1f})  level {cur:5.1f}" + (f' -> {new:5.1f}' if fix and new != cur else ''))
        if not fix or new == cur: continue
        # the scene's own line, or (HORROR) the preset's atmosphere block it points to
        m = re.search(r"SC\('%s', '%s', '(?:[^'\\]|\\.)*', A\.(\w+)\)" % (s['cat'], re.escape(s['name'])), text)
        pat = re.compile(r"(\n  %s:\s*\{ )(level: -?[\d.]+, )?" % m.group(1)) if m else \
              re.compile(r"(SC\('%s', '%s', '(?:[^'\\]|\\.)*', \{ )(level: -?[\d.]+, )?" % (s['cat'], re.escape(s['name'])))
        text, k = pat.subn(lambda mm: mm.group(1) + 'level: %g, ' % new, text, count=1)
        if not k: print('   (could not find it in the file)')
    if fix: PRESETS.write_text(text, encoding='utf-8'); print('wrote', PRESETS)
    print(f'largest miss: {worst:.1f} LU')


if __name__ == '__main__':
    main()
