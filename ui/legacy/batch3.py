import math
from batch2 import *

def hdr(t): return f'<span style="font-size: 11px; font-weight: 600; letter-spacing: .24em; color: {TXT}">{t}</span>'
def disp(svgbody, w, h=88):
    return f'<div style="border-radius: 10px; padding: 6px; background: #08080a; box-shadow: inset 0 2px 8px rgba(0,0,0,.95), inset 0 0 0 1px rgba(255,255,255,.04)"><svg width="{w}" height="{h}" viewBox="0 0 {w} {h}" aria-hidden="true" style="display: block"><path d="M0 {h/2}H{w}" stroke="rgba(255,255,255,.05)"/>{svgbody}</svg></div>'
def wav_icons(active, n=5):
    names = list(WAVES.keys())+['Noise']
    paths = dict(WAVES); paths['Noise']='M2 12l2-5 2 9 2-7 2 6 2-8 2 7 2-4 2 6 2-5 2 3'
    return '<div style="display: flex; gap: 3px">' + ''.join(f'<button aria-label="{nm} wave" style="width: 32px; height: 24px; border-radius: 6px; border: none; background: {"rgba(255,255,255,.13)" if nm==active else "none"}; color: {TXT if nm==active else MUT}; cursor: pointer; padding: 0; display: flex; align-items: center; justify-content: center"><svg aria-hidden="true" width="18" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round" stroke-linejoin="round"><path d="{paths[nm]}"/></svg></button>' for nm in names) + '</div>'
def k2(items, cols=2, d=30, gap=16):
    return f'<div style="display: grid; grid-template-columns: repeat({cols}, minmax(0, 1fr)); row-gap: {gap}px; justify-items: center">' + ''.join(knob(d,v,n,t) for n,v,t in items) + '</div>'
def col(w, inner, last=False):
    sep = '' if last else 'box-shadow: inset -1px 0 0 rgba(255,255,255,.07); '
    return f'<div style="width: {w}px; flex: none; box-sizing: border-box; padding: 16px 18px; {sep}display: flex; flex-direction: column; gap: 14px">{inner}</div>'
def chipsrow(opts, active, w=None):
    return '<div class="seg" style="display: flex; gap: 3px; padding: 3px; border-radius: 8px; background: rgba(0,0,0,.4)">' + ''.join(f'<button style="flex: 1; height: 22px; padding: 0 6px; border-radius: 6px; border: none; background: {"rgba(255,255,255,.14)" if o==active else "none"}; color: {TXT if o==active else MUT}; font: inherit; font-size: 10px; font-weight: 600; letter-spacing: .06em; cursor: pointer; font-variant-numeric: tabular-nums">{o}</button>' for o in opts) + '</div>'

