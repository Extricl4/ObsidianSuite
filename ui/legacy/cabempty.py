from batch6 import *
def cab_stage_empty():
    slot = (f'<div style="display: flex; flex-direction: column; align-items: center; gap: 14px"><button style="width: 400px; height: 400px; box-sizing: border-box; border-radius: 18px; border: 1.5px dashed {A}; background: rgba(255,255,255,.04); color: {TXT}; font: inherit; cursor: pointer; display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 14px">'
            f'<span style="width: 60px; height: 60px; border-radius: 50%; display: flex; align-items: center; justify-content: center; background: rgba(255,255,255,.06); box-shadow: inset 0 0 0 1px rgba(255,255,255,.08)">{svg("plus",24,1.7)}</span>'
            f'<span style="font-size: 13px; font-weight: 600; letter-spacing: .26em; margin-right: -.26em">ADD CAB</span><span style="font-size: 11px; color: {MUT}">Choose a cab from the list</span></button>'
            f'<span style="font-size: 10px; letter-spacing: .2em; color: {MUT}">MICS APPEAR ONCE A CAB IS CHOSEN</span></div>')
    cards = ('<div style="display: flex; gap: 16px; opacity: .4">'
             + obs.mic_card('MIC A','Dynamic 57',[('POSITION',.38,'0.38'),('DISTANCE',.25,'2 cm'),('LEVEL',.72,'0.0 dB'),('PAN',.5,'C')])
             + obs.mic_card('MIC B','Ribbon 121',[('POSITION',.62,'0.62'),('DISTANCE',.45,'6 cm'),('LEVEL',.55,'−4.5 dB'),('PAN',.5,'C')]) + '</div>')
    ir = (f'<div style="border-radius: 18px; padding: 14px 18px; display: flex; align-items: center; gap: 14px; {CARD}">'
          f'<span style="{LBL}; width: 84px">IR LOADER</span>'
          f'<button style="height: 36px; padding: 0 14px; border-radius: 10px; {RAISED}; color: {TXT}; font: inherit; font-size: 12px; letter-spacing: .1em; cursor: pointer; display: flex; align-items: center; gap: 8px">{svg("folder",15)}Load IR…</button>'
          f'<span style="font-size: 12px; color: {MUT}">Or load your own impulse response</span><div style="flex: 1"></div>'
          f'{hk("LOW CUT",.2,"80 Hz")}{hk("HIGH CUT",.75,"9.5 kHz")}</div>')
    right = f'<div style="display: flex; flex-direction: column; gap: 18px">{cards}{ir}</div>'
    return plain_stage(f'<div style="display: flex; align-items: center; gap: 46px">{slot}{right}</div>')
open(OUT+'FinalCab.dc.html','w').write(pg('Obsidian · Cab','CAB',cab_stage_empty()))
print('ok')
