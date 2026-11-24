"""Empyrean Shimmer: champagne gold, ivory enamel; a halo with angel wings around the LED."""
from celestial_art import *
KEY = 'empyrean'; INK = (246, 238, 220, 255)
spec = {'enclosure': {'color': [0.80, 0.63, 0.35], 'metal': 0.85, 'rough': 0.3, 'coat': 0.4, 'bump': ['noise', 500, 0.04]},
        'engrave': {'rough': 0.45, 'metal': 0.0, 'depth': 1.0, 'strength': 0.5, 'distance': 0.015}, 'glow_strength': 1.5,
        'knobs': std_knobs('pearl') + [{'at': [660, 1170], 'r': 0.3, 'style': 'pearl', 'v': 0.0, 'selector': True}],
        'leds': [{'at': [660, 1530], 'color': [1.0, 0.9, 0.7], 'strength': 18}],
        'footswitches': [[660, 2190, 1.0]], 'screws': [], 'rimcolor': [1.0, 0.9, 0.75]}
P = Plate(); frame(P, INK)
controls(P, spec, INK, ['DECAY', 'SIZE', 'MIX', 'SHIMMER', 'VOICE II', 'TONE', 'MOD', None])
selector_labels(P, 660, 1170, 0.3, ['5TH', '4TH', '2 OCT', 'SUB', '3RD'], INK, size=19, start=180, span=180, gap=30)
label(P, 660, 1268, 'INTERVAL', INK, 20, spacing=6)
cx, cy = 660, 1530
for sd in (-1, 1): wing(P, (cx + sd * 92, cy + 10), sd, 370, INK, lw=1.8, up=1.15)
P.circle(cx, cy, 128, CLEAR)
rays(P, cx, cy, 104, 175, 48, INK, w0=3.4, w1=0.6, alt=0.55)
P.circle(cx, cy, 92, INK, 5); P.circle(cx, cy, 78, INK, 1.5); P.circle(cx, cy, 104, INK, 1.2)
title(P, 'EMPYREAN', 'TWO-VOICE SHIMMER', INK, size=100, spacing=18)
P.save_as(KEY, glow_spots(W, H, [(cx, cy, 120, (255, 236, 190, 110)), (cx, cy, 30, (255, 245, 220, 220))], 24)); write_spec(KEY, spec)
