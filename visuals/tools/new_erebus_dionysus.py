"""Two new heads.
Erebus   : Bogner Uberschall MKII Ultra. Nyx's premium big brother: gloss black-chrome, chrome corners, pointed hex-slot grille
           with violet/ice tubes, a floating black-chrome EREBUS badge, violet light line, chrome lip, glowing-pointer knobs.
Dionysus : Bogner Ecstasy 101B. Wine-red tolex, black grille cut as a grapevine (glowing grape clusters, leaves and tendrils),
           oxblood/gold knobs, green/blue/red channel buttons like the real Ecstasy, a voice button."""
import json, math, sys, random
sys.path.insert(0, 'tools')
from PIL import Image, ImageDraw
from panel_art import panel, font, spaced

PXU = 100
IW = 24 - 2 * 0.5 - 0.04          # perf plate width used by head.py (IW-0.04)
ZA, ZB = 3.45, 9.5
MW, MH = int(round(IW * PXU)), int(round((ZB - ZA) * PXU))
def P(x, z): return ((x + IW / 2) * PXU, (ZB - z) * PXU)

# ================= Erebus =================
m = Image.new('RGBA', (MW, MH), (0, 0, 0, 0)); d = ImageDraw.Draw(m)
L, Hs, GX, PZ = 1.5, 0.3, 0.32, 0.56
badge = (-2.75, 2.75, 7.95, 9.2)
row = 0; z = ZA + 0.42
while z < ZB - 0.35:
    off = (L + GX) / 2 if row % 2 else 0
    x = -IW / 2 + 0.35 + off + L / 2 - (L + GX)
    while x < IW / 2:
        x0, x1 = x - L / 2, x + L / 2
        if x0 > -IW / 2 + 0.3 and x1 < IW / 2 - 0.3 and not (x1 > badge[0] and x0 < badge[1] and z + Hs > badge[2] and z - Hs < badge[3]):
            t = Hs / 2
            pts = [(x0, z), (x0 + t, z + Hs / 2), (x1 - t, z + Hs / 2), (x1, z), (x1 - t, z - Hs / 2), (x0 + t, z - Hs / 2)]
            d.polygon([P(*p) for p in pts], fill=(255, 255, 255, 255))
        x += L + GX
    z += PZ; row += 1
m.save('art/erebus_uu_mask.png')

bw, bh = 1100, 240
b = Image.new('RGBA', (bw, bh), (0, 0, 0, 0)); bd = ImageDraw.Draw(b)
SIL = (232, 234, 242, 255)
spaced(bd, (bw / 2, 112), 'EREBUS', font('cdec', 150), SIL, 26)
bd.line([(130, 206), (bw - 130, 206)], fill=SIL, width=4)
for sx in (-1, 1):
    cx = bw / 2 + sx * 500; bd.polygon([(cx, 98), (cx + 14, 112), (cx, 126), (cx - 14, 112)], fill=(170, 120, 255, 255))
b.save('art/erebus_uu_badge.png')

VI, ICE = [0.5, 0.25, 1.0], [0.25, 0.6, 1.0]
tubes = [dict(x=x, h=2.4, r=0.38, color=(VI if i % 2 == 0 else ICE), strength=10, light=70) for i, x in enumerate((-8.4, -5.6, -2.8, 0, 2.8, 5.6, 8.4))]
KX = [-8.7 + i * 2.65 for i in range(7)]; KZ = 1.75
V = [0.72, 0.5, 0.45, 0.55, 0.5, 0.55, 0.45]
knobs = [dict(x=round(x, 3), z=KZ, r=0.38, v=v, shape='cap', mat='blackchrome', mat2='silver', ptr=[0.6, 0.35, 1.0]) for x, v in zip(KX, V)]
spec = dict(size=[24, 10, 10], border=0.5, recess=1.6, bevel=0.2, rimcolor=[0.7, 0.55, 1.0], world=[0.01, 0.008, 0.016],
            body=dict(color=[0.008, 0.008, 0.011], metal=0.85, rough=0.24, bump=['noise', 260, 0.03]),
            corners='chrome', chrome_size=1.3,
            glow_strips=[dict(z=ZA, color=[0.55, 0.3, 1.0], strength=5, w=22.9)],
            zones=[dict(type='perf', z=[ZA, ZB], mask='art/erebus_uu_mask.png', color=[0.012, 0.012, 0.016], metal=0.92, rough=0.22, tubes=tubes,
                        ambient=[dict(x=x, energy=60, color=c, size=6) for x, c in ((-6, ICE), (0, VI), (6, ICE))],
                        logos=[dict(img='art/erebus_uu_badge.png', x=0, z=8.57, w=5.0, h=1.09, plate=[0.02, 0.02, 0.024], pm=1.0, pr=0.12, metal=0.95, rough=0.15, emis=0.08)]),
                   dict(type='panel', z=[0.5, ZA], color=[0.016, 0.016, 0.019], metal=0.8, rough=0.34, bump=['brush', 60, 0.03],
                        art='art/erebus_uu_panel.png', artemis=0.25, knobs=knobs,
                        rails=[dict(z=0.66, r=0.04, dy=-0.03, mat='silver', len=22.2)],
                        jewels=[dict(x=10.0, z=KZ, color=[0.6, 0.3, 1.0], r=0.28, id='power')])])
