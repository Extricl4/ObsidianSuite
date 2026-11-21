"""Heightmaps for the five flagship amps. python3 -I tools/art_flag.py <name>"""
import math, sys, numpy as np
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import relief_art as ra
from relief_art import Relief, arch_pts
from scipy import ndimage as ndi
from PIL import Image, ImageDraw, ImageFont

FD = __file__.rsplit('/', 2)[0] + '/render/fonts/'
DJB = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'


def text_mask(r, text, font, cx, cy, size_in, spacing=0.0, stretch=1.0):
    f = ImageFont.truetype(FD + font, max(8, int(size_in * ra.PX)))
    im = Image.new('L', (r.w, r.h), 0); d = ImageDraw.Draw(im)
    x, y = r.px(cx, cy)
    if spacing:
        tot = sum(d.textlength(c, font=f) for c in text) + spacing * ra.PX * (len(text) - 1); xx = x - tot / 2
        for c in text:
            d.text((xx, y), c, font=f, fill=255, anchor='lm'); xx += d.textlength(c, font=f) + spacing * ra.PX
    else:
        d.text((x, y), text, font=f, fill=255, anchor='mm')
    if stretch != 1.0:
        a = np.array(im); h = a.shape[0]; yc = int(y)
        rows = np.clip(((np.arange(h) - yc) / stretch + yc).astype(int), 0, h - 1)
        im = Image.fromarray(a[rows])
    return np.array(im) > 127


def leather(r, amp=0.035, scale=1.0, seed=1):
    rng = np.random.default_rng(seed)
    n = (ndi.gaussian_filter(rng.standard_normal(r.H.shape), 1.1 * scale) * 1.0 + ndi.gaussian_filter(rng.standard_normal(r.H.shape), 2.6 * scale) * 1.6
         + ndi.gaussian_filter(rng.standard_normal(r.H.shape), 7 * scale) * 1.2)
    n = n / (np.abs(n).max() + 1e-6)
    return amp * n


# ------------------------------------------------------------- ABADDON: nameplate for the bar grille
def abaddon():
    ra.PX = 150
    r = Relief(8.2, 2.7, base=0.0)
    # clipped-corner plate with a raised rim and rivets
    c = 0.35
    poly = [(-4.1 + c, 1.35), (4.1 - c, 1.35), (4.1, 1.35 - c), (4.1, -1.35 + c), (4.1 - c, -1.35), (-4.1 + c, -1.35), (-4.1, -1.35 + c), (-4.1, 1.35 - c)]
    plate = r.poly(poly)
    r.put(r.dome(plate, 0.42, 0.0, 0.25, reach=0.12), plate)
    inner = ndi.binary_erosion(plate, iterations=int(0.16 * ra.PX))
    rim = plate & ~inner
    r.put(r.dome(rim, 0.55, 0.3, 0.5, reach=0.08), rim)
    r.H[inner] = 0.36
    r.H[inner] += leather(r, 0.012, 0.8, 4)[inner]
    for (x, y) in ((-3.75, 1.0), (3.75, 1.0), (-3.75, -1.0), (3.75, -1.0)):
        b = (r.X - x) ** 2 + (r.Y - y) ** 2 <= 0.09 ** 2; r.put(r.dome(b, 0.62, 0.4, 0.6), b)
    # wordmark with a bevel, and flames rising behind it
    for k in range(9):
        x = -3.2 + k * 0.8
        fl = [(x - 0.22, -0.95), (x + 0.22, -0.95), (x + 0.1 + 0.08 * math.sin(k), 0.25 + 0.35 * abs(math.sin(k * 1.7))), (x - 0.02, 0.85 + 0.25 * abs(math.cos(k))), (x - 0.12, 0.2)]
        m = r.poly(fl) & inner
        r.put(r.dome(m, 0.48, 0.36, 0.5), m)
    m = text_mask(r, 'ABADDON', 'PirataOne-Regular.ttf', 0, -0.05, 1.45, spacing=0.04)
    r.put(r.dome(m, 1.0, 0.5, 0.45, reach=0.05), m)
    edge = m & ~ndi.binary_erosion(m, iterations=3); r.H[edge] -= 0.06
    r.save('art/abaddon_plate_h.png', blur=0.7)


