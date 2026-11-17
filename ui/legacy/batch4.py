import math, random, json
from batch3 import *

# ================= EQ 9-band, even grid =================
EW, EH = 1000, 220
F0, OCT = 31.25, 10            # 31.25 Hz .. 32 kHz, one octave per column
def ex(f): return math.log2(f/F0)/OCT*EW
def ey(db): return EH/2 - db*(EH/2-14)/15
B9 = [(1,63,0),(2,125,0),(3,250,0),(4,500,0),(5,1000,0),(6,2000,0),(7,4000,0),(8,8000,0),(9,16000,0)]
SEL = 6
LC2, HC2 = 20, 20000
def r9(f):
    db=0
    for n,fc,g in B9:
        o=math.log2(f/fc); db += g*math.exp(-0.5*(o*1.5)**2)
    if LC2>21: db += 10*math.log10(1/(1+(LC2/f)**4))
    if HC2<19500: db += 10*math.log10(1/(1+(f/HC2)**4))
    return max(db,-15)
def eq_stage3():
    pts=[(ex(F0*2**(OCT*i/300)), ey(r9(F0*2**(OCT*i/300)))) for i in range(301)]
    d='M'+' L'.join(f'{x:.1f} {y:.1f}' for x,y in pts)
    random.seed(7)
    spec=[(ex(F0*2**(OCT*i/150)), ey(-6-0.9*abs(OCT*i/150-3.2)**1.35+random.uniform(-2,2))) for i in range(151)]
    sd=f'M0 {EH} L'+' L'.join(f'{x:.1f} {y:.1f}' for x,y in spec)+f' L{EW} {EH} Z'
    grid=''.join(f'<path d="M{ex(F0*2**k):.1f} 0V{EH}" stroke="rgba(255,255,255,.06)"/>' for k in range(1,OCT))
    grid+=''.join(f'<path d="M0 {ey(db):.1f}H{EW}" stroke="rgba(255,255,255,{".14" if db==0 else ".06"})"/>' for db in [-12,-6,0,6,12])
    nodes=''
    for n,fc,g in B9:
        x=ex(fc); y=ey(g); sel=n==SEL
        if sel: nodes+=f'<circle cx="{x:.1f}" cy="{y:.1f}" r="15" fill="{A}" fill-opacity=".12"/>'
        nodes+=f'<circle cx="{x:.1f}" cy="{y:.1f}" r="9" fill="{A if sel else "#1b1b1e"}" stroke="{A}" stroke-width="1.6"/><text x="{x:.1f}" y="{y+3.6:.1f}" text-anchor="middle" font-size="10" font-weight="600" fill="{"#111113" if sel else "#e9e6df"}" font-family="Saira, sans-serif">{n}</text>'
    for lbl,f in [('LC',LC2),('HC',HC2)]:
        x=min(max(ex(f),14),EW-14); y=ey(-4)
        nodes+=f'<rect x="{x-12:.1f}" y="{y-8:.1f}" width="24" height="16" rx="5" fill="#1b1b1e" stroke="rgba(255,255,255,.5)"/><text x="{x:.1f}" y="{y+3.5:.1f}" text-anchor="middle" font-size="8" font-weight="600" fill="#e9e6df" font-family="Saira, sans-serif">{lbl}</text>'
    svgc=(f'<svg width="{EW}" height="{EH}" viewBox="0 0 {EW} {EH}" aria-hidden="true" style="display: block; overflow: visible">{grid}<path d="{sd}" fill="rgba(255,255,255,.035)"/>'
          f'<path d="{d} L{EW} {EH/2} L0 {EH/2} Z" fill="{A}" fill-opacity=".1"/><path d="{d}" fill="none" stroke="{A}" stroke-width="2.2" stroke-linejoin="round"/>{nodes}</svg>')
    bx,by=ex(2000),ey(3.5)
    bubble=(f'<div style="display: none; position: absolute; left: {bx+22:.0f}px; top: {by-40:.0f}px; padding: 7px 10px; border-radius: 9px; background: rgba(20,20,23,.95); box-shadow: inset 0 0 0 1px rgba(255,255,255,.1), 0 8px 18px rgba(0,0,0,.6); display: flex; gap: 10px; font-size: 11px; font-variant-numeric: tabular-nums; white-space: nowrap">'
            f'<span style="color: {A}; font-weight: 600">BAND 6</span><span>2.00 kHz</span><span>0 dB</span><span style="color: {DIM}">Q 1.4</span></div>')
    names={63:'63',125:'125',250:'250',500:'500',1000:'1k',2000:'2k',4000:'4k',8000:'8k',16000:'16k'}
    flab=''.join(f'<span style="position: absolute; left: {ex(f):.0f}px; transform: translateX(-50%); font-size: 10px; color: {MUT}">{t}</span>' for f,t in names.items())
    dlab=''.join(f'<span style="position: absolute; right: 0; top: {ey(db)-7:.0f}px; font-size: 10px; color: {MUT}; font-variant-numeric: tabular-nums">{"+" if db>0 else ""}{db}</span>' for db in [-12,-6,0,6,12])
    display=(f'<div style="display: flex; gap: 10px; padding: 16px 18px 10px 12px; border-radius: 18px; background: #09090a; box-shadow: inset 0 2px 12px rgba(0,0,0,.95), inset 0 0 0 1px rgba(255,255,255,.05)">'
             f'<div style="position: relative; width: 28px; height: {EH}px">{dlab}</div><div style="display: flex; flex-direction: column; gap: 6px"><div style="position: relative">{svgc}{bubble}</div><div style="position: relative; height: 14px">{flab}</div></div></div>')
    def fader(f,g):
        th=128; mid=th/2; pos=mid-g/12*(th/2-8)
        top=min(pos,mid); hgt=abs(pos-mid)
        return (f'<div style="position: absolute; left: {ex(f)-22:.0f}px; top: 0; width: 44px; display: flex; flex-direction: column; align-items: center; gap: 6px">'
                f'<span style="font-size: 10px; color: {TXT}; font-variant-numeric: tabular-nums; white-space: nowrap">{("+" if g>0 else "")}{g:g} dB</span>'
                f'<div class="fd" data-g="{g}" data-f="{f}" style="position: relative; width: 6px; height: {th}px; cursor: ns-resize; touch-action: none; border-radius: 3px; background: #050506; box-shadow: inset 0 1px 3px rgba(0,0,0,.9), 0 1px 0 rgba(255,255,255,.05)">'
                f'<span style="position: absolute; left: -6px; right: -6px; top: {mid-0.5:.0f}px; height: 1px; background: rgba(255,255,255,.18)"></span>'
                f'<span style="position: absolute; left: 0; right: 0; top: {top:.0f}px; height: {hgt:.0f}px; border-radius: 3px; background: {A}; opacity: .55"></span>'
                f'<span style="position: absolute; left: 50%; top: {pos:.0f}px; width: 30px; height: 14px; margin: -7px 0 0 -15px; border-radius: 4px; background: linear-gradient(#55555b, #2a2a2e 48%, #1a1a1d 52%, #3a3a3f); box-shadow: 0 4px 8px rgba(0,0,0,.7), inset 0 1px 0 rgba(255,255,255,.2)"><span style="position: absolute; left: 5px; right: 5px; top: 6px; height: 2px; background: {A}"></span></span></div></div>')
    wheels=''.join(fader(f,g) for n,f,g in B9)
    display=(f'<div style="display: flex; gap: 10px; padding: 16px 18px 14px 12px; border-radius: 18px; background: #09090a; box-shadow: inset 0 2px 12px rgba(0,0,0,.95), inset 0 0 0 1px rgba(255,255,255,.05)">'
             f'<div style="position: relative; width: 28px; height: {EH}px">{dlab}</div><div style="display: flex; flex-direction: column; gap: 6px"><div style="position: relative">{svgc}{bubble}</div><div style="position: relative; height: 14px">{flab}</div>'
             f'<div style="position: relative; height: 166px; margin-top: 8px; box-shadow: inset 0 1px 0 rgba(255,255,255,.05); padding-top: 12px"><div style="position: relative">{wheels}</div></div></div></div>')
    side = lambda k: f'<div style="width: 96px; flex: none; display: flex; flex-direction: column; align-items: center; justify-content: center">{k}</div>'
    strip=''
    EQP=[('Bass Boost',[5,4,2,0,0,0,0,0,0]),('Bass Cut',[-6,-4,-2,0,0,0,0,0,0]),('Tight Low',[-6,-2,1,0,0,0,0,0,0]),
         ('Rhythm',[-3,1,-1,-2.5,-1,1.5,3,1,-1]),('Tight Rhythm',[-5,0,-1,-3,-1,2,3.5,1.5,-1]),('Djent',[-6,-1,-2,-4,0,3,5,2,-1]),('Thrash',[-2,1,-1,-5,-3,1,4,2,0]),('Doom',[4,3,2,1,-2,-3,-2,-3,-5]),
         ('Lead',[-2,-1,0,2,4,3.5,1,-1,-2]),('Solo Cut-Through',[-3,-2,0,3,5,4,2,0,-2]),('Mid Boost',[0,0,1,3,4,3,1,0,0]),('Scoop',[3,2,0,-3,-5,-3,0,2,3]),
         ('Warm',[2,2,1,0,-1,-1.5,-2,-3,-4]),('Vintage',[1,2,2,1,0,-1,-2,-4,-6]),('Bright',[0,0,0,-1,0,2,3,4,4]),('Air',[0,0,0,0,0,1,2,4,5]),
         ('Clean Sparkle',[-1,0,0,-1,0,1,3,4,3]),('Mix Ready',[-4,-1,-1,-2,0,1,1.5,0.5,-1]),('Radio',[-12,-8,-2,2,4,3,0,-8,-12])]
    items=''.join(f'<button class="eqp" data-g="{",".join(str(x) for x in g)}" style="height: 32px; flex: none; border-radius: 8px; border: none; background: none; color: {TXT}; font: inherit; font-size: 13px; text-align: left; padding: 0 12px; cursor: pointer">{n}</button>' for n,g in EQP)
    lst=(f'<div class="eqlist" style="display: none; position: absolute; left: 0; top: 40px; z-index: 30; width: 230px; max-height: 330px; overflow: auto; padding: 6px; border-radius: 12px; flex-direction: column; gap: 1px; background: linear-gradient(#1d1d20, #141416); box-shadow: inset 0 0 0 1px rgba(255,255,255,.08), 0 18px 40px rgba(0,0,0,.75)">{items}</div>')
    dd=(f'<div style="position: relative"><button class="eqdd" style="height: 32px; min-width: 230px; box-sizing: border-box; border-radius: 9px; border: none; background: #121214; box-shadow: inset 0 0 0 1px rgba(255,255,255,.08); color: {TXT}; font: inherit; font-size: 13px; cursor: pointer; display: flex; align-items: center; justify-content: space-between; gap: 10px; padding: 0 12px"><span class="eqname">Flat</span>{svg("down",13,2,DIM)}</button>{lst}</div>')
    bar=(f'<div style="display: flex; align-items: center; justify-content: space-between; width: 100%">'
         f'<div style="display: flex; align-items: center; gap: 12px"><span style="{LBL}">EQ PRESET</span>{dd}</div>'
         f'<button class="eqreset" style="height: 32px; padding: 0 14px; border-radius: 9px; border: 1px solid rgba(255,255,255,.14); background: #1b1b1e; color: {TXT}; font: inherit; font-size: 10px; font-weight: 600; letter-spacing: .16em; cursor: pointer; display: flex; align-items: center; gap: 8px"><svg aria-hidden="true" width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 12a8 8 0 1 0 2.5-5.8M4 4v5h5"/></svg>RESET</button></div>')
    return plain_stage(f'<div style="display: flex; flex-direction: column; gap: 14px; align-items: center">{bar}<div style="display: flex; align-items: center; gap: 14px; padding: 18px; border-radius: 22px; {CARD}">{side(knob(36,0,"LOW CUT","OFF"))}{display}{side(knob(36,1,"HIGH CUT","OFF"))}</div></div>')

