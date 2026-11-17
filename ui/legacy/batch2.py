import json, math, random
from batch import *

# ---------------- EQ (parametric) ----------------
W, H = 1160, 290
FMIN, FMAX = 20, 20000
def fx_(f): return (math.log10(f)-math.log10(FMIN))/(math.log10(FMAX)-math.log10(FMIN))*W
def gy(db): return H/2 - db*(H/2-14)/15
BANDS = [ # n, type, f, gain, q
 (1,'shelfL',120,2.0,.7),(2,'bell',400,-3.0,1.0),(3,'bell',1200,3.5,.9),(4,'bell',3500,-2.0,2.0),(5,'shelfH',8000,1.5,.7)]
LC, HC = 80, 9500
def resp(f):
    db=0
    for n,t,fc,g,q in BANDS:
        o = math.log2(f/fc)
        if t=='bell': db += g*math.exp(-0.5*(o*q*1.6)**2)
        elif t=='shelfL': db += g/(1+math.exp(o*3))
        else: db += g/(1+math.exp(-o*3))
    db += 10*math.log10(1/(1+(LC/f)**4))
    db += 10*math.log10(1/(1+(f/HC)**4))
    return max(db,-15)
def eq_stage2():
    pts=[]
    for i in range(241):
        f = FMIN*(FMAX/FMIN)**(i/240)
        pts.append((fx_(f), gy(resp(f))))
    d = 'M' + ' L'.join(f'{x:.1f} {y:.1f}' for x,y in pts)
    random.seed(4)
    spec=[]
    for i in range(121):
        f = FMIN*(FMAX/FMIN)**(i/120)
        lvl = -2 - 6*math.log10(f/100)**2*0.6 + random.uniform(-2.2,2.2) - (8 if f<60 else 0)
        spec.append((fx_(f), gy(lvl-4)))
    sd = 'M0 '+str(H)+' L' + ' L'.join(f'{x:.1f} {y:.1f}' for x,y in spec) + f' L{W} {H} Z'
    grid=''
    for f in [50,100,200,500,1000,2000,5000,10000]:
        grid += f'<path d="M{fx_(f):.1f} 0V{H}" stroke="rgba(255,255,255,.05)"/>'
    for db in [-12,-6,0,6,12]:
        grid += f'<path d="M0 {gy(db):.1f}H{W}" stroke="rgba(255,255,255,{".13" if db==0 else ".05"})"/>'
    nodes=''
    for n,t,fc,g,q in BANDS:
        x=fx_(fc); y=gy(resp(fc)) if t=='bell' else gy(g/2+ (0))
        y = gy(g) if t=='bell' else gy(g*0.5)
        sel = n==3
        if sel: nodes += f'<circle cx="{x:.1f}" cy="{y:.1f}" r="15" fill="{A}" fill-opacity=".12"/>'
        nodes += f'<circle cx="{x:.1f}" cy="{y:.1f}" r="9" fill="{A if sel else "#1b1b1e"}" stroke="{A}" stroke-width="1.6"/><text x="{x:.1f}" y="{y+3.6:.1f}" text-anchor="middle" font-size="10" font-weight="600" fill="{"#111113" if sel else "#e9e6df"}" font-family="Saira, sans-serif">{n}</text>'
    for lbl,f in [('LC',LC),('HC',HC)]:
        x=fx_(f); y=gy(-3)
        nodes += f'<rect x="{x-12:.1f}" y="{y-8:.1f}" width="24" height="16" rx="5" fill="#1b1b1e" stroke="rgba(255,255,255,.5)"/><text x="{x:.1f}" y="{y+3.5:.1f}" text-anchor="middle" font-size="8" font-weight="600" fill="#e9e6df" font-family="Saira, sans-serif">{lbl}</text>'
    svgc = (f'<svg width="{W}" height="{H}" viewBox="0 0 {W} {H}" aria-hidden="true" style="display: block; overflow: visible">{grid}'
            f'<path d="{sd}" fill="rgba(255,255,255,.035)"/>'
            f'<path d="{d} L{W} {H/2} L0 {H/2} Z" fill="{A}" fill-opacity=".1"/><path d="{d}" fill="none" stroke="{A}" stroke-width="2.2" stroke-linejoin="round"/>{nodes}</svg>')
    bx, by = fx_(1200), gy(3.5)
    bubble = (f'<div style="position: absolute; left: {bx+14+18:.0f}px; top: {by-60+12:.0f}px; padding: 7px 10px; border-radius: 9px; background: rgba(20,20,23,.95); box-shadow: inset 0 0 0 1px rgba(255,255,255,.1), 0 8px 18px rgba(0,0,0,.6); display: flex; gap: 10px; font-size: 11px; font-variant-numeric: tabular-nums; white-space: nowrap">'
              f'<span style="color: {A}; font-weight: 600">BAND 3</span><span>1.20 kHz</span><span>+3.5 dB</span><span style="color: {DIM}">Q 0.9</span></div>')
    flab = ''.join(f'<span style="position: absolute; left: {fx_(f):.0f}px; transform: translateX(-50%); font-size: 10px; color: {MUT}">{t}</span>' for f,t in [(50,'50'),(100,'100'),(200,'200'),(500,'500'),(1000,'1k'),(2000,'2k'),(5000,'5k'),(10000,'10k')])
    dlab = ''.join(f'<span style="position: absolute; right: 0; top: {gy(db)-7:.0f}px; font-size: 10px; color: {MUT}; font-variant-numeric: tabular-nums">{"+" if db>0 else ""}{db}</span>' for db in [-12,-6,0,6,12])
    display = (f'<div style="display: flex; gap: 10px; padding: 16px 18px 10px 12px; border-radius: 18px; background: #09090a; box-shadow: inset 0 2px 12px rgba(0,0,0,.95), inset 0 0 0 1px rgba(255,255,255,.05)">'
               f'<div style="position: relative; width: 28px; height: {H}px">{dlab}</div>'
               f'<div style="display: flex; flex-direction: column; gap: 6px"><div style="position: relative">{svgc}{bubble}</div><div style="position: relative; height: 14px">{flab}</div></div></div>')
    chips = ''.join(f'<button style="width: 34px; height: 34px; border-radius: 50%; border: {"none" if n==3 else "1px solid rgba(255,255,255,.14)"}; background: {A if n==3 else "none"}; color: {"#111113" if n==3 else TXT}; font: inherit; font-size: 13px; font-weight: 600; cursor: pointer">{n}</button>' for n in range(1,6))
    strip = (f'<div style="display: flex; align-items: center; justify-content: space-between; padding: 14px 24px; border-radius: 18px; {CARD}">'
             f'<div style="display: flex; align-items: center; gap: 14px"><span style="{LBL}">BAND</span><div style="display: flex; gap: 6px">{chips}</div></div>'
             f'<div style="display: flex; align-items: center; gap: 14px"><span style="{LBL}">SHAPE</span>{seg(["SHELF","BELL","NOTCH"],1,h=28)}</div>'
             f'<div style="display: flex; align-items: center; gap: 22px">{knob(34,.62,"FREQ","1.20 kHz")}{knob(34,.62,"GAIN","+3.5 dB")}{knob(34,.35,"Q","0.9")}</div>'
             f'<span style="width: 1px; height: 64px; background: rgba(255,255,255,.08)"></span>'
             f'<div style="display: flex; align-items: center; gap: 22px">{knob(34,.2,"LOW CUT","80 Hz")}{knob(34,.75,"HIGH CUT","9.5 kHz")}</div></div>')
    return plain_stage(f'<div style="display: flex; flex-direction: column; gap: 14px">{display}{strip}</div>')

