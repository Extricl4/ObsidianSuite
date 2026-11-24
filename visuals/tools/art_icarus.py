"""Icarus Octave Fuzz: burnt copper, black enamel; the sun (LED) above falling wings and feathers."""
from celestial_art import *
KEY = 'icarus'; INK = (18, 12, 10, 255)
spec = {'enclosure': {'color': [0.52, 0.22, 0.10], 'metal': 0.85, 'rough': 0.38, 'bump': ['noise', 500, 0.06]},
        'engrave': {'rough': 0.4, 'metal': 0.0, 'depth': 1.0, 'strength': 0.55, 'distance': 0.015}, 'glow_strength': 1.6,
        'knobs': [{'at': [380, 470], 'r': 0.64, 'style': 'black', 'v': 0.65}, {'at': [940, 470], 'r': 0.64, 'style': 'black', 'v': 0.7}]
                 + [{'at': [x, 860], 'r': 0.38, 'style': 'black', 'v': v} for x, v in ((260, 0.45), (660, 0.5), (1060, 1.0))],
        'leds': [{'at': [660, 1290], 'color': [1.0, 0.55, 0.15], 'strength': 26}],
        'footswitches': [[660, 2190, 1.0]], 'screws': [], 'rimcolor': [1.0, 0.7, 0.45]}
P = Plate(); frame(P, INK)
controls(P, spec, INK, ['FUZZ', 'OCTAVE', 'TONE', 'LEVEL', 'MIX'])
sun(P, 660, 1290, 84, INK, n=28)
for sd in (-1, 1): wing(P, (660 + sd * 34, 1650), sd, 300, INK, lw=1.7, up=0.95)
rnd = random.Random(6)
for k in range(9):                                     # feathers coming loose, drifting down
    x = rnd.choice((rnd.uniform(160, 360), rnd.uniform(960, 1160))); y = rnd.uniform(1450, 1860)
    a = rnd.uniform(60, 120) + (30 if x > 660 else -30); L = rnd.uniform(70, 120)
    feather(P, (x, y), pol(x, y, L, a), L * 0.11, INK, bend=0.15, barbs=7, lw=1.3)
title(P, 'ICARUS', 'OCTAVE FUZZ', INK, y=1985, size=118)
P.save_as(KEY, glow_spots(W, H, [(660, 1290, 140, (255, 150, 60, 120)), (660, 1290, 34, (255, 210, 140, 240))], 26)); write_spec(KEY, spec)