# ------------------------------------------------------------- NIHIL: whole leather front, embossed mark, control window
def nihil():
    ra.PX = 130
    W, H = 22.56, 9.1          # zone z 0.7 .. 9.8
    r = Relief(W, H, base=0.3)
    r.H += leather(r, 0.03, 1.0, 7)
    # piping seam across the front, a little above the window
    y_seam = 1.05
    seam = np.abs(r.Y - y_seam) < 0.05
    r.H[seam] = 0.3 + 0.12 * np.sqrt(np.clip(1 - ((r.Y[seam] - y_seam) / 0.05) ** 2, 0, 1))
    g2 = (np.abs(r.Y - (y_seam + 0.07)) < 0.012) | (np.abs(r.Y - (y_seam - 0.07)) < 0.012); r.H[g2] -= 0.06
    for yy in (y_seam + 0.14, y_seam - 0.14):      # saddle stitching either side of the piping
        st = (np.abs(r.Y - yy) < 0.014) & (((r.X + 20) % 0.16) < 0.09); r.H[st] += 0.025
    # tonal embossed wordmark (black on black)
    m = text_mask(r, 'NIHIL', 'NewRocker-Regular.ttf', 0, 2.75, 1.75, spacing=0.18)
    r.put(r.dome(m, 0.62, 0.3, 0.45, reach=0.06), m)
    edge = m & ~ndi.binary_erosion(m, iterations=2); r.H[edge] -= 0.04
    # thin spike under the mark
    sp = r.poly([(-2.2, 1.95), (2.2, 1.95), (0, 1.78)]); r.put(r.dome(sp, 0.42, 0.3, 0.5), sp)
    # control window with rolled leather edge
    x0, x1, y0, y1, rad = -7.4, 7.4, -4.1, -1.6, 0.3
    win = ((np.abs(r.X) <= x1 - rad) & (r.Y >= y0) & (r.Y <= y1)) | ((np.abs(r.X) <= x1) & (r.Y >= y0 + rad) & (r.Y <= y1 - rad))
    for cx_ in (x0 + rad, x1 - rad):
        for cy_ in (y0 + rad, y1 - rad):
            win |= (r.X - cx_) ** 2 + (r.Y - cy_) ** 2 <= rad * rad
    d_out = ndi.distance_transform_edt(~win) / ra.PX
    lip = (d_out > 0) & (d_out < 0.18)
    r.H[lip] = np.maximum(r.H[lip], 0.3 + 0.16 * np.sin(np.clip(d_out[lip] / 0.18, 0, 1) * math.pi))
    r.H[win] = 0.0
    r.holes = win
    # corner piping
    dd = np.minimum.reduce([r.X + W / 2, W / 2 - r.X, r.Y + H / 2, H / 2 - r.Y])
    pip = dd < 0.09; r.H[pip] = np.maximum(r.H[pip], 0.42 * np.sin(np.clip(dd[pip] / 0.09, 0, 1) * math.pi / 2 + 0.6))
    r.save('art/nihil_h.png', 'art/nihil_holes.png', blur=0.6)


