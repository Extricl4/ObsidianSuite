"""Nyx -> Bogner Uberschall Rev Blue (16 amp-only captures: 3 clean presets, 12 lead presets)."""
import json, sys
sys.path.insert(0, 'tools')
from panel_art import panel, font, spaced
_ns = {}; exec(open('tools/reknob.py').read().split('for name, c in CFG.items()')[0].replace('from panel_art import panel', ''), _ns); c = _ns['CFG']['nyx']
s = json.load(open('amps/nyx.json')); z = [q for q in s['zones'] if q.get('knobs')][0]; KZ = 1.75; CY = [0.2, 0.9, 1.0]
z.pop('toggles', None)
z['jewels'] = [dict(z['jewels'][0], id='power')]
z['buttons'] = [dict(id='channel', x=8.1, z=KZ, w=0.5, h=0.34, cap=[0.03, 0.03, 0.035], metal=0.7),
                dict(id='voice', x=9.0, z=KZ, w=0.5, h=0.34, cap=[0.03, 0.03, 0.035], metal=0.7)]
z['leds'] = [dict(id=f'ch{i+1}', x=8.1 + (i - 0.5) * 0.22, z=KZ + 0.42, w=0.08, h=0.055, color=CY, st=12) for i in range(2)]
LAB = ['GAIN', 'BASS', 'MIDDLE', 'TREBLE', 'PRESENCE', 'DEPTH', 'MASTER']
def extra(d, P, col):
    f = font(c['face'], 12)
    for i, t in enumerate(('CLN', 'LD')): spaced(d, P(8.1 + (i - 0.5) * 0.22, KZ + 0.6), t, f, col, 0)
panel('art/nyx_ub_panel.png', s, tuple(z['z']), [dict(k, label=l) for k, l in zip(z['knobs'], LAB)], toggles=[(8.1, KZ, 'CHANNEL'), (9.0, KZ, 'VOICE')],
      jewels=[(10.2, KZ - 0.1, 'POWER')], face=c['face'], col=c['col'], size=c['size'], dial=c['dial'], extra=extra)
z['art'] = 'art/nyx_ub_panel.png'
json.dump(s, open('amps/nyx_ub.json', 'w'), indent=1); print('ok')
