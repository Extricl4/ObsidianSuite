#!/usr/bin/env python3
"""Re-voice presets after a cab change: EQ corrections (tonefix.json) that bring each preset's balance back to how it
was voiced before, so a new cab brings its own detail and resonance without changing what the preset was made to be.

    python3 -I revoice.py <before renders> <after renders> <spec.json> <names.txt> [--skip-type Acoustic] [--skip names2.txt]

Both render folders are the same DI through the presets (see tonecheck.py). Measured like tonecheck.py (lows, mud,
mids, bite, fizz); each band moves 85% of the way back, through the EQ bands that answer it (tonecheck.LEVER), at most
tonecheck.MAXFIX dB per EQ band in all. Run it again on fresh renders and it refines the corrections it made.
"""
import json, sys
from pathlib import Path
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import tonecheck as T

a = [x for x in sys.argv[1:] if not x.startswith('--')]
bdir, adir, spec, names = Path(a[0]), Path(a[1]), json.loads(Path(a[2]).read_text()), set(Path(a[3]).read_text().split('\n'))
skip_types = set(sys.argv[sys.argv.index('--skip-type') + 1].split(',')) if '--skip-type' in sys.argv else set()
skip = set(Path(sys.argv[sys.argv.index('--skip') + 1]).read_text().split('\n')) if '--skip' in sys.argv else set()
fixfile = HERE / 'tonefix.json'
fixes = json.loads(fixfile.read_text()) if fixfile.exists() else {}
K = ('lows', 'mud', 'mids', 'bite', 'fizz')
n = 0
for e in spec:
    if e['name'] not in names or e['name'] in skip or e['type'] in skip_types: continue
    stem = Path(e['file']).stem
    fb, fa = bdir / (stem + '.wav'), adir / (stem + '.wav')
    if not fb.exists() or not fa.exists(): continue
    mb, ma = T.measure(fb), T.measure(fa)
    if not mb or not ma: continue
    delta = [0.0] * 9
    for k in K:
        want = mb[k] - ma[k]
        if abs(want) < 0.5: continue
        for band, w in T.LEVER[k].items(): delta[band] += w * want * 0.85
    old = fixes.get(e['name'], [0.0] * 9)
    new = [round(max(-T.MAXFIX, min(T.MAXFIX, o + d)) * 2) / 2 for o, d in zip(old, delta)]
    if new != old: fixes[e['name']] = new; n += 1
fixfile.write_text(json.dumps(dict(sorted(fixes.items())), indent=0))
print(f'tonefix.json: {n} presets re-voiced ({len(fixes)} corrections in all)')
