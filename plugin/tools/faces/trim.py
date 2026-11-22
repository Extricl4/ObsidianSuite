"""Trims an amp face for the plugin: no carrying handle, no backdrop.

The renders in visuals/finished stand on a dark studio backdrop, and most heads have a carrying
handle on top. In the plugin that backdrop shows as a dark box around the amp (with the handle
inside it), so the face is cut to the amp itself:

  * the top is cut where the head's body starts (the handle and the backdrop above go);
    amps whose top is part of the design (Eos: its tubes stand on the chassis) keep it,
  * the backdrop that's left (rounded corners, the shadow under the amp, the space around Eos'
    tubes) becomes transparent, so the plugin's own background and theme show right up to the amp,
  * the picture is cut to what's left, and every position (knobs, LEDs, buttons, power) moves with it.
"""
import numpy as np
from PIL import Image
from scipy import ndimage

KEEP_TOP = {'eos'}          # amps whose top isn't a handle
SHAPE = {'eos': 'outline', 'eris': 'slant'}  # default: the head's box with rounded corners
RADIUS = 12                 # corner radius of the box, in render pixels
CAP = 24                    # how far corner caps may stand out of the box
CUT_BOTTOM = {'eos': 0.905}  # Eos stands on a glossy floor: cut under its feet (fraction of the height)
TOL = 34                    # |r-bg|+|g-bg|+|b-bg| above this is the amp
BOX_TOL = 24                # the same for counting rows / columns (the backdrop's shadow stays under it)


def _bg(a):
    c = np.concatenate([a[:6, :6].reshape(-1, 3), a[:6, -6:].reshape(-1, 3)])
    return np.median(c, 0)


def body_top(a):
    """First row of the head's body: the handle above is a narrow strip, the body spans the width."""
    d = np.abs(a - _bg(a)).sum(2)
    rows = (d > TOL).mean(1)
    return int(np.argmax(rows > 0.5))


def alpha(a):
    """The amp's outline -> opaque, the backdrop around it -> transparent.

    The outline is traced from the outside only (per row: leftmost..rightmost amp pixel, per column:
    topmost..bottommost; a pixel is the amp's if it's inside both), so dark parts of the amp itself
    (black panels, tolex) never turn transparent."""
    d = np.abs(a - _bg(a)).sum(2)
    m = ndimage.binary_opening(d > TOL, iterations=2)
    H, W = m.shape
    def spans(mm, n):
        lo = np.full(n, -1.0); hi = np.full(n, -2.0)
        anyv = mm.any(1)
        idx = np.where(anyv)[0]
        lo[idx] = mm[idx].argmax(1)
        hi[idx] = mm.shape[1] - 1 - mm[idx][:, ::-1].argmax(1)
        lo = ndimage.median_filter(lo, 5, mode='nearest'); hi = ndimage.median_filter(hi, 5, mode='nearest')
        return lo, hi
    L, R = spans(m, H)
    T, B = spans(m.T, W)
    y = np.arange(H)[:, None]; x = np.arange(W)[None, :]
    inside = (x >= L[:, None]) & (x <= R[:, None]) & (y >= T[None, :]) & (y <= B[None, :])
    return ndimage.gaussian_filter(inside.astype(np.float32), 0.8)


