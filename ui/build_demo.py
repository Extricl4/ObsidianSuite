import re
from cabempty import *
import batch6


def settings_card2():
    navs=['Device','Audio','MIDI','Interface','Presets','About']
    nav = ''.join(f'<button style="height: 38px; border-radius: 9px; border: none; background: {"rgba(255,255,255,.07)" if i==0 else "none"}; box-shadow: {f"inset 2px 0 0 {A}" if i==0 else "none"}; color: {"#f4f1ea" if i==0 else DIM}; font: inherit; font-size: 13px; font-weight: {"600" if i==0 else "400"}; cursor: pointer; text-align: left; padding: 0 14px">{n}</button>' for i,n in enumerate(navs))
    def row(lbl, hint, ctl):
        return f'<div style="display: flex; align-items: center; justify-content: space-between; gap: 24px; padding: 12px 0; box-shadow: inset 0 -1px 0 rgba(255,255,255,.05)"><div style="display: flex; flex-direction: column; gap: 3px"><span style="font-size: 13px; color: {TXT}">{lbl}</span><span style="font-size: 11px; color: {MUT}">{hint}</span></div>{ctl}</div>'
    dd = lambda t, w=150: f'<button style="height: 32px; min-width: {w}px; padding: 0 12px; border-radius: 9px; {RAISED}; color: {TXT}; font: inherit; font-size: 12px; cursor: pointer; display: flex; align-items: center; justify-content: space-between; gap: 10px">{t}{svg("down",12,2,DIM)}</button>'
    btn = lambda t, primary=False: f'<button style="height: 32px; padding: 0 14px; border-radius: 9px; border: {"none" if primary else "1px solid rgba(255,255,255,.12)"}; background: {A if primary else "none"}; color: {"#111113" if primary else TXT}; font: inherit; font-size: 11px; font-weight: 600; letter-spacing: .12em; cursor: pointer; white-space: nowrap">{t}</button>'
    head = lambda t: f'<span style="{LBL}; padding: 14px 0 4px">{t}</span>'
    lat = f'<span style="font-size: 11px; color: {MUT}; font-variant-numeric: tabular-nums">2.7 ms in · 3.1 ms out</span>'
    def field(t, w='100%'):
        return f'<button style="width: {w}; height: 38px; box-sizing: border-box; border-radius: 9px; border: none; background: #121214; box-shadow: inset 0 0 0 1px rgba(255,255,255,.06); color: {TXT}; font: inherit; font-size: 13px; cursor: pointer; display: flex; align-items: center; justify-content: space-between; gap: 10px; padding: 0 12px; text-align: left"><span style="white-space: nowrap; overflow: hidden; text-overflow: ellipsis">{t}</span>{svg("down",13,2,DIM)}</button>'
    def lbl2(t, right=''):
        return f'<div style="display: flex; align-items: center; justify-content: space-between; height: 18px"><span style="font-size: 12px; color: {DIM}">{t}</span>{right}</div>'
    def chk(t, on):
        box = (f'<span style="width: 18px; height: 18px; border-radius: 4px; display: flex; align-items: center; justify-content: center; background: {"#f4f1ea" if on else "none"}; box-shadow: {"none" if on else "inset 0 0 0 1.5px #4a4a50"}">'
               + ('<svg aria-hidden="true" width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="#111113" stroke-width="3.5" stroke-linecap="round" stroke-linejoin="round"><path d="M5 12.5l4.5 4.5L19 7"/></svg>' if on else '') + '</span>')
        return f'<button class="chk" data-on="{1 if on else 0}" style="height: 30px; border: none; background: none; padding: 0; cursor: pointer; display: flex; align-items: center; gap: 12px; color: {TXT if on else MUT}; font: inherit; font-size: 13px">{box}{t}</button>'
    def well(inner, h):
        return f'<div style="height: {h}px; box-sizing: border-box; border-radius: 10px; background: #121214; box-shadow: inset 0 0 0 1px rgba(255,255,255,.06); padding: 8px 12px; display: flex; flex-direction: column; overflow: auto">{inner}</div>'
    hr = '<div style="height: 1px; background: rgba(255,255,255,.08); margin: 14px 0"></div>'
    col = lambda inner: f'<div style="display: flex; flex-direction: column; gap: 8px; min-width: 0">{inner}</div>'
    grid = lambda a, b: f'<div style="display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); column-gap: 26px">{a}{b}</div>'
    dashes = f'<div style="display: flex; gap: 3px" aria-hidden="true">' + ''.join(f'<span style="width: 7px; height: 3px; border-radius: 1px; background: {A if i<3 else "#34343a"}"></span>' for i in range(7)) + '</div>'
    test = f'<button style="border: none; background: none; padding: 0; color: {TXT}; font: inherit; font-size: 10px; font-weight: 600; letter-spacing: .14em; cursor: pointer">TEST</button>'
    midi_list = ''.join(chk(n, i==0) for i,n in enumerate(['USB MIDI Controller','Loopback MIDI']))
    sbtn = lambda t: f'<button style="flex: 1; height: 38px; border-radius: 9px; border: none; background: #1f1f22; box-shadow: inset 0 1px 0 rgba(255,255,255,.06), 0 0 0 1px rgba(255,255,255,.06); color: {TXT}; font: inherit; font-size: 12px; font-weight: 600; cursor: pointer">{t}</button>'
    device = ('<div style="padding-top: 14px">'
              + grid(col(lbl2('Audio device type', dashes) + field('ASIO')), col(lbl2('Audio device', test) + field('USB Audio Interface (ASIO)')))
              + hr
              + grid(col(lbl2('Input channels') + well(chk('In 1',True)+chk('In 2',False), 104)), col(lbl2('Output channels') + well(chk('Out 1',True)+chk('Out 2',True), 104)))
              + hr
              + grid(col(lbl2('Sample rate') + field('48000 Hz') + '<div style="height: 6px"></div>' + lbl2('Buffer size') + field('256 samples (5.3 ms)') + f'<div style="display: flex; gap: 10px; margin-top: 8px">{sbtn("Control Panel")}{sbtn("Reset Device")}</div>'),
                     col(lbl2('Bit depth') + field('24-bit') + '<div style="height: 6px"></div>' + lbl2('MIDI input devices') + well(midi_list, 92)))
              + '</div>')
    audio = (head('AUDIO') + row('Oversampling','Higher sounds smoother, uses more CPU', seg(['1×','2×','4×'],1,h=26))
             + row('Quality when rendering','Used when you bounce or export', dd('Highest'))
             + row('Input calibration','Match the plugin to your interface', hk("LEVEL",.5,"0.0 dB"))
             + row('Mute output while tuning','Silence the plugin when the tuner is open', toggle('Mute output while tuning',True))
             + row('Live tuner','Small tuner in the bottom bar while playing', toggle('Live tuner',False))
             + row('Latency compensation','Report latency to your DAW', toggle('Latency compensation',True)))
    maps = [('CC 11','Output volume','Expression pedal'),('CC 64','Doubler on / off','Footswitch'),('CC 20','Gate threshold','Knob'),('PC 0–7','Presets 1–8','Program change')]
    maptbl = ('<div style="display: flex; flex-direction: column; gap: 4px; padding-top: 10px">'
              + f'<div style="display: grid; grid-template-columns: 80px minmax(0, 1fr) minmax(0, 1fr) 30px; gap: 10px; padding: 0 10px; {LBL}"><span>MESSAGE</span><span>CONTROLS</span><span>FROM</span><span></span></div>'
              + ''.join(f'<div style="display: grid; grid-template-columns: 80px minmax(0, 1fr) minmax(0, 1fr) 30px; gap: 10px; align-items: center; height: 34px; padding: 0 10px; border-radius: 8px; background: rgba(255,255,255,.03); font-size: 12px"><span style="color: {A}; font-variant-numeric: tabular-nums">{m}</span><span>{c}</span><span style="color: {MUT}">{f}</span><button aria-label="Remove mapping" style="width: 26px; height: 26px; border: none; background: none; color: {MUT}; cursor: pointer; padding: 0; display: flex; align-items: center; justify-content: center">{svg("close",12,2)}</button></div>' for m,c,f in maps) + '</div>')
    midi = (head('MIDI') + row('Input device','Where MIDI comes from', dd('All devices'))
            + row('Channel','Listen on one channel or all', dd('Omni', 110))
            + row('Program change loads presets','PC messages switch presets', toggle('Program change loads presets',True))
            + f'<div style="display: flex; align-items: center; justify-content: space-between; padding: 12px 0 0"><span style="font-size: 13px">Mappings</span><div style="display: flex; gap: 8px">{btn("MIDI LEARN",True)}{btn("CLEAR ALL")}</div></div>' + maptbl)
    sw = ''.join(f'<button class="acc" data-accent="{c}" aria-label="Accent {c}" style="width: 26px; height: 26px; border-radius: 50%; border: none; padding: 0; cursor: pointer; background: {c}; box-shadow: {"0 0 0 2px #0f0f11, 0 0 0 4px " + c if i==0 else "inset 0 0 0 1px rgba(0,0,0,.3)"}"></button>' for i,c in enumerate(['#f3ead8','#ff5a36','#ffb347','#59d0ff','#a995ff','#7fe08f']))
    interface = (head('INTERFACE') + row('Window size','Scale the whole plugin', dd('100%', 110))
                 + row('Accent colour','Used for knob rings, lights and highlights', f'<div style="display: flex; align-items: center; gap: 8px"><button id="accswatch" aria-label="Open colour picker" style="width: 40px; height: 32px; padding: 0; border: none; border-radius: 9px; background: var(--accent); box-shadow: inset 0 0 0 1px rgba(255,255,255,.25), 0 0 0 1px rgba(0,0,0,.6); cursor: pointer"></button><label for="acchex" style="position: absolute; width: 1px; height: 1px; overflow: hidden; clip-path: inset(50%)">Accent hex code</label><input id="acchex" type="text" value="#F3EAD8" maxlength="7" spellcheck="false" style="width: 86px; height: 32px; box-sizing: border-box; padding: 0 10px; border-radius: 9px; border: none; background: #0a0a0b; box-shadow: inset 0 2px 6px rgba(0,0,0,.9), 0 1px 0 rgba(255,255,255,.07); color: #e9e6df; font: inherit; font-size: 12px; letter-spacing: .06em; font-variant-numeric: tabular-nums; outline: none"><button id="accreset" style="height: 32px; padding: 0 12px; border-radius: 9px; border: 1px solid rgba(255,255,255,.12); background: none; color: #e9e6df; font: inherit; font-size: 11px; font-weight: 600; letter-spacing: .12em; cursor: pointer">RESET</button><button id="accsave" style="height: 32px; padding: 0 12px; border-radius: 9px; border: none; background: var(--accent); color: #111113; font: inherit; font-size: 11px; font-weight: 600; letter-spacing: .12em; cursor: pointer">SAVE</button></div>')
                 + row('Knob control','How dragging turns a knob', seg(['VERTICAL','CIRCULAR'],0,h=26))
                 + row('Show tooltips','Explain controls when you hover them', toggle('Show tooltips',True))
                 + row('Show level meters','Input and output meters in the top bar', toggle('Show level meters',True))
                 + row('Animations','Smooth panel and knob motion', toggle('Animations',True)))
    path = f'<div style="display: flex; align-items: center; gap: 8px"><span style="height: 32px; display: flex; align-items: center; padding: 0 12px; border-radius: 9px; {WELL}; font-size: 12px; color: {DIM}; font-variant-numeric: tabular-nums">Documents\\Obsidian\\Presets</span>{btn("CHANGE")}</div>'
    presets = (head('PRESETS') + row('Preset folder','Where your presets are saved', path)
               + row('Open last preset on start','Pick up where you left off', toggle('Open last preset on start',True))
               + row('Mark unsaved changes','Show * after the preset name when edited', toggle('Mark unsaved changes',True))
               + row('Ask before switching','Warn when leaving an edited preset', toggle('Ask before switching',False))
               + row('Import / export','Share presets as files', f'<div style="display: flex; gap: 8px">{btn("IMPORT")}{btn("EXPORT")}</div>')
               + row('Factory presets','Put back the presets that came with Obsidian', btn("RESTORE")))
    about = (f'<div style="flex: 1; display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 14px; text-align: center">'
             f'<div style="width: 26px; height: 26px; transform: rotate(45deg); background: linear-gradient(135deg, #f1ede4, #6d6a64); box-shadow: 0 0 20px rgba(255,255,255,.25)" aria-hidden="true"></div>'
             f'<span style="font-size: 26px; font-weight: 600; letter-spacing: .4em; margin-right: -.4em">OBSIDIAN</span><span style="font-size: 12px; letter-spacing: .26em; color: {DIM}">AMP SUITE</span>'
             f'<div style="display: grid; grid-template-columns: auto auto; column-gap: 18px; row-gap: 6px; margin-top: 10px; font-size: 12px; text-align: left"><span style="color: {MUT}">Version</span><span style="font-variant-numeric: tabular-nums">0.1 (preview)</span><span style="color: {MUT}">Format</span><span>VST3 · Standalone</span><span style="color: {MUT}">System</span><span>Windows 11 · 64-bit</span><span style="color: {MUT}">Made by</span><span>Emil</span></div>'
             f'<div style="display: flex; gap: 8px; margin-top: 12px">{btn("CHECK FOR UPDATES",True)}{btn("MANUAL")}{btn("LICENCES")}</div></div>')
    panels = ''.join(f'<div class="spanel" data-panel="{n}" style="flex: 1; min-width: 0; padding: 6px 26px 14px; box-sizing: border-box; flex-direction: column; overflow: auto; display: {"flex" if i==0 else "none"}">{c}</div>' for i,(n,c) in enumerate(zip(navs,[device,audio,midi,interface,presets,about])))
    nav_col = f'<div class="setnav" style="width: 200px; flex: none; display: flex; flex-direction: column; gap: 2px; padding: 14px 10px; box-sizing: border-box; box-shadow: inset -1px 0 0 rgba(255,255,255,.06)">{nav}</div>'
    top = f'<div style="height: 52px; flex: none; display: flex; align-items: center; justify-content: space-between; padding: 0 12px 0 20px; box-shadow: inset 0 -1px 0 rgba(255,255,255,.06)"><span style="font-size: 13px; font-weight: 600; letter-spacing: .3em">SETTINGS</span>{iconbtn("close","Close settings",32)}</div>'
    return f'<div style="width: 880px; height: 560px; border-radius: 22px; overflow: hidden; display: flex; flex-direction: column; background: linear-gradient(#1a1a1d, #0f0f11); box-shadow: inset 0 1px 0 rgba(255,255,255,.08), inset 0 0 0 1px rgba(255,255,255,.06), 0 30px 70px rgba(0,0,0,.8)">{top}<div style="flex: 1; min-height: 0; display: flex">{nav_col}{panels}</div></div>'