# ================= SYNTH (Overlord-style layout, Obsidian look) =================
def mtog(lbl): return toggle(lbl, True)
def sh(t, tog=None, right=''):
    tg = mtog(f'{t.title()} on') if tog else ''
    return f'<div style="display: flex; align-items: center; justify-content: center; gap: 8px; height: 18px"><span style="font-size: 11px; font-weight: 600; letter-spacing: .2em; color: {TXT}">{t}</span>{tg}{right}</div>'
def scr(body, w, h=74):
    return f'<div style="border-radius: 10px; padding: 6px; background: #08080a; box-shadow: inset 0 2px 8px rgba(0,0,0,.95), inset 0 0 0 1px rgba(255,255,255,.04)"><svg width="{w}" height="{h}" viewBox="0 0 {w} {h}" aria-hidden="true" style="display: block">{body}</svg></div>'
def wicons(active):
    names=['Sine','Triangle','Saw','Square','Pulse','Noise']
    P=dict(WAVES); P['Pulse']='M2 16V8h3v8h9V8h3v8h5'; P['Noise']='M2 12l2-5 2 9 2-7 2 6 2-8 2 7 2-4 2 6 2-5 2 3'
    return '<div style="display: flex; justify-content: space-between">'+''.join(f'<button aria-label="{n} wave" style="width: 27px; height: 22px; border-radius: 6px; border: none; background: {"rgba(255,255,255,.14)" if n==active else "none"}; color: {TXT if n==active else MUT}; cursor: pointer; padding: 0; display: flex; align-items: center; justify-content: center"><svg aria-hidden="true" width="16" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="{P[n]}"/></svg></button>' for n in names)+'</div>'
