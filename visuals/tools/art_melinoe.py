"""Melinoe: black lacquer front with silver baroque scrollwork and a moon cartouche."""
import math, sys, numpy as np
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import relief_art as ra
ra.PX = 140
from relief_art import Relief

W, H = 22.56, 5.55
r = Relief(W, H, base=0.12)
r.frame(fw=0.34, top=0.6, bead_r=0.045, bead_step=0.15)

for sx in (1, -1):
    for sy in (1, -1):
        # inner pair rolls out from the cartouche
        pts = r.sscroll((sx * 1.95, sy * 0.35), (sx * 2.9, sy * 0.1), (sx * 3.6, sy * 1.65), (sx * 4.75, sy * 1.0), 0.62, 1.15, -sx * sy,
                        width=(0.2, 0.05), top=0.62)
        # outer pair
        r.sscroll((sx * 6.0, sy * 0.15), (sx * 6.8, sy * 0.0), (sx * 7.4, sy * 1.7), (sx * 8.55, sy * 1.0), 0.6, 1.15, -sx * sy,
                  width=(0.18, 0.045), top=0.6)
        # small back-curl from each outer spiral toward the corner
        r.sscroll((sx * 9.2, sy * 0.2), (sx * 9.7, sy * 0.15), (sx * 10.2, sy * 0.9), (sx * 9.85, sy * 1.55), 0.32, 1.0, sx * sy,
                  width=(0.12, 0.035), top=0.55)
        # acanthus leaves sprouting where the curves leave the cartouche / meet
        r.leaf(sx * 2.65, sy * 0.55, math.atan2(sy * 0.8, sx * 1), 0.85, 0.3, 0.55, 0.12)
        r.leaf(sx * 6.55, sy * 0.5, math.atan2(sy * 0.9, sx * 1), 0.75, 0.3, 0.52, 0.12)
        r.star(sx * 3.4, sy * 1.85, 0.16, 0.6)
        r.star(sx * 7.3, sy * 1.85, 0.14, 0.6)
    # linking rosette between the two pairs and a drop at the far end
    r.rose(sx * 5.45, 0.0, 0.42, layers=4, base=0.2, top=0.78, seed=5 + sx)
    r.rose(sx * 9.6, 0.0, 0.3, layers=3, base=0.2, top=0.72, seed=9 + sx)

# cartouche: beaded oval, crescent moon and stars, shell crests
r.oval_ring(0, 0, 1.75, 1.98, 0.17, 0.72, beads=52, bead_r=0.04)
r.oval_ring(0, 0, 1.38, 1.58, 0.06, 0.55)
r.crescent(-0.12, 0.05, 0.95, off=0.45, ang=0.35, top=0.92)
for (x, y, s) in ((0.55, 0.55, 0.16), (0.75, -0.25, 0.11), (0.25, -0.75, 0.09), (0.95, 0.2, 0.07)):
    r.star(x, y, s, 0.75)
r.shell(0, 1.6, 0.48, up=1, ribs=11, top=0.8)
r.shell(0, -1.6, 0.48, up=-1, ribs=11, top=0.8)

out = sys.argv[1] if len(sys.argv) > 1 else 'art/melinoe_h.png'
r.save(out, blur=0.9)
print('saved', out)