# ---------------- empty chain + picker ----------------
def slot(label, active=False):
    br = f'1.5px dashed {A}' if active else '1.5px dashed rgba(255,255,255,.16)'
    bg = 'rgba(255,255,255,.05)' if active else 'rgba(255,255,255,.015)'
    col = TXT if active else DIM
    return (f'<button style="width: 150px; height: 214px; flex: none; box-sizing: border-box; border-radius: 14px; border: {br}; background: {bg}; color: {col}; font: inherit; font-size: 11px; font-weight: 600; letter-spacing: .2em; cursor: pointer; display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 12px">'
            f'<span style="width: 44px; height: 44px; border-radius: 50%; display: flex; align-items: center; justify-content: center; background: rgba(255,255,255,.06)">{svg("plus",20,1.8)}</span>{label}</button>')
def picker_pop(title, cats, item):
    chips = ''.join(f'<button style="height: 28px; padding: 0 12px; border-radius: 8px; border: {"none" if i==0 else "1px solid rgba(255,255,255,.1)"}; background: {A if i==0 else "none"}; color: {"#111113" if i==0 else DIM}; font: inherit; font-size: 10px; font-weight: 600; letter-spacing: .14em; cursor: pointer">{c}</button>' for i,c in enumerate(cats))
    tile = (f'<div style="height: 100px; border-radius: 12px; border: 1px dashed rgba(255,255,255,.12); display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 10px">'
            f'<span style="width: 38px; height: 54px; border-radius: 5px; background: linear-gradient(#3a3a3f, #2a2a2e)"></span><span style="font-size: 10px; letter-spacing: .16em; color: {MUT}">COMING SOON</span></div>')
    search = (f'<div style="display: flex; align-items: center; gap: 8px; height: 34px; padding: 0 12px; border-radius: 10px; {WELL}">'
              f'<svg aria-hidden="true" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="{MUT}" stroke-width="2" stroke-linecap="round"><circle cx="11" cy="11" r="6.5"/><path d="M16 16l4.5 4.5"/></svg>'
              f'<label for="{item}-search" style="position: absolute; width: 1px; height: 1px; overflow: hidden; clip-path: inset(50%)">Search</label>'
              f'<input id="{item}-search" type="text" placeholder="Search {item}s" style="flex: 1; min-width: 0; border: none; background: none; outline: none; font: inherit; font-size: 13px; color: {TXT}"></div>')
    return (f'<div style="width: 470px; box-sizing: border-box; border-radius: 18px; padding: 16px; display: flex; flex-direction: column; gap: 12px; background: linear-gradient(#1c1c1f, #121214); box-shadow: inset 0 1px 0 rgba(255,255,255,.08), inset 0 0 0 1px rgba(255,255,255,.07), 0 24px 50px rgba(0,0,0,.8)">'
            f'<div style="display: flex; align-items: center; justify-content: space-between"><span style="font-size: 12px; font-weight: 600; letter-spacing: .26em">{title}</span>{iconbtn("close","Close",30)}</div>'
            f'{search}<div style="display: flex; flex-wrap: wrap; gap: 6px">{chips}</div>'
            f'<div style="display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 10px">{tile*6}</div>'
            f'<span style="font-size: 11px; color: {MUT}">Your {item}s will show up here once they are made.</span></div>')
