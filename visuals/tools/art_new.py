"""Relief fronts for the ten new amps. Usage: python3 -I tools/art_new.py <name>  (writes art/<name>_h.png [+ _holes, _glass])"""
import math, sys, numpy as np
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import relief_art as ra
ra.PX = 120
from relief_art import Relief, arch_pts
from scipy import ndimage as ndi
from PIL import Image

W, H = 22.56, 5.55
DJ = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
DJB = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
FD = __file__.rsplit('/', 2)[0] + '/render/fonts/'


def window(r, cx, y0, w, ys, sharp=1.0, bar=0.11, top=0.8, mullions=1, trefoil=True):
    """pierced gothic window: moulded edge, mullions, trefoil head. Returns (opening, bars)."""
    outer, apex = arch_pts(cx, y0, w, ys, sharp)
    om = r.poly(outer)
    inner = ndi.binary_erosion(om, iterations=max(1, int(bar * ra.PX)))
    edge = om & ~inner
    r.put(r.dome(edge, top, 0.14, 0.5, reach=bar / 2), edge)
    bars = edge.copy()
    for i in range(mullions):
        mx = cx - w / 2 + w * (i + 1) / (mullions + 1)
        m = r.stroke([(mx, y0), (mx, ys + 0.15)], 0.08); r.put(r.dome(m, top * 0.92, 0.14, 0.5, reach=0.04), m); bars |= m
    if trefoil:
        tr = min(w * 0.17, 0.42); ty = ys + (apex - ys) * 0.5
        m = r.circle_ring(cx, ty, tr, 0.065, top * 0.88); bars |= m
        for k in range(3):
            a = math.pi / 2 + k * 2 * math.pi / 3
            m = r.circle_ring(cx + math.cos(a) * tr * 0.42, ty + math.sin(a) * tr * 0.42, tr * 0.38, 0.045, top * 0.8); bars |= m
        if mullions:
            for s2 in (-1, 1):
                sub, _ = arch_pts(cx + s2 * w / 4, y0, w / 2 - 0.1, ys + 0.05)
                m = r.stroke(sub[1:-1], 0.06); r.put(r.dome(m, top * 0.85, 0.14, 0.5, reach=0.03), m); bars |= m
    return inner, bars, apex


def window3(r, cx, y0, w, ys, sharp=0.8, bar=0.11, top=0.8, lights=3):
    """pierced gothic window with `lights` lancets and a rose: every bar ties into the next
    (lancet sides = mullions, rose sits on the middle lancet apex, struts to side lancets and arch)."""
    outer, apex = arch_pts(cx, y0, w, ys, sharp)
    om = r.poly(outer)
    inner = ndi.binary_erosion(om, iterations=max(1, int(bar * ra.PX)))
    edge = om & ~inner
    r.put(r.dome(edge, top, 0.14, 0.5, reach=bar / 2), edge)
    bars = edge.copy()
    wi = w - 2 * bar + 0.04; lw = wi / lights
    bx = [cx - wi / 2 + lw * k for k in range(lights + 1)]
    ysl = ys - 0.3
    _, lap = arch_pts(cx, y0, lw, ysl, 1.0)
    for k in range(1, lights):      # mullions run from the sill to the lancet springing
        m = r.stroke([(bx[k], y0), (bx[k], ysl + 0.02)], 0.08); r.put(r.dome(m, top * 0.92, 0.14, 0.5, reach=0.04), m); bars |= m
    tops = []
    for k in range(lights):          # lancet heads
        lc = (bx[k] + bx[k + 1]) / 2
        sub, a_ = arch_pts(lc, y0, lw, ysl, 1.0)
        pts = [p for p in sub if p[1] >= ysl - 0.01]
        m = r.stroke(pts, 0.07); r.put(r.dome(m, top * 0.88, 0.14, 0.5, reach=0.035), m); bars |= m
        tops.append((lc, a_))
    # rose: sits on the middle lancet apex and reaches up into the arch head
    in_apex = apex - bar * 1.6
    tr = (in_apex - lap) / 2 + 0.02; ty = lap + tr - 0.03
    def arch_half(y):  # inner half-width of the main arch at height y
        R = w * sharp; half = w / 2; cxl = cx - half + R
        return abs(cxl - math.sqrt(max(R * R - (y - ys) ** 2, 0)) - cx) - bar if y > ys else half - bar
    tr = min(tr, arch_half(ty) - 0.02)
    m = r.circle_ring(cx, ty, tr, 0.075, top * 0.9); bars |= m
    d = tr * 0.52; rho = tr * 0.45
    for k in range(3):
        a = math.pi / 2 + k * 2 * math.pi / 3
        m = r.circle_ring(cx + math.cos(a) * d, ty + math.sin(a) * d, rho, 0.05, top * 0.82); bars |= m
    def strut(a, b, wd=0.065):
        global_m = r.stroke([a, b], wd); r.put(r.dome(global_m, top * 0.86, 0.14, 0.5, reach=wd / 2), global_m); return global_m
    # struts: side lancet apexes -> rose, rose sides -> arch
    for (lc, a_) in tops:
        if abs(lc - cx) < 1e-3: continue
        vx, vy = lc - cx, a_ - ty; L = math.hypot(vx, vy)
        bars |= strut((lc, a_ - 0.02), (cx + vx / L * (tr - 0.01), ty + vy / L * (tr - 0.01)))
    for sx in (-1, 1):
        bars |= strut((cx + sx * (tr - 0.02), ty), (cx + sx * (arch_half(ty) + 0.05), ty))
    return inner, bars, apex


