"""Tileable grille-cloth textures for render/cab.py.

    python3 tools/cab_weave.py            # writes every cloth in CLOTHS
    python3 tools/cab_weave.py jute       # just one

Each cloth writes two images into art/:
  weave_<name>.png    RGBA: thread colour, alpha = thread coverage (the gaps let you see the speakers)
  weave_<name>_h.png  16-bit height (where threads cross over / under), used as bump
The pattern is periodic, so the tile repeats without a seam. cab.py maps one tile to `tile_cm` centimetres.
"""
import os, sys
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

CLOTHS = {
    # Mesa-style black jute: coarse plain weave (1 over 1), fuzzy, uneven threads, brownish-black
    'jute': dict(n=40, px=22, weave='plain', cover=0.64, jitter=0.12, wobble=0.10, fuzz=0.9, twist=1.0,
                 base=(0.016, 0.0145, 0.013), hi=(0.062, 0.055, 0.048), streak=0.55, seed=3),
    # salt & pepper: black and light-grey threads in pairs, a fine checkerboard (vintage British cabs)
    'checker': dict(n=64, px=12, weave='plain', cover=0.80, jitter=0.04, wobble=0.04, fuzz=0.3, twist=0.3, shade=0.05,
                    base=(0.010, 0.010, 0.011), hi=(0.045, 0.045, 0.047), base2=(0.05, 0.05, 0.049), hi2=(0.20, 0.195, 0.185),
                    pattern='checker', streak=0.3, seed=11),
    # brown cloth with a tan diamond lattice (Vox-style)
    'diamond': dict(n=72, px=12, weave='basket', cover=0.82, jitter=0.04, wobble=0.03, fuzz=0.3, twist=0.3, shade=0.05,
                    base=(0.022, 0.011, 0.005), hi=(0.08, 0.042, 0.02), base2=(0.06, 0.038, 0.018), hi2=(0.22, 0.15, 0.075),
                    pattern='diamond', diamond=24, line=0.07, streak=0.3, seed=13),
    # wheat / beige basket weave (Orange-style)
    'wheat': dict(n=64, px=12, weave='basket', cover=0.82, jitter=0.04, wobble=0.04, fuzz=0.3, twist=0.35, shade=0.06,
                  base=(0.09, 0.07, 0.042), hi=(0.36, 0.29, 0.18), streak=0.35, seed=17),
    # Marshall-style black grille cloth: fine 2x2 basket weave, tight, a little sheen
    'basket': dict(n=64, px=12, weave='basket', cover=0.80, jitter=0.04, wobble=0.04, fuzz=0.25, twist=0.35,
                   base=(0.012, 0.012, 0.013), hi=(0.055, 0.055, 0.058), streak=0.35, seed=7),
    # blackface-style silver cloth: light silver threads with a dark teal-black pinstripe every 4th thread
    'silver': dict(n=64, px=12, weave='plain', cover=0.80, jitter=0.03, wobble=0.03, fuzz=0.25, twist=0.3, shade=0.04,
                   base=(0.11, 0.115, 0.115), hi=(0.42, 0.43, 0.42), base2=(0.025, 0.038, 0.042), hi2=(0.09, 0.12, 0.13),
                   pattern='pin', period=4, streak=0.25, seed=19),
    # tweed-era grille cloth: brown with gold stripes running down
    'stripe': dict(n=72, px=12, weave='plain', cover=0.84, jitter=0.04, wobble=0.04, fuzz=0.3, twist=0.3, shade=0.05,
                   base=(0.026, 0.014, 0.007), hi=(0.10, 0.058, 0.03), base2=(0.10, 0.07, 0.032), hi2=(0.40, 0.29, 0.13),
                   pattern='stripe', period=6, width=2, streak=0.3, seed=23),
    # lacquered tweed covering (2/2 twill linen): light warp, darker weft, so the twill shows as diagonal lines
    'tweed': dict(n=48, px=10, weave='twill', cover=0.97, jitter=0.05, wobble=0.03, fuzz=0.2, twist=0.4, shade=0.07,
                  base=(0.10, 0.062, 0.026), hi=(0.36, 0.25, 0.12), base2=(0.05, 0.028, 0.012), hi2=(0.17, 0.10, 0.045),
                  pattern='warpweft', streak=0.4, seed=29),
}


def periodic_noise(shape, rng, scale):
    """Smooth random field that tiles: white noise low-passed in the Fourier domain."""
    h, w = shape
    f = np.fft.fft2(rng.standard_normal((h, w)))
    ky = np.fft.fftfreq(h)[:, None]; kx = np.fft.fftfreq(w)[None, :]
    g = np.exp(-((kx * scale[0]) ** 2 + (ky * scale[1]) ** 2))
    n = np.real(np.fft.ifft2(f * g))
    return (n - n.mean()) / (n.std() + 1e-9)