def chain_stage(heading, label, title, cats, item):
    cable = '<span style="width: 26px; height: 2px; flex: none; background: #2c2c30" aria-hidden="true"></span>'
    slots = cable.join([slot(label, True)] + [slot(label) for _ in range(4)])
    tray = (f'<div style="position: relative; display: flex; align-items: center; padding: 30px 28px; border-radius: 22px; background: linear-gradient(#141416, #0e0e10); box-shadow: inset 0 2px 10px rgba(0,0,0,.8), inset 0 0 0 1px rgba(255,255,255,.04), 0 1px 0 rgba(255,255,255,.05)">'
            f'{obs.jack("IN")}{cable}{slots}{cable}{obs.jack("OUT")}'
            f'<div style="position: absolute; left: 236px; top: -46px">{picker_pop(title, cats, item)}</div></div>')
    head = f'<div style="width: 1036px; display: flex; justify-content: space-between"><span style="{LBL}">{heading}</span><span style="font-size: 11px; color: {MUT}">Drag to reorder once added</span></div>'
    return plain_stage(f'<div style="display: flex; flex-direction: column; gap: 70px; align-items: center">{head}{tray}</div>')

# ---------------- SYNTH ----------------
SC = {'OSC':'#e8b46a','FILTER':'#6fc3d8','ENV':'#b59bf0','LFO':'#8fd18a'}
WAVES = {'Sine':'M2 12c2.5-7 5-7 7.5 0s5 7 7.5 0 3-5 5-3','Triangle':'M2 16l5-9 5 9 5-9 5 9','Saw':'M2 16l8-10v10l8-10v10','Square':'M2 16V8h5v8h5V8h5v8h5'}
def waves(active):
    return '<div style="display: flex; gap: 4px">' + ''.join(f'<button aria-label="{n} wave" style="width: 36px; height: 26px; border-radius: 7px; border: none; background: {"rgba(255,255,255,.12)" if n==active else "rgba(0,0,0,.35)"}; color: {TXT if n==active else MUT}; cursor: pointer; padding: 0; display: flex; align-items: center; justify-content: center"><svg aria-hidden="true" width="22" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="{p}"/></svg></button>' for n,p in WAVES.items()) + '</div>'
