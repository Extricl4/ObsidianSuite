"""Lamia: gothic cathedral tracery, pierced so the tubes glow through the windows."""
import math, sys, numpy as np
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import relief_art as ra
ra.PX = 120
from relief_art import Relief, arch_pts
from scipy import ndimage as ndi

W, H = 22.56, 5.55
r = Relief(W, H, base=0.16)
r.frame(fw=0.3, top=0.55, beads=False)
bars = np.zeros_like(r.holes); openings = np.zeros_like(r.holes)
BAR = 0.7


def bar(m, top=BAR, w=0.1):
    r.put(r.dome(m, top, 0.16, 0.5, reach=w), m)


# ---- rose window ----
R = 2.2; cy = 0.05
disc = (r.X ** 2 + (r.Y - cy) ** 2) <= R * R
openings |= disc
ring = r.circle_ring(0, cy, R, 0.2, 0.8); bars |= ring
ring2 = r.circle_ring(0, cy, R * 0.36, 0.16, 0.8); bars |= ring2
hub = (r.X ** 2 + (r.Y - cy) ** 2) <= (R * 0.36) ** 2
r.put(r.dome(hub, 0.95, 0.5, 0.6), hub); bars |= hub
for i in range(12):
    a = i * math.pi / 6
    pts = [(math.cos(a) * rr, cy + math.sin(a) * rr) for rr in np.linspace(R * 0.36, R, 40)]
    m = r.stroke(pts, 0.1); bar(m, 0.72, 0.05); bars |= m
    a2 = a + math.pi / 12
    m = r.circle_ring(math.cos(a2) * R * 0.7, cy + math.sin(a2) * R * 0.7, R * 0.2, 0.07, 0.66); bars |= m

# ---- lancet windows ----
for sx in (1, -1):
    for cx in (4.35, 6.95, 9.55):
        cx *= sx; w = 1.8; y0 = -2.25; ys = 0.25
        outer, apex = arch_pts(cx, y0, w, ys)
        om = r.poly(outer)
        inner = ndi.binary_erosion(om, iterations=int(0.12 * ra.PX))
        openings |= inner
        edge = om & ~inner; bar(edge, 0.82, 0.08); bars |= edge
        hood, hapex = arch_pts(cx, ys - 0.2, w + 0.34, ys)
        hm = r.stroke(hood[1:-1], 0.08); bar(hm, 0.6, 0.04); bars |= hm & ~openings
        mull = r.stroke([(cx, y0), (cx, ys + 0.32)], 0.1); bar(mull, 0.75, 0.05); bars |= mull
        for s2 in (-1, 1):
            sub, _ = arch_pts(cx + s2 * w / 4.15, y0, w / 2 - 0.12, ys + 0.02)
            m = r.stroke(sub[1:-1], 0.075); bar(m, 0.7, 0.04); bars |= m
        tre = r.circle_ring(cx, ys + 0.86, 0.3, 0.075, 0.7); bars |= tre
        for k in range(3):
            a = math.pi / 2 + k * 2 * math.pi / 3
            m = r.circle_ring(cx + math.cos(a) * 0.12, ys + 0.86 + math.sin(a) * 0.12, 0.11, 0.05, 0.62); bars |= m
        # pinnacle with crockets
        pin = r.poly([(cx - 0.12, apex - 0.05), (cx + 0.12, apex - 0.05), (cx, apex + 0.5)])
        r.put(r.dome(pin, 0.85, 0.2, 0.5), pin)
        for t in (0.15, 0.3):
            for s2 in (-1, 1):
                b = (r.X - (cx + s2 * 0.1 * (1 - t * 2))) ** 2 + (r.Y - (apex + t)) ** 2 <= 0.05 ** 2
                r.put(r.dome(b, 0.75, 0.2, 0.5), b)
        fin = (r.X - cx) ** 2 + (r.Y - (apex + 0.52)) ** 2 <= 0.07 ** 2
        r.put(r.dome(fin, 0.85, 0.2, 0.5), fin)
    # quatrefoils between windows (solid)
    for qx in (5.65, 8.25):
        for k in range(4):
            a = k * math.pi / 2 + math.pi / 4
            r.circle_ring(sx * qx + math.cos(a) * 0.13, 1.85 + math.sin(a) * 0.13, 0.12, 0.05, 0.55)
    # sill moulding under the windows
    m = r.stroke([(sx * 3.3, -2.32), (sx * 10.6, -2.32)], 0.1); bar(m, 0.6, 0.05)
    m = r.stroke([(sx * 3.3, 0.25 - 0.2), (sx * 10.6, 0.25 - 0.2)], 0.0001)

r.holes = openings & ~ndi.binary_dilation(bars, iterations=1)
out = sys.argv[1] if len(sys.argv) > 1 else 'art/lamia'
r.save(out + '_h.png', out + '_holes.png')
print('saved', out)
