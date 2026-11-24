"""Astral Shimmer Delay: midnight violet, silver; an astrolabe with the LED at its pole."""
from celestial_art import *
KEY = 'astral'; INK = (210, 214, 232, 255)
spec = {'enclosure': {'color': [0.10, 0.07, 0.20], 'metal': 0.6, 'rough': 0.3, 'coat': 0.4, 'bump': ['noise', 500, 0.04]},
        'engrave': {'rough': 0.25, 'metal': 1.0, 'depth': 1.0, 'strength': 0.5, 'distance': 0.015}, 'glow_strength': 1.6,
        'knobs': std_knobs('alu') + [{'at': [660, 1170], 'r': 0.3, 'style': 'alu', 'v': 0.0, 'selector': True}],
        'leds': [{'at': [660, 1600], 'color': [0.75, 0.8, 1.0], 'strength': 16}],
        'footswitches': [[660, 2190, 1.0]], 'screws': [], 'rimcolor': [0.8, 0.75, 1.0]}
P = Plate(); frame(P, INK)
controls(P, spec, INK, ['TIME', 'FEEDBACK', 'MIX', 'SHIMMER', 'DIFFUSE', 'MOD', 'TONE', None])
selector_labels(P, 660, 1170, 0.3, ['+OCT', '+5TH', '+2 OCT', '-OCT'], INK, size=19, start=180, span=180, gap=34)
label(P, 660, 1266, 'INTERVAL', INK, 20, spacing=6)
star_field(P, (110, 1250, 1210, 1880), 55, INK, seed=11, avoid=((660, 1600, 320), (660, 1170, 140)))
astrolabe(P, 660, 1600, 290, INK)
title(P, 'ASTRAL', 'SHIMMER DELAY', INK, y=1985, size=118)
P.save_as(KEY, glow_spots(W, H, [(660, 1600, 80, (190, 200, 255, 130)), (660, 1600, 24, (230, 235, 255, 230))], 22)); write_spec(KEY, spec)
