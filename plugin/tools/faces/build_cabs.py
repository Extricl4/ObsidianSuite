"""Cab pictures and mic layout for the plugin's CAB page (the IR cabs in plugin/Content/Cabs).

    python3 plugin/tools/faces/build_cabs.py [keys...]

Takes the straight-on renders visuals/out/front/<key>.png (made by visuals/render/cab.py, transparent background),
scales them to plugin/Content/Faces/img/cabs/<key>.webp and writes the picture fields into plugin/Content/Cabs/<key>.json:
  img            "cabs/<key>.webp"   (the plugin serves Documents\\<product>\\Faces\\img\\... as faces/img/...)
  imgW, imgH     its size in pixels
  screen, button where the cab's display and voice button are, in those pixels (from visuals/out/front/<key>_map.json):
                 the CAB page writes the voice name on the display, and the drawn button steps through the voices.
  (the IR loader cab, Proteus, uses screen + button too: its screen shows your IR, the button loads it.)

The mic layout is written for every cab with a spec in visuals/cabs (it needs no render, only the spec, read-only):
  speakers       [[cx, cy, rx, ry], ...]  every speaker's centre and radius in picture pixels, in reading order
                 (top-left first). The CAB page draws them and the A/B mics are dragged onto them.
  spots          for cabs whose IRs were recorded per speaker and position ("... SPKR n position m.wav"):
                 [{"spk": [speaker indices], "pos": 0..1, "ir": "Cabs/..."}]. SPKR 1 is the left column of speakers,
                 SPKR 2 the right one. Of a speaker's two positions the brighter IR is the one nearer the dust cap
                 (pos 0.2), the darker one is further out (pos 0.55). Dragging a mic picks the nearest real IR.
"""
import glob, json, math, os, re, sys
import numpy as np
from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
OUT = os.path.join(REPO, 'visuals', 'out', 'front')
SPECS = os.path.join(REPO, 'visuals', 'cabs')
CABS = os.path.join(REPO, 'plugin', 'Content', 'Cabs')
IRS = os.path.join(REPO, 'plugin', 'Content', 'IRs')
IMG = os.path.join(REPO, 'plugin', 'Content', 'Faces', 'img', 'cabs')
H = 640          # picture height: twice what the CAB page shows, so it's sharp on high-DPI screens


# ---------------------------------------------------------------------------------- speakers in picture pixels
# The same geometry and camera as visuals/render/cab.py (front view): the grille surface, the speakers on it at
# (x, distance along it from its bottom edge), and an 85 mm camera straight in front at half the cab's height.
def layout(S, imgW, imgH):
    W, D, Hc = S['size']
    SL = S.get('slant')
    HS, SB = (SL['z'], SL['back']) if SL else (Hc, 0.0)
    BD = S.get('border', {}); BB, BT = BD.get('bottom', 4.0), BD.get('top', 4.0)
    RD = S.get('recess', 1.0)
    RV = S.get('reveal'); INS = RV.get('w', 0.6) if RV else 0.0
    CAS = S.get('casters'); CH = CAS.get('h', 8.0) if CAS else 0.0
    if not CAS and S.get('feet'): CH = S['feet']

    def norm(v): l = math.hypot(*v); return (v[0] / l, v[1] / l)
    U = norm((SB, Hc - HS)) if SL else (0.0, 1.0)
    NIN = (U[1], -U[0])
    lo = (RD, BB + INS)
    if SL:
        p0 = (NIN[0] * RD, HS + NIN[1] * RD)
        t = (RD - p0[0]) / U[0] if abs(U[0]) > 1e-6 else 0.0
        path = [lo, (p0[0] + U[0] * t, p0[1] + U[1] * t), (SB - U[0] * (BT + INS) + NIN[0] * RD, Hc - U[1] * (BT + INS) + NIN[1] * RD)]
    else:
        path = [lo, (RD, Hc - BT - INS)]

    def point(s):                       # point on the grille (y, z) and the direction up it
        acc = 0.0
        for i in range(len(path) - 1):
            a, b = path[i], path[i + 1]; L = math.hypot(b[0] - a[0], b[1] - a[1])
            if s <= acc + L or i == len(path) - 2:
                t = (s - acc) / L if L else 0.0; d = ((b[0] - a[0]) / L, (b[1] - a[1]) / L)
                return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t), d
            acc += L

    HT = Hc + CH; m = S.get('margin', 0.03)
    TH = S.get('top_handle')            # a strap handle on top: the frame is taller by its height (as in cab.py)
    FT = HT + ((TH.get('h', 4.5) + TH.get('t', 1.1) + 0.8) if TH else 0.0)
    fw, fh = W + 2.0 + 2 * m * FT, FT * (1 + 2 * m)
    ry = int(S.get('px', 1000)); rx = int(round(ry * fw / fh))
    dist = (fh / 2) / (18 / 85)
    kx, ky = imgW / rx, imgH / ry

    def px(x, y, z):
        d = y + dist; t = 2 * d * 18 / 85
        return (0.5 + x / (t * rx / ry)) * rx * kx, (0.5 - (z + CH - FT / 2) / t) * ry * ky

    out = []
    for (sx, along, r) in S.get('speakers', []):
        (y, z), d = point(along)
        c = px(sx, y, z)
        l, rr = px(sx - r, y, z), px(sx + r, y, z)
        tp, bt = px(sx, y + d[0] * r, z + d[1] * r), px(sx, y - d[0] * r, z - d[1] * r)
        out.append([round(c[0], 1), round(c[1], 1), round((rr[0] - l[0]) / 2, 1), round(abs(bt[1] - tp[1]) / 2, 1)])
    out.sort(key=lambda s: (round(s[1] / max(4.0, s[3] * 0.5)), s[0]))     # rows top to bottom, then left to right
    return out