def scard(title, key, body, w=246):
    c = SC[key]
    return (f'<div style="width: {w}px; flex: none; box-sizing: border-box; border-radius: 18px; padding: 16px 16px 18px; display: flex; flex-direction: column; gap: 16px; position: relative; overflow: hidden; {CARD}">'
            f'<span style="position: absolute; left: 0; right: 0; top: 0; height: 3px; background: {c}"></span>'
            f'<div style="display: flex; align-items: center; gap: 8px"><span style="width: 7px; height: 7px; border-radius: 50%; background: {c}"></span><span style="font-size: 12px; font-weight: 600; letter-spacing: .24em">{title}</span></div>{body}</div>')
def krow(items, d=34):
    return '<div style="display: flex; justify-content: space-between">' + ''.join(knob(d,v,n,t) for n,v,t in items) + '</div>'
def synth_stage():
    osc1 = scard('OSC 1','OSC', waves('Saw') + krow([('OCTAVE',.5,'0'),('DETUNE',.45,'−4 ct'),('LEVEL',.8,'80%')]))
    osc2 = scard('OSC 2','OSC', waves('Square') + krow([('OCTAVE',.25,'−1'),('DETUNE',.55,'+6 ct'),('LEVEL',.55,'55%')]))
    filt = scard('FILTER','FILTER', seg(['LOW','BAND','HIGH'],0,h=26) + krow([('CUTOFF',.42,'1.8 kHz'),('RESO',.35,'35%'),('ENV AMT',.6,'60%')]))
    adsr_svg = f'<svg width="214" height="34" viewBox="0 0 214 34" aria-hidden="true" style="display: block"><path d="M2 32 L30 4 L70 16 L150 16 L212 32" fill="none" stroke="{SC["ENV"]}" stroke-width="1.8" stroke-linejoin="round"/><path d="M2 32 L30 4 L70 16 L150 16 L212 32 Z" fill="{SC["ENV"]}" fill-opacity=".1"/></svg>'
    env = scard('ENVELOPE','ENV', f'<div style="border-radius: 8px; padding: 4px 0; background: rgba(0,0,0,.3)">{adsr_svg}</div>' + krow([('A',.15,'12 ms'),('D',.35,'240 ms'),('S',.6,'60%'),('R',.3,'300 ms')], d=26))
    lfo = scard('LFO','LFO', f'<div style="display: flex; align-items: center; justify-content: space-between">{waves("Triangle").replace("gap: 4px","gap: 3px")}</div>' + krow([('RATE',.4,'2.4 Hz'),('DEPTH',.3,'30%')]) + f'<div style="display: flex; align-items: center; justify-content: space-between"><span style="{LBL}">TARGET</span><button style="height: 28px; padding: 0 10px; border-radius: 8px; {RAISED}; color: {TXT}; font: inherit; font-size: 12px; cursor: pointer; display: flex; align-items: center; gap: 8px">Filter cutoff{svg("down",12,2,DIM)}</button></div>')
    # tracking panel
    scope_pts = ' '.join(f'{i*4},{30-22*((i*4%60)/60*2-1) if (i*4%60)<55 else 30}' for i in range(0,101))
    sp=[]
    for i in range(0,201):
        x=i*2.2; ph=(x%66)/66
        y=30 - 20*(1-2*ph)*math.exp(-x/600)
        sp.append(f'{x:.1f},{y:.1f}')
    scope = f'<svg width="440" height="60" viewBox="0 0 440 60" aria-hidden="true" style="display: block"><path d="M0 30H440" stroke="rgba(255,255,255,.08)"/><polyline points="{" ".join(sp)}" fill="none" stroke="{A}" stroke-width="1.6" stroke-linejoin="round"/></svg>'
    track = (f'<div style="width: 1306px; box-sizing: border-box; display: flex; align-items: center; justify-content: space-between; gap: 24px; padding: 16px 26px; border-radius: 18px; {CARD}">'
             f'<div style="display: flex; align-items: center; gap: 18px"><div style="display: flex; flex-direction: column; gap: 2px"><span style="{LBL}">TRACKING</span><span style="font-size: 40px; font-weight: 600; line-height: 1">A<span style="font-size: 16px; color: {DIM}">2</span></span></div>'
             f'<div style="display: flex; flex-direction: column; gap: 6px"><span style="font-size: 11px; color: {DIM}">MONO · follows your guitar</span>{seg(["MONO","LEGATO"],0,h=24)}</div></div>'
             f'<div style="border-radius: 12px; padding: 8px 12px; background: #09090a; box-shadow: inset 0 2px 8px rgba(0,0,0,.9)">{scope}</div>'
             f'<div style="display: flex; align-items: center; gap: 22px">{knob(34,.55,"SENSE","−24 dB")}{knob(34,.2,"GLIDE","40 ms")}{knob(46,.62,"BLEND","62% synth")}</div></div>')
    cards = f'<div style="display: flex; gap: 14px">{osc1}{osc2}{filt}{env}{lfo}</div>'
    return plain_stage(f'<div style="display: flex; flex-direction: column; gap: 16px; align-items: center">{track}{cards}</div>')