def kk(items, d=28): return '<div style="display: flex; justify-content: space-around">'+''.join(knob(d,v,n,t) for n,v,t in items)+'</div>'
def lever(lbl, up=True):
    ang = -25 if up else 25
    return (f'<div style="display: flex; flex-direction: column; align-items: center; gap: 8px"><span style="{LBL}">{lbl}</span>'
            f'<div style="display: flex; align-items: center; gap: 10px"><span style="width: 6px; height: 6px; border-radius: 50%; border: 1px solid {MUT}"></span>'
            f'<button aria-label="{lbl.title()}" style="width: 40px; height: 40px; border-radius: 50%; border: none; padding: 0; cursor: pointer; position: relative; background: radial-gradient(circle at 50% 35%, #5a5a60, #1c1c1f 70%); box-shadow: 0 4px 8px rgba(0,0,0,.7), inset 0 1px 0 rgba(255,255,255,.2), 0 0 0 3px #111113"><span style="position: absolute; left: 50%; top: 50%; width: 8px; height: 22px; margin: -20px 0 0 -4px; border-radius: 4px; background: linear-gradient(90deg, #8d8d93, #f4f4f5 45%, #8d8d93); transform-origin: 50% 100%; transform: rotate({ang}deg); box-shadow: 0 2px 3px rgba(0,0,0,.6)"></span></button>'
            f'<span style="width: 6px; height: 6px; border-radius: 50%; background: {A}; box-shadow: 0 0 8px {A}"></span></div></div>')
