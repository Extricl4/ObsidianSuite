"""Hyperion -> Dumble Overdrive Special 102 'Ford' (slamminmofo, 83 amp-only captures, curated)."""
import json, sys
sys.path.insert(0, 'tools')
from panel_art import panel, font, spaced
s = json.load(open('amps/hyperion.json')); z = [q for q in s['zones'] if q.get('knobs')][0]; KZ = 2.2975; AM = [1.0, 0.7, 0.3]
z.pop('toggles', None)
z['jewels'] = [dict(z['jewels'][0], x=10.1, id='power')]
z['buttons'] = [dict(id='channel', x=8.05, z=KZ, w=0.46, h=0.32, cap=[0.06, 0.045, 0.03], metal=0.9, rough=0.3),
                dict(id='voice', x=8.95, z=KZ, w=0.46, h=0.32, cap=[0.06, 0.045, 0.03], metal=0.9, rough=0.3)]
z['leds'] = [dict(id=f'ch{i+1}', x=8.05 + (i - 0.5) * 0.22, z=KZ + 0.42, w=0.08, h=0.055, color=AM, st=12) for i in range(2)]
LAB = ['VOLUME', 'TREBLE', 'MIDDLE', 'BASS', 'PRESENCE', 'DRIVE', 'MASTER']
def extra(d, P, col):
    f = font('cdec', 12)
    spaced(d, P(7.94, KZ + 0.62), 'CL', f, col, 0); spaced(d, P(8.16, KZ + 0.62), 'OD', f, col, 0)
panel('art/hyperion_ds_panel.png', s, tuple(z['z']), [dict(k, label=l) for k, l in zip(z['knobs'], LAB)],
      toggles=[(8.05, KZ, 'CHANNEL'), (8.95, KZ, 'VOICE')], jewels=[(10.1, KZ - 0.1, 'POWER')],
      face='cdec', col=(230, 190, 110, 255), size=26, dial='numbers', extra=extra)
z['art'] = 'art/hyperion_ds_panel.png'
json.dump(s, open('amps/hyperion_ds.json', 'w'), indent=1); print('ok')
