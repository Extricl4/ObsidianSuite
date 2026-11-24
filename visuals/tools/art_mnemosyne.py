"""Mnemosyne Tape Echo: wide vintage unit, racing-green lacquer, brass; two tape reels and three heads."""
from celestial_art import *
KEY = 'mnemosyne'; INK = (200, 162, 92, 255)
PW, PH = 2000, 1600
spec = {'size': [10.0, 8.0, 3.2], 'px': 200, 'res': [1170, 936], 'lens': 40, 'cam': [0, -8.6, 19.4], 'target': [0, -0.3, 1.6], 'bevel': 0.3,
        'enclosure': {'color': [0.08, 0.16, 0.10], 'metal': 0.35, 'rough': 0.38, 'coat': 0.55, 'bump': ['noise', 600, 0.04]},
        'engrave': {'rough': 0.26, 'metal': 1.0, 'depth': 1.0, 'strength': 0.5, 'distance': 0.015}, 'glow_strength': 1.4,
        'knobs': [{'at': [x, 330], 'r': 0.5, 'style': 'cream', 'v': v} for x, v in ((260, 0.4), (580, 0.45), (1420, 0.45), (1740, 0.55))]
                 + [{'at': [1000, 470], 'r': 0.68, 'style': 'cream', 'v': 0.5, 'h': 0.6, 'selector': True}]
                 + [{'at': [x, 780], 'r': 0.4, 'style': 'cream', 'v': v} for x, v in ((260, 0.3), (580, 0.35), (1420, 0.25))],
        'leds': [{'at': [1580, 1190], 'color': [1.0, 0.35, 0.2], 'strength': 24}],
        'footswitches': [[1000, 1415, 1.05]], 'screws': [], 'rimcolor': [0.9, 1.0, 0.85]}
P = Plate(PW, PH); frame(P, INK, inset=50, r=40)
controls(P, spec, INK, ['TIME', 'INTENSITY', 'ECHO', 'TONE', None, 'WOW', 'AGE', 'SPRING'])
selector_labels(P, 1000, 470, 0.68, ['1', '2', '3', '1+2', '2+3', '1+3', '1+2+3'], INK, size=24, gap=36)
label(P, 1000, 712, 'HEADS', INK, 34, spacing=8)
for x in (420, 1580): reel(P, x, 1190, 190, INK)
# tape path over the reels and across the three heads
P.line([(420 - 190 * 0.25, 1190 - 186), (860 - 40, 1040)], INK, 3); P.line([(1580 + 190 * 0.25, 1190 - 186), (1140 + 40, 1040)], INK, 3)
P.line([(820, 1040), (1180, 1040)], INK, 3)
for i, x in enumerate((880, 1000, 1120)):
    P.rrect((x - 40, 1050, x + 40, 1120), 10, INK, 3)
    P.line([(x - 22, 1085), (x + 22, 1085)], INK, 1.4)
    P.text((x, 1150), ['I', 'II', 'III'][i], 'marc', 24, INK)
P.text((1000, 128), 'MNEMOSYNE', 'marc', 58, INK, spacing=16)
tw = text_width(P, 'MNEMOSYNE', 'marc', 58, 16)
meander(P, 112, 120, 1000 - tw / 2 - 40, INK, s=20); meander(P, 112, 1000 + tw / 2 + 40, 1880, INK, s=20)
P.save_as(KEY, glow_spots(PW, PH, [(1580, 1190, 50, (255, 120, 80, 120))], 18)); write_spec(KEY, spec)