def box_mask(a, slant=False):
    """The head's box: rows and columns where most of the picture is amp, with rounded corners.
    Counting whole rows / columns is robust where the amp's own edge is as dark as the backdrop.
    With a top drawn in perspective the columns are counted on the front panel only (lower half,
    finer threshold), so the narrower top doesn't narrow the box."""
    dd = np.abs(a - _bg(a)).sum(2)
    d = dd > BOX_TOL
    rows = np.where(d.mean(1) > 0.5)[0]
    y0, y1 = rows[0], rows[-1] + 1
    cols = np.where(((dd > 12)[(y0 + y1) // 2:y1] if slant else d[y0:y1]).mean(0) > 0.5)[0]
    x0, x1 = cols[0], cols[-1] + 1
    H, W = d.shape
    y = np.arange(H)[:, None] + .5; x = np.arange(W)[None, :] + .5
    cx = np.clip(x, x0 + RADIUS, x1 - RADIUS); cy = np.clip(y, y0 + RADIUS, y1 - RADIUS)
    dist = np.hypot(x - cx, y - cy)
    return np.clip(RADIUS + .5 - dist, 0, 1).astype(np.float32), (x0, x1)


def slant_mask(a, x0, x1):
    """A top drawn in perspective (Eris): the top's sides are straight lines that lean out down to
    the front panel. Measured on the top rows, fitted with a line each, anti-aliased."""
    d = ndimage.binary_opening(np.abs(a - _bg(a)).sum(2) > 18, iterations=2)
    H, W = d.shape
    ys, L, R = [], [], []
    for y in range(0, int(H * .45), 3):
        xs = np.where(d[y])[0]
        if len(xs) > W * .5:
            ys.append(y); L.append(xs[0]); R.append(xs[-1] + 1)
    ys, L, R = map(np.array, (ys, L, R))
    y = np.arange(H)[:, None] + .5; x = np.arange(W)[None, :] + .5
    out = np.ones((H, W), np.float32)
    sel = L > x0 + 3
    if sel.sum() > 8:
        pl = np.polyfit(ys[sel], L[sel], 1); out = np.minimum(out, np.clip(x - np.polyval(pl, y) + .5, 0, 1))
    sel = R < x1 - 3
    if sel.sum() > 8:
        pr = np.polyfit(ys[sel], R[sel], 1); out = np.minimum(out, np.clip(np.polyval(pr, y) - x + .5, 0, 1))
    return out


def trim(key, bases, mask_from=None):
    """bases: {name: PIL image}. Returns ({name: RGBA image}, (dx, dy), (W, H)).
    mask_from: another render of a head with the same size and camera whose outline is used instead (for a head whose
    backdrop is too close to its own colour to find its edges, Phlegethon's dark brown on brown)."""
    arrs = {n: np.asarray(im.convert('RGB')).astype(np.float32) for n, im in bases.items()}
    if mask_from is not None:
        ref = np.asarray(mask_from.convert('RGB')).astype(np.float32)
        top = max(0, body_top(ref) - CAP)
        al_ref, _ = box_mask(ref[top:])
        zone = ndimage.binary_dilation(al_ref > .5, iterations=CAP)
        xs = np.arange(al_ref.shape[1])[None, :]
        x0 = np.where((al_ref > .5).any(0))[0][0]; x1 = np.where((al_ref > .5).any(0))[0][-1] + 1
        zone &= (xs < x0 + 4 * CAP) | (xs >= x1 - 4 * CAP)
        al_ref = np.maximum(al_ref, alpha(ref[top:]) * zone)
        ys, xs_ = np.where(al_ref > 0.02)
        y0, y1, bx0, bx1 = ys.min(), ys.max() + 1, xs_.min(), xs_.max() + 1
        res = {}
        for n, a in arrs.items():
            rgba = np.dstack([a[top:], al_ref[..., None] * 255])[y0:y1, bx0:bx1]
            res[n] = Image.fromarray(np.clip(rgba + .5, 0, 255).astype(np.uint8), 'RGBA')
        return res, (int(bx0), int(top + y0)), (int(bx1 - bx0), int(y1 - y0))
    H0 = next(iter(arrs.values())).shape[0]
    # keep a few rows above the body: corner caps that stand out of the box (Lamia, Erebus...) stay whole
    top = 0 if key in KEEP_TOP else max(0, min(body_top(a) for a in arrs.values()) - CAP)
    bot = int(H0 * CUT_BOTTOM[key]) if key in CUT_BOTTOM else H0
    shape = SHAPE.get(key, 'box')
    out, box = {}, None
    for n, a in arrs.items():
        a = a[top:bot]
        if shape == 'outline':
            al = alpha(a)
        else:
            al, (x0, x1) = box_mask(a, shape == 'slant')
            # bright parts just outside the box's corners (chrome corner caps) belong to the amp too
            zone = ndimage.binary_dilation(al > .5, iterations=CAP)
            xs = np.arange(al.shape[1])[None, :]
            zone &= (xs < x0 + 4 * CAP) | (xs >= x1 - 4 * CAP)
            al = np.maximum(al, alpha(a) * zone)
            if shape == 'slant':
                al = np.minimum(al, slant_mask(a, x0, x1))
        if key in CUT_BOTTOM:            # fade the last rows instead of a hard cut through the floor
            f = min(24, al.shape[0]); al[-f:] *= np.linspace(1, 0, f)[:, None]
        out[n] = (a, al)
        ys, xs = np.where(al > 0.02)
        b = (xs.min(), ys.min(), xs.max() + 1, ys.max() + 1)
        box = b if box is None else (min(box[0], b[0]), min(box[1], b[1]), max(box[2], b[2]), max(box[3], b[3]))
    x0, y0, x1, y1 = box
    res = {}
    for n, (a, al) in out.items():
        rgba = np.dstack([a, al[..., None] * 255])[y0:y1, x0:x1]
        res[n] = Image.fromarray(np.clip(rgba + .5, 0, 255).astype(np.uint8), 'RGBA')
    return res, (int(x0), int(top + y0)), (int(x1 - x0), int(y1 - y0))


def move(face, dx, dy, W, H):
    """Moves every position in a face by (-dx, -dy) and sets its new size."""
    face['W'], face['H'] = W, H
    for k in face['knobs']:
        k['b'] = [k['b'][0] - dx, k['b'][1] - dy, k['b'][2], k['b'][3]]
        k['c'] = [round(k['c'][0] - dx, 1), round(k['c'][1] - dy, 1)]
    for d in (face['leds'], face['buttons']):
        for i, v in d.items():
            d[i] = [round(v[0] - dx, 1), round(v[1] - dy, 1)] + list(v[2:])
    p = face['power']
    face['power'] = [round(p[0] - dx, 1), round(p[1] - dy, 1)] + list(p[2:])
