"""Nimbus Cloud: sky-blue enclosure, white enamel; layered clouds under a glowing moon."""
from celestial_art import *
KEY = 'nimbus'; INK = (247, 249, 255, 255)
spec = {'enclosure': {'color': [0.30, 0.45, 0.68], 'metal': 0.25, 'rough': 0.3, 'coat': 0.7, 'bump': ['noise', 500, 0.03]},
        'engrave': {'rough': 0.4, 'metal': 0.0, 'depth': 1.0, 'strength': 0.5, 'distance': 0.015}, 'glow_strength': 1.6,
        'knobs': std_knobs('alu'), 'toggles': [[660, 1110, -18]],
        'leds': [{'at': [860, 1330], 'color': [0.85, 0.92, 1.0], 'strength': 16}],
        'footswitches': [[660, 2190, 1.0]], 'screws': [], 'rimcolor': [0.85, 0.92, 1.0], 'key': 2100}
P = Plate(); frame(P, INK)
controls(P, spec, INK, ['DECAY', 'CLOUD', 'MIX', 'DRIFT', 'SWELL', 'TONE', 'LOW CUT'])
label(P, 660, 1195, 'FREEZE', INK, 24, spacing=7)
star_field(P, (120, 1230, 1200, 1700), 60, INK, seed=5, avoid=((860, 1330, 110),))
crescent(P, 860, 1330, 78, INK, phase=0.36, rot=-140)
cloud(P, 470, 1560, 560, INK, seed=2, bumps=5)
cloud(P, 860, 1650, 520, INK, seed=4, bumps=4)
cloud(P, 600, 1790, 760, INK, seed=7, bumps=6)
title(P, 'NIMBUS', 'CLOUD REVERB', INK, y=1975, size=118)
P.save_as(KEY, glow_spots(W, H, [(860, 1330, 110, (220, 235, 255, 120)), (860, 1330, 30, (240, 248, 255, 220))], 26)); write_spec(KEY, spec)
