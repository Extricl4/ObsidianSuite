"""Gemini Dual Delay: brushed silver, black enamel; the twins and their echoes rippling apart."""
from celestial_art import *
KEY = 'gemini'; INK = (22, 22, 26, 255)
spec = {'enclosure': {'color': [0.74, 0.75, 0.77], 'metal': 1.0, 'rough': 0.32, 'bump': ['noise', 700, 0.04]},
        'engrave': {'rough': 0.35, 'metal': 0.0, 'depth': 1.0, 'strength': 0.5, 'distance': 0.015}, 'glow_strength': 1.4,
        'knobs': std_knobs('black', row2=3) + [{'at': [420, 1170], 'r': 0.3, 'style': 'black', 'v': 0.0, 'selector': True}],
        'toggles': [[900, 1170, -18]],
        'leds': [{'at': [660, 1620], 'color': [1.0, 0.25, 0.2], 'strength': 22}],
        'footswitches': [[660, 2190, 1.0]], 'screws': [], 'rimcolor': [1.0, 1.0, 1.0]}
P = Plate(); frame(P, INK)
controls(P, spec, INK, ['TIME', 'FEEDBACK', 'MIX', 'CROSS', 'TONE', 'MOD', None])
selector_labels(P, 420, 1170, 0.3, ['DOT', 'HALF', 'TRIP', '1/3', '1.5X', 'GOLD'], INK, size=17, start=150, span=240, gap=30)
label(P, 420, 1272, 'PATTERN', INK, 20, spacing=6); label(P, 900, 1272, 'REVERSE', INK, 20, spacing=6)
cx, cy = 660, 1620
for sd in (-1, 1):                                     # echoes rippling away from each twin
    ox = cx + sd * 130
    for k in range(6):
        r = 170 + 46 * k
        a0, a1 = (150, 210) if sd < 0 else (-30, 30)
        P.line(arc(ox, cy, r, a0 - k * 2, a1 + k * 2, 30), INK, 3.2 - 0.4 * k)
    P.circle(ox, cy, 140, INK, 3.2); P.circle(ox, cy, 126, INK, 1.1)
# the glyph between them: two pillars with the arched lintels
P.taper(arc(cx, cy - 92 + 300, 300, 258, 282, 30), INK, 3, 3, 6)        # top lintel (bows up)
P.taper(arc(cx, cy + 92 - 300, 300, 78, 102, 30), INK, 3, 3, 6)         # bottom lintel (bows down)
for x in (cx - 40, cx + 40): P.taper([(x, cy - 82), (x, cy + 82)], INK, 6, 6, 4)
for sd in (-1, 1): star(P.d, (cx + sd * 130) * SS, (cy - 215) * SS, 18 * SS, INK, pts=4, inner=0.25)
P.text((cx - 130, cy - 255), 'CASTOR', 'marc', 18, INK, spacing=4); P.text((cx + 130, cy - 255), 'POLLUX', 'marc', 18, INK, spacing=4)
title(P, 'GEMINI', 'DUAL DELAY', INK, y=1985, size=118)
P.save_as(KEY, glow_spots(W, H, [(cx, cy, 60, (255, 80, 60, 110))], 20)); write_spec(KEY, spec)
