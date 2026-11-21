"""Lyssa (Invective-inspired) and Belial (Satan-inspired): relief art, spec, panel art.  python3 -I tools/art_new2.py"""
import math, sys, json, numpy as np
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import relief_art as ra
ra.PX = 120
from relief_art import Relief
from scipy import ndimage as ndi
from PIL import Image
from panel_art import panel, font, spaced

W, H = 22.56, 5.55
KX = [-8.5, -6.0, -3.5, -1.0, 1.5, 4.0, 6.5]; KZ = 2.2975

def btn_leds(bid, x, z, n, col, dz=0.42, step=0.17, w=0.08, h=0.055, st=12):
    return [dict(id=f'{bid}{i+1}', x=x + (i - (n - 1) / 2) * step, z=z + dz, w=w, h=h, color=col, st=st) for i in range(n)]

def bevel_rim(r, m, w, top, base=0.2):
    rim = ndi.binary_dilation(m, iterations=max(1, int(w * ra.PX))) & ~m
    r.put(r.dome(rim, top, base, 0.5), rim); return rim

# =============================== LYSSA ===============================
r = Relief(W, H, base=0.3)
VI = [0.72, 0.32, 1.0]
# top and bottom rails
for y in (H / 2 - 0.42, -H / 2 + 0.42):
    m = np.abs(r.Y - y) <= 0.11; m &= np.abs(r.X) <= W / 2 - 0.35; r.put(r.dome(m, 0.62, 0.3, 0.5), m)
edge = (np.abs(r.X) > W / 2 - 0.22) | (np.abs(r.Y) > H / 2 - 0.22); r.H[edge] = 0.45
holes = np.zeros_like(r.holes)
# slanted gill vents, mirrored, leaning toward the centre
for sx in (-1, 1):
    for k in range(10):
        xc = sx * (2.55 + k * 0.78)
        sl = math.tan(math.radians(24)) * sx      # top leans toward centre
        hh = 3.3 - 0.12 * k
        m = (np.abs(r.Y) <= hh / 2) & (np.abs(r.X - xc + sl * r.Y) <= 0.13)
        holes |= m
for m_ in [holes]:
    bevel_rim(r, m_, 0.06, 0.55, 0.3)
r.H[holes] = 0.05
# centre diamond badge with an inset groove and a sharp chevron
dm = (np.abs(r.X) / 1.35 + np.abs(r.Y) / 2.05) <= 1
r.H[dm] = 0.3; r.put(r.dome(dm, 0.9, 0.35, 0.35), dm)
inner = (np.abs(r.X) / 0.95 + np.abs(r.Y) / 1.5) <= 1
ring = inner & ~((np.abs(r.X) / 0.86 + np.abs(r.Y) / 1.38) <= 1); r.groove(ring, 0.12, 1.0)
for dy in (0.25, -0.35):
    cv = (np.abs(np.abs(r.X) * 0.9 - (r.Y - dy) * -1 - 0.0) <= 0.06) & (np.abs(r.X) <= 0.55) & (r.Y <= dy + 0.05)
    r.put(r.dome(cv, 1.0, 0.6, 0.5, reach=0.05), cv)
r.holes = holes
r.save('art/lyssa_h.png', 'art/lyssa_holes.png', blur=0.6)
tubes = [dict(x=x, z=7.025, h=2.5, r=0.36, color=VI, strength=8, light=45) for x in (-8.6, -6.2, -3.8, 3.8, 6.2, 8.6)]
kn = [dict(x=x, z=KZ, r=0.42, v=v, shape='skirt2', mat='bakelite', mat2='silver', ptr=VI) for x, v in zip(KX, [0.7, 0.5, 0.42, 0.6, 0.5, 0.55, 0.45])]
spec = dict(size=[24, 10, 10.5], border=0.7, recess=1.4, bevel=0.2, rimcolor=[0.8, 0.6, 1.0],
            body=dict(color=[0.016, 0.016, 0.018], rough=0.6, bump=['leather', 40, 0.5]), handle=dict(w=8.5, h=1.2, color=[0.02, 0.02, 0.022]),
            glow_strips=[dict(z=4.25, color=VI, strength=3.5, w=22.54)],
            zones=[dict(type='relief', z=[4.25, 9.8], height='art/lyssa_h.png', holes='art/lyssa_holes.png', depth=0.34, res=60,
                        color=[0.2, 0.2, 0.215], cavity=0.35, metal=0.9, rough=0.4, hi_at=0.7, grain=[200, 0.05], tubes=tubes,
                        ambient=[dict(x=x, energy=60, color=VI, size=4) for x in (-6.2, 6.2)]),
                   dict(type='panel', z=[0.7, 4.25], art='art/lyssa_panel.png', artemis=0.12, color=[0.05, 0.05, 0.055], metal=0.85, rough=0.38,
                        bump=['brush', 60, 0.03], knobs=kn, jacks=[[-10.3, KZ, 'jack']],
                        buttons=[dict(id='channel', x=8.0, z=KZ, w=0.5, h=0.34, cap=[0.02, 0.02, 0.022], metal=0.5),
                                 dict(id='voice', x=8.95, z=KZ, w=0.5, h=0.34, cap=[0.02, 0.02, 0.022], metal=0.5)],
                        leds=btn_leds('ch', 8.0, KZ, 2, VI, step=0.2) + btn_leds('vo', 8.95, KZ, 3, VI, step=0.17, w=0.07),
                        jewels=[dict(x=10.1, z=KZ, color=VI, r=0.26, strength=24, id='power')])])
