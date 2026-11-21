"""Ossuary: catacomb reverb (Green-Wood Cemetery Catacombs IRs). Weathered stone, bone-white inlay; a corridor of arches receding to the LED."""
from pedal_art_lib import *
BONE = (214, 204, 178, 255)
P = Plate()
P.rrect((64, 64, W-64, H-64), 40, BONE, 5); P.rrect((84, 84, W-84, H-84), 30, BONE, 1.6)
for x, t in ((250, 'MIX'), (660, 'DECAY'), (1070, 'TONE')): P.text((x, 735), t, 'cinzel', 34, BONE, spacing=8)
for x, t in ((160, 'C'), (250, 'M'), (340, 'F')): P.text((x, 1060), t, 'cinzel', 24, BONE)
P.text((250, 880), 'ROOM', 'cinzel', 22, BONE, spacing=8)
for x, t in ((980, 'I'), (1070, 'II'), (1160, 'III')): P.text((x, 1060), t, 'cinzel', 24, BONE)
P.text((1070, 880), 'MIC', 'cinzel', 22, BONE, spacing=8)
# corridor: nested pointed arches shrinking toward the vanishing point (LED at 660,1420)
vx, vy = 660, 1400
for i in range(1, 10):
    s = 0.82**i; w = 400*s; top = vy - 560*s; base = vy + 300*s
    arch = [(vx - w, base)] + bez((vx - w, vy - 120*s), (vx - w, top + 160*s), (vx - w*0.4, top + 30*s), (vx, top), 24) + bez((vx, top), (vx + w*0.4, top + 30*s), (vx + w, top + 160*s), (vx + w, vy - 120*s), 24) + [(vx + w, base)]
    P.line(arch, BONE, max(1.4, 7*s))
    if i < 6:   # niches of skulls in the walls: small rounded cells
        for side in (-1, 1):
            for r in range(3):
                nx = vx + side*(w - 26*s*1.6); ny = vy - 60*s + r*95*s
                if s > .45:
                    nw, nh = 20*s, 30*s
                    P.line([(nx - nw, ny + nh)] + bez((nx - nw, ny - nh*.2), (nx - nw, ny - nh*.8), (nx - nw*.3, ny - nh), (nx, ny - nh*1.15), 10)
                           + bez((nx, ny - nh*1.15), (nx + nw*.3, ny - nh), (nx + nw, ny - nh*.8), (nx + nw, ny - nh*.2), 10) + [(nx + nw, ny + nh), (nx - nw, ny + nh)], BONE, max(1, 2.4*s))
# floor lines converging
for k in range(-4, 5):
    P.line([(vx + k*82, vy + 246), (vx + k*14, vy + 40)], BONE, 1.4)
P.line([(vx - 328, vy + 246), (vx + 328, vy + 246)], BONE, 5)
# inscription band
P.text((660, 1810), 'OSSUARY', 'cdec', 150, BONE, spacing=16)
P.text((660, 1905), 'MEMENTO  MORI', 'cinzel', 28, BONE, spacing=14)
for s in (-1, 1): P.circle(660 + s*230, 1905, 5, BONE)
P.text((660, 2290), 'CATACOMB  REVERB', 'cinzel', 22, BONE, spacing=10)
gl = A.canvas(); g = ImageDraw.Draw(gl)
for r, a in ((140, 60), (90, 120), (50, 200)): g.ellipse((vx-r, vy-r*1.3, vx+r, vy+r*0.9), fill=(255, 40, 25, a))
gl = gl.filter(ImageFilter.GaussianBlur(30))
P.save_as('ossuary', gl)
