"""Dionysus v2 spec: swap the stencil grille for the sculpted bronze grapevine relief.
Order: tools/new_erebus_dionysus.py (writes the base spec), tools/art_dionysus.py (relief art), then this script."""
import json
s = json.load(open('amps/dionysus.json'))
AMB = [1.0, 0.32, 0.12]
s['body'] = dict(color=[0.045, 0.006, 0.011], rough=0.55, bump=['leather', 42, 0.5])
s['handle'] = dict(w=8.5, h=1.2, color=[0.03, 0.005, 0.008])
s['glow_strips'] = [dict(z=3.45, color=[1.0, 0.42, 0.18], strength=2.2, w=22.9)]
s['rimcolor'] = [1.0, 0.7, 0.5]
s['zones'][0] = dict(type='relief', z=[3.45, 9.5], height='art/dionysus_h.png', holes='art/dionysus_holes.png', depth=0.42, res=58,
                     color=[0.8, 0.55, 0.27], low=[0.02, 0.007, 0.008], lo_at=0.13, hi_at=0.62, metal=0.2, metal_hi=1.0, rough=0.33, grain=[150, 0.03],
                     tubes=[dict(x=x, h=2.6, r=0.38, color=AMB, strength=8, light=55) for x in (-9.8, -7.2, -4.6, 4.6, 7.2, 9.8)],
                     ambient=[dict(x=x, energy=45, color=c, size=5) for x, c in ((-6.5, [0.85, 0.14, 0.18]), (0, [1.0, 0.4, 0.15]), (6.5, [0.85, 0.14, 0.18]))])
json.dump(s, open('amps/dionysus.json', 'w'), indent=1); print('ok')
