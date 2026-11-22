#!/usr/bin/env python3
"""Tone check: how each preset balances lows, low mids, bite and fizz on a real guitar DI.

    OBS_DI=di48.wav OBS_RENDER_DIR=<renders> "<binary>" --write-factory-presets <tmp dir> tools/presets/spec.json -20
    python3 -I tools/presets/tonecheck.py <renders> tools/presets/spec.json [--refs <renders of reference rigs>] [--csv out.csv]

The plugin writes every preset playing the DI to <renders>/<preset file stem>.wav (mono, 48 kHz); OBS_ONLY="a,b"
limits a run to presets whose name or folder contains a or b. Each render is measured where the guitar is playing
(quiet gaps left out), as band levels relative to the whole 100 Hz - 5 kHz range:

    lows  60-150 Hz    thump and chug weight          mud  200-450 Hz    boxy, woolly low mids
    mids  500-1.5 kHz  honk and body (low = scooped)  bite 2-5 kHz       pick attack, cut, articulation
    fizz  6-11 kHz     sizzle, hiss, harshness

A preset is flagged DULL when its bite sits well under the reference rigs' (the full-rig captures made by the tone
creators, the same DI through them with nothing else on), MUDDY when its low mids pile up, FIZZY / BOOMY / THIN
the same way. The numbers are for comparing presets with each other and the references, not absolute truth.

--fix: for every preset outside the ranges below, works out an EQ correction (the 9 bands, dB) that moves it back inside
and adds it to tonefix.json, which make_presets.py lays on top of the preset's own EQ. Run it again on fresh renders and
it refines the corrections it already made. Presets listed in KEEP are left as they are (their sound is the point).
"""
import json, sys
from pathlib import Path
import numpy as np
import soundfile as sf
from scipy.signal import welch

# where a preset may sit before it gets a correction (dB, as measured above); None = no limit
RANGE = {
    'Rhythm':   dict(lows=(-10, 3), mud=(None, 2.5), mids=(-7, None), bite=(-1.5, None), fizz=(None, -14)),
    'Lead':     dict(lows=(None, 2), mud=(None, 2.5), mids=(-6, None), bite=(-2.5, None), fizz=(None, -16)),
    'Crunch':   dict(lows=(None, 3), mud=(None, 2.5), mids=(-6, None), bite=(-3, None), fizz=(None, -16)),
    'Clean':    dict(lows=(None, 4), mud=(None, 3), bite=(-11, None)),
    'Ambient':  dict(lows=(None, 4), mud=(None, 3), bite=(-12, None)),
    'Acoustic': dict(mids=(-9, None), bite=(-9, None)),
}
KEEP = {'Blackened',             # ...And Justice for All: no bass on purpose
        # Nemean's Pantera presets are tuned by hand around those very bright, scooped captures
        'Cowboys from Hell', 'Domination', 'Cemetery Gates (Solo)', 'Walk', 'Mouth for War', "I'm Broken", '5 Minutes Alone',
        'Drag the Waters', 'The Great Southern Trendkill', 'Floods (Solo)'}
# how each measured band answers the EQ: which bands move it and by how much of the change asked for
LEVER = {'lows': {0: 0.5, 1: 0.9}, 'mud': {2: 0.9, 3: 0.4}, 'mids': {3: 0.6, 4: 0.8}, 'bite': {5: 0.7, 6: 0.9}, 'fizz': {7: 1.0, 8: 0.5}}
MAXFIX = 6.0                     # no band moves more than this in total

BANDS = {'lows': (60, 150), 'mud': (200, 450), 'mids': (500, 1500), 'bite': (2000, 5000), 'fizz': (6000, 11000)}


