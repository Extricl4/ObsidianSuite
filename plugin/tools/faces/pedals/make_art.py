#!/usr/bin/env python3
"""Draws the printed artwork of the plugin's own pedal designs (this folder), for build_pedals.py.

    python3 plugin/tools/faces/pedals/make_art.py

nampedal (WRAITH, the NAM capture pedal): a mini enclosure, knob labels, the name under its screen.
Writes art/<key>_art.png (the print) and art/<key>_glow.png (what lights up when the pedal is on),
in the design's units: 200 per cm.
"""
import io
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageFilter

HERE = Path(__file__).resolve().parent
FONTS = HERE.parents[3] / 'ui' / 'fonts'
U = 200                                   # design units per cm


def font(name, size):
    """The UI's own fonts (woff2), opened through fontTools."""
    from fontTools.ttLib import TTFont
    f = TTFont(str(FONTS / f'{name}.woff2')); f.flavor = None
    buf = io.BytesIO(); f.save(buf); buf.seek(0)
    return ImageFont.truetype(buf, size)


def spaced(d, cx, y, text, fnt, fill, track):
    """Centred text with letter spacing (track: extra units between letters)."""
    widths = [d.textlength(ch, font=fnt) for ch in text]
    x = cx - (sum(widths) + track * (len(text) - 1)) / 2
    for ch, w in zip(text, widths):
        d.text((x, y), ch, font=fnt, fill=fill, anchor='ls'); x += w + track


def nampedal():
    W, H = int(3.9 * U), int(9.3 * U)
    art = Image.new('RGBA', (W, H), (0, 0, 0, 0)); glow = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    d, g = ImageDraw.Draw(art), ImageDraw.Draw(glow)
    silver, dim, cyan = (206, 210, 218, 255), (122, 126, 136, 255), (90, 225, 255, 255)
    spaced(d, W / 2, 140, 'OBSIDIAN', font('rajdhani-600', 38), dim, 14)
    for x, t in ((150, 'IN'), (390, 'OUT'), (630, 'MIX')):            # under the three knobs (y 270)
        spaced(d, x, 410, t, font('rajdhani-600', 44), silver, 8)
    # the screen sits at y 490..1030; the name under it
    spaced(d, W / 2, 1172, 'WRAITH', font('cinzel-700', 104), silver, 10)
    spaced(g, W / 2, 1172, 'WRAITH', font('cinzel-700', 104), cyan, 10)
    spaced(d, W / 2, 1240, 'NAM  CAPTURE', font('rajdhani-600', 38), dim, 16)
    # engraved ring around the footswitch (centre y 1590)
    for im, col, wd in ((d, (0, 0, 0, 150), 7), (d, (150, 155, 165, 90), 3)):
        im.ellipse([390 - 190, 1590 - 190, 390 + 190, 1590 + 190], outline=col, width=wd)
    spaced(d, W / 2, 1830, 'BYPASS', font('rajdhani-500', 34), dim, 12)
    glow = glow.filter(ImageFilter.GaussianBlur(10))
    (HERE / 'art').mkdir(exist_ok=True)
    art.save(HERE / 'art' / 'nampedal_art.png'); glow.save(HERE / 'art' / 'nampedal_glow.png')
    print('art/nampedal_art.png, art/nampedal_glow.png')


if __name__ == '__main__':
    nampedal()
