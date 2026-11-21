"""Portcullis: dressed-stone gatehouse front with a round arch of voussoirs and a keystone; the
arch is open so the iron gate and the tubes behind it show through."""
import math, sys, numpy as np
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import relief_art as ra
ra.PX = 120
from relief_art import Relief
from scipy import ndimage as ndi

W, H = 22.56, 5.55
r = Relief(W, H, base=0.1)
rng = np.random.default_rng(4)
R_in, R_out, ys = 3.2, 3.95, -1.6           # arch: inner/outer radius, springing height
rad = np.sqrt(r.X ** 2 + (r.Y - ys) ** 2)
opening = ((rad <= R_in) & (r.Y >= ys)) | ((np.abs(r.X) <= R_in) & (r.Y < ys))
ring = ((rad > R_in) & (rad <= R_out) & (r.Y >= ys))
jambs = (np.abs(r.X) > R_in) & (np.abs(r.X) <= R_out) & (r.Y < ys)
keep_out = ndi.binary_dilation(opening | ring | jambs, iterations=int(0.04 * ra.PX))

chisel = ndi.gaussian_filter(rng.standard_normal(r.H.shape), 2.2) * 0.9 + ndi.gaussian_filter(rng.standard_normal(r.H.shape), 7) * 1.4


def block(m, top=0.62):
    if not m.any():
        return
    lay = r.dome(m, top + rng.uniform(-0.05, 0.05), 0.12, 0.45, reach=0.14)
    lay = np.where(m, lay + 0.035 * chisel * (lay > 0.3), 0)
    r.put(lay, m)


# coursed ashlar walling around the gate
rows = 6; rh = H / rows; gap = 0.07
for j in range(rows):
    y0 = -H / 2 + j * rh; x = -W / 2 - rng.uniform(0, 1.2)
    while x < W / 2:
        bw = rng.uniform(1.3, 2.3)
        m = (r.X > x + gap / 2) & (r.X < x + bw - gap / 2) & (r.Y > y0 + gap / 2) & (r.Y < y0 + rh - gap / 2) & ~keep_out
        block(m); x += bw

# voussoirs (radial joints) and keystone
n = 15
ang = np.arctan2(r.Y - ys, r.X)
for i in range(n):
    a0, a1 = math.pi * i / n, math.pi * (i + 1) / n
    m = ring & (ang >= a0 + 0.012) & (ang <= a1 - 0.012) & (rad > R_in + 0.03) & (rad < R_out - 0.03)
    if i == n // 2:
        m = (rad > R_in - 0.12) & (rad < R_out + 0.3) & (ang >= a0 - 0.02) & (ang <= a1 + 0.02) & (r.Y >= ys)
        block(m, 0.9)
    else:
        block(m, 0.72)
# jamb stones up the sides of the opening
for sx in (1, -1):
    y = -H / 2
    k = 0
    while y < ys - 0.05:
        hgt = 0.7 if k % 2 == 0 else 0.55
        wide = R_out + (0.35 if k % 2 == 0 else 0.0)
        m = (np.abs(r.X) > R_in + 0.03) & (np.abs(r.X) < wide) & (np.sign(r.X) == sx) & (r.Y > y + 0.035) & (r.Y < min(y + hgt, ys) - 0.035)
        block(m, 0.72); y += hgt; k += 1
    # impost blocks at the springing line
    m = (np.abs(r.X) > R_in - 0.25) & (np.abs(r.X) < R_out + 0.45) & (np.sign(r.X) == sx) & (r.Y > ys - 0.3) & (r.Y < ys)
    block(m, 0.82)

r.holes = opening
out = sys.argv[1] if len(sys.argv) > 1 else 'art/portcullis'
r.save(out + '_h.png', out + '_holes.png', blur=0.7)
print('saved', out)
