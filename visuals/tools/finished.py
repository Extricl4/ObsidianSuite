"""Specs for the finished, capture-backed amps (interactive page).
moirai_green / moirai_blue / moirai_red : EVH 5150 III Ivory, one look per channel (Nemesis / Styx / Hades skins)
umbra_mk5 : Mesa Mark V          citadel_iic : Mesa Mark IIC+
Writes amps/<key>.json and art/<key>_panel.png. Original amp specs are left untouched."""
import json, copy, sys
sys.path.insert(0, 'tools')
from panel_art import panel
_src = open('tools/reknob.py').read().split('for name, c in CFG.items()')[0]
_ns = {}; exec(_src.replace('from panel_art import panel', ''), _ns); CFG = _ns['CFG']   # knob/font config only, no side effects

def load(n): return json.load(open(f'amps/{n}.json'))
def kzone(s): return [z for z in s['zones'] if z.get('knobs')][0]
def accent(s, za): return [(-s['size'][0] / 2 + s['border'] + 0.5, za + 0.28, s['size'][0] / 2 - s['border'] - 0.5, za + 0.28, 2)]

# ---------- Moirai: EVH 5150 III, three skins ----------
CH = [('green', 'nemesis', [0.25, 1.0, 0.4]), ('blue', 'styx', [0.35, 0.6, 1.0]), ('red', 'hades', [1.0, 0.12, 0.06])]
LAB = ['GAIN', 'VOLUME', 'LOW', 'MID', 'HIGH', 'PRES', 'RESO']   # spec knob order: gain, master, bass, mid, treble, pres, depth
BX = [6.45, 7.4, 8.35]; BN = ['CLEAN', 'CRUNCH', 'LEAD']
for key, src, _ in CH:
    s = load(src); z = kzone(s); c = CFG[src]
    z.pop('toggles', None)
    z['jewels'] = [dict(x=9.55, z=1.9, color=z['jewels'][0]['color'], r=0.26, id='power')]
    z['buttons'] = [dict(id=f'ch{i+1}', x=x, z=1.9, w=0.56, h=0.4, cap=[0.03, 0.03, 0.032], metal=0.6, led=col, ledr=0.06)
                    for i, (x, (_, _, col)) in enumerate(zip(BX, CH))]
    ks = [dict(k, label=l) for k, l in zip(z['knobs'], LAB)]
    for kk in ks:
        if kk['r'] < 0.5 and c.get('small_dial'): kk['dial'] = c['small_dial']
    out = f'art/moirai_{key}_panel.png'
    face = c['face']
    panel(out, s, tuple(z['z']), ks, toggles=[(x, 1.9, n) for x, n in zip(BX, BN)], jewels=[(9.55, 1.8, 'POWER')],
          face=face, col=c['col'], size=c['size'], dial=c['dial'], numsize=c.get('numsize'),
          numface=('marcellus' if face in ('frak', 'grenze', 'pirata', 'rocker', 'uncial', 'medieval') else None), lines=accent(s, z['z'][0]))
    z['art'] = out
    json.dump(s, open(f'amps/moirai_{key}.json', 'w'), indent=1); print('ok moirai', key)

# ---------- Umbra: Mesa Mark V ----------
s = load('umbra'); z = [q for q in s['zones'] if q.get('knobs')][0]
z.pop('toggles', None)
z['jewels'] = [dict(x=-9.4, z=1.62, color=[1.0, 0.05, 0.02], r=0.24, strength=40, id='power')]
z['buttons'] = [dict(id='channel', x=6.0, z=1.55, w=0.52, h=0.34, cap=[0.025, 0.025, 0.027], metal=0.5),
                dict(id='mode', x=7.4, z=1.55, w=0.52, h=0.34, cap=[0.025, 0.025, 0.027], metal=0.5)]
leds = []
for bid, bx in (('ch', 6.0), ('md', 7.4)):
    for i, dx in enumerate((-0.2, 0, 0.2)):
        leds.append(dict(id=f'{bid}{i+1}', x=bx + dx, z=2.0, w=0.1, h=0.06, color=[1.0, 0.06, 0.03], st=12))
