"""Shared drawing helpers for new pedal art (Graven, Helios, Ossuary, Halcyon)."""
import os, sys, math, random
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'render'))
import art_common as A
FD = os.path.join(os.path.dirname(__file__), '..', 'render', 'fonts') + '/'
A.F = {k: FD + os.path.basename(v) for k, v in A.F.items()}
A.F['cardo'] = FD + 'Cardo.ttf'
from PIL import Image, ImageDraw, ImageFilter
ART = os.path.join(os.path.dirname(__file__), '..', 'art')
W, H = A.W, A.H
CLEAR = (0, 0, 0, 0)

def skull(d, cx, cy, s, ink, hole=CLEAR, flip=1):
    """Front-facing skull, s = width of cranium."""
    r = s/2
    d.ellipse((cx-r, cy-r*1.05, cx+r, cy+r*0.85), fill=ink)
    d.rounded_rectangle((cx-r*0.62, cy+r*0.4, cx+r*0.62, cy+r*1.2), r*0.18, fill=ink)
    for sx in (-1, 1):  # eye sockets
        ex = cx + sx*r*0.38
        d.ellipse((ex-r*0.27, cy-r*0.05, ex+r*0.27, cy+r*0.45), fill=hole)
    d.polygon([(cx, cy+r*0.45), (cx-r*0.12, cy+r*0.68), (cx+r*0.12, cy+r*0.68)], fill=hole)
    for k in range(-3, 4):  # teeth
        x = cx + k*r*0.15
        d.line([(x, cy+r*0.86), (x, cy+r*1.18)], fill=hole, width=max(2, int(r*0.045)))
    d.line([(cx-r*0.55, cy+r*0.98), (cx+r*0.55, cy+r*0.98)], fill=hole, width=max(2, int(r*0.04)))
    # cranium crack
    d.line([(cx+r*0.2, cy-r*1.0), (cx+r*0.05, cy-r*0.7), (cx+r*0.22, cy-r*0.5), (cx+r*0.1, cy-r*0.3)], fill=hole, width=max(2, int(r*0.04)))

def spiral(d, x0, y0, s, ink, flip=1, turns=2.6, width=6, start=0):
    pts = []
    n = 140
    for i in range(n):
        t = start + i/n*turns*2*math.pi; rr = s*(1 - i/(n*1.15))
        pts.append((x0 + flip*rr*math.cos(t), y0 + rr*math.sin(t)))
    d.line(pts, fill=ink, width=width, joint='curve')

