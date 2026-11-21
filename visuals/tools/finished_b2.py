"""Moros -> Omega Granophyre, Vulcan -> Diezel VH4 (merged packs), Niflheim -> Revv Generator 120 Red."""
import json, sys
sys.path.insert(0, 'tools')
from panel_art import panel, font, spaced
_ns = {}; exec(open('tools/reknob.py').read().split('for name, c in CFG.items()')[0].replace('from panel_art import panel', ''), _ns); CFG = _ns['CFG']
def load(n): return json.load(open(f'amps/{n}.json'))
def kz(s): return [q for q in s['zones'] if q.get('knobs')][0]
def leds(bid, x, z, n, col, step=0.17, w=0.08, dz=0.42): return [dict(id=f'{bid}{i+1}', x=x + (i - (n - 1) / 2) * step, z=z + dz, w=w, h=0.055, color=col, st=12) for i in range(n)]

# ---------------- Moros: Granophyre (amp, + Blower Box, + Plumes) ----------------
s = load('moros'); z = kz(s); KZ = 2.25; BL = [0.3, 0.55, 1.0]
z.pop('toggles', None)
z['jewels'] = [dict(x=8.9, z=KZ - 0.15, color=BL, r=0.22, strength=24, id='power')]
z['buttons'] = [dict(id='boost', x=-1.2, z=KZ, w=0.44, h=0.3, cap=[0.02, 0.02, 0.022], metal=0.5)]
z['leds'] = leds('bo', -1.2, KZ, 2, BL, step=0.2)
ks = [(-8.6, 'GAIN'), (-6.7, 'BASS'), (-5.5, 'MID'), (-4.3, 'TREBLE'), (-2.4, 'LEVEL'), (0.0, 'MASTER'), (1.75, 'TIGHT'), (2.95, 'AIR')]
def mextra(d, P, col):
    f = font('mich', 15); fb = font('mich', 28); fs = font('mich', 10)
    y = P(0, 3.25)[1]
    for (a, b) in ((-7.1, -3.9), (1.35, 3.35)):
        x0, x1 = P(a, 0)[0], P(b, 0)[0]; d.line([(x0, y + 12), (x0, y), (x1, y), (x1, y + 12)], fill=col, width=2)
    yl = P(0, KZ)[1]; d.line([(P(-10.0, 0)[0], yl), (P(-9.2, 0)[0], yl)], fill=col, width=2); d.line([(P(3.5, 0)[0], yl), (P(8.4, 0)[0], yl)], fill=col, width=2)
    spaced(d, P(8.9, 3.45), 'MOROS', fb, col, 18)
    spaced(d, P(-1.2, KZ - 0.5), 'BOOST', f, col, 2); spaced(d, P(8.9, KZ - 0.68), 'POWER', f, col, 2)
    spaced(d, P(-1.3, KZ + 0.6), 'BB', fs, col, 0); spaced(d, P(-1.1, KZ + 0.6), 'PL', fs, col, 0)
    for (x, r, l) in ((-8.6, 0.45, 0), (-2.4, 0.45, 0), (0.0, 0.48, 0)):
        cx, cy = P(x, KZ); rr = (r + 0.2) * 120; d.line([(cx, cy + rr), (cx, cy + rr + 22)], fill=col, width=2)
panel('art/moros_gp_panel.png', s, (0.7, 4.25), [dict(k, label=l, dial=('lines' if k['r'] > 0.4 else 'none')) for k, (x, l) in zip(z['knobs'], ks)],
      face='mich', col=(200, 202, 208, 255), size=15, dial='lines', tick_len=0.08, extra=mextra, spacing=3)
z['art'] = 'art/moros_gp_panel.png'
json.dump(s, open('amps/moros_gp.json', 'w'), indent=1); print('ok moros')

# ---------------- Vulcan: Diezel VH4 (4 channels) ----------------
s = load('vulcan'); z = kz(s); c = CFG['vulcan']; KZ = 2.2975; OR = [1.0, 0.35, 0.05]
z.pop('toggles', None)
z['jewels'] = [dict(z['jewels'][0], x=10.35, id='power')]
z['buttons'] = [dict(id=b, x=x, z=KZ, w=0.44, h=0.32, cap=[0.03, 0.025, 0.02], metal=0.5) for b, x in (('channel', 7.75), ('voice', 8.6), ('boost', 9.45))]
z['leds'] = leds('ch', 7.75, KZ, 4, OR, step=0.13, w=0.07) + leds('vo', 8.6, KZ, 3, OR, step=0.15, w=0.07) + leds('bo', 9.45, KZ, 1, OR)
def vextra(d, P, col):
    f = font('rocker', 11)
    for i in range(4): spaced(d, P(7.75 + (i - 1.5) * 0.13, KZ + 0.6), str(i + 1), f, col, 0)
    spaced(d, P(9.45, KZ + 0.6), '808', f, col, 0)
panel('art/vulcan_vh4_panel.png', s, tuple(z['z']), [dict(k, label=l) for k, l in zip(z['knobs'], c['labels'])],
      toggles=[(7.75, KZ, 'CHANNEL'), (8.6, KZ, 'VOICE'), (9.45, KZ, 'BOOST')], jewels=[(10.35, KZ - 0.1, 'POWER')],
      face=c['face'], col=c['col'], size=26, dial=c['dial'], extra=vextra)
z['art'] = 'art/vulcan_vh4_panel.png'
json.dump(s, open('amps/vulcan_vh4.json', 'w'), indent=1); print('ok vulcan')

# ---------------- Niflheim: Revv Generator 120 Red ----------------
s = load('niflheim'); z = kz(s); c = dict(face='jacquard', col=(196, 220, 244, 255), size=40, dial='lines'); KZ = 2.2975; IC = [0.45, 0.75, 1.0]
z.pop('toggles', None)
z['jewels'] = [dict(z['jewels'][0], x=9.6, id='power')]
z['buttons'] = [dict(id='mode', x=7.85, z=KZ, w=0.44, h=0.32, cap=[0.03, 0.03, 0.035], metal=0.6),
                dict(id='boost', x=8.7, z=KZ, w=0.44, h=0.32, cap=[0.03, 0.03, 0.035], metal=0.6)]
z['leds'] = leds('md', 7.85, KZ, 2, IC, step=0.2) + leds('bo', 8.7, KZ, 1, IC)
labs = ['Gain', 'Bass', 'Middle', 'Treble', 'Presence', 'Volume', 'Master']
def nextra(d, P, col):
    f = font('jacquard', 16)
    spaced(d, P(7.75, KZ + 0.6), 'C', f, col, 0); spaced(d, P(7.95, KZ + 0.6), 'R', f, col, 0); spaced(d, P(8.7, KZ + 0.6), 'OD', f, col, 0)
panel('art/niflheim_rv_panel.png', s, tuple(z['z']), [dict(k, label=l) for k, l in zip(z['knobs'], labs)],
      toggles=[(7.85, KZ, 'Mode'), (8.7, KZ, 'Boost')], jewels=[(9.6, KZ - 0.1, 'Power')],
      face=c['face'], col=c['col'], size=c['size'], dial=c['dial'], extra=nextra, lines=[(-10.9, 0.98, 10.9, 0.98, 2)])
z['art'] = 'art/niflheim_rv_panel.png'
json.dump(s, open('amps/niflheim_rv.json', 'w'), indent=1); print('ok niflheim')
