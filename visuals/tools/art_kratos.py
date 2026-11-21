"""Kratos (Peavey 6505+ captures): bead-blasted matte steel front with a heavy square grate, bolted frame and red tubes behind.
python3 -I tools/art_kratos.py"""
import math, sys, json, numpy as np
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import relief_art as ra
ra.PX = 120
from relief_art import Relief
from scipy import ndimage as ndi
from panel_art import panel, font, spaced

W, H = 22.56, 5.55
r = Relief(W, H, base=0.5)
# heavy outer frame with a chamfered inner edge
fr = (np.abs(r.X) > W / 2 - 0.55) | (np.abs(r.Y) > H / 2 - 0.5)
r.H[fr] = 0.75
inner = ~fr
ch = ndi.binary_dilation(fr, iterations=int(0.12 * ra.PX)) & inner
r.put(r.dome(ch, 0.75, 0.45, 0.5, reach=0.12), ch)
# grate: rounded-square holes on a 0.44 pitch, bars 0.1 wide, kept clear of the frame and the centre plate
P, S = 0.44, 0.17
gx = (np.round(r.X / P) * P); gy = (np.round(r.Y / P) * P)
dx, dy = np.abs(r.X - gx), np.abs(r.Y - gy)
rr = 0.05
sq = (np.maximum(dx, dy) <= S) & ~((dx > S - rr) & (dy > S - rr) & ((dx - (S - rr)) ** 2 + (dy - (S - rr)) ** 2 > rr ** 2))
plate = (np.abs(r.X) <= 1.9) & (np.abs(r.Y) <= 1.35)
field = inner & ~ndi.binary_dilation(fr, iterations=int(0.22 * ra.PX)) & ~ndi.binary_dilation(plate, iterations=int(0.2 * ra.PX))
cfit = (np.abs(gx) <= W / 2 - 0.55 - 0.2 - S) & (np.abs(gy) <= H / 2 - 0.5 - 0.18 - S) & ~((np.abs(gx) <= 1.9 + 0.18 + S) & (np.abs(gy) <= 1.35 + 0.18 + S))
holes = sq & cfit
# bevel round every hole
rim = ndi.binary_dilation(holes, iterations=int(0.04 * ra.PX)) & ~holes
r.H[rim] = 0.38
r.H[holes] = 0.0
# centre plate: raised, chamfered, with a stamped chevron and four bolts
r.H[plate] = 0.62
pe = plate & ~ndi.binary_erosion(plate, iterations=int(0.1 * ra.PX)); r.put(r.dome(pe, 0.8, 0.62, 0.5, reach=0.1), pe)
for dyv in (0.35, -0.25):
    cv = (np.abs(np.abs(r.X) * 0.75 + (r.Y - dyv)) <= 0.08) & (np.abs(r.X) <= 1.25) & (r.Y >= dyv - 1.0)
    r.groove(cv, 0.1, 1.0)
bolts = [(sx * 1.6, sy * 1.05) for sx in (1, -1) for sy in (1, -1)] + [(sx * (W / 2 - 0.3), sy * (H / 2 - 0.27)) for sx in (1, -1) for sy in (1, -1)] \
        + [(x, sy * (H / 2 - 0.27)) for x in (-7.5, -3.75, 3.75, 7.5) for sy in (1, -1)]
for (x, y) in bolts:
    m = ((r.X - x) ** 2 + (r.Y - y) ** 2) <= 0.13 ** 2; r.put(r.dome(m, 0.98, 0.7, 0.45), m)
    # hex socket
    hx = (np.maximum(np.abs(r.X - x) * 0.87 + np.abs(r.Y - y) * 0.5, np.abs(r.Y - y)) <= 0.05); r.groove(hx & m, 0.12, 1.0)
r.holes = holes
r.save('art/kratos_h.png', 'art/kratos_holes.png', blur=0.4)
print('art ok')

