"""Specs + panel art for the five flagship amps."""
import json, sys, math
sys.path.insert(0, 'tools')
from panel_art import panel, font, spaced

def save(name, spec): json.dump(spec, open(f'amps/{name}.json', 'w'), indent=1)

def abaddon():
    OR = [1, 0.38, 0.08]
    xs = [-9.25, -8.15, -7.05, -5.95, -4.85, -3.05, -1.95, -0.85, 0.25, 1.35, 3.15, 4.25, 5.35]
    lab = ['GAIN', 'BASS', 'MID', 'TREBLE', 'VOLUME', 'GAIN', 'BASS', 'MID', 'TREBLE', 'VOLUME', 'DEPTH', 'PRESENCE', 'MASTER']
    V = [0.4, 0.55, 0.45, 0.6, 0.5, 0.8, 0.6, 0.35, 0.6, 0.55, 0.65, 0.5, 0.42]
    KZ = 2.25
    kn = [dict(x=x, z=KZ, r=0.32, v=v, shape='skirt2', mat='bakelite', mat2='blackchrome', ptr=[1, 0.12, 0.04]) for x, v in zip(xs, V)]
    tubes = [dict(x=x, z=7.0, h=2.8, r=0.38, color=OR, strength=10, light=60) for x in (-9.9, -7.8, -5.7, 5.7, 7.8, 9.9, -2.6, 2.6)]
    spec = dict(size=[24, 10, 10.5], border=0.7, recess=2.4, bevel=0.2, rimcolor=[1, 0.55, 0.3],
        body=dict(color=[0.014, 0.014, 0.015], rough=0.7, bump=['leather', 34, 0.7]), handle=dict(w=8.5, h=1.2, color=[0.02, 0.02, 0.02]),
        glow_strips=[dict(z=4.25, color=OR, strength=3.5, w=22.54)],
        zones=[dict(type='bars', z=[4.25, 9.8], n=10, bh=0.27, bd=0.34, pad=0.22, color=[0.018, 0.018, 0.02], metal=0.6, rough=0.42,
                    gaps=[[[-4.4, 4.4], [5.5, 8.55]]], tubes=tubes, backglow=[1, 0.22, 0.03], backglow_strength=0.07,
                    ambient=[dict(x=x, energy=110, color=[1, 0.3, 0.05], size=5) for x in (-7, 0, 7)],
                    plates=[dict(height='art/abaddon_plate_h.png', x=0, z=7.025, w=8.2, h=2.7, depth=0.36, res=80, lift=0.18,
                                 color=[0.62, 0.62, 0.65], low=[0.03, 0.03, 0.032], lo_at=0.35, hi_at=0.95, metal=0.85, metal_hi=1.0, rough=0.3, grain=[200, 0.02])]),
               dict(type='panel', z=[0.7, 4.25], art='art/abaddon_panel.png', artemis=0.2, color=[0.03, 0.03, 0.032], metal=0.85, rough=0.38, bump=['brush', 60, 0.03],
                    knobs=kn, rockers=[[7.55, KZ, [1, 0.25, 0.05]], [8.85, KZ, [1, 0.1, 0.04]]],
                    jacks=[[-10.5, KZ + 0.45, 'jack'], [-10.5, KZ - 0.55, 'jack']],
                    jewels=[dict(x=-7.05, z=3.55, color=[1, 0.1, 0.03], r=0.07, strength=40), dict(x=-0.85, z=3.55, color=[1, 0.1, 0.03], r=0.07, strength=40), dict(x=4.25, z=3.55, color=[1, 0.35, 0.03], r=0.07, strength=40)])])
    save('abaddon', spec)
    def extra(d, P, col):
        f = font('cinzel', 18)
        for (a, b, t) in ((-9.25, -4.85, 'CHANNEL  I'), (-3.05, 1.35, 'CHANNEL  II'), (3.15, 5.35, 'OUTPUT')):
            y = P(0, 1.18)[1]; x0, x1 = P(a - 0.4, 0)[0], P(b + 0.4, 0)[0]
            d.line([(x0, y - 14), (x0, y), (x1, y), (x1, y - 14)], fill=col, width=2)
            spaced(d, ((x0 + x1) / 2, y + 22), t, f, col, 3)
        fb = font('pirata', 56); d.text((P(10.2, 3.6)[0], P(0, 3.6)[1]), 'Abaddon', font=fb, fill=(220, 80, 30, 255), anchor='rm')
    panel('art/abaddon_panel.png', spec, (0.7, 4.25), [dict(k, label=l) for k, l in zip(kn, lab)], toggles=[(7.55, KZ - 0.15, 'STANDBY'), (8.85, KZ - 0.15, 'POWER')],
          face='marcellus', col=(205, 200, 196, 255), size=19, dial='lines', tick_len=0.1, extra=extra, spacing=2)