def finish_holes(r, openings, bars):
    r.holes = openings & ~ndi.binary_dilation(bars, iterations=1)


# ------------------------------------------------------------------ STRIX (vampire)
def strix():
    r = Relief(W, H, base=0.14); r.frame(fw=0.3, top=0.6, bead_r=0.045, bead_step=0.16)
    O = np.zeros_like(r.holes); Bm = np.zeros_like(r.holes)
    o, b, apex = window3(r, 0, -2.25, 3.2, -0.55, sharp=0.8); O |= o; Bm |= b
    for sx in (1, -1):
        for cx in (4.7, 7.6):
            o, b, _ = window(r, sx * cx, -2.25, 1.55, 0.05, mullions=1); O |= o; Bm |= b
            pin = r.poly([(sx * cx - 0.1, 1.35), (sx * cx + 0.1, 1.35), (sx * cx, 1.85)]); r.put(r.dome(pin, 0.8, 0.2, 0.5), pin)
        # thorny vines with roses winding along the bottom and up between windows
        pts = [(sx * x, -2.05 + 0.18 * math.sin(x * 2.3)) for x in np.linspace(2.0, 10.3, 300)]
        r.vine(pts, 0.1, 0.5, 0.14, thorn_every=0.3, seed=3 + sx)
        for vx in (3.15, 6.15, 9.05):
            pts = [(sx * (vx + 0.22 * math.sin(y * 3)), y) for y in np.linspace(-2.05, 1.7, 160)]
            r.vine(pts, 0.085, 0.48, 0.14, thorn_every=0.28, seed=int(vx * 10) + sx)
            r.rose(sx * vx, 1.75, 0.3, layers=4, base=0.2, top=0.78, seed=int(vx * 7) + sx)
        r.crescent(sx * 9.95, 1.55, 0.36, off=0.42, ang=math.pi if sx > 0 else 0, top=0.8)
        for (x, y) in ((10.2, 0.6), (9.6, 1.95), (10.3, -0.4)):
            r.star(sx * x, y, 0.09, 0.7)
    r.bat(0, 1.42, 4.0, top=0.95)
    finish_holes(r, O, Bm); return r


# ------------------------------------------------------------------ ASTRAEA (tarot: The Star)
def astraea():
    r = Relief(W, H, base=0.1); r.frame(fw=0.32, top=0.6, bead_r=0.045, bead_step=0.16)
    zod = '♈♉♊♋♌♍♎♏♐♑♒♓'
    for i, ch in enumerate(zod):   # zodiac along the top and bottom borders
        x = -9.6 + i * (19.2 / 11)
        r.glyph(ch, DJ, x, 2.02, 0.26, 0.65)
        r.glyph(zod[(i + 6) % 12], DJ, x, -2.02, 0.26, 0.65)
    # three arched cards
    for cx, kind in ((-6.4, 'moon'), (0, 'star'), (6.4, 'sun')):
        m, inner = r.card(cx, -0.08, 3.3, 3.55, top=0.62)
        if kind == 'star':
            r.rays(cx, 0.15, 0.25, 1.35, 8, 0.32, 0.0, top=0.95)
            r.rays(cx, 0.15, 0.2, 0.9, 8, 0.18, 0.0, top=0.85, off=math.pi / 8)
            b = (r.X - cx) ** 2 + (r.Y - 0.15) ** 2 <= 0.22 ** 2; r.put(r.dome(b, 1.0, 0.5, 0.6), b)
            for (dx, dy, s) in ((-1.0, 1.15, 0.12), (1.05, 1.1, 0.13), (-1.15, -0.9, 0.1), (1.1, -0.95, 0.11), (0, -1.45, 0.12), (-0.5, 1.5, 0.08), (0.55, 1.55, 0.08)):
                r.star(cx + dx, 0.15 + dy, s, 0.8, n=4)
        if kind == 'moon':
            r.crescent(cx - 0.15, 0.2, 1.05, off=0.42, ang=0.4, top=0.95)
            for (dx, dy, s) in ((0.75, 0.9, 0.14), (1.0, 0.1, 0.1), (0.55, -0.6, 0.09), (-0.9, -1.2, 0.1), (0.9, -1.25, 0.12), (-0.2, 1.45, 0.08)):
                r.star(cx + dx, dy, s, 0.8, n=4)
        if kind == 'sun':
            r.rays(cx, 0.15, 0.62, 1.4, 16, 0.22, 0.0, top=0.82, wavy=True)
            b = (r.X - cx) ** 2 + (r.Y - 0.15) ** 2 <= 0.6 ** 2; r.put(r.dome(b, 0.95, 0.4, 0.55), b)
            r.circle_ring(cx, 0.15, 0.5, 0.05, 1.0)
            th = np.linspace(0, 4 * math.pi, 220); sp = [(cx + math.cos(t) * 0.38 * (1 - t / (4.5 * math.pi)), 0.15 + math.sin(t) * 0.38 * (1 - t / (4.5 * math.pi))) for t in th]
            r.groove(r.stroke(sp, 0.03), 0.07, 0.8)
    # constellation lines between the cards
    for sx in (1, -1):
        pts = [(sx * 2.0, 1.0), (sx * 2.55, 1.45), (sx * 3.1, 0.75), (sx * 2.7, -0.2), (sx * 3.25, -0.95), (sx * 2.25, -1.3)]
        for a, b_ in zip(pts, pts[1:]):
            m = r.stroke([a, b_], 0.025); r.put(r.dome(m, 0.42, 0.1, 0.5, reach=0.012), m)
        for (x, y) in pts:
            r.star(x, y, 0.1, 0.75, n=4)
        for (x, y) in ((9.6, 1.1), (10.1, 0.2), (9.7, -0.9), (10.2, -1.5)):
            r.star(sx * x, y, 0.12, 0.75)
    return r


