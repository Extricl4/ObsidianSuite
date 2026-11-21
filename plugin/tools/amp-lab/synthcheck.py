#!/usr/bin/env python3
"""Measures the synth demo renders: level, peak, DC, pitch and aliasing.
    "Obsidian Amp Suite" --render-synth <folder>
    python synthcheck.py <folder>"""
import sys, glob, os, wave, numpy as np
def load(f):
    w = wave.open(f); n = w.getnframes(); raw = w.readframes(n); sw = w.getsampwidth(); ch = w.getnchannels(); sr = w.getframerate()
    a = np.frombuffer(raw, dtype=np.uint8).reshape(-1, sw)
    x = (a[:, 0].astype(np.int32) | (a[:, 1].astype(np.int32) << 8) | (a[:, 2].astype(np.int32) << 16))
    x = np.where(x >= 1 << 23, x - (1 << 24), x) / float(1 << 23)
    return x.reshape(-1, ch), sr
def db(x): return 20 * np.log10(max(x, 1e-9))
for f in sorted(glob.glob(os.path.join(sys.argv[1], '*.wav'))):
    x, sr = load(f); m = x.mean(1)
    rms = np.sqrt((m ** 2).mean()); peak = np.abs(x).max(); dc = abs(m.mean())
    line = f'{os.path.basename(f):22s} rms {db(rms):6.1f} dBFS  peak {db(peak):6.1f}  dc {dc:.4f}  L/R corr {np.corrcoef(x[:,0], x[:,1])[0,1]:.2f}'
    if 'E6' in f:
        # E6 = 1318.5 Hz: everything that is not a harmonic of it is aliasing
        seg = m[int(.2 * sr):int(.2 * sr) + 32768] * np.hanning(32768)
        S = np.abs(np.fft.rfft(seg)); fr = np.fft.rfftfreq(32768, 1 / sr); f0 = 1318.51
        harm = np.zeros_like(S, bool)
        for k in range(1, int(20000 / f0) + 1): harm |= np.abs(fr - k * f0) < 12
        audible = fr < 16000
        ratio = db(np.sqrt((S[audible & ~harm] ** 2).sum()) / np.sqrt((S[harm] ** 2).sum()))
        pk = fr[np.argmax(S)]
        line += f'  strongest {pk:.1f} Hz  alias {ratio:.1f} dB'
    print(line)
