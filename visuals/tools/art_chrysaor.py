"""Chrysaor (JCM800 2203 captures): black lacquer front with gold baroque work, winged sword in a laurel wreath;
gold Marshall-style control panel.  python3 -I tools/art_chrysaor.py"""
import math, sys, json, numpy as np
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import relief_art as ra
ra.PX = 140
from relief_art import Relief
from panel_art import panel, font, spaced

W, H = 22.56, 5.55
r = Relief(W, H, base=0.1)
r.frame(fw=0.34, top=0.62, bead_r=0.045, bead_step=0.15)

# ---- centre: laurel wreath around a winged sword ----
R = 1.72
for sx in (1, -1):
    def pt(th):   # th in degrees on the right-hand arc; mirrored for the left
        t = math.radians(th if sx > 0 else 180 - th)
        return math.cos(t) * R, math.sin(t) * R * 1.02
    arc = [pt(th) for th in np.linspace(-95, 62, 120)]
    m = r.stroke(arc, 0.07); r.put(r.dome(m, 0.6, 0.15, 0.5, reach=0.035), m)
    for th in np.linspace(-82, 55, 11):    # pairs of laurel leaves pointing up the stem
        x, y = pt(th); x2, y2 = pt(th + 4)
        up = math.atan2(y2 - y, x2 - x)
        r.leaf(x, y, up + 0.55 * sx * (1 if sx > 0 else -1) * 0 + 0.5, 0.5, 0.26, 0.62, 0.14)
        r.leaf(x, y, up - 0.5, 0.44, 0.24, 0.58, 0.14)
# ribbon knot at the bottom of the wreath
r.beads([(0, -R * 1.02)], 0.13, 0.75)
for sx in (1, -1):
    m = r.stroke([(0, -R * 1.02), (sx * 0.35, -R * 1.02 - 0.28), (sx * 0.55, -R * 1.02 - 0.2)], 0.09, taper=(0.1, 0.04)); r.put(r.dome(m, 0.66, 0.15, 0.5), m)
# sword: blade pointing down, crossguard, grip and pommel
blade = r.poly([(-0.13, 0.55), (0.13, 0.55), (0.1, -1.05), (0, -1.38), (-0.1, -1.05)]); r.put(r.dome(blade, 0.98, 0.35, 0.45, reach=0.1), blade)
fuller = r.stroke([(0, 0.45), (0, -0.95)], 0.035); r.groove(fuller, 0.12, 1.0)
guard = r.stroke([(-0.62, 0.62), (-0.3, 0.55), (0.3, 0.55), (0.62, 0.62)], 0.11, taper=(0.13, 0.08)); r.put(r.dome(guard, 1.0, 0.4, 0.5), guard)
r.beads([(-0.66, 0.64), (0.66, 0.64)], 0.07, 1.0)
grip = r.stroke([(0, 0.62), (0, 1.02)], 0.1); r.put(r.dome(grip, 0.92, 0.4, 0.5), grip)
for y in np.arange(0.68, 1.0, 0.07): r.groove(r.stroke([(-0.05, y), (0.05, y + 0.02)], 0.015), 0.05, 1.0)
r.beads([(0, 1.12)], 0.11, 1.0)
# wings off the crossguard: feathers fanning up and out
for sx in (1, -1):
    for k in range(8):
        ang = math.radians(8 + k * 9); L = 1.05 - k * 0.06
        x0, y0 = sx * (0.3 + k * 0.012), 0.7 + k * 0.02
        r.leaf(x0, y0, math.atan2(math.sin(ang), sx * math.cos(ang)), L, 0.15, 0.82 - k * 0.015, 0.22)
# crown above the wreath
crown = r.poly([(-0.42, R + 0.1), (0.42, R + 0.1), (0.5, R + 0.42), (0.25, R + 0.27), (0, R + 0.5), (-0.25, R + 0.27), (-0.5, R + 0.42)])
r.put(r.dome(crown, 0.85, 0.25, 0.5), crown); r.beads([(-0.5, R + 0.46), (0, R + 0.55), (0.5, R + 0.46)], 0.05, 0.9)

