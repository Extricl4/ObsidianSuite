"""Lamia -> Friedman BE-100 Deluxe (24 full-rig captures: CLN / BE / HBE / EL34 power amp, each with SM57, SM58 and a blend)."""
import json, sys
sys.path.insert(0, 'tools')
from panel_art import panel, font, spaced
_ns = {}; exec(open('tools/reknob.py').read().split('for name, c in CFG.items()')[0].replace('from panel_art import panel', ''), _ns); c = _ns['CFG']['lamia']
s = json.load(open('amps/lamia.json')); z = [q for q in s['zones'] if q.get('knobs')][0]
KZ = 2.2975; RED = [1, 0.07, 0.05]
KX = [-8.6 + i * 2.2 for i in range(7)]
for k, x in zip(z['knobs'], KX): k['x'] = round(x, 3)
z.pop('toggles', None)
BX = [6.35, 7.1, 7.85, 8.6]; MX = 9.55; PX_ = 10.45
z['jewels'] = [dict(z['jewels'][0], x=PX_, r=0.26, id='power')]
z['buttons'] = [dict(id=f'ch{i+1}', x=x, z=KZ, w=0.46, h=0.32, cap=[0.03, 0.02, 0.022], metal=0.6) for i, x in enumerate(BX)] + \
               [dict(id='mic', x=MX, z=KZ, w=0.46, h=0.32, cap=[0.03, 0.02, 0.022], metal=0.6)]
z['leds'] = [dict(id=f'ch{i+1}', x=x, z=KZ + 0.42, w=0.09, h=0.06, color=RED, st=12) for i, x in enumerate(BX)] + \
            [dict(id=f'mic{i+1}', x=MX + (i - 1) * 0.19, z=KZ + 0.42, w=0.07, h=0.055, color=RED, st=12) for i in range(3)]
LAB = c['labels']
col = c['col']


def extra(d, P, col):
    f = font('frak', 30)
    y = P(0, KZ + 0.98)[1]
    spaced(d, (P((BX[0] + BX[-1]) / 2, 0)[0], y), 'Channel', f, col, 3)
    spaced(d, (P(MX, 0)[0], y), 'Mic', f, col, 3)
    fs = font('marcellus', 12)
    for i, t in enumerate(('57', '58', 'MIX')):
        spaced(d, P(MX + (i - 1) * 0.19, KZ + 0.62), t, fs, col, 0)


panel('art/lamia_be_panel.png', s, tuple(z['z']), [dict(k, label=l) for k, l in zip(z['knobs'], LAB)],
      toggles=[(x, KZ, t) for x, t in zip(BX, ('Clean', 'BE', 'HBE', 'EL34'))] + [(MX, KZ, '')], jewels=[(PX_, KZ - 0.05, 'Power')],
      face=c['face'], col=col, size=c['size'], dial=c['dial'], numface='marcellus', extra=extra)
z['art'] = 'art/lamia_be_panel.png'
json.dump(s, open('amps/lamia_be.json', 'w'), indent=1); print('ok')