# ------------------------------------------------------------------ TYCHE (tarot: Wheel of Fortune)
def tyche():
    r = Relief(W, H, base=0.1); r.frame(fw=0.32, top=0.62, bead_r=0.045, bead_step=0.16)
    R = 2.05
    r.circle_ring(0, 0, R, 0.2, 0.85); r.circle_ring(0, 0, R - 0.42, 0.08, 0.7); r.circle_ring(0, 0, 0.62, 0.1, 0.8)
    for i in range(8):
        a = i * math.pi / 4
        m = r.stroke([(math.cos(a) * 0.62, math.sin(a) * 0.62), (math.cos(a) * (R - 0.42), math.sin(a) * (R - 0.42))], 0.1)
        r.put(r.dome(m, 0.75, 0.1, 0.5, reach=0.05), m)
        b = (r.X - math.cos(a) * (R + 0.02)) ** 2 + (r.Y - math.sin(a) * (R + 0.02)) ** 2 <= 0.14 ** 2; r.put(r.dome(b, 0.95, 0.5, 0.6), b)
    for i, ch in enumerate('☉☽☿♀♂♃♄★'):
        a = i * math.pi / 4 + math.pi / 8
        r.glyph(ch, DJ, math.cos(a) * (R - 0.21), math.sin(a) * (R - 0.21), 0.2, 0.75)
    hub = r.X ** 2 + r.Y ** 2 <= 0.45 ** 2; r.put(r.dome(hub, 1.0, 0.45, 0.6), hub)
    r.rose(0, 0, 0.36, layers=4, base=0.6, top=1.0, seed=2)
    for sx, ch in ((-1, '⚔'), (1, '⚖')):
        m, inner = r.card(sx * 6.3, -0.05, 3.0, 3.6, top=0.6)
        r.glyph(ch, DJB, sx * 6.3, -0.05, 1.45, 0.9)
        for k in range(5):
            r.star(sx * 6.3 - 1.0 + k * 0.5, 1.35, 0.07, 0.7)
        r.circle_ring(sx * 6.3, -0.05, 0.98, 0.04, 0.55)
        for (x, y) in ((3.4, 1.4), (3.4, -1.4), (9.6, 1.4), (9.6, -1.4)):
            r.rose(sx * x, y, 0.3, layers=3, base=0.2, top=0.75, seed=abs(int(x * 3 + y * 7 + sx * 5)))
        r.scroll(sx * 3.35, 0.0, 0.55, turns=1.2, sgn=sx, ang0=0 if sx > 0 else math.pi, width=(0.12, 0.035), top=0.6, lobes=False)
        r.scroll(sx * 9.65, 0.0, 0.55, turns=1.2, sgn=-sx, ang0=math.pi if sx > 0 else 0, width=(0.12, 0.035), top=0.6, lobes=False)
    return r


