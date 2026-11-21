"""Art for Emperor (tarot-card styled 3-mode drive; Duke of Tone captures BST/OD/DIST). Writes art/emperor_art.png + art/emperor_glow.png"""
import os, sys, math
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'render'))
import art_common as A
sys.path.insert(0, os.path.dirname(__file__))
FD = os.path.join(os.path.dirname(__file__), '..', 'render', 'fonts') + '/'
A.F = {k: FD + os.path.basename(v) for k, v in A.F.items()}
from PIL import ImageDraw, ImageFilter
ART = os.path.join(os.path.dirname(__file__), '..', 'art')
GO = (222, 178, 92, 255); GOD = (170, 128, 60, 255)
INK = (38, 24, 36, 255); CREAM = (234, 222, 194, 255); RED = (140, 30, 38, 255)
from pedal_art_lib import Plate, ScaledDraw, W, H
PL = Plate(); d = ScaledDraw(PL); gl = A.canvas(); g = ImageDraw.Draw(gl)
PL.rrect((46, 46, W-46, H-46), 60, BR if 'BR' in dir() else GO, 5); PL.rrect((64, 64, W-64, H-64), 46, BR if 'BR' in dir() else GO, 2) #GO, inset=46, r=60, w=5)

# top controls: labels
PL.text((330, 760), 'DRIVE', 'cinzel', 40, GO, spacing=6)
PL.text((990, 760), 'VOLUME', 'cinzel', 40, GO, spacing=6)
for x, t in ((560, 'BST'), (660, 'OD'), (760, 'DIST')):
    PL.text((x, 400), t, 'cinzel', 30, GO, spacing=3)
# little moons/stars between labels
for x in (130, 1190):
    d.ellipse((x-14, 560-14, x+14, 560+14), outline=GO, width=3)

# the card
L, T, R, B = 170, 840, 1150, 1975
d.rounded_rectangle((L, T, R, B), 34, fill=CREAM)
d.rounded_rectangle((L+16, T+16, R-16, B-16), 24, outline=INK, width=5)
d.rounded_rectangle((L+30, T+30, R-30, B-30), 18, outline=GOD, width=2)
PL.text((660, T+95), 'IV', 'cinzel', 70, INK, spacing=10)
cx, cy = 660, 1390
# sun / rays behind
for k in range(36):
    a = math.radians(k*10); r0, r1 = 210, 330 if k % 2 == 0 else 280
    d.line([(cx+r0*math.cos(a), cy+r0*math.sin(a)), (cx+r1*math.cos(a), cy+r1*math.sin(a))], fill=GOD, width=4)
d.ellipse((cx-200, cy-200, cx+200, cy+200), fill=(226, 196, 140, 255), outline=INK, width=5)
# throne back
d.rounded_rectangle((cx-140, cy-130, cx+140, cy+190), 20, fill=RED, outline=INK, width=5)
# ram horns on throne arms (spirals)
def spiral(x0, y0, s, flip):
    pts = []
    for i in range(160):
        t = i/160*3.2*math.pi; r = s*(1 - i/190)
        pts.append((x0 + flip*r*math.cos(t), y0 + r*math.sin(t)))
    d.line(pts, fill=INK, width=6, joint='curve')
spiral(cx-185, cy+150, 52, -1); spiral(cx+185, cy+150, 52, 1)
# crown
cr = [(cx-110, cy-60), (cx-110, cy-150), (cx-60, cy-100), (cx, cy-180), (cx+60, cy-100), (cx+110, cy-150), (cx+110, cy-60)]
d.polygon(cr, fill=GO, outline=INK); d.line(cr + [cr[0]], fill=INK, width=5)
for x, y in ((cx-110, cy-160), (cx, cy-192), (cx+110, cy-160)):
    d.ellipse((x-13, y-13, x+13, y+13), fill=RED, outline=INK, width=3)
# sceptre (ankh) and orb
sx = cx - 70
d.line([(sx, cy-20), (sx, cy+175)], fill=INK, width=8)
d.ellipse((sx-24, cy-70, sx+24, cy-18), outline=INK, width=7); d.line([(sx-34, cy-18), (sx+34, cy-18)], fill=INK, width=8)
ox, oy = cx + 70, cy + 70
d.ellipse((ox-38, oy-38, ox+38, oy+38), fill=GO, outline=INK, width=5)
d.line([(ox, oy-38), (ox, oy-70)], fill=INK, width=6); d.line([(ox-16, oy-58), (ox+16, oy-58)], fill=INK, width=6)
# corner stars
def star(x, y, r, col):
    pts = [(x + (r if i % 2 == 0 else r*0.42)*math.cos(math.radians(-90 + i*36)), y + (r if i % 2 == 0 else r*0.42)*math.sin(math.radians(-90 + i*36))) for i in range(10)]
    d.polygon(pts, fill=col)
for x, y in ((L+90, T+90), (R-90, T+90), (L+90, B-230), (R-90, B-230)): star(x, y, 26, INK)
# banner
by = B - 120
d.polygon([(L+70, by-48), (R-70, by-48), (R-30, by), (R-70, by+48), (L+70, by+48), (L+30, by)], fill=CREAM, outline=INK)
d.line([(L+70, by-48), (R-70, by-48), (R-30, by), (R-70, by+48), (L+70, by+48), (L+30, by), (L+70, by-48)], fill=INK, width=5)
PL.text((660, by+2), 'THE  EMPEROR', 'cinzel', 64, INK, spacing=8)
PL.text((660, 2290), 'ON', 'cinzel', 30, GO, spacing=6)
# glow: just the sun's rim, very soft
g.ellipse((cx-215, cy-215, cx+215, cy+215), outline=(255, 190, 110, 160), width=18)
gl = gl.filter(ImageFilter.GaussianBlur(14))
PL.save_as('emperor', gl); print('art ok')
