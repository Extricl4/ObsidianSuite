"""Persephone -> Roland JC-120 (autoguitarrig amp-only pack: Clean/Bright/Warm, normal + Stereo)."""
import json, sys
sys.path.insert(0, 'tools')
from panel_art import panel, font, spaced
_ns = {}; exec(open('tools/reknob.py').read().split('for name, c in CFG.items()')[0].replace('from panel_art import panel', ''), _ns); c = _ns['CFG']['persephone']
s = json.load(open('amps/persephone.json')); z = [q for q in s['zones'] if q.get('knobs')][0]; KZ = 2.2975; PK = [1.0, 0.45, 0.55]
z.pop('toggles', None)
z['jewels'] = [dict(z['jewels'][0], x=10.25, id='power')]
z['buttons'] = [dict(id='channel', x=7.75, z=KZ, w=0.44, h=0.32, cap=[0.8, 0.79, 0.76], metal=0.0, rough=0.5),
                dict(id='voice', x=8.6, z=KZ, w=0.44, h=0.32, cap=[0.8, 0.79, 0.76], metal=0.0, rough=0.5),
                dict(id='chorus', x=9.45, z=KZ, w=0.44, h=0.32, cap=[0.8, 0.79, 0.76], metal=0.0, rough=0.5, led=PK, ledr=0.05)]
z['leds'] = [dict(id=f'ch{i+1}', x=7.75 + (i - 0.5) * 0.2, z=KZ + 0.42, w=0.08, h=0.055, color=PK, st=12) for i in range(2)] + \
            [dict(id=f'vo{i+1}', x=8.6 + (i - 1) * 0.16, z=KZ + 0.42, w=0.07, h=0.055, color=PK, st=12) for i in range(3)]
LAB = ['VOLUME', 'TREBLE', 'MIDDLE', 'BASS', 'REVERB', 'SPEED', 'DEPTH']
def extra(d, P, col):
    f = font(c['face'], 13)
    for i, t in enumerate(('I', 'II')): spaced(d, P(7.75 + (i - 0.5) * 0.2, KZ + 0.62), t, f, col, 0)
    for i, t in enumerate(('C', 'B', 'W')): spaced(d, P(8.6 + (i - 1) * 0.16, KZ + 0.62), t, f, col, 0)
panel('art/persephone_jc_panel.png', s, tuple(z['z']), [dict(k, label=l) for k, l in zip(z['knobs'], LAB)],
      toggles=[(7.75, KZ, 'CHANNEL'), (8.6, KZ, 'VOICE'), (9.45, KZ, 'CHORUS')], jewels=[(10.25, KZ - 0.1, 'POWER')],
      face=c['face'], col=c['col'], size=26, dial=c['dial'], extra=extra)
z['art'] = 'art/persephone_jc_panel.png'
json.dump(s, open('amps/persephone_jc.json', 'w'), indent=1); print('ok')
