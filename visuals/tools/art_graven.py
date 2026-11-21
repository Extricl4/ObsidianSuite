"""Graven v3: matte charcoal TS-style drive (Fortin-modded TS-9 captures). Gloss-black vine-and-thorn engraving cut into the plate."""
from pedal_art_lib import *
INK = (8, 8, 10, 255)
P = Plate()
bead_border(P, (64, 64, W-64, H-64), 40, INK, w=5)
for s in (-1, 1):
    # big vines rising around the DRIVE knob (660,640 r~210) and curling into the top corners
    rinceau(P, bez((660 + s*40, 900), (660 + s*330, 900), (660 + s*390, 520), (660 + s*300, 330), 70), INK, flip=s, w0=18, w1=3, curl=150, leaf_len=115, leaf_every=10, thorns=3, seed=3)
    # inner vine under the knob, curling inward
    rinceau(P, bez((660 + s*470, 980), (660 + s*560, 860), (660 + s*560, 700), (660 + s*500, 600), 50), INK, flip=-s, w0=11, curl=80, leaf_len=75, leaf_every=10, tendrils=1, seed=5)
    # vines out of the sword's crossguard, swinging down around the title
    rinceau(P, bez((660 + s*250, 1300), (660 + s*470, 1330), (660 + s*520, 1460), (660 + s*430, 1520), 60), INK, flip=s, w0=13, curl=85, leaf_len=80, leaf_every=9, thorns=2, tendrils=1, seed=8)
    # footswitch vines
    rinceau(P, bez((660 + s*150, 2180), (660 + s*360, 2200), (660 + s*470, 2060), (660 + s*390, 1950), 60), INK, flip=-s, w0=15, curl=110, leaf_len=95, leaf_every=10, thorns=3, seed=11)
P.text((660, 940), 'DRIVE', 'cinzel', 40, INK, spacing=12)
P.text((250, 1185), 'LEVEL', 'cinzel', 30, INK, spacing=6)
for x, t in ((990, 'I'), (1070, 'II'), (1150, 'III')): P.text((x, 1185), t, 'cinzel', 30, INK)
# sword through the LED (660,1300)
cx = 660
P.poly([(cx-28, 1350), (cx+28, 1350), (cx+20, 1500), (cx, 1550), (cx-20, 1500)], INK); P.line([(cx, 1365), (cx, 1500)], CLEAR, 2.2)
P.taper([(cx-250 + i*500/40, 1300 + 16*math.sin(i/40*math.pi)) for i in range(41)], INK, 7, 7, 22)
P.circle(cx, 1300, 84, INK, 10); P.circle(cx, 1300, 68, CLEAR); P.circle(cx, 1300, 100, INK, 2.5)
P.poly([(cx-15, 1205), (cx+15, 1205), (cx+11, 1085), (cx-11, 1085)], INK)
for y in range(1095, 1200, 13): P.line([(cx-13, y), (cx+13, y+7)], CLEAR, 2)
P.circle(cx, 1058, 27, INK); P.circle(cx, 1058, 12, CLEAR)
# title
P.text((660, 1670), 'Graven', 'frak', 230, INK)
for s in (-1, 1): P.taper(bez((660 + s*60, 1805), (660 + s*250, 1790), (660 + s*420, 1820), (660 + s*500, 1795), 40), INK, 5, 1)
P.text((660, 1850), 'FORGED  OVERDRIVE', 'cinzel', 26, INK, spacing=10)
P.text((660, 2290), 'OBSIDIAN', 'cinzel', 26, INK, spacing=14)
gl = A.canvas(); g = ImageDraw.Draw(gl); g.ellipse((600, 1240, 720, 1360), fill=(255, 40, 20, 140)); gl = gl.filter(ImageFilter.GaussianBlur(25))
P.save_as('graven', gl)
