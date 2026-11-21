import json, sys, math
sys.path.insert(0, 'tools')
from panel_art import panel, font
ACC = [0.43, 0.8, 1.0]
WB, HB = 22.8, 8.75
Z = [0.06, HB - 0.06]
SX0, SX1, SZ0, SZ1 = -8.05, 7.45, 3.25, 7.9          # screen edges: the knobs sit in six equal columns under it
KX = [round(SX0 + (SX1 - SX0) / 12 * (2 * i + 1), 3) for i in range(6)]; KZ = 1.6; V = [0.7, 0.55, 0.45, 0.6, 0.5, 0.4]
LAB = ['GAIN', 'BASS', 'MIDDLE', 'TREBLE', 'PRESENCE', 'MASTER']
knobs = [dict(x=x, z=KZ, r=0.42, v=v, shape='knurl', mat='bakelite', mat2='silver', ptr='white') for x, v in zip(KX, V)]
B = [dict(x=8.4, z=4.0, w=0.82, h=0.42, led=ACC, on=False, ring=True, lab='PREV'), dict(x=9.6, z=4.0, w=0.82, h=0.42, led=ACC, on=False, ring=True, lab='NEXT'),
     dict(x=9.0, z=7.45, w=1.6, h=0.44, led=ACC, on=True, ring=True, lab='LOAD')]
leds = []
for i in range(12):
    t = i / 11; col = [0.1, 1, 0.3] if t < 0.65 else [1, 0.65, 0.15] if t < 0.85 else [1, 0.2, 0.15]
    leds.append([-9.6, 3.85 + i * 0.27, 0.42, 0.15, col, 1.4 if i < 7 else 0.08])
spec = dict(size=[WB, 14, HB], border=0.04, recess=0.6, front_inset=0.03, bevel=0.06, kscale=1.2, rimcolor=[0.6, 0.85, 1.0],
    rack=dict(ear=1.0, holes=[0.9, HB / 2, HB - 0.9], handle_off=0.9),
    body=dict(color=[0.016, 0.016, 0.018], metal=0.7, rough=0.42, bump=['brush', 60, 0.02]),
    zones=[dict(type='panel', z=Z, color=[0.011, 0.011, 0.012], metal=0.8, rough=0.42, aniso=0.6, bump=['brush', 70, 0.015],
                art='art/eidolon_panel.png', artemis=0.22, knobs=knobs, buttons=B, leds=leds,
                screens=[dict(x=(SX0 + SX1) / 2, z=(SZ0 + SZ1) / 2, w=SX1 - SX0, h=SZ1 - SZ0, img='art/eidolon_screen.png', emis=1.5, bezel=0.18, glass=0.1, light=22, light_color=[0.45, 0.75, 1.0])],
                encoders=[dict(x=9.0, z=5.65, r=0.78, ring=ACC, ring_strength=1.1)],
                leds2=[], jacks=[[-10.2, KZ, 'jack']], rockers=[[10.2, KZ, ACC]],
                screws=[[x, z, 30] for x in (-11.0, 11.0) for z in (0.45, HB - 0.45)])])
json.dump(spec, open('amps/eidolon.json', 'w'), indent=1)
col = (196, 204, 214, 255); accc = (110, 205, 255, 255)
def extra(d, P, c):
    f = font('mich', 15); fb = font('mich', 30)
    def sp(xy, t, f_, cc, s):
        tot = sum(d.textlength(ch, font=f_) for ch in t) + s * (len(t) - 1); x = xy[0] - tot / 2
        for ch in t: d.text((x, xy[1]), ch, font=f_, fill=cc, anchor='lm'); x += d.textlength(ch, font=f_) + s
    sp(P(-9.6, 8.0), 'EIDOLON', fb, accc, 9)
    sp(P(-9.6, 7.62), 'NAM CAPTURE AMP', font('mich', 10), col, 3)
    d.line([P(-10.8, 7.38), P(-8.4, 7.38)], fill=(70, 80, 92, 255), width=2)
    sp(P(-9.6, 7.08), 'INPUT', f, col, 2)
    for z_, t in ((3.85, '-30'), (5.2, '-12'), (6.55, '-3'), (6.82, '0')):
        d.text(P(-9.15, z_), t, font=font('mich', 10), fill=(150, 160, 172, 255), anchor='lm')
    sp(P(9.0, 6.78), 'BROWSE', f, col, 2)
    for b in B: sp(P(b['x'], b['z'] + b['h'] / 2 + 0.15), b['lab'], f, col, 2)
    sp(P(-10.2, KZ - 0.62), 'INPUT', f, col, 2); sp(P(10.2, KZ - 0.72), 'POWER', f, col, 2)
panel('art/eidolon_panel.png', spec, Z, [dict(k, label=l) for k, l in zip(knobs, LAB)], face='mich', col=col, size=19, dial='numbers', numsize=15, tick_len=0.08, extra=extra, spacing=3)
print('ok')
