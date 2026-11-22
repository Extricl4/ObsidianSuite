#!/usr/bin/env python3
"""Draws the pedal faces the plugin shows on its pedal board.

    python3 plugin/tools/faces/build_pedals.py

Reads (never writes) the pedal designs of the visuals chat: visuals/pedals/<key>.json (enclosure colour,
knob / switch / LED / footswitch positions) and their artwork in visuals/art (<key>_art.png, the printed or
engraved top, and <key>_glow.png, the parts that light up). The 3D renders need Blender, so this draws the
same design flat with shading: enclosure with sheen and bevel, the artwork, machined knobs at their real
positions and values, toggles, buttons, LEDs, screws and the footswitch.

The plugin's own designs (plugin/tools/faces/pedals/<key>.json, art made by make_art.py there) are read
the same way; they may also have "screens": [{"at": [x, y], "w": cm, "h": cm, "color": [r, g, b]}], a glass
display the board writes live text on (its place goes into obsidian-pedals.js as "screen").

    python3 plugin/tools/faces/build_pedals.py nampedal     # just these pedals (the others stay as they are)

Writes, for every pedal the plugin has:
    plugin/WebUI/pedals/<key>_off.webp   the pedal switched off
    plugin/WebUI/pedals/<key>_on.webp    switched on: LED lit, glowing artwork
    plugin/WebUI/obsidian-pedals.js      sizes and where the LED and the footswitch are (for the board)
"""
import json, math, re
from pathlib import Path
import numpy as np
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[3]
VIS = ROOT / 'visuals'
OWN = ROOT / 'plugin' / 'tools' / 'faces' / 'pedals'       # the plugin's own designs (not the visuals chat's)
OUT = ROOT / 'plugin' / 'WebUI' / 'pedals'
JS = ROOT / 'plugin' / 'WebUI' / 'obsidian-pedals.js'
PX_PER_CM = 40            # output scale (a standard 6.6 x 12 cm pedal -> 264 x 480 px)
SS = 2                    # supersampling

def srgb(c):
    return np.clip(np.asarray(c, float), 0, 1) ** (1 / 2.2)

def plugin_keys():
    keys = set()
    for f in (ROOT / 'plugin' / 'Source' / 'DSP' / 'Modules').glob('*.cpp'):
        src = f.read_text()
        keys |= set(re.findall(r'(?:push_back \(\{ |add\w+Pedal \(t, )"([a-z0-9]+)"', src))
        keys |= set(re.findall(r'"([a-z0-9]{3,12})", "[A-Z][^"]*", "[a-z]+",', src))
    return keys

class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.rgb = np.zeros((h, w, 3)); self.a = np.zeros((h, w))
    def disc(self, cx, cy, r, fn, alpha=1.0):
        """Paints a disc; fn(nx, ny, rr) -> rgb array for normalized coords inside it."""
        x0, x1 = int(max(0, cx - r - 2)), int(min(self.w, cx + r + 3)); y0, y1 = int(max(0, cy - r - 2)), int(min(self.h, cy + r + 3))
        if x1 <= x0 or y1 <= y0: return
        ys, xs = np.mgrid[y0:y1, x0:x1].astype(float)
        nx, ny = (xs + .5 - cx) / r, (ys + .5 - cy) / r
        rr = np.sqrt(nx * nx + ny * ny)
        cov = np.clip((1 - rr) * r + .5, 0, 1) * alpha
        col = fn(nx, ny, rr)
        self.rgb[y0:y1, x0:x1] = self.rgb[y0:y1, x0:x1] * (1 - cov[..., None]) + col * cov[..., None]
        self.a[y0:y1, x0:x1] = np.maximum(self.a[y0:y1, x0:x1], cov)
    def image(self, im, x, y, mode='over', amount=1.0):
        im = np.asarray(im.convert('RGBA'), float) / 255
        h, w = im.shape[:2]
        x0, y0 = int(x), int(y); x1, y1 = min(self.w, x0 + w), min(self.h, y0 + h)
        sub = im[:y1 - y0, :x1 - x0]; al = sub[..., 3:4] * amount
        if mode == 'add': self.rgb[y0:y1, x0:x1] = np.clip(self.rgb[y0:y1, x0:x1] + sub[..., :3] * al, 0, 1.4)
        else:
            self.rgb[y0:y1, x0:x1] = self.rgb[y0:y1, x0:x1] * (1 - al) + sub[..., :3] * al
            self.a[y0:y1, x0:x1] = np.maximum(self.a[y0:y1, x0:x1], al[..., 0])

