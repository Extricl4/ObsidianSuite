"""Build the interactive finished-amps page from the layer renders.  python3 build_page.py [--partial]"""
import json, os, sys, math, html
from PIL import Image
sys.path.insert(0, os.path.dirname(__file__))
from amps_data import AMPS

VIS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))      # visuals/
L = os.path.join(VIS, 'out', 'layers')                                 # head.py ... out=<layer> writes here
OUT = os.path.join(VIS, 'finished', 'site'); IMG = OUT + '/img'
os.makedirs(IMG, exist_ok=True)
partial = '--partial' in sys.argv

def need(skin):
    fs = ['on.png', 'off.png', 'ptr.png', 'map.json']
    return all(os.path.exists(f'{L}/{skin}/{f}') for f in fs)

def crop(im, x0, y0, x1, y1, path):
    im.crop((x0, y0, x1, y1)).save(path, optimize=True)

data = []
for a in AMPS:
    if not all(need(s) for s in a['skins']) or not os.path.exists(f"{L}/{a['skins'][-1]}/led.png"):
        if partial: print('skip', a['key']); continue
        raise SystemExit('missing layers for ' + a['key'])
    skins = []
    m = json.load(open(f"{L}/{a['skins'][0]}/map.json")); W, H = m['w'], m['h']
    knobs = [h for h in m['hot'] if h['kind'] == 'knob']
    spec = json.load(open(os.path.join(VIS, "amps", a["skins"][0] + ".json")))
    kspec = [k for z in spec['zones'] for k in z.get('knobs', [])]
    kb = []
    for i, k in enumerate(knobs):
        cx, cy = k['px']; hr = math.ceil(k['rpx'] * 1.18) + 2
        x0, y0 = int(math.floor(cx - hr)), int(math.floor(cy - hr)); x1, y1 = int(math.ceil(cx + hr)), int(math.ceil(cy + hr))
        kb.append(dict(b=[x0, y0, x1 - x0, y1 - y0], o=[round((cx - x0) / (x1 - x0) * 100, 3), round((cy - y0) / (y1 - y0) * 100, 3)],
                       c=[round(cx, 1), round(cy, 1)], r=round(k['rpx'], 1), v=(kspec[i].get('v', 0.5) if i < len(kspec) else 0.5), label=a['knobs'][i]))
    for s in a['skins']:
        for st in ('on', 'off'):
            Image.open(f'{L}/{s}/{st}.png').convert('RGB').save(f'{IMG}/{s}_{st}.jpg', quality=84, optimize=True, progressive=True)
        pr = Image.open(f'{L}/{s}/ptr.png'); po = Image.open(f'{L}/{s}/ptr_off.png') if os.path.exists(f'{L}/{s}/ptr_off.png') else None
        for i, k in enumerate(kb):
            x0, y0, w, h = k['b']
            crop(pr, x0, y0, x0 + w, y0 + h, f'{IMG}/{s}_k{i}.png')
            if po: crop(po, x0, y0, x0 + w, y0 + h, f'{IMG}/{s}_k{i}o.png')
        skins.append(dict(id=s, ptrOff=bool(po)))
    led = Image.open(f"{L}/{a['skins'][-1]}/led.png"); leds = {}
    for h in m['hot']:
        if h['kind'] != 'led': continue
        bx = h["box"]; p = 3
        x0, y0, x1, y1 = int(min(bx[0], bx[2]) - p), int(min(bx[1], bx[3]) - p), int(max(bx[0], bx[2]) + p), int(max(bx[1], bx[3]) + p)
        crop(led, x0, y0, x1, y1, f"{IMG}/{a['key']}_led_{h['id']}.png"); leds[h['id']] = [x0, y0, x1 - x0, y1 - y0]
    buttons = {h['id']: [round(min(h['box'][0], h['box'][2]), 1), round(min(h['box'][1], h['box'][3]), 1), round(abs(h['box'][2] - h['box'][0]), 1), round(abs(h['box'][3] - h['box'][1]), 1)]
               for h in m['hot'] if h['kind'] == 'button'}
    pw = [h for h in m['hot'] if h['kind'] == 'jewel'][0]
    d = {k: a[k] for k in ('key', 'name', 'accent', 'based', 'pack', 'by', 'nfiles', 'kind', 'gain', 'dims', 'caps', 'intro', 'notes', 'switches') if k in a}
    scr = [h for h in m['hot'] if h['kind'] == 'screen']
    if scr:
        bx = scr[0]['box']; d['screen'] = [round(min(bx[0], bx[2]), 1), round(min(bx[1], bx[3]), 1), round(abs(bx[2] - bx[0]), 1), round(abs(bx[3] - bx[1]), 1)]
    d['loader'] = a.get('loader', False)
    for j, names in (a.get('steps') or {}).items():
        kb[j]['steps'] = len(names); kb[j]['stepNames'] = names
    if a.get('sel'): d['sel'] = a['sel']
    d.update(mid=a.get('mid'), master=a.get('master'), esr=a.get('esr'), new=a.get('new', False), W=W, H=H, skins=skins, knobs=kb, leds=leds, buttons=buttons,
             power=[round(pw['px'][0], 1), round(pw['px'][1], 1), round(pw['rpx'], 1)])
    data.append(d); print('ok', a['key'])

tpl = open(os.path.join(os.path.dirname(__file__), 'page_template.html')).read()
open(f'{OUT}/index.html', 'w').write(tpl.replace('/*DATA*/[]', json.dumps(data, separators=(',', ':'))))
print('page written', len(data), 'amps')