import re as _re
S=980/1128
def ovknob(cx,cy,dia,v,name):
    dd=dia*S; ang=-135+270*v
    return (f'<div class="kn" data-v="{v}" title="{name}" style="position: absolute; left: {cx*S-dd/2:.1f}px; top: {cy*S-dd/2:.1f}px; width: {dd:.1f}px; height: {dd:.1f}px; border-radius: 50%; cursor: ns-resize; touch-action: none; '
            f'background: repeating-conic-gradient(#1c1c1f 0deg 7deg, #070708 7deg 14deg); box-shadow: 0 3px 6px rgba(0,0,0,.6)">'
            f'<span style="position: absolute; inset: 12%; border-radius: 50%; background: radial-gradient(circle at 40% 28%, #3a3a3f, #0c0c0e 68%); box-shadow: inset 0 1px 1px rgba(255,255,255,.18)"></span>'
            f'<div class="kn-rot" style="position: absolute; inset: 0; transform: rotate({ang}deg)"><span style="position: absolute; left: 50%; top: 14%; width: 2px; height: 34%; margin-left: -1px; border-radius: 1px; background: #f2f2f2"></span></div>'
            f'<span class="kn-val ampread" style="position: absolute; left: 50%; top: -22px; transform: translateX(-50%); padding: 2px 6px; border-radius: 5px; background: rgba(10,10,12,.9); font-size: 10px; color: #e9e6df; font-variant-numeric: tabular-nums; opacity: 0; pointer-events: none; transition: opacity .15s">{v*10:.1f}</span></div>')
KN=[('Gain',334,341,36,.75),('Bass',426.5,339,30,.57),('Mid',486.5,339,30,.43),('Treble',546.5,339,30,.63),('Presence',636.5,339,30,.58),('Depth',696.5,339,30,.47),('Output',790,339,40,.5)]
ampview=(f'<div id="ampview" style="display: none; position: absolute; left: 50%; top: 18px; transform: translateX(-50%); width: 980px; height: 378px; filter: drop-shadow(0 26px 22px rgba(0,0,0,.75))">'
         f'<img src="styx.jpg" alt="Styx amp head with a blue-stained wood front" style="width: 980px; height: 378px; display: block; border-radius: 4px">'
         + ''.join(ovknob(x,y,dia,v,n) for n,x,y,dia,v in KN)
         + f'<button class="ampjewel" aria-label="Power" style="position: absolute; left: {(1043-16)*S:.1f}px; top: {(337-16)*S:.1f}px; width: {32*S:.1f}px; height: {32*S:.1f}px; border-radius: 50%; border: none; padding: 0; background: transparent; cursor: pointer"></button>'
         + f'<span class="jewelmask" style="display: none; position: absolute; left: {(1043-14)*S:.1f}px; top: {(337-14)*S:.1f}px; width: {28*S:.1f}px; height: {28*S:.1f}px; border-radius: 50%; background: radial-gradient(circle at 45% 40%, #3a3f48, #121418 70%); box-shadow: 0 0 0 3px #16171a; pointer-events: none"></span>'
         + f'<button class="ampremove" aria-label="Remove amp" style="position: absolute; right: -8px; top: -8px; width: 30px; height: 30px; border-radius: 50%; border: 1px solid rgba(255,255,255,.15); background: rgba(16,16,18,.92); color: #e9e6df; cursor: pointer; padding: 0; display: flex; align-items: center; justify-content: center">{svg("close",13,2)}</button>'
         + '</div>')