def thorn_vine(d, pts, ink, width=6, every=3, size=22, seed=1):
    random.seed(seed)
    d.line(pts, fill=ink, width=width, joint='curve')
    for i in range(1, len(pts)-1, every):
        (x0, y0), (x1, y1) = pts[i-1], pts[i+1]
        ang = math.atan2(y1-y0, x1-x0); side = 1 if (i//every) % 2 else -1
        nx, ny = -math.sin(ang)*side, math.cos(ang)*side
        x, y = pts[i]
        tip = (x + nx*size + math.cos(ang)*size*0.6, y + ny*size + math.sin(ang)*size*0.6)
        d.polygon([(x - math.cos(ang)*8, y - math.sin(ang)*8), (x + math.cos(ang)*8, y + math.sin(ang)*8), tip], fill=ink)

def curve(x0, y0, x1, y1, bend, n=40):
    mx, my = (x0+x1)/2, (y0+y1)/2; dx, dy = x1-x0, y1-y0; L = math.hypot(dx, dy) or 1
    cx, cy = mx - dy/L*bend, my + dx/L*bend
    return [((1-t)**2*x0 + 2*(1-t)*t*cx + t*t*x1, (1-t)**2*y0 + 2*(1-t)*t*cy + t*t*y1) for t in (i/n for i in range(n+1))]

def knot_ring(d, cx, cy, r, ink, width=7, lobes=4):
    """Interlaced looped ring (knotwork feel)."""
    for k in range(lobes):
        a = 2*math.pi*k/lobes
        x, y = cx + r*0.55*math.cos(a), cy + r*0.55*math.sin(a)
        d.ellipse((x-r*0.55, y-r*0.55, x+r*0.55, y+r*0.55), outline=ink, width=width)
    d.ellipse((cx-r, cy-r, cx+r, cy+r), outline=ink, width=width)

def star(d, x, y, r, col, pts=5, inner=0.42, rot=-90):
    P = [(x + (r if i % 2 == 0 else r*inner)*math.cos(math.radians(rot + i*180/pts)), y + (r if i % 2 == 0 else r*inner)*math.sin(math.radians(rot + i*180/pts))) for i in range(pts*2)]
    d.polygon(P, fill=col)

def save(key, im, gl):
    im.save(os.path.join(ART, key + '_art.png')); gl.save(os.path.join(ART, key + '_glow.png')); print(key, 'art ok')

# ---------------------------------------------------------------- v2: supersampled, engraver-style drawing
SS = 3   # draw at 3x, downsample -> clean antialiased lines

class Plate:
    """Draw in pedal pixels (default 1320x2400); stored at SS x and downsampled on save."""
    def __init__(self, w=None, h=None):
        self.w, self.h = w or W, h or H
        self.im = Image.new('RGBA', (self.w*SS, self.h*SS), (0, 0, 0, 0)); self.d = ImageDraw.Draw(self.im)
    def _p(self, pts): return [(x*SS, y*SS) for x, y in pts]
    def line(self, pts, ink, w=4):
        self.d.line(self._p(pts), fill=ink, width=max(1, int(w*SS)), joint='curve')
        for x, y in (pts[0], pts[-1]):   # round caps
            r = w*SS/2; self.d.ellipse((x*SS-r, y*SS-r, x*SS+r, y*SS+r), fill=ink)
    def poly(self, pts, ink): self.d.polygon(self._p(pts), fill=ink)
    def circle(self, cx, cy, r, ink, w=None):
        b = (cx*SS-r*SS, cy*SS-r*SS, cx*SS+r*SS, cy*SS+r*SS)
        if w is None: self.d.ellipse(b, fill=ink)
        else: self.d.ellipse(b, outline=ink, width=max(1, int(w*SS)))
    def rrect(self, box, r, ink, w=None, fill=None):
        b = tuple(v*SS for v in box)
        self.d.rounded_rectangle(b, r*SS, outline=ink if w else None, width=max(1, int((w or 1)*SS)), fill=fill)
    def text(self, xy, t, k, size, ink, spacing=0):
        f = A.font(k, int(size*SS)); x, y = xy[0]*SS, xy[1]*SS
        if spacing:
            tot = sum(self.d.textlength(c, font=f) for c in t) + spacing*SS*(len(t)-1); x -= tot/2
            for c in t:
                self.d.text((x, y), c, font=f, fill=ink, anchor='lm'); x += self.d.textlength(c, font=f) + spacing*SS
        else: self.d.text((x, y), t, font=f, fill=ink, anchor='mm')
    def taper(self, pts, ink, w0, w1, wmid=None):
        """Engraver's stroke: width swells/tapers along the path."""
        n = len(pts); L = []; R = []
        for i, (x, y) in enumerate(pts):
            a = pts[max(0, i-1)]; b = pts[min(n-1, i+1)]
            dx, dy = b[0]-a[0], b[1]-a[1]; l = math.hypot(dx, dy) or 1; nx, ny = -dy/l, dx/l
            t = i/(n-1)
            w = (w0 + (w1-w0)*t) if wmid is None else (w0 + (wmid-w0)*t*2 if t < .5 else wmid + (w1-wmid)*(t-.5)*2)
            L.append((x + nx*w/2, y + ny*w/2)); R.append((x - nx*w/2, y - ny*w/2))
        self.poly(L + R[::-1], ink)
    def save_as(self, key, glow=None):
        im = self.im.resize((self.w, self.h), Image.LANCZOS)
        im.save(os.path.join(ART, key + '_art.png'))
        (glow or A.canvas(self.w, self.h)).save(os.path.join(ART, key + '_glow.png')); print(key, 'art ok')

def bez(p0, p1, p2, p3, n=60):
    return [((1-t)**3*p0[0] + 3*(1-t)**2*t*p1[0] + 3*(1-t)*t*t*p2[0] + t**3*p3[0],
             (1-t)**3*p0[1] + 3*(1-t)**2*t*p1[1] + 3*(1-t)*t*t*p2[1] + t**3*p3[1]) for t in (i/n for i in range(n+1))]

def volute(cx, cy, r0, turns=1.15, flip=1, rot=0, n=90):
    """Golden-ish spiral, outer end first."""
    k = math.log(0.18)/(turns*2*math.pi)
    return [(cx + flip*r0*math.exp(k*t)*math.cos(t*flip + rot), cy + r0*math.exp(k*t)*math.sin(t*flip + rot)) for t in (i/n*turns*2*math.pi for i in range(n+1))]

def leaf(P, base, tip, ink, width, bend=0.25, veins=True):
    """Acanthus-style pointed leaf from base to tip."""
    (x0, y0), (x1, y1) = base, tip; dx, dy = x1-x0, y1-y0; L = math.hypot(dx, dy) or 1; nx, ny = -dy/L, dx/L
    c1 = (x0 + dx*.3 + nx*width, y0 + dy*.3 + ny*width); c2 = (x0 + dx*.75 + nx*width*.6 + nx*L*bend*.3, y0 + dy*.75 + ny*width*.6 + ny*L*bend*.3)
    d1 = (x0 + dx*.3 - nx*width*.7, y0 + dy*.3 - ny*width*.7); d2 = (x0 + dx*.75 - nx*width*.3 + nx*L*bend*.3, y0 + dy*.75 - ny*width*.3 + ny*L*bend*.3)
    P.poly(bez(base, c1, c2, tip, 30) + bez(tip, d2, d1, base, 30), ink)
    if veins:
        mid = bez(base, ((x0+x1)/2 + nx*L*bend*.15, (y0+y1)/2 + ny*L*bend*.15), ((x0+x1)/2 + nx*L*bend*.15, (y0+y1)/2 + ny*L*bend*.15), tip, 20)
        P.line(mid[2:-4], CLEAR, max(1.2, width*0.12))

def scroll(P, x, y, size, ink, flip=1, rot=0, leaves=4, w=None):
    """Gunsmith scroll: tapered volute with leaves sprouting off the outer arc."""
    w = w or size*0.11
    v = volute(x, y, size, turns=1.25, flip=flip, rot=rot)
    P.taper(v, ink, w, w*0.25)
    for i in range(leaves):
        j = int(len(v)*(0.06 + i*0.13)); px, py = v[j]
        qx, qy = v[min(len(v)-1, j+3)]; ang = math.atan2(qy-py, qx-px) - flip*math.pi/2*0.75
        Lf = size*(0.55 - i*0.08)
        leaf(P, (px, py), (px + math.cos(ang)*Lf, py + math.sin(ang)*Lf), ink, Lf*0.32, bend=flip*0.4)
    P.circle(v[-1][0], v[-1][1], w*0.35, ink)

def thorn_branch(P, pts, ink, w0=7, w1=2, every=6, size=24, seed=1):
    random.seed(seed); P.taper(pts, ink, w0, w1)
    n = len(pts)
    for i in range(every//2, n-2, every):
        (x0, y0), (x1, y1) = pts[i-1], pts[i+1]; ang = math.atan2(y1-y0, x1-x0)
        side = 1 if (i//every) % 2 else -1; s = size*(1 - i/n*0.6)
        x, y = pts[i]; nx, ny = -math.sin(ang)*side, math.cos(ang)*side
        tip = (x + nx*s + math.cos(ang)*s*0.9, y + ny*s + math.sin(ang)*s*0.9)
        P.taper(bez((x - math.cos(ang)*s*0.3, y - math.sin(ang)*s*0.3), (x, y), (x + nx*s*0.4, y + ny*s*0.4), tip, 10), ink, s*0.4, 0.5)

def bead_border(P, box, r, ink, step=22, dot=3.2, w=3):
    x0, y0, x1, y1 = box
    P.rrect(box, r, ink, w)
    P.rrect((x0+16, y0+16, x1-16, y1-16), max(4, r-12), ink, w*0.5)
    for x in range(int(x0+r+10), int(x1-r-10), step):
        P.circle(x, y0+8, dot, ink); P.circle(x, y1-8, dot, ink)
    for y in range(int(y0+r+10), int(y1-r-10), step):
        P.circle(x0+8, y, dot, ink); P.circle(x1-8, y, dot, ink)

def hatch_circle(P, cx, cy, r, ink, gap=7, w=1.3, ang=45):
    a = math.radians(ang); ca, sa = math.cos(a), math.sin(a)
    for k in range(-int(r), int(r), gap):
        h = math.sqrt(max(0, r*r - k*k))
        P.line([(cx + k*ca - h*sa, cy + k*sa + h*ca), (cx + k*ca + h*sa, cy + k*sa - h*ca)], ink, w)

def _ang(pts, i):
    a = pts[max(0, i-1)]; b = pts[min(len(pts)-1, i+1)]; return math.atan2(b[1]-a[1], b[0]-a[0])

def rinceau(P, stem, ink, flip=1, w0=10, w1=2.5, curl=90, leaf_every=7, leaf_len=70, thorns=0, tendrils=2, seed=1):
    """Flowing engraved vine: stem curve that ends in a volute, alternating acanthus leaves, tendril curls, optional thorns."""
    random.seed(seed)
    ex, ey = stem[-1]; a = _ang(stem, len(stem)-1)
    # volute continuing the stem's direction, curling toward flip side
    k = math.log(0.15)/(1.3*2*math.pi); vol = []
    for i in range(1, 80):
        t = i/80*1.3*2*math.pi; r = curl*math.exp(k*t)
        # start tangent to stem
        cxv = ex + math.cos(a + flip*math.pi/2)*curl; cyv = ey + math.sin(a + flip*math.pi/2)*curl
        th = a - flip*math.pi/2 + flip*t
        vol.append((cxv + r*math.cos(th), cyv + r*math.sin(th)))
    path = stem + vol
    P.taper(path, ink, w0, w1)
    P.circle(path[-1][0], path[-1][1], w1*1.4, ink)
    n = len(stem); side = 1
    for j, i in enumerate(range(leaf_every, n + 50, leaf_every)):
        if i >= len(path) - 12: break
        x, y = path[i]; aa = _ang(path, i); side = -side
        Lf = leaf_len*(1 - 0.55*i/len(path))*random.uniform(.85, 1.1)
        if thorns and j % thorns == thorns-1:
            nx, ny = -math.sin(aa)*side, math.cos(aa)*side
            tip = (x + nx*Lf*.45 + math.cos(aa)*Lf*.35, y + ny*Lf*.45 + math.sin(aa)*Lf*.35)
            P.taper(bez((x, y), (x + nx*Lf*.1, y + ny*Lf*.1), (x + nx*Lf*.3, y + ny*Lf*.3), tip, 10), ink, Lf*.16, .5)
            continue
        la = aa + side*0.85
        leaf(P, (x, y), (x + math.cos(la)*Lf, y + math.sin(la)*Lf), ink, Lf*0.34, bend=side*0.5)
        if Lf > 40:  # second lobe -> acanthus feel
            lb = aa + side*0.45
            leaf(P, (x, y), (x + math.cos(lb)*Lf*.62, y + math.sin(lb)*Lf*.62), ink, Lf*0.2, bend=side*0.3, veins=False)
    for t in range(tendrils):
        i = int(n*(0.3 + 0.3*t))
        if i >= n: break
        x, y = stem[i]; aa = _ang(stem, i); sd = 1 if t % 2 else -1
        ln = leaf_len*0.7
        tp = bez((x, y), (x + math.cos(aa + sd*.9)*ln*.5, y + math.sin(aa + sd*.9)*ln*.5), (x + math.cos(aa + sd*.5)*ln, y + math.sin(aa + sd*.5)*ln),
                 (x + math.cos(aa + sd*.2)*ln*1.2, y + math.sin(aa + sd*.2)*ln*1.2), 25)
        ex2, ey2 = tp[-1]; a2 = _ang(tp, len(tp)-1); vol2 = []
        for q in range(1, 40):
            tt = q/40*1.1*2*math.pi; rr = ln*.28*math.exp(math.log(.2)/(1.1*2*math.pi)*tt)
            cx2 = ex2 + math.cos(a2 + sd*math.pi/2)*ln*.28; cy2 = ey2 + math.sin(a2 + sd*math.pi/2)*ln*.28
            th = a2 - sd*math.pi/2 + sd*tt; vol2.append((cx2 + rr*math.cos(th), cy2 + rr*math.sin(th)))
        P.taper(tp + vol2, ink, w0*.45, 1.2)

class ScaledDraw:
    """ImageDraw stand-in that takes 1x pedal pixels and draws on a supersampled Plate."""
    def __init__(self, plate): self.P = plate
    def __getattr__(self, n):
        f = getattr(self.P.d, n)
        def g(xy, *a, **k):
            if 'width' in k: k['width'] = max(1, int(k['width']*SS))
            if isinstance(xy, (list, tuple)) and xy and isinstance(xy[0], (list, tuple)): xy = [(x*SS, y*SS) for x, y in xy]
            else: xy = tuple(v*SS for v in xy)
            if n == 'rounded_rectangle' and a: a = (a[0]*SS,) + tuple(a[1:])
            return f(xy, *a, **k)
        return g
