import json
from final import *
import obs

def hc(active): return header_c(active)
def plain_stage(inner, extra=''):
    return (f'<div style="flex: 1; min-height: 0; position: relative; overflow: hidden; display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 20px; background: radial-gradient(60% 70% at 50% 8%, rgba(255,255,255,.06), transparent 70%), linear-gradient(#0c0c0d, #121214)">{inner}{extra}</div>')
def pg(title, active, stage, **k):
    return doc(title, hc(active) + stage + foot(**k))
CARD = 'background: linear-gradient(#19191c, #111113); box-shadow: inset 0 1px 0 rgba(255,255,255,.07), inset 0 0 0 1px rgba(255,255,255,.05), 0 14px 30px rgba(0,0,0,.55)'

# ---- Pedals
def pedals_stage():
    arrow = f'<span style="flex: none; color: #3a3a3f; margin-bottom: 120px" aria-hidden="true">{svg("next",22,2)}</span>'
    empty = (f'<div style="display: flex; flex-direction: column; align-items: center; gap: 12px"><button style="width: 170px; height: 250px; box-sizing: border-box; border-radius: 10px; border: 1px dashed rgba(255,255,255,.18); background: rgba(255,255,255,.015); color: {DIM}; font: inherit; font-size: 11px; letter-spacing: .2em; cursor: pointer; display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 10px">{svg("plus",22,1.6)}ADD PEDAL</button>'
             f'<span style="font-size: 12px; letter-spacing: .2em; color: {MUT}">SLOT V</span><span style="height: 65px"></span></div>')
    row = f'<div style="display: flex; align-items: flex-end; gap: 18px">{obs.pedal("COMPRESSOR",True,"I")}{arrow}{obs.pedal("OVERDRIVE",True,"II")}{arrow}{obs.pedal("BOOST",True,"III")}{arrow}{obs.pedal("WAH",False,"IV")}{arrow}{empty}</div>'
    head = (f'<div style="display: flex; align-items: center; justify-content: space-between; width: 1090px"><span style="{LBL}">PRE-AMP CHAIN · DRAG TO REORDER</span>'
            f'<div style="display: flex; gap: 8px">{obs.chip("ALL ON")}{obs.chip("BYPASS ALL")}</div></div>')
    return plain_stage(head+row)

