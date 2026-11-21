"""Helios: SD-1 captures (Boost I-III, Drive I-VI). Sun-yellow lacquer, dark bronze engraving."""
from pedal_art_lib import *
from pedal_art_lib import _ang
INK = (52, 32, 10, 255)
P = Plate()
bead_border(P, (64, 64, W-64, H-64), 40, INK, w=5)
P.text((250, 760), 'LEVEL', 'cinzel', 36, INK, spacing=8); P.text((1070, 760), 'DRIVE', 'cinzel', 36, INK, spacing=8)
# 9-way capture selector, center knob at (660,560)
labels = ['B I', 'B II', 'B III', 'D I', 'D II', 'D III', 'D IV', 'D V', 'D VI']
for i, t in enumerate(labels):
    a = math.radians(135 + 270*i/8)
    P.line([(660 + 115*math.cos(a), 560 + 115*math.sin(a)), (660 + 135*math.cos(a), 560 + 135*math.sin(a))], INK, 4)
    P.text((660 + 180*math.cos(a), 560 + 180*math.sin(a)), t, 'cinzel', 22, INK)
P.text((660, 790), 'VOICE', 'cinzel', 28, INK, spacing=8)
# the sun: disc + engraved flame rays around the LED (660,1300)
cx, cy = 660, 1290
for k in range(48):
    a = 2*math.pi*k/48
    if k % 2 == 0:   # wavy flame rays
        pts = [(cx + (150 + 240*t)*math.cos(a + 0.06*math.sin(t*9)), cy + (150 + 240*t)*math.sin(a + 0.06*math.sin(t*9))) for t in (i/30 for i in range(31))]
        P.taper(pts, INK, 9, 0.8)
    else:
        P.taper([(cx + (150 + 150*t)*math.cos(a), cy + (150 + 150*t)*math.sin(a)) for t in (i/10 for i in range(11))], INK, 5, 0.6)
P.circle(cx, cy, 138, INK, 9); P.circle(cx, cy, 118, INK, 2.5)
hatch_circle(P, cx, cy, 112, INK, gap=9, w=1.6)
P.circle(cx, cy, 70, CLEAR); P.circle(cx, cy, 72, INK, 5)
# laurel branches under the sun
for s in (-1, 1):
    stem = bez((cx + s*60, cy + 300), (cx + s*250, cy + 300), (cx + s*400, cy + 210), (cx + s*450, cy + 20), 60)
    P.taper(stem, INK, 7, 2)
    for i in range(4, 58, 6):
        x, y = stem[i]; aa = _ang(stem, i)
        for sd in (-1, 1):
            la = aa + sd*0.7; L = 62*(1 - i/90)
            leaf(P, (x, y), (x + math.cos(la)*L, y + math.sin(la)*L), INK, L*0.4, bend=sd*0.2, veins=True)
P.text((660, 1790), 'HELIOS', 'cdec', 165, INK, spacing=14)
P.text((660, 1895), 'SOLAR  OVERDRIVE', 'cinzel', 28, INK, spacing=10)
for s in (-1, 1): P.taper(bez((660 + s*330, 1895), (660 + s*390, 1891), (660 + s*450, 1899), (660 + s*510, 1895), 20), INK, 4, 1)
gl = A.canvas(); g = ImageDraw.Draw(gl); g.ellipse((cx-150, cy-150, cx+150, cy+150), outline=(255, 190, 90, 140), width=22); gl = gl.filter(ImageFilter.GaussianBlur(18))
P.save_as('helios', gl)
