"""Cab pictures for the plugin's CAB page (the IR cabs in plugin/Content/Cabs).

    python3 plugin/tools/faces/build_cabs.py [keys...]

Takes the straight-on renders visuals/out/front/<key>.png (made by visuals/render/cab.py, transparent background),
scales them to plugin/Content/Faces/img/cabs/<key>.webp and writes the picture fields into plugin/Content/Cabs/<key>.json:
  img            "cabs/<key>.webp"   (the plugin serves Documents\\<product>\\Faces\\img\\... as faces/img/...)
  imgW, imgH     its size in pixels
  screen, button where the cab's display and voice button are, in those pixels (from visuals/out/front/<key>_map.json):
                 the CAB page writes the voice name on the display, and the drawn button steps through the voices.
"""
import json, os, sys
import numpy as np
from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
OUT = os.path.join(REPO, 'visuals', 'out', 'front')
CABS = os.path.join(REPO, 'plugin', 'Content', 'Cabs')
IMG = os.path.join(REPO, 'plugin', 'Content', 'Faces', 'img', 'cabs')
H = 640          # picture height: twice what the CAB page shows, so it's sharp on high-DPI screens

os.makedirs(IMG, exist_ok=True)
keys = sys.argv[1:] or sorted(f[:-5] for f in os.listdir(CABS) if f.endswith('.json'))
for k in keys:
    png = os.path.join(OUT, k + '.png'); jf = os.path.join(CABS, k + '.json')
    if not os.path.exists(png) or not os.path.exists(jf):
        print('skip', k, '(no render yet)' if not os.path.exists(png) else '(no cab file)'); continue
    im = Image.open(png).convert('RGBA'); w0, h0 = im.size
    # clean the transparent background: the denoiser and the shadow catcher leave a 1-3 % alpha haze that shows as a
    # faint box (and casts a box-shaped drop shadow in the UI); keep the cab and a soft floor shadow, fade that at the sides
    px = np.array(im).astype(np.float32); a = px[..., 3]
    a = np.clip((a - 12.0) * 255.0 / 243.0, 0, 255)
    x = np.arange(w0, dtype=np.float32); ramp = np.clip(np.minimum(x, w0 - 1 - x) / (0.05 * w0), 0, 1)[None, :]
    a = np.where(a < 110, a * ramp, a)
    px[..., 3] = a; im = Image.fromarray(px.round().astype(np.uint8), 'RGBA')
    s = H / h0; im = im.resize((round(w0 * s), H), Image.LANCZOS)
    im.save(os.path.join(IMG, k + '.webp'), 'WEBP', quality=90, method=6)
    o = json.load(open(jf))
    o.update(img='cabs/' + k + '.webp', imgW=im.size[0], imgH=im.size[1])
    for f in ('screen', 'button'): o.pop(f, None)
    mp = os.path.join(OUT, k + '_map.json')
    if os.path.exists(mp):
        m = json.load(open(mp))
        for f in ('display', 'button'):
            if f in m: o['screen' if f == 'display' else f] = [round(v * s, 1) for v in m[f]]
    json.dump(o, open(jf, 'w'), indent=1, ensure_ascii=False)
    print(k, im.size, os.path.getsize(os.path.join(IMG, k + '.webp')) // 1024, 'KB')
