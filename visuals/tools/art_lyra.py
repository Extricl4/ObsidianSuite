"""Lyra 12-String: burgundy lacquer, gold; a lyre under Vega (the LED)."""
from celestial_art import *
KEY = 'lyra'; INK = (206, 164, 84, 255)
spec = {'enclosure': {'color': [0.28, 0.035, 0.06], 'metal': 0.35, 'rough': 0.28, 'coat': 0.85, 'bump': ['noise', 500, 0.03]},
        'engrave': {'rough': 0.24, 'metal': 1.0, 'depth': 1.0, 'strength': 0.5, 'distance': 0.015}, 'glow_strength': 1.6,
        'knobs': std_knobs('gold', row2=3),
        'leds': [{'at': [660, 1200], 'color': [1.0, 0.9, 0.7], 'strength': 18}],
        'footswitches': [[660, 2190, 1.0]], 'screws': [], 'rimcolor': [1.0, 0.85, 0.7]}
P = Plate(); frame(P, INK)
controls(P, spec, INK, ['OCTAVE', 'DETUNE', 'MIX', 'SPREAD', 'CHIME', 'WIDTH'])
# Vega and the little parallelogram of Lyra
pts = [(660, 1200), (735, 1240), (790, 1300), (760, 1365), (700, 1330)]
P.line(pts[1:] + [pts[1]], INK, 1.4); P.line([pts[0], pts[1]], INK, 1.4)
for x, y in pts[1:]: star(P.d, x * SS, y * SS, 9 * SS, INK, pts=4, inner=0.3)
rays(P, 660, 1200, 26, 64, 16, INK, w0=2.4, w1=0.4, alt=0.55)
lyre(P, 660, 1600, 560, INK, lw=4.2)
for sd in (-1, 1):                                     # laurel sprays at the lyre's foot
    stem = bez((660 + sd * 70, 1860), (660 + sd * 200, 1880), (660 + sd * 330, 1840), (660 + sd * 420, 1760), 40)
    P.taper(stem, INK, 4, 1.2)
    for i in range(3, 38, 5):
        x, y = stem[i]; a = math.atan2(stem[i + 1][1] - stem[i - 1][1], stem[i + 1][0] - stem[i - 1][0])
        for s2 in (-1, 1):
            la = a + s2 * 0.75; L = 40 * (1 - i / 70)
            leaf(P, (x, y), (x + math.cos(la) * L, y + math.sin(la) * L), INK, L * 0.42, bend=s2 * 0.2)
title(P, 'LYRA', '12-STRING SHIMMER', INK, y=1985, size=140, spacing=40)
P.save_as(KEY, glow_spots(W, H, [(660, 1200, 70, (255, 230, 180, 140)), (660, 1200, 22, (255, 245, 220, 240))], 20)); write_spec(KEY, spec)