# ------------------------------------------------------------------ MIDGARD (Norse)
def midgard():
    r = Relief(W, H, base=0.1)
    r.frame(fw=0.28, top=0.55, beads=False, inner_line=False)
    rf = FD + 'NotoSansRunic-Regular.ttf'
    r.braid(-10.85, 10.85, 2.15, 0.14, 0.62, 0.1, top=0.7, base=0.1)
    r.braid(-10.85, 10.85, -2.15, 0.14, 0.62, 0.1, top=0.7, base=0.1)
    # rune band under the top braid
    runes = 'ᚠᚢᚦᚨᚱᚲᚷᚹᚺᚾᛁᛃᛇᛈᛉᛊᛏᛒᛖᛗᛚᛜᛞᛟ'
    for i, ch in enumerate(runes[:14]):
        for sx in (1, -1):
            x = sx * (3.0 + i * 0.52)
            if abs(x) < 10.4:
                r.glyph(ch, rf, x, 1.62, 0.32, 0.6)
    # Jormungandr: serpent ring eating its tail, scales, with a valknut inside
    R = 1.62; th = np.linspace(0.25, 2 * math.pi - 0.05, 400)
    body = [(math.cos(t) * R, math.sin(t) * R) for t in th]
    m = r.stroke(body, 0.36, taper=(0.36, 0.14)); r.put(r.dome(m, 0.85, 0.12, 0.5, reach=0.18), m)
    for t in np.linspace(0.4, 2 * math.pi - 0.2, 70):      # scale ridges
        ww = 0.36 - 0.22 * (t / (2 * math.pi))
        a1 = (math.cos(t) * (R - ww / 2), math.sin(t) * (R - ww / 2)); a2 = (math.cos(t + 0.03) * (R + ww / 2), math.sin(t + 0.03) * (R + ww / 2))
        r.groove(r.stroke([a1, a2], 0.02), 0.05, 0.6)
    hx, hy = math.cos(0.12) * R, math.sin(0.12) * R
    head = r.poly([(hx - 0.1, hy - 0.42), (hx + 0.3, hy - 0.25), (hx + 0.38, hy + 0.05), (hx + 0.22, hy + 0.32), (hx - 0.12, hy + 0.3), (hx - 0.28, hy - 0.05)])
    r.put(r.dome(head, 0.95, 0.15, 0.5), head)
    eye = (r.X - (hx + 0.12)) ** 2 + (r.Y - (hy + 0.08)) ** 2 <= 0.05 ** 2; r.H[eye] += 0.08
    for k in range(3):     # valknut: three interlocked triangles
        a0 = math.pi / 2 + k * 2 * math.pi / 3; c = (math.cos(a0) * 0.25, math.sin(a0) * 0.25 - 0.05)
        tri = [(c[0] + math.cos(math.pi / 2 + j * 2 * math.pi / 3) * 0.78, c[1] + math.sin(math.pi / 2 + j * 2 * math.pi / 3) * 0.78) for j in range(4)]
        mm = r.stroke(tri, 0.1); r.put(r.dome(mm, 0.75 + 0.05 * k, 0.12, 0.5, reach=0.05), mm)
        edge = mm & ~ndi.binary_erosion(mm, iterations=2); r.H[edge] -= 0.04
    # round shields with crossed spears on each side
    for sx in (1, -1):
        cx = sx * 7.0
        for d in (-1, 1):
            p0, p1 = (cx - 2.1, -1.55 * d), (cx + 2.1, 1.55 * d)
            mm = r.stroke([p0, p1], 0.09); r.put(r.dome(mm, 0.55, 0.1, 0.5, reach=0.045), mm)
            tip = (p1[0] + 0.0, p1[1]); dx, dy = (p1[0] - p0[0]), (p1[1] - p0[1]); L = math.hypot(dx, dy); ux, uy = dx / L, dy / L
            blade = r.poly([(tip[0] - uy * 0.13, tip[1] + ux * 0.13), (tip[0] + ux * 0.55, tip[1] + uy * 0.55), (tip[0] + uy * 0.13, tip[1] - ux * 0.13), (tip[0] - ux * 0.12, tip[1] - uy * 0.12)])
            r.put(r.dome(blade, 0.7, 0.1, 0.5), blade)
        disc = (r.X - cx) ** 2 + r.Y ** 2 <= 1.25 ** 2
        r.put(np.where(disc, 0.55, 0), disc)
        for k in range(7):    # planks
            x = cx - 1.25 + k * (2.5 / 6)
            g = disc & (np.abs(r.X - x) < 0.012); r.H[g] -= 0.04
        r.circle_ring(cx, 0, 1.25, 0.12, 0.72)
        for k in range(12):
            a = k * math.pi / 6
            b = (r.X - cx - math.cos(a) * 1.25) ** 2 + (r.Y - math.sin(a) * 1.25) ** 2 <= 0.05 ** 2; r.put(r.dome(b, 0.85, 0.6, 0.6), b)
        boss = (r.X - cx) ** 2 + r.Y ** 2 <= 0.38 ** 2; r.put(r.dome(boss, 1.0, 0.6, 0.6), boss)
        r.circle_ring(cx, 0, 0.4, 0.06, 0.85)
        for k in range(3):
            a0 = math.pi / 2 * k
        # triskele spirals near the outer edge
        for k in range(3):
            a = math.pi / 2 + k * 2 * math.pi / 3
            r.scroll(sx * 10.0 + math.cos(a) * 0.22, 0.0 + math.sin(a) * 0.22, 0.42, turns=1.1, sgn=1, ang0=a, width=(0.09, 0.03), top=0.6, lobes=False)
    return r


