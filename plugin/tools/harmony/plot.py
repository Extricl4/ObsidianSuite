import sys, numpy as np, scipy.io.wavfile as wf, matplotlib
matplotlib.use('Agg'); import matplotlib.pyplot as plt
sr, x = wf.read(sys.argv[1]); x = x.astype(float)/32000
t0, t1 = float(sys.argv[2]), float(sys.argv[3])
a, b = int(t0*sr), int(t1*sr)
fig, ax = plt.subplots(3, 1, figsize=(14, 9))
t = np.arange(a, b)/sr*1000
ax[0].plot(t, x[a:b,1], lw=0.6, label='in'); ax[0].plot(t, x[a:b,0], lw=0.6, label='voice'); ax[0].legend()
ax[1].specgram(x[:,0], NFFT=2048, Fs=sr, noverlap=1792, cmap='magma', vmin=-120); ax[1].set_ylim(0, 3000)
seg = x[a:b,0]
ax[2].plot(t, seg, lw=0.6); ax[2].set_xlim(t0*1000, t0*1000+60)
plt.tight_layout(); plt.savefig(sys.argv[4], dpi=70)
