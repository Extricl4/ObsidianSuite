#!/usr/bin/env python3
"""The plugin's own capture-amp faces: art (heightmaps, hole masks, glow images, panel labels) and render specs.

    python3 -I plugin/tools/faces/amps/make_amps.py          writes art/*.png and <skin>.json here

Then render (render.sh) and build the plugin faces (build_own_amps.py). The drawing helpers come from
visuals/tools (relief_art, panel_art), used read-only; nothing here writes into visuals/.

NEMEAN  (Pantera, the "Full Tone" captures, one per album): black diamond tread plate with four claw
        slashes torn right through it, lit from behind in the album's colour (Cowboys a yellow-brown tint,
        Vulgar grey-red, Driven blue-black, Trendkill red-orange). The buttons pick the album.
PHLEGETHON (Gojira, the Archetype X "Black Go" capture): a slab of cooling magma, black crust plates
        split by molten cracks that run white-hot at the bottom.
"""
import json, math, sys
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy import ndimage as ndi
from scipy.spatial import cKDTree

HERE = Path(__file__).resolve().parent
VIS = HERE.parents[3] / 'visuals'
sys.path.insert(0, str(VIS / 'tools'))
import relief_art as ra                     # noqa: E402
ra.PX = 120
from relief_art import Relief               # noqa: E402
import panel_art                            # noqa: E402
from panel_art import panel                 # noqa: E402

ART = HERE / 'art'; ART.mkdir(exist_ok=True)
FONT = VIS / 'render' / 'fonts'
W, H = 22.56, 5.55                          # the upper zone (z 4.25 .. 9.8) of a 24" head
KZ = 2.2975                                 # knob row height on the panel


def saira(px, wght=900, wdth=100):
    f = ImageFont.truetype(str(FONT / 'Saira.ttf'), int(px))
    f.set_variation_by_axes([wght, wdth])
    return f


def text_mask(r, text, f, cx, cy, shear=0.0):
    """mask of a text line centred at (cx, cy) inches; shear leans it (italic)"""
    im = Image.new('L', (r.w, r.h), 0)
    d = ImageDraw.Draw(im)
    x, y = r.px(cx, cy)
    d.text((x, y), text, font=f, fill=255, anchor='mm')
    if shear:
        im = im.transform(im.size, Image.AFFINE, (1, shear, -shear * y, 0, 1, 0), resample=Image.BICUBIC)
    return np.array(im) > 127


def fbm(shape, scales, seed, weights=None):
    rng = np.random.default_rng(seed)
    out = np.zeros(shape, np.float32)
    weights = weights or [1.0 / (i + 1) for i in range(len(scales))]
    for s, w in zip(scales, weights):
        n = ndi.gaussian_filter(rng.standard_normal(shape).astype(np.float32), s)
        out += w * n / (n.std() + 1e-9)
    return out / sum(weights)


def smooth(t):
    t = np.clip(t, 0, 1)
    return t * t * (3 - 2 * t)