styx_tile=(f'<button class="pick-amp" style="height: 100px; border-radius: 12px; border: 1px solid rgba(255,255,255,.14); padding: 0; cursor: pointer; overflow: hidden; position: relative; background: #0b0b0d url(styx_thumb.jpg) center 40% / 150% auto no-repeat; color: #e9e6df; font: inherit">'
           f'<span style="position: absolute; left: 0; right: 0; bottom: 0; padding: 6px 0; background: linear-gradient(transparent, rgba(0,0,0,.85)); font-size: 11px; font-weight: 600; letter-spacing: .22em">STYX</span></button>')
def with_styx(p):
    return _re.sub(r'<div style="height: 100px; border-radius: 12px; border: 1px dashed[^>]*>.*?COMING SOON</span></div>', lambda m: styx_tile, p, count=1, flags=_re.S)

POPHTML='<div id="accpop" style="display: none; position: absolute; z-index: 70; width: 260px; box-sizing: border-box; padding: 14px; border-radius: 16px; flex-direction: column; gap: 12px; background: linear-gradient(#1f1f22, #151517); box-shadow: inset 0 0 0 1px rgba(255,255,255,.09), 0 22px 50px rgba(0,0,0,.85)"><div id="sv" style="position: relative; height: 160px; border-radius: 10px; cursor: crosshair; touch-action: none; background: linear-gradient(to top, #000, transparent), linear-gradient(to right, #fff, hsl(40,100%,50%))"><span id="svk" style="position: absolute; width: 14px; height: 14px; margin: -7px 0 0 -7px; border-radius: 50%; box-shadow: 0 0 0 2px #fff, 0 0 0 3px rgba(0,0,0,.5); pointer-events: none"></span></div><div id="hue" style="position: relative; height: 14px; border-radius: 7px; cursor: pointer; touch-action: none; background: linear-gradient(to right, #f00, #ff0, #0f0, #0ff, #00f, #f0f, #f00)"><span id="huek" style="position: absolute; top: 50%; width: 14px; height: 14px; margin: -7px 0 0 -7px; border-radius: 50%; background: #fff; box-shadow: 0 1px 4px rgba(0,0,0,.6); pointer-events: none"></span></div><div style="display: flex; align-items: center; gap: 10px"><div style="display: flex; border-radius: 8px; overflow: hidden; box-shadow: inset 0 0 0 1px rgba(255,255,255,.15)"><span id="accold" title="Before" style="width: 28px; height: 28px"></span><span id="accnew" title="Now" style="width: 28px; height: 28px; background: var(--accent)"></span></div><span style="font-size: 11px; color: #8d8d93">Drag to pick, or type a hex code</span></div></div>'
ACC='#f3ead8'
def fix(s):
    s=re.sub(r'(fill|stroke|stop-color)="@A@"', lambda m: f'{m.group(1)}="{ACC}"', s)
    return s.replace('@A@','var(--accent)').replace('/_blob/ce3456ddbf38bb81fa64b83b155ce314','grain.png')

def hide_pop(html, old, cls):
    assert old in html, old
    return html.replace(old, old.replace('<div style="', f'<div class="pop {cls}" style="display: none; '))

POP_CHAIN='<div style="position: absolute; left: 236px; top: -46px">'
special = hide_pop(chain_stage('SPECIAL EFFECTS','ADD SPECIAL FX','ADD SPECIAL FX',['ALL','PITCH','FREEZE','GLITCH','LOOPER','FILTER'],'special effect'), POP_CHAIN, '')
pedals  = hide_pop(chain_stage('PRE-AMP PEDALS','ADD PEDAL','ADD PEDAL',['ALL','DRIVE','BOOST','COMPRESSOR','WAH','PITCH'],'pedal'), POP_CHAIN, '')
fxs     = hide_pop(chain_stage('POST-AMP EFFECTS','ADD EFFECT','ADD EFFECT',['ALL','DELAY','REVERB','MODULATION','PITCH','DYNAMICS'],'effect'), POP_CHAIN, '')
cabpick = amp_picker().replace('CHOOSE AMP','CHOOSE CAB').replace('CRUNCH','2×12').replace('HIGH GAIN','4×12').replace('CLEAN','1×12').replace('width: 76px; height: 34px; border-radius: 4px','width: 46px; height: 46px; border-radius: 4px').replace('Search amps','Search cabs').replace('Your amps','Your cabs')
ampst = batch6.stage_f()
ampst = ampst[:-6] + f'<div class="pop" data-for="AMP" style="display: none; position: absolute; right: 60px; top: 40px">{amp_picker()}</div><div class="pop" data-for="CAB" style="display: none; position: absolute; left: 60px; top: 60px">{cabpick}</div></div>'
cabst = cab_stage_empty()
cabst = cabst[:-6] + f'<div class="pop" data-for="CAB" style="display: none; position: absolute; left: 470px; top: 60px">{cabpick}</div></div>'
pages = [('SPECIAL FX',special),('PEDALS',pedals),('SYNTH',plain_stage(synth_unit2())),('AMP',ampst),('CAB',cabst),('EQ',eq_stage3()),('FX',fxs)]
pages_html=''.join(f'<div class="page" data-page="{n}" style="flex: 1; min-height: 0; flex-direction: column; position: relative; display: {"flex" if n=="AMP" else "none"}">{h}</div>' for n,h in pages)
ovs = (f'<div class="ov" data-ov="presets" style="display: none; position: absolute; inset: 0; z-index: 20">{dim_overlay(presets_card())}</div>'
       f'<div class="ov" data-ov="settings" style="display: none; position: absolute; inset: 0; z-index: 20">{dim_overlay(settings_card2())}</div>'
       f'<div class="ov" data-ov="tuner" style="display: none; position: absolute; inset: 0; z-index: 20">{tuner_f()}</div>')
body = (header_c('AMP')
        + f'<div id="pages" style="flex: 1; min-height: 0; position: relative; display: flex; flex-direction: column">{pages_html}{ovs}</div>'
        + f'<div id="f0">{foot()}</div><div id="f1" style="display: none">{foot(live=True)}</div>')

import re as _re2
body = _re2.sub(r'<span style="display: flex; align-items: center; gap: 7px"><span[^>]*></span>MIDI</span>', '', body)
body = body.replace('120 BPM</button>', '<span class="bpmtxt" style="display: inline-block; width: 64px; text-align: left; font-variant-numeric: tabular-nums">120 BPM</span></button>')
def _sig():
    return seg(['2/4','3/4','4/4','5/4','6/8','7/8'],2,h=26)
metro = (f'<div id="metropop" style="display: none; position: absolute; left: 150px; bottom: 58px; z-index: 40; width: 340px; box-sizing: border-box; border-radius: 18px; padding: 16px; flex-direction: column; gap: 14px; background: linear-gradient(#1c1c1f, #121214); box-shadow: inset 0 1px 0 rgba(255,255,255,.08), inset 0 0 0 1px rgba(255,255,255,.07), 0 24px 50px rgba(0,0,0,.8)">'
         f'<div style="display: flex; align-items: center; justify-content: space-between"><span style="font-size: 12px; font-weight: 600; letter-spacing: .26em">METRONOME</span>{iconbtn("close","Close metronome",30)}</div>'
         f'<div style="display: flex; align-items: center; justify-content: space-between">'
         f'<button aria-label="Slower" class="bpmstep" data-d="-1" style="width: 40px; height: 40px; border-radius: 12px; {RAISED}; color: #e9e6df; font: inherit; font-size: 20px; cursor: pointer">−</button>'
         f'<div style="display: flex; align-items: baseline; gap: 8px"><span id="bpmval" style="font-size: 44px; font-weight: 500; line-height: 1; font-variant-numeric: tabular-nums">120</span><span style="font-size: 11px; letter-spacing: .2em; color: {MUT}">BPM</span></div>'
         f'<button aria-label="Faster" class="bpmstep" data-d="1" style="width: 40px; height: 40px; border-radius: 12px; {RAISED}; color: #e9e6df; font: inherit; font-size: 20px; cursor: pointer">+</button></div>'
         f'<label for="bpmrange" style="position: absolute; width: 1px; height: 1px; overflow: hidden; clip-path: inset(50%)">Tempo</label><input id="bpmrange" type="range" min="40" max="240" value="120" style="width: 100%; accent-color: var(--accent)">'
         f'<button id="taptempo" style="height: 36px; border-radius: 10px; border: 1px solid rgba(255,255,255,.12); background: none; color: #e9e6df; font: inherit; font-size: 11px; font-weight: 600; letter-spacing: .2em; cursor: pointer">TAP TEMPO</button>'
         f'<div style="display: flex; flex-direction: column; gap: 8px"><span style="{LBL}">TIME SIGNATURE</span>{_sig()}</div>'
         f'<div style="display: flex; align-items: center; justify-content: space-between"><span style="font-size: 12px">Accent first beat</span>{toggle("Accent first beat",True)}</div>'
         f'</div>')
