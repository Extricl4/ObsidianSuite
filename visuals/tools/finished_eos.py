"""Eos -> Hiwatt DR103 (13 amp-only captures) + Comfortably Numb solo tones (7 full-rig captures).
Restyled: thick ivory faceplate with rounded edges, gold rails, round lacquered transformer cans, gold tube collars and feet,
a gold sunrise emblem, gold channel buttons and an ivory chicken-head SOLO selector."""
import json, math, sys
sys.path.insert(0, 'tools')
from PIL import Image, ImageDraw
from panel_art import panel, font, spaced

GOLD = (236, 196, 118, 255)
# ---------- sunrise emblem ----------
S_ = 600; im = Image.new('RGBA', (S_, S_), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
cx, hy, R = S_ / 2, 330, 118
for i in range(15):
    a = math.radians(180 - i * 180 / 14); L = 238 if i % 2 == 0 else 190; w = 14 if i % 2 == 0 else 9
    a1, a2 = a - math.radians(2.2 if i % 2 == 0 else 1.6), a + math.radians(2.2 if i % 2 == 0 else 1.6)
    d.polygon([(cx + math.cos(a1) * (R + 18), hy - math.sin(a1) * (R + 18)), (cx + math.cos(a) * L, hy - math.sin(a) * L),
               (cx + math.cos(a2) * (R + 18), hy - math.sin(a2) * (R + 18))], fill=GOLD)
d.pieslice((cx - R, hy - R, cx + R, hy + R), 180, 360, fill=GOLD)
d.pieslice((cx - R + 16, hy - R + 16, cx + R - 16, hy + R - 16), 180, 360, fill=(0, 0, 0, 0))
d.pieslice((cx - R + 30, hy - R + 30, cx + R - 30, hy + R - 30), 180, 360, fill=GOLD)
d.rectangle((40, hy + 6, S_ - 40, hy + 16), fill=GOLD)
for k, (x0, x1) in enumerate(((120, 480), (180, 420))):
    yy = hy + 40 + k * 22; d.rectangle((x0, yy, x1, yy + 6), fill=GOLD)
f = font('cormorant', 120)
spaced(d, (cx, 510), 'EOS', f, GOLD, 34)
im.save('art/eos_logo.png')

# ---------- spec ----------
s = json.load(open('amps/eos.json'))
s['recess'] = 1.3; s['front_inset'] = 0.15
op = s['open']
op.update(chassis=[0.6, 0.55, 0.46], chassis_rough=0.26, chassis_bevel=0.3, collar='gold', feet='gold')
op['trans'] = [dict(x=-6.2, y=2.6, w=3.0, d=3.0, h=2.9, style='can', color=[0.012, 0.011, 0.012], band='gold', bands=[0.12, 0.88]),
               dict(x=0, y=2.8, w=2.3, d=2.3, h=2.3, style='can', color=[0.012, 0.011, 0.012], band='gold', bands=[0.14]),
               dict(x=6.2, y=2.6, w=3.0, d=3.0, h=2.9, style='can', color=[0.012, 0.011, 0.012], band='gold', bands=[0.12, 0.88])]
z = s['zones'][0]; KZ = 2.0
KX = [-7.9, -6.0, -4.1, -2.2, -0.3, 1.6]; V = [0.5, 0.5, 0.55, 0.5, 0.45, 0.6]
SX = 3.85; NAMES = ['OFF', 'STUDIO', 'DSOT', 'PULSE', 'RTN', 'GDANSK', 'POMPEII', 'ROME']
z['knobs'] = [dict(x=x, z=KZ, r=0.42, v=v, shape='ivory', mat='ivory', mat2='gold', ptr='gold') for x, v in zip(KX, V)] + \
             [dict(x=SX, z=KZ, r=0.4, v=0.0, shape='chicken', mat='ivory', ptr='gold')]
z.update(color=[0.8, 0.755, 0.655], rough=0.36, slab_bevel=0.34, bump=['noise', 300, 0.012],
         rails=[dict(z=3.66, r=0.045, dy=-0.035, mat='gold', len=21.0), dict(z=0.74, r=0.045, dy=-0.035, mat='gold', len=21.0)],
         logos=[dict(img='art/eos_logo.png', x=-9.85, z=KZ + 0.12, w=1.75, h=1.75, metal=1.0, rough=0.2, emis=0.04)])
z.pop('toggles', None)
BX = [5.95, 6.9, 7.85]; DX = 9.0; PJ = 10.25; AMB = [1.0, 0.55, 0.2]
gcap = [0.78, 0.56, 0.26]
z['buttons'] = [dict(id=f'ch{i+1}', x=x, z=KZ, w=0.5, h=0.36, cap=gcap, metal=1.0, led=AMB, ledr=0.055) for i, x in enumerate(BX)] + \
               [dict(id='drive', x=DX, z=KZ, w=0.5, h=0.36, cap=gcap, metal=1.0)]
z['leds'] = [dict(id=f'dr{i+1}', x=DX + (i - 1) * 0.2, z=KZ + 0.42, w=0.08, h=0.055, color=AMB, st=14) for i in range(3)]
z['jewels'] = [dict(x=PJ, z=KZ, color=[1, 0.85, 0.6], r=0.26, strength=15, button=True, id='power')]
col = (92, 70, 40, 255)


def extra(d, P, c):
    fh = font('cormorant', 26)
    y = P(0, KZ + 0.98)[1]
    spaced(d, (P(SX, 0)[0], y), 'SOLO', fh, c, 4)
    spaced(d, (P(sum(BX) / 3, 0)[0], y), 'CHANNEL', fh, c, 4)
    spaced(d, (P(DX, 0)[0], y), 'DRIVE', fh, c, 4)
    fn = font('cormorant', 15)
    for i, n in enumerate(NAMES):
        a = math.radians(225 - 270 * i / (len(NAMES) - 1)); ca, sa = math.cos(a), math.sin(a)
        dx, dz = SX + ca * 0.62, KZ + sa * 0.62
        d.line([P(SX + ca * 0.5, KZ + sa * 0.5), P(dx, dz)], fill=c, width=2)
        tx, tz = SX + ca * 0.7, KZ + sa * 0.7
        anc = 'rm' if ca < -0.3 else 'lm' if ca > 0.3 else 'mb'
        d.text(P(tx, tz), n, font=fn, fill=c, anchor=anc)
    fs = font('cormorant', 14)
    for i, t in enumerate(('I', 'II', 'III')):
        spaced(d, P(DX + (i - 1) * 0.2, KZ + 0.6), t, fs, c, 0)


LAB = ['VOLUME', 'BASS', 'TREBLE', 'MIDDLE', 'PRESENCE', 'MASTER']
panel('art/eos_hw_panel.png', s, tuple(z['z']), [dict(k, label=l) for k, l in zip(z['knobs'][:6], LAB)] + [dict(z['knobs'][6], dial='none')],
      toggles=[(BX[0], KZ, 'NORMAL'), (BX[1], KZ, 'BRILLIANT'), (BX[2], KZ, 'LINKED'), (DX, KZ, '')], jewels=[(PJ, KZ - 0.05, 'POWER')],
      face='cormorant', col=col, size=26, dial='numbers', extra=extra, spacing=4)
z['art'] = 'art/eos_hw_panel.png'; z['artemis'] = 0
json.dump(s, open('amps/eos_hw.json', 'w'), indent=1); print('ok')
