#!/usr/bin/env python3
"""Builds the faces of the plugin's own capture amps (Nemean, Phlegethon...) from their renders.

    python3 -I plugin/tools/faces/amps/make_amps.py           art + specs
    PY=<python with bpy> plugin/tools/faces/amps/render.sh    layer renders -> amps/out/layers/<skin>/
    python3 -I plugin/tools/faces/build_own_amps.py           -> plugin/Content/Faces/<key>.json + img/

The same face format build_faces.py writes for the visuals chat's amps (and trimmed the same way), so the
plugin can't tell them apart. These amps aren't on the finished-amps page: build_faces.py leaves them alone.
A skin that only has its 'on' render borrows the off face, pointer sprites and LEDs of the amp's first skin
(the glow colour is the only thing that changes between skins).
"""
import json, math, shutil, sys
from pathlib import Path
from PIL import Image
sys.path.insert(0, str(Path(__file__).resolve().parent))
from trim import trim, move

HERE = Path(__file__).resolve().parent
L = HERE / 'amps' / 'out' / 'layers'
OUT = HERE.parents[1] / 'Content' / 'Faces'
AMPS = HERE.parents[1] / 'Content' / 'Amps'

ALBUM_NOTES = ["Cowboys From Hell (1990): the rawest of the four, more mids and bite.",
               "Vulgar Display of Power (1992), the Walk tone: the famous scooped, tight Dime crunch.",
               "Far Beyond Driven (1994), the I'm Broken tone: heavier, more gain and low end.",
               "The Great Southern Trendkill (1996): the darkest and most saturated."]

OWN = [
    dict(key='nemean', name='Nemean', accent='#ff3b1f', skins=['nemean_cfh', 'nemean_vdop', 'nemean_fbd', 'nemean_tgst'],
         based="Dimebag's Randall rigs, one capture per Pantera album",
         intro=None, labels=['Gain', 'Bass', 'Middle', 'Treble', 'Presence', 'Tight', 'Master'],
         caps={str(i): dict(name=n, note=ALBUM_NOTES[i], by='one', rig=True) for i, n in
               enumerate(['Cowboys From Hell', 'Vulgar Display of Power', 'Far Beyond Driven', 'The Great Southern Trendkill'])},
         notes=["One capture per album, each a full rig (amp and cab), so the plugin's cab is bypassed on this amp.",
                "COWBOYS, VULGAR, DRIVEN and TRENDKILL pick the album. Each album remembers its own Gain, EQ and Tight.",
                "Gain drives the input into the capture; Bass, Middle, Treble and Presence are a tone stack around it, "
                "Tight trims the low end before the amp."]),
    dict(key='phlegethon', name='Phlegethon', accent='#ff5a14', skins=['phlegethon'], mask_from='nemean_cfh',
         based='Neural DSP Archetype: Gojira X',
         intro=None, labels=['Gain', 'Bass', 'Middle', 'Treble', 'Presence', 'Depth', 'Master'],
         caps={'': dict(by='one', rig=True)},
         notes=["A single capture of the Gojira X plugin's \"Black Go\" tone, amp and cab together (full rig), "
                "so the plugin's cab is bypassed on this amp.",
                "Gain drives the input into it; Bass, Middle, Treble, Presence and Depth shape the tone around it."]),
]


def need(skin, f):
    p = L / skin / f
    return p if p.exists() else None


