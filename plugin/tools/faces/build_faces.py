#!/usr/bin/env python3
"""Builds the amp faces the plugin draws for the capture amps.

    python3 plugin/tools/faces/build_faces.py

Reads (never writes) the finished-amps page in visuals/finished/site/ (made by the visuals chat):
its layer renders (face on / off, one pointer sprite per knob, one sprite per lit LED) and the
data block with every knob, button, LED and power-switch position. Writes:

    plugin/Content/Faces/<key>.json   positions, knob names and defaults, buttons, LEDs, setting names
    plugin/Content/Faces/img/*        the images those files use

The plugin finds them in Documents\\Obsidian Amp Suite\\Faces (the pack installs them there).
Which capture plays for which knob/button setting is NOT here: that's the amp's own file,
plugin/Content/Amps/<key>.json ("map"), which the engine reads.
"""
import json, shutil, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SITE = ROOT / 'visuals' / 'finished' / 'site'
OUT = ROOT / 'plugin' / 'Content' / 'Faces'
AMPS = ROOT / 'plugin' / 'Content' / 'Amps'

def page_data():
    s = (SITE / 'index.html').read_text(encoding='utf-8')
    i = s.index('const AMPS=') + len('const AMPS=')
    data, _ = json.JSONDecoder().raw_decode(s[i:])
    return data

def main():
    amps = page_data()
    (OUT / 'img').mkdir(parents=True, exist_ok=True)
    have = {p.stem for p in AMPS.glob('*.json')}
    used, n = set(), 0
    for a in amps:
        if a['key'] not in have:            # the loader (Eidolon) and anything without captures
            continue
        knobs = []
        for k in a['knobs']:
            o = {x: k[x] for x in ('label', 'b', 'o', 'c', 'r', 'v')}
            if k.get('steps'): o['steps'] = k['steps']
            if k.get('stepNames'): o['stepNames'] = k['stepNames']
            knobs.append(o)
        caps = {key: {x: c[x] for x in ('name', 'label', 'note', 'by', 'rig') if x in c} for key, c in a['caps'].items()}
        face = {
            'key': a['key'], 'name': a['name'], 'accent': a['accent'], 'based': a['based'],
            'intro': a.get('intro', ''), 'notes': a.get('notes', []),
            'W': a['W'], 'H': a['H'],
            'skins': [{'id': s['id'], 'ptrOff': bool(s.get('ptrOff'))} for s in a['skins']],
            'knobs': knobs, 'leds': a['leds'], 'buttons': a['buttons'], 'power': a['power'],
            'dims': a['dims'], 'switches': a.get('switches', []), 'caps': caps,
            'gain': a['gain'], 'mid': a.get('mid'), 'master': a.get('master'),
        }
        if a.get('sel'):
            face['sel'] = {x: a['sel'][x] for x in ('knob', 'label', 'names', 'note') if x in a['sel']}
        imgs = []
        for s in a['skins']:
            imgs += [s['id'] + '_on.jpg', s['id'] + '_off.jpg'] + [f"{s['id']}_k{j}.png" for j in range(len(knobs))]
            if s.get('ptrOff'): imgs += [f"{s['id']}_k{j}o.png" for j in range(len(knobs))]
        imgs += [f"{a['key']}_led_{l}.png" for l in a['leds']]
        missing = [f for f in imgs if not (SITE / 'img' / f).exists()]
        if missing:
            sys.exit(f"{a['key']}: missing images {missing[:5]}")
        for f in imgs:
            shutil.copy2(SITE / 'img' / f, OUT / 'img' / f); used.add(f)
        face['images'] = imgs
        (OUT / (a['key'] + '.json')).write_text(json.dumps(face, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')
        n += 1
    for f in (OUT / 'img').iterdir():     # images of amps that are gone
        if f.name not in used: f.unlink()
    size = sum(f.stat().st_size for f in (OUT / 'img').iterdir())
    print(f'{n} faces, {len(used)} images, {size / 1e6:.1f} MB -> {OUT.relative_to(ROOT)}')
    if n != len(have):
        sys.exit(f'amps without a face: {sorted(have - {a["key"] for a in amps})}')

if __name__ == '__main__':
    main()
