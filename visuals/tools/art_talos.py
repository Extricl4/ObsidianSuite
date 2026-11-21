"""Talos (hot-rod diamond plate, Dual Rectifier capture): relief heightmap + holes, spec and panel art.
python3 -I tools/art_talos.py"""
import math, sys, json, numpy as np
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import relief_art as ra
ra.PX = 120
from relief_art import Relief
from scipy import ndimage as ndi
from panel_art import panel, font, spaced

W, H = 22.56, 5.55
r = Relief(W, H, base=0.12)

# --- diamond (tread) plate: raised lozenges alternating +/-45 deg on a checkerboard ---
P = 0.4; L, Wd = 0.19, 0.06
i = np.floor(r.X / P); j = np.floor(r.Y / P)
u = r.X - (i + 0.5) * P; v = r.Y - (j + 0.5) * P
s = np.where(((i + j) % 2) == 0, 1.0, -1.0)
a = math.radians(45)
pu = (u * math.cos(a) + s * v * math.sin(a)); pv = (-u * math.sin(a) * s + v * math.cos(a))
q = np.abs(pu) / L + np.abs(pv) / Wd      # lozenge distance (1 = edge)
lz = np.clip(1 - q, 0, 1) ** 0.45
plate = 0.12 + 0.2 * lz
r.H = np.maximum(r.H, plate.astype(np.float32))

# --- border: rolled edge + rivets ---
edge = (np.abs(r.X) > W / 2 - 0.3) | (np.abs(r.Y) > H / 2 - 0.3)
r.H[edge] = 0.38
rv = []
for x in np.arange(-W / 2 + 0.55, W / 2 - 0.4, 1.1): rv += [(x, H / 2 - 0.48), (x, -H / 2 + 0.48)]
for y in (-1.3, 0, 1.3): rv += [(-W / 2 + 0.55, y), (W / 2 - 0.55, y)]
for (x, y) in rv:
    m = (r.X - x) ** 2 + (r.Y - y) ** 2 <= 0.085 ** 2; r.H[m] = 0.12; r.put(r.dome(m, 0.62, 0.25, 0.5), m)

# --- centre: recessed exhaust grille window with chrome bars; tubes glow behind ---
gw, gh = 9.2, 2.3
win = (np.abs(r.X) <= gw / 2) & (np.abs(r.Y + 0.1) <= gh / 2)
rr = 0.55   # rounded ends
cxs = gw / 2 - rr
win &= ~(((np.abs(r.X) > cxs) & (np.abs(r.Y + 0.1) > gh / 2 - rr)) & ((np.abs(r.X) - cxs) ** 2 + (np.abs(r.Y + 0.1) - (gh / 2 - rr)) ** 2 > rr ** 2))
rim = ndi.binary_dilation(win, iterations=int(0.22 * ra.PX)) & ~win
r.H[rim] = 0.0; r.put(r.dome(rim, 0.8, 0.2, 0.5), rim)
inner_ring = ndi.binary_dilation(win, iterations=int(0.06 * ra.PX)) & ~win
r.put(r.dome(inner_ring, 0.55, 0.1, 0.5), inner_ring)
bars = np.zeros_like(win)
for x in np.arange(-gw / 2 + 0.36, gw / 2 - 0.3, 0.36):
    b = (np.abs(r.X - x) <= 0.055) & win; bars |= b
hb = (np.abs(r.Y + 0.1) <= 0.05) & win; bars |= hb
r.H[win] = 0.02
r.put(r.dome(bars, 0.7, 0.15, 0.5, reach=0.05), bars)
r.holes = win & ~ndi.binary_dilation(bars, iterations=1)