json.dump(spec, open('amps/erebus_uu.json', 'w'), indent=1)
panel('art/erebus_uu_panel.png', spec, (0.5, ZA), [dict(k, label=l) for k, l in zip(knobs, ['GAIN', 'BASS', 'MIDDLE', 'TREBLE', 'PRESENCE', 'DEPTH', 'MASTER'])],
      jewels=[(10.0, KZ - 0.1, 'POWER')], face='mich', col=(205, 208, 222, 255), size=17, dial='dots', spacing=4)
print('ok erebus')

# ================= Dionysus =================
random.seed(7)
m = Image.new('RGBA', (MW, MH), (0, 0, 0, 0)); d = ImageDraw.Draw(m)
W_ = (255, 255, 255, 255)
def vine_z(x): return 6.85 + 0.6 * math.sin(x * 0.62)
pts = [(x / 20, vine_z(x / 20)) for x in range(int(-IW / 2 * 20) + 8, int(IW / 2 * 20) - 7)]
d.line([P(*p) for p in pts], fill=W_, width=14, joint='curve')
def leaf(cx, cz, ang, s):   # almond leaf with a pointed tip, base at (cx, cz), pointing along ang
    poly = []
    for i in range(61):
        t = i / 60
        w = s * 0.36 * math.sin(math.pi * t) ** 0.8 * (1 - 0.35 * t)
        poly.append((t * s, w))
    poly += [(x, -w) for x, w in reversed(poly)]
    ca, sa = math.cos(ang), math.sin(ang)
    d.polygon([P(cx + x * ca - z * sa, cz + x * sa + z * ca) for x, z in poly], fill=W_)
def grapes(cx, cz, s):
    rows = [5, 4, 4, 3, 2, 1]; rr = 0.13 * s
    for ri, n in enumerate(rows):
        for k in range(n):
            gx = cx + (k - (n - 1) / 2) * rr * 2.15; gz = cz - ri * rr * 1.85
            a, bb = P(gx - rr, gz + rr), P(gx + rr, gz - rr); d.ellipse([a, bb], fill=W_)
def tendril(x0, z0, dirn):
    p = []
    for i in range(60):
        t = i / 59; a = t * 3.6 * math.pi; r = 0.42 * (1 - t) + 0.05
        p.append(P(x0 + dirn * (t * 0.5 + r * math.sin(a) * 0.5), z0 + 0.25 + r * math.cos(a) * 0.5 - 0.25))
    d.line(p, fill=W_, width=6, joint='curve')
xs = [-9.6, -6.6, -3.6, -0.6, 2.4, 5.4, 8.4]
for i, x in enumerate(xs):
    vz = vine_z(x)
    if i % 2 == 0:
        grapes(x, vz - 0.5, 1.05)
        leaf(x + 0.6, vine_z(x + 0.6) + 0.04, math.radians(50), 1.0); leaf(x + 1.5, vine_z(x + 1.5) - 0.04, math.radians(-35), 0.75)
        tendril(x - 0.7, vz + 0.35, -1)
    else:
        grapes(x, vz - 0.5, 0.95)
        leaf(x - 0.6, vine_z(x - 0.6) + 0.04, math.radians(130), 1.0); leaf(x - 1.5, vine_z(x - 1.5) - 0.04, math.radians(215), 0.75)
        tendril(x + 0.75, vz + 0.3, 1)
    d.line([P(x, vz), P(x, vz - 0.4)], fill=W_, width=8)
