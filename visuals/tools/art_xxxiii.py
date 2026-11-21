"""XXXIII: Fortin 33 capture. Matte black, gloss-black geometric engraving, Michroma lettering."""
from pedal_art_lib import *
INK = (5, 5, 7, 255)
PL = Plate()
class _D:
    def __getattr__(self, n):
        f = getattr(PL.d, n)
        def g(xy, *a, **k):
            if 'width' in k: k['width'] = int(k['width']*SS)
            if isinstance(xy, (list, tuple)) and xy and isinstance(xy[0], (list, tuple)): xy = [(x*SS, y*SS) for x, y in xy]
            else: xy = tuple(v*SS for v in xy)
            return f(xy, *a, **k)
        return g
d = _D(); gl = A.canvas()
import functools
# angular frame with clipped corners
c = 70; m = 70
fr = [(m+c, m), (W-m-c, m), (W-m, m+c), (W-m, H-m-c), (W-m-c, H-m), (m+c, H-m), (m, H-m-c), (m, m+c), (m+c, m)]
d.line(fr, fill=INK, width=9)
fr2 = [(x + (14 if x < W/2 else -14), y + (14 if y < H/2 else -14)) for x, y in fr]
d.line(fr2, fill=INK, width=3)
# knob labels
PL.text((380, 735), 'DRIVE', 'mich', 34, INK, spacing=8); PL.text((940, 735), 'LEVEL', 'mich', 34, INK, spacing=8)
for x in (380, 940):  # tick rings around knobs
    for k in range(11):
        a = math.radians(135 + 27*k)
        d.line([(x + 175*math.cos(a), 520 + 175*math.sin(a)), (x + 205*math.cos(a), 520 + 205*math.sin(a))], fill=INK, width=6)
# central geometric sigil: 33 spikes, nested rings, interlocked triangles
cx, cy = 660, 1240
for k in range(33):
    a = 2*math.pi*k/33 - math.pi/2; L = 380 if k % 3 == 0 else 300
    b0, b1 = a - 0.035, a + 0.035
    d.polygon([(cx+215*math.cos(b0), cy+215*math.sin(b0)), (cx+215*math.cos(b1), cy+215*math.sin(b1)), (cx+L*math.cos(a), cy+L*math.sin(a))], fill=INK)
for r, w in ((215, 9), (185, 3), (110, 7)):
    d.ellipse((cx-r, cy-r, cx+r, cy+r), outline=INK, width=w)
for rr, ww in ((180, 6), (150, 3)):
    P = [(cx + rr*math.cos(math.radians(-90 + 120*i)), cy + rr*math.sin(math.radians(-90 + 120*i))) for i in range(3)]
    d.line(P + [P[0]], fill=INK, width=ww)
d.ellipse((cx-60, cy-60, cx+60, cy+60), fill=CLEAR)
# circuit-like traces off the sigil
for s in (-1, 1):
    for i, dy in enumerate((-150, 0, 150)):
        x0 = cx + s*(300 if dy == 0 else 260)
        pts = [(x0, cy+dy), (cx + s*450, cy+dy), (cx + s*490, cy+dy + (40 if dy >= 0 else -40)), (cx + s*520, cy+dy + (40 if dy >= 0 else -40))]
        d.line(pts, fill=INK, width=5); x, y = pts[-1]; d.ellipse((x-12, y-12, x+12, y+12), outline=INK, width=5)
# title
PL.text((660, 1700), 'XXXIII', 'mich', 150, INK, spacing=6)
d.line([(150, 1810), (1170, 1810)], fill=INK, width=5); d.line([(150, 1590), (1170, 1590)], fill=INK, width=5)
PL.text((660, 1850), 'THIRTY-THREE  BOOST', 'mich', 24, INK, spacing=8)
# footswitch: hex frame + angular brackets
fx, fy = 660, 2080
P = [(fx + 170*math.cos(math.radians(30 + 60*i)), fy + 170*math.sin(math.radians(30 + 60*i))) for i in range(6)]
d.line(P + [P[0]], fill=INK, width=8)
for s in (-1, 1):
    d.line([(fx + s*240, fy - 110), (fx + s*300, fy - 110), (fx + s*300, fy + 110), (fx + s*240, fy + 110)], fill=INK, width=7)
    d.line([(fx + s*350, fy - 60), (fx + s*400, fy), (fx + s*350, fy + 60)], fill=INK, width=7)
g = ImageDraw.Draw(gl); g.ellipse((cx-55, cy-55, cx+55, cy+55), fill=(255, 30, 25, 150)); gl = gl.filter(ImageFilter.GaussianBlur(25))
PL.save_as('xxxiii', gl)
