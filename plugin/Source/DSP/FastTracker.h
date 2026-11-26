#pragma once
// =============================================================================
//  FastTracker: the guitar pitch tracker (NSDF at ~16 kHz, a new estimate every
//  millisecond, the window sized to the note and ending at the newest sample)
//  and a pick-attack / new-note detector. HYDRA's own, and the shared
//  PitchTracker (synth, tuner, KERAUNOS, AETHER) runs on it too.
//  No JUCE in here: Source/Tests/HarmonyBench builds it on its own.
// =============================================================================
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#ifdef HYDRA_DEBUG
#include <cstdio>
#endif

namespace obs::dsp::hydra
{
using i64 = std::int64_t;
constexpr double kPiD = 3.14159265358979323846;

inline float clampf (float v, float a, float b)    { return v < a ? a : (v > b ? b : v); }
inline double clampd (double v, double a, double b) { return v < a ? a : (v > b ? b : v); }
inline float dbToGain (float db)                    { return db <= -120.f ? 0.f : std::pow (10.f, db * 0.05f); }

// ---------------------------------------------------------------------------- small blocks
/** RBJ biquad (lowpass / highpass), transposed direct form II, double state. */
struct Biquad
{
    enum Type { LP, HP };
    void set (Type t, double sr, double f, double q)
    {
        const double w = 2.0 * kPiD * clampd (f, 1.0, sr * 0.49) / sr, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double a0 = 1.0 + al;
        if (t == LP) { b0 = (1.0 - cw) * 0.5 / a0; b1 = (1.0 - cw) / a0; b2 = b0; }
        else         { b0 = (1.0 + cw) * 0.5 / a0; b1 = -(1.0 + cw) / a0; b2 = b0; }
        a1 = -2.0 * cw / a0; a2 = (1.0 - al) / a0;
    }
    void reset()                    { z1 = z2 = 0; }
    inline float process (float x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y;
        return (float) y;
    }
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
};

// ---------------------------------------------------------------------------- the fast tracker
/** NSDF pitch tracker on the clean guitar at ~16 kHz, every millisecond. The window for lag tau is
    max(tau, 4 ms) and ends at the newest sample, so a high note needs only a couple of periods.
    Also detects pick attacks (a jump in the guitar's energy above 2.5 kHz). */
class FastTracker
{
public:
    struct Estimate { i64 time = 0; float period = 0; float f0 = 0; float clarity = 0; bool voiced = false; float span = 0; };