# --- hot-rod flames licking out of both ends of the grille (one raised mass with a pinstripe edge) ---
def flame(sx):
    m = np.zeros_like(r.holes)
    licks = [(0.75, 2.3, 0.5, 0.0), (0.25, 3.3, 0.62, 1.3), (-0.3, 2.8, 0.56, 2.4), (-0.8, 2.0, 0.46, 3.5), (-1.15, 1.3, 0.34, 0.6)]
    for (y0, ln, wd, ph) in licks:
        x0 = sx * (gw / 2 - 0.25); pts = []
        for t in np.linspace(0, 1, 80):
            x = x0 + sx * ln * t
            y = y0 - 0.1 + 0.28 * math.sin(math.pi * 1.25 * t + ph) * t + 0.3 * t ** 2
            pts.append((x, y))
        m |= r.stroke(pts, wd, taper=(wd, 0.015))
    m &= ~ndi.binary_dilation(win, iterations=int(0.25 * ra.PX))
    r.H[m] = np.minimum(r.H[m], 0.13)
    r.put(r.dome(m, 0.5, 0.16, 0.6, reach=0.18), m)
    ring = m & ~ndi.binary_erosion(m, iterations=int(0.045 * ra.PX))
    r.put(r.dome(ring, 0.6, 0.3, 0.5, reach=0.02), ring)
flame(1); flame(-1)
r.save('art/talos_h.png', 'art/talos_holes.png', blur=0.6)
print('art ok')

# ---------- spec ----------
KX = [-8.5, -6.0, -3.5, -1.0, 1.5, 4.0, 6.5]; KZ = 2.2975; V = [0.65, 0.55, 0.35, 0.6, 0.5, 0.45, 0.5]
OR = [1.0, 0.32, 0.05]
knobs = [dict(x=x, z=KZ, r=0.42, v=v, shape='cap', mat='silver', mat2='pewter', ptr='black') for x, v in zip(KX, V)]
tubes = [dict(x=x, z=6.92, h=1.7, r=0.36, color=OR, strength=10, light=55) for x in (-3.0, -1.0, 1.0, 3.0)]
spec = dict(size=[24, 10, 10.5], border=0.7, recess=1.6, bevel=0.2, rimcolor=[1, 0.6, 0.35],
            body=dict(color=[0.014, 0.014, 0.016], rough=0.55, bump=['leather', 40, 0.55]), handle=dict(w=8.5, h=1.2, color=[0.02, 0.02, 0.022]),
            glow_strips=[dict(z=4.25, color=OR, strength=4, w=22.54)],
            zones=[dict(type='relief', z=[4.25, 9.8], height='art/talos_h.png', holes='art/talos_holes.png', depth=0.32, res=64,
                        color=[0.62, 0.62, 0.64], cavity=0.55, metal=1.0, rough=0.3, hi_at=0.45, grain=[260, 0.04], tubes=tubes,
                        ambient=[dict(x=x, energy=55, color=OR, size=3) for x in (-2.5, 2.5)]),
                   dict(type='panel', z=[0.7, 4.25], art='art/talos_panel.png', artemis=0.15, color=[0.008, 0.008, 0.009], metal=0.5, rough=0.32,
                        bump=['brush', 60, 0.02], knobs=knobs, jacks=[[-10.3, KZ, 'jack']],
                        buttons=[dict(id='boost', x=8.35, z=KZ, w=0.52, h=0.36, cap=[0.6, 0.6, 0.62], metal=1.0, rough=0.25, led=OR, ledr=0.06)],
                        jewels=[dict(x=9.6, z=KZ, color=OR, r=0.28, strength=28, id='power')])])
json.dump(spec, open('amps/talos.json', 'w'), indent=1)
LAB = ['GAIN', 'TREBLE', 'MIDDLE', 'BASS', 'PRESENCE', 'MASTER', 'OUTPUT']
panel('art/talos_panel.png', spec, (0.7, 4.25), [dict(k, label=l) for k, l in zip(knobs, LAB)], toggles=[(8.35, KZ, 'BOOST')],
      jewels=[(9.6, KZ - 0.1, 'POWER')], face='rocker', col=(236, 222, 196, 255), size=28, dial='numbers', numface='marcellus',
      lines=[(-11.3 + 0.5, 0.98, 11.3 - 0.5, 0.98, 2)])
print('spec ok')
