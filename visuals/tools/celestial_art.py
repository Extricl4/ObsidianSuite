"""Motifs and layout for the celestial pedals (Elysium, Empyrean, Nimbus, Vesper, Halo, Astral,
Mnemosyne, Gemini, Lyra, Icarus). Builds on pedal_art_lib (3x supersampled Plate, tapered strokes).

Each art_<key>.py describes its pedal once (knobs with labels, switches, LED, footswitch) and this
module writes both the spec (pedals/<key>.json) and the art, so labels always sit under their knobs.
"""
import json, math, os, random
from pedal_art_lib import *          # Plate, bez, taper helpers, leaf, star, A (fonts), W, H, CLEAR, SS
from PIL import Image, ImageDraw, ImageFilter

FD = os.path.join(os.path.dirname(__file__), '..', 'render', 'fonts') + '/'
A.F.update({'marc': FD + 'MarcellusSC-Regular.ttf', 'corm': FD + 'CormorantSC-SemiBold.ttf',
            'play': FD + 'PlayfairDisplaySC-Bold.ttf', 'fell': FD + 'IMFeENsc28P.ttf', 'cardo': FD + 'Cardo.ttf'})
ROOT = os.path.join(os.path.dirname(__file__), '..')
PX = 200                              # art pixels per cm


# ----------------------------------------------------------------------------- small geometry
def pol(cx, cy, r, a):                # a in degrees, 0 = right, 90 = down
    return (cx + r * math.cos(math.radians(a)), cy + r * math.sin(math.radians(a)))

def arc(cx, cy, r, a0, a1, n=60, ry=None):
    ry = ry or r
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)), cy + ry * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]

def rotate(pts, cx, cy, deg):
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    return [(cx + (x - cx) * c - (y - cy) * s, cy + (x - cx) * s + (y - cy) * c) for x, y in pts]


# ----------------------------------------------------------------------------- frame + controls
def frame(P, ink, w=None, h=None, inset=54, r=46, corners=True):
    w = w or P.w; h = h or P.h
    P.rrect((inset, inset, w - inset, h - inset), r, ink, 4)
    P.rrect((inset + 16, inset + 16, w - inset - 16, h - inset - 16), r - 12, ink, 1.4)
    if corners:
        for (x, y) in ((inset + 34, inset + 34), (w - inset - 34, inset + 34), (inset + 34, h - inset - 34), (w - inset - 34, h - inset - 34)):
            star(P.d, x * SS, y * SS, 13 * SS, ink, pts=4, inner=0.28)

def knob_scale(P, x, y, r, ink, ticks=11, big=(0, 5, 10), w=2.6):
    rr = r * PX
    for k in range(ticks):
        a = 135 + 270 * k / (ticks - 1)
        L = 22 if k in big else 12
        P.line([pol(x, y, rr + 18, a), pol(x, y, rr + 18 + L, a)], ink, w if k in big else w * 0.7)

def selector_labels(P, x, y, r, labels, ink, font='marc', size=22, span=270, start=135, gap=34):
    rr = r * PX
    n = len(labels)
    for i, t in enumerate(labels):
        a = start + span * i / max(1, n - 1)
        P.line([pol(x, y, rr + 10, a), pol(x, y, rr + 24, a)], ink, 2.4)
        P.text(pol(x, y, rr + 24 + gap, a), t, font, size, ink)

def label(P, x, y, text, ink, size=34, font='marc', spacing=7):
    P.text((x, y), text, font, size, ink, spacing=spacing)

def controls(P, spec, ink, labels, font='marc', size=34, small_size=26, scales=True):
    """Labels (and tick scales) under every knob; labels = list aligned with spec['knobs'] (None = no label)."""
    for k, t in zip(spec['knobs'], labels):
        x, y = k['at']; r = k['r']
        if scales and not k.get('selector'): knob_scale(P, x, y, r, ink)
        if t: label(P, x, y + r * PX + (112 if r > 0.5 else 86), t, ink, size if r > 0.5 else small_size, font, spacing=7 if r > 0.5 else 5)