z['leds'] = leds
UL = ['GAIN', 'TREBLE', 'MIDDLE', 'BASS', 'PRESENCE', 'MASTER', 'OUTPUT']
ks = [dict(k, label=l) for k, l in zip(z['knobs'], UL)]
col = (150, 146, 152, 255)
def umbra_extra(d, P, col):
    from panel_art import font
    f = font('cinzel', 13)
    for bx in (6.0, 7.4):
        for i, dx in enumerate((-0.2, 0, 0.2)):
            x, y = P(bx + dx, 2.2); d.text((x, y), ('I', 'II', 'III')[i], font=f, fill=col, anchor='mm')
panel('art/umbra_mk5_panel.png', s, tuple(z['z']), ks, toggles=[(6.0, 1.55, 'CHANNEL'), (7.4, 1.55, 'MODE')],
      jewels=[(-9.4, 1.52, 'POWER')], face='cinzel', col=col, size=18, dial='lines', tick_len=0.13, spacing=3, extra=umbra_extra)
z['art'] = 'art/umbra_mk5_panel.png'
json.dump(s, open('amps/umbra_mk5.json', 'w'), indent=1); print('ok umbra')

# ---------- Citadel: Mesa Mark IIC+ ----------
s = load('citadel'); z = kzone(s); c = CFG['citadel']
z.pop('toggles', None)
z['jewels'] = [dict(z['jewels'][0], id='power')] if z.get('jewels') else []
jx, jz = z['jewels'][0]['x'], z['jewels'][0]['z']
z['buttons'] = [dict(id='lead', x=8.3, z=2.2, w=0.5, h=0.34, cap=[0.06, 0.05, 0.04], metal=0.8, led=[1.0, 0.1, 0.04], ledr=0.06)]
CL = ['VOLUME', 'TREBLE', 'BASS', 'MIDDLE', 'PRESENCE', 'LEAD DRIVE', 'MASTER']
ks = [dict(k, label=l) for k, l in zip(z['knobs'], CL)]
panel('art/citadel_iic_panel.png', s, tuple(z['z']), ks, toggles=[(8.3, 2.2, 'LEAD')], jewels=[(jx, jz - 0.1, 'POWER')],
      face=c['face'], col=c['col'], size=24, dial=c['dial'], numface='marcellus', lines=accent(s, z['z'][0]))
z['art'] = 'art/citadel_iic_panel.png'
json.dump(s, open('amps/citadel_iic.json', 'w'), indent=1); print('ok citadel')

# ---------- shared: button + LED row ----------
def btn_leds(bid, x, z, n, col, dz=0.42, step=0.17, w=0.08, h=0.055, st=12):
    return [dict(id=f'{bid}{i+1}', x=x + (i - (n - 1) / 2) * step, z=z + dz, w=w, h=h, color=col, st=st) for i in range(n)]
from panel_art import font, spaced

# ---------- Ares: ENGL Powerball II ----------
s = load('ares'); z = kzone(s); c = CFG['ares']; KZ = 2.35; RED = [1.0, 0.12, 0.06]
z.pop('toggles', None)
z['jewels'] = [dict(z['jewels'][0], x=10.3, id='power')]
z['buttons'] = [dict(id='channel', x=8.3, z=KZ, w=0.5, h=0.34, cap=[0.12, 0.12, 0.13], metal=0.9),
                dict(id='mic', x=9.25, z=KZ, w=0.5, h=0.34, cap=[0.12, 0.12, 0.13], metal=0.9)]
z['leds'] = btn_leds('ch', 8.3, KZ, 2, RED, step=0.2) + btn_leds('mic', 9.25, KZ, 4, RED, step=0.15, w=0.07)
AL = ['GAIN', 'BASS', 'MIDDLE', 'TREBLE', 'PRESENCE', 'DEPTH', 'MASTER']
def ares_extra(d, P, col):
    f = font('playfair', 13)
    for i, t in enumerate(('CR', 'LD')): spaced(d, P(8.3 + (i - 0.5) * 0.2, KZ + 0.62), t, f, col, 1)
    for i, t in enumerate(('57', '75', '906', 'MX')): spaced(d, P(9.25 + (i - 1.5) * 0.15, KZ + 0.62), t, font('playfair', 10), col, 0)