# ---- Cab
def cab_stage():
    dot = lambda x,y,t,c,ring: f'<button aria-label="Mic {t} position" style="position: absolute; left: {x}px; top: {y}px; width: 36px; height: 36px; margin: -18px 0 0 -18px; border-radius: 50%; border: none; padding: 0; cursor: grab; background: radial-gradient(circle at 50% 30%, #3a3a3f, #141416); box-shadow: 0 0 0 2px {ring}, 0 0 16px {ring}, 0 6px 12px rgba(0,0,0,.7); color: {c}; font-family: inherit; font-size: 13px; font-weight: 600">{t}</button>'
    cab = (f'<div style="display: flex; flex-direction: column; align-items: center; gap: 14px"><div style="position: relative; filter: drop-shadow(0 24px 20px rgba(0,0,0,.75))">{ph("CAB",400,400,True)}'
           f'{dot(132,150,"A",A,A)}{dot(262,268,"B","#f4f1ea","rgba(255,255,255,.55)")}</div>'
           f'<span style="font-size: 10px; letter-spacing: .2em; color: {MUT}">DRAG A / B TO MOVE THE MICS</span></div>')
    cards = ('<div style="display: flex; gap: 16px">'
             + obs.mic_card('MIC A','Dynamic 57',[('POSITION',.38,'0.38'),('DISTANCE',.25,'2 cm'),('LEVEL',.72,'0.0 dB'),('PAN',.5,'C')],active=True)
             + obs.mic_card('MIC B','Ribbon 121',[('POSITION',.62,'0.62'),('DISTANCE',.45,'6 cm'),('LEVEL',.55,'−4.5 dB'),('PAN',.5,'C')]) + '</div>')
    mix = (f'<div style="display: flex; align-items: center; gap: 16px; padding: 0 6px"><span style="{LBL}; width: 84px">MIC BLEND</span><span style="font-size: 12px; font-weight: 600; color: {A}">A</span>'
           f'<div style="flex: 1; position: relative; height: 6px; border-radius: 3px; {WELL}"><div style="position: absolute; left: 0; top: 0; bottom: 0; width: 60%; border-radius: 3px; background: {A}"></div><div style="position: absolute; left: 60%; top: -8px; width: 22px; height: 22px; margin-left: -11px; border-radius: 50%; background: radial-gradient(circle at 50% 25%, #4a4a50, #1b1b1e); box-shadow: 0 4px 8px rgba(0,0,0,.7), inset 0 1px 0 rgba(255,255,255,.2)"></div></div>'
           f'<span style="font-size: 12px; font-weight: 600">B</span><span style="font-size: 12px; color: {DIM}; font-variant-numeric: tabular-nums; width: 64px; text-align: right">60 / 40</span></div>')
    ir = (f'<div style="border-radius: 18px; padding: 14px 18px; display: flex; align-items: center; gap: 14px; {CARD}">'
          f'<span style="{LBL}; width: 84px">IR LOADER</span>'
          f'<button style="height: 36px; padding: 0 14px; border-radius: 10px; {RAISED}; color: {TXT}; font: inherit; font-size: 12px; letter-spacing: .1em; cursor: pointer; display: flex; align-items: center; gap: 8px">{svg("folder",15)}Load IR…</button>'
          f'<span style="font-size: 12px; color: {MUT}">Using built-in cab</span><div style="flex: 1"></div>'
          f'{hk("LOW CUT",.2,"80 Hz")}{hk("HIGH CUT",.75,"9.5 kHz")}</div>')
    right = f'<div style="display: flex; flex-direction: column; gap: 18px">{cards}{mix}{ir}</div>'
    return plain_stage(f'<div style="display: flex; align-items: center; gap: 46px">{cab}{right}</div>')