# ----------------------------------------------------------------------------- motifs
def feather(P, base, tip, w, ink, bend=0.12, barbs=12, lw=1.8, erase=True, side_bias=0.0):
    """An engraved feather: outline, shaft and barbs. erase=True hides what's behind it."""
    (x0, y0), (x1, y1) = base, tip
    dx, dy = x1 - x0, y1 - y0; L = math.hypot(dx, dy) or 1; nx, ny = -dy / L, dx / L
    shaft = bez(base, (x0 + dx * .33 + nx * L * bend, y0 + dy * .33 + ny * L * bend), (x0 + dx * .7 + nx * L * bend * .8, y0 + dy * .7 + ny * L * bend * .8), tip, 30)
    def edge(side):
        pts = []
        for i, (sx, sy) in enumerate(shaft):
            t = i / (len(shaft) - 1)
            wid = w * (math.sin(math.pi * min(1, t * 1.15)) ** 0.6) * (1 + side * side_bias) * (0.55 if t < .12 else 1)
            j = min(len(shaft) - 1, i + 1); k = max(0, i - 1)
            ex, ey = shaft[j][0] - shaft[k][0], shaft[j][1] - shaft[k][1]; el = math.hypot(ex, ey) or 1
            pts.append((sx - ey / el * wid * side, sy + ex / el * wid * side))
        return pts
    left, right = edge(1), edge(-1)
    outline = left + right[::-1]
    if erase: P.poly(outline, CLEAR)
    P.line(left, ink, lw); P.line(right, ink, lw)
    P.taper(shaft, ink, lw * 1.9, lw * 0.5)
    for i in range(3, len(shaft) - 2, max(1, len(shaft) // barbs)):
        for e in (left, right):
            j = min(len(e) - 1, i + 3)
            P.line([shaft[i], e[j]], ink, lw * 0.55)

def wing(P, root, flip, S, ink, lw=1.8, up=1.0):
    """Angel wing growing out of `root`, sweeping to the side (flip = +1 right, -1 left). S = span in px."""
    rx, ry = root
    arm = bez((rx, ry), (rx + flip * S * .35, ry - S * .42 * up), (rx + flip * S * .75, ry - S * .48 * up), (rx + flip * S, ry - S * .22 * up), 40)
    # primaries (long, outer), drawn first so the shorter rows cover their roots
    rows = [(8, .3, 1.0, 0.8, 34), (7, .18, .62, 0.55, 22), (6, .06, .36, 0.32, 14)]
    for n, t0, lmul, wmul, ang_spread in rows:
        for i in range(n):
            t = t0 + (1 - t0) * i / (n - 1)
            bx, by = arm[int(t * (len(arm) - 1))]
            L = S * lmul * (0.38 + 0.62 * t)
            a = 90 - flip * (8 + ang_spread * t)                  # hang down, splaying outward toward the tip
            tip = (bx + L * math.cos(math.radians(a)), by + L * math.sin(math.radians(a)))
            feather(P, (bx, by), tip, S * 0.05 * wmul * (1.4 if lmul < 0.5 else 1), ink, bend=-flip * 0.06, barbs=10, lw=lw)
    # the leading edge (the "arm") on top
    P.taper(arm, ink, lw * 3.2, lw * 1.2)
    for i in range(4, len(arm) - 2, 3):            # little scallops: the coverts along the arm
        bx, by = arm[i]
        P.line(arc(bx, by + 10, 12, 20, 160, 10), ink, lw * 0.8)

def rays(P, cx, cy, r0, r1, n, ink, w0=5, w1=0.8, alt=0.7, phase=0):
    for k in range(n):
        a = phase + 360 * k / n
        L = r1 if k % 2 == 0 else r0 + (r1 - r0) * alt
        P.taper([pol(cx, cy, r0 + (L - r0) * t, a) for t in (i / 10 for i in range(11))], ink, w0, w1)

def pointed_arch(x, y_base, w, h, n=24, jamb_frac=0.42):
    """Gothic pointed arch from the bottom-left corner round to the bottom-right: straight jambs, then two
    arcs (each centred on the opposite side) meeting at the apex, scaled to total height h."""
    r = w * 0.85
    th = math.acos(1 - w / (2 * r))                       # where the two arcs meet
    jamb = h * jamb_frac
    sy = (h - jamb) / (r * math.sin(th))
    cl, cr = x - w / 2 + r, x + w / 2 - r                 # centre of the left arc (on the right) and of the right arc
    pts = [(x - w / 2, y_base), (x - w / 2, y_base - jamb)]
    for i in range(1, n + 1):
        a = math.pi + th * i / n
        pts.append((cl + r * math.cos(a), y_base - jamb + r * math.sin(a) * sy))
    for i in range(1, n + 1):
        a = 2 * math.pi - th + th * i / n
        pts.append((cr + r * math.cos(a), y_base - jamb + r * math.sin(a) * sy))
    pts += [(x + w / 2, y_base)]
    return pts

def lancet(P, x, y_base, w, h, ink, lw=3.5, tracery=True):
    P.line(pointed_arch(x, y_base, w, h), ink, lw)
    P.line(pointed_arch(x, y_base - 6, w - 26, h - 22), ink, lw * 0.45)
    if tracery:
        P.line([(x, y_base - 8), (x, y_base - h * 0.55)], ink, lw * 0.6)
        P.line(pointed_arch(x - w * 0.22, y_base - 8, w * 0.36, h * 0.62), ink, lw * 0.45)
        P.line(pointed_arch(x + w * 0.22, y_base - 8, w * 0.36, h * 0.62), ink, lw * 0.45)
        P.circle(x, y_base - h * 0.72, w * 0.14, ink, lw * 0.5)

def rose_window(P, cx, cy, R, ink, petals=12, lw=3.2):
    P.circle(cx, cy, R, ink, lw * 1.4); P.circle(cx, cy, R - 14, ink, lw * 0.5)
    for k in range(petals * 3):                                   # beaded rim
        P.circle(*pol(cx, cy, R - 30, 360 * k / (petals * 3)), 3.4, ink)
    ri = R * 0.36
    for k in range(petals):
        a = 360 * k / petals - 90
        # a radial lancet petal from the inner ring out to the rim
        w = 2 * math.pi * ri / petals * 0.92
        ux, uy = math.cos(math.radians(a)), math.sin(math.radians(a)); vx, vy = -uy, ux
        pts = [(cx + ux * (ri - py) + vx * px, cy + uy * (ri - py) + vy * px) for px, py in pointed_arch(0, 0, w, (R - 44) - ri, jamb_frac=0.5)]
        P.line(pts, ink, lw * 0.7)
        # inside each petal: a smaller lancet and a cusped oculus (tracery)
        inner = [(cx + ux * (ri + 22 - py) + vx * px, cy + uy * (ri + 22 - py) + vy * px) for px, py in pointed_arch(0, 0, w * 0.5, (R - 44) - ri - 70, jamb_frac=0.55)]
        P.line(inner, ink, lw * 0.35)
        ox, oy = pol(cx, cy, R * 0.79, a)
        P.circle(ox, oy, w * 0.19, ink, lw * 0.45)
        for j in range(4): P.circle(*pol(ox, oy, w * 0.085, a + 45 + 90 * j), w * 0.075, ink, lw * 0.3)
        # mullion between petals, ending in a small rosette at the rim
        mb = a + 180 / petals
        P.line([pol(cx, cy, ri + 4, mb), pol(cx, cy, R - 44, mb)], ink, lw * 0.4)
        P.circle(*pol(cx, cy, R - 54, mb), 7, ink, lw * 0.4)
    P.circle(cx, cy, ri, ink, lw); P.circle(cx, cy, ri - 12, ink, lw * 0.45)
    for k in range(6):                                            # trefoils round the centre
        a = 60 * k - 90
        for j in (-1, 0, 1):
            P.circle(*pol(cx, cy, ri * 0.62 + (8 if j == 0 else 0), a + j * 13), ri * 0.13, ink, lw * 0.4)

def cloud(P, cx, cy, w, ink, lw=2.6, seed=1, bumps=5, erase=True, shade=True):
    """Stylised engraved cloud: scalloped top, gently curved base, inner contour lines."""
    rnd = random.Random(seed)
    xs = [cx - w / 2 + w * i / bumps for i in range(bumps + 1)]
    pts = []
    for i in range(bumps):
        a, b = xs[i], xs[i + 1]; mid = (a + b) / 2
        rr = (b - a) / 2 * rnd.uniform(1.0, 1.35)
        hump = 1.0 - abs((mid - cx) / (w / 2)) ** 1.6
        top = cy - w * 0.08 - w * 0.13 * hump
        pts += arc(mid, top + rr * 0.55, rr, 180, 360, 20, ry=rr * 0.9)
    base = bez((cx + w / 2, pts[-1][1]), (cx + w * .45, cy + w * .1), (cx - w * .45, cy + w * .1), (cx - w / 2, pts[0][1]), 30)
    outline = pts + base
    if erase: P.poly(outline, CLEAR)
    P.line(pts, ink, lw); P.line(base, ink, lw * 0.8)
    if shade:                                                      # inner contours following the bumps
        for i in range(bumps):
            a, b = xs[i], xs[i + 1]; mid = (a + b) / 2; rr = (b - a) / 2 * 0.7
            top = cy - w * 0.02 - w * 0.08 * (1.0 - abs((mid - cx) / (w / 2)) ** 1.6)
            P.line(arc(mid, top + rr * 0.6, rr, 200, 340, 14, ry=rr * 0.8), ink, lw * 0.45)
        for j in range(4):                                         # base hatching
            y = cy + w * (0.03 + 0.02 * j); half = w * (0.36 - 0.06 * j)
            P.line([(cx - half, y), (cx + half, y)], ink, lw * 0.35)

def crescent(P, cx, cy, r, ink, phase=0.42, rot=-30, lw=2.6):
    P.circle(cx, cy, r, ink)
    ox, oy = pol(0, 0, r * phase * 2, rot)
    P.circle(cx + ox, cy + oy, r * 0.98, CLEAR)
    P.line(arc(cx, cy, r + 10, rot + 70, rot + 290, 40), ink, lw * 0.5)

def astrolabe(P, cx, cy, R, ink, lw=3.0):
    P.circle(cx, cy, R, ink, lw * 1.4); P.circle(cx, cy, R - 12, ink, lw * 0.5); P.circle(cx, cy, R - 52, ink, lw * 0.7)
    for k in range(72):                                           # degree scale
        a = k * 5
        L = 30 if k % 6 == 0 else 16
        P.line([pol(cx, cy, R - 14, a), pol(cx, cy, R - 14 - L, a)], ink, lw * (0.8 if k % 6 == 0 else 0.45))
    for k in range(12):                                           # zodiac sectors
        a = k * 30
        P.circle(*pol(cx, cy, R - 34, a + 15), 3.2, ink)
    # ecliptic (offset ring) + tropics + horizon/almucantar arcs
    P.circle(cx, cy - R * 0.18, R * 0.55, ink, lw * 0.6)
    P.circle(cx, cy, R * 0.28, ink, lw * 0.5)
    for k in range(1, 5):
        P.line(arc(cx, cy + R * 0.25, R * (0.3 + 0.13 * k), 200, 340, 40), ink, lw * 0.35)
    P.line([(cx - R + 52, cy), (cx + R - 52, cy)], ink, lw * 0.5); P.line([(cx, cy - R + 52), (cx, cy + R - 52)], ink, lw * 0.5)
    # rete: star pointers (flames)
    for k, (a, rr) in enumerate([(20, .72), (75, .62), (130, .8), (200, .68), (250, .58), (310, .76)]):
        bx, by = pol(cx, cy, R * (rr - 0.12), a); tx, ty = pol(cx, cy, R * rr, a)
        P.taper([(bx, by), ((bx + tx) / 2 + 6, (by + ty) / 2), (tx, ty)], ink, 9, 0.5)
        star(P.d, tx * SS, ty * SS, 9 * SS, ink, pts=4, inner=0.3)

def lyre(P, cx, cy, Hh, ink, lw=4.0, strings=7):
    """Classical lyre: two horned arms rising from a rounded sound box, a crossbar, strings."""
    W2 = Hh * 0.42
    base_y = cy + Hh * 0.42
    box = bez((cx - W2 * .55, base_y - Hh * .12), (cx - W2 * .6, base_y + Hh * .1), (cx + W2 * .6, base_y + Hh * .1), (cx + W2 * .55, base_y - Hh * .12), 40)
    P.taper(box, ink, lw * 1.4, lw * 1.4, lw * 2.2)
    P.line([(cx - W2 * .55, base_y - Hh * .12), (cx + W2 * .55, base_y - Hh * .12)], ink, lw * 0.7)
    for s in (-1, 1):
        armpts = bez((cx + s * W2 * .5, base_y - Hh * .12), (cx + s * W2 * 1.15, base_y - Hh * .45), (cx + s * W2 * .25, cy - Hh * .25), (cx + s * W2 * .62, cy - Hh * .5), 50)
        P.taper(armpts, ink, lw * 2.4, lw * 1.2)
        hx, hy = armpts[-1]
        P.taper(volute(hx + s * 18, hy - 4, 34, turns=1.1, flip=-s, rot=math.pi if s > 0 else 0), ink, lw * 1.2, lw * 0.4)
    yb = cy - Hh * .36
    P.taper([(cx - W2 * .72, yb), (cx, yb - 10), (cx + W2 * .72, yb)], ink, lw * 1.3, lw * 1.3, lw * 1.8)
    for i in range(strings):
        x = cx - W2 * .38 + W2 * .76 * i / (strings - 1)
        P.line([(x, yb + 4), (x, base_y - Hh * .12 - 2)], ink, lw * 0.35)
        P.circle(x, yb + 4, 4, ink)

def reel(P, cx, cy, R, ink, lw=3.2):
    P.circle(cx, cy, R, ink, lw * 1.3); P.circle(cx, cy, R - 12, ink, lw * 0.5)
    P.circle(cx, cy, R * 0.22, ink, lw); P.circle(cx, cy, R * 0.09, ink)
    for k in range(3):                                             # three windows, petal-shaped
        a = 120 * k - 90
        pts = arc(cx, cy, R * 0.82, a - 38, a + 38, 30) + arc(cx, cy, R * 0.32, a + 30, a - 30, 20)
        P.line(pts + [pts[0]], ink, lw * 0.8)
        P.line(arc(cx, cy, R * 0.72, a - 30, a + 30, 24), ink, lw * 0.35)
    for k in range(24):
        P.circle(*pol(cx, cy, R - 24, 15 * k), 2.6, ink)

def sun(P, cx, cy, r, ink, lw=3.0, n=24):
    P.circle(cx, cy, r, ink, lw * 1.3); P.circle(cx, cy, r - 12, ink, lw * 0.5)
    for k in range(n):
        a = 360 * k / n
        if k % 2 == 0:
            pts = [pol(cx, cy, r + 16 + 120 * t, a + 6 * math.sin(t * 9)) for t in (i / 30 for i in range(31))]
            P.taper(pts, ink, 7, 0.8)
        else:
            P.taper([pol(cx, cy, r + 16 + 70 * t, a) for t in (i / 10 for i in range(11))], ink, 4.5, 0.6)

def meander(P, y, x0, x1, ink, s=24, w=2.6):
    x = x0
    while x + 2 * s <= x1:
        P.line([(x, y + s), (x, y), (x + s * 1.5, y), (x + s * 1.5, y + s * 0.66), (x + s * 0.6, y + s * 0.66), (x + s * 0.6, y + s * 0.33), (x + s, y + s * 0.33)], ink, w)
        P.line([(x, y + s), (x + 2 * s, y + s)], ink, w); x += 2 * s

def star_field(P, box, n, ink, seed=3, avoid=()):
    rnd = random.Random(seed); x0, y0, x1, y1 = box
    for _ in range(n):
        x, y = rnd.uniform(x0, x1), rnd.uniform(y0, y1)
        if any((x - ax) ** 2 + (y - ay) ** 2 < ar * ar for ax, ay, ar in avoid): continue
        if rnd.random() < 0.18: star(P.d, x * SS, y * SS, rnd.uniform(6, 11) * SS, ink, pts=4, inner=0.25)
        else: P.circle(x, y, rnd.uniform(1.4, 3.0), ink)


# ----------------------------------------------------------------------------- output
def glow_spots(w, h, spots, blur=26):
    """Soft light for the lit face: [(x, y, r, (r,g,b,a))]."""
    gl = Image.new('RGBA', (w, h), (0, 0, 0, 0)); g = ImageDraw.Draw(gl)
    for x, y, r, c in spots: g.ellipse((x - r, y - r, x + r, y + r), fill=c)
    return gl.filter(ImageFilter.GaussianBlur(blur))

def write_spec(key, spec):
    spec = dict(spec)
    spec.setdefault('art', f'art/{key}_art.png'); spec.setdefault('glow', f'art/{key}_glow.png')
    for k in spec.get('knobs', []): k.pop('selector', None)
    with open(os.path.join(ROOT, 'pedals', key + '.json'), 'w') as f: json.dump(spec, f, indent=1)


def std_knobs(style, row2=4, v1=(0.45, 0.65, 0.35), v2=(0.4, 0.5, 0.3, 0.55), big=(0.6, 0.38)):
    """Three big knobs on top, three or four small ones under them (positions in art px)."""
    k = [{'at': [x, 470], 'r': big[0], 'style': style, 'v': v} for x, v in zip((260, 660, 1060), v1)]
    xs = (200, 507, 813, 1120) if row2 == 4 else (260, 660, 1060)
    k += [{'at': [x, 860], 'r': big[1], 'style': style, 'v': v} for x, v in zip(xs, v2)]
    return k

def text_width(P, t, font, size, spacing=0):
    f = A.font(font, int(size * SS))
    return (sum(P.d.textlength(c, font=f) for c in t) + spacing * SS * (len(t) - 1)) / SS

def title(P, name, sub, ink, y=1955, size=112, font='marc', spacing=22, sub_size=25):
    P.text((660, y), name, font, size, ink, spacing=spacing)
    if sub:
        P.text((660, y + 92), sub, 'marc', sub_size, ink, spacing=12)
        tw = text_width(P, sub, 'marc', sub_size, 12)
        for sd in (-1, 1):
            P.line([(660 + sd * (tw / 2 + 24), y + 92), (660 + sd * (tw / 2 + 100), y + 92)], ink, 1.6)
            star(P.d, (660 + sd * (tw / 2 + 116)) * SS, (y + 92) * SS, 8 * SS, ink, pts=4, inner=0.3)