def chrome(nx, ny, rr):
    v = .42 + .38 * (-ny) * .7 + .9 * np.exp(-((nx + .35) ** 2 + (ny + .45) ** 2) / .05) + .25 * np.exp(-((nx - .4) ** 2 + (ny - .35) ** 2) / .08)
    v = v * (1 - .35 * np.clip((rr - .8) / .2, 0, 1))
    return np.stack([v, v * .99, v * .97], -1)

def knob_fn(style, part):
    base = {'black': (.07, .07, .075), 'alu': (.72, .72, .74), 'copper': (.85, .48, .3), 'cream': (.9, .86, .74)}[style]
    top = {'black': (.11, .11, .115), 'alu': (.78, .78, .8), 'copper': (.78, .78, .8), 'cream': (.92, .88, .77)}[style]
    metal = style in ('alu', 'copper')
    def skirt(nx, ny, rr):
        ang = np.arctan2(ny, nx)
        knurl = .5 + .5 * np.sign(np.sin(ang * 40))
        shade = .55 + .45 * (-ny * .6 - nx * .3 + .5)
        c = np.asarray(base)[None, None] * shade[..., None] * (.82 + .18 * knurl[..., None])
        if metal: c = c + .18 * np.exp(-((nx + .4) ** 2 + (ny + .5) ** 2) / .06)[..., None]
        return c
    def cap(nx, ny, rr):
        shade = .7 + .35 * (-ny * .5 - nx * .3) - .25 * rr ** 3
        c = np.asarray(top)[None, None] * shade[..., None]
        if metal:   # brushed concentric rings
            c = c * (.94 + .06 * np.sin(rr * 90)[..., None]) + .3 * np.exp(-((nx + .35) ** 2 + (ny + .4) ** 2) / .08)[..., None]
        else:
            c = c + .12 * np.exp(-((nx + .35) ** 2 + (ny + .45) ** 2) / .06)[..., None]
        return c
    return skirt if part == 'skirt' else cap

