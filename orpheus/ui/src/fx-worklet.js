/* SIREN's SHIMMER (ORPHEUS standalone): a real shimmer reverb, as an AudioWorklet.
   - The sound is pre-delayed and smeared by four allpass diffusers.
   - An 8-line feedback delay network rings it out: every line's length slowly moves (a few hundredths of a semitone), a
     Householder matrix mixes the lines, a lowpass in each line lets the highs die first and a gentle highcut keeps lows clean.
   - The shimmer: the network's own tail is pitched up an octave (and, in FIFTHS, a fifth) by a granular shifter with four
     overlapping, randomly jittered grains, and fed back in. So every pass climbs: the octaves are made of the space itself.
   Parameters (k-rate): decay (seconds to -60 dB), rise (0..1, how much climbs), fifth (0/1), damp (Hz). */
const TAU = Math.PI * 2;
class Grains {                                     // a pitch shifter on a shared circular buffer: 4 grains, Hann windows, jittered
  constructor(sr, ratio, grain) {
    this.r = ratio; this.len = Math.round(grain * sr); this.jit = Math.round(.018 * sr);
    this.g = [0, 1, 2, 3].map(k => ({ age: Math.round(k * this.len / 4), d0: 0 }));
    this.g.forEach(g => this.restart(g, true));
  }
  restart(g, keepAge) { g.d0 = (this.r - 1) * this.len + 6 + Math.random() * this.jit; if (!keepAge) g.age = 0; }
  read(buf, M, w) {
    let out = 0; const L = this.len, s = this.r - 1;
    for (let k = 0; k < 4; k++) {
      const g = this.g[k];
      if (g.age >= L) this.restart(g, false);
      const d = g.d0 - s * g.age, pos = w - d, i = Math.floor(pos), f = pos - i;
      const a = buf[i & M], b = buf[(i + 1) & M];
      out += (a + (b - a) * f) * (.5 - .5 * Math.cos(TAU * g.age / L));
      g.age++;
    }
    return out * .5;                                 // four Hann windows a quarter apart add up to 2
  }
}
class ObsShimmer extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [
      { name: 'decay', defaultValue: 5, minValue: .3, maxValue: 30, automationRate: 'k-rate' },
      { name: 'rise', defaultValue: .5, minValue: 0, maxValue: 1, automationRate: 'k-rate' },
      { name: 'fifth', defaultValue: 0, minValue: 0, maxValue: 1, automationRate: 'k-rate' },
      { name: 'damp', defaultValue: 7000, minValue: 800, maxValue: 18000, automationRate: 'k-rate' },
    ];
  }
  constructor() {
    super();
    const sr = sampleRate, ms = x => x * sr / 1000; this.sr = sr;
    this.pre = new Float32Array(4096); this.preW = 0; this.preD = Math.round(ms(28));
    this.ap = [4.77, 3.59, 12.73, 9.31].map((d, k) => ({ b: new Float32Array(Math.round(ms(d))), i: 0, g: k < 2 ? .72 : .62 }));
    const L = [37.1, 41.9, 48.7, 53.3, 61.1, 67.3, 73.9, 83.3].map(x => x * 1.35);
    this.N = 8; this.M = 16383; this.len = L.map(ms); this.dep = L.map((_, i) => ms(.28 + .09 * i));
    this.inc = [.071, .093, .117, .131, .157, .183, .211, .239].map(f => TAU * f / sr); this.ph = L.map((_, i) => i * 1.3);
    this.buf = L.map(() => new Float32Array(16384)); this.w = 0;
    this.lp = new Float64Array(8); this.hp = new Float64Array(8); this.v = new Float64Array(8); this.g = new Float64Array(8);
    this.sgIn = [1, -1, 1, -1, -1, 1, -1, 1]; this.sgSh = [1, 1, -1, -1, 1, 1, -1, -1];
    this.oL = [1, 0, 1, 0, 1, 0, 1, 0].map((x, i) => (x ? 1 : .35) * (i % 4 < 2 ? 1 : -1));
    this.oR = [0, 1, 0, 1, 0, 1, 0, 1].map((x, i) => (x ? 1 : .35) * (i % 4 === 1 || i % 4 === 2 ? 1 : -1));
    this.sb = new Float32Array(65536); this.SM = 65535; this.sw = 0;
    this.up12 = new Grains(sr, 2, .095); this.up7 = new Grains(sr, 1.5, .13);
    this.rise = 0; this.f5 = 0; this.decay = -1; this.norm = 1; this.dampHz = -1; this.a = .5; this.ah = 1 - Math.exp(-TAU * 22 / sr);
    this.shLp = 0; this.shHp = 0; this.bad = 0; this.quiet = 0; this.idle = false;
  }
  reset() { this.buf.forEach(b => b.fill(0)); this.sb.fill(0); this.pre.fill(0); this.ap.forEach(a => a.b.fill(0)); this.lp.fill(0); this.hp.fill(0); this.shLp = this.shHp = 0; }
  process(ins, outs, P) {
    const inp = ins[0], out = outs[0], oL = out[0], oR = out[1] || out[0], n = oL.length, sr = this.sr;
    const iL = inp && inp[0], iR = inp && (inp[1] || inp[0]);
    /* asleep while nothing comes in and the tail has died away (switched off, or silent): it costs nothing */
    let live = false; if (iL) for (let t = 0; t < n; t += 4) if (iL[t] !== 0 || iR[t] !== 0) { live = true; break; }
    if (!live && this.idle) { oL.fill(0); if (oR !== oL) oR.fill(0); return true; }
    if (live) { this.idle = false; this.quiet = 0; }
    const decay = P.decay[0], damp = P.damp[0];
    if (decay !== this.decay) { this.decay = decay; let gm = 0; for (let i = 0; i < 8; i++) { this.g[i] = Math.pow(10, -3 * this.len[i] / (sr * decay)); gm += this.g[i] / 8; }
      const gr = Math.pow(10, -3 * .0787 / 4); this.norm = Math.sqrt(Math.max(.03, 1 - gm * gm) / (1 - gr * gr)); }   // as loud at any DECAY (a long one piles up more)
    if (damp !== this.dampHz) { this.dampHz = damp; this.a = 1 - Math.exp(-TAU * damp / sr); }
    const riseT = P.rise[0], f5T = P.fifth[0];
    const M = this.M, SM = this.SM, buf = this.buf, v = this.v, lp = this.lp, hp = this.hp, g = this.g, a = this.a, ah = this.ah;
    const aSh = 1 - Math.exp(-TAU * 5500 / sr), ahSh = 1 - Math.exp(-TAU * 380 / sr);
    for (let t = 0; t < n; t++) {
      this.rise += (riseT - this.rise) * .0005; this.f5 += (f5T - this.f5) * .0005;
      /* in: mono, pre-delay, diffusion */
      let x = iL ? (iL[t] + iR[t]) * .5 : 0;
      const pre = this.pre; pre[this.preW] = x; x = pre[(this.preW - this.preD) & 4095]; this.preW = (this.preW + 1) & 4095;
      for (let k = 0; k < 4; k++) { const A = this.ap[k], d = A.b[A.i], y = d - A.g * x; A.b[A.i] = x + A.g * y; A.i = A.i + 1 === A.b.length ? 0 : A.i + 1; x = y; }
      /* the network: read every line (moving lengths, cubic), damp, mix */
      let s = 0;
      for (let i = 0; i < 8; i++) {
        this.ph[i] += this.inc[i]; if (this.ph[i] > TAU) this.ph[i] -= TAU;
        const pos = this.w - this.len[i] - this.dep[i] * Math.sin(this.ph[i]), j = Math.floor(pos), f = pos - j, b = buf[i];
        const xm = b[(j - 1) & M], x0 = b[j & M], x1 = b[(j + 1) & M], x2 = b[(j + 2) & M];
        let y = ((((.5 * (x2 - xm) + 1.5 * (x0 - x1)) * f + (xm - 2.5 * x0 + 2 * x1 - .5 * x2)) * f + .5 * (x1 - xm)) * f + x0);
        lp[i] += (y - lp[i]) * a; y = lp[i];
        hp[i] += (y - hp[i]) * ah; y -= hp[i];
        v[i] = y; s += y;
      }
      /* the shimmer: the tail, pitched up, back in */
      let sh = 0;
      if (this.rise > 1e-4) {
        this.sb[this.sw] = s * .35;
        const u = this.up12.read(this.sb, SM, this.sw), q = this.f5 > 1e-3 ? this.up7.read(this.sb, SM, this.sw) : 0;
        sh = u * (1 - .35 * this.f5) + q * .7 * this.f5;
        this.shLp += (sh - this.shLp) * aSh; this.shHp += (this.shLp - this.shHp) * ahSh; sh = this.shLp - this.shHp;     // 380 Hz - 5.5 kHz
        sh *= this.rise * 1.5; sh = sh / (1 + Math.abs(sh) * .8);                                                         // soft, never runs away
      } else this.sb[this.sw] = 0;
      this.sw = (this.sw + 1) & SM;
      const hs = s * .25, w = this.w;
      let l = 0, r = 0;
      for (let i = 0; i < 8; i++) {
        buf[i][w] = (v[i] - hs) * g[i] + x * this.sgIn[i] * .42 + sh * this.sgSh[i] * .5;
        l += v[i] * this.oL[i]; r += v[i] * this.oR[i];
      }
      this.w = (w + 1) & M;
      l *= .42 * this.norm; r *= .42 * this.norm;
      if (!(Math.abs(l) < 8 && Math.abs(r) < 8)) { this.reset(); l = r = 0; }
      oL[t] = l; oR[t] = r;
    }
    if (!live) { let m = 0; for (let t = 0; t < n; t += 8) m = Math.max(m, Math.abs(oL[t]), Math.abs(oR[t])); if (m < 2e-6) { if (++this.quiet > 120) { this.idle = true; this.reset(); } } else this.quiet = 0; }
    return true;
  }
}
registerProcessor('obs-shimmer', ObsShimmer);