panel('art/ares_pb_panel.png', s, tuple(z['z']), [dict(k, label=l) for k, l in zip(z['knobs'], AL)],
      toggles=[(8.3, KZ, 'CHANNEL'), (9.25, KZ, 'MIC')], jewels=[(10.3, KZ - 0.1, 'POWER')], face=c['face'], col=c['col'], size=22,
      dial=c['dial'], lines=accent(s, z['z'][0]), extra=ares_extra)
z['art'] = 'art/ares_pb_panel.png'
json.dump(s, open('amps/ares_pb.json', 'w'), indent=1); print('ok ares')

# ---------- Strix: Hellwin 100W (Synyster settings) ----------
s = load('strix'); z = kzone(s); KZ = 2.2975; SR = [1, 0.06, 0.04]
for t in s['zones'][0]['tubes']:
    if abs(t['x']) < 2: t['x'] = {-1.07: -1.0, 0: 0, 1.07: 1.0}[t['x']]; t['z'] = 6.55; t['h'] = 2.3
z.pop('toggles', None)
z['jewels'] = [dict(z['jewels'][0], x=10.2, id='power')]
z['buttons'] = [dict(id='channel', x=8.2, z=KZ, w=0.5, h=0.34, cap=[0.03, 0.02, 0.022], metal=0.6),
                dict(id='voice', x=9.1, z=KZ, w=0.5, h=0.34, cap=[0.03, 0.02, 0.022], metal=0.6)]
z['leds'] = btn_leds('ch', 8.2, KZ, 2, SR, step=0.2) + btn_leds('vo', 9.1, KZ, 2, SR, step=0.2)
SL = ['GAIN', 'BASS', 'MIDDLE', 'TREBLE', 'PRESENCE', 'RESONANCE', 'MASTER']
def strix_extra(d, P, col):
    f = font('texturina', 14)
    for bx, labs in ((8.2, ('C', 'L')), (9.1, ('I', 'II'))):
        for i, t in enumerate(labs): spaced(d, P(bx + (i - 0.5) * 0.2, KZ + 0.62), t, f, col, 1)
panel('art/strix_hw_panel.png', s, tuple(z['z']), [dict(k, label=l) for k, l in zip(z['knobs'], SL)],
      toggles=[(8.2, KZ, 'CHANNEL'), (9.1, KZ, 'VOICE')], jewels=[(10.2, KZ - 0.1, 'POWER')], face='texturina', col=(205, 175, 180, 255),
      size=26, dial='roman', extra=strix_extra)
z['art'] = 'art/strix_hw_panel.png'
json.dump(s, open('amps/strix_hw.json', 'w'), indent=1); print('ok strix')

# ---------- Nihil: Fortin Meshuggah pack (Fortin / Sunn+Butterslax / blend) ----------
s = load('nihil'); z = kzone(s); KZ = 2.4; GR = [0.2, 1, 0.45]
z.pop('toggles', None)
z['jewels'] = [dict(z['jewels'][0], id='power')]
z['buttons'] = [dict(id='amp', x=-5.55, z=2.25, w=0.46, h=0.3, cap=[0.02, 0.02, 0.022], metal=0.5)]
z['leds'] = btn_leds('amp', -5.55, 2.25, 3, GR, dz=0.45, step=0.17, w=0.07, h=0.05)
NL = ['MASTER', 'PRESENCE', 'BASS', 'MIDDLE', 'TREBLE', 'DRIVE', 'GAIN']
def nihil_extra(d, P, col):
    f = font('marcellus', 20)
    for (x, zz, t) in ((4.2, 2.95, 'I'), (5.6, 2.95, 'HIGH'), (7.0, 2.95, 'II'), (4.2, 1.85, 'I'), (5.6, 1.85, 'LOW'), (7.0, 1.85, 'II')):
        spaced(d, P(x, zz), t, f, col, 2)
    fs = font('marcellus', 13)
    for i, t in enumerate(('F', 'S', 'B')): spaced(d, P(-5.55 + (i - 1) * 0.17, 2.95), t, fs, col, 0)
    spaced(d, P(-5.55, 1.72), 'AMP', fs, col, 2); spaced(d, P(-6.75, 1.72), 'POWER', fs, col, 2)