    /** minHz: the lowest note it looks for (48 Hz: a 7-string's low B, drop A; 38 Hz: 8-strings, drop E). */
    void prepare (double sampleRate, double minHz = 48.0)
    {
        sr = sampleRate;
        decim = std::max (1, (int) std::lround (sr / 16000.0));
        dsr = sr / decim;
        aa1.set (Biquad::LP, sr, dsr * 0.42, 0.5412); aa2.set (Biquad::LP, sr, dsr * 0.42, 1.3066);
        hp.set (Biquad::HP, sr, 45.0, 0.707);
        apLp1.set (Biquad::LP, sr, 1200.0, 0.5412); apLp2.set (Biquad::LP, sr, 1200.0, 1.3066);
        tauMax = std::min (kBuf / 2 - 8, (int) (dsr / std::max (30.0, minHz)));
        tauMin = std::max (3, (int) (dsr / 1700.0));
        wMin = (int) (dsr * 0.002);
        hop = std::max (4, (int) std::lround (dsr * 0.001));
        hfa.set (Biquad::HP, sr, 2500.0, 0.5412); hfb.set (Biquad::HP, sr, 2500.0, 1.3066);
        fastC = std::exp (-1.0 / (0.0007 * sr)); slowC = std::exp (-1.0 / (0.030 * sr)); apC = std::exp (-1.0 / (0.0015 * sr));
        refractory = (i64) (0.035 * sr);
        reset();
    }
    void reset()
    {
        aa1.reset(); aa2.reset(); hp.reset(); hfa.reset(); hfb.reset(); apLp1.reset(); apLp2.reset();
        std::fill (std::begin (ring), std::end (ring), 0.f);
        pos = 0; phase = 0; hopCount = 0; nEst = 0;
        fast = slow = 0; lastOnset = -(1 << 30); lvl = 0;
        fFast = fSlow = fSlowPrev = 0; apD = apX = apXslow = 0; apSlow = 1; perHost = 0; lastVoicedT = -(1 << 30);
        std::fill (std::begin (hist), std::end (hist), 0.f); hpos = 0;
    }
    /** Feed one host-rate sample of the clean guitar at time t. Returns true when a new estimate
        is ready (est()), sets `onset` when a note starts at about this time: a pick attack (a jump
        in the energy above 2.5 kHz or overall) or the waveform no longer repeating with the
        period of the note that was playing (a new note, also a hammer-on). */
    inline bool push (float x, i64 t, bool& onset)
    {
        onset = false;
        const float xh = hp.process (x);
        const float xl = apLp2.process (apLp1.process (xh));      // the strings' high partials aren't harmonic enough to compare
        hist[(size_t) (hpos & (kHist - 1))] = xl; ++hpos;
        // pick attacks: energy above 2.5 kHz, and overall
        const float h = hfb.process (hfa.process (x));
        const double e = (double) h * h, ef = (double) xh * xh;
        fast = fastC * fast + (1.0 - fastC) * e;
        slow = slowC * slow + (1.0 - slowC) * e;
        fFast = fastC * fFast + (1.0 - fastC) * ef;
        fSlowPrev = fSlow;
        fSlow = slowC * fSlow + (1.0 - slowC) * ef;
        bool hit = (fast > 7.0 * slow + 2e-9 && fast > 4e-8)
                || (fFast > 5.0 * fSlowPrev + 1e-9 && fFast > 2e-6);
       #ifdef HYDRA_DEBUG
        int why = hit ? ((fast > 7.0 * slow + 2e-9 && fast > 4e-8) ? 1 : 2) : 0;
       #endif
        // a new note: no longer periodic with the period that was playing
        if (perHost > 4.0 && perHost < kHist - 8)
        {
            const double rp = (double) hpos - 1.0 - perHost;
            const i64 i0 = (i64) std::floor (rp); const float fr = (float) (rp - (double) i0);
            const float a = hist[(size_t) (i0 & (kHist - 1))], b = hist[(size_t) ((i0 + 1) & (kHist - 1))];
            const float dlt = xl - (a + (b - a) * fr);
            apD = apC * apD + (1.0 - apC) * (double) dlt * dlt;
            apX = apC * apX + (1.0 - apC) * (double) xl * xl;
            apXslow = slowC * apXslow + (1.0 - slowC) * (double) xl * xl;
            const double ap = apD / (apX + 1e-12);
            apSlow = slowC * apSlow + (1.0 - slowC) * std::min (ap, 2.0);
            if (ap > 0.7 && apSlow < 0.15 && apX > 1e-6 && apX > 0.6 * apXslow          // not a note dying away
                && t - lastVoicedT < (i64) (0.05 * sr) && t - lastOnset > (i64) (0.06 * sr))
            {
               #ifdef HYDRA_DEBUG
                if (! hit) why = 3;
               #endif
                hit = true;
            }
        }
        if (hit && t - lastOnset > refractory)
        {
            onset = true; lastOnset = t; apSlow = 1.0;
           #ifdef HYDRA_DEBUG
            std::printf ("  (onset by %s, per %.1f)\n", why == 1 ? "HF" : why == 2 ? "energy" : "aperiodic", perHost);
           #endif
        }
        lvl = std::max ((double) std::abs (x), lvl * 0.9995);
        // decimate
        const float s = aa2.process (aa1.process (xh));
        if (++phase < decim) return false;
        phase = 0;
        ring[pos] = s; ring[pos + kBuf] = s; pos = (pos + 1) & (kBuf - 1);
        if (++hopCount < hop) return false;
        hopCount = 0;
        analyse (t);
        if (last.voiced && last.clarity > 0.9f) { perHost = last.period; lastVoicedT = t; }
        return true;
    }
    const Estimate& est() const     { return last; }
    int estimates() const           { return nEst; }
    double decimRate() const        { return dsr; }

private:
    static constexpr int kBuf = 2048;