# ---- EQ
def eq_stage():
    bands = [('63',-2),('125',1),('250',3),('500',0),('1k',-1.5),('2k',2),('4k',4),('8k',1),('16k',-3)]
    W=920; H=190
    xs = [round(40 + i*(W-80)/8) for i in range(9)]
    ys = [round(H/2 - v*8) for _,v in bands]
    d = f'M0 {ys[0]} ' + ' '.join(f'S {x-40} {y}, {x} {y}' for x,y in zip(xs,ys)) + f' S {W-20} {ys[-1]}, {W} {ys[-1]}'
    grid = ''.join(f'<path d="M{x} 0V{H}" stroke="rgba(255,255,255,.05)"/>' for x in xs) + ''.join(f'<path d="M0 {y}H{W}" stroke="rgba(255,255,255,{".12" if y==H//2 else ".05"})"/>' for y in [H//2-48,H//2-24,H//2,H//2+24,H//2+48])
    pts = ''.join(f'<circle cx="{x}" cy="{y}" r="4" fill="#111113" stroke="{A}" stroke-width="2"/>' for x,y in zip(xs,ys))
    svgc = (f'<svg width="{W}" height="{H}" viewBox="0 0 {W} {H}" aria-hidden="true" style="display: block">{grid}'
            f'<path d="{d} V{H} H0 Z" fill="{A}" fill-opacity=".08"/><path d="{d}" fill="none" stroke="{A}" stroke-width="2"/>{pts}</svg>')
    disp = f'<div style="border-radius: 14px; padding: 10px; {WELL}">{svgc}</div>'
    def vs(f,v):
        top = 70 - v*12
        fill_top = min(top, 70); fill_h = abs(v*12)
        return (f'<div style="width: {round((W-80)/8)}px; display: flex; flex-direction: column; align-items: center; gap: 8px">'
                f'<span style="font-size: 11px; color: {TXT}; font-variant-numeric: tabular-nums">{"+" if v>0 else ""}{v:g} dB</span>'
                f'<div style="position: relative; width: 6px; height: 140px; border-radius: 3px; {WELL}"><div style="position: absolute; left: 0; right: 0; top: {fill_top}px; height: {fill_h}px; background: {A}; opacity: .7"></div>'
                f'<div style="position: absolute; left: 50%; top: {top}px; width: 26px; height: 12px; margin: -6px 0 0 -13px; border-radius: 4px; background: linear-gradient(#f4f1ea, #a9a59c); box-shadow: 0 3px 6px rgba(0,0,0,.7)"></div></div>'
                f'<span style="{LBL}">{f}</span></div>')
    sliders = f'<div style="display: flex; padding: 0 10px">' + ''.join(vs(f,v) for f,v in bands) + '</div>'
    side = (f'<div style="display: flex; flex-direction: column; align-items: center; gap: 18px; width: 130px">'
            f'<div style="display: flex; align-items: center; gap: 8px"><span style="{HL}">EQ</span>{toggle("EQ on",True)}</div>'
            f'{knob(46,.2,"LOW CUT","80 Hz")}{knob(46,.75,"HIGH CUT","9.5 kHz")}'
            f'<button style="height: 28px; padding: 0 12px; border-radius: 8px; border: 1px solid rgba(255,255,255,.1); background: none; color: {DIM}; font: inherit; font-size: 10px; font-weight: 600; letter-spacing: .16em; cursor: pointer">RESET</button></div>')
    body = f'<div style="display: flex; align-items: center; gap: 30px; padding: 22px 26px; border-radius: 20px; {CARD}"><div style="display: flex; flex-direction: column; gap: 14px">{disp}{sliders}</div>{side}</div>'
    return plain_stage(body)

# ---- FX
def fx_card(name, typ, on, knobs, extra=''):
    op = '' if on else 'opacity: .55; '
    ks = ''.join(knob(46,v,n,t) for n,v,t in knobs)
    return (f'<div style="width: 360px; box-sizing: border-box; border-radius: 20px; padding: 18px 20px; display: flex; flex-direction: column; gap: 18px; {CARD}">'
            f'<div style="display: flex; align-items: center; justify-content: space-between"><div style="display: flex; align-items: center; gap: 10px"><span style="font-size: 14px; font-weight: 600; letter-spacing: .26em">{name}</span>{toggle(f"{name.title()} on",on)}</div>'
            f'<button style="height: 30px; padding: 0 12px; border-radius: 9px; {RAISED}; color: {TXT}; font: inherit; font-size: 12px; cursor: pointer; display: flex; align-items: center; gap: 8px">{typ}{svg("down",12,2,DIM)}</button></div>'
            f'<div style="{op}display: flex; justify-content: space-between">{ks}</div>'
            f'<div style="{op}display: flex; gap: 8px; min-height: 26px">{extra}</div></div>')
def fx_stage():
    cards = (fx_card('DELAY','Digital',True,[('TIME',.42,'1/8'),('FEEDBACK',.35,'35%'),('MIX',.22,'22%')], obs.chip('SYNC',True)+obs.chip('PING-PONG')+obs.chip('DUCK'))
           + fx_card('REVERB','Plate',True,[('SIZE',.55,'55%'),('DECAY',.4,'2.1 s'),('MIX',.18,'18%')], obs.chip('PRE-DELAY 20 ms'))
           + fx_card('MODULATION','Chorus',False,[('RATE',.3,'0.8 Hz'),('DEPTH',.45,'45%'),('MIX',.3,'30%')]))
    head = (f'<div style="display: flex; align-items: center; justify-content: space-between; width: 1128px"><span style="{LBL}">POST-AMP EFFECTS · DRAG TO REORDER</span>'
            f'<div style="display: flex; gap: 8px">{obs.chip("ALL ON")}{obs.chip("BYPASS ALL")}</div></div>')
    return plain_stage(head + f'<div style="display: flex; gap: 24px">{cards}</div>')

# ---- overlays on amp stage
def dim_overlay(card):
    return f'<div style="position: absolute; inset: 0; background: rgba(6,6,7,.8); backdrop-filter: blur(8px); -webkit-backdrop-filter: blur(8px); display: flex; align-items: center; justify-content: center">{card}</div>'

def presets_card():
    folders = [('Factory',48,False),('My presets',12,True),('Clean',9,False),('Rhythm',21,False),('Lead',11,False),('Ambient',7,False),('Favorites',6,False)]
    fl = ''.join(f'<button style="height: 36px; border-radius: 9px; border: none; background: {"rgba(255,255,255,.07)" if on else "none"}; color: {"#f4f1ea" if on else DIM}; font: inherit; font-size: 13px; cursor: pointer; display: flex; align-items: center; gap: 10px; padding: 0 10px; text-align: left">{svg("folder",15)}<span style="flex: 1">{n}</span><span style="font-size: 11px; color: {MUT}; font-variant-numeric: tabular-nums">{c}</span></button>' for n,c,on in folders)
    left = (f'<div style="width: 220px; flex: none; display: flex; flex-direction: column; gap: 2px; padding: 14px 10px; box-sizing: border-box; box-shadow: inset -1px 0 0 rgba(255,255,255,.06)"><span style="{LBL}; padding: 0 10px 8px">FOLDERS</span>{fl}<div style="flex: 1"></div>'
            f'<button style="height: 34px; border-radius: 9px; border: 1px dashed rgba(255,255,255,.14); background: none; color: {DIM}; font: inherit; font-size: 11px; letter-spacing: .14em; cursor: pointer; display: flex; align-items: center; justify-content: center; gap: 8px">{svg("plus",13,2)}NEW FOLDER</button></div>')
    rows = [('Tight Wall','Rhythm head','Drop C · Chug',True,True),('Legion Crunch','Rhythm head','Crunch',False,False),('Glass Clean','Clean head','Clean · Chorus',False,True),('Solo Burn','Lead head','Lead · Delay',False,False),('Night Drive','Clean head','Ambient',False,False),('Styx Chug','Rhythm head','Drop B',False,True),('War Drum','Rhythm head','Drop A · Tight',False,False),('Halo Lead','Lead head','Lead · Reverb',False,False)]
    star = lambda f: f'<svg aria-hidden="true" width="14" height="14" viewBox="0 0 24 24" fill="{"currentColor" if f else "none"}" stroke="currentColor" stroke-width="1.8" stroke-linejoin="round"><path d="M12 3.5l2.6 5.4 5.9.8-4.3 4.1 1 5.8L12 16.8 6.8 19.6l1-5.8L3.5 9.7l5.9-.8z"/></svg>'
    rl = ''.join(f'<button style="height: 38px; flex: none; border-radius: 9px; border: none; background: {"rgba(255,255,255,.08)" if sel else "none"}; box-shadow: {f"inset 2px 0 0 {A}" if sel else "none"}; color: {"#f4f1ea" if sel else TXT}; font: inherit; font-size: 13px; cursor: pointer; display: grid; grid-template-columns: 22px minmax(0, 1.4fr) minmax(0, 1fr) minmax(0, 1fr); align-items: center; gap: 10px; padding: 0 12px; text-align: left"><span style="color: {A if fav else "#4a4a50"}">{star(fav)}</span><span style="font-weight: {"600" if sel else "400"}">{n}</span><span style="color: {DIM}; font-size: 12px">{amp}</span><span style="color: {MUT}; font-size: 12px">{tags}</span></button>' for n,amp,tags,sel,fav in rows)
    search = (f'<div style="display: flex; align-items: center; gap: 8px; height: 36px; padding: 0 12px; border-radius: 10px; {WELL}; flex: 1">'
              f'<svg aria-hidden="true" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="{MUT}" stroke-width="2" stroke-linecap="round"><circle cx="11" cy="11" r="6.5"/><path d="M16 16l4.5 4.5"/></svg>'
              f'<label for="preset-search" style="position: absolute; width: 1px; height: 1px; overflow: hidden; clip-path: inset(50%)">Search presets</label>'
              f'<input id="preset-search" type="text" placeholder="Search presets" style="flex: 1; min-width: 0; border: none; background: none; outline: none; font: inherit; font-size: 13px; color: {TXT}"></div>')
    hdr = f'<div style="display: grid; grid-template-columns: 22px minmax(0, 1.4fr) minmax(0, 1fr) minmax(0, 1fr); gap: 10px; padding: 0 12px; {LBL}"><span></span><span>NAME</span><span>AMP</span><span>TAGS</span></div>'
    mid = f'<div style="flex: 1; min-width: 0; display: flex; flex-direction: column; gap: 10px; padding: 14px 16px; box-sizing: border-box"><div style="display: flex; gap: 8px">{search}</div>{hdr}<div style="display: flex; flex-direction: column; gap: 2px">{rl}</div></div>'
    act = lambda t,primary=False: f'<button style="height: 36px; border-radius: 10px; border: {"none" if primary else "1px solid rgba(255,255,255,.1)"}; background: {A if primary else "none"}; color: {"#111113" if primary else TXT}; font: inherit; font-size: 11px; font-weight: 600; letter-spacing: .16em; cursor: pointer">{t}</button>'
    right = (f'<div style="width: 250px; flex: none; display: flex; flex-direction: column; gap: 12px; padding: 18px; box-sizing: border-box; box-shadow: inset 1px 0 0 rgba(255,255,255,.06)">'
             f'<span style="{LBL}">SELECTED</span><span style="font-size: 20px; font-weight: 500">Tight Wall</span>'
             f'<div style="display: flex; flex-direction: column; gap: 6px; font-size: 12px; color: {DIM}"><span>Folder · My presets</span><span>Amp · Rhythm head, 4×12</span><span>Pedals · Overdrive, Boost</span><span>Edited · today 00:14</span></div>'
             f'<div style="flex: 1"></div>{act("LOAD",True)}{act("RENAME")}{act("DUPLICATE")}{act("SHOW IN EXPLORER")}<div style="height: 1px; background: rgba(255,255,255,.08); margin: 4px 0"></div><button style="height: 36px; border-radius: 10px; border: 1px dashed rgba(255,255,255,.22); background: none; color: {TXT}; font: inherit; font-size: 11px; font-weight: 600; letter-spacing: .16em; cursor: pointer">LOAD DEFAULT</button><span style="font-size: 10px; color: {MUT}; text-align: center">Empty rig, every knob at 12 o\'clock</span></div>')
    top = f'<div style="height: 52px; display: flex; align-items: center; justify-content: space-between; padding: 0 12px 0 20px; box-shadow: inset 0 -1px 0 rgba(255,255,255,.06)"><span style="font-size: 13px; font-weight: 600; letter-spacing: .3em">PRESETS</span>{iconbtn("close","Close presets",32)}</div>'
    return f'<div style="width: 1120px; height: 540px; border-radius: 22px; overflow: hidden; display: flex; flex-direction: column; background: linear-gradient(#1a1a1d, #0f0f11); box-shadow: inset 0 1px 0 rgba(255,255,255,.08), inset 0 0 0 1px rgba(255,255,255,.06), 0 30px 70px rgba(0,0,0,.8)">{top}<div style="flex: 1; min-height: 0; display: flex">{left}{mid}{right}</div></div>'

def settings_card():
    nav = ''.join(f'<button style="height: 38px; border-radius: 9px; border: none; background: {"rgba(255,255,255,.07)" if i==0 else "none"}; box-shadow: {f"inset 2px 0 0 {A}" if i==0 else "none"}; color: {"#f4f1ea" if i==0 else DIM}; font: inherit; font-size: 13px; font-weight: {"600" if i==0 else "400"}; cursor: pointer; text-align: left; padding: 0 14px">{n}</button>' for i,n in enumerate(['Audio','MIDI','Interface','Presets','About']))
    def row(lbl, hint, ctl):
        return f'<div style="display: flex; align-items: center; justify-content: space-between; gap: 24px; padding: 14px 0; box-shadow: inset 0 -1px 0 rgba(255,255,255,.05)"><div style="display: flex; flex-direction: column; gap: 3px"><span style="font-size: 13px; color: {TXT}">{lbl}</span><span style="font-size: 11px; color: {MUT}">{hint}</span></div>{ctl}</div>'
    dd = lambda t: f'<button style="height: 32px; min-width: 150px; padding: 0 12px; border-radius: 9px; {RAISED}; color: {TXT}; font: inherit; font-size: 12px; cursor: pointer; display: flex; align-items: center; justify-content: space-between; gap: 10px">{t}{svg("down",12,2,DIM)}</button>'
    content = (row('Oversampling','Higher sounds smoother, uses more CPU', seg(['1×','2×','4×'],1,h=26))
             + row('Quality when rendering','Used when you bounce or export', dd('Highest'))
             + row('Input calibration','Match the plugin to your interface', f'<div style="display: flex; align-items: center; gap: 12px">{hk("LEVEL",.5,"0.0 dB")}</div>')
             + row('Mute output while tuning','Silence the plugin when the tuner is open', toggle('Mute output while tuning',True))
             + row('Live tuner','Small tuner in the bottom bar while playing', toggle('Live tuner',False))
             + row('Interface size','Scale the whole window', dd('100%')))
    nav_col = f'<div style="width: 200px; flex: none; display: flex; flex-direction: column; gap: 2px; padding: 14px 10px; box-sizing: border-box; box-shadow: inset -1px 0 0 rgba(255,255,255,.06)">{nav}</div>'
    body = f'<div style="flex: 1; min-width: 0; padding: 6px 26px 14px; box-sizing: border-box; display: flex; flex-direction: column"><span style="{LBL}; padding: 14px 0 4px">AUDIO</span>{content}</div>'
    top = f'<div style="height: 52px; display: flex; align-items: center; justify-content: space-between; padding: 0 12px 0 20px; box-shadow: inset 0 -1px 0 rgba(255,255,255,.06)"><span style="font-size: 13px; font-weight: 600; letter-spacing: .3em">SETTINGS</span>{iconbtn("close","Close settings",32)}</div>'
    return f'<div style="width: 860px; height: 530px; border-radius: 22px; overflow: hidden; display: flex; flex-direction: column; background: linear-gradient(#1a1a1d, #0f0f11); box-shadow: inset 0 1px 0 rgba(255,255,255,.08), inset 0 0 0 1px rgba(255,255,255,.06), 0 30px 70px rgba(0,0,0,.8)">{top}<div style="flex: 1; min-height: 0; display: flex">{nav_col}{body}</div></div>'

boards = [
 ('FinalAmp.dc.html','Amp', pg('Obsidian · Amp','AMP',stage_f())),
 ('FinalPedals.dc.html','Pedals', pg('Obsidian · Pedals','PEDALS',pedals_stage())),
 ('FinalCab.dc.html','Cab', pg('Obsidian · Cab','CAB',cab_stage())),
 ('FinalEQ.dc.html','EQ', pg('Obsidian · EQ','EQ',eq_stage())),
 ('FinalFX.dc.html','FX', pg('Obsidian · FX','FX',fx_stage())),
 ('FinalPresets.dc.html','Preset browser', pg('Obsidian · Presets','AMP',stage_f(dim_overlay(presets_card())))),
 ('FinalSettings.dc.html','Settings', pg('Obsidian · Settings','AMP',stage_f(dim_overlay(settings_card())),settings=True)),
 ('FinalTuner.dc.html','Tuner open', pg('Obsidian · Tuner','AMP',stage_f(tuner_f()),tuner=True)),
 ('FinalLive.dc.html','Live tuner on', pg('Obsidian · Live tuner','AMP',stage_f(),live=True)),
]
for fn,_,h in boards: open(OUT+fn,'w').write(h)
json.dump([(fn,t) for fn,t,_ in boards], open('/tmp/batch.json','w'))
print('ok', len(boards))
