"""Melinoe -> Fender Super Reverb '77 (3 full-rig captures: SM57 / AKG 414 / both)."""
import json, sys
sys.path.insert(0, 'tools')
from panel_art import panel, font, spaced
_ns = {}; exec(open('tools/reknob.py').read().split('for name, c in CFG.items()')[0].replace('from panel_art import panel', ''), _ns); c = _ns['CFG']['melinoe']
s = json.load(open('amps/melinoe.json')); z = [q for q in s['zones'] if q.get('knobs')][0]; KZ = 2.2975; VI = [0.62, 0.25, 1.0]
z.pop('toggles', None)
z['jewels'] = [dict(z['jewels'][0], id='power')]
z['buttons'] = [dict(id='mic', x=8.4, z=KZ, w=0.5, h=0.34, cap=[0.03, 0.028, 0.034], metal=0.6)]
z['leds'] = [dict(id=f'mic{i+1}', x=8.4 + (i - 1) * 0.25, z=KZ + 0.42, w=0.08, h=0.055, color=VI, st=12) for i in range(3)]
LAB = ['VOLUME', 'TREBLE', 'BASS', 'MIDDLE', 'REVERB', 'SPEED', 'INTENSITY']
def extra(d, P, col):
    f = font(c['face'], 11)
    for i, t in enumerate(('57', '414', 'MIX')): spaced(d, P(8.4 + (i - 1) * 0.25, KZ + 0.62), t, f, col, 0)
panel('art/melinoe_sr_panel.png', s, tuple(z['z']), [dict(k, label=l) for k, l in zip(z['knobs'], LAB)], toggles=[(8.4, KZ, 'MIC')],
      jewels=[(9.5, KZ - 0.1, 'POWER')], face=c['face'], col=c['col'], size=c['size'], dial=c['dial'], extra=extra)
z['art'] = 'art/melinoe_sr_panel.png'
json.dump(s, open('amps/melinoe_sr.json', 'w'), indent=1); print('ok')