# ------------------------------------------------------------------------- recorded spots (per-speaker IR packs)
def brightness(path):
    import soundfile as sf
    x, sr = sf.read(path); x = x if x.ndim == 1 else x[:, 0]
    X = np.abs(np.fft.rfft(x, 8192)) ** 2; fr = np.fft.rfftfreq(8192, 1 / sr)
    band = (fr >= 80) & (fr < 10000)
    return float((X * fr)[band].sum() / X[band].sum())


def spots(o, speakers):
    files = set()
    for v in [o] + list(o.get('voices') or []):
        for k in ('irA', 'irB'):
            if v.get(k): files.add(v[k])
    grid = {}
    for f in files:
        mm = re.search(r'SPKR (\d+) position (\d+)', f)
        if mm: grid.setdefault(int(mm.group(1)), {})[int(mm.group(2))] = f
    if len(grid) < 2 or not speakers: return None
    xs = sorted({s[0] for s in speakers}); mid = (xs[0] + xs[-1]) / 2
    cols = {1: [i for i, s in enumerate(speakers) if s[0] < mid], 2: [i for i, s in enumerate(speakers) if s[0] >= mid]}
    out = []
    for n, pos in sorted(grid.items()):
        ranked = sorted(pos.values(), key=lambda f: -brightness(os.path.join(IRS, f)))     # brightest first = nearest the cap
        for i, f in enumerate(ranked):
            out.append(dict(spk=cols.get(n) or list(range(len(speakers))), pos=[0.2, 0.55, 0.8][min(i, 2)], ir=f))
    return out


os.makedirs(IMG, exist_ok=True)
keys = sys.argv[1:] or sorted(f[:-5] for f in os.listdir(CABS) if f.endswith('.json'))
for k in keys:
    png = os.path.join(OUT, k + '.png'); jf = os.path.join(CABS, k + '.json')
    if not os.path.exists(jf): print('skip', k, '(no cab file)'); continue
    o = json.load(open(jf))
    if os.path.exists(png):
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
        o.update(img='cabs/' + k + '.webp', imgW=im.size[0], imgH=im.size[1])
        for f in [f for f in o if f.startswith(('screen', 'button'))]: o.pop(f, None)
        mp = os.path.join(OUT, k + '_map.json')
        if os.path.exists(mp):
            # every screen and button by id: display -> screen, displayA -> screenA, button / buttonA -> the same
            m = json.load(open(mp))
            for f, box in m.items():
                if f.startswith(('display', 'button')):
                    o[('screen' + f[7:]) if f.startswith('display') else f] = [round(v * s, 1) for v in box]
        print(k, im.size, os.path.getsize(os.path.join(IMG, k + '.webp')) // 1024, 'KB')
    elif not o.get('imgW'):
        print('skip', k, '(no render yet)'); continue
    spec = os.path.join(SPECS, k + '.json')
    if os.path.exists(spec):
        o['speakers'] = layout(json.load(open(spec)), o['imgW'], o['imgH'])
        sp = spots(o, o['speakers'])
        if sp: o['spots'] = sp
        else: o.pop('spots', None)
        print(k, len(o['speakers']), 'speakers' + (', %d recorded spots' % len(sp) if sp else ''))
    txt = json.dumps(o, indent=1, ensure_ascii=False)
    # keep number lists and the spots on one line each
    txt = re.sub(r'\[[\s\d.,-]+\]', lambda mm: re.sub(r'\s+', '', mm.group(0)).replace(',', ', '), txt)
    txt = re.sub(r'\{\s+"spk": (\[[^\]]*\]),\s+"pos": ([\d.]+),\s+"ir": ("[^"]*")\s+\}', r'{"spk": \1, "pos": \2, "ir": \3}', txt)
    open(jf, 'w').write(txt)
