"""Persephone: matte white baroque front: beaded cartouche with shell crests, roses on thorny vines
that grow out of the cartouche and end in curled scrolls with acanthus leaves."""
import math, sys, numpy as np
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import relief_art as ra
ra.PX = 140
from relief_art import Relief

W, H = 22.56, 5.55
r = Relief(W, H, base=0.12)
r.frame(fw=0.36, top=0.6, bead_r=0.05, bead_step=0.17)

X0, X1 = 1.86, 9.0
def vy(x, ph=0.0, amp=0.95):
    t = (x - X0) / (X1 - X0)
    return amp * math.sin(t * 2.0 * math.pi + ph) * math.sin(min(t, 1) * math.pi / 2) ** 0.4

for sx in (1, -1):
    ends = []
    for ph, amp, wd in ((0.0, 1.05, 0.13), (math.pi, 0.9, 0.1)):
        xs = np.linspace(X0, X1, 520)
        pts = [(sx * x, vy(x, ph, amp)) for x in xs]
        r.vine(pts, width=wd, top=0.46, base=0.12, thorn_every=0.36, seed=int(7 + ph * 3 + sx))
        ends.append((pts[-1], wd, ph))
    # vine ends curl into scrolls toward the corners
    for (ex, ey), wd, ph in ends:
        sy = 1 if ph == 0 else -1
        r.scroll(ex + sx * 0.62, ey + sy * 0.95, 1.05, turns=1.2, sgn=sx * sy, ang0=(math.pi if sx > 0 else 0) + sy * sx * 0.9,
                 width=(wd, 0.04), top=0.46, lobes=False)
        r.leaf(ex - sx * 0.1, ey, math.atan2(sy * 0.9, sx * 0.2), 0.7, 0.34, 0.5, 0.14)
    for i, x in enumerate(np.linspace(2.7, 8.4, 7)):
        up = 1 if i % 2 == 0 else -1
        r.leaf(sx * x, vy(x, 0, 1.05), math.atan2(up, sx * 0.6), 0.68, 0.34, 0.5, 0.14)
    r.rose(sx * 5.25, 0.65, 0.9, layers=7, base=0.24, top=0.92, seed=11 + sx)
    r.rose(sx * 7.6, -0.6, 0.82, layers=7, base=0.24, top=0.9, seed=21 + sx)
    r.rose(sx * 3.55, -1.3, 0.4, layers=4, base=0.2, top=0.76, seed=31 + sx)
    r.rose(sx * 6.55, 1.55, 0.36, layers=4, base=0.2, top=0.74, seed=41 + sx)
    r.rose(sx * 9.05, 1.55, 0.3, layers=3, base=0.2, top=0.7, seed=51 + sx)
    r.rose(sx * 9.05, -1.55, 0.3, layers=3, base=0.2, top=0.7, seed=61 + sx)

# central beaded cartouche, shell crests breaking over its top and bottom, big rose inside
r.oval_ring(0, 0, 1.85, 1.98, 0.17, 0.72, beads=56, bead_r=0.042)
r.shell(0, 1.62, 0.5, up=1, ribs=11, top=0.8)
r.shell(0, -1.62, 0.5, up=-1, ribs=11, top=0.8)
r.rose(0, 0.0, 1.22, layers=8, base=0.28, top=1.0, seed=3)

out = sys.argv[1] if len(sys.argv) > 1 else 'art/persephone_h.png'
r.save(out, blur=0.9)
print('saved', out, r.H.shape)