boards = [
 ('FinalAmp.dc.html','Amp', pg('Obsidian · Amp','AMP',stage_f())),
 ('FinalPedals.dc.html','Pedals · empty, picker open', pg('Obsidian · Pedals','PEDALS',chain_stage('PRE-AMP PEDALS','ADD PEDAL','ADD PEDAL',['ALL','DRIVE','BOOST','COMPRESSOR','WAH','PITCH'],'pedal'))),
 ('FinalSynth.dc.html','Synth', pg('Obsidian · Synth','SYNTH',synth_stage())),
 ('FinalCab.dc.html','Cab', pg('Obsidian · Cab','CAB',cab_stage())),
 ('FinalEQ.dc.html','EQ', pg('Obsidian · EQ','EQ',eq_stage2())),
 ('FinalFX.dc.html','FX · empty, picker open', pg('Obsidian · FX','FX',chain_stage('POST-AMP EFFECTS','ADD EFFECT','ADD EFFECT',['ALL','DELAY','REVERB','MODULATION','PITCH','DYNAMICS'],'effect'))),
 ('FinalPresets.dc.html','Preset browser', pg('Obsidian · Presets','AMP',stage_f(dim_overlay(presets_card())))),
 ('FinalSettings.dc.html','Settings', pg('Obsidian · Settings','AMP',stage_f(dim_overlay(settings_card())),settings=True)),
 ('FinalTuner.dc.html','Tuner open', pg('Obsidian · Tuner','AMP',stage_f(tuner_f()),tuner=True)),
 ('FinalLive.dc.html','Live tuner on', pg('Obsidian · Live tuner','AMP',stage_f(),live=True)),
]
for fn,_,h in boards: open(OUT+fn,'w').write(h)
json.dump([(fn,t) for fn,t,_ in boards], open('/tmp/batch2.json','w'))
print('ok')