def shadow_layer(w, h, discs, blur):
    m = Image.new('L', (w, h), 0); px = m.load()
    arr = np.zeros((h, w))
    ys, xs = np.mgrid[0:h, 0:w]
    for cx, cy, r, a in discs:
        arr = np.maximum(arr, np.clip((r - np.sqrt((xs - cx) ** 2 + (ys - cy) ** 2)) / 2, 0, 1) * a)
    im = Image.fromarray((arr * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(blur))
    return np.asarray(im, float) / 255

def render(key, spec, lit):
    Wc, Lc = (spec.get('size') or [6.6, 12.0])[:2]
    artpx = 200
    s = PX_PER_CM * SS
    w, h = int(round(Wc * s)), int(round(Lc * s))
    P = lambda px, py: (px / artpx * s, py / artpx * s)
    cv = Canvas(w, h)
    enc = spec['enclosure']
    col = srgb(enc['color'])
    metal, rough, coat = enc.get('metal', 0), enc.get('rough', .5), enc.get('coat', 0)
    ys, xs = np.mgrid[0:h, 0:w].astype(float)
    u, v = xs / w, ys / h
    light = .78 + .32 * (1 - (u * .4 + v * .6))
    sheen = metal * (1 - rough) * .35 * np.exp(-((u - .3) ** 2 / .06 + (v - .22) ** 2 / .05))
    gloss = coat * .1 * np.exp(-((u - .62) ** 2 / .01 + (v - .1) ** 2 / .002))
    rng = np.random.default_rng(abs(hash(key)) % (2 ** 31))
    noise = np.asarray(Image.fromarray((rng.normal(.5, .5, (h, w)).clip(0, 1) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(.8)), float) / 255 - .5
    bump = (enc.get('bump') or ['noise', 0, 0])[2]
    body = col[None, None] * (light + noise * min(.25, bump * .4))[..., None] + (sheen + gloss)[..., None]
    # rounded body + bevel
    rad = int(.55 * s)
    mask = Image.new('L', (w, h), 0)
    from PIL import ImageDraw
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, w - 1, h - 1], rad, fill=255)
    m = np.asarray(mask, float) / 255
    inner = np.asarray(mask.filter(ImageFilter.GaussianBlur(s * .12)), float) / 255
    edge = np.clip((m - inner) * 3, 0, 1)
    tl = np.clip(1 - (u + v), -1, 1)
    rim = srgb(spec.get('rimcolor', [.8, .85, 1]))
    body = body * (1 - .35 * edge[..., None]) + edge[..., None] * (np.clip(tl, 0, 1)[..., None] * .35 + np.clip(-tl, 0, 1)[..., None] * rim[None, None] * .25)
    cv.rgb = body; cv.a = m
    # artwork (+ glow when lit)
    def art(path):
        p = Path(spec.get('_dir', VIS)) / path
        return Image.open(p).convert('RGBA').resize((w, h), Image.LANCZOS) if p.exists() else None
    a = art(spec['art'].replace('art/', 'art/')) if spec.get('art') else None
    if a is not None:
        am = spec.get('artmat', {}).get('metal', 0)
        arr = np.asarray(a, float) / 255
        if am > 0:
            sh = (.85 + .35 * np.exp(-((u - .3) ** 2 / .08 + (v - .25) ** 2 / .08)))[..., None]
            arr[..., :3] = np.clip(arr[..., :3] * sh, 0, 1)
        cv.image(Image.fromarray((arr * 255).astype(np.uint8)), 0, 0)
    tread = spec.get('treadle')
    if tread:
        tx, ty = P(*tread['at']); tw, tl2 = tread['w'] * s, tread['l'] * s
        tr = Image.new('RGBA', (int(tw), int(tl2)), (0, 0, 0, 0))
        ImageDraw.Draw(tr).rounded_rectangle([0, 0, int(tw) - 1, int(tl2) - 1], int(s * .4), fill=(20, 20, 22, 255))
        ta = VIS / tread['art']
        if ta.exists(): tr.alpha_composite(Image.open(ta).convert('RGBA').resize(tr.size, Image.LANCZOS))
        cv.image(tr, tx - tw / 2, ty - tl2 / 2)
    if lit and spec.get('glow'):
        g = art(spec['glow'])
        if g is not None: cv.image(g, 0, 0, 'add', min(1.0, .35 * spec.get('glow_strength', 1.5)))
    # hardware shadows
    discs = []
    for k in spec.get('knobs', []):
        x, y = P(*k['at']); discs.append((x + s * .08, y + s * .16, k['r'] * s * 1.02, .75))
    for f in spec.get('footswitches', []):
        x, y = P(f[0], f[1]); discs.append((x + s * .06, y + s * .12, .55 * s * f[2], .7))
    if discs:
        sh = shadow_layer(w, h, discs, s * .12)
        cv.rgb = cv.rgb * (1 - .65 * sh[..., None])
    # screws
    for sc in spec.get('screws', []):
        x, y = P(*sc); r = .13 * s
        cv.disc(x, y, r, chrome)
        cv.disc(x, y, r * .2, lambda nx, ny, rr: np.zeros(nx.shape + (3,)) + .15)
    # buttons
    for bt in spec.get('buttons', []):
        x, y = P(*bt['at']); bw, bh = bt['w'] * s, bt['h'] * s
        c = srgb(bt['color']); em = bt.get('emit')
        if lit and em: c = np.clip(c + srgb(em) * .55, 0, 1)
        btn = Image.new('RGBA', (int(bw), int(bh)), (0, 0, 0, 0))
        ImageDraw.Draw(btn).rounded_rectangle([0, 0, int(bw) - 1, int(bh) - 1], int(min(bw, bh) * .25), fill=tuple(int(v * 255) for v in c) + (255,))
        arr = np.asarray(btn, float) / 255
        yy = np.linspace(1.15, .8, arr.shape[0])[:, None, None]
        arr[..., :3] = np.clip(arr[..., :3] * yy + .06, 0, 1)
        cv.image(Image.fromarray((arr * 255).astype(np.uint8)), x - bw / 2, y - bh / 2)
    # toggles: nut + chrome lever + ball
    for t in spec.get('toggles', []):
        x, y = P(t[0], t[1]); tilt = t[2] if len(t) > 2 else -20
        cv.disc(x, y, .3 * s, chrome)
        ln = .9 * s * abs(math.sin(math.radians(tilt))) + .25 * s
        d = -1 if tilt < 0 else 1
        for i in range(int(ln)):
            cv.disc(x, y + d * i, .07 * s, chrome)
        cv.disc(x, y + d * ln, .11 * s, chrome)
    # screens: a bezel and dark glass (the board writes on it), faintly backlit when on
    for sc in spec.get('screens', []):
        x, y = P(*sc['at']); sw, sh = sc['w'] * s, sc['h'] * s; bz = .14 * s
        fr = Image.new('RGBA', (int(sw + 2 * bz), int(sh + 2 * bz)), (0, 0, 0, 0))
        ImageDraw.Draw(fr).rounded_rectangle([0, 0, fr.width - 1, fr.height - 1], int(.2 * s), fill=(30, 30, 35, 255))
        fa = np.asarray(fr, float) / 255
        fa[..., :3] = np.clip(fa[..., :3] * np.linspace(1.5, .7, fa.shape[0])[:, None, None], 0, 1)
        cv.image(Image.fromarray((fa * 255).astype(np.uint8)), x - sw / 2 - bz, y - sh / 2 - bz)
        gl = Image.new('RGBA', (int(sw), int(sh)), (0, 0, 0, 0))
        ImageDraw.Draw(gl).rounded_rectangle([0, 0, int(sw) - 1, int(sh) - 1], int(.09 * s), fill=(5, 7, 9, 255))
        ga = np.asarray(gl, float) / 255
        gy, gx = np.mgrid[0:ga.shape[0], 0:ga.shape[1]].astype(float); gy /= ga.shape[0]; gx /= ga.shape[1]
        tint = srgb(sc.get('color', [.35, .88, 1]))
        glass = .05 * np.exp(-((gx - .55 * gy - .05) ** 2) / .004)[..., None]                      # diagonal reflection
        glass = glass - .05 * np.exp(-gy / .08)[..., None]                                          # recessed top edge
        if lit: glass = glass + tint[None, None] * .07 * np.exp(-((gx - .5) ** 2 / .3 + (gy - .5) ** 2 / .3))[..., None]
        ga[..., :3] = np.clip(ga[..., :3] + glass, 0, 1)
        cv.image(Image.fromarray((ga * 255).astype(np.uint8)), x - sw / 2, y - sh / 2)
    # knobs
    for k in spec.get('knobs', []):
        x, y = P(*k['at']); r = k['r'] * s; st = k.get('style', 'black')
        cv.disc(x, y, r, knob_fn(st, 'skirt'))
        cv.disc(x, y, r * .82, knob_fn(st, 'cap'))
        ang = math.radians(135 - 270 * k.get('v', .5))
        pc = (.95, .95, .93) if st == 'black' else (.12, .12, .12)
        for i in range(24):
            t = .12 + .62 * i / 23
            cv.disc(x + math.cos(ang) * r * t, y - math.sin(ang) * r * t, max(1.2, r * .05), lambda nx, ny, rr: np.zeros(nx.shape + (3,)) + pc)
    # footswitch
    for f in spec.get('footswitches', []):
        x, y = P(f[0], f[1]); sc2 = f[2]
        cv.disc(x, y, .62 * s * sc2, lambda nx, ny, rr: chrome(nx, ny, rr) * .85)
        cv.disc(x, y, .52 * s * sc2, chrome)
        cv.disc(x, y, .4 * s * sc2, lambda nx, ny, rr: chrome(nx * .9, ny * .9 - .1, rr) * (1 - .25 * rr[..., None] ** 4))
    # LEDs
    for l in spec.get('leds', []):
        x, y = P(*l['at']); c = srgb(l['color'])
        cv.disc(x, y, .22 * s, chrome)
        if lit:
            halo = shadow_layer(w, h, [(x, y, .5 * s, 1)], s * .35)
            cv.rgb = np.clip(cv.rgb + halo[..., None] * c[None, None] * .9, 0, 1.2)
            cv.disc(x, y, .15 * s, lambda nx, ny, rr: np.clip(c[None, None] * (1 - .3 * rr[..., None]) + .65 * np.exp(-((nx + .3) ** 2 + (ny + .3) ** 2) / .1)[..., None], 0, 1))
        else:
            cv.disc(x, y, .15 * s, lambda nx, ny, rr: c[None, None] * .22 + .25 * np.exp(-((nx + .3) ** 2 + (ny + .35) ** 2) / .05)[..., None])
    rgb = np.clip(cv.rgb, 0, 1) * m[..., None]
    img = Image.fromarray((np.dstack([rgb, cv.a * m]) * 255).astype(np.uint8), 'RGBA')
    img = img.resize((w // SS, h // SS), Image.LANCZOS)
    pos = {'w': w // SS, 'h': h // SS,
           'led': [[round(l['at'][0] / (Wc * artpx), 4), round(l['at'][1] / (Lc * artpx), 4), '#%02x%02x%02x' % tuple(int(v * 255) for v in srgb(l['color']))] for l in spec.get('leds', [])],
           'fs': [[round(f[0] / (Wc * artpx), 4), round(f[1] / (Lc * artpx), 4), round(.62 * f[2] / Wc, 4)] for f in spec.get('footswitches', [])]}
    for sc in spec.get('screens', [])[:1]:   # the glass, as fractions: left, top, width, height
        pos['screen'] = [round((sc['at'][0] / artpx - sc['w'] / 2) / Wc, 4), round((sc['at'][1] / artpx - sc['h'] / 2) / Lc, 4), round(sc['w'] / Wc, 4), round(sc['h'] / Lc, 4)]
    return img, pos

def main():
    import sys
    only = set(sys.argv[1:])               # just these pedals; the others keep their pictures and entries
    keys = plugin_keys()
    OUT.mkdir(parents=True, exist_ok=True)
    data, made = {}, []
    if only and JS.exists():
        txt = JS.read_text(encoding='utf-8'); data = json.loads(txt[txt.index('{'):txt.rindex('}') + 1])
    designs = {f.stem: f for f in sorted((VIS / 'pedals').glob('*.json'))}
    designs.update({f.stem: f for f in sorted(OWN.glob('*.json'))})      # the plugin's own designs
    for key, f in sorted(designs.items()):
        if key not in keys or (only and key not in only): continue
        spec = json.loads(f.read_text())
        if f.parent == OWN: spec['_dir'] = str(OWN)
        off, pos = render(key, spec, False)
        on, _ = render(key, spec, True)
        off.save(OUT / f'{key}_off.webp', 'WEBP', quality=86, method=6)
        on.save(OUT / f'{key}_on.webp', 'WEBP', quality=86, method=6)
        data[key] = pos; made.append(key)
    for old in OUT.glob('*.webp'):
        if not only and old.stem.rsplit('_', 1)[0] not in data: old.unlink()
    JS.write_text('/* Generated by plugin/tools/faces/build_pedals.py: pedal face sizes, LED and footswitch positions (fractions). */\n'
                  'window.OBS_PEDALS = ' + json.dumps(data, separators=(',', ':')) + ';\n', encoding='utf-8')
    size = sum(p.stat().st_size for p in OUT.glob('*.webp'))
    print(f'{len(made)} pedal faces ({size / 1e6:.1f} MB): {" ".join(made)}')
    missing = sorted(keys - set(made))
    if missing and not only: print('plugin modules without a pedal design:', ' '.join(missing))

if __name__ == '__main__':
    main()