json.dump(spec, open('amps/lyssa.json', 'w'), indent=1)
LC = (214, 210, 222, 255)
def lyssa_extra(d, P, col):
    f = font('mich', 10)
    for i, t in enumerate(('R', 'L')): spaced(d, P(8.0 + (i - 0.5) * 0.2, KZ + 0.62), t, f, col, 0)
    for i, t in enumerate(('1', '2', '3')): spaced(d, P(8.95 + (i - 1) * 0.17, KZ + 0.62), t, f, col, 0)
panel('art/lyssa_panel.png', spec, (0.7, 4.25), [dict(k, label=l) for k, l in zip(kn, ['GAIN', 'LOW', 'MID', 'HIGH', 'PRESENCE', 'RESONANCE', 'VOLUME'])],
      toggles=[(8.0, KZ, 'CHANNEL'), (8.95, KZ, 'VOICE')], jewels=[(10.1, KZ - 0.1, 'POWER')], face='mich', col=LC, size=15, dial='numbers',
      numface='mich', numsize=11, tick_len=0.1, spacing=3, extra=lyssa_extra, lines=[(-10.8, 0.98, 10.8, 0.98, 2)])
print('lyssa ok')

# =============================== BELIAL ===============================
# blacked-out head: thin black grille bars across the whole front, red-hot tubes and red light between them,
# and a free-floating gothic wordmark in front of the bars (relief plate with everything but the letters cut away)
RD = [1.0, 0.07, 0.03]
FD = __file__.rsplit('/', 2)[0] + '/render/fonts/'
r = Relief(8.4, 2.7, base=0.0)
m = r.glyph('Belial', FD + 'GrenzeGotisch.ttf', 0, 0.2, 2.75, top=1.0, base=0.45, power=0.45, reach=0.07)
sw = r.stroke([(x, -0.92 - 0.1 * math.sin((x + 3.6) / 7.2 * math.pi)) for x in np.linspace(-3.6, 3.6, 120)], 0.09, taper=(0.09, 0.02))
r.put(r.dome(sw, 0.9, 0.45, 0.5, reach=0.04), sw)
solid = r.H > 0.01
r.holes = ~ndi.binary_dilation(solid, iterations=1)
r.save('art/belial_logo_h.png', 'art/belial_logo_holes.png', blur=0.5)
tubes = [dict(x=x, z=7.0, h=2.7, r=0.38, color=[1.0, 0.2, 0.05], strength=10, light=55) for x in (-9.4, -7.4, -5.4, -3.4, 3.4, 5.4, 7.4, 9.4)]
kn = [dict(x=x, z=KZ, r=0.42, v=v, shape='knurl', mat='bakelite', mat2='bakelite', ptr='white') for x, v in zip(KX, [0.72, 0.55, 0.38, 0.6, 0.5, 0.6, 0.45])]
spec = dict(size=[24, 10, 10.5], border=0.7, recess=2.4, bevel=0.2, rimcolor=[1, 0.25, 0.2],
            body=dict(color=[0.01, 0.01, 0.011], rough=0.62, bump=['leather', 36, 0.6]), handle=dict(w=8.5, h=1.2, color=[0.014, 0.014, 0.015]),
            glow_strips=[dict(z=4.25, color=RD, strength=4, w=22.54)],
            zones=[dict(type='bars', z=[4.25, 9.8], n=13, bh=0.2, bd=0.3, pad=0.24, color=[0.012, 0.012, 0.013], metal=0.55, rough=0.36,
                        tubes=tubes, backglow=[1.0, 0.06, 0.02], backglow_strength=0.22,
                        ambient=[dict(x=x, energy=120, color=[1.0, 0.1, 0.03], size=5) for x in (-7, 0, 7)],
                        plates=[dict(height='art/belial_logo_h.png', holes='art/belial_logo_holes.png', x=0, z=7.1, w=8.4, h=2.7, depth=0.26, res=90, lift=0.45,
                                     color=[0.06, 0.06, 0.065], cavity=0.6, metal=0.9, rough=0.22, hi_at=0.9)]),
                   dict(type='panel', z=[0.7, 4.25], art='art/belial_panel.png', artemis=0.2, color=[0.011, 0.011, 0.012], metal=0.45, rough=0.42,
                        bump=['brush', 60, 0.02], knobs=kn, jacks=[[-10.3, KZ, 'jack']],
                        buttons=[dict(id='mic', x=8.4, z=KZ, w=0.5, h=0.34, cap=[0.012, 0.012, 0.013], metal=0.4)],
                        leds=btn_leds('mic', 8.4, KZ, 2, RD, step=0.22),
                        jewels=[dict(x=9.7, z=KZ, color=RD, r=0.28, strength=30, id='power')])])
json.dump(spec, open('amps/belial.json', 'w'), indent=1)
BC = (205, 200, 204, 255)
def belial_extra(d, P, col):
    f = font('grenze', 15)
    for i, t in enumerate(('C', 'O')): spaced(d, P(8.4 + (i - 0.5) * 0.22, KZ + 0.62), t, f, col, 0)
panel('art/belial_panel.png', spec, (0.7, 4.25), [dict(k, label=l) for k, l in zip(kn, ['Gain', 'Bass', 'Middle', 'Treble', 'Presence', 'Depth', 'Master'])],
      toggles=[(8.4, KZ, 'Mic')], jewels=[(9.7, KZ - 0.1, 'Power')], face='grenze', col=BC, size=30, dial='lines', extra=belial_extra,
      lines=[(-10.8, 0.98, 10.8, 0.98, 2)])
print('belial ok')