# ---- flanks: scrolls, acanthus, rosettes, stars and shells (same family as Melinoe / Hyperion) ----
for sx in (1, -1):
    for sy in (1, -1):
        r.sscroll((sx * 2.35, sy * 0.25), (sx * 3.2, sy * 0.05), (sx * 3.75, sy * 1.55), (sx * 4.85, sy * 0.95), 0.6, 1.2, -sx * sy, width=(0.2, 0.05), top=0.64)
        r.sscroll((sx * 5.9, sy * 0.15), (sx * 6.75, sy * 0.0), (sx * 7.3, sy * 1.6), (sx * 8.45, sy * 0.95), 0.58, 1.2, -sx * sy, width=(0.18, 0.045), top=0.62)
        r.leaf(sx * 3.0, sy * 0.5, math.atan2(sy * 0.8, sx), 0.85, 0.3, 0.58, 0.12)
        r.leaf(sx * 6.55, sy * 0.45, math.atan2(sy * 0.9, sx), 0.78, 0.3, 0.56, 0.12)
        r.star(sx * 4.0, sy * 1.95, 0.15, 0.62)
        r.star(sx * 7.55, sy * 1.95, 0.13, 0.62)
    r.rose(sx * 5.4, 0.0, 0.4, layers=4, base=0.2, top=0.8, seed=21 + sx)
    r.shell(sx * 9.6, -0.5, 0.6, up=1, ribs=11, top=0.78)
    r.rose(sx * 9.6, 1.05, 0.34, layers=4, base=0.2, top=0.78, seed=31 + sx)
r.save('art/chrysaor_h.png', blur=0.9)
print('art ok')

# ---------------- spec ----------------
GOLD = [0.86, 0.62, 0.26]; KZ = 2.2975
KX = [-7.6, -5.1, -2.6, -0.1, 2.4, 4.9]; V = [0.5, 0.5, 0.6, 0.55, 0.5, 0.7]
kn = [dict(x=x, z=KZ, r=0.44, v=v, shape='cap', mat='bakelite', mat2='gold', ptr='white') for x, v in zip(KX, V)]
spec = dict(size=[24, 10, 10.5], border=0.7, recess=1.0, bevel=0.2, rimcolor=[1.0, 0.75, 0.4],
            body=dict(color=[0.012, 0.011, 0.011], rough=0.6, bump=['leather', 42, 0.55]), handle=dict(w=8.5, h=1.2),
            glow_strips=[dict(z=4.25, color=[1.0, 0.72, 0.32], strength=3, w=22.54)],
            zones=[dict(type='relief', z=[4.25, 9.8], height='art/chrysaor_h.png', depth=0.4, res=56, color=GOLD, low=[0.008, 0.007, 0.006],
                        lo_at=0.16, hi_at=0.34, metal=0.25, metal_hi=1.0, rough=0.26, grain=[160, 0.03],
                        graze=[dict(color=[1.0, 0.8, 0.5], energy=55, tilt=-25, out=1.0, top=0.4)]),
                   dict(type='panel', z=[0.7, 4.25], art='art/chrysaor_panel.png', artemis=0.0, color=[0.78, 0.56, 0.22], metal=0.95, rough=0.33,
                        bump=['brush', 70, 0.03], knobs=kn, jacks=[[7.4, KZ, 'jack'], [8.5, KZ, 'jack']],
                        jewels=[dict(x=-9.9, z=KZ, color=[1.0, 0.12, 0.04], r=0.3, strength=30, id='power')])])
json.dump(spec, open('amps/chrysaor.json', 'w'), indent=1)
INK = (16, 12, 8, 255)
def extra(d, P, col):
    f = font('cinzel', 15)
    spaced(d, P(7.4, KZ - 0.55), 'HIGH', f, col, 2); spaced(d, P(8.5, KZ - 0.55), 'LOW', f, col, 2)
    spaced(d, P(7.95, KZ + 0.6), 'INPUTS', f, col, 3)
panel('art/chrysaor_panel.png', spec, (0.7, 4.25), [dict(k, label=l) for k, l in zip(kn, ['PRESENCE', 'BASS', 'MIDDLE', 'TREBLE', 'MASTER', 'GAIN'])],
      jewels=[(-9.9, KZ - 0.1, 'POWER')], face='cinzel', col=INK, size=22, dial='numbers', numface='cinzel', numsize=14, extra=extra,
      lines=[(-10.8, 0.98, 10.8, 0.98, 3), (-10.8, 3.95, 10.8, 3.95, 3)])
print('spec ok')