# ------------------------------------------------------------------ AVALON (Arthurian medieval)
def avalon():
    r = Relief(W, H, base=0.1); r.frame(fw=0.3, top=0.55, bead_r=0.04, bead_step=0.15)
    # romanesque arcade: columns with round arches across the whole front
    n = 7; bw = 20.6 / n; y0, ys = -2.1, 0.75
    for i in range(n):
        cx = -10.3 + bw * (i + 0.5)
        R = bw / 2 - 0.12
        arch = [(cx + math.cos(a) * R, ys + math.sin(a) * R) for a in np.linspace(math.pi, 0, 60)]
        m = r.stroke(arch, 0.16); r.put(r.dome(m, 0.7, 0.1, 0.5, reach=0.08), m)
        m = r.stroke([(cx + math.cos(a) * (R + 0.2), ys + math.sin(a) * (R + 0.2)) for a in np.linspace(math.pi, 0, 60)], 0.05); r.put(r.dome(m, 0.5, 0.1, 0.5, reach=0.025), m)
        for k in range(9):    # chevron/dogtooth on the arch
            a = math.pi * (k + 0.5) / 9
            r.star(cx + math.cos(a) * (R + 0.1), ys + math.sin(a) * (R + 0.1), 0.06, 0.6, n=4)
    for i in range(n + 1):
        x = -10.3 + bw * i
        col = (np.abs(r.X - x) < 0.13) & (r.Y > y0) & (r.Y < ys)
        r.put(np.where(col, 0.1 + 0.6 * np.sqrt(np.clip(1 - ((r.X - x) / 0.13) ** 2, 0, 1)), 0), col)
        for yy, ww in ((ys, 0.22), (y0 + 0.08, 0.22)):
            cap = (np.abs(r.X - x) < ww) & (np.abs(r.Y - yy) < 0.08); r.put(r.dome(cap, 0.75, 0.2, 0.5), cap)
    # heraldry in the bays: centre shield with chevron and crossed swords behind, fleurs and crosses
    for i in range(n):
        cx = -10.3 + bw * (i + 0.5)
        if i == 3:
            for d in (-1, 1):
                p0, p1 = (cx - 0.95 * d, -1.85), (cx + 0.95 * d, 0.55)
                bl = r.stroke([p0, p1], 0.13, taper=(0.15, 0.04)); r.put(r.dome(bl, 0.62, 0.1, 0.5, reach=0.065), bl)
                gx, gy = p0[0] + (p1[0] - p0[0]) * 0.18, p0[1] + (p1[1] - p0[1]) * 0.18
                dx, dy = p1[0] - p0[0], p1[1] - p0[1]; L = math.hypot(dx, dy)
                gd = r.stroke([(gx - dy / L * 0.42, gy + dx / L * 0.42), (gx + dy / L * 0.42, gy - dx / L * 0.42)], 0.1); r.put(r.dome(gd, 0.75, 0.1, 0.5, reach=0.05), gd)
                pm = (r.X - p0[0]) ** 2 + (r.Y - p0[1]) ** 2 <= 0.13 ** 2; r.put(r.dome(pm, 0.8, 0.2, 0.6), pm)
            sm, inner = r.shield(cx, 0.15, 1.75, 2.3, top=0.85)
            chev = r.stroke([(cx - 0.75, -0.55), (cx, 0.3), (cx + 0.75, -0.55)], 0.24) & inner; r.put(np.where(chev, 0.8, 0), chev)
            r.glyph('⚜', DJB, cx, 0.82, 0.48, 0.92)
            r.glyph('⚜', DJB, cx, -0.75, 0.32, 0.9)
        else:
            sm, inner = r.shield(cx, -0.3, 1.45, 1.9, top=0.72)
            ch = '⚜' if i % 2 == 0 else '✠'
            r.glyph(ch, DJB, cx, -0.2, 0.8, 0.9)
            r.rose(cx, 1.6, 0.26, layers=3, base=0.2, top=0.7, seed=i)
    return r


# ------------------------------------------------------------------ HYPERION (baroque gold sunburst)
def hyperion():
    r = Relief(W, H, base=0.1)
    r.frame(fw=0.34, top=0.62, bead_r=0.045, bead_step=0.15)
    # egg-and-dart just inside the frame (top and bottom rows)
    for row in (1.98, -1.98):
        x = -10.2
        while x <= 10.21:
            e = ((r.X - x) / 0.11) ** 2 + ((r.Y - row) / 0.15) ** 2 <= 1; r.put(r.dome(e, 0.7, 0.2, 0.55), e)
            dart = r.poly([(x + 0.2, row + 0.13), (x + 0.24, row + 0.13), (x + 0.22, row - 0.16)]); r.put(r.dome(dart, 0.55, 0.1, 0.5), dart)
            x += 0.44
    r.rays(0, 0, 0.75, 1.7, 24, 0.24, 0.0, top=0.85, wavy=True)
    disc = r.X ** 2 + r.Y ** 2 <= 0.78 ** 2; r.put(r.dome(disc, 0.95, 0.45, 0.55), disc)
    r.circle_ring(0, 0, 0.72, 0.06, 1.0)
    r.beads([(math.cos(t) * 0.6, math.sin(t) * 0.6) for t in np.linspace(0, 2 * math.pi, 28, endpoint=False)], 0.035, 1.0)
    r.rose(0, 0, 0.42, layers=5, base=0.7, top=1.0, seed=8)
    for sx in (1, -1):
        for sy in (1, -1):
            r.sscroll((sx * 1.85, sy * 0.25), (sx * 2.8, sy * 0.05), (sx * 3.4, sy * 1.55), (sx * 4.55, sy * 0.95), 0.6, 1.2, -sx * sy, width=(0.21, 0.05), top=0.66)
            r.sscroll((sx * 5.6, sy * 0.15), (sx * 6.5, sy * 0.0), (sx * 7.1, sy * 1.6), (sx * 8.3, sy * 0.95), 0.58, 1.2, -sx * sy, width=(0.19, 0.045), top=0.64)
            r.leaf(sx * 2.55, sy * 0.5, math.atan2(sy * 0.8, sx), 0.9, 0.3, 0.6, 0.12)
            r.leaf(sx * 6.3, sy * 0.45, math.atan2(sy * 0.9, sx), 0.8, 0.3, 0.58, 0.12)
        r.shell(sx * 9.55, -0.55, 0.62, up=1, ribs=11, top=0.8)
        r.rose(sx * 9.55, 1.0, 0.36, layers=4, base=0.2, top=0.8, seed=12 + sx)
    return r


