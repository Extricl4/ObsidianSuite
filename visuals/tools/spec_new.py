"""Specs + panel art for the ten new amps. python3 -I tools/spec_new.py [name ...]"""
import json, sys
sys.path.insert(0, 'tools')
from panel_art import panel

KX = [-8.5, -6.0, -3.5, -1.0, 1.5, 4.0, 6.5]; KZ = 2.2975
V = [0.68, 0.5, 0.44, 0.56, 0.5, 0.58, 0.4]
L7 = ['GAIN', 'BASS', 'MIDDLE', 'TREBLE', 'PRESENCE', 'DEPTH', 'MASTER']

A = {
 'strix': dict(title='Strix · vampire gothic', relief=dict(color=[0.13, 0.12, 0.12], cavity=0.3, metal=0.85, rough=0.36, hi_at=0.75, grain=[120, 0.05]),
               holes=True, tubes=dict(xs=[-1.07, 0, 1.07, -7.6, -4.7, 4.7, 7.6], color=[1, 0.06, 0.04], z=6.85, h=2.6), recess=2.4,
               body=dict(color=[0.1, 0.008, 0.012], rough=0.55, bump=['leather', 40, 0.5]), corners=1.5, glow=[1, 0.06, 0.04],
               panel=dict(color=[0.012, 0.01, 0.011], metal=0.4, rough=0.4), knob=dict(shape='skirt2', mat='bakelite', mat2='silver', ptr=[1, 0.08, 0.05]),
               face='texturina', dial='roman', col=(205, 175, 180, 255), size=30, labels=L7),
 'astraea': dict(title='Astraea · tarot star', relief=dict(color=[1.0, 0.76, 0.36], low=[0.008, 0.015, 0.055], lo_at=0.14, hi_at=0.36, metal=0.15, metal_hi=1.0, rough=0.28, grain=[160, 0.03]),
               body=dict(color=[0.01, 0.013, 0.035], rough=0.6, bump=['leather', 40, 0.5]), glow=[0.55, 0.72, 1.0], graze=[1, 0.92, 0.8],
               panel=dict(color=[0.008, 0.012, 0.035], metal=0.35, rough=0.38), knob=dict(shape='knurl', mat='bakelite', mat2='gold', ptr='black'),
               face='macondo', dial='dots', col=(228, 192, 112, 255), size=32, labels=L7),
 'tyche': dict(title='Tyche · tarot wheel', relief=dict(color=[0.88, 0.64, 0.32], low=[0.04, 0.008, 0.06], lo_at=0.14, hi_at=0.36, metal=0.15, metal_hi=1.0, rough=0.32, grain=[160, 0.03]),
               body=dict(color=[0.012, 0.01, 0.014], rough=0.6, bump=['leather', 40, 0.5]), glow=[0.65, 0.3, 1.0], graze=[1, 0.85, 0.7],
               panel=dict(color=[0.03, 0.008, 0.04], metal=0.3, rough=0.35), knob=dict(shape='knurl', mat='bakelite', mat2='brass', ptr='black'),
               face='marcellus', dial='numbers', col=(212, 172, 102, 255), size=28, labels=L7),
 'midgard': dict(title='Midgard · Norse', relief=dict(color=[0.22, 0.12, 0.055], cavity=0.35, rough=0.75, hi_at=0.7, grain=[22, 0.3]),
               body=dict(color=[0.03, 0.025, 0.022], rough=0.85, bump=['leather', 30, 0.6]), glow=[0.4, 0.75, 1.0], graze=[0.75, 0.88, 1.0],
               panel=dict(color=[0.045, 0.043, 0.042], metal=0.8, rough=0.6, bump=['noise', 14, 0.35]), knob=dict(shape='knurl', mat='iron', mat2='bronze', ptr=[0.4, 0.8, 1.0]),
               face='uncial', dial='dots', col=(200, 222, 236, 255), size=30, labels=['GAIN', 'BASS', 'MIDDLE', 'TREBLE', 'PRESENCE', 'FROST', 'MASTER']),
 'avalon': dict(title='Avalon · medieval', relief=dict(color=[0.6, 0.56, 0.48], cavity=0.5, rough=0.9, hi_at=0.65, grain=[70, 0.18]),
               body=dict(color=[0.04, 0.035, 0.03], rough=0.85, bump=['leather', 30, 0.6]), glow=[1.0, 0.55, 0.2], graze=[1.0, 0.75, 0.5],
               panel=dict(color=[0.04, 0.038, 0.036], metal=0.7, rough=0.55, bump=['noise', 14, 0.3]), knob=dict(shape='skirt2', mat='bakelite', mat2='brass', ptr='black'),
               face='fell', dial='roman', col=(218, 192, 142, 255), size=32, labels=L7),
 'hyperion': dict(title='Hyperion · baroque gold', relief=dict(color=[1.0, 0.74, 0.34], low=[0.13, 0.008, 0.015], lo_at=0.14, hi_at=0.34, metal=0.2, metal_hi=1.0, rough=0.26, grain=[160, 0.03]),
               body=dict(color=[0.1, 0.007, 0.012], rough=0.5, bump=['leather', 40, 0.45]), glow=[1.0, 0.7, 0.3], graze=[1, 0.88, 0.7],
               panel=dict(color=[0.014, 0.01, 0.01], metal=0.4, rough=0.35), knob=dict(shape='cap', mat='bakelite', mat2='gold', ptr='black'),
               face='cdec', dial='numbers', col=(230, 190, 110, 255), size=26, labels=L7),
 'lilith': dict(title='Lilith · gothic glass', relief=dict(color=[0.07, 0.066, 0.07], cavity=0.4, metal=0.3, rough=0.55, hi_at=0.75, grain=[90, 0.08]),
               holes=True, glass=True, recess=1.2,
               body=dict(color=[0.012, 0.011, 0.013], rough=0.6, bump=['leather', 40, 0.5]), corners=1.4, glow=[1, 0.08, 0.12],
               panel=dict(color=[0.012, 0.011, 0.013], metal=0.4, rough=0.4), knob=dict(shape='knurl', mat='bakelite', mat2='silver', ptr=[1, 0.08, 0.06]),
               face='grenze', dial='lines', col=(215, 192, 205, 255), size=32, labels=L7),
 'hermes': dict(title='Hermes · alchemy', relief=dict(color=[0.86, 0.64, 0.32], low=[0.05, 0.17, 0.13], lo_at=0.14, hi_at=0.36, metal=0.2, metal_hi=1.0, rough=0.32, grain=[120, 0.05]),
               body=dict(color=[0.01, 0.022, 0.018], rough=0.6, bump=['leather', 40, 0.5]), glow=[0.2, 1.0, 0.5], graze=[0.9, 1, 0.85],
               panel=dict(color=[0.01, 0.025, 0.02], metal=0.35, rough=0.38), knob=dict(shape='knurl', mat='bakelite', mat2='brass', ptr=[0.2, 1.0, 0.5]),
               face='meta', dial='dots', col=(215, 178, 104, 255), size=26, labels=L7),
 'thoth': dict(title='Thoth · grimoire', relief=dict(color=[0.85, 0.62, 0.3], low=[0.13, 0.06, 0.025], lo_at=0.3, hi_at=0.62, metal=0.0, metal_hi=1.0, rough=0.45, grain=[90, 0.1]),
               body=dict(color=[0.07, 0.035, 0.016], rough=0.7, bump=['leather', 34, 0.6]), glow=[1.0, 0.6, 0.2], graze=[1, 0.82, 0.6],
               panel=dict(color=[0.03, 0.016, 0.01], metal=0.2, rough=0.5, bump=['leather', 50, 0.3]), knob=dict(shape='knurl', mat='bone', mat2='brass', ptr='black'),
               face='cormorant', dial='roman', col=(220, 180, 110, 255), size=32, labels=L7),
 'euterpe': dict(title='Euterpe · gothic organ', relief=dict(color=[0.72, 0.72, 0.75], low=[0.045, 0.028, 0.018], lo_at=0.2, hi_at=0.5, metal=0.1, metal_hi=1.0, rough=0.3, grain=[120, 0.04]),
               body=dict(color=[0.012, 0.011, 0.013], rough=0.6, bump=['leather', 40, 0.5]), corners=1.3, glow=[0.6, 0.3, 1.0], graze=[0.85, 0.8, 1.0],
               panel=dict(color=[0.012, 0.011, 0.014], metal=0.4, rough=0.38), knob=dict(shape='skirt2', mat='bakelite', mat2='pewter', ptr='black'),
               face='cinzel', dial='numbers', col=(208, 200, 222, 255), size=26, labels=L7),
}