for x in (-11.0, 11.0): pass
# thin border slots, top and bottom
for zz in (ZA + 0.35, ZB - 0.35):
    d.rounded_rectangle([P(-IW / 2 + 0.5, zz + 0.05), P(IW / 2 - 0.5, zz - 0.05)], radius=5, fill=W_)
m.save('art/dionysus_mask.png')

KX = [-8.9 + i * 2.2 for i in range(7)]; KZ = 1.75
V = [0.65, 0.5, 0.5, 0.55, 0.5, 0.5, 0.45]
knobs = [dict(x=round(x, 3), z=KZ, r=0.38, v=v, shape='skirt2', mat='oxblood', mat2='gold', ptr='gold') for x, v in zip(KX, V)]
BX = [5.75, 6.6, 7.45]; VX = 8.55; PJ = 10.0
CHC = [[0.2, 1.0, 0.35], [0.25, 0.5, 1.0], [1.0, 0.12, 0.08]]
cap = [0.04, 0.012, 0.016]
spec = dict(size=[24, 10, 10], border=0.5, recess=1.6, bevel=0.22, rimcolor=[1.0, 0.6, 0.6], world=[0.012, 0.006, 0.008],
            body=dict(color=[0.075, 0.008, 0.016], rough=0.6, bump=['leather', 42, 0.5]),
            handle=dict(w=8.5, h=1.2, color=[0.05, 0.006, 0.01]),
            glow_strips=[dict(z=ZA, color=[1.0, 0.3, 0.35], strength=4, w=22.9)],
            zones=[dict(type='perf', z=[ZA, ZB], mask='art/dionysus_mask.png', color=[0.014, 0.011, 0.011], metal=0.55, rough=0.36, thick=0.1,
                        backglow=dict(scale=0.5, strength=1.5, colors=[[0.1, 0.0, 0.025], [0.5, 0.015, 0.09], [0.95, 0.12, 0.2]])),
                   dict(type='panel', z=[0.5, ZA], color=[0.015, 0.012, 0.012], metal=0.5, rough=0.4, bump=['brush', 60, 0.025],
                        art='art/dionysus_panel.png', artemis=0.2, knobs=knobs,
                        rails=[dict(z=0.66, r=0.04, dy=-0.03, mat='gold', len=22.2)],
                        buttons=[dict(id=f'ch{i+1}', x=x, z=KZ, w=0.5, h=0.36, cap=cap, metal=0.6, led=c, ledr=0.055) for i, (x, c) in enumerate(zip(BX, CHC))] +
                                [dict(id='voice', x=VX, z=KZ, w=0.5, h=0.36, cap=cap, metal=0.6)],
                        leds=[dict(id=f'vo{i+1}', x=VX + (i - 1) * 0.2, z=KZ + 0.42, w=0.08, h=0.055, color=[1.0, 0.7, 0.3], st=12) for i in range(3)],
                        jewels=[dict(x=PJ, z=KZ, color=[1.0, 0.25, 0.3], r=0.28, id='power')])])
json.dump(spec, open('amps/dionysus.json', 'w'), indent=1)
GC = (226, 186, 112, 255)
def dex(d, Pp, c):
    fh = font('cinzel', 20)
    y = Pp(0, KZ + 0.98)[1]
    spaced(d, (Pp(BX[1], 0)[0], y), 'CHANNEL', fh, c, 4)
    spaced(d, (Pp(VX, 0)[0], y), 'VOICE', fh, c, 4)
    fs = font('cinzel', 12)
    for i, t in enumerate(('I', 'II', 'III')): spaced(d, Pp(VX + (i - 1) * 0.2, KZ + 0.6), t, fs, c, 0)
panel('art/dionysus_panel.png', spec, (0.5, ZA), [dict(k, label=l) for k, l in zip(knobs, ['GAIN', 'BASS', 'MIDDLE', 'TREBLE', 'PRESENCE', 'DEPTH', 'MASTER'])],
      toggles=[(BX[0], KZ, 'GREEN'), (BX[1], KZ, 'BLUE'), (BX[2], KZ, 'RED'), (VX, KZ, '')], jewels=[(PJ, KZ - 0.1, 'POWER')],
      face='cinzel', col=GC, size=20, dial='numbers', numsize=13, spacing=3, extra=dex)
print('ok dionysus')
