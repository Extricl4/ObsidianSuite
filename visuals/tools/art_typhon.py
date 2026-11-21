"""Typhon (5150 Stealth captures): matte black, 3D pyramid grid with glossy black criss-cross stripes; one skin per channel colour.
python3 -I tools/art_typhon.py"""
import math, sys, json, numpy as np
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import relief_art as ra
ra.PX = 120
from relief_art import Relief
from scipy import ndimage as ndi
from panel_art import panel, font, spaced

W, H = 22.56, 5.55
r = Relief(W, H, base=0.0)
# 3D pyramid grid
P = 0.34
dx = np.abs(r.X - np.round(r.X / P) * P); dy = np.abs(r.Y - np.round(r.Y / P) * P)
pyr = np.clip(1 - np.maximum(dx, dy) / (P / 2 * 0.92), 0, 1)
r.H = (0.08 + 0.47 * pyr).astype(np.float32)
# frame
fr = (np.abs(r.X) > W / 2 - 0.3) | (np.abs(r.Y) > H / 2 - 0.28)
r.H[fr] = 0.6
# criss-cross stripes, layered like tape: each band is a flat glossy plateau that sits over the grid
bands = [(-6.5, 0.4, 28, 0.42), (-2.0, -0.3, -35, 0.34), (3.4, 0.2, 52, 0.46), (7.6, -0.5, -22, 0.3), (0.6, 0.9, 12, 0.26),
         (-9.2, -0.6, 64, 0.28), (5.5, 1.1, -58, 0.24), (-4.2, 1.6, -8, 0.22)]
for i, (cx, cy, ang, wd) in enumerate(bands):
    a = math.radians(ang); d = np.abs(-(r.X - cx) * math.sin(a) + (r.Y - cy) * math.cos(a))
    m = (d <= wd / 2) & ~fr
    lvl = 0.86 + 0.02 * i
    r.H[m] = lvl
    edge = m & ~ndi.binary_erosion(m, iterations=3); r.H[edge] = lvl - 0.05
r.save('art/typhon_h.png', blur=0.5)
print('art ok')

KZ = 2.2975
CH = [('green', [0.2, 1.0, 0.35]), ('blue', [0.25, 0.55, 1.0]), ('red', [1.0, 0.1, 0.04])]
KX = [-8.5, -6.0, -3.5, -1.0, 1.5, 4.0, 6.5]; V = [0.75, 0.5, 0.45, 0.55, 0.5, 0.55, 0.5]
kn = [dict(x=x, z=KZ, r=0.42, v=v, shape='knurl', mat='bakelite', mat2='bakelite', ptr='white') for x, v in zip(KX, V)]
BX = [7.75, 8.55, 9.35]
for key, col in CH:
    spec = dict(size=[24, 10, 10.5], border=0.7, recess=1.0, bevel=0.2, rimcolor=[0.7, 0.7, 0.75],
                body=dict(color=[0.01, 0.01, 0.011], rough=0.85, bump=['noise', 300, 0.012]), handle=dict(w=8.5, h=1.2, color=[0.012, 0.012, 0.013]),
                glow_strips=[dict(z=4.25, color=col, strength=3.2, w=22.54)],
                zones=[dict(type='relief', z=[4.25, 9.8], height='art/typhon_h.png', depth=0.3, res=64, color=[0.04, 0.04, 0.045], low=[0.006, 0.006, 0.007],
                            lo_at=0.2, hi_at=0.86, metal=0.0, metal_hi=0.85, rough=0.62, grain=[300, 0.01],
                            graze=[dict(color=col, energy=40, tilt=-25, out=1.0, top=0.35)]),
                       dict(type='panel', z=[0.7, 4.25], art='art/typhon_panel.png', artemis=0.12, color=[0.009, 0.009, 0.01], metal=0.1, rough=0.8,
                            bump=['noise', 300, 0.01], knobs=kn, jacks=[[-10.3, KZ, 'jack']],
                            buttons=[dict(id=f'ch{i+1}', x=x, z=KZ, w=0.46, h=0.32, cap=[0.012, 0.012, 0.013], metal=0.2, rough=0.7, led=c, ledr=0.055)
                                     for i, (x, (_, c)) in enumerate(zip(BX, CH))],
                            jewels=[dict(x=10.3, z=KZ, color=col, r=0.24, strength=24, id='power')])])
    json.dump(spec, open(f'amps/typhon_{key}.json', 'w'), indent=1)
panel('art/typhon_panel.png', spec, (0.7, 4.25), [dict(k, label=l) for k, l in zip(kn, ['GAIN', 'VOLUME', 'LOW', 'MID', 'HIGH', 'PRESENCE', 'RESONANCE'])],
      toggles=[(x, KZ, n) for x, n in zip(BX, ('GREEN', 'BLUE', 'RED'))], jewels=[(10.3, KZ - 0.1, 'POWER')], face='mich', col=(210, 210, 214, 255), size=14,
      dial='lines', tick_len=0.1, spacing=3, lines=[(-10.9, 0.98, 10.9, 0.98, 2)])
print('spec ok')