# ======================================================================================= NEMEAN
def nemean():
    r = Relief(W, H, base=0.18)
    X, Y = r.X, r.Y
    # diamond tread plate: raised lens bars, alternating +-45 degrees on a checkerboard
    P = 0.5
    gx = np.floor(X / P); gy = np.floor(Y / P)
    lx = X - (gx + 0.5) * P; ly = Y - (gy + 0.5) * P
    sgn = np.where((gx + gy) % 2 == 0, 1.0, -1.0)
    a = math.radians(45)
    u = lx * math.cos(a) + sgn * ly * math.sin(a); v = -lx * math.sin(a) * sgn + ly * math.cos(a)
    L, Wd = 0.2, 0.05                      # half length, half width
    q = (u / L) ** 2 + (v / Wd) ** 2
    bar = np.clip(1 - q, 0, 1) ** 0.45
    r.H = (0.2 + 0.24 * bar).astype(np.float32)
    # frame
    d = np.minimum.reduce([X + W / 2, W / 2 - X, Y + H / 2, H / 2 - Y])
    fr = d < 0.26
    r.H[fr] = 0.5
    r.H[(d >= 0.26) & (d < 0.3)] = 0.12

    # four claw slashes, top right to bottom left, curved, tapered both ends
    holes = np.zeros_like(r.holes)
    rim = np.zeros_like(r.H)
    jag = fbm(r.H.shape, [5, 14], 11)
    for i in range(4):
        off = (i - 1.5) * 0.95
        n = 260
        t = np.linspace(0, 1, n)
        # an arc from upper right to lower left
        x = 8.8 - 17.6 * t + off * 0.45
        y = 2.05 - 4.1 * t + 0.55 * np.sin(math.pi * t) + off * 0.8
        wmax = [0.32, 0.4, 0.42, 0.34][i]
        w = wmax * np.sin(math.pi * np.clip(t * 1.04 - 0.02, 0, 1)) ** 0.65
        im = Image.new('L', (r.w, r.h), 0); dd = ImageDraw.Draw(im)
        for xi, yi, wi in zip(x, y, w):
            cx, cy = r.px(xi, yi); rr = wi * ra.PX / 2
            if rr > 0.5:
                dd.ellipse((cx - rr, cy - rr, cx + rr, cy + rr), fill=255)
        m = np.array(im) > 127
        m &= ~fr
        holes |= m
    # torn metal: a jagged lip bent up around each slash, the plate dished in just outside it
    dist = ndi.distance_transform_edt(~holes) / ra.PX
    lipw = 0.075 + 0.03 * jag
    t = np.clip(dist / lipw, 0, 1)
    lip = (dist > 0) & (dist < lipw)
    r.H = np.where(lip, np.maximum(r.H, 0.56 * np.sin(np.pi * (0.35 + 0.65 * t)) ** 0.5 + 0.04 * jag), r.H)
    dish = (dist >= lipw) & (dist < lipw + 0.22)
    r.H = np.where(dish, r.H - 0.12 * np.clip(1 - (dist - lipw) / 0.22, 0, 1) ** 1.5, r.H)

    r.H = np.clip(r.H, 0, 1)
    r.holes = holes
    r.save(str(ART / 'nemean_h.png'), str(ART / 'nemean_holes.png'), blur=0.6)

    # the glow behind the slashes, one per album: brightest along the middle of the plate
    gw, gh = 1400, 344
    yy, xx = np.mgrid[0:gh, 0:gw]
    n = fbm((gh, gw), [6, 20], 5)
    core = np.exp(-((yy / gh - 0.5) / 0.34) ** 2)
    for key, col in SKINS_N:
        c = np.array(col, np.float32)
        i = np.clip(0.55 + 0.45 * core + 0.12 * n, 0.2, 1.2)[..., None]
        hot = np.clip((i - 1.02) * 2.0, 0, 0.35)                 # only the hottest spots go a little towards white
        rgb = c * np.minimum(i, 1.0) * (1 - hot) + (0.6 * c + 0.4) * hot
        Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8)).save(ART / f'nemean_glow_{key}.png')


SKINS_N = [('cfh', [0.72, 0.5, 0.24]), ('vdop', [0.55, 0.24, 0.23]), ('fbd', [0.07, 0.13, 0.55]), ('tgst', [1.0, 0.3, 0.05])]
GLOW_N = {'cfh': 2.3, 'vdop': 2.2, 'fbd': 1.1, 'tgst': 3.2}       # blue-black glows low, red-orange hot
NAMES_N = ['COWBOYS', 'VULGAR', 'DRIVEN', 'TRENDKILL']
KX_N = [-9.55, -7.3, -5.05, -2.8, -0.55, 1.7, 3.95]
V_N = [0.62, 0.62, 0.38, 0.62, 0.6, 0.55, 0.5]
LAB_N = ['GAIN', 'BASS', 'MIDDLE', 'TREBLE', 'PRESENCE', 'TIGHT', 'MASTER']
BX_N = [5.6, 6.65, 7.7, 8.75]


