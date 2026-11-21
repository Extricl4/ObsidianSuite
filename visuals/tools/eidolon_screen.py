"""Screen UI for Eidolon, the NAM capture amp head (loads amp captures only)."""
import math, numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

FD = __file__.rsplit('/', 2)[0] + '/render/fonts/'
MONOB = '/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf'
SW_IN, SH_IN = 13.6, 4.3
PPI = 220; S = 2
W, H = int(SW_IN * PPI * S), int(SH_IN * PPI * S)
BG = (6, 9, 13); PANE = (11, 16, 23); LINE = (34, 46, 60); DIM = (96, 116, 136); TXT = (216, 228, 240)
ACC = (110, 205, 255); ACC2 = (60, 140, 220); AMB = (255, 176, 72); RED = (255, 80, 70); GRN = (90, 230, 150)


def F(name, px, w=None):
    f = ImageFont.truetype(name if name.startswith('/') else FD + name, int(px * S))
    if w is not None:
        try:
            ax = f.get_variation_axes()
            if ax: f.set_variation_by_axes([w] + [a['default'] for a in ax[1:]])
        except Exception:
            pass
    return f


def X(px): return int(px * S)


img = Image.new('RGB', (W, H), BG); d = ImageDraw.Draw(img)
MICH = 'Michroma-Regular.ttf.ttf'
title = F(MICH, 30); h1 = F(MICH, 78); body = F('Saira.ttf', 30, 500); bodyb = F('Saira.ttf', 30, 700)
small = F('Saira.ttf', 23, 600); tiny = F('Saira.ttf', 20, 600); monob = F(MONOB, 26)


def rr(box, r, fill=None, outline=None, width=1):
    d.rounded_rectangle([int(v) for v in box], radius=int(r), fill=fill, outline=outline, width=int(width))


def spaced(xy, t, f, fill, sp=0):
    x, y = xy
    for c in t:
        d.text((x, y), c, font=f, fill=fill, anchor='lm'); x += d.textlength(c, font=f) + sp


