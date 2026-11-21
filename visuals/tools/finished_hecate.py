"""Hecate -> EVH 5150 (rusen, 1 full-rig capture). No channels -> no channel buttons; toggles removed, lamp is power."""
import json, sys
sys.path.insert(0, 'tools')
from panel_art import panel
_ns = {}; exec(open('tools/reknob.py').read().split('for name, c in CFG.items()')[0].replace('from panel_art import panel', ''), _ns); c = _ns['CFG']['hecate']
s = json.load(open('amps/hecate.json')); z = [q for q in s['zones'] if q.get('knobs')][0]
z.pop('toggles', None)
z['jewels'] = [dict(z['jewels'][0], id='power')]
LAB = ['GAIN', 'VOLUME', 'LOW', 'MID', 'HIGH', 'PRES', 'RESO']
ks = [dict(k, label=l) for k, l in zip(z['knobs'], LAB)]
for k in ks:
    if k['r'] < 0.5: k['dial'] = c.get('small_dial', 'none')
panel('art/hecate_evh_panel.png', s, tuple(z['z']), ks, jewels=[(9.38, 1.8, 'POWER')], face=c['face'], col=c['col'], size=c['size'], dial=c['dial'],
      lines=[(-s['size'][0] / 2 + s['border'] + 0.5, z['z'][0] + 0.28, s['size'][0] / 2 - s['border'] - 0.5, z['z'][0] + 0.28, 2)])
z['art'] = 'art/hecate_evh_panel.png'
json.dump(s, open('amps/hecate_evh.json', 'w'), indent=1); print('ok')