def nemean_specs():
    kn = [dict(x=x, z=KZ, r=0.43, v=v, shape='knurl', mat='bakelite', mat2='silver', ptr='bakelite') for x, v in zip(KX_N, V_N)]
    specs = {}
    for key, col in SKINS_N:
        spec = dict(size=[24, 10, 10.5], border=0.7, recess=1.0, bevel=0.2, rimcolor=[0.75, 0.75, 0.8],
                    body=dict(color=[0.012, 0.012, 0.013], rough=0.8, bump=['pebble', 40, 0.25]),
                    handle=dict(w=8.5, h=1.2, color=[0.012, 0.012, 0.013]),
                    glow_strips=[dict(z=4.25, color=col, strength=3.0 * GLOW_N[key] / 3, w=22.54)],
                    zones=[dict(type='relief', z=[4.25, 9.8], height='art/nemean_h.png', holes='art/nemean_holes.png', depth=0.32, res=84,
                                color=[0.16, 0.16, 0.17], low=[0.008, 0.008, 0.009], lo_at=0.12, hi_at=0.5, metal=0.85, metal_hi=1.0,
                                rough=0.42, grain=[260, 0.012], gloss=[0.85, [0.8, 0.8, 0.83], 0.1, 1.0],
                                glass=f'art/nemean_glow_{key}.png', glass_base=[0.01, 0.01, 0.01], glass_strength=GLOW_N[key], glass_y=0.35,
                                glass_lights=[dict(x=x, color=col, energy=14 * GLOW_N[key] / 3, size=6) for x in (-6, 0, 6)]),
                           dict(type='panel', z=[0.7, 4.25], art='art/nemean_panel.png', artemis=0.15, color=[0.011, 0.011, 0.012], metal=0.15,
                                rough=0.7, bump=['noise', 300, 0.012], knobs=kn,
                                buttons=[dict(id=f'al{i + 1}', x=x, z=KZ + 0.05, w=0.5, h=0.34, cap=[0.02, 0.02, 0.022], metal=0.6, rough=0.35,
                                              led=c, ledr=0.06, led_str=3.5) for i, (x, (_, c)) in enumerate(zip(BX_N, SKINS_N))],
                                jewels=[dict(x=10.05, z=KZ + 0.05, color=col, r=0.26, strength=3.5 * GLOW_N[key] / 2.5, id='power')])])
        specs['nemean_' + key] = spec
    return specs


def nemean_panel(spec):
    kn = [dict(x=x, z=KZ, r=0.43, label=l) for x, l in zip(KX_N, LAB_N)]
    def extra(d, P, col):
        f = saira(24, 700, 75)
        for x, nm in zip(BX_N, NAMES_N):
            cx, cy = P(x, KZ - 0.42)
            d.text((cx, cy), nm, font=f, fill=col, anchor='mm')
        cx, cy = P(10.05, KZ - 0.52)
        d.text((cx, cy), 'POWER', font=f, fill=col, anchor='mm')
        # a thin bracket over the four album buttons
        x0, y0 = P(BX_N[0] - 0.45, KZ + 0.72); x1, _ = P(BX_N[-1] + 0.45, KZ + 0.72)
        d.line([(x0, y0 + 10), (x0, y0), (x1, y0), (x1, y0 + 10)], fill=col, width=2)
        t = saira(22, 600, 100)
        mx = (x0 + x1) / 2; tw = d.textlength('ALBUM', font=t) + 16
        d.rectangle((mx - tw / 2, y0 - 4, mx + tw / 2, y0 + 4), fill=(0, 0, 0, 0))
        d.text((mx, y0), 'ALBUM', font=t, fill=col, anchor='mm')
    panel_art.FONTS['sairac'] = 'Saira.ttf'
    old = panel_art.font
    panel_art.font = lambda k, s, wght=None: saira(s, 700, 75) if k == 'sairac' else old(k, s, wght)
    try:
        panel(str(ART / 'nemean_panel.png'), spec, (0.7, 4.25), kn, face='sairac', col=(222, 222, 226, 255), size=30,
              dial='lines', tick_len=0.11, spacing=3, lines=[(-10.9, 0.98, 10.9, 0.98, 2)], extra=extra)
    finally:
        panel_art.font = old