def nihil():
    KZ = 2.4; xs = [-4.2, -3.0, -1.8, -0.6, 0.6, 1.8, 3.0]
    lab = ['MASTER', 'PRESENCE', 'BASS', 'MIDDLE', 'TREBLE', 'DRIVE', 'GAIN']; V = [0.35, 0.55, 0.5, 0.42, 0.6, 0.7, 0.75]
    kn = [dict(x=x, z=KZ, r=0.27, v=v, shape='knurl', mat='bakelite', mat2='bakelite', ptr='white') for x, v in zip(xs, V)]
    spec = dict(size=[24, 10, 10.5], border=0.7, recess=1.0, bevel=0.28, rimcolor=[0.8, 0.85, 0.9],
        body=dict(color=[0.016, 0.016, 0.018], rough=0.6, bump=['leather', 34, 0.8]), handle=dict(w=8.5, h=1.2, color=[0.02, 0.02, 0.02]),
        zones=[dict(type='relief', z=[0.7, 9.8], height='art/nihil_h.png', holes='art/nihil_holes.png', depth=0.32, res=62,
                    color=[0.03, 0.03, 0.033], cavity=0.45, rough=0.6, hi_at=0.5),
               dict(type='panel', z=[1.15, 3.65], window=[-7.4, 7.4], inset=0.32, art='art/nihil_panel.png', artemis=0.1,
                    color=[0.004, 0.004, 0.005], metal=0.4, rough=0.06, bump=0, knobs=kn,
                    toggles=[[-6.0, KZ], [-5.25, KZ]], jewels=[dict(x=-6.75, z=KZ, color=[0.2, 1, 0.45], r=0.18, strength=18)],
                    jacks=[[4.75, 2.95, 'jack'], [6.45, 2.95, 'jack'], [4.75, 1.85, 'jack'], [6.45, 1.85, 'jack']])])
    save('nihil', spec)
    def extra(d, P, col):
        f = font('marcellus', 20)
        for (x, z, t) in ((4.2, 2.95, 'I'), (5.6, 2.95, 'HIGH'), (7.0, 2.95, 'II'), (4.2, 1.85, 'I'), (5.6, 1.85, 'LOW'), (7.0, 1.85, 'II')):
            spaced(d, P(x, z), t, f, col, 2)
        for (x, t1, t2) in ((-6.0, 'OFF', 'ON'), (-5.25, 'STBY', 'ON')):
            spaced(d, P(x, KZ + 0.55), t1, f, col, 2); spaced(d, P(x, KZ - 0.55), t2, f, col, 2)
    panel('art/nihil_panel.png', spec, (1.15, 3.65), [dict(k, label=l) for k, l in zip(kn, lab)], face='marcellus', col=(212, 212, 216, 255), size=19, dial='numbers', numsize=15, tick_len=0.06, extra=extra, spacing=2)

