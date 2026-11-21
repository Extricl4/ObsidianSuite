"""Halcyon: ambient shoegaze reverb (Tone Charm Audio IRs: slap back + percussive verb). Pearl lilac, pastel haze, fine white line-work."""
from pedal_art_lib import *
WHT = (250, 248, 255, 255)
IND = (58, 36, 112, 255)
P = Plate()
# soft pastel haze painted into the plate (partly transparent so the pearl shows)
haze = Image.new('RGBA', (W, H), (0, 0, 0, 0)); hd = ImageDraw.Draw(haze)
for (x, y, r, c) in ((420, 1250, 420, (255, 170, 210, 120)), (900, 1450, 480, (170, 190, 255, 120)), (660, 1050, 300, (255, 220, 190, 90)), (700, 1700, 360, (200, 170, 255, 100))):
    hd.ellipse((x-r, y-r, x+r, y+r), fill=c)
haze = haze.filter(ImageFilter.GaussianBlur(120))
P.im.alpha_composite(haze.resize((W*SS, H*SS)))
P.rrect((64, 64, W-64, H-64), 40, WHT, 3); P.rrect((80, 80, W-80, H-80), 30, WHT, 1)
for x, t in ((250, 'MIX'), (660, 'DECAY'), (1070, 'TONE')): P.text((x, 735), t, 'saira', 30, IND, spacing=10)
P.text((540, 900), 'SLAP', 'saira', 24, IND, spacing=6); P.text((785, 900), 'VERB', 'saira', 24, IND, spacing=6)
# calm-water ripples around the LED (660,1300) + reflected horizon lines
cx, cy = 660, 1300
for i, r in enumerate((90, 150, 220, 300, 390, 490)):
    P.d.ellipse(((cx-r)*SS, (cy-r*0.38)*SS, (cx+r)*SS, (cy+r*0.38)*SS), outline=WHT, width=max(1, int((3.2 - i*0.4)*SS)))
for k in range(14):
    y = 1520 + k*14 + k*k*1.2; w = 470 - k*22
    P.line([(cx - w, y), (cx + w, y)], WHT, max(0.8, 2.4 - k*0.12))
# drifting stars
random.seed(4)
for _ in range(55):
    x, y = random.randint(130, 1190), random.randint(950, 1450)
    if abs(x-cx) < 120 and abs(y-cy) < 80: continue
    if random.random() < .2: star(P.d, x*SS, y*SS, random.uniform(7, 12)*SS, WHT, pts=4, inner=0.25)
    else: P.circle(x, y, random.uniform(1.6, 3.2), WHT)
P.text((660, 1890), 'HALCYON', 'saira', 120, IND, spacing=26)
P.text((660, 1985), 'AMBIENT  REVERB', 'saira', 26, IND, spacing=14)
gl = A.canvas(); g = ImageDraw.Draw(gl); g.ellipse((cx-200, cy-80, cx+200, cy+80), fill=(220, 200, 255, 120)); gl = gl.filter(ImageFilter.GaussianBlur(40))
P.save_as('halcyon', gl)