# =================================================================================== PHLEGETHON
def phlegethon():
    r = Relief(W, H, base=0.0)
    X, Y = r.X, r.Y
    rng = np.random.default_rng(23)
    # warp the plane so the cracks wander instead of running straight
    wx = fbm(r.H.shape, [40, 14], 1) * 0.16; wy = fbm(r.H.shape, [40, 14], 2) * 0.16
    Xw, Yw = X + wx, Y + wy
    hot = smooth((H / 2 - Y) / H)            # 0 at the top .. 1 at the bottom
    # crust plates: a jittered grid of sites, smaller and more broken towards the hot bottom
    sites = []
    for gy in np.arange(-H / 2 - 0.4, H / 2 + 0.9, 0.95):
        sx = 1.25 if gy > 0.6 else 0.95
        for gx in np.arange(-W / 2 - 0.6, W / 2 + 1.2, sx):
            sites.append((gx + rng.uniform(-0.42, 0.42) * sx, gy + rng.uniform(-0.42, 0.42) * 0.95))
    sites = np.array(sites)
    tree = cKDTree(sites)
    pts = np.stack([Xw.ravel(), Yw.ravel()], 1)
    dd, ii = tree.query(pts, k=2)
    p1, p2 = sites[ii[:, 0]], sites[ii[:, 1]]
    edge = ((dd[:, 1] ** 2 - dd[:, 0] ** 2) / (2 * np.linalg.norm(p2 - p1, axis=1))).reshape(r.H.shape)
    cell = ii[:, 0].reshape(r.H.shape)
    nz = fbm(r.H.shape, [2, 6], 3)
    cw = 0.035 + 0.085 * hot + 0.018 * nz                      # crack half width: hairline at the top, wide rivers below
    holes = edge < np.maximum(cw, 0.012)
    # plates: domed crust, each at its own height and tilt, rough rock on top
    ch = rng.uniform(0.5, 0.78, len(sites)).astype(np.float32)[cell]
    tx = rng.uniform(-0.08, 0.08, len(sites)).astype(np.float32); ty = rng.uniform(-0.08, 0.08, len(sites)).astype(np.float32)
    tilt = tx[cell] * (X - sites[cell, 0]) + ty[cell] * (Y - sites[cell, 1])
    prof = smooth((edge - cw) / 0.22) ** 0.7
    rock = fbm(r.H.shape, [0.8, 2.5, 7], 4, [0.35, 0.4, 0.25])
    r.H = np.clip(0.06 + (ch - 0.06) * prof + tilt + 0.045 * rock * prof, 0, 0.86).astype(np.float32)
    # hairline fractures inside the plates: shallow, they only glow a little
    fine = []
    for gy in np.arange(-H / 2, H / 2 + 0.5, 0.42):
        for gx in np.arange(-W / 2, W / 2 + 0.5, 0.48):
            fine.append((gx + rng.uniform(-0.2, 0.2), gy + rng.uniform(-0.18, 0.18)))
    fine = np.array(fine)
    d2, i2 = cKDTree(fine).query(pts, k=2)
    q1, q2 = fine[i2[:, 0]], fine[i2[:, 1]]
    e2 = ((d2[:, 1] ** 2 - d2[:, 0] ** 2) / (2 * np.linalg.norm(q2 - q1, axis=1))).reshape(r.H.shape)
    keep = rng.random(len(fine)) < 0.55
    fmask = (e2 < 0.012) & keep[i2[:, 0].reshape(r.H.shape)] & (prof > 0.5)
    r.H = np.where(fmask, r.H * 0.45, r.H)

    # a heavy basalt frame keeps it in
    d = np.minimum.reduce([X + W / 2, W / 2 - X, Y + H / 2, H / 2 - Y])
    fr = d < 0.22
    r.H[fr] = 0.55 + 0.05 * rock[fr]
    holes &= ~ndi.binary_dilation(fr, iterations=3)
    r.H[holes] = 0.0
    r.holes = holes
    r.save(str(ART / 'phlegethon_h.png'), str(ART / 'phlegethon_holes.png'), blur=0.7)

    # the molten rock seen through the cracks: dark red at the top, orange, yellow-white at the bottom
    gw, gh = 1400, 344
    yy, xx = np.mgrid[0:gh, 0:gw]
    hv = smooth(yy / gh)
    n = fbm((gh, gw), [3, 10, 30], 6)
    heat = np.clip(0.28 + 0.62 * hv + 0.16 * n, 0, 1)
    stops = [(0.0, (0.3, 0.015, 0.0)), (0.35, (0.8, 0.08, 0.0)), (0.6, (1.0, 0.26, 0.01)), (0.82, (1.0, 0.5, 0.05)), (1.0, (1.0, 0.74, 0.24))]
    rgb = np.zeros((gh, gw, 3), np.float32)
    for (a0, c0), (a1, c1) in zip(stops, stops[1:]):
        m = (heat >= a0) & (heat <= a1)
        t = ((heat - a0) / (a1 - a0))[m][:, None]
        rgb[m] = np.array(c0) * (1 - t) + np.array(c1) * t
    Image.fromarray((rgb * 255).astype(np.uint8)).save(ART / 'phlegethon_glow.png')