body = body + metro + POPHTML + '<div id="ddmenu" role="listbox" style="display: none; position: absolute; z-index: 60; min-width: 160px; max-height: 300px; overflow: auto; padding: 6px; border-radius: 12px; flex-direction: column; gap: 1px; background: linear-gradient(#1f1f22, #151517); box-shadow: inset 0 0 0 1px rgba(255,255,255,.09), 0 18px 40px rgba(0,0,0,.8)"></div>'
body = fix(body)

JS = r'''
const $=(s,r=document)=>r.querySelector(s), $$=(s,r=document)=>[...r.querySelectorAll(s)];
const app=$('#app');
let zoom=1; function fit(){const s=Math.min(innerWidth/1376, innerHeight/836)*zoom; app.style.transform='scale('+s+')';}
addEventListener('resize',fit); fit();
const TABS=['SPECIAL FX','PEDALS','SYNTH','AMP','CAB','EQ','FX'];
const tabBtns=$$('button').filter(b=>b.querySelector('svg[width="28"]') && TABS.includes(b.textContent.trim()));
let curTab='AMP'; const bypassed=new Set();
function styleTabs(){ tabBtns.forEach(b=>{ const name=b.textContent.trim(), on=name===curTab, off=bypassed.has(name);
    b.style.color=off?'#4b4b51':on?'#faf8f3':'#bdbbb6';
    const ic=b.querySelector('svg'); ic.style.filter=off?'none':on?'drop-shadow(0 0 4px var(--accent)) drop-shadow(0 0 8px rgba(243,234,216,.45))':'drop-shadow(0 0 3px rgba(243,234,216,.28))';
    ic.style.opacity=off?'.55':'1';
    const sp=b.querySelectorAll(':scope > span'); sp[0].style.fontWeight=on?'600':'500'; sp[0].style.textShadow=off?'none':on?'0 0 8px rgba(255,255,255,.22)':'none';
    sp[0].style.textDecoration=off?'line-through':'none';
    sp[1].style.background=on?(off?'#4b4b51':'var(--accent)'):'transparent'; sp[1].style.boxShadow=on&&!off?'0 0 8px var(--accent)':'none';
    b.setAttribute('aria-pressed', on?'true':'false'); b.title=off?name+' (bypassed, double-click to turn on)':'Double-click to bypass '+name; }); }
tabBtns.forEach(b=>b.addEventListener('dblclick',e=>{ const n=b.textContent.trim(); if(bypassed.has(n)){ bypassed.delete(n); toast(n+' on'); } else { bypassed.add(n); toast(n+' bypassed'); } styleTabs(); }));
function setTab(n){ curTab=n; styleTabs();
  $$('.page').forEach(p=>p.style.display=p.dataset.page===n?'flex':'none');
  closeOv(); $$('.pop').forEach(p=>p.style.display='none');
}
function openOv(n){$$('.ov').forEach(o=>o.style.display=o.dataset.ov===n?'block':'none'); setFoot();}
function closeOv(){$$('.ov').forEach(o=>o.style.display='none'); setFoot();}
let live=false;
function setFoot(){ $('#f0').style.display=live?'none':'block'; $('#f1').style.display=live?'block':'none'; }
function setTog(b,on){ b.dataset.on=on?1:0; b.style.background=on?'var(--accent)':'#2a2a2e'; const k=b.firstElementChild;
  k.style.left=on?'auto':'2px'; k.style.right=on?'2px':'auto'; k.style.background=on?'#111113':'#8d8d93'; }
const PRESETS=['Tight Wall','Legion Crunch','Glass Clean','Solo Burn','Night Drive','Styx Chug','War Drum','Halo Lead'];
let pi=0;
const pname=()=>$('[aria-label="Previous preset"]').nextElementSibling.querySelector('span');
function setPreset(n,edited){pname().innerHTML=n+(edited?' <span style="color: var(--accent)">*</span>':'');}
let tr=0;
function oddOne(btns){ if(btns.length<3) return null; const c={}; btns.forEach(b=>{const s=b.getAttribute('style'); c[s]=(c[s]||0)+1;});
  const odd=btns.filter(b=>c[b.getAttribute('style')]===1); return odd.length===1?odd[0]:null; }
function isAct2(b){const s=b.getAttribute('style'); return /var\(--accent\)|#2a2a2e, #1b1b1e|\.14\)|\.13\)/.test(s) && !/background: none/.test(s);}
document.addEventListener('click',e=>{
  const dm=$('#ddmenu'); if(dm.style.display==='flex' && !dm.contains(e.target)) { dm.style.display='none'; if(ddBtn && ddBtn.contains(e.target)) { ddBtn=null; return; } }
  const el=$('.eqlist'); if(el && el.style.display==='flex' && !el.contains(e.target) && !e.target.closest('.eqdd')) el.style.display='none';
  const ovb=e.target.classList && e.target.parentElement && e.target.parentElement.classList.contains('ov') && e.target===e.target.parentElement.firstElementChild; if(ovb){ closeOv(); return; }
  const b=e.target.closest('button'); if(!b) return;
  if(b.closest('#ddmenu')){ ddPick(b); return; }
  const t=b.textContent.trim(), al=b.getAttribute('aria-label')||'';
  if(tabBtns.includes(b)) return setTab(t);
  if(b.classList.contains('tog')){ const on=b.dataset.on!=='1';
    if(al==='Live tuner'){ live=on; $$('.tog[aria-label="Live tuner"]').forEach(x=>setTog(x,on)); setFoot(); }
    else setTog(b,on); return; }
  if(al==='Previous preset'||al==='Next preset'){ pi=(pi+(al==='Next preset'?1:PRESETS.length-1))%PRESETS.length; setPreset(PRESETS[pi],false); return; }
  if(b===$('[aria-label="Previous preset"]').nextElementSibling) return openOv('presets');
  if(t==='SETTINGS') return openOv('settings');
  if(t.startsWith('TUNER')) return openOv('tuner');
  if(/^Close (presets|settings|tuner)$/.test(al)) return closeOv();
  if(t==='LOAD'){ const row=$$('.ov[data-ov="presets"] button').find(x=>/inset 2px 0 0/.test(x.getAttribute('style'))); if(row){const n=row.children[1].textContent; pi=Math.max(0,PRESETS.indexOf(n)); setPreset(n,false);} return closeOv(); }
  if(al==='Transpose down'||al==='Transpose up'){ tr=Math.max(-12,Math.min(12,tr+(al==='Transpose up'?1:-1))); b.parentElement.children[1].children[0].textContent=(tr>0?'+':tr<0?'−':'')+Math.abs(tr); return; }
  if(al==='Start metronome'||al==='Stop metronome'){ const on=al==='Start metronome'; $$('[aria-label="Start metronome"],[aria-label="Stop metronome"]').forEach(x=>{x.setAttribute('aria-label',on?'Stop metronome':'Start metronome'); x.querySelector('path').setAttribute('d',on?'M6 4.5h4.5v15H6zM13.5 4.5H18v15h-4.5z':'M7 4.5v15l13-7.5z');}); metro=on; return; }
  if(/^ADD /.test(t)){ const pg=b.closest('.page'); const km=t.match(/^ADD (AMP|CAB)/); const kind=km?km[1]:''; const pops=$$('.pop',pg); const p=pops.find(x=>x.dataset.for===kind)||(pops.length&&!pops[0].dataset.for?pops[0]:null); pops.forEach(x=>x.style.display='none'); if(p) p.style.display='block'; return; }
  if(al==='Close' && b.closest('.pop')){ b.closest('.pop').style.display='none'; return; }
  if(b.classList.contains('pick-amp')){ $('#ampview').style.display='block'; const s=$$('.page[data-page="AMP"] button').find(x=>x.textContent.includes('ADD AMP')); if(s) s.parentElement.style.display='none'; $$('.pop').forEach(p=>p.style.display='none'); return; }
  if(b.classList.contains('ampremove')){ removeAmp(); return; }
  if(b.classList.contains('ampjewel')){ const m=$('.jewelmask'); m.style.display=m.style.display==='block'?'none':'block'; return; }
  if(b.closest('#metropop')){
    if(al==='Close metronome'){ $('#metropop').style.display='none'; return; }
    if(b.classList.contains('bpmstep')){ setBpm(bpm+(+b.dataset.d)); return; }
    if(b.id==='taptempo'){ const now=performance.now(); taps=taps.filter(x=>now-x<2500); taps.push(now); if(taps.length>1){ const iv=(taps[taps.length-1]-taps[0])/(taps.length-1); setBpm(Math.round(60000/iv)); } return; }
  }
  if(/^\d+ BPM$/.test(t)){ const p=$('#metropop'); p.style.display=p.style.display==='flex'?'none':'flex'; return; }
  if(b.classList.contains('folder')){ $$('.folder').forEach(x=>{const on=x===b; x.style.background=on?'rgba(255,255,255,.07)':'none'; x.style.color=on?'#f4f1ea':'#a3a3a8';}); return; }
  if(t==='SAVE'){ setPreset(PRESETS[pi],false); toast('Preset saved'); return; }
  if(t==='SAVE AS'){ toast('Save as: type a new name (coming with the real plugin)'); return; }
  if(t==='DELETE'){ toast('Delete is turned off in the demo'); return; }
  if(al==='Remove mapping'){ b.parentElement.remove(); return; }
  if(al==='Lower reference'||al==='Raise reference'){ refHz=Math.max(430,Math.min(450,refHz+(al==='Raise reference'?1:-1))); b.parentElement.children[1].textContent=refHz+' Hz'; return; }
  if(/height: 26px; padding: 0 12px; border-radius: 8px/.test(b.getAttribute('style')||'') || t==='SYNC'){ const on=!b.dataset.on||b.dataset.on==='0'; b.dataset.on=on?1:0; b.style.background=on?'var(--accent)':'none'; b.style.color=on?'#111113':'#a3a3a8'; b.style.borderColor=on?'transparent':'rgba(255,255,255,.12)'; return; }
  if(/^(env retrigger|pre \/ post)$/i.test(al)){ const lv=b.querySelector('span'); const up=!/rotate\(25deg\)/.test(lv.style.transform); lv.style.transform='rotate('+(up?25:-25)+'deg)'; return; }
  if(b.classList.contains('eqdd')){ const l=$('.eqlist'); l.style.display=l.style.display==='flex'?'none':'flex'; return; }
  if(b.classList.contains('eqp')){ const g=b.dataset.g.split(',').map(Number); $$('.fd').forEach((f,i)=>setFader(f,g[i])); $$('.eqp').forEach(x=>{const on=x===b; x.style.background=on?'rgba(255,255,255,.1)':'none'; x.style.boxShadow=on?'inset 2px 0 0 var(--accent)':'none';}); $('.eqname').textContent=b.textContent; $('.eqlist').style.display='none'; const bub=eqSvg().parentElement.lastElementChild; bub.style.display='none'; return; }
  if(b.classList.contains('eqreset')){ eqFlat(); return; }
  if(t==='LOAD DEFAULT'){ resetAll(); return; }
  if(b.classList.contains('chk')){ const on=b.dataset.on!=='1'; b.dataset.on=on?1:0; b.style.color=on?'#e9e6df':'#8d8d93'; const bx=b.firstElementChild; bx.style.background=on?'#f4f1ea':'none'; bx.style.boxShadow=on?'none':'inset 0 0 0 1.5px #4a4a50'; bx.innerHTML=on?'<svg aria-hidden="true" width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="#111113" stroke-width="3.5" stroke-linecap="round" stroke-linejoin="round"><path d="M5 12.5l4.5 4.5L19 7"/></svg>':''; return; }
  if(b.classList.contains('acc')){ setAccent(b.dataset.accent); $$('.acc').forEach(x=>x.style.boxShadow=x===b?'0 0 0 2px #0f0f11, 0 0 0 4px '+x.dataset.accent:'inset 0 0 0 1px rgba(0,0,0,.3)'); return; }
  if(b.closest('.setnav')){ $$('.setnav button').forEach(x=>{const on=x===b; x.style.background=on?'rgba(255,255,255,.07)':'none'; x.style.boxShadow=on?'inset 2px 0 0 var(--accent)':'none'; x.style.color=on?'#f4f1ea':'#a3a3a8'; x.style.fontWeight=on?'600':'400';}); $$('.spanel').forEach(p=>p.style.display=p.dataset.panel===t?'flex':'none'); return; }
  if(isDropdown(b)){ openDD(b); return; }
  if(t==='NEW FOLDER'){ const f=$$('.folder'); const last=f[f.length-1]; const c=last.cloneNode(true); c.children[1].textContent='New folder'; c.children[2].textContent='0'; last.after(c); c.click(); toast('Folder created'); return; }
  if(t==='CLEAR ALL'){ $$('button[aria-label="Remove mapping"]').forEach(x=>x.parentElement.remove()); toast('All MIDI mappings cleared'); return; }
  if(t==='Reset Device'){ resetDevice(); toast('Audio device reset'); return; }
  const MSG={'TEST':'Playing a test tone…','MIDI LEARN':'MIDI learn: move a control, then a knob on your controller','CHECK FOR UPDATES':'You have the latest version','RENAME':'Rename: type a new name (real plugin)','DUPLICATE':'Preset duplicated','SHOW IN EXPLORER':'Opens the preset folder in Explorer','CHANGE':'Choose a new preset folder','IMPORT':'Import presets from a file','EXPORT':'Export this preset to a file','RESTORE':'Factory presets restored','MANUAL':'Opens the manual','LICENCES':'Shows third-party licences','Control Panel':'Opens your interface\'s ASIO control panel','Load IR…':'Pick an impulse response file (.wav)'};
  if(MSG[t]){ toast(MSG[t]); return; }
  const par=b.parentElement; const sib=[...par.children].filter(x=>x.tagName==='BUTTON');
  if(par.classList.contains('seg') || par.classList.contains('radio2')){ const a=sib.find(isAct2); if(a&&a!==b){ const sa=a.getAttribute('style'); a.setAttribute('style',b.getAttribute('style')); b.setAttribute('style',sa); const led=a.querySelector(':scope > span:first-child'); if(led && !led.textContent.trim()) b.prepend(led);} return; }
  if(par.classList.contains('radio')){ const odd=oddOne(sib); if(odd && odd!==b){ const sa=odd.getAttribute('style'); odd.setAttribute('style',b.getAttribute('style')); b.setAttribute('style',sa); } return; }
});
// knobs
let drag=null;
function setKnob(k,v){ k.dataset.v=v; const deg=(270*v).toFixed(1); const arc=k.querySelector('.kn-arc'); if(arc) arc.style.background='conic-gradient(from 225deg, var(--accent) 0deg '+deg+'deg, #2b2b30 '+deg+'deg 270deg, transparent 270deg 360deg)';
  const r=k.querySelector('.kn-rot'); if(r) r.style.transform='rotate('+(-135+270*v)+'deg)'; const val=k.querySelector('.kn-val')||k.parentElement.querySelector(':scope > .kn-val'); if(val) val.textContent=(v*10).toFixed(1); }
// EQ
const EW=1000, EH=220; const ex=f=>Math.log2(f/31.25)/10*EW, ey=db=>EH/2-db*(EH/2-14)/15;
const NAMES={63:'63 Hz',125:'125 Hz',250:'250 Hz',500:'500 Hz',1000:'1.00 kHz',2000:'2.00 kHz',4000:'4.00 kHz',8000:'8.00 kHz',16000:'16.0 kHz'};
function eqRedraw(){ const fds=$$('.fd'); if(!fds.length) return; const bands=fds.map(f=>[+f.dataset.f,+f.dataset.g]);
  const pts=[]; for(let i=0;i<=300;i++){ const f=31.25*Math.pow(2,10*i/300); let db=0; bands.forEach(([fc,g])=>{const o=Math.log2(f/fc); db+=g*Math.exp(-0.5*(o*1.5)**2);});
    if(LCF>21) db+=10*Math.log10(1/(1+Math.pow(LCF/f,4))); if(HCF<19500) db+=10*Math.log10(1/(1+Math.pow(f/HCF,4))); db=Math.max(db,-15); pts.push(ex(f).toFixed(1)+' '+ey(db).toFixed(1)); }
  const d='M'+pts.join(' L'); const svg=$('.page[data-page="EQ"] svg[width="1000"]');
  svg.querySelectorAll('path[stroke-width="2.2"]').forEach(p=>p.setAttribute('d',d));
  svg.querySelectorAll('path[fill-opacity=".1"]').forEach(p=>p.setAttribute('d',d+' L'+EW+' '+EH/2+' L0 '+EH/2+' Z'));
  const circ=[...svg.querySelectorAll('circle[r="9"]')]; circ.forEach((c,i)=>{ const y=ey(bands[i][1]); c.setAttribute('cy',y); const tx=c.nextElementSibling; if(tx) tx.setAttribute('y',y+3.6); const pr=c.previousElementSibling; if(pr&&pr.getAttribute('r')==='15') pr.setAttribute('cy',y); }); }
function setFader(fd,g){ g=Math.max(-12,Math.min(12,Math.round(g*2)/2)); fd.dataset.g=g; const th=128, mid=th/2, pos=mid-g/12*(th/2-8);
  fd.children[1].style.top=Math.min(pos,mid)+'px'; fd.children[1].style.height=Math.abs(pos-mid)+'px'; fd.children[2].style.top=pos+'px';
  fd.previousElementSibling.textContent=(g>0?'+':'')+g+' dB';
  const bub=$('.page[data-page="EQ"] svg[width="1000"]').parentElement.lastElementChild; const f=+fd.dataset.f; const idx=$$('.fd').indexOf(fd);
  bub.style.display='flex'; bub.style.left=(ex(f)+22)+'px'; bub.style.top=(ey(g)-40)+'px'; const sp=bub.children; sp[0].textContent='BAND '+(idx+1); sp[1].textContent=NAMES[f]; sp[2].textContent=(g>0?'+':'')+g+' dB'; eqRedraw(); }
document.addEventListener('pointerdown',e=>{
  const k=e.target.closest('.kn'); if(k){ drag={type:'k',el:k,y:e.clientY,v:+k.dataset.v}; e.preventDefault(); return; }
  const fd=e.target.closest('.fd'); if(fd){ drag={type:'f',el:fd}; e.preventDefault(); moveF(e); }
});
function moveF(e){ const r=drag.el.getBoundingClientRect(); const y=(e.clientY-r.top)/r.height*128; setFader(drag.el,(64-y)/(64-8)*12); }
addEventListener('pointermove',e=>{ if(!drag) return; const sc=app.getBoundingClientRect().width/1360;
  if(drag.type==='k') setKnob(drag.el, Math.max(0,Math.min(1, drag.v+(drag.y-e.clientY)/sc/180))); else if(drag.type==='f') moveF(e); });
addEventListener('pointerup',()=>drag=null);
// meters + tuner + metronome animation
const meters=$$('div').filter(d=>/background-position: bottom/.test(d.getAttribute('style')||'')).map(d=>({d,b:parseFloat(d.style.height)}));
let metro=false, beat=0;
setInterval(()=>{ meters.forEach(m=>{ m.d.style.height=Math.max(4,Math.min(98,m.b+(Math.random()*22-14)))+'%'; }); },90);
setInterval(()=>{ const c=Math.round(Math.random()*8-5); const txt=(c>0?'+':c<0?'−':'')+Math.abs(c);
  $$('span').forEach(s=>{ if(/^[−+]?\d+ cents$/.test(s.textContent)) s.textContent=txt+' cents'; });
  const m=$('#f1 #livemini'); if(m){ const sp=m.querySelector('div > span:last-child'); const lab=m.querySelectorAll('span'); lab[lab.length-1].textContent=txt||'0'; } },420);


let accent='#f3ead8';
function setAccent(c){ document.documentElement.style.setProperty('--accent',c);
  $$('[fill],[stroke],[stop-color]').forEach(el=>['fill','stroke','stop-color'].forEach(a=>{ if((el.getAttribute(a)||'').toLowerCase()===accent) el.setAttribute(a,c); })); accent=c; }
let LCF=20, HCF=20000;
const fmtF=f=>f>=1000?(f/1000).toFixed(f>=10000?1:2)+' kHz':Math.round(f)+' Hz';
function eqSvg(){ return $('.page[data-page="EQ"] svg[width="1000"]'); }
function cutKnob(label,f){ const k=$$('.page[data-page="EQ"] .kn').find(x=>x.parentElement.textContent.includes(label)); if(!k) return;
  const v=Math.log2(f/20)/Math.log2(20000/20); setKnob(k,Math.max(0,Math.min(1,v))); const val=k.parentElement.querySelector(':scope > .kn-val'); if(val) val.textContent=(f<=20||f>=20000)?'OFF':fmtF(f); }
function setCut(which,x){ x=Math.max(0,Math.min(EW,x)); let f=Math.max(20,Math.min(20000,31.25*Math.pow(2,x/EW*10))); if(which==='LC'&&x<=16) f=20; if(which==='HC'&&x>=EW-16) f=20000;
  const svg=eqSvg(); const rects=svg.querySelectorAll('rect'); const i=which==='LC'?0:1; const r=rects[i]; const xx=ex(f);
  const mx=Math.max(14,Math.min(EW-14,xx)); r.setAttribute('x',(mx-12).toFixed(1)); const tx=r.nextElementSibling; if(tx) tx.setAttribute('x',mx.toFixed(1));
  if(which==='LC') LCF=f; else HCF=f; cutKnob(which==='LC'?'LOW CUT':'HIGH CUT',f); eqRedraw(); }
document.addEventListener('pointerdown',e=>{
  const svg=eqSvg(); if(!svg || !svg.contains(e.target)) return;
  const el=e.target; const circ=[...svg.querySelectorAll('circle[r="9"]')]; const rects=[...svg.querySelectorAll('rect')];
  let idx=circ.indexOf(el); if(idx<0 && el.tagName==='text'){ const p=el.previousElementSibling; idx=circ.indexOf(p); if(idx<0 && rects.includes(p)){ drag={type:'c',which:rects.indexOf(p)===0?'LC':'HC'}; e.preventDefault(); return; } }
  if(idx>=0){ drag={type:'n',idx}; e.preventDefault(); return; }
  const ri=rects.indexOf(el); if(ri>=0){ drag={type:'c',which:ri===0?'LC':'HC'}; e.preventDefault(); }
},true);
addEventListener('pointermove',e=>{ if(!drag || (drag.type!=='n' && drag.type!=='c')) return; const svg=eqSvg(); const r=svg.getBoundingClientRect();
  if(drag.type==='n'){ const y=(e.clientY-r.top)/r.height*EH; setFader($$('.fd')[drag.idx], (EH/2-y)/(EH/2-14)*15); }
  else setCut(drag.which,(e.clientX-r.left)/r.width*EW); });

function eqFlat(){ $$('.fd').forEach(f=>setFader(f,0)); $$('.eqp').forEach(x=>{x.style.background='none'; x.style.boxShadow='none';}); const n=$('.eqname'); if(n) n.textContent='Flat';
  const svg=eqSvg(); if(svg){ setCut('LC',0); setCut('HC',EW); svg.parentElement.lastElementChild.style.display='none'; } }
function toast(msg){ let t=$('#toast'); if(!t){ t=document.createElement('div'); t.id='toast'; t.setAttribute('role','status');
  t.style.cssText='position:absolute;left:50%;bottom:70px;transform:translateX(-50%);z-index:50;padding:10px 16px;border-radius:12px;background:rgba(24,24,27,.96);box-shadow:inset 0 0 0 1px rgba(255,255,255,.1),0 14px 30px rgba(0,0,0,.6);font-size:12px;letter-spacing:.06em;color:#e9e6df;transition:opacity .3s'; app.appendChild(t);}
  t.textContent=msg; t.style.opacity=1; clearTimeout(t._h); t._h=setTimeout(()=>t.style.opacity=0,2200); }
function resetAll(){
  $$('.kn').forEach(k=>{ if(!k.closest('.ov') && !k.closest('#ampview')) setKnob(k,.5); });
  removeAmp(); eqFlat(); tr=0; const td=$('[aria-label="Transpose down"]'); if(td) td.parentElement.children[1].children[0].textContent='0';
  $$('.pop').forEach(p=>p.style.display='none'); closeOv(); setPreset('Default',false);
  toast('Default loaded · no amps, cabs or effects, every knob at 12 o\'clock'); }

function removeAmp(){ const v=$('#ampview'); if(v) v.style.display='none'; const s=$$('.page[data-page="AMP"] button').find(x=>x.textContent.includes('ADD AMP')); if(s) s.parentElement.style.display='block'; }
document.addEventListener('pointerdown',e=>{ const k=e.target.closest('#ampview .kn'); if(k){ const r=k.querySelector('.ampread'); if(r) r.style.opacity=1; } });
addEventListener('pointerup',()=>$$('.ampread').forEach(r=>r.style.opacity=0));

let bpm=120, taps=[], refHz=440;
function setBpm(n){ bpm=Math.max(40,Math.min(240,n)); $('#bpmval').textContent=bpm; $('#bpmrange').value=bpm;
  $$('.bpmtxt').forEach(x=>x.textContent=bpm+' BPM'); }
$('#bpmrange').addEventListener('input',e=>setBpm(+e.target.value));
document.addEventListener('click',e=>{ const p=$('#metropop'); if(p.style.display==='flex' && !p.contains(e.target) && !(e.target.closest('button') && /^\d+ BPM$/.test(e.target.closest('button').textContent.trim()))) p.style.display='none'; });
const markR=(els,cls)=>els.forEach(x=>x.parentElement.classList.add(cls||'radio'));
markR($$('button[aria-label$=" wave"]')); markR($$('button[aria-label="Up"]'));
markR($$('button').filter(x=>x.textContent.trim()==='ROOT')); markR($$('button').filter(x=>x.textContent.trim()==='OSC 1'),'radio2');
markR($$('.pop button').filter(x=>x.textContent.trim()==='ALL'));
markR($$('.ov[data-ov="presets"] button').filter(x=>/grid-template-columns: 22px/.test(x.getAttribute('style')||'')).slice(0,1));
$$('.ov[data-ov="presets"] button').filter(x=>/height: 36px; border-radius: 9px/.test(x.getAttribute('style')||'') && x.querySelector('svg') && x.textContent.trim()!=='NEW FOLDER').forEach(x=>x.classList.add('folder'));

const DD=[['ASIO','WASAPI','DirectSound'],['USB Audio Interface (ASIO)','ASIO4ALL v2','Realtek ASIO'],['44100 Hz','48000 Hz','88200 Hz','96000 Hz'],
 ['64 samples (1.3 ms)','128 samples (2.7 ms)','256 samples (5.3 ms)','512 samples (10.7 ms)','1024 samples (21.3 ms)'],['16-bit','24-bit','32-bit float'],
 ['Standard','High','Highest'],['75%','100%','125%','150%'],['All devices','USB MIDI Controller','Loopback MIDI'],['Omni','1','2','3','4','5','6','7','8','9','10','11','12','13','14','15','16'],
 ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'],['Dynamic 57','Dynamic 421','Ribbon 121','Condenser 414','Condenser 87']];
const DEFAULTS={'ASIO':0,'USB Audio Interface (ASIO)':0,'48000 Hz':0,'256 samples (5.3 ms)':0,'24-bit':0};
let ddBtn=null;
function ddText(b){ const s=b.querySelector('span'); return (s?s.textContent:b.textContent).trim(); }
function setDDText(b,v){ const s=b.querySelector('span'); if(s) s.textContent=v; else { const n=[...b.childNodes].find(n=>n.nodeType===3&&n.textContent.trim()); if(n) n.textContent=v; } }
function isDropdown(b){ if(b.classList.contains('eqdd')||b.closest('#ddmenu')) return false; if(!b.querySelector('path[d="M6 9l6 6 6-6"]')) return false; const v=ddText(b); return DD.some(g=>g.includes(v)); }
function openDD(b){ const m=$('#ddmenu'); if(ddBtn===b && m.style.display==='flex'){ m.style.display='none'; ddBtn=null; return; }
  ddBtn=b; const v=ddText(b); const g=DD.find(g=>g.includes(v));
  m.innerHTML=g.map(o=>'<button role="option" style="height: 32px; flex: none; border-radius: 8px; border: none; background: '+(o===v?'rgba(255,255,255,.1)':'none')+'; box-shadow: '+(o===v?'inset 2px 0 0 var(--accent)':'none')+'; color: #e9e6df; font: inherit; font-size: 13px; text-align: left; padding: 0 12px; cursor: pointer; white-space: nowrap">'+o+'</button>').join('');
  const ar=app.getBoundingClientRect(), r=b.getBoundingClientRect(), sc=ar.width/1360;
  m.style.display='flex'; m.style.minWidth=Math.max(160,r.width/sc)+'px';
  let top=(r.bottom-ar.top)/sc+4; const h=Math.min(300, g.length*33+12); if(top+h>812) top=(r.top-ar.top)/sc-h-4;
  m.style.left=((r.left-ar.left)/sc)+'px'; m.style.top=top+'px'; }
function ddPick(o){ if(ddBtn){ setDDText(ddBtn,o.textContent); if(['75%','100%','125%','150%'].includes(o.textContent)){ zoom=parseInt(o.textContent)/100; fit(); } }
  $('#ddmenu').style.display='none'; ddBtn=null; }
function resetDevice(){ $$('.spanel[data-panel="Device"] button').forEach(b=>{ if(!b.querySelector('path[d="M6 9l6 6 6-6"]')) return; const v=ddText(b); const g=DD.find(g=>g.includes(v)); if(!g) return;
  const def={'ASIO':'ASIO','WASAPI':'ASIO','DirectSound':'ASIO'}; const want=g.includes('ASIO')&&g.includes('WASAPI')?'ASIO':g.includes('48000 Hz')?'48000 Hz':g.includes('256 samples (5.3 ms)')?'256 samples (5.3 ms)':g.includes('24-bit')?'24-bit':g[0]; setDDText(b,want); }); }
// preset search
const ps=$('#preset-search'); if(ps) ps.addEventListener('input',()=>{ const q=ps.value.toLowerCase(); $$('.ov[data-ov="presets"] button').filter(x=>/grid-template-columns: 22px/.test(x.getAttribute('style')||'')).forEach(r=>r.style.display=r.textContent.toLowerCase().includes(q)?'grid':'none'); });
// show meters toggle
document.addEventListener('click',e=>{ const b=e.target.closest('.tog[aria-label="Show level meters"]'); if(!b) return; const on=b.dataset.on==='1'; meters.forEach(m=>{ const c=m.d.parentElement.parentElement; if(!c.closest('.ov')) c.style.visibility=on?'visible':'hidden'; }); });

const ACC0='#f3ead8';
let H=40,Sa=.1,V=.95;
const hx=n=>('0'+Math.round(n).toString(16)).slice(-2);
function hsv2hex(h,s,v){ const f=n=>{const k=(n+h/60)%6; return v-v*s*Math.max(0,Math.min(k,4-k,1));}; return '#'+hx(f(5)*255)+hx(f(3)*255)+hx(f(1)*255); }
function hex2hsv(c){ const r=parseInt(c.slice(1,3),16)/255,g=parseInt(c.slice(3,5),16)/255,b=parseInt(c.slice(5,7),16)/255; const mx=Math.max(r,g,b),mn=Math.min(r,g,b),dd=mx-mn;
  let h=0; if(dd){ h=mx===r?((g-b)/dd)%6:mx===g?(b-r)/dd+2:(r-g)/dd+4; h*=60; if(h<0) h+=360; } return [h, mx?dd/mx:0, mx]; }
function drawPicker(){ const sv=$('#sv'); if(!sv) return; sv.style.background='linear-gradient(to top, #000, transparent), linear-gradient(to right, #fff, hsl('+H+',100%,50%))';
  $('#svk').style.left=(Sa*100)+'%'; $('#svk').style.top=((1-V)*100)+'%'; $('#svk').style.background=accent; $('#huek').style.left=(H/360*100)+'%'; }
function applyAcc(c, fromHex, fromPicker){ c=c.toLowerCase(); setAccent(c); const h=$('#acchex'); if(h && !fromHex) h.value=c.toUpperCase(); if(!fromPicker){ [H,Sa,V]=hex2hsv(c); } drawPicker(); }
$('#accswatch').addEventListener('click',e=>{ e.stopPropagation(); const p=$('#accpop'); if(p.style.display==='flex'){ p.style.display='none'; return; }
  const ar=app.getBoundingClientRect(), r=e.currentTarget.getBoundingClientRect(), sc=ar.width/1360; p.style.display='flex';
  let top=(r.bottom-ar.top)/sc+8; if(top+290>812) top=(r.top-ar.top)/sc-290; p.style.left=Math.min(1360-280,(r.left-ar.left)/sc)+'px'; p.style.top=top+'px';
  $('#accold').style.background=accent; [H,Sa,V]=hex2hsv(accent); drawPicker(); });
let pk=null;
function pickMove(e){ if(!pk) return; const r=pk.getBoundingClientRect(); const x=Math.max(0,Math.min(1,(e.clientX-r.left)/r.width)), y=Math.max(0,Math.min(1,(e.clientY-r.top)/r.height));
  if(pk.id==='sv'){ Sa=x; V=1-y; } else { H=x*360; } applyAcc(hsv2hex(H,Sa,V),false,true); }
['sv','hue'].forEach(id=>$('#'+id).addEventListener('pointerdown',e=>{ pk=e.currentTarget; pickMove(e); e.preventDefault(); }));
addEventListener('pointermove',pickMove); addEventListener('pointerup',()=>pk=null);
document.addEventListener('click',e=>{ const p=$('#accpop'); if(p.style.display==='flex' && !p.contains(e.target) && e.target.id!=='accswatch') p.style.display='none'; });

$('#acchex').addEventListener('input',e=>{ let v=e.target.value.trim(); if(!v.startsWith('#')) v='#'+v; if(/^#[0-9a-fA-F]{6}$/.test(v)) applyAcc(v,true); });
$('#acchex').addEventListener('blur',e=>{ e.target.value=accent.toUpperCase(); });
$('#accreset').addEventListener('click',e=>{ e.stopPropagation(); applyAcc(ACC0); toast('Accent colour reset'); });
$('#accsave').addEventListener('click',e=>{ e.stopPropagation(); try{ localStorage.setItem('obsidianAccent',accent); }catch(_){} toast('Accent colour saved'); });
try{ const s=localStorage.getItem('obsidianAccent'); if(s && /^#[0-9a-f]{6}$/.test(s)) applyAcc(s); }catch(_){}
const tip=document.createElement('div'); tip.id='tip'; tip.setAttribute('role','tooltip');
tip.style.cssText='position:absolute;z-index:90;max-width:240px;padding:7px 10px;border-radius:9px;background:rgba(14,14,16,.97);box-shadow:inset 0 0 0 1px rgba(255,255,255,.1),0 10px 24px rgba(0,0,0,.7);font-size:11px;line-height:1.4;color:#e9e6df;pointer-events:none;opacity:0;transition:opacity .12s;white-space:normal';
app.appendChild(tip);
const TABTIP={'SPECIAL FX':'Special effects like pitch, freeze and glitch','PEDALS':'Drive and boost pedals before the amp','SYNTH':'Synth that follows what you play on the guitar','AMP':'The amp head','CAB':'Speaker cabinet and mics','EQ':'9-band EQ after the cab','FX':'Delay, reverb and modulation after the amp'};
const KTIP={'INPUT':'Input gain: how hard your guitar hits the amp','GATE':'Noise gate: cuts hiss and hum when you stop playing','DOUBLER':'Doubler: fakes a double-tracked guitar for width','OUTPUT':'Output volume','SENSITIVITY':'How easily the synth picks up your notes','GLIDE':'Slide time between notes','MIX':'Blend between guitar and synth','DECAY':'How fast the synth note fades','SUSTAIN':'Level the synth holds while the note rings','SEMITONES':'Tune the oscillator up or down','DETUNE':'Slightly out-of-tune for a thicker sound','LEVEL':'Oscillator volume','WIDTH':'Stereo spread of the unison voices','CUTOFF':'Filter brightness','RESONANCE':'Boost right at the cutoff for a sharper sound','ATTACK':'How fast the filter opens','DRIVE':'Saturation inside the synth','LOW CUT':'Removes rumble below this frequency','HIGH CUT':'Removes fizz above this frequency','POSITION':'Mic position from centre to edge of the speaker','DISTANCE':'How far the mic is from the speaker','PAN':'Left or right in the stereo field'};
const BTIP={'Previous preset':'Previous preset','Next preset':'Next preset','Transpose down':'Transpose down a semitone','Transpose up':'Transpose up a semitone','Start metronome':'Start the metronome','Stop metronome':'Stop the metronome','Close presets':'Close','Close settings':'Close','Close tuner':'Close','Open colour picker':'Pick a colour','Gate on':'Turn the noise gate on or off','Doubler on':'Turn the doubler on or off','Live tuner':'Small tuner in the bottom bar while you play','Lower reference':'Lower the tuning reference','Raise reference':'Raise the tuning reference'};
const TTIP={'TUNER':'Open the tuner','SETTINGS':'Audio device, MIDI, look and presets','SAVE':'Save changes to this preset','SAVE AS':'Save as a new preset','DELETE':'Delete this preset','LOAD':'Load the selected preset','LOAD DEFAULT':'Empty rig, every knob at 12 o\'clock','RESET':'Back to the original settings','TAP TEMPO':'Tap a few times to set the tempo','ADD PEDAL':'Choose a pedal','ADD EFFECT':'Choose an effect','ADD SPECIAL FX':'Choose a special effect','SYNC':'Lock the rate to the tempo'};
function knobLabel(k){ const sp=[...k.parentElement.querySelectorAll('span')].filter(s=>!k.contains(s) && /^[A-Z][A-Z .\/]+$/.test(s.textContent.trim())); return sp.length?sp[0].textContent.trim():''; }
function tipFor(el){
  if(tabBtns.includes(el)){ const n=el.textContent.trim(); return (TABTIP[n]||n)+(bypassed.has(n)?' · bypassed':'')+'. Double-click to bypass.'; }
  if(el.classList.contains('kn')){ const l=knobLabel(el); return (KTIP[l]||(l?l.charAt(0)+l.slice(1).toLowerCase():'Knob'))+'. Drag up or down.'; }
  if(el.classList.contains('fd')){ return (+el.dataset.f>=1000?(+el.dataset.f/1000)+' kHz':el.dataset.f+' Hz')+' band. Drag to boost or cut.'; }
  const al=el.getAttribute('aria-label'); if(al&&BTIP[al]) return BTIP[al];
  const t=el.textContent.trim(); if(TTIP[t]) return TTIP[t]; if(/^\d+ BPM$/.test(t)) return 'Metronome tempo and time signature';
  if(el===$('[aria-label="Previous preset"]').nextElementSibling) return 'Open the preset browser';
  if(el.classList.contains('eqdd')) return 'Pick an EQ preset';
  return '';
}
let tipT=null, tipEl=null;
function tipsOn(){ const tg=$('.tog[aria-label="Show tooltips"]'); return !tg || tg.dataset.on==='1'; }
document.addEventListener('mouseover',e=>{
  const el=e.target.closest('.kn,.fd,button'); if(el===tipEl) return; tipEl=el; clearTimeout(tipT); tip.style.opacity=0;
  if(!el || !tipsOn() || drag) return; const txt=tipFor(el); if(!txt) return;
  tipT=setTimeout(()=>{ tip.textContent=txt; const ar=app.getBoundingClientRect(), r=el.getBoundingClientRect(), sc=ar.width/1360;
    tip.style.opacity=0; tip.style.left='0px'; tip.style.top='0px'; const tw=tip.offsetWidth, th=tip.offsetHeight;
    let x=(r.left+r.width/2-ar.left)/sc-tw/2, y=(r.top-ar.top)/sc-th-8; if(y<4) y=(r.bottom-ar.top)/sc+8;
    tip.style.left=Math.max(6,Math.min(1360-tw-6,x))+'px'; tip.style.top=y+'px'; tip.style.opacity=1; },450); });
document.addEventListener('pointerdown',()=>{ clearTimeout(tipT); tip.style.opacity=0; },true);

setTab('AMP');
'''

