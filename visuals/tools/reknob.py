"""Give every amp on the canvas its own knob design, label font and dial scale.
Updates amps/<name>.json in place (knob shape/material fields + new panel art file)."""
import json, sys
sys.path.insert(0, 'tools')
from panel_art import panel

STYX_LAB = ['GAIN', 'MASTER', 'BASS', 'MID', 'TREBLE', 'PRES', 'DEPTH']   # knob order in the styx-family specs
L7 = ['GAIN', 'BASS', 'MIDDLE', 'TREBLE', 'PRESENCE', 'DEPTH', 'MASTER']
CFG = {
 # name: knob shape, materials, pointer, label font, dial, label colour, labels, extras
 'styx':     dict(shape='knurl', mat='bakelite', mat2='silver', ptr='white', face='cdecr', dial='roman', col=(190, 205, 225, 255), labels=STYX_LAB, size=24, small_dial='none'),
 'hades':    dict(shape='knurl', mat='bakelite', mat2='silver', ptr=[1, 0.1, 0.06], face='grenze', dial='numbers', col=(225, 200, 200, 255), labels=STYX_LAB, size=28, small_dial='none'),
 'hecate':   dict(shape='knurl', mat='bakelite', mat2='gold', ptr='black', face='meta', dial='dots', col=(220, 200, 240, 255), labels=STYX_LAB, size=24, small_dial='none'),
 'nemesis':  dict(shape='knurl', mat='bakelite', mat2='brass', ptr='black', face='pirata', dial='lines', col=(200, 230, 205, 255), labels=STYX_LAB, size=30, small_dial='none'),
 'ares':     dict(shape='skirt2', mat='bakelite', mat2='silver', ptr=[1, 0.1, 0.05], face='playfair', dial='numbers', col=(215, 215, 222, 255), labels=['GAIN I', 'GAIN II', 'BASS', 'MID', 'TREBLE', 'PRES', 'MASTER'], size=24),
 'nyx':      dict(shape='cap', mat='silver', mat2='pewter', ptr=[0.2, 0.9, 1.0], face='marcellus', dial='dots', col=(200, 225, 235, 255), labels=L7, size=24),
 'thanatos': dict(shape='knurl', mat='bakelite', mat2='silver', ptr=[0.2, 0.9, 1.0], face='mich', dial='numbers', col=(200, 225, 235, 255), labels=L7, size=17, numsize=15),
 'vulcan':   dict(shape='knurl', mat='iron', mat2='copper', ptr=[1.0, 0.35, 0.05], face='rocker', dial='lines', col=(235, 120, 40, 255), labels=['GAIN', 'LOW', 'MID', 'HIGH', 'PRESENCE', 'DEPTH', 'MASTER'], size=30),
 'sanctum':  dict(shape='skirt2', mat='bakelite', mat2='gold', ptr='black', face='fell', dial='roman', col=(225, 210, 230, 255), labels=L7, size=28),
 'citadel':  dict(shape='knurl', mat='iron', mat2='brass', ptr='black', face='medieval', dial='roman', col=(205, 180, 120, 255), labels=['GAIN', 'BASS', 'MIDDLE', 'TREBLE', 'KEEP', 'PRESENCE', 'MASTER'], size=28),
 'eos':      dict(shape='ivory', mat='ivory', mat2='gold', ptr='gold', face='cormorant', dial='numbers', col=(70, 66, 60, 255), labels=['GAIN', 'BASS', 'MIDDLE', 'TREBLE', 'PRESENCE', 'MASTER'], size=30),
 'umbra':    dict(shape='chicken', mat='bakelite', ptr=[1, 0.08, 0.04], face=None, dial='lines', col=(150, 146, 152, 255), labels=None, size=0),
 'persephone': dict(shape='knurl', mat='white', mat2='rosegold', ptr='black', face='almendra', dial='numbers', col=(112, 100, 92, 255), labels=L7, size=30),
 'lamia':    dict(shape='skirt2', mat='bakelite', mat2='silver', ptr=[1, 0.08, 0.05], face='frak', dial='roman', col=(195, 190, 196, 255), labels=['Gain', 'Bass', 'Middle', 'Treble', 'Presence', 'Resonance', 'Master'], size=36),
 'melinoe':  dict(shape='knurl', mat='bakelite', mat2='silver', ptr=[0.6, 0.25, 1.0], face='cdecr', dial='dots', col=(195, 190, 205, 255), labels=L7, size=26),
}

for name, c in CFG.items():
    s = json.load(open(f'amps/{name}.json'))
    z = [z for z in s['zones'] if z.get('knobs')][0]
    big = max(k['r'] for k in z['knobs'])
    for k in z['knobs']:
        k.pop('style', None)
        k.update(shape=c['shape'], mat=c['mat'], mat2=c.get('mat2', c['mat']), ptr=c['ptr'])
        if 'gem' in c: k['gem'] = c['gem']
    out = f'art/{name}_panel3.png'
    if name == 'umbra':          # no text on Umbra: dial lines only
        ks = [dict(k) for k in z['knobs']]
        panel(out, s, z['z'], ks, dial='lines', col=c['col'], size=20, tick_len=0.13)
    else:
        ks = []
        for k, lab in zip(z['knobs'], c['labels']):
            kk = dict(k, label=lab)
            if k['r'] < big * 0.8 and c.get('small_dial'):
                kk['dial'] = c['small_dial']
            ks.append(kk)
        tog = [(t[0], t[1], 'STBY' if i == 0 else 'MODE') for i, t in enumerate(z.get('toggles', []))]
        jw = [(j['x'], j['z'] - 0.1, 'POWER') for j in z.get('jewels', [])]
        if c['face'] == 'frak':
            tog = [(a, b, 'Standby' if l == 'STBY' else 'Mode') for a, b, l in tog]; jw = [(a, b, 'Power') for a, b, l in jw]
        za, zb = z['z']
        panel(out, s, (za, zb), ks, toggles=tog, jewels=jw, face=c['face'], col=c['col'], size=c['size'], dial=c['dial'],
              numsize=c.get('numsize'), numface=('marcellus' if c['face'] in ('frak', 'grenze', 'pirata', 'rocker', 'uncial', 'medieval') else None), lines=[(-s['size'][0] / 2 + s['border'] + 0.5, za + 0.28, s['size'][0] / 2 - s['border'] - 0.5, za + 0.28, 2)])
    z['art'] = out
    json.dump(s, open(f'amps/{name}.json', 'w'), indent=1)
    print('ok', name)
