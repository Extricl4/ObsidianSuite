"""Orpheus (acoustic sim): matte white body, light spruce front with a soundhole-style round grille and mosaic rosette.
python3 -I tools/art_orpheus.py"""
import math, sys, json, numpy as np
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import relief_art as ra
ra.PX = 120
from relief_art import Relief
from panel_art import panel, font, spaced

D = 4.9; R = 2.35
r = Relief(D, D, base=0.0)
rho = np.hypot(r.X, r.Y); th = np.arctan2(r.Y, r.X)
# perforated grille: light face with dark round perforations
g = 0.16; rows = np.round(r.Y / (g * 0.866)); off = (rows % 2) * g / 2
gx = np.round((r.X - off) / g) * g + off; gy = rows * g * 0.866
perf = (r.X - gx) ** 2 + (r.Y - gy) ** 2 <= 0.05 ** 2
inner = rho <= 1.72
r.H[inner] = 0.72; r.H[inner & perf] = 0.08
# rings and mosaic band
def ring(a, b, h): m = (rho >= a) & (rho <= b); r.H[m] = h; return m
ring(1.72, 1.8, 0.95); ring(2.2, 2.27, 0.95)
band = (rho > 1.8) & (rho < 2.2)
seg = np.floor((th + math.pi) / (2 * math.pi) * 48); row = np.floor((rho - 1.8) / 0.2)
r.H[band] = np.where(((seg + row) % 2) == 0, 0.82, 0.28)[band]
ring(1.98, 2.02, 0.95)
ring(2.27, R, 0.5)
r.holes = rho > R
r.save('art/orpheus_rosette_h.png', 'art/orpheus_rosette_holes.png', blur=0.4)
print('art ok')

KZ = 2.05; WW = [1.0, 0.82, 0.58]
KX = [-7.6, -5.4, -3.2, -1.0, 1.2, 3.4, 5.6]; V = [0.45, 0.5, 0.5, 0.55, 0.5, 0.35, 0.6]
kn = [dict(x=x, z=KZ, r=0.4, v=v, shape='cap', mat='white', mat2='bronze', ptr='black') for x, v in zip(KX, V)]
spec = dict(size=[22, 9.5, 9.5], border=0.6, recess=0.8, bevel=0.3, rimcolor=[1, 0.9, 0.75],
            body=dict(color=[0.78, 0.76, 0.72], rough=0.88, bump=['noise', 300, 0.012]), handle=dict(w=7.5, h=1.1, color=[0.72, 0.7, 0.66]),
            glow_strips=[dict(z=3.9, color=WW, strength=2.2, w=20.76)],
            zones=[dict(type='wood', z=[3.9, 8.9], planks=18, vertical=True, c1=[0.8, 0.63, 0.42], c2=[0.66, 0.49, 0.3], coat=0.0, rough=0.72),
                   dict(type='panel', z=[0.6, 3.9], art='art/orpheus_panel.png', artemis=0.0, artrough=0.7, color=[0.8, 0.78, 0.74], metal=0.0, rough=0.85,
                        bump=['noise', 300, 0.01], knobs=kn, jacks=[[-9.6, KZ, 'jack']],
                        buttons=[dict(id='take', x=7.55, z=KZ, w=0.46, h=0.32, cap=[0.82, 0.8, 0.76], metal=0.0, rough=0.6)],
                        leds=[dict(id=f'tk{i+1}', x=7.55 + (i - 0.5) * 0.22, z=KZ + 0.42, w=0.08, h=0.055, color=WW, st=12) for i in range(2)],
                        jewels=[dict(x=9.0, z=KZ, color=WW, r=0.24, strength=18, id='power')])])
json.dump(spec, open('amps/orpheus.json', 'w'), indent=1)
INK = (92, 64, 42, 255)
def extra(d, P, col):
    f = font('saira', 13)
    spaced(d, P(7.44, KZ + 0.62), 'I', f, col, 0); spaced(d, P(7.66, KZ + 0.62), 'II', f, col, 0)
panel('art/orpheus_panel.png', spec, (0.6, 3.9), [dict(k, label=l) for k, l in zip(kn, ['GAIN', 'BASS', 'MIDDLE', 'TREBLE', 'BODY', 'REVERB', 'VOLUME'])],
      toggles=[(7.55, KZ, 'TAKE')], jewels=[(9.0, KZ - 0.1, 'POWER')], face='saira', col=INK, size=18, dial='lines', tick_len=0.1, spacing=4, extra=extra)
print('spec ok')
