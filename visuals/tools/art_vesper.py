"""Vesper Choir: midnight indigo, silver; three choir windows, the evening star in the middle one."""
from celestial_art import *
KEY = 'vesper'; INK = (206, 213, 230, 255)
spec = {'enclosure': {'color': [0.06, 0.07, 0.19], 'metal': 0.55, 'rough': 0.35, 'coat': 0.5, 'bump': ['noise', 500, 0.04]},
        'engrave': {'rough': 0.25, 'metal': 1.0, 'depth': 1.0, 'strength': 0.5, 'distance': 0.015}, 'glow_strength': 1.6,
        'knobs': std_knobs('alu'), 'toggles': [[660, 1100, -18]],
        'leds': [{'at': [660, 1480], 'color': [0.8, 0.85, 1.0], 'strength': 16}],
        'footswitches': [[660, 2190, 1.0]], 'screws': [], 'rimcolor': [0.75, 0.8, 1.0]}
spec['knobs'][3]['selector'] = True
P = Plate(); frame(P, INK)
controls(P, spec, INK, ['DECAY', 'SIZE', 'MIX', 'VOWEL', 'CHOIR', 'BREATH', 'TONE'])
selector_labels(P, 200, 860, 0.38, ['A', 'E', 'I', 'O', 'U'], INK, size=22, start=180, span=180, gap=24)
label(P, 660, 1180, 'UNISON  ·  OCTAVE  ·  FIFTH', INK, 19, spacing=4)
star_field(P, (110, 1230, 1210, 1870), 70, INK, seed=8, avoid=((660, 1480, 90), (420, 1700, 110), (900, 1700, 110), (660, 1290, 70)))
for x, w, h in ((420, 170, 380), (900, 170, 380), (660, 220, 500)):
    P.poly(pointed_arch(x, 1860, w + 20, h + 14), CLEAR)
    lancet(P, x, 1860, w, h, INK, lw=3.4)
P.line([(250, 1868), (1070, 1868)], INK, 3); P.line([(270, 1882), (1050, 1882)], INK, 1.2)
crescent(P, 660, 1290, 46, INK, phase=0.38, rot=-160)
title(P, 'VESPER', 'CHOIR REVERB', INK, y=1975, size=118)
P.save_as(KEY, glow_spots(W, H, [(660, 1480, 90, (200, 215, 255, 130)), (660, 1480, 26, (235, 240, 255, 230))], 22)); write_spec(KEY, spec)