# ------------------------------------------------------------- MORTIS: catacomb wall of skulls, glowing eye sockets
def skull(r, cx, cy, s, top=0.85, base=0.15, rot=0.0):
    """front-view skull, returns eye-socket mask (cut through)"""
    cr = ((r.X - cx) / (0.5 * s)) ** 2 + ((r.Y - cy - 0.12 * s) / (0.52 * s)) ** 2 <= 1
    jaw = r.poly([(cx - 0.36 * s, cy - 0.12 * s), (cx + 0.36 * s, cy - 0.12 * s), (cx + 0.26 * s, cy - 0.62 * s), (cx - 0.26 * s, cy - 0.62 * s)])
    cheeks = (((r.X - cx) / (0.46 * s)) ** 2 + ((r.Y - cy + 0.08 * s) / (0.28 * s)) ** 2 <= 1)
    head = cr | jaw | cheeks
    lay = r.dome(head, top, base, 0.45)
    r.put(lay, head)
    eyes = np.zeros_like(r.holes)
    for sx in (-1, 1):
        e = ((r.X - cx - sx * 0.19 * s) / (0.15 * s)) ** 2 + ((r.Y - cy - 0.02 * s) / (0.13 * s)) ** 2 <= 1
        brow = ((r.X - cx - sx * 0.19 * s) / (0.2 * s)) ** 2 + ((r.Y - cy - 0.06 * s) / (0.18 * s)) ** 2 <= 1
        r.H[brow & ~e] -= 0.12 * (top - base)
        eyes |= e
    nose = r.poly([(cx, cy - 0.12 * s), (cx - 0.07 * s, cy - 0.3 * s), (cx + 0.07 * s, cy - 0.3 * s)])
    r.H[nose] = base + 0.15
    for k in range(8):
        tx = cx - 0.21 * s + k * 0.06 * s
        t = (np.abs(r.X - tx) < 0.024 * s) & (r.Y < cy - 0.4 * s) & (r.Y > cy - 0.55 * s)
        r.H[t] += 0.04
        g = (np.abs(r.X - (tx + 0.03 * s)) < 0.006 * s) & (r.Y < cy - 0.38 * s) & (r.Y > cy - 0.57 * s); r.H[g] -= 0.06
    mouth = (np.abs(r.Y - (cy - 0.475 * s)) < 0.008 * s) & (np.abs(r.X - cx) < 0.24 * s); r.H[mouth] -= 0.08
    crack = r.stroke([(cx + 0.1 * s, cy + 0.6 * s), (cx + 0.16 * s, cy + 0.42 * s), (cx + 0.08 * s, cy + 0.3 * s), (cx + 0.14 * s, cy + 0.18 * s)], 0.012 * s); r.H[crack] -= 0.07
    return eyes