def measure(path):
    x, sr = sf.read(str(path), dtype='float32')
    if x.ndim > 1: x = x.mean(1)
    n = int(0.1 * sr)
    fr = x[: len(x) // n * n].reshape(-1, n)
    rms = np.sqrt((fr ** 2).mean(1) + 1e-12)
    act = fr[rms > rms.max() * 10 ** (-20 / 20)].ravel()       # only where the guitar plays
    if act.size < sr: return None
    f, p = welch(act, sr, nperseg=8192)
    g = 2 ** np.arange(np.log2(40), np.log2(16000), 1 / 24)    # a log axis: every octave counts the same
    pg = np.interp(g, f, p * f)                         # power per octave
    def lvl(a, b):
        m = (g >= a) & (g < b)
        return 10 * np.log10(pg[m].mean() + 1e-20)
    whole = lvl(100, 5000)
    out = {k: round(lvl(*v) - whole, 1) for k, v in BANDS.items()}
    out['level'] = round(20 * np.log10(np.sqrt((act ** 2).mean()) + 1e-9), 1)
    # brightness in one number: spectral centroid of the 80 Hz - 12 kHz power on a log axis
    m = (f >= 80) & (f <= 12000)
    out['centroid'] = int(2 ** ((np.log2(f[m]) * p[m]).sum() / p[m].sum()))
    return out


def main():
    a = [x for x in sys.argv[1:] if not x.startswith('--')]
    rdir, spec = Path(a[0]), json.loads(Path(a[1]).read_text(encoding='utf-8'))
    refs = None
    if '--refs' in sys.argv:
        refs = [m for f in sorted(Path(sys.argv[sys.argv.index('--refs') + 1]).glob('*.wav')) if (m := measure(f))]
    by = {Path(e['file']).stem: e for e in spec}
    rows = []
    for f in sorted(rdir.glob('*.wav')):
        e = by.get(f.stem)
        m = measure(f)
        if e is None or m is None: continue
        rows.append(dict(name=e['name'], type=e['type'], folder=e['category'], amp=e['amp'].get('key', ''), **m))
    # reference: the full-rig captures, else the high-gain presets' own median
    hg = [r for r in rows if r['type'] in ('Rhythm', 'Lead', 'Crunch')]
    base = refs or hg
    K = ('lows', 'mud', 'mids', 'bite', 'fizz')
    ref = {k: float(np.median([r[k] for r in base])) for k in K}
    spread = {k: float(np.std([r[k] for r in base])) for k in K}
    print('reference (' + ('full-rig captures' if refs else 'median of the drive presets') + '): '
          + ', '.join(f'{k} {v:+.1f} dB (sd {spread[k]:.1f})' for k, v in ref.items()))
    for r in rows:
        fl = []
        drive = r['type'] in ('Rhythm', 'Lead', 'Crunch')
        if drive:
            if r['bite'] < ref['bite'] - 4: fl.append('DULL')
            if r['mud'] > ref['mud'] + 4: fl.append('MUDDY')
            if r['fizz'] > ref['fizz'] + 5: fl.append('FIZZY')
            if r['lows'] > ref['lows'] + 6: fl.append('BOOMY')
            if r['lows'] < ref['lows'] - 8 and r['type'] == 'Rhythm': fl.append('THIN')
        else:
            if r['bite'] < -14: fl.append('DULL')
            if r['mud'] > 6: fl.append('MUDDY')
        r['flags'] = ' '.join(fl)
    rows.sort(key=lambda r: (r['folder'], r['name']))
    for r in rows:
        print(f"{r['name'][:36]:36s} {r['type']:8s} {r['amp'][:10]:10s} lows {r['lows']:+6.1f}  mud {r['mud']:+6.1f}  mids {r['mids']:+6.1f}  bite {r['bite']:+6.1f}  "
              f"fizz {r['fizz']:+6.1f}  centroid {r['centroid']:5d}  {r['flags']}")
    if '--fix' in sys.argv:
        fixfile = Path(__file__).resolve().parent / 'tonefix.json'
        fixes = json.loads(fixfile.read_text()) if fixfile.exists() else {}
        n = 0
        for r in rows:
            rg = RANGE.get(r['type'])
            if not rg or r['name'] in KEEP: continue
            delta = [0.0] * 9
            for k, (lo, hi) in rg.items():
                want = (lo - r[k]) if lo is not None and r[k] < lo else (hi - r[k]) if hi is not None and r[k] > hi else 0.0
                for b, w in LEVER[k].items(): delta[b] += w * want * 0.85
            if not any(abs(d) >= 0.3 for d in delta): continue
            old = fixes.get(r['name'], [0.0] * 9)
            fixes[r['name']] = [round(max(-MAXFIX, min(MAXFIX, o + d)) * 2) / 2 for o, d in zip(old, delta)]
            n += 1
        fixfile.write_text(json.dumps(dict(sorted(fixes.items())), indent=0))
        print(f'tonefix.json: {n} presets corrected ({len(fixes)} in all)')
    flagged = [r for r in rows if r['flags']]
    print(f'{len(flagged)} of {len(rows)} flagged')
    if '--csv' in sys.argv:
        import csv
        with open(sys.argv[sys.argv.index('--csv') + 1], 'w', newline='') as fh:
            w = csv.DictWriter(fh, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)


if __name__ == '__main__':
    main()
