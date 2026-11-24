#!/usr/bin/env python3
"""Finishes a top view (render.py ... top): cuts the corners to the enclosure's rounded corners (full-bleed artwork
is a flat sheet that would stick out past them) and saves renders/top/<key>_<state>.webp.

    python3 render/top_finish.py pedals/bloom.json on
"""
import json, os, sys
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
spec_path, state = sys.argv[1], sys.argv[2]
key = os.path.splitext(os.path.basename(spec_path))[0]
spec = json.load(open(spec_path))
img = Image.open(os.path.join(ROOT, 'out', 'top', f'{key}_{state}.png')).convert('RGBA')
w, h = img.size
px = w / (spec.get('size') or [6.6])[0]                      # pixels per cm
r = spec.get('bevel', 0.35) * px                             # the enclosure's corner radius in pixels
ss = 4                                                       # supersampled edge
y, x = np.mgrid[0:h * ss, 0:w * ss].astype(np.float32) / ss + 0.5 / ss
dx = np.maximum(np.maximum(r - x, x - (w - r)), 0); dy = np.maximum(np.maximum(r - y, y - (h - r)), 0)
inside = (np.hypot(dx, dy) <= r).astype(np.float32)
mask = inside.reshape(h, ss, w, ss).mean(axis=(1, 3))
a = np.asarray(img).astype(np.float32)
a[..., 3] *= mask
os.makedirs(os.path.join(ROOT, 'renders', 'top'), exist_ok=True)
Image.fromarray(a.astype(np.uint8), 'RGBA').save(os.path.join(ROOT, 'renders', 'top', f'{key}_{state}.webp'), 'WEBP', quality=92, method=6)