    void analyse (i64 t)
    {
        const int need = tauMax + std::max (tauMax, wMin) + 2;
        const float* f = &ring[(size_t) ((pos - need) & (kBuf - 1))];     // oldest first, newest at f[need-1]
        // prefix sums of squares (double)
        pre[0] = 0;
        for (int i = 0; i < need; ++i) pre[i + 1] = pre[i] + (double) f[i] * f[i];
        const double total = pre[need] - pre[need - std::max (wMin, 32)];
        last = {}; last.time = t; ++nEst;
        if (total / std::max (wMin, 32) < 1e-9 || lvl < 2e-4) return;     // silence

        const int N = need;
        // NSDF lag by lag, and McLeod's key maxima (one per positive lobe) as they close
        nsdf[0] = 1.f;
        int keyTau[64]; float keyVal[64]; int nk = 0;
        bool above = false; int bt = -1; float bv = -1;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            const int W = std::max (tau, wMin);
            const float* b = f + (N - W);
            const float* a = b - tau;
            float s0 = 0, s1 = 0, s2 = 0, s3 = 0, s4 = 0, s5 = 0, s6 = 0, s7 = 0;
            int j = 0;
            for (; j + 8 <= W; j += 8)
            {
                s0 += a[j] * b[j];         s1 += a[j + 1] * b[j + 1]; s2 += a[j + 2] * b[j + 2]; s3 += a[j + 3] * b[j + 3];
                s4 += a[j + 4] * b[j + 4]; s5 += a[j + 5] * b[j + 5]; s6 += a[j + 6] * b[j + 6]; s7 += a[j + 7] * b[j + 7];
            }
            double r = (double) ((s0 + s1) + (s2 + s3)) + (double) ((s4 + s5) + (s6 + s7));
            for (; j < W; ++j) r += (double) a[j] * b[j];
            const double m = (pre[N] - pre[N - W]) + (pre[N - tau] - pre[N - W - tau]);
            const float v = nsdf[tau] = m > 1e-12 ? (float) (2.0 * r / m) : 0.f;

            if (! above) { if (v > 0 && nsdf[tau - 1] <= 0) { above = true; bt = tau; bv = v; } continue; }
            if (v > bv) { bv = v; bt = tau; }
            if (v <= 0 || tau == tauMax)
            {
                if (nk < 64 && bt >= tauMin && bt < tauMax) { keyTau[nk] = bt; keyVal[nk] = bv; ++nk; }
                above = false;
                // a near-perfect match: McLeod's pick is this one or an earlier one whatever comes later (it passes
                // 0.9 x any maximum), so the longer lags needn't be computed (a high note costs a fraction)
                if (bv >= 0.985f && bt >= tauMin && bt < tauMax) break;
                bt = -1; bv = -1;
            }
        }
        if (nk == 0) return;
        float mx = 0; for (int k = 0; k < nk; ++k) mx = std::max (mx, keyVal[k]);
        int pick = 0; for (int k = 0; k < nk; ++k) if (keyVal[k] >= 0.9f * mx) { pick = k; break; }
        // an octave too low? (a strong peak at half the lag means the note is the one above)
        for (int k = 0; k < pick; ++k)
            if (std::abs (keyTau[k] * 2 - keyTau[pick]) <= std::max (2, keyTau[pick] / 25) && keyVal[k] >= octaveRatio * keyVal[pick]) { pick = k; break; }
        const int tb = keyTau[pick];
        double tau = tb; float val = keyVal[pick];
        {
            const double y0 = nsdf[tb - 1], y1 = nsdf[tb], y2 = nsdf[tb + 1], den = y0 - 2.0 * y1 + y2;
            if (std::abs (den) > 1e-12) { const double dlt = clampd (0.5 * (y0 - y2) / den, -0.5, 0.5); tau += dlt; val = (float) (y1 - 0.25 * (y0 - y2) * dlt); }
        }
        last.clarity = val;
        last.period = (float) (tau * decim);
        last.f0 = (float) (dsr / tau);
        last.span = (float) ((tau + std::max (tau, (double) wMin)) * decim);
        last.voiced = val >= 0.80f;
    }

    double sr = 48000, dsr = 16000;
    int decim = 3, tauMax = 333, tauMin = 9, wMin = 64, hop = 16;
public:
    /** An octave too low? A key maximum at half the lag at least this high (vs the picked one) means the note is the one above. */
    float octaveRatio = 0.8f;
private:
    int pos = 0, phase = 0, hopCount = 0, nEst = 0;
    float ring[kBuf * 2] {};
    double pre[kBuf + 2] {};
    float nsdf[kBuf / 2 + 8] {};
    Biquad aa1, aa2, hp, hfa, hfb, apLp1, apLp2;
    double fast = 0, slow = 0, fastC = 0, slowC = 0, lvl = 0;
    double fFast = 0, fSlow = 0, fSlowPrev = 0, apD = 0, apX = 0, apXslow = 0, apSlow = 1, apC = 0, perHost = 0;
    static constexpr int kHist = 4096;
    float hist[kHist] {}; i64 hpos = 0, lastVoicedT = 0;
    i64 lastOnset = 0, refractory = 1680;
    Estimate last;
};

} // namespace obs::dsp::hydra
