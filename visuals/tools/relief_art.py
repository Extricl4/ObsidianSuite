"""Heightmaps (16-bit PNG) and hole masks for the 3D relief fronts.

Everything is drawn in inches at PX px/inch, combined as float heights in 0..1,
then saved as 16-bit greyscale. head.py displaces a dense grid with it (zone type
'relief'), so the detail is real geometry, not a printed picture.
"""
import math, numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy import ndimage as ndi

PX = 100


class Relief:
    def __init__(self, w_in, h_in, base=0.1):
        self.w, self.h = int(round(w_in * PX)), int(round(h_in * PX))
        self.W, self.Hin = w_in, h_in
        self.H = np.full((self.h, self.w), base, np.float32)
        self.holes = np.zeros((self.h, self.w), bool)
        yy, xx = np.mgrid[0:self.h, 0:self.w]
        self.X = (xx + 0.5) / PX - w_in / 2          # inches, 0 = centre
        self.Y = h_in / 2 - (yy + 0.5) / PX          # inches, up positive

    # ---------- primitives ----------
    def px(self, x, y):
        return ((x + self.W / 2) * PX, (self.Hin / 2 - y) * PX)

    def mask(self, draw):
        im = Image.new('L', (self.w, self.h), 0)
        draw(ImageDraw.Draw(im), self.px)
        return np.array(im) > 127

    def dome(self, m, top, base=0.0, power=0.55, reach=None):
        """rounded profile over mask m: base at the edge, top in the middle"""
        out = np.zeros_like(self.H)
        if not m.any():
            return out
        ys, xs = np.where(m); y0, y1, x0, x1 = max(ys.min() - 2, 0), ys.max() + 3, max(xs.min() - 2, 0), xs.max() + 3
        sub = m[y0:y1, x0:x1]
        d = ndi.distance_transform_edt(sub)
        r = reach * PX if reach else max(d.max(), 1)
        prof = np.clip(d / r, 0, 1) ** power
        out[y0:y1, x0:x1] = np.where(sub, base + (top - base) * prof, 0)
        return out

    def put(self, layer, m, mode='max'):
        if mode == 'max':
            self.H[m] = np.maximum(self.H[m], layer[m])
        elif mode == 'set':
            self.H[m] = layer[m]
        elif mode == 'add':
            self.H[m] += layer[m]

    def groove(self, m, depth, soft=1.2):
        g = ndi.gaussian_filter(m.astype(np.float32), soft)
        self.H -= depth * g

    def stroke(self, pts, width_in, taper=None):
        """mask of a thick polyline; taper=(w0,w1) in inches varies the width along it"""
        step = max(min(width_in, taper[1] if taper else width_in) / 3, 0.5 / PX)
        dense = [pts[0]]
        for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
            n = max(1, int(math.hypot(x2 - x1, y2 - y1) / step))
            dense += [(x1 + (x2 - x1) * k / n, y1 + (y2 - y1) * k / n) for k in range(1, n + 1)]
        pts = dense
        def dr(d, P):
            n = len(pts)
            for i, (x, y) in enumerate(pts):
                wv = width_in if taper is None else taper[0] + (taper[1] - taper[0]) * i / max(n - 1, 1)
                cx, cy = P(x, y); r = wv * PX / 2
                d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=255)
        return self.mask(dr)

    def poly(self, pts):
        return self.mask(lambda d, P: d.polygon([P(x, y) for x, y in pts], fill=255))

    def ellipse_mask(self, cx, cy, a, b, ang):
        c, s = math.cos(ang), math.sin(ang)
        u = (self.X - cx) * c + (self.Y - cy) * s
        v = -(self.X - cx) * s + (self.Y - cy) * c
        return u, v, (u / a) ** 2 + (v / b) ** 2 <= 1

    # ---------- ornaments ----------
    def frame(self, fw=0.38, top=0.62, beads=True, inner_line=True, bead_r=0.055, bead_step=0.19):
        d = np.minimum.reduce([self.X + self.W / 2, self.W / 2 - self.X, self.Y + self.Hin / 2, self.Hin / 2 - self.Y])
        t = d / fw
        prof = np.where(t < 0.35, top * np.sin(np.clip(t / 0.35, 0, 1) * math.pi / 2) ** 0.6,
                        np.where(t < 0.55, top * 0.82, np.where(t < 1.0, top * 0.82 * np.clip(1 - (t - 0.55) / 0.45, 0, 1) ** 1.6 + self.H.min() * 0, -1)))
        m = t < 1.0
        self.H[m] = np.maximum(self.H[m], prof[m])
        if beads:  # pearl beading just inside the frame, evenly spaced so the corners line up
            r = bead_r; inset = fw + 0.1
            x0, x1, y0, y1 = -self.W / 2 + inset, self.W / 2 - inset, -self.Hin / 2 + inset, self.Hin / 2 - inset
            nx = max(2, round((x1 - x0) / bead_step)); ny = max(2, round((y1 - y0) / bead_step))
            pts = set()
            for i in range(nx + 1):
                x = x0 + (x1 - x0) * i / nx; pts.add((round(x, 4), y0)); pts.add((round(x, 4), y1))
            for j in range(ny + 1):
                y = y0 + (y1 - y0) * j / ny; pts.add((x0, round(y, 4))); pts.add((x1, round(y, 4)))
            self.beads(sorted(pts), r, top * 0.62)
        if inner_line:
            a = fw + 0.24; lw = 0.035
            dd = np.minimum.reduce([self.X + self.W / 2, self.W / 2 - self.X, self.Y + self.Hin / 2, self.Hin / 2 - self.Y])
            m = np.abs(dd - a) <= lw
            self.put(self.dome(m, top * 0.45, 0.12, 0.5, reach=lw), m)

    def beads(self, pts, r, top):
        m = np.zeros_like(self.holes); d = np.zeros_like(self.H); rp = int(r * PX) + 2
        for x, y in pts:
            cx, cy = self.px(x, y); cx, cy = int(cx), int(cy)
            ya, yb, xa, xb = max(cy - rp, 0), cy + rp + 1, max(cx - rp, 0), cx + rp + 1
            rr = (self.X[ya:yb, xa:xb] - x) ** 2 + (self.Y[ya:yb, xa:xb] - y) ** 2
            sel = rr <= r * r
            m[ya:yb, xa:xb] |= sel
            d[ya:yb, xa:xb] = np.where(sel, np.maximum(d[ya:yb, xa:xb], top * np.sqrt(np.clip(1 - rr / (r * r), 0, 1))), d[ya:yb, xa:xb])
        self.put(d, m)

    def rose(self, cx, cy, R, layers=6, base=0.2, top=1.0, seed=0):
        """rose seen from the front: cupped petals whose height follows one smooth dome,
        tips curled up and separated by fine grooves, spiral bud in the middle"""
        rng = np.random.default_rng(abs(int(seed)))
        rad = np.sqrt((self.X - cx) ** 2 + (self.Y - cy) ** 2)
        env = base + (top - base) * np.sqrt(np.clip(1 - (rad / (R * 1.08)) ** 2, 0, 1))
        for k in range(layers):
            rk = R * (1 - k / (layers + 0.4)) ** 0.9
            n = 5 + (k % 3 == 1)
            off = rng.uniform(0, 2 * math.pi)
            for i in range(n):
                ang = off + i * 2 * math.pi / n + rng.uniform(-0.12, 0.12)
                a = rk * rng.uniform(0.44, 0.52); b = rk * rng.uniform(0.5, 0.6)
                d0 = rk - a * 0.98
                pcx = cx + math.cos(ang) * d0; pcy = cy + math.sin(ang) * d0
                u, v, m = self.ellipse_mask(pcx, pcy, a, b, ang)
                if not m.any():
                    continue
                uu = np.clip(u / a, -1, 1); vv = np.clip(v / b, -1, 1)
                lip = np.clip((uu - 0.35) / 0.65, 0, 1) ** 2 * (1 - vv ** 2)     # curled-up petal lip
                hollow = np.sqrt(np.clip(1 - uu ** 2 - vv ** 2, 0, 1))
                hgt = env * (0.86 + 0.06 * hollow) + 0.09 * (top - base) * lip
                self.put(np.where(m, hgt, 0), m)
                ys, xs = np.where(m); y0, y1, x0, x1 = max(ys.min() - 3, 0), ys.max() + 4, max(xs.min() - 3, 0), xs.max() + 4
                sub = m[y0:y1, x0:x1]
                e = sub & ~ndi.binary_erosion(sub, iterations=max(1, int(PX / 70)))
                # only the outer rim of the petal gets a groove (where it overlaps the petal below)
                outer = (u[y0:y1, x0:x1] / a) > -0.2
                self.H[y0:y1, x0:x1][e & outer] -= 0.03
        r0 = R * 0.22
        bud = rad <= r0
        self.put(np.where(bud, env + 0.03, 0), bud)
        th = np.linspace(0.3, 4.4 * math.pi, 260); rr = r0 * (1 - th / (4.8 * math.pi))
        sp = [(cx + math.cos(t) * q, cy + math.sin(t) * q) for t, q in zip(th, rr)]
        self.groove(self.stroke(sp, 0.022), 0.05, 0.7)

    def leaf(self, x0, y0, ang, L, Wd=0.32, top=0.55, base=0.15):
        n = 40; side1 = []; side2 = []
        c, s = math.cos(ang), math.sin(ang)
        for i in range(n + 1):
            t = i / n; hw = L * Wd * math.sin(math.pi * t) ** 0.85
            ax, ay = x0 + c * L * t, y0 + s * L * t
            side1.append((ax - s * hw, ay + c * hw)); side2.append((ax + s * hw, ay - c * hw))
        m = self.poly(side1 + side2[::-1])
        self.put(self.dome(m, top, base, 0.6), m)
        rib = [(x0 + c * L * t, y0 + s * L * t) for t in np.linspace(0.05, 0.92, 30)]
        self.groove(self.stroke(rib, 0.03), 0.07, 0.8)
        for t in np.linspace(0.22, 0.78, 5):
            ax, ay = x0 + c * L * t, y0 + s * L * t
            for sg in (1, -1):
                vx, vy = c * 0.5 - sg * s * 0.8, s * 0.5 + sg * c * 0.8
                hw = L * Wd * math.sin(math.pi * t) ** 0.85 * 0.75
                self.groove(self.stroke([(ax + vx * hw * f, ay + vy * hw * f) for f in np.linspace(0, 1, 8)], 0.02), 0.04, 0.7)

    def vine(self, pts, width=0.16, top=0.5, base=0.15, thorns=True, thorn_every=0.55, seed=1):
        m = self.stroke(pts, width)
        self.put(self.dome(m, top, base, 0.5, reach=width / 2), m)
        if not thorns:
            return
        rng = np.random.default_rng(abs(int(seed))); acc = 0; side = 1
        for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
            seg = math.hypot(x2 - x1, y2 - y1); acc += seg
            if acc >= thorn_every and seg > 0:
                acc = 0; side *= -1
                tx, ty = (x2 - x1) / seg, (y2 - y1) / seg; nx, ny = -ty * side, tx * side
                L = width * rng.uniform(1.6, 2.2); bw = width * 0.45
                bx, by = x1 + nx * width * 0.35, y1 + ny * width * 0.35
                tip = (bx + nx * L - tx * L * 0.45, by + ny * L - ty * L * 0.45)
                tm = self.poly([(bx - tx * bw, by - ty * bw), (bx + tx * bw, by + ty * bw), tip])
                self.put(self.dome(tm, top * 0.95, base, 0.45), tm)

    def scroll(self, cx, cy, r0, turns=1.6, sgn=1, ang0=0.0, width=(0.26, 0.06), top=0.6, base=0.15, lobes=True, flip=1):
        th = np.linspace(0, turns * 2 * math.pi, 260); b = 0.22
        rr = r0 * np.exp(-b * th)
        pts = [(cx + flip * math.cos(ang0 + sgn * t) * r, cy + math.sin(ang0 + sgn * t) * r) for t, r in zip(th, rr)]
        m = self.stroke(pts, width[0], taper=width)
        self.put(self.dome(m, top, base, 0.5, reach=width[0] / 2), m)
        self.groove(self.stroke(pts[:200], 0.025), 0.05, 0.7)
        if lobes:   # acanthus lobes along the outer side of the first turn
            for t in np.linspace(0.4, 3.6, 6):
                r = r0 * math.exp(-b * t) * 1.08; a = ang0 + sgn * t
                lx, ly = cx + flip * math.cos(a) * r, cy + math.sin(a) * r
                out = math.atan2(math.sin(a), flip * math.cos(a)) + sgn * flip * 0.9
                self.leaf(lx, ly, out, r0 * 0.42 * math.exp(-b * t * 0.6), 0.36, top * 0.85, base)
        return pts

    def circle_ring(self, cx, cy, r, w, top, base=0.15):
        m = self.mask(lambda d, P: d.ellipse((P(cx - r, cy + r)[0], P(cx - r, cy + r)[1], P(cx + r, cy - r)[0], P(cx + r, cy - r)[1]), outline=255, width=max(2, int(w * PX))))
        self.put(self.dome(m, top, base, 0.5, reach=w / 2), m)
        return m

    def shell(self, cx, cy, R, up=1, ribs=11, top=0.75, base=0.15):
        """scallop shell: fan of rounded ribs opening up (up=1) or down (up=-1)"""
        for i in range(ribs):
            a = math.pi * (0.08 + 0.84 * i / (ribs - 1))
            tip = (cx + math.cos(a) * R, cy + up * math.sin(a) * R)
            m = self.poly([(cx - 0.04, cy), (cx + 0.04, cy), (tip[0] + 0.09 * math.sin(a), tip[1] - up * 0.09 * math.cos(a)), (tip[0] - 0.09 * math.sin(a), tip[1] + up * 0.09 * math.cos(a))])
            self.put(self.dome(m, top, base, 0.5), m)
        rim = [(cx + math.cos(a) * R, cy + up * math.sin(a) * R) for a in np.linspace(0.05 * math.pi, 0.95 * math.pi, 80)]
        m = self.stroke(rim, 0.08); self.put(self.dome(m, top * 0.9, base, 0.5, reach=0.04), m)
        b = (self.X - cx) ** 2 + (self.Y - cy) ** 2 <= (R * 0.16) ** 2
        self.put(self.dome(b, top, base, 0.6), b)

    def oval_ring(self, cx, cy, rx, ry, w, top, base=0.15, beads=0, bead_r=0.045):
        e = ((self.X - cx) / rx) ** 2 + ((self.Y - cy) / ry) ** 2
        e2 = ((self.X - cx) / (rx - w)) ** 2 + ((self.Y - cy) / (ry - w)) ** 2
        m = (e <= 1) & (e2 >= 1)
        self.put(self.dome(m, top, base, 0.55, reach=w / 2), m)
        if beads:
            k = rx - w - bead_r * 1.8; kk = ry - w - bead_r * 1.8
            self.beads([(cx + math.cos(t) * k, cy + math.sin(t) * kk) for t in np.linspace(0, 2 * math.pi, beads, endpoint=False)], bead_r, top * 0.8)
        return m

    def sscroll(self, p0, c1, c2, centre, r0, turns, sgn, width=(0.2, 0.05), top=0.6, base=0.12, flute=True):
        """baroque S/C-scroll: a cubic curve from p0 that rolls into a logarithmic spiral at 'centre'"""
        b = 0.24; th = np.linspace(0, turns * 2 * math.pi, 200)
        a0 = math.atan2(c2[1] - centre[1], c2[0] - centre[0])
        spiral = [(centre[0] + math.cos(a0 + sgn * t) * r0 * math.exp(-b * t), centre[1] + math.sin(a0 + sgn * t) * r0 * math.exp(-b * t)) for t in th]
        s0 = spiral[0]
        bez = [((1 - t) ** 3 * p0[0] + 3 * (1 - t) ** 2 * t * c1[0] + 3 * (1 - t) * t * t * c2[0] + t ** 3 * s0[0],
                (1 - t) ** 3 * p0[1] + 3 * (1 - t) ** 2 * t * c1[1] + 3 * (1 - t) * t * t * c2[1] + t ** 3 * s0[1]) for t in np.linspace(0, 1, 120)]
        pts = bez + spiral[1:]
        m = self.stroke(pts, width[0], taper=width)
        self.put(self.dome(m, top, base, 0.5, reach=width[0] / 2), m)
        if flute:
            self.groove(self.stroke(pts[5:-30], 0.022), 0.06, 0.7)
        return pts

    def crescent(self, cx, cy, R, off=0.42, ang=0.0, top=0.85, base=0.12):
        d1 = (self.X - cx) ** 2 + (self.Y - cy) ** 2 <= R * R
        ox, oy = cx + math.cos(ang) * R * off, cy + math.sin(ang) * R * off
        d2 = (self.X - ox) ** 2 + (self.Y - oy) ** 2 <= (R * 0.88) ** 2
        m = d1 & ~d2
        self.put(self.dome(m, top, base, 0.55), m)

    def star(self, cx, cy, R, top=0.7, base=0.12, n=4):
        pts = []
        for i in range(2 * n):
            a = math.pi / 2 + i * math.pi / n; rr = R if i % 2 == 0 else R * 0.3
            pts.append((cx + math.cos(a) * rr, cy + math.sin(a) * rr))
        m = self.poly(pts); self.put(self.dome(m, top, base, 0.6), m)

    def glyph(self, text, fontpath, cx, cy, size_in, top=0.7, base=0.12, power=0.5, rot=0, reach=None):
        """raised relief of a font glyph/string, centred at (cx,cy), cap height ~size_in"""
        f = ImageFont.truetype(fontpath, max(8, int(size_in * PX)))
        im = Image.new('L', (self.w, self.h), 0); d = ImageDraw.Draw(im)
        x, y = self.px(cx, cy)
        if rot:
            tmp = Image.new('L', (int(size_in * PX * 4 * max(1, len(text))), int(size_in * PX * 3)), 0)
            ImageDraw.Draw(tmp).text((tmp.size[0] / 2, tmp.size[1] / 2), text, font=f, fill=255, anchor='mm')
            tmp = tmp.rotate(rot, resample=Image.BICUBIC, expand=False)
            im.paste(tmp, (int(x - tmp.size[0] / 2), int(y - tmp.size[1] / 2)), tmp)
        else:
            d.text((x, y), text, font=f, fill=255, anchor='mm')
        m = np.array(im) > 127
        self.put(self.dome(m, top, base, power, reach=reach), m)
        return m

    def rays(self, cx, cy, r0, r1, n, w0, w1=0.0, top=0.6, base=0.12, wavy=False, off=0.0, arc=(0, 2 * math.pi)):
        for i in range(n):
            a = arc[0] + (arc[1] - arc[0]) * (i + 0.5) / n + off
            c, s_ = math.cos(a), math.sin(a)
            if wavy and i % 2 == 1:
                pts = [(cx + c * rr - s_ * 0.06 * math.sin(k * 1.4) , cy + s_ * rr + c * 0.06 * math.sin(k * 1.4)) for k, rr in enumerate(np.linspace(r0, r1 * 0.92, 18))]
                m = self.stroke(pts, w0, taper=(w0, max(w1, 0.02)))
            else:
                m = self.poly([(cx + c * r0 - s_ * w0 / 2, cy + s_ * r0 + c * w0 / 2), (cx + c * r0 + s_ * w0 / 2, cy + s_ * r0 - c * w0 / 2),
                               (cx + c * r1 + s_ * w1 / 2, cy + s_ * r1 - c * w1 / 2), (cx + c * r1 - s_ * w1 / 2, cy + s_ * r1 + c * w1 / 2)])
            self.put(self.dome(m, top, base, 0.5), m)

    def braid(self, x0, x1, yc, amp, period, w, top=0.6, base=0.12, strands=2):
        """interlaced band: strands that alternately pass over and under each other"""
        xs = np.linspace(x0, x1, int((x1 - x0) * 60) + 2)
        mid = (top + base) / 2 + 0.08; sw = (top - base) * 0.32
        for sidx in range(strands):
            ph = 2 * math.pi * sidx / strands
            pts = [(x, yc + amp * math.sin(2 * math.pi * x / period + ph)) for x in xs]
            m = self.stroke(pts, w)
            if not m.any():
                continue
            hh = mid + sw * np.cos(2 * math.pi * self.X / period + ph)   # strand 0 high at even crossings, strand 1 at odd
            lay = self.dome(m, 1.0, 0.0, 0.5, reach=w / 2)
            lay = np.where(m, base + (hh - base) * (0.55 + 0.45 * lay), 0)
            edge = m & ~ndi.binary_erosion(m, iterations=max(1, int(PX / 60)))
            self.put(lay, m)
            self.H[edge] = np.minimum(self.H[edge], base + 0.04)

    def bat(self, cx, cy, span, top=0.7, base=0.12, rot=0.0):
        """bat with spread wings: smooth leading edge, scalloped trailing edge, finger bones, ears"""
        half = span / 2

        def qcurve(p0, p1, bulge, n=16):
            mx, my = (p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2 + bulge
            return [((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * mx + t * t * p1[0], (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * my + t * t * p1[1]) for t in np.linspace(0, 1, n)]
        lead = qcurve((0.07, 0.06), (0.42, 0.3), 0.06) + qcurve((0.42, 0.3), (1.0, 0.24), 0.1)[1:]
        tips = [(1.0, 0.24), (0.8, -0.04), (0.58, -0.12), (0.34, -0.16), (0.08, -0.12)]
        trail = []
        for a, b in zip(tips, tips[1:]):
            trail += qcurve(a, b, 0.09)[1:]
        wing = lead + trail
        right = [(cx + half * u, cy + half * v) for u, v in wing]
        left = [(cx - half * u, cy + half * v) for u, v in wing[::-1]]
        head = [(cx - 0.07 * half, cy + 0.1 * half), (cx - 0.065 * half, cy + 0.19 * half), (cx - 0.03 * half, cy + 0.13 * half),
                (cx + 0.03 * half, cy + 0.13 * half), (cx + 0.065 * half, cy + 0.19 * half), (cx + 0.07 * half, cy + 0.1 * half)]
        tail = [(cx + 0.05 * half, cy - 0.2 * half), (cx, cy - 0.27 * half), (cx - 0.05 * half, cy - 0.2 * half)]
        m = self.poly(right[::-1][::-1] and (head[::-1][:0] + right + head[::-1] + left + tail) or right)
        self.put(self.dome(m, top, base, 0.45), m)
        body = ((self.X - cx) / (0.075 * half)) ** 2 + ((self.Y - cy + 0.03 * half) / (0.2 * half)) ** 2 <= 1
        self.put(self.dome(body, top + 0.06, base, 0.6), body)
        for (u, v) in tips[:4]:
            for sx in (1, -1):
                self.groove(self.stroke([(cx + sx * half * 0.08, cy + half * 0.04), (cx + sx * half * u * 0.96, cy + half * (v * 0.9 + 0.02))], 0.022), 0.05, 0.7)
        return m

    def shield(self, cx, cy, w, h, top=0.6, base=0.12, rim=0.08, charge=None):
        n = 30; pts = [(cx - w / 2, cy + h / 2), (cx + w / 2, cy + h / 2), (cx + w / 2, cy + h * 0.05)]
        for t in np.linspace(0, 1, n):     # curved point
            pts.append((cx + w / 2 * (1 - t), cy + h * 0.05 - (h * 0.55) * math.sin(t * math.pi / 2)))
        pts += [(cx - w / 2 * t, cy + h * 0.05 - (h * 0.55) * math.sin((1 - t) * math.pi / 2)) for t in np.linspace(0, 1, n)]
        m = self.poly(pts)
        inner = ndi.binary_erosion(m, iterations=max(1, int(rim * PX)))
        self.put(self.dome(m & ~inner, top, base, 0.5, reach=rim / 2), m & ~inner)
        self.put(np.where(inner, top * 0.55, 0), inner)
        return m, inner

    def card(self, cx, cy, w, h, top=0.55, base=0.12, arch=True, rim=0.07):
        """tarot-card style frame: rectangle (optionally round-arched top) with a double border"""
        n = 40; r = w / 2
        if arch:
            pts = [(cx - w / 2, cy - h / 2), (cx + w / 2, cy - h / 2), (cx + w / 2, cy + h / 2 - r)]
            pts += [(cx + r * math.cos(a), cy + h / 2 - r + r * math.sin(a)) for a in np.linspace(0, math.pi, n)]
            pts += [(cx - w / 2, cy + h / 2 - r)]
        else:
            pts = [(cx - w / 2, cy - h / 2), (cx + w / 2, cy - h / 2), (cx + w / 2, cy + h / 2), (cx - w / 2, cy + h / 2)]
        m = self.poly(pts)
        i1 = ndi.binary_erosion(m, iterations=max(1, int(rim * PX)))
        i2 = ndi.binary_erosion(i1, iterations=max(1, int(0.07 * PX)))
        i3 = ndi.binary_erosion(i2, iterations=max(1, int(0.035 * PX)))
        self.put(self.dome(m & ~i1, top, base, 0.5, reach=rim / 2), m & ~i1)
        self.put(self.dome(i2 & ~i3, top * 0.8, base, 0.5, reach=0.02), i2 & ~i3)
        self.H[i3] = np.minimum(self.H[i3], base + 0.02)
        return m, i3

    def pipe(self, cx, y0, y1, r, top=0.85, base=0.12, mouth=True):
        """organ pipe: vertical rounded cylinder with a pointed foot and a mouth"""
        m = (np.abs(self.X - cx) <= r) & (self.Y >= y0 + r * 1.6) & (self.Y <= y1)
        foot = self.poly([(cx - r, y0 + r * 1.6), (cx + r, y0 + r * 1.6), (cx + r * 0.25, y0), (cx - r * 0.25, y0)])
        u = np.clip((self.X - cx) / r, -1, 1)
        prof = np.sqrt(np.clip(1 - u * u, 0, 1))
        self.put(np.where(m | foot, base + (top - base) * (0.3 + 0.7 * prof), 0), m | foot)
        cap = (np.abs(self.X - cx) <= r) & (np.abs(self.Y - y1) < 0.025)
        self.H[cap] -= 0.03
        if mouth:
            my = y0 + r * 2.4
            mm = self.poly([(cx - r * 0.55, my), (cx + r * 0.55, my), (cx + r * 0.4, my + r * 0.75), (cx - r * 0.4, my + r * 0.75)])
            self.H[mm] = np.minimum(self.H[mm], base + (top - base) * 0.35)
            lip = self.poly([(cx - r * 0.55, my - 0.02), (cx + r * 0.55, my - 0.02), (cx, my - r * 0.6)])
            self.put(np.where(lip, top + 0.02, 0), lip)

    # ---------- output ----------
    def save(self, hpath, holes_path=None, blur=0.8):
        H = ndi.gaussian_filter(self.H, blur)
        H = np.clip(H, 0, 1)
        Image.fromarray((H * 65535).astype(np.uint16)).save(hpath)
        if holes_path:
            Image.fromarray((self.holes * 255).astype(np.uint8)).save(holes_path)


def arch_pts(cx, y0, w, ys, sharp=1.0, n=40):
    """pointed (gothic) arch outline: vertical sides from y0 up to the springing line ys,
    then two arcs of radius w*sharp meeting at the apex."""
    R = w * sharp; half = w / 2
    cxl = cx - half + R                              # left arc is centred on the right side
    apex_x = cx; apex_dy = math.sqrt(max(R * R - (R - half) ** 2, 0))
    left = [(cx - half, y0), (cx - half, ys)]
    a0 = math.pi; a1 = math.atan2(apex_dy, cx - cxl)
    for t in np.linspace(0, 1, n):
        a = a0 + (a1 - a0) * t
        left.append((cxl + R * math.cos(a), ys + R * math.sin(a)))
    right = [(2 * cx - x, y) for x, y in left[::-1]]
    return left + right[1:], ys + apex_dy


def offset_poly_mask(rel, pts, inset):
    m = rel.poly(pts)
    return ndi.binary_erosion(m, iterations=max(1, int(inset * PX))) if inset > 0 else m
