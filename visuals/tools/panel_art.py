"""Control-panel art (labels, dial scales, accent lines) as a transparent PNG that head.py lays on the panel.
dial: 'lines' (ticks), 'numbers' (0-10), 'numbers1' (1-10), 'roman' (I-X), 'dots', 'none'."""
import math
from PIL import Image, ImageDraw, ImageFont

FD = __file__.rsplit('/', 2)[0] + '/render/fonts/'
FONTS = {'cinzel': 'Cinzel.ttf', 'cdec': 'CinzelDecorative-Bold.ttf.ttf', 'cdecr': 'CinzelDecorative-Regular.ttf',
         'frak': 'UnifrakturCook-Bold.ttf.ttf', 'mich': 'Michroma-Regular.ttf.ttf', 'saira': 'Saira.ttf',
         'fell': 'IMFeENsc28P.ttf', 'pirata': 'PirataOne-Regular.ttf', 'medieval': 'MedievalSharp.ttf', 'uncial': 'UncialAntiqua-Regular.ttf',
         'meta': 'Metamorphous-Regular.ttf', 'almendra': 'AlmendraSC-Regular.ttf', 'grenze': 'GrenzeGotisch.ttf', 'marcellus': 'MarcellusSC-Regular.ttf',
         'cormorant': 'CormorantSC-SemiBold.ttf', 'runic': 'NotoSansRunic-Regular.ttf', 'rocker': 'NewRocker-Regular.ttf', 'macondo': 'Macondo-Regular.ttf',
         'playfair': 'PlayfairDisplaySC-Bold.ttf', 'texturina': 'Texturina.ttf', 'jacquard': 'Jacquard24-Regular.ttf'}
PX = 120
ROMAN = ['0', 'I', 'II', 'III', 'IV', 'V', 'VI', 'VII', 'VIII', 'IX', 'X']


def font(k, s, wght=None):
    f = ImageFont.truetype(FD + FONTS[k], s)
    try:
        axes = f.get_variation_axes()
        if axes:
            f.set_variation_by_axes([wght or (700 if k in ('cinzel',) else 600)] + [a['default'] for a in axes[1:]])
    except Exception:
        pass
    return f


def spaced(d, xy, t, f, fill, sp):
    tot = sum(d.textlength(c, font=f) for c in t) + sp * (len(t) - 1); x = xy[0] - tot / 2
    for c in t:
        d.text((x, xy[1]), c, font=f, fill=fill, anchor='lm'); x += d.textlength(c, font=f) + sp


def panel(path, spec, zone, knobs, toggles=(), jewels=(), face='cinzel', col=(200, 196, 190, 255), size=30,
          dial='lines', numface=None, numsize=None, tick_len=0.13, lines=(), extra=None, spacing=4, arc=False, upper=False):
    W = spec['size'][0]; B = spec['border']; AW = W - 2 * B - 0.1
    za, zb = zone
    w, h = int(AW * PX), int((zb - za) * PX)
    im = Image.new('RGBA', (w, h), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    P = lambda x, z: ((x + AW / 2) * PX, (zb - z) * PX)
    f = font(face, size)
    nf = font(numface or face, numsize or int(size * 0.62))
    for k in knobs:
        cx, cy = P(k['x'], k['z']); kr = k.get('rr', k['r'] * {'skirt': 1.42, 'skirt2': 1.22, 'witch': 1.18, 'spire': 1.18}.get(k.get('shape'), 1.06))
        r0 = (kr + 0.08) * PX; r1 = r0 + tick_len * PX
        mode = k.get('dial', dial)
        if arc:
            d.arc((cx - r0, cy - r0, cx + r0, cy + r0), 135, 405, fill=col, width=2)
        n = 11
        for i in range(n):
            a = math.radians(225 - 270 * i / (n - 1)); ca, sa = math.cos(a), -math.sin(a)
            if mode in ('lines', 'numbers', 'numbers1', 'roman'):
                major = mode != 'lines' or i in (0, 5, 10)
                L = r1 if major else r0 + (r1 - r0) * 0.6
                if mode == 'lines' or i % 1 == 0:
                    d.line([(cx + ca * r0, cy + sa * r0), (cx + ca * L, cy + sa * L)], fill=col, width=3 if major else 2)
            if mode == 'dots':
                rr = 4 if i in (0, 5, 10) else 2.5; q = r0 + (r1 - r0) * 0.5
                d.ellipse((cx + ca * q - rr, cy + sa * q - rr, cx + ca * q + rr, cy + sa * q + rr), fill=col)
            if mode in ('numbers', 'numbers1', 'roman'):
                if mode == 'roman' and i % 2 == 1 and kr < 0.5:
                    continue
                t = ROMAN[i] if mode == 'roman' else str(i if mode == 'numbers' else i + 1 if i < 10 else 10)
                if mode == 'numbers1' and i == 0:
                    t = '1'
                if mode == 'numbers1':
                    t = str(1 + round(i * 9 / 10)) if i not in (0, 10) else ('1' if i == 0 else '10')
                q = r1 + (numsize or size * 0.62) * 0.75
                d.text((cx + ca * q, cy + sa * q), t, font=nf, fill=col, anchor='mm')
        if k.get('label'):
            off = r1 + (size * 1.55 if mode in ('numbers', 'numbers1', 'roman') else size * 0.85)
            spaced(d, (cx, cy - off), k['label'], f, col, spacing)
    fs = font(face, int(size * 0.62))
    for (x, z, lab) in toggles:
        cx, cy = P(x, z)
        if lab:
            spaced(d, (cx, cy + 0.5 * PX), lab, fs, col, 2)
    for (x, z, lab) in jewels:
        cx, cy = P(x, z)
        if lab:
            spaced(d, (cx, cy + 0.55 * PX), lab, fs, col, 2)
    for ln in lines:
        d.line([P(ln[0], ln[1]), P(ln[2], ln[3])], fill=col, width=ln[4])
    if extra:
        extra(d, P, col)
    im.save(path)
