from batch5 import *

def slot_amp(active):
    br = f'1.5px dashed {A}' if active else '1.5px dashed rgba(255,255,255,.18)'
    bg = 'rgba(255,255,255,.045)' if active else 'rgba(255,255,255,.018)'
    return (f'<button style="width: 1160px; height: 372px; box-sizing: border-box; border-radius: 18px; border: {br}; background: {bg}; color: {TXT}; font: inherit; cursor: pointer; display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 14px">'
            f'<span style="width: 64px; height: 64px; border-radius: 50%; display: flex; align-items: center; justify-content: center; background: rgba(255,255,255,.06); box-shadow: inset 0 0 0 1px rgba(255,255,255,.08)">{svg("plus",26,1.7)}</span>'
            f'<span style="font-size: 14px; font-weight: 600; letter-spacing: .26em; margin-right: -.26em">ADD AMP</span><span style="font-size: 11px; color: {MUT}">Choose an amp from the list</span></button>')
def slot_cab():
    return (f'<button style="width: 1250px; height: 330px; box-sizing: border-box; border-radius: 18px 18px 0 0; border: 1.5px dashed rgba(255,255,255,.18); border-bottom: none; background: rgba(255,255,255,.018); color: {TXT}; font: inherit; cursor: pointer; display: flex; align-items: flex-start; justify-content: center; padding-top: 44px">'
            f'<span style="display: flex; align-items: center; gap: 16px"><span style="width: 52px; height: 52px; border-radius: 50%; display: flex; align-items: center; justify-content: center; background: rgba(255,255,255,.06); box-shadow: inset 0 0 0 1px rgba(255,255,255,.08)">{svg("plus",22,1.7)}</span>'
            f'<span style="display: flex; flex-direction: column; align-items: flex-start; gap: 4px"><span style="font-size: 13px; font-weight: 600; letter-spacing: .26em">ADD CAB</span><span style="font-size: 11px; color: {MUT}">Choose a cab from the list</span></span></span></button>')

def stage_f(overlay='', picker=False):
    pop = f'<div style="position: absolute; right: 60px; top: 40px">{amp_picker()}</div>' if picker else ''
    return (f'<div style="flex: 1; min-height: 0; position: relative; overflow: hidden; background: radial-gradient(60% 70% at 50% 8%, rgba(255,255,255,.06), transparent 70%), linear-gradient(#0c0c0d, #121214)">'
            f'<div style="position: absolute; left: 50%; top: 412px; transform: translateX(-50%)">{slot_cab()}</div>'
            f'<div style="position: absolute; left: 50%; top: 24px; transform: translateX(-50%)">{slot_amp(picker)}</div>{pop}{overlay}</div>')

boards = [
 ('FinalAmp.dc.html', pg('Obsidian · Amp','AMP',stage_f(picker=True))),
 ('FinalPresets.dc.html', pg('Obsidian · Presets','AMP',stage_f(dim_overlay(presets_card())))),
 ('FinalSettings.dc.html', pg('Obsidian · Settings','AMP',stage_f(dim_overlay(settings_card())),settings=True)),
 ('FinalTuner.dc.html', pg('Obsidian · Tuner','AMP',stage_f(tuner_f()),tuner=True)),
 ('FinalLive.dc.html', pg('Obsidian · Live tuner','AMP',stage_f(),live=True)),
]
for fn,h in boards: open(OUT+fn,'w').write(h)
print('ok')