# ------------------------------------------------------------------ LILITH (gothic stained glass)
def lilith():
    r = Relief(W, H, base=0.14); r.frame(fw=0.3, top=0.58, beads=False)
    O = np.zeros_like(r.holes); Bm = np.zeros_like(r.holes)
    R = 2.15; cy = 0.0
    disc = r.X ** 2 + (r.Y - cy) ** 2 <= R ** 2; O |= disc
    for rr, w in ((R, 0.2), (R * 0.32, 0.14), (R * 0.68, 0.07)):
        Bm |= r.circle_ring(0, cy, rr, w, 0.85)
    hub = r.X ** 2 + (r.Y - cy) ** 2 <= (R * 0.32) ** 2; r.put(r.dome(hub, 0.95, 0.5, 0.6), hub); Bm |= hub
    r.rose(0, cy, R * 0.26, layers=4, base=0.6, top=1.0, seed=4)
    for i in range(16):
        a = i * math.pi / 8
        m = r.stroke([(math.cos(a) * R * 0.32, cy + math.sin(a) * R * 0.32), (math.cos(a) * R, cy + math.sin(a) * R)], 0.07 if i % 2 else 0.1)
        r.put(r.dome(m, 0.75, 0.14, 0.5, reach=0.04), m); Bm |= m
    for i in range(8):
        a = i * math.pi / 4 + math.pi / 8
        Bm |= r.circle_ring(math.cos(a) * R * 0.84, cy + math.sin(a) * R * 0.84, R * 0.13, 0.05, 0.7)
    for sx in (1, -1):
        for cx in (4.0, 6.15, 8.3, 10.05):
            w = 1.55 if cx < 10 else 0.9
            o, b, apex = window(r, sx * cx, -2.25, w, 0.2 if cx < 10 else 0.6, mullions=1 if cx < 10 else 0, trefoil=cx < 10)
            O |= o; Bm |= b
            pin = r.poly([(sx * cx - 0.09, apex - 0.05), (sx * cx + 0.09, apex - 0.05), (sx * cx, min(apex + 0.5, 2.25))]); r.put(r.dome(pin, 0.8, 0.2, 0.5), pin)
    finish_holes(r, O, Bm)
    # stained glass behind: symmetric rose (colour by ring + alternating petals), diamond quarries in the lancets
    rng = np.random.default_rng(5)
    deep = np.array([[0.55, 0.0, 0.03], [0.32, 0.0, 0.22], [0.12, 0.02, 0.38], [0.8, 0.35, 0.03], [0.65, 0.02, 0.06]])
    ang = (np.arctan2(r.Y - cy, r.X) + math.pi) / (2 * math.pi); rad = np.sqrt(r.X ** 2 + (r.Y - cy) ** 2) / R
    sector = np.floor(ang * 16).astype(int); ring = np.floor(rad * 4).astype(int)
    rose_idx = np.where(ring == 0, 3, np.where(ring == 1, np.where(sector % 2 == 0, 0, 2), np.where(ring == 2, np.where(sector % 2 == 0, 1, 4), 0)))
    u = (r.X + r.Y) / 0.3; v = (r.X - r.Y) / 0.3
    fu = np.floor(u).astype(np.int64); fv = np.floor(v).astype(np.int64)
    hq = ((fu * 73856093) ^ (fv * 19349663)) % 12
    lan_idx = np.where(hq < 6, np.where(hq % 2 == 0, 0, 4), np.where(hq < 9, 1, np.where(hq < 11, 2, 3)))
    inrose = rad <= 1.05
    idx = np.where(inrose, rose_idx, lan_idx)
    img = deep[idx]
    lead = np.where(inrose, ndi.morphological_gradient(sector * 10 + ring, size=3) > 0,
                    (np.abs(u - np.round(u)) < 0.06) | (np.abs(v - np.round(v)) < 0.06))
    img[lead] = 0.015
    tex = ndi.gaussian_filter(rng.standard_normal(r.H.shape), 2.5) * 0.18 + ndi.gaussian_filter(rng.standard_normal(r.H.shape), 12) * 0.25
    img = np.clip(img * (1 + tex[..., None]), 0, 1)
    Image.fromarray((img * 255).astype(np.uint8)).save('art/lilith_glass.png')
    return r