def mortis():
    GR = [0.35, 1.0, 0.2]; KX = [-8.5, -6.0, -3.5, -1.0, 1.5, 4.0, 6.5]; KZ = 2.2975; V = [0.85, 0.6, 0.3, 0.65, 0.55, 0.7, 0.45]
    kn = [dict(x=x, z=KZ, r=0.42, v=v, shape='knurl', mat='bone', mat2='iron', ptr='black') for x, v in zip(KX, V)]
    spec = dict(size=[24, 10, 10.5], border=0.7, recess=1.2, bevel=0.2, rimcolor=[0.6, 1, 0.5],
        body=dict(color=[0.016, 0.015, 0.014], rough=0.75, bump=['leather', 30, 0.8]), handle=dict(w=8.5, h=1.2, color=[0.02, 0.02, 0.02]),
        glow_strips=[dict(z=4.25, color=GR, strength=3.5, w=22.54)],
        zones=[dict(type='relief', z=[4.25, 9.8], height='art/mortis_h.png', holes='art/mortis_holes.png', glass='art/mortis_glass.png', glass_strength=1.6, glass_y=0.18,
                    glass_lights=[dict(x=x, color=GR, energy=10, size=4) for x in (-7, 0, 7)], depth=0.45, res=60,
                    color=[0.2, 0.18, 0.15], cavity=0.12, rough=0.85, hi_at=0.72, grain=[90, 0.14], graze=[dict(color=[0.9, 1.0, 0.85], energy=30, tilt=-25, out=1.0, top=0.3)]),
               dict(type='panel', z=[0.7, 4.25], art='art/mortis_panel.png', artemis=0.15, color=[0.035, 0.032, 0.03], metal=0.8, rough=0.65, bump=['noise', 14, 0.4],
                    knobs=kn, toggles=[[8.4, KZ]], jewels=[dict(x=9.5, z=KZ, color=GR, r=0.28, strength=28)], jacks=[[-10.3, KZ, 'jack']],
                    posts=[[-11.0, 0.98], [11.0, 0.98], [-11.0, 3.97], [11.0, 3.97]])])
    save('mortis', spec)
    panel('art/mortis_panel.png', spec, (0.7, 4.25), [dict(k, label=l) for k, l in zip(kn, ['Gain', 'Bass', 'Middle', 'Treble', 'Presence', 'Rot', 'Master'])],
          toggles=[(8.4, KZ, 'Standby')], jewels=[(9.5, KZ - 0.15, 'Power')], face='pirata', col=(222, 206, 172, 255), size=42, dial='numbers', numface='marcellus', numsize=19,
          lines=[(-10.9, 0.98, 10.9, 0.98, 2)])

def niflheim():
    IC = [0.45, 0.75, 1.0]; KX = [-8.5, -6.0, -3.5, -1.0, 1.5, 4.0, 6.5]; KZ = 2.2975; V = [0.8, 0.45, 0.4, 0.6, 0.55, 0.65, 0.4]
    kn = [dict(x=x, z=KZ, r=0.42, v=v, shape='knurl', mat='bakelite', mat2='silver', ptr=IC) for x, v in zip(KX, V)]
    spec = dict(size=[24, 10, 10.5], border=0.7, recess=1.2, bevel=0.2, rimcolor=[0.6, 0.8, 1.0], corners='chrome', chrome_size=1.4,
        body=dict(color=[0.012, 0.012, 0.014], rough=0.6, bump=['leather', 40, 0.6]), handle=dict(w=8.5, h=1.2, color=[0.02, 0.02, 0.02]),
        glow_strips=[dict(z=4.25, color=IC, strength=3.5, w=22.54)],
        zones=[dict(type='relief', z=[4.25, 9.8], height='art/niflheim_h.png', holes='art/niflheim_holes.png', glass='art/niflheim_glass.png', glass_strength=0.35, glass_y=0.2,
                    glass_lights=[dict(x=x, color=IC, energy=25, size=4) for x in (-7, 0, 7)], depth=0.45, res=60,
                    color=[0.4, 0.43, 0.48], cavity=0.12, metal=0.85, rough=0.3, hi_at=0.9, grain=[120, 0.05]),
               dict(type='panel', z=[0.7, 4.25], art='art/niflheim_panel.png', artemis=0.15, color=[0.012, 0.012, 0.014], metal=0.4, rough=0.4,
                    knobs=kn, toggles=[[8.4, KZ]], jewels=[dict(x=9.5, z=KZ, color=IC, r=0.28, strength=28)], jacks=[[-10.3, KZ, 'jack']])])
    save('niflheim', spec)
    panel('art/niflheim_panel.png', spec, (0.7, 4.25), [dict(k, label=l) for k, l in zip(kn, ['Gain', 'Bass', 'Middle', 'Treble', 'Presence', 'Frost', 'Master'])],
          toggles=[(8.4, KZ, 'Standby')], jewels=[(9.5, KZ - 0.15, 'Power')], face='jacquard', col=(196, 220, 244, 255), size=40, dial='lines', lines=[(-10.9, 0.98, 10.9, 0.98, 2)])