def synth_unit2():
    lcd=(f'<div style="display: flex; flex-direction: column; align-items: center; gap: 8px"><span style="{LBL}">TUNING</span>'
         f'<div style="width: 118px; height: 46px; box-sizing: border-box; border-radius: 9px; display: flex; align-items: center; justify-content: center; gap: 8px; background: #08080a; box-shadow: inset 0 2px 10px rgba(0,0,0,.95), inset 0 0 0 1px rgba(255,255,255,.07)">'
         f'<svg aria-hidden="true" width="10" height="16" viewBox="0 0 10 16" fill="{DIM}"><path d="M5 1l4 5H1zM5 15l4-5H1z"/></svg><span style="font-size: 24px; font-weight: 500; color: {A}; font-variant-numeric: tabular-nums; text-shadow: 0 0 12px rgba(243,234,216,.3)">440.0</span></div></div>')
    top=(f'<div style="height: 104px; flex: none; display: flex; align-items: center; justify-content: space-between; padding: 0 34px">'
         f'<div style="display: flex; gap: 20px">{knob(36,.4,"GATE")}{knob(36,.55,"SENSITIVITY")}{knob(36,.25,"GLIDE")}</div>'
         f'{lever("ENV RETRIGGER")}{lcd}{lever("PRE / POST",False)}'
         f'<div style="display: flex; gap: 60px">{knob(36,.6,"MIX")}{knob(36,.7,"OUTPUT")}</div></div>')
    # ARP
    cells=''.join(f'<button style="height: 26px; border-radius: 6px; border: none; background: {A if i==0 else "rgba(255,255,255,.04)"}; color: {"#111113" if i==0 else MUT}; font: inherit; font-size: {10 if i==0 else 11}px; font-weight: 600; letter-spacing: .06em; cursor: pointer">{"ROOT" if i==0 else i+1}</button>' for i in range(8))
    dirs=[('Up','M12 19V5M6 11l6-6 6 6'),('Down','M12 5v14M6 13l6 6 6-6'),('Up and down','M8 19V5M4 9l4-4 4 4M16 5v14M12 15l4 4 4-4'),('Down and up','M8 5v14M4 15l4 4 4-4M16 19V5M12 9l4-4 4 4'),('Random','M4 7h4l8 10h4M4 17h4l8-10h4M18 5l2 2-2 2M18 15l2 2-2 2')]
    dbtn=''.join(f'<button aria-label="{n}" style="width: 32px; height: 26px; border-radius: 6px; border: none; background: {"rgba(255,255,255,.14)" if i==1 else "none"}; color: {TXT if i==1 else MUT}; cursor: pointer; padding: 0; display: flex; align-items: center; justify-content: center"><svg aria-hidden="true" width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="{p}"/></svg></button>' for i,(n,p) in enumerate(dirs))
    rate=(f'<div style="display: flex; align-items: center; gap: 8px"><div style="flex: 1; position: relative; height: 26px; border-radius: 7px; background: rgba(0,0,0,.45); overflow: hidden"><div style="position: absolute; left: 0; top: 0; bottom: 0; width: 34%; background: rgba(243,234,216,.14); box-shadow: inset -2px 0 0 {A}"></div><span style="position: absolute; left: 10px; top: 5px; font-size: 11px; font-variant-numeric: tabular-nums">2.00 Hz</span></div>'
          f'<button style="height: 26px; padding: 0 9px; border-radius: 7px; border: 1px solid rgba(255,255,255,.12); background: none; color: {DIM}; font: inherit; font-size: 10px; font-weight: 600; letter-spacing: .12em; cursor: pointer">SYNC</button></div>')
    arp=col(238, f'{sh("ARPEGGIATOR",True)}<div style="display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 4px">{cells}</div>'
        f'<div style="display: flex; align-items: center; justify-content: space-between"><span style="{LBL}">KEY</span><button style="height: 24px; padding: 0 8px; border-radius: 6px; border: none; background: rgba(0,0,0,.45); color: {TXT}; font: inherit; font-size: 12px; cursor: pointer; display: flex; align-items: center; gap: 6px">C{svg("down",11,2,DIM)}</button>{chipsrow(["MAJ","MIN"],"MIN")}</div>'
        f'<div style="display: flex; flex-direction: column; gap: 5px"><span style="{LBL}; text-align: center">OCTAVE</span>{chipsrow(["−2","−1","0","+1","+2"],"+2")}</div>'
        f'<div style="display: flex; flex-direction: column; gap: 5px"><span style="{LBL}; text-align: center">PATTERN</span><div style="display: flex; justify-content: space-between">{dbtn}</div></div>'
        f'<div style="display: flex; flex-direction: column; gap: 5px"><span style="{LBL}; text-align: center">RATE</span>{rate}</div>')
    # AMPLIFIER
    dec=' '.join(f'{x},{(6+62*(1-math.exp(-x/38))):.1f}' for x in range(0,135,3))
    dec_svg=f'<polyline points="{dec}" fill="none" stroke="{A}" stroke-width="2"/>'
    ampl=col(180, f'{sh("AMPLIFIER")}{scr(dec_svg,134)}<div style="display: flex; flex-direction: column; align-items: center; gap: 16px; padding-top: 6px">{knob(34,.55,"DECAY","1378 ms")}{knob(34,.9,"SUSTAIN","90%")}</div>')
    # OSCILLATORS
    sawp=f'<path d="M10 64 V12 L80 64 H150" fill="none" stroke="{A}" stroke-width="2" stroke-linejoin="round"/>'
    sqp=f'<path d="M10 64 V12 H80 V64 H150" fill="none" stroke="{A}" stroke-width="2" stroke-linejoin="round"/>'
    def osc(name, path, active, lvl):
        return (f'<div style="width: 214px; flex: none; display: flex; flex-direction: column; gap: 12px; padding: 0 16px; box-sizing: border-box">{sh(name,True)}{scr(path,170)}{wicons(active)}'
                f'{kk([("SEMITONES",.5,"0"),("DETUNE",.5,"0%")])}<div style="display: flex; justify-content: center">{knob(28,.62,"LEVEL",lvl)}</div></div>')
    uni_svg=''.join(f'<path d="M{x} {74-h} V74" stroke="{A}" stroke-width="2.4"/>' for x,h in [(40,40),(52,62),(64,48)])
    uni=(f'<div style="width: 148px; flex: none; display: flex; flex-direction: column; gap: 12px; padding: 0 14px; box-sizing: border-box; box-shadow: inset 1px 0 0 rgba(255,255,255,.05), inset -1px 0 0 rgba(255,255,255,.05)">'
         f'{sh("UNISON 3")}{scr(uni_svg,108)}<div style="display: flex; gap: 4px"><button style="flex: 1; height: 22px; border-radius: 6px; border: none; background: {A}; color: #111113; font: inherit; font-size: 10px; font-weight: 600; cursor: pointer">OSC 1</button><button style="flex: 1; height: 22px; border-radius: 6px; border: 1px solid rgba(255,255,255,.12); background: none; color: {DIM}; font: inherit; font-size: 10px; font-weight: 600; cursor: pointer">OSC 2</button></div>'
         f'<div style="display: flex; flex-direction: column; align-items: center; gap: 12px">{knob(28,1.0,"WIDTH","100%")}{knob(28,.1,"DETUNE","10%")}</div></div>')
    oscs=f'<div style="flex: none; display: flex; padding: 16px 0; box-shadow: inset -1px 0 0 rgba(255,255,255,.07)">{osc("OSCILLATOR 1",sawp,"Saw","−7.5 dB")}{uni}{osc("OSCILLATOR 2",sqp,"Square","−8.3 dB")}</div>'
    # FILTER
    fp=[]
    for i in range(0,191,3):
        f=20*1000**(i/190); db=-10*math.log10(1+(f/4480)**2)
        fp.append(f'{i},{max(4,min(70, 22-db*3)):.1f}')
    filt=col(250, f'{sh("FILTER",True)}{scr(f"<polyline points=\"{" ".join(fp)}\" fill=\"none\" stroke=\"{A}\" stroke-width=\"2\"/>",200)}'
         f'{chipsrow(["6 dB","12 dB","18 dB","24 dB"],"12 dB")}{kk([("CUTOFF",.6,"4.48 kHz"),("RESONANCE",.05,"0%")])}'
         f'<div style="position: relative; height: 20px; border-radius: 6px; background: rgba(0,0,0,.45)"><span style="position: absolute; left: 50%; top: 3px; transform: translateX(-50%); font-size: 10px; color: {DIM}; font-variant-numeric: tabular-nums">ENV AMOUNT 0%</span></div>'
         f'{kk([("ATTACK",.05,"1 ms"),("DRIVE",.0,"0%")])}', last=True)
    inner=(f'<div style="flex: 1; min-height: 0; margin: 0 18px; border-radius: 16px; display: flex; justify-content: center; background: linear-gradient(#141416, #0f0f11); box-shadow: inset 0 3px 12px rgba(0,0,0,.85), inset 0 0 0 1px rgba(255,255,255,.04), 0 1px 0 rgba(255,255,255,.06)">'
           f'{arp}{ampl}{oscs}{filt}</div>')
    name=f'<div style="height: 30px; flex: none; display: flex; align-items: center; justify-content: center; gap: 14px"><span style="width: 60px; height: 1px; background: linear-gradient(90deg, transparent, rgba(255,255,255,.2))"></span><span style="font-size: 11px; font-weight: 600; letter-spacing: .5em; margin-right: -.5em; color: {DIM}">OBSIDIAN · SYNTH</span><span style="width: 60px; height: 1px; background: linear-gradient(90deg, rgba(255,255,255,.2), transparent)"></span></div>'
    screws=''.join(f'<div style="position: absolute; {p}">{obs.screw()}</div>' for p in ['left: 14px; top: 14px','right: 14px; top: 14px','left: 14px; bottom: 12px','right: 14px; bottom: 12px'])
    return (f'<div style="width: 1310px; height: 580px; box-sizing: border-box; position: relative; border-radius: 26px; display: flex; flex-direction: column; background: radial-gradient(80% 60% at 50% 0%, rgba(255,255,255,.06), transparent 70%), linear-gradient(#232327, #18181b); box-shadow: inset 0 1px 0 rgba(255,255,255,.12), inset 0 -1px 0 rgba(0,0,0,.6), inset 0 0 0 1px rgba(255,255,255,.04), 0 24px 50px rgba(0,0,0,.7), 0 4px 10px rgba(0,0,0,.5)">'
            f'{screws}{top}{inner}{name}</div>')