# ------------------------------------------------------------------ HERMES (alchemy + caducei)
def hermes():
    r = Relief(W, H, base=0.1); r.frame(fw=0.32, top=0.6, bead_r=0.045, bead_step=0.16)
    R = 2.08
    r.circle_ring(0, 0, R, 0.12, 0.85); r.circle_ring(0, 0, R - 0.5, 0.06, 0.7)
    for i, ch in enumerate('☉☽☿♀♂♃♄'):
        a = math.pi / 2 + i * 2 * math.pi / 7
        r.glyph(ch, DJ, math.cos(a) * (R - 0.25), math.sin(a) * (R - 0.25), 0.26, 0.85)
    tri = [(math.cos(math.pi / 2 + j * 2 * math.pi / 3) * (R - 0.55), math.sin(math.pi / 2 + j * 2 * math.pi / 3) * (R - 0.55)) for j in range(4)]
    m = r.stroke(tri, 0.07); r.put(r.dome(m, 0.75, 0.1, 0.5, reach=0.035), m)
    s = (R - 0.55) * 0.5
    sq = [(-s, -s), (s, -s), (s, s), (-s, s), (-s, -s)]
    m = r.stroke(sq, 0.06); r.put(r.dome(m, 0.7, 0.1, 0.5, reach=0.03), m)
    r.circle_ring(0, 0, s * 0.95, 0.05, 0.7)
    r.glyph('☉', DJB, 0, 0, 0.62, 0.95)
    r.rays(0, 0, R + 0.08, R + 0.32, 36, 0.06, 0.0, top=0.6)
    for sx in (1, -1):        # caduceus: staff, two serpents, wings, orb
        cx = sx * 6.3
        st = r.stroke([(cx, -2.05), (cx, 1.35)], 0.13); r.put(r.dome(st, 0.75, 0.1, 0.5, reach=0.065), st)
        for ph in (0, math.pi):
            pts = [(cx + 0.42 * math.sin(y * 2.6 + ph) * (0.6 + 0.4 * (y + 2) / 3.4), y) for y in np.linspace(-1.85, 0.95, 260)]
            mm = r.stroke(pts, 0.13, taper=(0.06, 0.15))
            hh = 0.62 + 0.15 * np.cos(r.Y * 2.6 + ph)
            lay = np.where(mm, hh, 0); r.put(lay, mm)
            ex, ey = pts[-1]
            hd = ((r.X - (ex + sx * 0.0)) / 0.16) ** 2 + ((r.Y - (ey + 0.08)) / 0.11) ** 2 <= 1; r.put(r.dome(hd, 0.85, 0.2, 0.5), hd)
        orb = (r.X - cx) ** 2 + (r.Y - 1.5) ** 2 <= 0.2 ** 2; r.put(r.dome(orb, 0.95, 0.4, 0.6), orb)
        for d in (-1, 1):
            for k in range(5):
                L = 1.55 - k * 0.22
                a = math.radians(18 + k * 9)
                p0 = (cx + d * 0.12, 1.15 - k * 0.02); p1 = (cx + d * L * math.cos(a), 1.15 + L * math.sin(a) * 0.55 - k * 0.12)
                r.leaf(p0[0], p0[1], math.atan2(p1[1] - p0[1], p1[0] - p0[0]), math.hypot(p1[0] - p0[0], p1[1] - p0[1]), 0.16, 0.7, 0.12)
        for (x, y) in ((3.4, 1.4), (3.2, -1.3), (9.3, 1.45), (9.6, -1.35), (10.2, 0.1), (3.6, 0.05)):
            r.star(sx * x, y, 0.12, 0.75, n=4 if y else 5)
        r.glyph('☽' if sx < 0 else '☉', DJ, sx * 9.6, 0.15, 0.6, 0.85)
    return r


