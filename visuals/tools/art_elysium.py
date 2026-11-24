"""Elysium Hall: pearl enclosure, gold engraving, a cathedral rose window around the LED."""
from celestial_art import *
KEY = 'elysium'
INK = (190, 146, 66, 255)
spec = {
 'enclosure': {'color': [0.87, 0.85, 0.81], 'metal': 0.3, 'rough': 0.22, 'coat': 0.9, 'bump': ['noise', 500, 0.03]},
 'engrave': {'rough': 0.24, 'metal': 1.0, 'depth': 1.0, 'strength': 0.5, 'distance': 0.015},
 'glow_strength': 1.4,
 'knobs': [{'at': [260, 470], 'r': 0.6, 'style': 'gold', 'v': 0.45}, {'at': [660, 470], 'r': 0.6, 'style': 'gold', 'v': 0.7}, {'at': [1060, 470], 'r': 0.6, 'style': 'gold', 'v': 0.35},
           {'at': [200, 860], 'r': 0.38, 'style': 'gold', 'v': 0.3}, {'at': [507, 860], 'r': 0.38, 'style': 'gold', 'v': 0.5}, {'at': [813, 860], 'r': 0.38, 'style': 'gold', 'v': 0.25}, {'at': [1120, 860], 'r': 0.38, 'style': 'gold', 'v': 0.5}],
 'toggles': [[660, 1110, -18]],
 'leds': [{'at': [660, 1540], 'color': [1.0, 0.86, 0.6], 'strength': 18}],
 'footswitches': [[660, 2190, 1.0]],
 'screws': [], 'rimcolor': [1.0, 0.92, 0.8],
}
P = Plate()
frame(P, INK)
controls(P, spec, INK, ['DECAY', 'SIZE', 'MIX', 'PRE-DELAY', 'TONE', 'LOW CUT', 'MOD'])
label(P, 660, 1195, 'INFINITE', INK, 24, spacing=7)
for s in (-1, 1): P.line([(660 + s * 70, 1110), (660 + s * 180, 1110)], INK, 1.6); P.circle(660 + s * 190, 1110, 4, INK)
rose_window(P, 660, 1540, 300, INK)
# laurel sprigs around the title
P.text((660, 1950), 'ELYSIUM', 'marc', 118, INK, spacing=22)
P.text((660, 2045), 'CATHEDRAL  HALL', 'marc', 26, INK, spacing=12)
for s in (-1, 1):
    stem = bez((660 + s * 250, 2045), (660 + s * 330, 2052), (660 + s * 420, 2030), (660 + s * 470, 1990), 30)
    P.taper(stem, INK, 4, 1.2)
    for i in range(3, 28, 5):
        x, y = stem[i]; a = math.atan2(stem[i + 1][1] - stem[i - 1][1], stem[i + 1][0] - stem[i - 1][0])
        for sd in (-1, 1):
            la = a + sd * 0.75; L = 34 * (1 - i / 60)
            leaf(P, (x, y), (x + math.cos(la) * L, y + math.sin(la) * L), INK, L * 0.42, bend=sd * 0.2)
P.save_as(KEY, glow_spots(W, H, [(660, 1540, 70, (255, 220, 150, 120)), (660, 1540, 26, (255, 240, 210, 220))], 22))
write_spec(KEY, spec)