boards = [
 ('FinalSpecial.dc.html','Special FX · empty, picker open', pg('Obsidian · Special FX','SPECIAL FX',chain_stage('SPECIAL EFFECTS','ADD SPECIAL FX','ADD SPECIAL FX',['ALL','PITCH','FREEZE','GLITCH','LOOPER','FILTER'],'special effect'))),
 ('FinalAmp.dc.html','Amp', pg('Obsidian · Amp','AMP',stage_f())),
 ('FinalPedals.dc.html','Pedals · empty, picker open', pg('Obsidian · Pedals','PEDALS',chain_stage('PRE-AMP PEDALS','ADD PEDAL','ADD PEDAL',['ALL','DRIVE','BOOST','COMPRESSOR','WAH','PITCH'],'pedal'))),
 ('FinalSynth.dc.html','Synth', pg('Obsidian · Synth','SYNTH',plain_stage(synth_unit2()))),
 ('FinalCab.dc.html','Cab', pg('Obsidian · Cab','CAB',cab_stage())),
 ('FinalEQ.dc.html','EQ · 9 band', pg('Obsidian · EQ','EQ',eq_stage3())),
 ('FinalFX.dc.html','FX · empty, picker open', pg('Obsidian · FX','FX',chain_stage('POST-AMP EFFECTS','ADD EFFECT','ADD EFFECT',['ALL','DELAY','REVERB','MODULATION','PITCH','DYNAMICS'],'effect'))),
 ('FinalPresets.dc.html','Preset browser', pg('Obsidian · Presets','AMP',stage_f(dim_overlay(presets_card())))),
 ('FinalSettings.dc.html','Settings', pg('Obsidian · Settings','AMP',stage_f(dim_overlay(settings_card())),settings=True)),
 ('FinalTuner.dc.html','Tuner open', pg('Obsidian · Tuner','AMP',stage_f(tuner_f()),tuner=True)),
 ('FinalLive.dc.html','Live tuner on', pg('Obsidian · Live tuner','AMP',stage_f(),live=True)),
]
for fn,_,h in boards: open(OUT+fn,'w').write(h)
json.dump([(fn,t) for fn,t,_ in boards], open('/tmp/batch4.json','w'))
print('ok')