def mortis():
    ra.PX = 120
    W, H = 22.56, 5.55
    r = Relief(W, H, base=0.08)
    rng = np.random.default_rng(13)
    r.H += ndi.gaussian_filter(rng.standard_normal(r.H.shape), 3) * 0.02
    eyes = np.zeros_like(r.holes)
    # rows of skulls packed in the wall (smaller toward the edges), crossed femurs between rows
    s = 0.92
    for row, y in enumerate((1.75, 0.0, -1.75)):
        off = 0.5 if row == 1 else 0.0
        x = -10.6 + off
        while x <= 10.7:
            if abs(x) > 3.4:
                eyes |= skull(r, x, y, s * (0.95 + 0.08 * rng.random()), top=0.7 + 0.1 * rng.random())
            x += 1.0
    for y in (0.88, -0.88):
        for x in np.arange(-10.1, 10.2, 1.0):
            if abs(x) > 3.6:
                for d in (-1, 1):
                    m = r.stroke([(x - 0.42, y - 0.12 * d), (x + 0.42, y + 0.12 * d)], 0.07); r.put(r.dome(m, 0.55, 0.1, 0.5, reach=0.035), m)
                    for e in (-1, 1):
                        b = (r.X - (x + e * 0.44)) ** 2 + (r.Y - (y + e * 0.12 * d)) ** 2 <= 0.06 ** 2; r.put(r.dome(b, 0.58, 0.1, 0.6), b)
    # central horned skull framed by two femur columns
    for sx in (1, -1):
        m = r.stroke([(sx * 3.15, -2.3), (sx * 3.15, 2.3)], 0.22); r.put(r.dome(m, 0.7, 0.1, 0.5, reach=0.11), m)
        for yy in (-2.35, 2.35):
            for d in (-1, 1):
                b = (r.X - (sx * 3.15 + d * 0.13)) ** 2 + (r.Y - yy) ** 2 <= 0.15 ** 2; r.put(r.dome(b, 0.78, 0.2, 0.6), b)
    S2 = 2.3
    for sx in (1, -1):    # ram horns: out from the temples, up, then curling down and back in
        c0 = (sx * 1.55, 0.55)
        th = np.linspace(0, 1.75 * 2 * math.pi * 0.62, 260)
        pts = [(c0[0] + sx * math.cos(math.pi - t) * 0.0 + sx * (0.95 * math.exp(-0.16 * t)) * math.cos(t * 1.0 + math.pi) * -1,
                c0[1] + 0.95 * math.exp(-0.16 * t) * math.sin(t * 1.0 + math.pi) * -1) for t in th]
        pts = [(sx * 0.62, 0.95)] + pts
        hm = r.stroke(pts, 0.44, taper=(0.44, 0.07)); r.put(r.dome(hm, 0.9, 0.1, 0.5, reach=0.22), hm)
        for k in range(2, len(pts) - 6, 8):
            px_, py_ = pts[k]; qx, qy = pts[k + 1]
            nx, ny = -(qy - py_), (qx - px_); L = math.hypot(nx, ny) + 1e-9
            wv = 0.44 - 0.37 * k / len(pts)
            r.groove(r.stroke([(px_ - nx / L * wv / 2, py_ - ny / L * wv / 2), (px_ + nx / L * wv / 2, py_ + ny / L * wv / 2)], 0.018), 0.07, 0.6)
    eyes |= skull(r, 0, 0.1, S2, top=1.0, base=0.2)
    r.holes = eyes
    r.save('art/mortis_h.png', 'art/mortis_holes.png', blur=0.7)
    # toxic green glow behind the sockets
    img = np.zeros(r.H.shape + (3,)); g = np.clip(1 - np.abs(r.Y) / 3.2, 0.3, 1)
    img[..., 0] = 0.25 * g; img[..., 1] = 1.0 * g; img[..., 2] = 0.2 * g
    Image.fromarray((img * 255).astype(np.uint8)).save('art/mortis_glass.png')


# ------------------------------------------------------------- NIFLHEIM: frozen black-metal thorns over ice shards
def thorn_branch(r, x, y, ang, L, w, depth, rng, top, mirror=True):
    pts = [(x + math.cos(ang) * L * t + 0.06 * math.sin(t * 9) * L * 0.2, y + math.sin(ang) * L * t) for t in np.linspace(0, 1, 30)]
    for sgn in ((1, -1) if mirror else (1,)):
        q = [(sgn * px_, py_) for px_, py_ in pts]
        m = r.stroke(q, w, taper=(w, 0.012)); r.put(r.dome(m, top, 0.12, 0.5, reach=w / 2), m)
    if depth <= 0:
        return
    for k in range(rng.integers(2, 4)):
        t = rng.uniform(0.3, 0.85)
        bx, by = x + math.cos(ang) * L * t, y + math.sin(ang) * L * t
        na = ang + rng.choice([-1, 1]) * rng.uniform(0.6, 1.2)
        thorn_branch(r, bx, by, na, L * rng.uniform(0.35, 0.55), w * 0.6, depth - 1, rng, top * 0.97, mirror)


