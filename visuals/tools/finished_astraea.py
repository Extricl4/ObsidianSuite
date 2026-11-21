"""Astraea -> Vox AC30 CH (96 amp-only captures: Normal and Top Boost at many Volume / Tone Cut / Bass / Treble settings, plus a treble booster)."""
import json, sys
sys.path.insert(0, 'tools')
from panel_art import panel, font, spaced
s = json.load(open('amps/astraea.json')); z = [q for q in s['zones'] if q.get('knobs')][0]
KZ = 2.2975; BLUE = [0.55, 0.72, 1.0]
z['knobs'] = z['knobs'][:6]
V = [0.7, 0.5, 0.5, 0.2, 0.3, 0.5]
for k, v in zip(z['knobs'], V): k['v'] = v
z.pop('toggles', None)
BX = [5.95, 7.3]; BB = 8.6; PJ = 9.95
z['jewels'] = [dict(z['jewels'][0], x=PJ, r=0.27, id='power')]
cap = [0.02, 0.025, 0.05]
z['buttons'] = [dict(id=f'ch{i+1}', x=x, z=KZ, w=0.56, h=0.4, cap=cap, metal=0.6, led=BLUE, ledr=0.06) for i, x in enumerate(BX)] + \
               [dict(id='boost', x=BB, z=KZ, w=0.56, h=0.4, cap=cap, metal=0.6, led=[1.0, 0.72, 0.3], ledr=0.06)]
LAB = ['VOLUME', 'TREBLE', 'BASS', 'TONE CUT', 'REVERB', 'MASTER']
col = (228, 192, 112, 255)


def extra(d, P, c):
    f = font('macondo', 30)
    spaced(d, (P((BX[0] + BX[1]) / 2, 0)[0], P(0, KZ + 1.02)[1]), 'CHANNEL', f, c, 4)
    d.line([P(BX[0] - 0.1, KZ + 0.72), P(BX[1] + 0.1, KZ + 0.72)], fill=c, width=2)


panel('art/astraea_ac_panel.png', s, tuple(z['z']), [dict(k, label=l) for k, l in zip(z['knobs'], LAB)],
      toggles=[(BX[0], KZ, 'NORMAL'), (BX[1], KZ, 'TOP BOOST'), (BB, KZ, 'BOOST')], jewels=[(PJ, KZ - 0.05, 'POWER')],
      face='macondo', col=col, size=32, dial='dots', lines=[(-10.9, 0.98, 10.9, 0.98, 2)], extra=extra)
z['art'] = 'art/astraea_ac_panel.png'
json.dump(s, open('amps/astraea_ac.json', 'w'), indent=1); print('ok')