def moros():
    KZ = 2.25
    ks = [(-8.6, 0.45, 'knurl', 'GAIN'), (-6.7, 0.3, 'chicken', 'BASS'), (-5.5, 0.3, 'chicken', 'MID'), (-4.3, 0.3, 'chicken', 'TREBLE'), (-2.4, 0.45, 'knurl', 'LEVEL'),
          (0.0, 0.48, 'knurl', 'MASTER'), (1.75, 0.3, 'chicken', 'TIGHT'), (2.95, 0.3, 'chicken', 'AIR')]
    V = [0.7, 0.55, 0.4, 0.6, 0.5, 0.35, 0.65, 0.55]
    kn = [dict(x=x, z=KZ, r=r, v=v, shape=sh, mat='bakelite', mat2='silver', ptr='white') for (x, r, sh, l), v in zip(ks, V)]
    spec = dict(size=[24, 10, 10.5], border=0.7, recess=1.0, bevel=0.22, rimcolor=[0.75, 0.82, 1.0],
        body=dict(color=[0.014, 0.014, 0.016], rough=0.65, bump=['leather', 36, 0.8]), handle=dict(w=8.5, h=1.2, color=[0.02, 0.02, 0.02]),
        zones=[dict(type='relief', z=[4.25, 9.8], height='art/moros_h.png', depth=0.3, res=56, color=[0.14, 0.11, 0.095], low=[0.025, 0.02, 0.017], lo_at=0.3, hi_at=0.62,
                    rough=0.5, metal=0.1, grain=[200, 0.03], graze=[dict(color=[0.85, 0.9, 1.0], energy=55, tilt=-22, out=1.0, top=0.4)],
                    plates=[dict(height='art/moros_badge_h.png', x=-8.3, z=7.45, w=2.2, h=2.2, depth=0.3, res=110, lift=0.12,
                                 color=[0.7, 0.7, 0.73], low=[0.02, 0.02, 0.022], lo_at=0.32, hi_at=0.75, metal=0.5, metal_hi=1.0, rough=0.28)]),
               dict(type='panel', z=[0.7, 4.25], art='art/moros_panel.png', artemis=0.15, color=[0.008, 0.008, 0.009], metal=0.3, rough=0.32, knobs=kn,
                    toggles=[[-7.6, KZ], [-1.2, KZ], [7.4, KZ - 0.15], [8.35, KZ - 0.15]], jewels=[dict(x=9.4, z=KZ - 0.15, color=[0.3, 0.55, 1.0], r=0.2, strength=24)],
                    jacks=[[-10.4, KZ, 'jack']])])
    save('moros', spec)
    def extra(d, P, col):
        f = font('mich', 15); fb = font('mich', 28)
        y = P(0, 3.25)[1]
        for (a, b) in ((-7.1, -3.9), (1.35, 3.35)):
            x0, x1 = P(a, 0)[0], P(b, 0)[0]; d.line([(x0, y + 12), (x0, y), (x1, y), (x1, y + 12)], fill=col, width=2)
        yl = P(0, KZ)[1]; d.line([(P(-10.0, 0)[0], yl), (P(-9.2, 0)[0], yl)], fill=col, width=2); d.line([(P(3.5, 0)[0], yl), (P(10.6, 0)[0], yl)], fill=col, width=2)
        spaced(d, P(8.9, 3.45), 'MOROS', fb, col, 18)
        spaced(d, P(-7.6, KZ + 0.5), 'HIGH', f, col, 2); spaced(d, P(-7.6, KZ - 0.5), 'LOW', f, col, 2)
        spaced(d, P(-1.2, KZ + 0.5), 'VOICE', f, col, 2)
        spaced(d, P(7.4, KZ - 0.65), 'ACTIVE', f, col, 2); spaced(d, P(8.35, KZ - 0.65), 'POWER', f, col, 2)
        for (x, r, sh, l) in ks:     # crosshair under the big knobs
            if r > 0.4:
                cx, cy = P(x, KZ); rr = (r + 0.2) * 120
                d.line([(cx, cy + rr), (cx, cy + rr + 22)], fill=col, width=2)
    panel('art/moros_panel.png', spec, (0.7, 4.25), [dict(k, label=l, dial=('lines' if k['r'] > 0.4 else 'none')) for k, (x, r, sh, l) in zip(kn, ks)],
          face='mich', col=(200, 202, 208, 255), size=15, dial='lines', tick_len=0.08, extra=extra, spacing=3, arc=False)

for n in (sys.argv[1:] or ['abaddon', 'nihil', 'mortis', 'niflheim', 'moros']):
    globals()[n](); print('ok', n)