RD = [1.0, 0.1, 0.04]; GN = [0.25, 1.0, 0.35]; KZ = 2.2975
KX = [-8.6 + i * 1.9 for i in range(9)]
V = [0.4, 0.55, 0.72, 0.5, 0.55, 0.45, 0.6, 0.5, 0.55]
kn = [dict(x=x, z=KZ, r=0.36, v=v, shape='skirt2', mat='bakelite', mat2='silver', ptr='white') for x, v in zip(KX, V)]
tubes = [dict(x=x, z=7.0, h=2.3, r=0.36, color=[1.0, 0.22, 0.05], strength=10, light=55) for x in (-8.4, -6.0, -3.6, 3.6, 6.0, 8.4)]
spec = dict(size=[24, 10, 10.5], border=0.7, recess=1.4, bevel=0.2, rimcolor=[1, 0.35, 0.25],
            body=dict(color=[0.012, 0.012, 0.013], rough=0.6, bump=['leather', 38, 0.55]), handle=dict(w=8.5, h=1.2, color=[0.018, 0.018, 0.02]),
            glow_strips=[dict(z=4.25, color=RD, strength=3.2, w=22.54)],
            zones=[dict(type='relief', z=[4.25, 9.8], height='art/kratos_h.png', holes='art/kratos_holes.png', depth=0.42, res=60,
                        color=[0.36, 0.36, 0.38], cavity=0.3, metal=0.9, rough=0.62, hi_at=0.8, grain=[300, 0.06], tubes=tubes,
                        ambient=[dict(x=x, energy=70, color=[1.0, 0.18, 0.04], size=5) for x in (-6, 6)]),
                   dict(type='panel', z=[0.7, 4.25], art='art/kratos_panel.png', artemis=0.15, color=[0.03, 0.03, 0.032], metal=0.8, rough=0.55,
                        bump=['noise', 300, 0.03], knobs=kn, jacks=[[-10.45, KZ, 'jack']],
                        buttons=[dict(id='crunch', x=8.0, z=KZ, w=0.42, h=0.3, cap=[0.02, 0.02, 0.022], metal=0.4, led=RD, ledr=0.05),
                                 dict(id='bright', x=8.75, z=KZ, w=0.42, h=0.3, cap=[0.02, 0.02, 0.022], metal=0.4, led=RD, ledr=0.05),
                                 dict(id='channel', x=9.6, z=KZ, w=0.46, h=0.32, cap=[0.02, 0.02, 0.022], metal=0.4)],
                        leds=[dict(id='ch1', x=9.48, z=KZ + 0.42, w=0.08, h=0.055, color=GN, st=12), dict(id='ch2', x=9.72, z=KZ + 0.42, w=0.08, h=0.055, color=RD, st=12)],
                        jewels=[dict(x=10.5, z=KZ, color=RD, r=0.24, strength=26, id='power')])])
json.dump(spec, open('amps/kratos.json', 'w'), indent=1)
LC = (222, 222, 226, 255)
LAB = ['PRE', 'POST', 'PRE', 'POST', 'LOW', 'MID', 'HIGH', 'RESONANCE', 'PRESENCE']
def extra(d, P, col):
    f = font('mich', 14)
    y = P(0, 3.55)[1]
    for (a, b, t) in ((-9.1, -6.2, 'RHYTHM'), (-5.2, -2.3, 'LEAD')):
        x0, x1 = P(a, 0)[0], P(b, 0)[0]; d.line([(x0, y + 10), (x0, y), (x1, y), (x1, y + 10)], fill=col, width=2)
        spaced(d, ((x0 + x1) / 2, y - 16), t, f, col, 3)
    fs = font('mich', 9)
    spaced(d, P(9.48, KZ + 0.6), 'R', fs, col, 0); spaced(d, P(9.72, KZ + 0.6), 'L', fs, col, 0)
panel('art/kratos_panel.png', spec, (0.7, 4.25), [dict(k, label=l) for k, l in zip(kn, LAB)],
      toggles=[(8.0, KZ, 'CRUNCH'), (8.75, KZ, 'BRIGHT'), (9.6, KZ, 'CHANNEL')], jewels=[(10.5, KZ - 0.1, 'POWER')],
      face='mich', col=LC, size=13, dial='numbers', numface='mich', numsize=10, tick_len=0.08, spacing=2, extra=extra,
      lines=[(-10.9, 0.98, 10.9, 0.98, 2)])
print('spec ok')