KX_P = [-8.25, -5.75, -3.25, -0.75, 1.75, 4.25, 6.75]
V_P = [0.7, 0.55, 0.45, 0.55, 0.5, 0.5, 0.5]
LAB_P = ['GAIN', 'BASS', 'MIDDLE', 'TREBLE', 'PRESENCE', 'DEPTH', 'MASTER']
LAVA = [1.0, 0.3, 0.03]


def phlegethon_spec():
    kn = [dict(x=x, z=KZ, r=0.44, v=v, shape='skirt2', mat='obsidian', mat2='blackchrome', ptr=[1.0, 0.36, 0.05]) for x, v in zip(KX_P, V_P)]
    return dict(size=[24, 10, 10.5], border=0.7, recess=1.0, bevel=0.2, rimcolor=[1.0, 0.55, 0.3],
                body=dict(color=[0.02, 0.018, 0.017], rough=0.95, bump=['noise', 14, 0.8]),
                handle=dict(w=8.5, h=1.2, color=[0.014, 0.013, 0.012]), world=[0.012, 0.008, 0.006],
                glow_strips=[dict(z=4.25, color=LAVA, strength=4.5, w=22.54)],
                zones=[dict(type='relief', z=[4.25, 9.8], height='art/phlegethon_h.png', holes='art/phlegethon_holes.png', depth=0.42, res=84,
                            color=[0.085, 0.076, 0.07], low=[0.014, 0.01, 0.009], lo_at=0.05, hi_at=0.7, metal=0.0, rough=0.9,
                            grain=[90, 0.04], emit=[[1.0, 0.14, 0.005], 3.2, 0.28], emit_pow=2.2,
                            gloss=[0.86, [0.006, 0.006, 0.008], 0.08, 0.0],
                            glass='art/phlegethon_glow.png', glass_strength=2.4, glass_y=0.3,
                            glass_lights=[dict(x=x, color=LAVA, energy=12, size=7) for x in (-7, 0, 7)]),
                       dict(type='panel', z=[0.7, 4.25], art='art/phlegethon_panel.png', artemis=0.25, color=[0.03, 0.027, 0.025], metal=0,
                            rough=0.95, bump=['noise', 30, 0.5], knobs=kn, jacks=[[-10.3, KZ, 'jack']],
                            jewels=[dict(x=9.75, z=KZ, color=LAVA, r=0.3, strength=4, id='power')])])


def phlegethon_panel(spec):
    kn = [dict(x=x, z=KZ, r=0.44, shape='skirt2', label=l) for x, l in zip(KX_P, LAB_P)]
    def extra(d, P, col):
        cx, cy = P(9.75, KZ - 0.62)
        d.text((cx, cy), 'POWER', font=saira(24, 700, 110), fill=col, anchor='mm')
        cx, cy = P(-10.3, KZ - 0.55)
        d.text((cx, cy), 'INPUT', font=saira(24, 700, 110), fill=col, anchor='mm')
    old = panel_art.font
    panel_art.font = lambda k, s, wght=None: saira(s, 800, 118) if k == 'sairaw' else old(k, s, wght)
    try:
        panel(str(ART / 'phlegethon_panel.png'), spec, (0.7, 4.25), kn, face='sairaw', col=(255, 150, 60, 255), size=26,
              dial='lines', tick_len=0.1, spacing=5, lines=[(-10.9, 0.98, 10.9, 0.98, 2)], extra=extra)
    finally:
        panel_art.font = old


if __name__ == '__main__':
    which = sys.argv[1:] or ['nemean', 'phlegethon']
    if 'nemean' in which:
        nemean()
        for k, s in nemean_specs().items():
            (HERE / (k + '.json')).write_text(json.dumps(s, indent=1))
        nemean_panel(s)
    if 'phlegethon' in which:
        phlegethon()
        s = phlegethon_spec()
        (HERE / 'phlegethon.json').write_text(json.dumps(s, indent=1))
        phlegethon_panel(s)
    print('art and specs written')