def niflheim():
    ra.PX = 120
    W, H = 22.56, 5.55
    r = Relief(W, H, base=0.2)
    rng = np.random.default_rng(66)
    r.frame(fw=0.26, top=0.5, beads=False)
    # shattered-ice openings
    holes = np.zeros_like(r.holes)
    pts = rng.uniform([-10.4, -2.3], [10.4, 2.3], size=(48, 2))
    yy, xx = r.Y, r.X
    dmin = np.full(r.H.shape, 1e9); dsec = np.full(r.H.shape, 1e9)
    for (px_, py_) in pts:
        d = (xx - px_) ** 2 + ((yy - py_) * 1.4) ** 2
        dsec = np.minimum(dsec, np.maximum(dmin, d)); dmin = np.minimum(dmin, d)
    edge = np.sqrt(dsec) - np.sqrt(dmin)
    shards = (edge > 0.17) & (np.abs(r.X) < 10.55) & (np.abs(r.Y) < 2.45)
    shards &= ndi.binary_erosion(shards, iterations=2)
    holes |= shards
    ribs = ~shards & (np.abs(r.X) < 10.6) & (np.abs(r.Y) < 2.5)
    r.H[ribs] = 0.32 + 0.1 * np.clip(1 - edge[ribs] / 0.17, 0, 1)
    # icicle fangs hanging from the top and rising from the bottom
    for x in np.arange(-10.3, 10.31, 0.42):
        L = rng.uniform(0.35, 0.95)
        f = r.poly([(x - 0.12, 2.48), (x + 0.12, 2.48), (x + 0.01, 2.48 - L)]); r.put(r.dome(f, 0.62, 0.3, 0.5), f); holes &= ~f
        L = rng.uniform(0.2, 0.55)
        f = r.poly([(x + 0.09, -2.48), (x + 0.33, -2.48), (x + 0.22, -2.48 + L)]); r.put(r.dome(f, 0.58, 0.3, 0.5), f); holes &= ~f
    # symmetric thorned emblem (no lettering), black-metal style
    core = r.poly([(-0.16, -1.9), (0.16, -1.9), (0.1, 1.9), (0, 2.25), (-0.1, 1.9)]); r.put(r.dome(core, 1.0, 0.3, 0.5), core); holes &= ~core
    bar = r.poly([(-6.4, 0.06)] + [(sx_ * x_, 0.06 + (0.2 if k_ % 2 == 0 else 0.1) * (1 - x_ / 6.4)) for sx_ in (-1,) for k_, x_ in enumerate(np.linspace(6.2, 0.2, 14))] + [(0, 0.3)] + [(x_, 0.06 + (0.2 if k_ % 2 == 0 else 0.1) * (1 - x_ / 6.4)) for k_, x_ in enumerate(np.linspace(0.2, 6.2, 14))] + [(6.4, 0.06), (0.2, -0.14), (0, -0.22), (-0.2, -0.14)]); r.put(r.dome(bar, 0.95, 0.3, 0.5), bar); holes &= ~bar
    before = r.H.copy()
    for k in range(7):
        x = 0.5 + k * 0.85; y = 0.06 - k * 0.012
        thorn_branch(r, x, y, math.radians(rng.uniform(55, 110)), rng.uniform(1.1, 2.0), 0.13, 2, rng, 0.92)
        thorn_branch(r, x, y, math.radians(-rng.uniform(55, 110)), rng.uniform(0.9, 1.8), 0.12, 2, rng, 0.92)
    grown = r.H > before + 1e-4
    holes &= ~ndi.binary_dilation(grown, iterations=2)
    inv = r.poly([(-0.55, 0.55), (0.55, 0.55), (0, -0.55)]); r.put(r.dome(inv, 0.85, 0.3, 0.5), inv); holes &= ~inv
    holes &= ~ndi.binary_dilation(core | bar, iterations=2)
    r.holes = holes
    r.save('art/niflheim_h.png', 'art/niflheim_holes.png', blur=0.6)
    img = np.zeros(r.H.shape + (3,)); n = ndi.gaussian_filter(rng.standard_normal(r.H.shape), 18); n = (n - n.min()) / (n.max() - n.min())
    img[..., 0] = 0.25 + 0.3 * n; img[..., 1] = 0.6 + 0.35 * n; img[..., 2] = 1.0
    Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8)).save('art/niflheim_glass.png')