html = f'''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Obsidian Amp Sim Demo</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=Saira:wght@400;500;600&display=swap" rel="stylesheet">
<style>
:root{{--accent:#f3ead8;color-scheme:dark}}
html,body{{height:100%;margin:0;background:#050506;overflow:hidden}}
body{{display:flex;align-items:center;justify-content:center}}
#app{{transform-origin:center center;flex:none}}
.grain{{position:absolute;inset:0;background-image:url(grain.png);background-size:256px 256px;mix-blend-mode:overlay;opacity:.2;pointer-events:none;z-index:5}}
#app{{filter:contrast(1.04) brightness(1.02)}}
button:focus-visible{{outline:2px solid var(--accent);outline-offset:2px}}
input::placeholder{{color:#77777d}}
.page[data-page="EQ"] svg circle[r="9"], .page[data-page="EQ"] svg text{{cursor:ns-resize}}
.page[data-page="EQ"] svg rect{{cursor:ew-resize}}
</style>
</head>
<body>
<div id="app" style="width: 1360px; height: 820px; box-sizing: border-box; position: relative; isolation: isolate; overflow: hidden; display: flex; flex-direction: column; border-radius: 14px; box-shadow: 0 30px 80px rgba(0,0,0,.7); background: radial-gradient(120% 90% at 50% 0%, #222226 0%, #111113 55%, #08080a 100%); color: #e9e6df; font-family: 'Saira', 'Segoe UI', system-ui, sans-serif">
{body}
<div class="grain" aria-hidden="true"></div>
</div>
<script>{JS}</script>
</body>
</html>
'''
open('/home/claude/demo/index.html','w').write(html)
print(len(html))