def build(a):
    amp = json.loads((AMPS / (a['key'] + '.json')).read_text(encoding='utf-8'))
    first = a['skins'][0]
    for f in ('on.png', 'off.png', 'ptr.png', 'map.json'):
        if not need(first, f): sys.exit(f"{a['key']}: no {first}/{f} yet (run amps/render.sh)")
    for s in a['skins']:
        if not need(s, 'on.png'): sys.exit(f"{a['key']}: no {s}/on.png yet (run amps/render.sh)")
    m = json.loads((L / first / 'map.json').read_text())
    spec = json.loads((HERE / 'amps' / (first + '.json')).read_text())
    kspec = [k for z in spec['zones'] for k in z.get('knobs', [])]
    knobs = []
    for i, k in enumerate(h for h in m['hot'] if h['kind'] == 'knob'):
        cx, cy = k['px']; hr = math.ceil(k['rpx'] * 1.18) + 2
        x0, y0 = int(math.floor(cx - hr)), int(math.floor(cy - hr)); x1, y1 = int(math.ceil(cx + hr)), int(math.ceil(cy + hr))
        knobs.append(dict(label=a['labels'][i], b=[x0, y0, x1 - x0, y1 - y0],
                          o=[round((cx - x0) / (x1 - x0) * 100, 3), round((cy - y0) / (y1 - y0) * 100, 3)],
                          c=[round(cx, 1), round(cy, 1)], r=round(k['rpx'], 1), v=kspec[i].get('v', 0.5)))
    ptr_off = need(first, 'ptr_off.png') is not None
    leds, buttons = {}, {}
    ledim = Image.open(need(first, 'led.png')) if need(first, 'led.png') else None
    for h in m['hot']:
        if h['kind'] == 'led' and ledim is not None:
            bx = h['box']; p = 3
            x0, y0 = int(min(bx[0], bx[2]) - p), int(min(bx[1], bx[3]) - p); x1, y1 = int(max(bx[0], bx[2]) + p), int(max(bx[1], bx[3]) + p)
            leds[h['id']] = ([x0, y0, x1 - x0, y1 - y0], ledim.crop((x0, y0, x1, y1)))
        if h['kind'] == 'button':
            bx = h['box']
            buttons[h['id']] = [round(min(bx[0], bx[2]), 1), round(min(bx[1], bx[3]), 1), round(abs(bx[2] - bx[0]), 1), round(abs(bx[3] - bx[1]), 1)]
    pw = [h for h in m['hot'] if h['kind'] == 'jewel'][0]
    face = dict(key=a['key'], name=a['name'], accent=a['accent'], based=a['based'], intro=a['intro'] or amp['description'], notes=a['notes'],
                W=m['w'], H=m['h'], skins=[dict(id=s, ptrOff=ptr_off) for s in a['skins']], knobs=knobs,
                leds={k: v[0] for k, v in leds.items()}, buttons=buttons, power=[round(pw['px'][0], 1), round(pw['px'][1], 1), round(pw['rpx'], 1)],
                dims=amp['map'].get('buttons', []), switches=[], caps=a['caps'], gain=amp['map']['gainKnob'], mid=None, master=None)
    # faces: every skin's on, the first skin's off for skins that have none; trimmed together so they line up
    bases = {}
    for s in a['skins']:
        bases[s + '_on'] = Image.open(L / s / 'on.png')
        bases[s + '_off'] = Image.open(need(s, 'off.png') or L / first / 'off.png')
    ref = a.get('mask_from')                  # outline taken from another head's render (same size and camera)
    cut, (dx, dy), (W, H) = trim(a['key'], bases, Image.open(L / ref / 'on.png') if ref else None)
    used = set()
    for b, im in cut.items():
        im.save(OUT / 'img' / (b + '.webp'), 'WEBP', quality=90, method=6); used.add(b + '.webp')
    move(face, dx, dy, W, H)
    # sprites are cut from the untrimmed renders at the untrimmed positions (the face JSON holds the moved ones)
    imgs = [f'{s}_{v}.webp' for s in a['skins'] for v in ('on', 'off')]
    for s in a['skins']:
        pr = Image.open(need(s, 'ptr.png') or L / first / 'ptr.png')
        po = Image.open(need(s, 'ptr_off.png') or L / first / 'ptr_off.png') if ptr_off else None
        for j, k in enumerate(knobs):
            x0, y0, w, h = k['b'][0] + dx, k['b'][1] + dy, k['b'][2], k['b'][3]
            pr.crop((x0, y0, x0 + w, y0 + h)).save(OUT / 'img' / f'{s}_k{j}.png', optimize=True); imgs.append(f'{s}_k{j}.png')
            if po:
                po.crop((x0, y0, x0 + w, y0 + h)).save(OUT / 'img' / f'{s}_k{j}o.png', optimize=True); imgs.append(f'{s}_k{j}o.png')
    for lid, (_, im) in leds.items():
        im.save(OUT / 'img' / f"{a['key']}_led_{lid}.png", optimize=True); imgs.append(f"{a['key']}_led_{lid}.png")
    face['images'] = imgs
    (OUT / (a['key'] + '.json')).write_text(json.dumps(face, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')
    print(a['key'], f'{W}x{H},', len(imgs), 'images')
    return set(imgs)


def own_keys():
    return {a['key'] for a in OWN}


def own_images():
    """every image the own amps' faces use (build_faces.py keeps them when it clears out old images)"""
    out = set()
    for a in OWN:
        f = OUT / (a['key'] + '.json')
        if f.exists():
            out |= set(json.loads(f.read_text(encoding='utf-8')).get('images', []))
    return out


if __name__ == '__main__':
    (OUT / 'img').mkdir(parents=True, exist_ok=True)
    keys = sys.argv[1:]
    for a in OWN:
        if not keys or a['key'] in keys:
            build(a)