def build(name):
    a = A[name]
    zr = dict(type='relief', z=[4.25, 9.8], height=f'art/{name}_h.png', depth=0.42, res=56, **a['relief'])
    if a.get('holes'):
        zr['holes'] = f'art/{name}_holes.png'
    if a.get('glass'):
        zr['glass'] = f'art/{name}_glass.png'; zr['glass_strength'] = 2.2
        zr['glass_lights'] = [dict(x=0, color=[1, 0.15, 0.2], energy=40, size=4), dict(x=-7, color=[0.6, 0.1, 0.8], energy=25), dict(x=7, color=[0.6, 0.1, 0.8], energy=25)]
    if a.get('tubes'):
        t = a['tubes']
        zr['tubes'] = [dict(x=x, z=t['z'], h=t['h'], r=0.36, color=t['color'], strength=9, light=50) for x in t['xs']]
        zr['ambient'] = [dict(x=x, energy=80, color=t['color'], size=5) for x in (-7, 0, 7)]
    if a.get('graze'):
        zr['graze'] = [dict(color=a['graze'], energy=65, tilt=-25, out=1.0, top=0.4)]
    knobs = [dict(x=x, z=KZ, r=0.42, v=v, **a['knob']) for x, v in zip(KX, V)]
    for k in knobs:
        k.setdefault('mat2', k['mat'])
    zp = dict(type='panel', z=[0.7, 4.25], art=f'art/{name}_panel.png', artemis=0.15, **a['panel'], knobs=knobs,
              toggles=[[8.4, KZ]], jewels=[dict(x=9.5, z=KZ, color=a['glow'], r=0.28, strength=28)], jacks=[[-10.3, KZ, 'jack']])
    spec = dict(size=[24, 10, 10.5], border=0.7, recess=a.get('recess', 1.0), bevel=0.2, rimcolor=[min(1, c + 0.3) for c in a['glow']],
                body=a['body'], handle=dict(w=8.5, h=1.2, color=[c * 1.2 for c in a['body']['color']]),
                glow_strips=[dict(z=4.25, color=a['glow'], strength=4, w=22.54)], zones=[zr, zp])
    if a.get('corners'):
        spec['corners'] = 'chrome'; spec['chrome_size'] = a['corners']
    json.dump(spec, open(f'amps/{name}.json', 'w'), indent=1)
    nf = 'marcellus' if a['face'] in ('grenze', 'uncial', 'texturina') else None
    panel(f'art/{name}_panel.png', spec, (0.7, 4.25), [dict(k, label=l) for k, l in zip(knobs, a['labels'])],
          toggles=[(8.4, KZ, 'STANDBY')], jewels=[(9.5, KZ - 0.15, 'POWER')], face=a['face'], col=a['col'], size=a['size'], dial=a['dial'],
          numface=nf, lines=[(-10.9, 0.98, 10.9, 0.98, 2)])
    print('ok', name)


if __name__ == '__main__':
    for n in (sys.argv[1:] or list(A)):
        build(n)
