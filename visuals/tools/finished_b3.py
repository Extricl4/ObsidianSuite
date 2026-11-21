"""Tyche -> Marshall JMP-50 Plexi (1 full rig), Thanatos -> Soldano SLO-100 Drive (1 full rig). No channels: toggles removed, lamp = power."""
import json, sys
sys.path.insert(0, 'tools')
from panel_art import panel
_ns = {}; exec(open('tools/reknob.py').read().split('for name, c in CFG.items()')[0].replace('from panel_art import panel', ''), _ns); CFG = _ns['CFG']
for src, dst, labs, face, col, size, dial, numsize, lines in [
        ('tyche', 'tyche_pl', ['GAIN', 'TREBLE', 'MIDDLE', 'BASS', 'PRESENCE', 'VOLUME', 'MASTER'], 'marcellus', (212, 172, 102, 255), 28, 'numbers', None, True),
        ('thanatos', 'thanatos_slo', ['PREAMP', 'BASS', 'MIDDLE', 'TREBLE', 'PRESENCE', 'DEPTH', 'MASTER'], 'mich', (200, 225, 235, 255), 17, 'numbers', 15, False)]:
    s = json.load(open(f'amps/{src}.json')); z = [q for q in s['zones'] if q.get('knobs')][0]
    z.pop('toggles', None); j = z['jewels'][0]; z['jewels'] = [dict(j, id='power')]
    kw = dict(numsize=numsize) if numsize else {}
    if lines: kw['lines'] = [(-s['size'][0] / 2 + s['border'] + 0.5, z['z'][0] + 0.28, s['size'][0] / 2 - s['border'] - 0.5, z['z'][0] + 0.28, 2)]
    panel(f'art/{dst}_panel.png', s, tuple(z['z']), [dict(k, label=l) for k, l in zip(z['knobs'], labs)], jewels=[(j['x'], j['z'] - 0.1, 'POWER')],
          face=face, col=col, size=size, dial=dial, **kw)
    z['art'] = f'art/{dst}_panel.png'
    json.dump(s, open(f'amps/{dst}.json', 'w'), indent=1); print('ok', dst)
