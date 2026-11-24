"""Halo Reverse: matte black, gold; an eclipse corona with the diamond-ring flash as the LED."""
from celestial_art import *
KEY = 'halo'; INK = (226, 186, 102, 255)
spec = {'enclosure': {'color': [0.022, 0.022, 0.026], 'metal': 0.2, 'rough': 0.75, 'bump': ['noise', 500, 0.08]},
        'engrave': {'rough': 0.42, 'metal': 0.85, 'depth': 1.0, 'strength': 0.6, 'distance': 0.015}, 'glow_strength': 1.3,
        'knobs': std_knobs('gold', row2=3), 'toggles': [[660, 1110, -18]],
        'leds': [{'at': [767, 1413], 'color': [1.0, 0.88, 0.6], 'strength': 22}],
        'footswitches': [[660, 2190, 1.0]], 'screws': [], 'rimcolor': [1.0, 0.85, 0.6]}
P = Plate(); frame(P, INK)
controls(P, spec, INK, ['LENGTH', 'DECAY', 'MIX', 'TONE', 'MOD', 'LOW CUT'])
label(P, 470, 1112, 'REVERSE', INK, 22, spacing=5); label(P, 850, 1112, 'BLOOM', INK, 22, spacing=5)
cx, cy, R = 660, 1520, 150
rnd = random.Random(4)
for k in range(150):                                   # the corona: fine streamers of every length
    a = 360 * k / 150 + rnd.uniform(-0.6, 0.6)
    L = R + 30 + rnd.uniform(40, 200) * (1.2 if (k % 25) < 6 else 0.9)
    P.taper([pol(cx, cy, R + 14 + (L - R - 14) * t, a + 2.5 * math.sin(t * 3 + k)) for t in (i / 16 for i in range(17))], INK, 2.4, 0.4)
P.circle(cx, cy, R, INK, 4); P.circle(cx, cy, R - 12, INK, 1.2)
for rr, seg in ((R + 262, 72), (R + 280, 120)):      # two faint dashed rings: the outer halo
    for k in range(seg):
        if k % 2 == 0: P.line(arc(cx, cy, rr, 360 * k / seg, 360 * (k + 1) / seg, 4), INK, 1.2)
lx, ly = pol(cx, cy, R, -45)                           # diamond ring: the flash at the edge
for a, L in ((0, 90), (90, 90), (45, 46), (135, 46), (180, 90), (270, 90), (225, 46), (315, 46)):
    P.taper([pol(lx, ly, 14 + L * t, a) for t in (i / 8 for i in range(9))], INK, 4, 0.5)
title(P, 'HALO', 'REVERSE  ·  BLOOM', INK, y=1975, size=150, spacing=46)
P.save_as(KEY, glow_spots(W, H, [(lx, ly, 70, (255, 225, 160, 150)), (lx, ly, 22, (255, 245, 220, 255))], 22)); write_spec(KEY, spec)