def periodic_wave(length, rng, k=3):
    """1-D smooth periodic wobble (sum of a few integer-frequency sines), unit amplitude."""
    t = np.arange(length) / length
    s = np.zeros(length)
    for f in range(1, k + 1):
        s += rng.normal() / f * np.sin(2 * np.pi * f * t + rng.uniform(0, 2 * np.pi))
    return s / (np.abs(s).max() + 1e-9)


def make(name, c):
    rng = np.random.default_rng(c['seed'])
    n, P = c['n'], c['px']; S = n * P
    yy, xx = np.mgrid[0:S, 0:S].astype(np.float64) + 0.5

    def threads(coord, along, vertical):
        """Height (0..1) of the n parallel threads crossing `coord`; returns (h, idx)."""
        h = np.zeros((S, S)); idx = np.full((S, S), -1); acr = np.zeros((S, S))
        for i in range(n):
            centre = (i + 0.5) * P + c['wobble'] * P * periodic_wave(S, rng)[along.astype(int) % S]
            width = P * c['cover'] * (1 + c['jitter'] * rng.normal())
            width = width * (1 + 0.5 * c['jitter'] * periodic_wave(S, rng)[along.astype(int) % S])
            d = coord - centre
            d = (d + S / 2) % S - S / 2          # wrap so the threads tile
            r = np.clip(1 - (2 * d / width) ** 2, 0, None)
            prof = np.sqrt(r)
            better = prof > h
            h = np.where(better, prof, h); idx = np.where(better, i, idx); acr = np.where(better, d / width, acr)
        return h, idx, acr

    warp_h, warp_i, warp_a = threads(xx, yy, True)     # vertical threads
    weft_h, weft_i, weft_a = threads(yy, xx, False)    # horizontal threads

    # over / under: which thread is on top in each crossing cell
    cx = np.floor(xx / P).astype(int) % n; cy = np.floor(yy / P).astype(int) % n
    if c['weave'] == 'basket':
        warp_top = ((cx // 2 + cy // 2) % 2) == 0
    elif c['weave'] == 'twill':          # 2/2 twill: each thread goes over two, under two, shifted one per row
        warp_top = ((cx - cy) % 4) < 2
    else:
        warp_top = ((cx + cy) % 2) == 0
    # smooth undulation: a thread rises where it's on top and dips where it's under
    fy = (yy / P) % 1.0; fx = (xx / P) % 1.0
    bump_y = 0.5 - 0.5 * np.cos(2 * np.pi * fy)   # 0 at cell edges, 1 in the middle
    bump_x = 0.5 - 0.5 * np.cos(2 * np.pi * fx)
    warp_m = np.where(warp_top, 0.55 + 0.45 * bump_y, 0.55 - 0.35 * bump_y)
    weft_m = np.where(~warp_top, 0.55 + 0.45 * bump_x, 0.55 - 0.35 * bump_x)
    wh = warp_h * warp_m; fh = weft_h * weft_m
    height = np.maximum(wh, fh)
    on_warp = wh >= fh

    # coverage with a soft, slightly fuzzy edge
    fuzz = periodic_noise((S, S), rng, (2.0, 2.0)) * 0.08 * c['fuzz']
    cov = np.clip(np.maximum(warp_h, weft_h) * 5.0 + fuzz - 0.1, 0, 1)

    # colour: per-thread shade + fibre streaks running along each thread
    shade_w = 1 + c.get('shade', 0.08) * rng.standard_normal(n); shade_f = 1 + c.get('shade', 0.08) * rng.standard_normal(n)
    streak_v = periodic_noise((S, S), rng, (60.0, 1.2))     # long along y -> for warp
    streak_h = periodic_noise((S, S), rng, (1.2, 60.0))     # long along x -> for weft
    streak = np.where(on_warp, streak_v, streak_h)
    shade = np.where(on_warp, shade_w[np.clip(warp_i, 0, n - 1)], shade_f[np.clip(weft_i, 0, n - 1)])
    # twisted strands: diagonal striations along each thread, rounded cross-section catches the light
    across = np.where(on_warp, warp_a, weft_a); along = np.where(on_warp, yy, xx)
    phase = np.where(on_warp, np.clip(warp_i, 0, n - 1), np.clip(weft_i, 0, n - 1)) * 1.7
    tw = 0.5 + 0.5 * np.sin(2 * np.pi * (along / (P * 0.85) + across * 1.6) + phase)
    prof = np.maximum(warp_h, weft_h)
    m = np.where(on_warp, warp_m, weft_m)
    t = (np.power(prof, 0.8) * (0.45 + 0.55 * m) * (1 - 0.35 * c['twist'] + 0.35 * c['twist'] * tw)
         + 0.12 * streak * c['streak']) * np.clip(shade, 0.6, 1.5)
    t = np.clip(t, 0, 1.4)
    base = np.array(c['base']); hi = np.array(c['hi'])
    col = base[None, None, :] + (hi - base)[None, None, :] * t[..., None]
    pat = c.get('pattern')
    if pat:
        wi = np.clip(warp_i, 0, n - 1); fi = np.clip(weft_i, 0, n - 1)
        if pat == 'checker':     # thread colour by thread index, in pairs: crossings make the checkerboard
            light = np.where(on_warp, (wi // 2) % 2 == 0, (fi // 2) % 2 == 0).astype(float)
        elif pat == 'pin':       # every `period`-th thread both ways is the second colour (a fine pinstripe grid)
            light = np.where(on_warp, wi % c['period'] == 0, fi % c['period'] == 0).astype(float)
        elif pat == 'stripe':    # `width` warp threads in every `period` are the second colour: stripes running down
            light = (on_warp & (wi % c['period'] < c['width'])).astype(float)
        elif pat == 'warpweft':  # warp in the first colour, weft in the second
            light = (~on_warp).astype(float)
        else:                    # diamond lattice laid over the weave
            L = c['diamond'] * P; u = (xx + yy) / L; v = (xx - yy) / L
            du = np.abs((u % 1.0) - 0.5); dv = np.abs((v % 1.0) - 0.5)
            light = np.clip((c['line'] - np.minimum(0.5 - du, 0.5 - dv)) / 0.03, 0, 1)
            light = np.maximum(light, np.clip((c['line'] - np.minimum(du, dv)) / 0.03, 0, 1) * 0)
        b2 = np.array(c['base2']); h2 = np.array(c['hi2'])
        col2 = b2[None, None, :] + (h2 - b2)[None, None, :] * t[..., None]
        col = col * (1 - light[..., None]) + col2 * light[..., None]
    # linear -> sRGB for the colour image
    srgb = np.where(col <= 0.0031308, col * 12.92, 1.055 * np.power(np.clip(col, 0, None), 1 / 2.4) - 0.055)
    rgba = np.dstack([np.clip(srgb, 0, 1), cov])
    os.makedirs(os.path.join(ROOT, 'art'), exist_ok=True)
    Image.fromarray((rgba * 255 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, 'art', f'weave_{name}.png'))
    h16 = (np.clip(height, 0, 1) * 65535 + 0.5).astype(np.uint16)
    Image.fromarray(h16).save(os.path.join(ROOT, 'art', f'weave_{name}_h.png'))
    print(f'weave_{name}: {S}px tile, {n} threads, open area {100 * (1 - cov.mean()):.1f}%')


PERFS = {
    # name: hole shape, pitch (px), hole size as a fraction of the pitch, tile holes
    'perf_round': dict(shape='round', pitch=40, size=0.62, n=24),
    'perf_hex': dict(shape='hex', pitch=40, size=0.78, n=24),
    'perf_square': dict(shape='square', pitch=40, size=0.66, n=24),
    'perf_slot': dict(shape='slot', pitch=40, size=0.5, n=24),
}


def make_perf(name, c):
    """Perforated sheet metal: alpha 0 in the holes, height rolls over at the hole edges. Staggered rows."""
    P = c['pitch']; n = c['n']; Sx = n * P; rowh = P * (np.sqrt(3) / 2 if c['shape'] in ('round', 'hex') else 1.0)
    rows = int(round(Sx / rowh)); rows += rows % 2; Sy = int(round(rows * rowh)); rowh = Sy / rows
    yy, xx = np.mgrid[0:Sy, 0:Sx].astype(np.float64) + 0.5
    r = np.floor(yy / rowh); off = np.where((r % 2) == 1, P / 2, 0) if c['shape'] in ('round', 'hex') else 0
    cx = ((xx - off) % P) - P / 2; cy = (yy % rowh) - rowh / 2
    a = c['size'] * P / 2
    if c['shape'] == 'round': d = np.sqrt(cx ** 2 + cy ** 2) / a
    elif c['shape'] == 'hex':
        q = np.abs(np.stack([cx, cx * 0.5 + cy * np.sqrt(3) / 2, -cx * 0.5 + cy * np.sqrt(3) / 2]))
        d = q.max(0) / (a * np.sqrt(3) / 2)
    elif c['shape'] == 'slot': d = np.maximum(np.abs(cx) / (a * 0.45), np.abs(cy) / (a * 1.5 * rowh / P))
    else: d = np.maximum(np.abs(cx), np.abs(cy)) / a
    alpha = np.clip((d - 1.0) / 0.06 + 0.5, 0, 1)
    height = np.clip((d - 1.0) / 0.35, 0, 1) ** 0.5
    rgba = np.dstack([np.ones_like(alpha) * 0.5] * 3 + [alpha])
    Image.fromarray((rgba * 255 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, 'art', f'{name}.png'))
    Image.fromarray((height * 65535 + 0.5).astype(np.uint16)).save(os.path.join(ROOT, 'art', f'{name}_h.png'))
    print(f'{name}: {Sx}x{Sy}px, open area {100 * (1 - alpha.mean()):.0f}%')


if __name__ == '__main__':
    if sys.argv[1:2] == ['perf']:
        for nm, c in PERFS.items(): make_perf(nm, c)
        sys.exit()
    names = sys.argv[1:] or list(CLOTHS)
    for nm in names:
        make(nm, CLOTHS[nm])