# ------------------------------------------------------------------ THOTH (tooled-leather grimoire)
def thoth():
    r = Relief(W, H, base=0.12)
    r.frame(fw=0.26, top=0.5, beads=False)
    # stamped lozenge field
    gx = (r.X / 0.55); gy = (r.Y / 0.55)
    dia = (np.abs(((gx + gy) % 1) - 0.5) < 0.035) | (np.abs(((gx - gy) % 1) - 0.5) < 0.035)
    field = (np.abs(r.X) < 10.6) & (np.abs(r.Y) < 2.15)
    r.H[dia & field] -= 0.05
    dots = ((((gx + 0.5) % 1) - 0.5) ** 2 + ((((gy + 0.5) % 1) - 0.5)) ** 2 < 0.012) & field
    r.H[dots] += 0.12
    # border band with tooled dots and fillets
    for inset in (0.45, 0.62):
        dd = np.minimum.reduce([r.X + W / 2, W / 2 - r.X, r.Y + H / 2, H / 2 - r.Y])
        m = np.abs(dd - inset) <= 0.025; r.H[m] -= 0.05
    # brass corner pieces with rivets
    for sx in (1, -1):
        for sy in (1, -1):
            cx, cy = sx * (W / 2 - 0.3), sy * (H / 2 - 0.3)
            m = r.poly([(cx, cy), (cx - sx * 1.6, cy), (cx - sx * 1.25, cy - sy * 0.35), (cx - sx * 0.35, cy - sy * 0.35), (cx - sx * 0.35, cy - sy * 1.25), (cx, cy - sy * 1.6)])
            r.put(r.dome(m, 0.85, 0.5, 0.35, reach=0.12), m)
            for (dx, dy) in ((0.15, 0.15), (1.05, 0.12), (0.12, 1.05)):
                b = (r.X - (cx - sx * dx)) ** 2 + (r.Y - (cy - sy * dy)) ** 2 <= 0.06 ** 2; r.put(r.dome(b, 1.0, 0.8, 0.6), b)
            r.groove(r.stroke([(cx - sx * 0.55, cy - sy * 0.12), (cx - sx * 0.12, cy - sy * 0.55)], 0.03), 0.08, 0.6)
    # central medallion with an ankh, rays and an eye-like lens
    R = 1.75
    disc = r.X ** 2 + r.Y ** 2 <= R ** 2; r.put(np.where(disc, 0.6, 0), disc)
    r.circle_ring(0, 0, R, 0.14, 0.9); r.circle_ring(0, 0, R - 0.3, 0.05, 0.85)
    r.beads([(math.cos(t) * (R - 0.18), math.sin(t) * (R - 0.18)) for t in np.linspace(0, 2 * math.pi, 44, endpoint=False)], 0.04, 0.95)
    r.rays(0, 0, 0.5, R - 0.4, 24, 0.07, 0.0, top=0.78)
    r.glyph('☥', DJB, 0, -0.02, 1.25, 1.0)
    # clasp straps over the top and bottom edge with brass catches
    for sx in (1, -1):
        for sy in (1, -1):
            x = sx * 5.2
            st = (np.abs(r.X - x) < 0.32) & (sy * r.Y > 1.35)
            r.put(np.where(st, 0.5, 0), st)
            r.groove(st & ~ndi.binary_erosion(st, iterations=3), 0.05, 0.6)
            catch = (np.abs(r.X - x) < 0.42) & (np.abs(r.Y - sy * 1.3) < 0.22); r.put(r.dome(catch, 0.95, 0.55, 0.4, reach=0.08), catch)
            b = (r.X - x) ** 2 + (r.Y - sy * 1.3) ** 2 <= 0.07 ** 2; r.H[b] += 0.05
        for (x, y) in ((8.2, 0), (2.9, 0)):
            r.glyph('✦', DJB, sx * x, y, 0.55, 0.9)
        r.glyph('☽' if sx < 0 else '☉', DJ, sx * 9.6, 0.0, 0.6, 0.85)
    return r


# ------------------------------------------------------------------ EUTERPE (gothic pipe organ)
def euterpe():
    r = Relief(W, H, base=0.1); r.frame(fw=0.3, top=0.55, beads=False)
    # three towers of pipes (tall centre), flats between, gothic hood arches over each tower
    def tower(cx, n, w, y0, ymax, ymin):
        rp = w / n / 2 * 0.86
        for i in range(n):
            x = cx - w / 2 + (i + 0.5) * w / n
            t = abs(i - (n - 1) / 2) / ((n - 1) / 2 + 1e-9)
            r.pipe(x, y0, ymax - (ymax - ymin) * t ** 1.6, rp, top=0.92)
    tower(0, 9, 4.2, -2.0, 1.3, 0.3)
    for sx in (1, -1):
        tower(sx * 4.5, 7, 3.1, -2.0, 0.75, -0.05)
        tower(sx * 8.45, 9, 3.4, -2.0, 1.0, 0.15)
        for px_ in (2.5, 6.5, 10.35):
            col = (np.abs(r.X - sx * px_) < 0.09) & (r.Y > -2.0) & (r.Y < 1.3)
            r.put(np.where(col, 0.12 + 0.5 * np.sqrt(np.clip(1 - ((r.X - sx * px_) / 0.09) ** 2, 0, 1)), 0), col)
            pin = r.poly([(sx * px_ - 0.13, 1.3), (sx * px_ + 0.13, 1.3), (sx * px_, 1.95)]); r.put(r.dome(pin, 0.75, 0.2, 0.5), pin)
            for t in (0.2, 0.4):
                for d in (-1, 1):
                    b = (r.X - (sx * px_ + d * 0.13 * (1 - t * 1.6))) ** 2 + (r.Y - (1.3 + t * 0.65)) ** 2 <= 0.045 ** 2; r.put(r.dome(b, 0.7, 0.2, 0.5), b)
    # tracery frieze of small pointed arches along the top
    k = -10.3
    while k < 10.3:
        a, ap = arch_pts(k + 0.35, 1.55, 0.6, 1.75, 1.0, n=20)
        m = r.stroke(a[1:-1], 0.05); r.put(r.dome(m, 0.55, 0.1, 0.5, reach=0.025), m)
        r.circle_ring(k + 0.35, 1.82, 0.08, 0.03, 0.5)
        k += 0.7
    m = r.stroke([(-10.6, 1.5), (10.6, 1.5)], 0.08); r.put(r.dome(m, 0.6, 0.1, 0.5, reach=0.04), m)
    m = r.stroke([(-10.6, -2.08), (10.6, -2.08)], 0.12); r.put(r.dome(m, 0.6, 0.1, 0.5, reach=0.06), m)
    return r


if __name__ == '__main__':
    name = sys.argv[1]
    r = globals()[name]()
    hp = f'art/{name}_h.png'
    r.save(hp, f'art/{name}_holes.png' if r.holes.any() else None, blur=0.8)
    print('saved', name, r.holes.any())