# ------------------------------------------------------------- MOROS: dark burl-stone panels + geometric badge
def moros():
    ra.PX = 130
    W, H = 22.56, 5.55
    r = Relief(W, H, base=0.25)
    rng = np.random.default_rng(21)
    # pressed burl/stone chips: many overlapping irregular flakes at slightly different heights
    lay = np.zeros(r.H.shape)
    for _ in range(2600):
        cx, cy = rng.uniform(-11.5, 11.5), rng.uniform(-3, 3)
        a, b = rng.uniform(0.15, 0.5), rng.uniform(0.08, 0.25); ang = rng.uniform(0, math.pi)
        x0, x1 = int((cx - 0.6 + W / 2) * ra.PX), int((cx + 0.6 + W / 2) * ra.PX); y0, y1 = int((H / 2 - cy - 0.6) * ra.PX), int((H / 2 - cy + 0.6) * ra.PX)
        x0, y0 = max(x0, 0), max(y0, 0); x1, y1 = min(x1, r.w), min(y1, r.h)
        if x1 <= x0 or y1 <= y0:
            continue
        X = r.X[y0:y1, x0:x1] - cx; Y = r.Y[y0:y1, x0:x1] - cy
        u = X * math.cos(ang) + Y * math.sin(ang); v = -X * math.sin(ang) + Y * math.cos(ang)
        sel = (u / a) ** 2 + (v / b) ** 2 <= 1
        lay[y0:y1, x0:x1][sel] = rng.uniform(0, 1)
    lay = ndi.gaussian_filter(lay, 0.8)
    r.H = 0.25 + 0.05 * lay + leather(r, 0.008, 0.7, 3)
    # two panels split by a fine seam, with a border
    seam = np.abs(r.Y + 0.15) < 0.025; r.H[seam] = 0.12
    dd = np.minimum.reduce([r.X + W / 2, W / 2 - r.X, r.Y + H / 2, H / 2 - r.Y]); r.H[dd < 0.06] = 0.16
    for (x, y) in ((-10.75, 2.35), (10.75, 2.35), (-10.75, -2.35), (10.75, -2.35)):
        b = (r.X - x) ** 2 + (r.Y - y) ** 2 <= 0.11 ** 2; r.put(r.dome(b, 0.5, 0.3, 0.6), b)
        sl = b & (np.abs(r.Y - y) < 0.015); r.H[sl] -= 0.1
    r.save('art/moros_h.png', blur=0.6)
    # badge: rounded-rect frame with an hourglass/void glyph
    ra.PX = 160
    b = Relief(2.4, 2.4, base=0.0)
    rr = 0.3
    box = ((np.abs(b.X) <= 1.18 - rr) & (np.abs(b.Y) <= 1.18)) | ((np.abs(b.X) <= 1.18) & (np.abs(b.Y) <= 1.18 - rr))
    for sx in (-1, 1):
        for sy in (-1, 1):
            box |= (b.X - sx * (1.18 - rr)) ** 2 + (b.Y - sy * (1.18 - rr)) ** 2 <= rr * rr
    inner = ndi.binary_erosion(box, iterations=int(0.16 * ra.PX))
    b.put(b.dome(box & ~inner, 0.9, 0.2, 0.5, reach=0.08), box & ~inner)
    b.H[inner] = 0.3
    for sy in (1, -1):
        t = b.poly([(-0.62, sy * 0.78), (0.62, sy * 0.78), (0.0, sy * 0.04)])
        tin = ndi.binary_erosion(t, iterations=int(0.1 * ra.PX))
        b.put(b.dome(t & ~tin, 1.0, 0.4, 0.5, reach=0.05), t & ~tin)
        b.H[tin] = 0.42
    for sx in (-1, 1):
        bar = (np.abs(b.X - sx * 0.82) < 0.06) & (np.abs(b.Y) < 0.82); b.put(b.dome(bar, 0.9, 0.4, 0.5, reach=0.03), bar)
    b.save('art/moros_badge_h.png', blur=0.6)


if __name__ == '__main__':
    globals()[sys.argv[1]]()
    print('saved', sys.argv[1])
