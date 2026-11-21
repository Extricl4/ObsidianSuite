"""Maw v2: Grind-inspired single-knob high-gain drive. Gloss-black engraving cut into blued gunmetal."""
from pedal_art_lib import *
INK = (6, 6, 8, 255)
P = Plate()
bead_border(P, (64, 64, W-64, H-64), 40, INK, w=5)
cx, cy = 660, 560
# the maw: ring of fangs around the big GAIN knob (r~230px)
P.circle(cx, cy, 300, INK, 13); P.circle(cx, cy, 326, INK, 3); P.circle(cx, cy, 340, INK, 1.5)
for k in range(24):
    a = 2*math.pi*k/24 + math.pi/24; L = 82 if k % 2 == 0 else 50
    pts = [(cx + 300*math.cos(a-0.1), cy + 300*math.sin(a-0.1))]
    pts += bez((cx + 300*math.cos(a-0.1), cy + 300*math.sin(a-0.1)), (cx + (300-L*.6)*math.cos(a-0.06), cy + (300-L*.6)*math.sin(a-0.06)),
               (cx + (300-L)*math.cos(a+0.01), cy + (300-L)*math.sin(a+0.01)), (cx + (300-L)*math.cos(a+0.015), cy + (300-L)*math.sin(a+0.015)), 12)
    pts += [(cx + 300*math.cos(a+0.1), cy + 300*math.sin(a+0.1))]
    P.poly(pts, INK)
for k in range(72):   # fine engraved rays behind the maw
    a = 2*math.pi*k/72; r1 = 440 if k % 2 == 0 else 395
    if k % 6 == 0: r1 = 470
    if math.radians(48) < a < math.radians(132): continue
    P.taper([(cx + (350 + (r1-350)*t)*math.cos(a), cy + (350 + (r1-350)*t)*math.sin(a)) for t in (i/10 for i in range(11))], INK, 3.2, 0.6)
P.text((660, 960), 'GAIN', 'cinzel', 46, INK, spacing=16)
P.text((505, 1110), 'SLAM', 'cinzel', 28, INK, spacing=5); P.text((815, 1110), 'KODY', 'cinzel', 28, INK, spacing=5)
# gothic arch window around the LED (660,1300)
ey = 1300
arch = bez((cx-170, ey+170), (cx-170, ey-60), (cx-110, ey-170), (cx, ey-230), 30) + bez((cx, ey-230), (cx+110, ey-170), (cx+170, ey-60), (cx+170, ey+170), 30)
P.line(arch + [(cx-170, ey+170)], INK, 8)
arch2 = [(cx + (x-cx)*0.84, ey + (y-ey)*0.84 + 14) for x, y in arch]; P.line(arch2, INK, 2.5)
P.circle(cx, ey, 80, INK, 8); P.circle(cx, ey, 98, INK, 2)
for k in range(12):
    a = 2*math.pi*k/12
    P.line([(cx + 104*math.cos(a), ey + 104*math.sin(a)), (cx + 140*math.cos(a), ey + 140*math.sin(a))], INK, 3)
for s in (-1, 1):
    rinceau(P, bez((cx + s*175, ey + 120), (cx + s*330, ey + 150), (cx + s*450, ey + 40), (cx + s*420, ey - 110), 50), INK, flip=-s, w0=11, curl=70, leaf_len=70, leaf_every=9, thorns=2, tendrils=1, seed=24)
# title
P.text((660, 1650), 'MAW', 'cdec', 230, INK, spacing=30)
for s in (-1, 1): P.taper(bez((660 + s*190, 1800), (660 + s*320, 1792), (660 + s*420, 1808), (660 + s*500, 1795), 30), INK, 5, 1)
P.text((660, 1800), 'GRINDER', 'cinzel', 28, INK, spacing=12)
for s in (-1, 1):
    rinceau(P, bez((660 + s*150, 2190), (660 + s*370, 2210), (660 + s*480, 2060), (660 + s*400, 1950), 60), INK, flip=-s, w0=15, curl=105, leaf_len=90, leaf_every=10, thorns=3, seed=27)
P.text((660, 2290), 'OBSIDIAN', 'cinzel', 26, INK, spacing=14)
gl = A.canvas(); g = ImageDraw.Draw(gl); g.ellipse((600, 1240, 720, 1360), fill=(255, 30, 20, 150)); gl = gl.filter(ImageFilter.GaussianBlur(25))
P.save_as('maw', gl)