TOP = X(80); BOT = X(64)
d.rectangle([0, 0, W, TOP], fill=(9, 13, 19)); d.line([0, TOP, W, TOP], fill=LINE, width=X(2))
spaced((X(28), TOP // 2), 'EIDOLON', title, ACC, X(6))
d.line([X(268), X(20), X(268), TOP - X(20)], fill=LINE, width=X(2))
d.text((X(288), TOP // 2), 'NAM AMP CAPTURE', font=small, fill=DIM, anchor='lm')
d.text((W - X(28), TOP // 2), 'MODEL LOADED   ·   48 kHz   ·   DSP 18%', font=small, fill=DIM, anchor='rm')

LX1 = X(860); RX0 = W - X(330); BY = H - BOT
d.rectangle([0, TOP + X(2), LX1, BY], fill=PANE); d.rectangle([RX0, TOP + X(2), W, BY], fill=PANE)
d.line([LX1, TOP, LX1, BY], fill=LINE, width=X(2)); d.line([RX0, TOP, RX0, BY], fill=LINE, width=X(2))

# browser
d.text((X(26), TOP + X(40)), 'CAPTURES', font=tiny, fill=DIM, anchor='lm')
d.text((X(26), TOP + X(76)), 'My Captures  /  High Gain', font=small, fill=ACC2, anchor='lm')
files = ['Black Iron Lead', 'Cold Steel Crunch', 'Doom Fuzz Wall', 'Grave Drive', 'Jet Black Lead', 'Tight Rhythm 01', 'Tight Rhythm 02',
         'Tight Rhythm 03', 'Void Rhythm', 'Winter Hall Clean', 'Witching Hour Lead']
y = TOP + X(112); rh = X(52)
for f_ in files:
    sel = f_ == 'Tight Rhythm 02'
    if sel: rr([X(12), y, LX1 - X(32), y + rh - X(5)], X(8), fill=(20, 52, 76), outline=ACC, width=X(2))
    gx = X(32); gy = y + rh // 2
    d.polygon([(gx, gy - X(12)), (gx + X(14), gy - X(12)), (gx + X(21), gy - X(5)), (gx + X(21), gy + X(12)), (gx, gy + X(12))], outline=ACC if sel else DIM, width=X(2))
    d.text((X(68), gy), f_, font=bodyb if sel else body, fill=TXT if sel else (170, 186, 202), anchor='lm')
    d.text((LX1 - X(48), gy), '.nam', font=tiny, fill=ACC if sel else (70, 86, 100), anchor='rm')
    y += rh
rr([LX1 - X(22), TOP + X(112), LX1 - X(13), BY - X(16)], X(4), fill=(22, 30, 40))
rr([LX1 - X(22), TOP + X(220), LX1 - X(13), TOP + X(400)], X(4), fill=ACC2)

# centre
cx0 = LX1 + X(40); cx1 = RX0 - X(40)
d.text((cx0, TOP + X(42)), 'NOW PLAYING', font=tiny, fill=DIM, anchor='lm')
spaced((cx0, TOP + X(108)), 'TIGHT RHYTHM 02', h1, TXT, X(4))
xx = cx0; yy = TOP + X(168)
for t, c in (('WAVENET', ACC), ('STANDARD', ACC), ('ESR 0.0071', GRN), ('CAL +12.0 dBu', AMB), ('LEVEL MATCH', GRN)):
    tw = d.textlength(t, font=small) + X(32)
    rr([xx, yy, xx + tw, yy + X(42)], X(21), outline=c, width=X(2), fill=(c[0] // 10, c[1] // 10, c[2] // 10))
    d.text((xx + tw / 2, yy + X(21)), t, font=small, fill=c, anchor='mm'); xx += tw + X(14)
gy0, gy1 = TOP + X(240), BY - X(28)
g1x1 = cx0 + X(520)
for (a, b) in ((cx0, g1x1), (g1x1 + X(24), cx1)):
    rr([a, gy0, b, gy1], X(12), fill=(8, 12, 18), outline=LINE, width=X(2))
for k in range(1, 6):
    xk = cx0 + (g1x1 - cx0) * k / 6; yk = gy0 + (gy1 - gy0) * k / 6
    d.line([xk, gy0 + X(4), xk, gy1 - X(4)], fill=(20, 28, 38), width=X(1)); d.line([cx0 + X(4), yk, g1x1 - X(4), yk], fill=(20, 28, 38), width=X(1))
d.line([cx0 + X(10), gy1 - X(10), g1x1 - X(10), gy0 + X(10)], fill=(40, 52, 66), width=X(2))
pts = []
for i in range(241):
    t = i / 240; o = math.tanh(2.6 * (t * 2 - 1)) / math.tanh(2.6)
    pts.append((cx0 + X(12) + t * (g1x1 - cx0 - X(24)), (gy0 + gy1) / 2 - o * (gy1 - gy0 - X(36)) / 2))
d.line(pts, fill=ACC, width=X(4), joint='curve')
d.text((cx0 + X(16), gy0 + X(24)), 'TRANSFER CURVE', font=tiny, fill=DIM, anchor='lm')
g2x0 = g1x1 + X(24); mid = (gy0 + gy1) / 2; amp = (gy1 - gy0) * 0.4
d.line([g2x0 + X(8), mid, cx1 - X(8), mid], fill=(26, 36, 48), width=X(1))
n = 1200; xs = np.linspace(0, 1, n)
env = 0.3 + 0.7 * np.exp(-((xs * 2) % 1) * 2.0)
sig = np.sin(xs * 2 * math.pi * 18) * 0.75 + np.sin(xs * 2 * math.pi * 36 + 1.2) * 0.2 + np.sin(xs * 2 * math.pi * 54 + 0.4) * 0.08
inp = sig * env * 0.55; out = np.tanh(inp * 3.2) / np.tanh(1.8) * 0.95
for arr, col in ((inp, (70, 88, 108)), (out, ACC)):
    d.line([(g2x0 + X(12) + x * (cx1 - g2x0 - X(24)), mid - v * amp) for x, v in zip(xs, arr)], fill=col, width=X(2))
d.text((g2x0 + X(16), gy0 + X(24)), 'OUTPUT', font=tiny, fill=ACC, anchor='lm')
d.text((g2x0 + X(110), gy0 + X(24)), 'INPUT', font=tiny, fill=(90, 108, 128), anchor='lm')

# right: meters + readouts
mx0 = RX0 + X(40); my0, my1 = TOP + X(44), BY - X(150)
for k, (lab, lv) in enumerate((('IN', 0.66), ('OUT', 0.58))):
    x = mx0 + k * X(70)
    rr([x, my0, x + X(42), my1], X(7), fill=(10, 14, 20), outline=LINE, width=X(2))
    nseg = 26
    for sg in range(nseg):
        t = sg / (nseg - 1); yb = my1 - X(6) - (sg + 1) * (my1 - my0 - X(12)) / nseg; yt = yb + (my1 - my0 - X(12)) / nseg - X(3)
        col = GRN if t < 0.66 else AMB if t < 0.86 else RED
        d.rectangle([x + X(7), yb + X(3), x + X(35), yt], fill=col if t <= lv else (col[0] // 9, col[1] // 9, col[2] // 9))
    d.text((x + X(21), my1 + X(24)), lab, font=tiny, fill=DIM, anchor='mm')
gy = BY - X(84)
for lab, val in (('IN', '-14.2 dB'), ('OUT', '-11.8 dB')):
    d.text((mx0, gy), lab, font=tiny, fill=DIM, anchor='lm'); d.text((W - X(36), gy), val, font=monob, fill=TXT, anchor='rm'); gy += X(40)

# status bar
d.rectangle([0, BY, W, H], fill=(9, 13, 19)); d.line([0, BY, W, BY], fill=LINE, width=X(2))
d.text((X(28), BY + BOT / 2), 'Turn BROWSE to pick a capture   ·   press LOAD   ·   PREV / NEXT step through the folder', font=small, fill=DIM, anchor='lm')
d.text((W - X(28), BY + BOT / 2), '7 / 11', font=small, fill=ACC, anchor='rm')

img = img.resize((W // S, H // S), Image.LANCZOS)
a = np.asarray(img).astype(np.float32) / 255
glow = np.asarray(Image.fromarray((np.clip(a - 0.45, 0, 1) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(5))).astype(np.float32) / 255
a = np.clip(a + glow * 0.35, 0, 1)
yy, xx = np.mgrid[0:a.shape[0], 0:a.shape[1]]
a *= (1 - 0.09 * (((xx / a.shape[1] - 0.5) * 2) ** 2 + ((yy / a.shape[0] - 0.5) * 2) ** 2))[..., None]
Image.fromarray((a * 255).astype(np.uint8)).save('art/eidolon_screen.png')
print('ok', img.size)
