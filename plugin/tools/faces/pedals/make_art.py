#!/usr/bin/env python3
"""Draws the printed artwork of the plugin's own pedal designs (this folder), for build_pedals.py.

    python3 plugin/tools/faces/pedals/make_art.py

nampedal (WRAITH, the NAM capture pedal): a tiny rack-style box in Eidolon's look, mostly screen.
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
    """WRAITH: a tiny rack-style box in Eidolon's look, mostly screen (6.0 x 8.4 cm)."""
    import math
    W, H = int(6.0 * U), int(8.4 * U)
    art = Image.new('RGBA', (W, H), (0, 0, 0, 0)); glow = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    d, g = ImageDraw.Draw(art), ImageDraw.Draw(glow)
    ice, light, grey, dim = (110, 205, 255, 255), (205, 208, 214, 255), (140, 143, 150, 255), (88, 91, 98, 255)
    # rack handles on both sides
    for x in (46, W - 46):
        for col, wd in (((0, 0, 0, 170), 30), ((70, 72, 78, 255), 22), ((150, 154, 162, 255), 8)):
            d.line([(x, 250), (x, 1250)], fill=col, width=wd)
        for y in (250, 1250): d.ellipse([x - 20, y - 20, x + 20, y + 20], fill=(40, 41, 45, 255), outline=(120, 124, 132, 255), width=3)
    # title, like Eidolon's: spaced, ice blue, a rule under it
    spaced(d, 330, 150, 'WRAITH', font('orbitron-500', 74), ice, 18)
    spaced(g, 330, 150, 'WRAITH', font('orbitron-500', 74), ice, 18)
    spaced(d, 330, 205, 'NAM CAPTURE PEDAL', font('rajdhani-600', 30), grey, 7)
    d.line([(120, 236), (545, 236)], fill=(60, 62, 68, 255), width=3)
    # LOAD bar button (top right), with its light bar
    spaced(d, 905, 108, 'LOAD', font('orbitron-500', 30), light, 6)
    bx0, by0, bx1, by1 = 775, 128, 1035, 218
    d.rounded_rectangle([bx0 - 6, by0 - 6, bx1 + 6, by1 + 6], 14, fill=(0, 0, 0, 200))
    for k in range(by1 - by0):
        t = k / (by1 - by0); c = int(44 - 30 * t)
        d.line([(bx0 + 4, by0 + k), (bx1 - 4, by0 + k)], fill=(c, c, c + 3, 255))
    d.rounded_rectangle([bx0, by0, bx1, by1], 10, outline=(90, 92, 98, 255), width=3)
    d.line([(bx0 + 70, by1 - 26), (bx1 - 70, by1 - 26)], fill=(235, 240, 245, 255), width=6)
    g.line([(bx0 + 70, by1 - 26), (bx1 - 70, by1 - 26)], fill=(180, 230, 255, 255), width=10)
    # the screen sits at y 290..890 (drawn by build_pedals.py)
    # three knobs with 0..10 scales, like Eidolon's
    for x, label in ((300, 'INPUT'), (600, 'OUTPUT'), (900, 'MIX')):
        cy = 1095
        spaced(d, x, 985, label, font('orbitron-500', 30), light, 5)
        for v in range(11):
            ang = math.radians(225 - 27 * v)
            r0, r1 = 74, 88 if v % 5 else 96
            d.line([(x + math.cos(ang) * r0, cy - math.sin(ang) * r0), (x + math.cos(ang) * r1, cy - math.sin(ang) * r1)], fill=light, width=4)
            if v in (0, 10):
                tx, ty = x + math.cos(ang) * 122, cy - math.sin(ang) * 122
                d.text((tx, ty), str(v), font=font('rajdhani-600', 30), fill=grey, anchor='mm')
    # footswitch ring and BYPASS
    for col, wd in (((0, 0, 0, 150), 7), ((120, 124, 132, 110), 3)):
        d.ellipse([600 - 150, 1440 - 150, 600 + 150, 1440 + 150], outline=col, width=wd)
    spaced(d, 600, 1630, 'BYPASS', font('orbitron-500', 26), dim, 8)
    glow = glow.filter(ImageFilter.GaussianBlur(9))
    (HERE / 'art').mkdir(exist_ok=True)
    art.save(HERE / 'art' / 'nampedal_art.png'); glow.save(HERE / 'art' / 'nampedal_glow.png')
    print('art/nampedal_art.png, art/nampedal_glow.png')


if __name__ == '__main__':
    nampedal()
