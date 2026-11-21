"""Eris (Mesa JP-2C captures): compact matte-black solid-state style head; the cabinet's top front is chamfered
(~55 deg from vertical) and that slanted face carries a frameless black mesh, framed by the cabinet edges.  python3 -I tools/art_eris.py"""
import json, sys, math, numpy as np
sys.path.insert(0, __file__.rsplit('/', 1)[0])
from PIL import Image
from panel_art import panel, font, spaced

W, H, D = 17.0, 6.6, 8.4; B = 0.5; IW = W - 2 * B
ANG = 55; HC = 2.5; RUN = HC * math.tan(math.radians(ANG)); L = math.hypot(HC, RUN)
FI = 0.15
ZA = H - HC; ZB = ZA + L - 0.12
PX = 120
w, h = int((IW - 0.04) * PX), int((ZB - ZA) * PX)
yy, xx = np.mgrid[0:h, 0:w] / PX
sp = 0.11; rows = np.round(yy / (sp * 0.866)); off = (rows % 2) * sp / 2
gx = np.round((xx - off) / sp) * sp + off; gy = rows * sp * 0.866
a = (((xx - gx) ** 2 + (yy - gy) ** 2 <= 0.046 ** 2) * 255).astype('uint8')
Image.fromarray(np.dstack([a, a, a, a])).save('art/eris_mask.png')

TL = [0.25, 1.0, 0.8]; KZ = 2.1; PT = H - HC - 0.2
KX = [-6.2, -4.45, -2.7, -0.95, 0.8, 2.55, 4.3]; V = [0.75, 0.6, 0.35, 0.45, 0.55, 0.45, 0.5]
kn = [dict(x=x, z=KZ, r=0.34, v=v, shape='knurl', mat='bakelite', mat2='bakelite', ptr='white') for x, v in zip(KX, V)]
def leds(bid, x, n, step, wd=0.06):
    return [dict(id=f'{bid}{i+1}', x=x + (i - (n - 1) / 2) * step, z=KZ + 0.36, w=wd, h=0.045, color=TL, st=12) for i in range(n)]
BX = (('channel', 5.35), ('voice', 6.05), ('boost', 6.75))
spec = dict(size=[W, D, H], border=B, recess=0.5, front_inset=FI, recess_z=[B, PT], chamfer=dict(h=HC, run=RUN), bevel=0.12, rimcolor=[0.5, 1, 0.9],
            body=dict(color=[0.011, 0.011, 0.012], rough=0.85, bump=['noise', 300, 0.012]), handle=dict(w=6.0, h=1.0, color=[0.012, 0.012, 0.013]),
            glow_strips=[dict(z=PT, color=TL, strength=2.4, w=IW - 0.04)],
            zones=[dict(type='perf', z=[ZA, ZB], tilt=ANG, shift_y=-FI - 0.09, mask='art/eris_mask.png', color=[0.07, 0.07, 0.075], metal=0.25, rough=0.6, thick=0.05,
                        ambient=[dict(x=x, energy=14, color=TL, size=5) for x in (-4.0, 0, 4.0)]),
                   dict(type='panel', z=[B, PT], art='art/eris_panel.png', artemis=0.1, color=[0.01, 0.01, 0.011], metal=0.2, rough=0.6,
                        bump=['noise', 300, 0.01], knobs=kn, jacks=[[-7.35, KZ, 'jack']],
                        buttons=[dict(id=b, x=x, z=KZ, w=0.38, h=0.27, cap=[0.015, 0.015, 0.016], metal=0.3) for b, x in BX],
                        leds=leds('ch', 5.35, 3, 0.13) + leds('vo', 6.05, 5, 0.085, 0.045) + leds('bo', 6.75, 2, 0.15),
                        jewels=[dict(x=7.45, z=KZ, color=TL, r=0.17, strength=20, id='power')])])
json.dump(spec, open('amps/eris.json', 'w'), indent=1)
GC = (190, 192, 196, 255)
def extra(d, P, col):
    f = font('mich', 8)
    for i, t in enumerate(('1', '2', '3')): spaced(d, P(5.35 + (i - 1) * 0.13, KZ + 0.5), t, f, col, 0)
    for i, t in enumerate(('808', 'SH')): spaced(d, P(6.75 + (i - 0.5) * 0.15, KZ + 0.5), t, font('mich', 7), col, 0)
panel('art/eris_panel.png', spec, (B, PT), [dict(k, label=l) for k, l in zip(kn, ['GAIN', 'TREBLE', 'MIDDLE', 'BASS', 'PRESENCE', 'MASTER', 'OUTPUT'])],
      toggles=[(x, KZ, n.upper()) for n, x in BX], jewels=[(7.45, KZ - 0.1, 'POWER')], face='mich', col=GC, size=11,
      dial='lines', tick_len=0.09, spacing=2, extra=extra)
print('eris ok')