panel('art/nihil_fm_panel.png', s, (1.15, 3.65), [dict(k, label=l) for k, l in zip(z['knobs'], NL)], face='marcellus',
      col=(212, 212, 216, 255), size=19, dial='numbers', numsize=15, tick_len=0.06, extra=nihil_extra, spacing=2)
z['art'] = 'art/nihil_fm_panel.png'
json.dump(s, open('amps/nihil_fm.json', 'w'), indent=1); print('ok nihil')

# ---------- Achlys: Mesa Mark IV, blacked-out smooth matte ----------
KX = [-8.5, -6.0, -3.5, -1.0, 1.5, 4.0, 6.5]; KZ = 2.2975; V = [0.55, 0.6, 0.4, 0.5, 0.5, 0.65, 0.45]; CW = [0.75, 0.85, 1.0]
kn = [dict(x=x, z=KZ, r=0.42, v=v, shape='knurl', mat='bakelite', mat2='bakelite', ptr='white') for x, v in zip(KX, V)]
spec = dict(size=[24, 10, 10.5], border=0.7, recess=0.8, bevel=0.2, rimcolor=[0.75, 0.8, 0.9],
            body=dict(color=[0.011, 0.011, 0.012], rough=0.88, bump=['noise', 300, 0.015]),
            handle=dict(w=8.5, h=1.2, color=[0.012, 0.012, 0.013]),
            glow_strips=[dict(z=4.25, color=CW, strength=2.2, w=22.54)],
            zones=[dict(type='cloth', z=[4.25, 9.8], color=[0.006, 0.006, 0.007], weave=84, sheen=0.08, piping=[0.018, 0.018, 0.02]),
                   dict(type='panel', z=[0.7, 4.25], art='art/achlys_panel.png', artemis=0.06, color=[0.009, 0.009, 0.01], metal=0.0, rough=0.82,
                        bump=['noise', 300, 0.012], knobs=kn, jacks=[[-10.3, KZ, 'jack']],
                        buttons=[dict(id='channel', x=8.0, z=KZ, w=0.5, h=0.34, cap=[0.012, 0.012, 0.013], metal=0.0, rough=0.7),
                                 dict(id='voice', x=8.95, z=KZ, w=0.5, h=0.34, cap=[0.012, 0.012, 0.013], metal=0.0, rough=0.7)],
                        leds=btn_leds('ch', 8.0, KZ, 3, CW, step=0.17, w=0.07) + btn_leds('vo', 8.95, KZ, 5, CW, step=0.11, w=0.055),
                        jewels=[dict(x=10.1, z=KZ, color=CW, r=0.24, strength=18, id='power')])])
json.dump(spec, open('amps/achlys.json', 'w'), indent=1)
GL = (128, 128, 134, 255)
def achlys_extra(d, P, col):
    f = font('mich', 10)
    for i, t in enumerate(('R1', 'R2', 'LD')): spaced(d, P(8.0 + (i - 1) * 0.17, KZ + 0.62), t, f, col, 0)
panel('art/achlys_panel.png', spec, (0.7, 4.25), [dict(k, label=l) for k, l in zip(kn, ['GAIN', 'TREBLE', 'BASS', 'MIDDLE', 'PRESENCE', 'LEAD DRIVE', 'MASTER'])],
      toggles=[(8.0, KZ, 'CHANNEL'), (8.95, KZ, 'VOICE')], jewels=[(10.1, KZ - 0.1, 'POWER')], face='mich', col=GL, size=15, dial='lines',
      tick_len=0.1, spacing=3, extra=achlys_extra)
print('ok achlys')
