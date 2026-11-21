"""Dionysus v2: a sculpted bronze grapevine frieze instead of the flat stencil.
Pierced diamond trellis (tubes glow through it), mirrored vines growing out of a central beaded cartouche,
domed grape clusters, three-lobed veined leaves, curling tendrils, beaded classical frame."""
import math, sys, numpy as np
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import relief_art as ra
ra.PX = 110
from relief_art import Relief
from scipy import ndimage as ndi

W, H = 22.96, 6.05
r = Relief(W, H, base=0.1)
FONT = __file__.rsplit('/', 2)[0] + '/render/fonts/Cinzel.ttf'
IN = 0.62                                     # trellis starts this far inside the edge

# ---- trellis ----
s2 = math.sqrt(2); S = 0.5; BW = 0.055
u = (r.X + r.Y) / s2; v = (r.X - r.Y) / s2
inner = (np.abs(r.X) < W / 2 - IN) & (np.abs(r.Y) < H / 2 - IN)
bars = inner & ((np.abs((u / S) - np.round(u / S)) * S < BW) | (np.abs((v / S) - np.round(v / S)) * S < BW))
r.put(r.dome(bars, 0.26, 0.16, 0.5, reach=BW), bars)
nodes = np.zeros_like(bars)
nodes |= inner & ((np.abs((u / S) - np.round(u / S)) * S < BW * 1.7) & (np.abs((v / S) - np.round(v / S)) * S < BW * 1.7))
r.put(r.dome(nodes, 0.29, 0.2, 0.5), nodes)
H_TRELLIS = r.H.copy()

# ---- frame ----
r.frame(fw=0.3, top=0.6, beads=True, bead_r=0.05, bead_step=0.18)


def grapes(cx, cy, rad=0.13, rows=(4, 4, 3, 3, 2, 1)):
    for ri, n in enumerate(rows):
        for k in range(n):
            gx = cx + (k - (n - 1) / 2) * rad * 1.92 + (0.03 if ri % 2 else -0.03)
            gy = cy - ri * rad * 1.68
            m = (r.X - gx) ** 2 + (r.Y - gy) ** 2 <= rad ** 2
            r.put(r.dome(m, 0.86 - ri * 0.015, 0.42, 0.5), m)


def grape_leaf(bx, by, ang, s=1.0):
    for da, L in ((0, 0.92), (-48, 0.72), (48, 0.72), (-100, 0.46), (100, 0.46)):
        a = math.radians(ang + da)
        r.leaf(bx, by, a, L * s, Wd=0.36, top=0.66, base=0.34)


def tendril(x0, y0, sgn, size=0.36):
    th = np.linspace(0, 2.6 * math.pi, 120)
    pts = [(x0 + sgn * (size * (1 - t / (2.9 * math.pi)) * math.sin(t)), y0 + size * (1 - t / (2.9 * math.pi)) * (1 - math.cos(t)) * 0.8) for t in th]
    m = r.stroke(pts, 0.05, taper=(0.06, 0.025))
    r.put(r.dome(m, 0.58, 0.3, 0.5, reach=0.03), m)


def vine_y(t):          # t = distance from the cartouche
    return 0.22 + 0.62 * math.sin(t * 0.66 - 0.25)


for sg in (-1, 1):
    X0 = 2.45
    pts = [(sg * (X0 + t), vine_y(t)) for t in np.linspace(0, 8.95, 300)]
    m = r.stroke(pts, 0.2, taper=(0.24, 0.1))
    r.put(r.dome(m, 0.62, 0.3, 0.5, reach=0.12), m)
    # grape clusters hang under the vine
    for t in (1.55, 4.35, 7.2):
        x = sg * (X0 + t); y = vine_y(t)
        st = r.stroke([(x, y), (x + sg * 0.08, y - 0.28)], 0.05); r.put(r.dome(st, 0.55, 0.3, 0.5, reach=0.03), st)
        grapes(x + sg * 0.08, y - 0.42)
    # leaves above the vine
    for t, a in ((2.9, 62), (5.75, 78), (8.3, 60)):
        x = sg * (X0 + t); y = vine_y(t)
        lx, ly = x - sg * 0.25, y + 0.35
        st = r.stroke([(x, y), (lx, ly)], 0.05); r.put(r.dome(st, 0.55, 0.3, 0.5, reach=0.03), st)
        grape_leaf(lx, ly, 90 + sg * (90 - a) * -1, 0.95)
    for t in (0.75, 3.75, 6.55):
        x = sg * (X0 + t); y = vine_y(t)
        tendril(x, y + 0.1, sg)

# ---- cartouche ----
cx, cy, rx, ry = 0, 0.1, 2.3, 1.12
oval = ((r.X - cx) / rx) ** 2 + ((r.Y - cy) / ry) ** 2 <= 1
r.H[oval] = 0.32
r.oval_ring(cx, cy, rx, ry, 0.13, 0.72, base=0.32, beads=46, bead_r=0.045)
r.oval_ring(cx, cy, rx - 0.3, ry - 0.3, 0.035, 0.5, base=0.3)
r.glyph('DIONYSUS', FONT, cx, cy - 0.02, 0.56, top=0.78, base=0.3)
for sg in (-1, 1):                      # small grape at each side of the name
    m = (r.X - sg * 1.72) ** 2 + (r.Y - cy) ** 2 <= 0.07 ** 2
    r.put(r.dome(m, 0.7, 0.35, 0.5), m)

# ---- holes: trellis openings not covered by ornament or frame ----
orn = r.H > H_TRELLIS + 0.02
orn = ndi.binary_dilation(orn, iterations=2)
r.holes = inner & ~bars & ~nodes & ~orn & ~oval
r.save('art/dionysus_h.png', 'art/dionysus_holes.png')
print('saved', r.holes.mean())
