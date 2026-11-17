import json
from batch4 import *

def big_slot(kind, active=False):
    br = f'1.5px dashed {A}' if active else '1.5px dashed rgba(255,255,255,.18)'
    bg = 'rgba(255,255,255,.045)' if active else 'rgba(255,255,255,.018)'
    return (f'<button style="width: 980px; height: 248px; flex: none; box-sizing: border-box; border-radius: 18px; border: {br}; background: {bg}; color: {TXT}; font: inherit; cursor: pointer; display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 12px">'
            f'<span style="width: 58px; height: 58px; border-radius: 50%; display: flex; align-items: center; justify-content: center; background: rgba(255,255,255,.06); box-shadow: inset 0 0 0 1px rgba(255,255,255,.08)">{svg("plus",24,1.7)}</span>'
            f'<span style="font-size: 13px; font-weight: 600; letter-spacing: .26em; margin-right: -.26em">ADD {kind}</span>'
            f'<span style="font-size: 11px; color: {MUT}">Choose {"an amp" if kind=="AMP" else "a cab"} from the list</span></button>')

def amp_picker():
    p = picker_pop('CHOOSE AMP', ['ALL','CLEAN','CRUNCH','HIGH GAIN'], 'amp')
    return p.replace('width: 38px; height: 54px; border-radius: 5px','width: 76px; height: 34px; border-radius: 4px')

def stage_f(overlay='', picker=False):
    pop = f'<div style="position: absolute; right: 70px; top: 26px">{amp_picker()}</div>' if picker else ''
    return (f'<div style="flex: 1; min-height: 0; position: relative; overflow: hidden; display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 16px; background: radial-gradient(60% 70% at 50% 8%, rgba(255,255,255,.06), transparent 70%), linear-gradient(#0c0c0d, #121214)">'
            f'{big_slot("AMP", picker)}{big_slot("CAB")}{pop}{overlay}</div>')

boards = [
 ('FinalAmp.dc.html', pg('Obsidian · Amp','AMP',stage_f(picker=True))),
 ('FinalPresets.dc.html', pg('Obsidian · Presets','AMP',stage_f(dim_overlay(presets_card())))),
 ('FinalSettings.dc.html', pg('Obsidian · Settings','AMP',stage_f(dim_overlay(settings_card())),settings=True)),
 ('FinalTuner.dc.html', pg('Obsidian · Tuner','AMP',stage_f(tuner_f()),tuner=True)),
 ('FinalLive.dc.html', pg('Obsidian · Live tuner','AMP',stage_f(),live=True)),
]
for fn,h in boards: open(OUT+fn,'w').write(h)
print('ok')
