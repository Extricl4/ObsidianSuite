"""Art for Chiron (Klon-style transparent overdrive, Centaur OD captures). Writes art/chiron_art.png + art/chiron_glow.png"""
import os, sys, math, random
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'render'))
import art_common as A
sys.path.insert(0, os.path.dirname(__file__))
FD = os.path.join(os.path.dirname(__file__), '..', 'render', 'fonts') + '/'
A.F = {k: FD + os.path.basename(v) for k, v in A.F.items()}
from PIL import ImageDraw, ImageFilter
ART = os.path.join(os.path.dirname(__file__), '..', 'art')
BR = (232, 188, 104, 255)      # aged brass engraving
BRD = (190, 146, 78, 255)
W, H = A.W, A.H
from pedal_art_lib import Plate, ScaledDraw
PL = Plate(); d = ScaledDraw(PL)
gl = A.canvas(); g = ImageDraw.Draw(gl)
PL.rrect((46, 46, W-46, H-46), 60, BR if 'BR' in dir() else GO, 5); PL.rrect((64, 64, W-64, H-64), 46, BR if 'BR' in dir() else GO, 2) #BR, inset=46, r=60, w=5)

# knob labels (3 in a row) + toggle labels
for x, t in ((260, 'DRIVE'), (660, 'BLEND'), (1060, 'OUTPUT')):
    PL.text((x, 790), t, 'cinzel', 40, BR, spacing=6)
PL.text((520, 905), 'MID', 'cinzel', 34, BR, spacing=4)
PL.text((800, 905), 'HIGH', 'cinzel', 34, BR, spacing=4)
PL.text((660, 1005), 'CAPTURE', 'cinzel', 24, BRD, spacing=8)

# Greek meander band
def meander(y, x0=120, x1=1200, s=36, w=5):
    x = x0
    while x + 2*s <= x1:
        pts = [(x, y+s), (x, y), (x+s*1.5, y), (x+s*1.5, y+s*0.66), (x+s*0.6, y+s*0.66), (x+s*0.6, y+s*0.33), (x+s, y+s*0.33)]
        d.line(pts, fill=BR, width=w, joint='curve'); d.line([(x, y+s), (x+2*s, y+s)], fill=BR, width=w)
        x += 2*s
meander(1060); meander(1820)

# Archer's bow + arrow (Chiron, the centaur archer) behind the Centaurus stars
cx, cy = 660, 1450
d.arc((cx-430, cy-330, cx+430, cy+330), 198, 342, fill=BR, width=12)     # bow limb
d.arc((cx-415, cy-315, cx+415, cy+315), 205, 335, fill=BRD, width=3)
lx, rx = cx + 430*math.cos(math.radians(200)), cx + 430*math.cos(math.radians(340))
ly = cy + 330*math.sin(math.radians(200))
d.line([(lx, ly), (cx, cy+120), (rx, ly)], fill=BRD, width=3)          # drawn string
d.line([(cx, cy+120), (cx, cy-270)], fill=BR, width=7)                  # arrow shaft
d.polygon([(cx, cy-330), (cx-34, cy-262), (cx+34, cy-262)], fill=BR)    # arrowhead
for k in range(3):                                                       # fletching
    yy = cy + 80 + k*18
    d.line([(cx, yy), (cx-34, yy+30)], fill=BR, width=4); d.line([(cx, yy), (cx+34, yy+30)], fill=BR, width=4)

# Centaurus-like constellation
stars = [(330, 1250, 14), (450, 1330, 9), (560, 1240, 11), (660, 1300, 16), (780, 1230, 10), (900, 1340, 12), (1000, 1260, 9),
         (600, 1470, 8), (740, 1500, 10), (880, 1460, 8), (470, 1520, 7)]
links = [(0,1),(1,2),(2,3),(3,4),(4,5),(5,6),(3,8),(8,9),(8,7),(7,10)]
for a, b in links:
    d.line([stars[a][:2], stars[b][:2]], fill=(230, 200, 140, 170), width=2)
for x, y, r in stars:
    d.ellipse((x-r, y-r, x+r, y+r), fill=(250, 228, 170, 255))
    g.ellipse((x-r*2.2, y-r*2.2, x+r*2.2, y+r*2.2), fill=(255, 200, 120, 200))
random.seed(7)
for _ in range(60):
    x, y = random.randint(150, 1170), random.randint(1110, 1780)
    if abs(x-cx) < 40: continue
    r = random.choice((2, 2, 3, 4)); d.ellipse((x-r, y-r, x+r, y+r), fill=(225, 200, 150, 200))

# title
PL.text((660, 1660), 'CHIRON', 'cdec', 168, BR, spacing=10)
PL.text((660, 1760), 'TRANSPARENT  OVERDRIVE', 'cinzel', 30, BRD, spacing=7)
PL.text((660, 2290), 'ON', 'cinzel', 30, BR, spacing=6)

gl = gl.filter(ImageFilter.GaussianBlur(9))
g2 = ImageDraw.Draw(gl)
for x, y, r in stars: g2.ellipse((x-r, y-r, x+r, y+r), fill=(255, 230, 170, 255))
PL.save_as('chiron', gl)
print('art ok')