def synth_unit():
    # --- top strip
    lcd = (f'<div style="width: 200px; height: 62px; box-sizing: border-box; border-radius: 10px; padding: 0 16px; display: flex; align-items: center; justify-content: space-between; background: #08080a; box-shadow: inset 0 2px 10px rgba(0,0,0,.95), inset 0 0 0 1px rgba(255,255,255,.06), 0 1px 0 rgba(255,255,255,.06)">'
           f'<span style="font-size: 34px; font-weight: 600; line-height: 1; color: {A}; text-shadow: 0 0 14px rgba(243,234,216,.35)">A<span style="font-size: 14px">2</span></span>'
           f'<div style="display: flex; flex-direction: column; align-items: flex-end; gap: 2px"><span style="font-size: 10px; letter-spacing: .18em; color: {MUT}">TUNING</span><span style="font-size: 15px; font-variant-numeric: tabular-nums">440.0 Hz</span></div></div>')
    sw = lambda lbl, opts, a: f'<div style="display: flex; flex-direction: column; align-items: center; gap: 8px"><span style="{LBL}">{lbl}</span>{seg(opts,a,h=24)}</div>'
    top = (f'<div style="height: 112px; flex: none; display: flex; align-items: center; justify-content: space-between; padding: 0 30px">'
           f'<div style="display: flex; gap: 26px">{knob(40,.55,"SENSE","−24 dB")}{knob(40,.2,"GLIDE","40 ms")}</div>'
           f'{sw("VOICE",["LEGATO","RETRIG"],0)}{lcd}{sw("PLACE",["PRE AMP","POST AMP"],0)}'
           f'<div style="display: flex; gap: 26px">{knob(40,.62,"BLEND","62%")}{knob(40,.7,"LEVEL","−3.0 dB")}</div></div>')
    # --- pattern column
    steps = [.9,.4,.7,.4,1,.4,.7,.55]
    bars = ''.join(f'<div style="display: flex; flex-direction: column; align-items: center; gap: 5px"><div style="width: 16px; height: 86px; border-radius: 4px; position: relative; background: #0b0b0d; box-shadow: inset 0 1px 3px rgba(0,0,0,.9)"><div style="position: absolute; left: 2px; right: 2px; bottom: 2px; height: {int(v*80)}px; border-radius: 3px; background: {A if i==2 else "rgba(243,234,216,.35)"}"></div></div><span style="font-size: 9px; color: {A if i==2 else MUT}; font-variant-numeric: tabular-nums">{i+1}</span></div>' for i,v in enumerate(steps))
    pattern = col(232, f'<div style="display: flex; justify-content: space-between; align-items: center">{hdr("PATTERN")}<span style="font-size: 10px; letter-spacing: .14em; color: {MUT}">8 STEPS</span></div>'
        f'<div style="display: flex; justify-content: space-between">{bars}</div>'
        f'<div style="display: flex; flex-direction: column; gap: 6px"><span style="{LBL}">OCTAVE</span>{chipsrow(["−2","−1","0","+1","+2"],"+1")}</div>'
        f'<div style="display: flex; flex-direction: column; gap: 6px"><span style="{LBL}">RATE</span>{chipsrow(["1/4","1/8","1/16","1/32"],"1/16")}</div>')
    # --- envelope column
    adsr = f'<path d="M4 82 L34 10 L78 40 L150 40 L192 82" fill="none" stroke="{A}" stroke-width="2" stroke-linejoin="round"/><path d="M4 82 L34 10 L78 40 L150 40 L192 82 Z" fill="{A}" fill-opacity=".08"/>' + ''.join(f'<circle cx="{x}" cy="{y}" r="3.5" fill="#08080a" stroke="{A}" stroke-width="1.6"/>' for x,y in [(34,10),(78,40),(150,40)])
    env = col(232, f'{hdr("ENVELOPE")}{disp(adsr,196)}{k2([("ATTACK",.12,"8 ms"),("DECAY",.45,"620 ms"),("SUSTAIN",.6,"60%"),("RELEASE",.3,"280 ms")])}')
    # --- oscillators
    saw = f'<path d="M6 70 L60 18 V70 L114 18 V70 L168 18 V70 L196 44" fill="none" stroke="{A}" stroke-width="2" stroke-linejoin="round"/>'
    sq = f'<path d="M6 66 V22 H50 V66 H94 V22 H138 V66 H182 V22 H196" fill="none" stroke="{A}" stroke-width="2" stroke-linejoin="round"/>'
    def oschalf(name, svgw, active, items, last):
        return (f'<div style="flex: 1; display: flex; flex-direction: column; gap: 12px; padding: 0 16px; {"" if last else "box-shadow: inset -1px 0 0 rgba(255,255,255,.05); "}">'
                f'{hdr(name)}{disp(svgw,196)}{wav_icons(active)}<div style="display: flex; justify-content: space-between">' + ''.join(knob(30,v,n,t) for n,v,t in items) + '</div></div>')
    oscs = (f'<div style="width: 500px; flex: none; box-sizing: border-box; padding: 16px 2px; box-shadow: inset -1px 0 0 rgba(255,255,255,.07); display: flex">'
            + oschalf('OSC 1', saw, 'Saw', [('OCTAVE',.5,'0'),('DETUNE',.45,'−4 ct'),('LEVEL',.75,'−2 dB')], False)
            + oschalf('OSC 2', sq, 'Square', [('OCTAVE',.25,'−1'),('DETUNE',.55,'+6 ct'),('LEVEL',.55,'−7 dB')], True) + '</div>')
    # --- filter
    fpts = []
    for i in range(0,227,4):
        f = 20*(1000)**(i/226); fc=1800
        db = -10*math.log10(1+(f/fc)**4) + 6*math.exp(-((math.log2(f/fc))**2)*6)
        fpts.append(f'{i},{max(4,min(84, 34 - db*2.2)):.1f}')
    fcurve = f'<polyline points="{" ".join(fpts)}" fill="none" stroke="{A}" stroke-width="2" stroke-linejoin="round"/>'
    filt = col(276, f'{hdr("FILTER")}{disp(fcurve,226)}<div style="display: flex; gap: 8px">{chipsrow(["LP","BP","HP"],"LP")}{chipsrow(["12 dB","24 dB"],"24 dB")}</div>'
               f'{k2([("CUTOFF",.42,"1.80 kHz"),("RESO",.35,"35%"),("ENV AMT",.6,"60%"),("DRIVE",.15,"15%")],gap=10)}', last=True)
    inner = (f'<div style="flex: 1; min-height: 0; margin: 0 18px; border-radius: 16px; display: flex; background: linear-gradient(#141416, #0f0f11); box-shadow: inset 0 3px 12px rgba(0,0,0,.85), inset 0 0 0 1px rgba(255,255,255,.04), 0 1px 0 rgba(255,255,255,.06)">'
             f'{pattern}{env}{oscs}{filt}</div>')
    name = f'<div style="height: 34px; flex: none; display: flex; align-items: center; justify-content: center; gap: 14px"><span style="width: 60px; height: 1px; background: linear-gradient(90deg, transparent, rgba(255,255,255,.2))"></span><span style="font-size: 11px; font-weight: 600; letter-spacing: .5em; margin-right: -.5em; color: {DIM}">OBSIDIAN · SYNTH</span><span style="width: 60px; height: 1px; background: linear-gradient(90deg, rgba(255,255,255,.2), transparent)"></span></div>'
    screws = ''.join(f'<div style="position: absolute; {p}">{obs.screw()}</div>' for p in ['left: 14px; top: 14px','right: 14px; top: 14px','left: 14px; bottom: 14px','right: 14px; bottom: 14px'])
    return (f'<div style="width: 1300px; height: 556px; box-sizing: border-box; position: relative; border-radius: 26px; display: flex; flex-direction: column; background: radial-gradient(80% 60% at 50% 0%, rgba(255,255,255,.06), transparent 70%), linear-gradient(#232327, #18181b); box-shadow: inset 0 1px 0 rgba(255,255,255,.12), inset 0 -1px 0 rgba(0,0,0,.6), inset 0 0 0 1px rgba(255,255,255,.04), 0 24px 50px rgba(0,0,0,.7), 0 4px 10px rgba(0,0,0,.5)">'
            f'{screws}{top}{inner}{name}</div>')

html = pg('Obsidian · Synth','SYNTH',plain_stage(synth_unit()))
open(OUT+'FinalSynth.dc.html','w').write(html)
print('ok')
