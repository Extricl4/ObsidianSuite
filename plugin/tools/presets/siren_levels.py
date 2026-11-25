#!/usr/bin/env python3
"""Level-matches the SIREN presets (ui/src/synth_presets.js).

    python3 plugin/tools/presets/siren_levels.py [binary] [--write]

Renders every SIREN preset with the real engine (--render-sounds), 100 % wet, twice: following a guitar riff and
played from MIDI (single notes, a legato pair, a chord, a high and a low note). Measures each one's loudness
(ITU BS.1770, the two renders averaged) and works out the trim that brings it to the presets' median, keeping its
peaks under -6 dBFS. Trims under 2 dB are left at 0. HORROR presets are left alone: their AETHER part (not trimmed)
is balanced against the synth by ear.

--write puts the trims into the TRIM table at the top of synth_presets.js (they become each preset's "syn.trim").
--check measures them with their trims (then the suggested trims are what is still off).
Needs: node, numpy, soundfile, pyloudnorm (pip install pyloudnorm).
"""
import json, re, subprocess, sys, tempfile
from pathlib import Path
import numpy as np
import soundfile as sf

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
JS = ROOT / 'ui' / 'src' / 'synth_presets.js'
DEFAULT_BIN = Path('/home/claude/build/ObsidianSuite_artefacts/Release/Standalone/Obsidian Amp Suite')
sys.path.insert(0, str(HERE))
from make_presets import pack_steps  # noqa: E402

NOTES = [[52, .2, .8, 110], [55, 1.3, .45, 100], [57, 1.7, .45, 100], [60, 2.4, .5, 90],
         [52, 3.2, 1.0, 110], [55, 3.2, 1.0, 100], [59, 3.2, 1.0, 100], [76, 4.6, .6, 120], [40, 5.5, .8, 120]]


def presets():
    js = JS.read_text(encoding='utf-8').replace('<script>', '').replace('</script>', '')
    out = subprocess.run(['node', '-e', 'global.window={};' + js + ';process.stdout.write(JSON.stringify(window.OBS_SYNTH_PRESETS))'],
                         capture_output=True, text=True, check=True)
    return json.loads(out.stdout)


def loudness(x, sr):
    try:
        import pyloudnorm as pyln
        return pyln.Meter(sr).integrated_loudness(x)
    except ImportError:                       # gated RMS, near enough
        m = x.mean(axis=1); fr = int(.4 * sr)
        bl = np.array([np.mean(m[i:i + fr] ** 2) for i in range(0, len(m) - fr, fr // 4)])
        bl = bl[bl > 10 ** (-70 / 10)]; g = bl[bl > np.mean(bl) * 10 ** (-10 / 10)]
        return 10 * np.log10(np.mean(g) + 1e-12)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    binary = Path(args[0]) if args else DEFAULT_BIN
    pr = presets()
    spec = []
    for i, p in enumerate(pr):
        params = {'synth.on': 1, 'syn.mix': 100, 'syn.trim': 0}
        params.update({'syn.' + k: v for k, v in p['v'].items() if k != 'trim' or '--check' in sys.argv})
        if p.get('a'):
            params['atm.on'] = 1; params.update({'atm.' + k: v for k, v in p['a'].items()})
        steps = pack_steps(p.get('steps'))
        spec.append({'name': f'm{i:03d}', 'seconds': 7, 'params': params, 'notes': NOTES, 'steps': steps})
        spec.append({'name': f'g{i:03d}', 'seconds': 8, 'params': params, 'guitar': True, 'steps': steps})
    with tempfile.TemporaryDirectory() as tmp:
        sp = Path(tmp) / 'spec.json'; sp.write_text(json.dumps(spec))
        r = subprocess.run([str(binary), '--render-sounds', tmp, str(sp)], capture_output=True, text=True, timeout=3600)
        bad = [l for l in r.stdout.splitlines() if 'no setting' in l or 'has no choice' in l]
        if bad: print('\n'.join(sorted(set(bad))))
        rows = []
        for i, p in enumerate(pr):
            xm, sr = sf.read(Path(tmp) / f'm{i:03d}.wav'); xg, _ = sf.read(Path(tmp) / f'g{i:03d}.wav')
            xm = xm[int(.2 * sr):int(6.6 * sr)]
            rows.append(dict(name=p['name'], cat=p['cat'], lm=loudness(xm, sr), lg=loudness(xg, sr),
                             peak=20 * np.log10(max(np.abs(xm).max(), np.abs(xg).max()) + 1e-9)))
    both = np.array([(r['lm'] + r['lg']) / 2 for r in rows])
    target = float(np.median([b for b, r in zip(both, rows) if r['cat'] != 'HORROR']))
    trims = {}
    for r, b in zip(rows, both):
        want = target - b; room = -6 - r['peak']
        t = min(want, room) if want > 0 else want
        t = round(t * 2) / 2
        if abs(t) < 2 or r['cat'] == 'HORROR': t = 0.0
        if t: trims[r['name']] = t
        print(f"{r['name'][:26]:<27}{r['cat'][:11]:<12}MIDI {r['lm']:6.1f}  guitar {r['lg']:6.1f}  peak {r['peak']:5.1f}   trim {t:+5.1f}"
              + ('  (peaks limit it)' if 0 < room < want else ''))
    print(f'target {target:.1f} LUFS (the median), {len(trims)} presets trimmed')
    nh = [b for b, r in zip(both, rows) if r['cat'] != 'HORROR']
    print(f'spread (not HORROR): {min(nh):.1f} .. {max(nh):.1f} LUFS, middle 80 %: {np.percentile(nh, 10):.1f} .. {np.percentile(nh, 90):.1f}')
    if '--write' in sys.argv:
        s = JS.read_text(encoding='utf-8')
        body = ',\n'.join(f"  {json.dumps(n)}: {t:g}" for n, t in trims.items())
        new = 'const TRIM = {\n' + body + '\n};'
        s, n = re.subn(r'const TRIM = \{[^}]*\};', lambda m: new, s, count=1)
        if n != 1: raise SystemExit('no TRIM table in synth_presets.js')
        JS.write_text(s, encoding='utf-8'); print('wrote', JS)


if __name__ == '__main__':
    main()
