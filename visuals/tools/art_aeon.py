"""Aeon: wide bass/guitar drive (Darkglass Alpha Omega Ultra captures, 56 files).
Slate-blue anodised plate; engraving cuts through to bare silver aluminium. Big alpha/omega medallion in the middle."""
from pedal_art_lib import *
PW, PH = 2000, 1600
INK = (215, 220, 230, 255)
P = Plate(PW, PH)
bead_border(P, (50, 50, PW-50, PH-50), 36, INK, step=24, w=4)
# meander bands top of the medallion zone and bottom
def meander(y, x0, x1, s=26, w=3.2):
    x = x0
    while x + 2*s <= x1:
        P.line([(x, y+s), (x, y), (x+s*1.5, y), (x+s*1.5, y+s*0.66), (x+s*0.6, y+s*0.66), (x+s*0.6, y+s*0.33), (x+s, y+s*0.33)], INK, w)
        P.line([(x, y+s), (x+2*s, y+s)], INK, w); x += 2*s
# --- top row controls: VOICE toggle, BLEND, LEVEL, DRIVE (stepped)
P.text((230, 175), 'VOICE', 'cinzel', 28, INK, spacing=8)
P.text((160, 335), 'α', 'cardo', 64, INK); P.text((300, 335), 'Ω', 'cardo', 58, INK)
for x, t in ((650, 'BLEND'), (1050, 'LEVEL')): P.text((x, 520), t, 'cinzel', 32, INK, spacing=10)
for x in (650, 1050):
    for k in range(11):
        a = math.radians(135 + 27*k); P.line([(x + 150*math.cos(a), 330 + 150*math.sin(a)), (x + (172 if k % 5 == 0 else 162)*math.cos(a), 330 + (172 if k % 5 == 0 else 162)*math.sin(a))], INK, 3)
P.text((1450, 520), 'DRIVE', 'cinzel', 32, INK, spacing=10)
for i, t in enumerate(['0', '1', '3', '5', '7', '8', '10']):
    a = math.radians(135 + 270*i/6)
    P.line([(1450 + 150*math.cos(a), 330 + 150*math.sin(a)), (1450 + 170*math.cos(a), 330 + 170*math.sin(a))], INK, 4)
    P.text((1450 + 205*math.cos(a), 330 + 205*math.sin(a)), t, 'cinzel', 24, INK)
# --- side buttons (GROWL left, BITE right) with LEDs above
P.text((300, 950), 'GROWL', 'cinzel', 30, INK, spacing=8); P.text((1700, 950), 'BITE', 'cinzel', 30, INK, spacing=8)
for x in (300, 1700):
    P.circle(x, 820, 92, INK, 3); P.circle(x, 820, 104, INK, 1.2)
# --- central medallion (center 1000, 960)
cx, cy, R = 1000, 965, 310
P.circle(cx, cy, R, INK, 6); P.circle(cx, cy, R - 16, INK, 1.6); P.circle(cx, cy, R - 66, INK, 1.6); P.circle(cx, cy, R - 82, INK, 4)
# inscription ring: "beginning and end" in Greek, set on the ring
f = A.font('cardo', int(30*SS)); rr = (R - 41)*SS
unit = 'ΑΡΧΗ · ΚΑΙ · ΤΕΛΟΣ · '; txt = unit
while sum(P.d.textlength(c, font=f) for c in txt + unit) < 2*math.pi*rr*0.97: txt += unit
tot = sum(P.d.textlength(c, font=f) for c in txt); ang = -math.pi/2 - (tot/rr)/2
for c in txt:
    wch = P.d.textlength(c, font=f); a = ang + (wch/2)/rr
    g = Image.new('RGBA', (int(70*SS), int(70*SS)), (0, 0, 0, 0)); gd = ImageDraw.Draw(g)
    gd.text((g.width/2, g.height/2), c, font=f, fill=INK, anchor='mm')
    g = g.rotate(-math.degrees(a) - 90, resample=Image.BICUBIC)
    P.im.alpha_composite(g, (int(cx*SS + rr*math.cos(a) - g.width/2), int(cy*SS + rr*math.sin(a) - g.height/2)))
    ang += wch/rr
# fine radial hatching inside the inner ring (engraved field)
for k in range(180):
    a = 2*math.pi*k/180
    P.line([(cx + (R-96)*math.cos(a), cy + (R-96)*math.sin(a)), (cx + (R-112)*math.cos(a), cy + (R-112)*math.sin(a))], INK, 1.3)
# the letters: big alpha and omega, interlocked by a single woven ring
P.circle(cx, cy, 150, INK, 5)
P.text((cx - 105, cy - 10), 'α', 'cardo', 300, INK)
P.text((cx + 105, cy - 14), 'Ω', 'cardo', 270, INK)
P.line([(cx - 200, cy + 150), (cx + 200, cy + 150)], INK, 3)
for s in (-1, 1): P.circle(cx + s*215, cy + 150, 7, INK)
# compass points / stars on the medallion
for k in range(8):
    a = 2*math.pi*k/8 + math.pi/8
    star(P.d, (cx + (R + 30)*math.cos(a))*SS, (cy + (R + 30)*math.sin(a))*SS, 13*SS, INK, pts=4, inner=0.3)
# laurel branches flanking the medallion
for s in (-1, 1):
    stem = bez((cx + s*120, cy + R + 30), (cx + s*330, cy + R + 10), (cx + s*450, cy + 160), (cx + s*470, cy - 160), 60)
    P.taper(stem, INK, 6, 1.5)
    for i in range(4, 58, 5):
        x, y = stem[i]; aa = math.atan2(stem[i+1][1]-stem[i-1][1], stem[i+1][0]-stem[i-1][0])
        for sd in (-1, 1):
            la = aa + sd*0.7; L = 54*(1 - i/100)
            leaf(P, (x, y), (x + math.cos(la)*L, y + math.sin(la)*L), INK, L*0.4, bend=sd*0.2)
meander(570, 560, 1440, s=22, w=2.6)
# --- footswitch labels + title
P.text((540, 1400), 'DRIVE', 'cinzel', 28, INK, spacing=8); P.text((1460, 1400), 'BYPASS', 'cinzel', 28, INK, spacing=8)
P.text((1000, 1420), 'AEON', 'cinzel', 120, INK, spacing=40)
P.text((1000, 1505), 'BASS  DRIVE', 'cinzel', 24, INK, spacing=8)
for s in (-1, 1): P.taper(bez((1000 + s*230, 1420), (1000 + s*300, 1416), (1000 + s*360, 1424), (1000 + s*430, 1420), 20), INK, 4, 1)
gl = A.canvas(PW, PH)
P.save_as('aeon', gl)
