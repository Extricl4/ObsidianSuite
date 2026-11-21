#pragma once
// =============================================================================
//  HYDRA: the smart harmonizer (the HARMONY page). Up to six voices, each a
//  step in the chosen key and mode (SMART: the semitones follow the note you
//  play) or a fixed number of semitones.
//
//  LEAD (single notes, the main mode) — built for no lag and no pops:
//   * Its own fast pitch tracker on the clean guitar (NSDF, the window grows
//     with the period, so high notes are found in ~2 periods, a low E in ~25 ms),
//     plus a pick-attack detector.
//   * Each voice is a pitch-synchronous shifter: a read head moving through the
//     signal at the pitch ratio, which jumps by whole periods of the note you
//     play (measured, then aligned by cross-correlation) with a short
//     crossfade. Two heads one period apart carry the same waveform, so the
//     splice is seamless. The output trails the guitar by only ~1-2 periods.
//   * Splices never repeat or skip a pick attack: a voice jumps before an
//     attack reaches it, never while one is passing.
//   * Notes are time-stamped: a voice reading the signal at time t uses the
//     note that was played at time t (a voice with some DELAY gets the new
//     interval exactly on the attack). Until a new note is known, smart voices
//     dip slightly instead of sounding a wrong interval.
//   * Band-limited (32-tap windowed sinc) reading, anti-aliased when shifting up.
//  CHORDS: a polyphonic note detector on the clean guitar and a phase-vocoder
//   voice (Signalsmith Stretch) per harmony, whose frequency map moves every
//   partial by the interval of the note it belongs to. ~60 ms behind.
//
//  No JUCE in here: Source/Tests/HarmonyBench builds it on its own.
// =============================================================================
#include "HarmonyDefs.h"
#include <signalsmith-stretch/signalsmith-stretch.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>
#ifdef HYDRA_DEBUG
#include <cstdio>
#include <cstdlib>
#endif

namespace obs::dsp::hydra
{
using i64 = std::int64_t;
namespace hd = obs::harmdefs;
constexpr double kPiD = 3.14159265358979323846;
constexpr int kVoices = hd::kVoices;

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

/** Smooth random wander in -1..1 (cubic between random points), for HUMANIZE. */
struct Wander
{
    void prepare (double sr, double hz, unsigned seed)  { step = hz / sr; state = seed * 2654435761u + 12345u; a = rnd(); b = rnd(); c = rnd(); d = rnd(); ph = 0; }
    float next()
    {
        ph += step;
        if (ph >= 1.0) { ph -= 1.0; a = b; b = c; c = d; d = rnd(); }
        const float t = (float) ph;
        // Catmull-Rom between b and c
        return 0.5f * ((2.f * b) + (-a + c) * t + (2.f * a - 5.f * b + 4.f * c - d) * t * t + (-a + 3.f * b - 3.f * c + d) * t * t * t) * 0.8f;
    }
    float rnd()             { state = state * 1664525u + 1013904223u; return (float) ((state >> 8) & 0xFFFF) / 32767.5f - 1.f; }
    double step = 0, ph = 0; unsigned state = 1; float a = 0, b = 0, c = 0, d = 0;
};

// ---------------------------------------------------------------------------- band-limited reading
/** 32-tap Kaiser-windowed sinc, 256 sub-sample phases, 10 cutoffs (for reading faster than 1:1). */
class Sinc
{
public:
    static constexpr int kHalf = 16, kTaps = 32, kPhases = 256, kCutoffs = 10;

    static const Sinc& get()        { static const Sinc s; return s; }

    /** Table for a read speed (ratio): no band-limiting at or below 1, then 4 steps per octave. */
    static int cutoffFor (double ratio)
    {
        if (ratio <= 1.0001) return 0;
        const int k = (int) std::ceil (4.0 * std::log2 (ratio) - 1e-6);
        return k < 0 ? 0 : (k >= kCutoffs ? kCutoffs - 1 : k);
    }
    /** Interpolated sample at x[0] + frac, where x points at the sample kHalf-1 before the read
        position's integer part (x[kHalf-1] is that sample). */
    inline float read (const float* x, double frac, int table) const
    {
        const double fp = frac * kPhases;
        int p = (int) fp; if (p >= kPhases) p = kPhases - 1;
        const float t = (float) (fp - p);
        const float* r0 = &t_[(size_t) ((table * (kPhases + 1) + p) * kTaps)];
        const float* r1 = r0 + kTaps;
        float s0 = 0, s1 = 0, s2 = 0, s3 = 0;
        for (int i = 0; i < kTaps; i += 4)
        {
            s0 += (r0[i]     + t * (r1[i]     - r0[i]))     * x[i];
            s1 += (r0[i + 1] + t * (r1[i + 1] - r0[i + 1])) * x[i + 1];
            s2 += (r0[i + 2] + t * (r1[i + 2] - r0[i + 2])) * x[i + 2];
            s3 += (r0[i + 3] + t * (r1[i + 3] - r0[i + 3])) * x[i + 3];
        }
        return (s0 + s1) + (s2 + s3);
    }
private:
    Sinc()
    {
        t_.assign ((size_t) (kCutoffs * (kPhases + 1) * kTaps), 0.f);
        const double beta = 9.0, i0b = bessel0 (beta);
        for (int k = 0; k < kCutoffs; ++k)
        {
            const double c = 0.94 * std::pow (2.0, -k / 4.0);        // fraction of Nyquist
            for (int p = 0; p <= kPhases; ++p)
            {
                const double f = (double) p / kPhases;
                double row[kTaps], sum = 0;
                for (int i = 0; i < kTaps; ++i)
                {
                    const double u = (double) (i - (kHalf - 1)) - f;   // tap position relative to the read point
                    const double sx = c * u * kPiD;
                    const double sinc = std::abs (sx) < 1e-9 ? c : c * std::sin (sx) / sx;
                    const double q = u / (double) kHalf;
                    const double w = std::abs (q) >= 1.0 ? 0.0 : bessel0 (beta * std::sqrt (1.0 - q * q)) / i0b;
                    row[i] = sinc * w; sum += row[i];
                }
                for (int i = 0; i < kTaps; ++i) t_[(size_t) ((k * (kPhases + 1) + p) * kTaps + i)] = (float) (row[i] / sum);
            }
        }
    }
    static double bessel0 (double x)
    {
        double s = 1, t = 1;
        for (int k = 1; k < 40; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; if (t < 1e-12 * s) break; }
        return s;
    }
    std::vector<float> t_;
};

/** The signal being harmonized, kept twice over so any window of it is contiguous. Indexes are
    absolute sample times. */
class Ring
{
public:
    void prepare (int minSize)
    {
        size = 1; while (size < minSize) size <<= 1;
        mask = size - 1; buf.assign ((size_t) size * 2, 0.f); w = 0;
    }
    void clear()                            { std::fill (buf.begin(), buf.end(), 0.f); }
    inline void push (float x)              { const int i = (int) (w & mask); buf[(size_t) i] = x; buf[(size_t) (i + size)] = x; ++w; }
    i64 written() const                     { return w; }
    int capacity() const                    { return size; }
    /** Contiguous pointer to `len` samples starting at absolute index `idx` (len <= size). */
    inline const float* at (i64 idx) const  { return &buf[(size_t) (idx & mask)]; }
    inline float sample (i64 idx) const     { return buf[(size_t) (idx & mask)]; }
    inline float read (double pos, int table) const
    {
        const i64 n = (i64) std::floor (pos);
        return Sinc::get().read (at (n - (Sinc::kHalf - 1)), pos - (double) n, table);
    }
private:
    std::vector<float> buf { 0.f, 0.f };
    int size = 1, mask = 0; i64 w = 0;
};

// ---------------------------------------------------------------------------- what was played, when
/** Notes (segments) and pick attacks on one time line (absolute sample times of the signal). */
class Timeline
{
public:
    struct Segment { i64 start = 0; int note = -1; float period = 0; float f0 = 0; bool picked = false; int id = 0; };

    void reset()
    {
        segs = {}; nSeg = 0; nextId = 1; onsets = {}; nOn = 0; knownSince = -1;
        add (0, -1, 0, 0, false);
    }
    void add (i64 start, int note, float period, float f0, bool picked)
    {
        if (nSeg > 0 && start < latest().start) start = latest().start;       // never before the previous one
        auto& s = segs[(size_t) (nSeg % kSeg)];
        s = { start, note, period, f0, picked, nextId++ };
        ++nSeg;
    }
    Segment& latest()                       { return segs[(size_t) ((nSeg - 1) % kSeg)]; }
    const Segment& latest() const           { return segs[(size_t) ((nSeg - 1) % kSeg)]; }
    /** The segment that was playing at time t. */
    const Segment& at (i64 t) const
    {
        const int lo = std::max (0, nSeg - kSeg);
        for (int k = nSeg - 1; k > lo; --k)
        {
            const auto& s = segs[(size_t) (k % kSeg)];
            if (s.start <= t) return s;
        }
        return segs[(size_t) (lo % kSeg)];
    }
    void addOnset (i64 t)                   { onsets[(size_t) (nOn % kOn)] = t; ++nOn; }
    /** Any pick attack in [a, b]? */
    bool onsetIn (i64 a, i64 b) const
    {
        for (int k = nOn - 1; k >= std::max (0, nOn - kOn); --k)
        {
            const i64 t = onsets[(size_t) (k % kOn)];
            if (t < a) return false;                // onsets are in time order
            if (t <= b) return true;
        }
        return false;
    }
    /** First attack after t (or -1). */
    i64 nextOnsetAfter (i64 t) const
    {
        i64 best = -1;
        for (int k = nOn - 1; k >= std::max (0, nOn - kOn); --k)
        {
            const i64 o = onsets[(size_t) (k % kOn)];
            if (o <= t) break;
            best = o;
        }
        return best;
    }
    /** Latest attack at or before t (or -1). */
    i64 lastOnsetAtOrBefore (i64 t) const
    {
        for (int k = nOn - 1; k >= std::max (0, nOn - kOn); --k)
        {
            const i64 o = onsets[(size_t) (k % kOn)];
            if (o <= t) return o;
        }
        return -1;
    }
    i64 lastOnset() const                   { return nOn > 0 ? onsets[(size_t) ((nOn - 1) % kOn)] : -1; }

    i64 knownSince = -1;    // the latest attack after which the note is known (new note or the same again)
private:
    static constexpr int kSeg = 64, kOn = 64;
    std::array<Segment, kSeg> segs {};
    std::array<i64, kOn> onsets {};
    int nSeg = 0, nOn = 0, nextId = 1;
};

// ---------------------------------------------------------------------------- the fast tracker
/** NSDF pitch tracker on the clean guitar at ~16 kHz, every millisecond. The window for lag tau is
    max(tau, 4 ms) and ends at the newest sample, so a high note needs only a couple of periods.
    Also detects pick attacks (a jump in the guitar's energy above 2.5 kHz). */
class FastTracker
{
public:
    struct Estimate { i64 time = 0; float period = 0; float f0 = 0; float clarity = 0; bool voiced = false; float span = 0; };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        decim = std::max (1, (int) std::lround (sr / 16000.0));
        dsr = sr / decim;
        aa1.set (Biquad::LP, sr, dsr * 0.42, 0.5412); aa2.set (Biquad::LP, sr, dsr * 0.42, 1.3066);
        hp.set (Biquad::HP, sr, 45.0, 0.707);
        apLp1.set (Biquad::LP, sr, 1200.0, 0.5412); apLp2.set (Biquad::LP, sr, 1200.0, 1.3066);
        tauMax = std::min (kBuf / 2 - 8, (int) (dsr / 48.0));
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
            nsdf[tau] = m > 1e-12 ? (float) (2.0 * r / m) : 0.f;
        }
        nsdf[0] = 1.f;

        // McLeod: the first key maximum above 0.9 x the highest one
        int keyTau[64]; float keyVal[64]; int nk = 0;
        bool pos = false; int bt = -1; float bv = -1;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            const float v = nsdf[tau];
            if (! pos) { if (v > 0 && nsdf[tau - 1] <= 0) { pos = true; bt = tau; bv = v; } continue; }
            if (v > bv) { bv = v; bt = tau; }
            if (v <= 0 || tau == tauMax)
            {
                if (nk < 64 && bt >= tauMin && bt < tauMax) { keyTau[nk] = bt; keyVal[nk] = bv; ++nk; }
                pos = false; bt = -1; bv = -1;
            }
        }
        if (nk == 0) return;
        float mx = 0; for (int k = 0; k < nk; ++k) mx = std::max (mx, keyVal[k]);
        int pick = 0; for (int k = 0; k < nk; ++k) if (keyVal[k] >= 0.9f * mx) { pick = k; break; }
        // an octave too low? (a strong peak at half the lag means the note is the one above)
        for (int k = 0; k < pick; ++k)
            if (std::abs (keyTau[k] * 2 - keyTau[pick]) <= std::max (2, keyTau[pick] / 25) && keyVal[k] >= 0.8f * keyVal[pick]) { pick = k; break; }
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

// ---------------------------------------------------------------------------- notes from estimates
/** Turns pitch estimates into notes with hysteresis and writes them on the time line, stamped with
    the time the note began (its pick attack when there is one). */
class NoteFollower
{
public:
    void prepare (double s)     { sr = s; reset(); }
    void reset()                { cur = -1; cand = -1; candN = 0; lastVoiced = -(1 << 30); resolved = -1; hist = {}; nh = 0; }
    int current() const         { return cur; }
    i64 lastVoicedTime() const  { return lastVoiced; }

    void update (const FastTracker::Estimate& e, float transpose, Timeline& tl)
    {
        if (! e.voiced || e.f0 <= 0) return;
        const float m = 69.f + 12.f * std::log2 (e.f0 / 440.f) + transpose;
        const float period = e.period / std::pow (2.f, transpose / 12.f);
        const int n = (int) std::lround (m);
        const i64 onset = tl.lastOnset();
        const float mAgo = hist[(size_t) ((nh + 1) % kH)];           // the pitch ~kH-1 estimates ago
        hist[(size_t) (nh % kH)] = m; ++nh;

        // A pick attack whose note isn't known yet: wait for a window that holds only the new note,
        // then stamp the note at the attack (voices reading from there on switch at once).
        if (onset >= 0 && onset > resolved)
        {
            if (e.time - onset > (i64) (0.1 * sr)) resolved = onset;          // give up on this one
            else
            {
                // the window must hold the new note only; a very clean estimate may overlap the attack by a quarter
                const i64 windowStart = e.time - (i64) (e.span * (e.clarity >= 0.95f ? 0.75f : 1.f));
                if (windowStart < onset + (i64) (0.0005 * sr) || e.clarity < 0.86f || std::abs (m - (float) n) > 0.45f) return;
                if (n == cand) ++candN; else { cand = n; candN = 1; }
                if (candN < (e.clarity > 0.95f && std::abs (m - (float) n) < 0.3f ? 1 : 2)) return;
                candN = 0; resolved = onset; lastVoiced = e.time;
                tl.knownSince = std::max (tl.knownSince, onset);
                if (n == cur) { auto& s = tl.latest(); s.period = period; s.f0 = e.f0; return; }
                cur = n;
               #ifdef HYDRA_DEBUG
                std::printf ("  NOTE %d at onset %lld (est %lld) period %.1f clarity %.2f\n", n, (long long) onset, (long long) e.time, period, e.clarity);
               #endif
                tl.add (onset, n, period, e.f0, true);
                return;
            }
        }

        // No new attack: follow slides, hammer-ons and bends with hysteresis.
        const bool gap = cur < 0 || e.time - lastVoiced > (i64) (0.08 * sr);
        lastVoiced = e.time;
        if (! gap && std::abs (m - (float) cur) <= 0.70f)
        {
            auto& s = tl.latest(); s.period = period; s.f0 = e.f0; candN = 0;
            return;
        }
        if (std::abs (m - (float) n) > 0.45f || e.clarity < 0.84f) return;
        if (n == cand) ++candN; else { cand = n; candN = 1; }
        // an octave jump without a pick attack is usually the tracker, not the guitar: be sure first
        const bool octaveJump = cur >= 0 && std::abs (std::abs (n - cur) - 12) <= 0;
        if (candN < (gap ? 2 : (octaveJump ? 8 : 3))) return;
        candN = 0;
        if (n == cur) { auto& s = tl.latest(); s.period = period; s.f0 = e.f0; return; }
        // a hammer-on / pull-off jumps (the harmony jumps too); a bend or slide passes through (it glides)
        const bool sudden = cur >= 0 && nh > kH && std::abs (mAgo - (float) cur) < 0.4f;
        cur = n;
        const i64 start = e.time - (i64) (e.span * 0.5f);
       #ifdef HYDRA_DEBUG
        std::printf ("  NOTE %d at %lld (legato, est %lld) period %.1f clarity %.2f\n", n, (long long) start, (long long) e.time, period, e.clarity);
       #endif
        tl.add (start, n, period, e.f0, sudden);
    }
private:
    double sr = 48000;
    int cur = -1, cand = -1, candN = 0;
    i64 lastVoiced = 0, resolved = -1;
    static constexpr int kH = 8;
    std::array<float, kH> hist {}; int nh = 0;
};

// ---------------------------------------------------------------------------- one LEAD voice
struct VoiceSetup
{
    bool on = false, smart = true;
    int step = 2, semi = 4;
    float level = 0.7f, pan = 0, detune = 0, delayMs = 0;
};

struct Shared
{
    int key = 4, mode = 1; bool hold = false; float human = 0;
    i64 lastLoud = -1;          // the last sample of the signal above -70 dBFS
    float hint = 0;             // the tracker's latest raw period (host samples), for a note not confirmed yet
};

class LeadVoice
{
public:
    struct Stats { int jumps = 0, forced = 0, relocations = 0, postponed = 0; double minDelay = 1e9, maxDelay = 0, sumDelay = 0; i64 n = 0; };

    void prepare (double s, const Ring* rg, const Timeline* t, int index)
    {
        sr = s; ring = rg; tl = t; idx = index;
        absMin = Sinc::kHalf + 3;
        dmin = absMin + 0.0015 * sr;
        attack = 0.012 * sr;
        pDef = 0.010 * sr;
        maxExtra = 0.004 * sr;          // most delay added to let an attack pass
        wPitch.prepare (sr, 0.7, 11u + (unsigned) index * 7u);
        wTime.prepare (sr, 0.09, 101u + (unsigned) index * 13u);
        wLevel.prepare (sr, 0.35, 1001u + (unsigned) index * 17u);
        glideCoef = 0;
        reset (0);
    }
    void reset (i64 written)
    {
        r = (double) written - dmin - 2.0; fading = false; xfPos = 0; xfLen = 1;
        segId = -1; shift = 0; semis = 0; semisTarget = 0; rampLeft = 0; rampStep = 0;
        lastShift = 0; duck = 1; dUser = -1; dHum = 0; stats = {};
        humanCents = 0;
    }
    const Stats& getStats() const   { return stats; }
    double delay (i64 w) const      { return (double) w - r; }
    int currentShift() const        { return shift; }

    /** One output sample. w = samples written to the ring so far (the newest is w - 1). */
    inline float process (i64 w, const VoiceSetup& v, const Shared& sh)
    {
        lastLoud = sh.lastLoud; hintP = sh.hint;
        // ---- the note at the read position -> the interval
        const auto& seg = tl->at ((i64) r);
        if (seg.id != segId || v.smart != lastSmart || v.step != lastStep || v.semi != lastSemi || sh.key != lastKey || sh.mode != lastMode)
        {
            const bool noteChange = seg.id != segId;
            segId = seg.id; lastSmart = v.smart; lastStep = v.step; lastSemi = v.semi; lastKey = sh.key; lastMode = sh.mode;
            int s = v.smart ? (seg.note >= 0 ? hd::smartShift (seg.note, v.step, sh.key, sh.mode, sh.hold, lastShift) : lastShift) : v.semi;
            if (v.smart && seg.note >= 0) lastShift = s;
            if (s != shift || rampLeft == 0)
            {
                shift = s;
                // a picked note changes at once (it starts there), a slide or bend glides over
                const double ms = noteChange && seg.picked ? 1.5 : (noteChange ? 25.0 : 12.0);
                rampLeft = std::max (1, (int) (ms * 0.001 * sr));
                rampStep = ((float) shift - semis) / (float) rampLeft;
            }
        }
        if (rampLeft > 0) { semis += rampStep; if (--rampLeft == 0) semis = (float) shift; }

        // ---- humanize: drifting pitch, timing and level
        const float h = sh.human;
        float hc = 0, ht = 0;
        if (h > 0.001f) { hc = wPitch.next() * 9.f * h; ht = (wTime.next() * 0.5f + 0.5f) * 7.f * h; humLevel = 1.f + wLevel.next() * 0.16f * h; }
        else humLevel = 1.f;
        const double dHumNew = ht * 0.001 * sr;
        const double dHumRate = dHumNew - dHum; dHum = dHumNew;

        const float st = semis + (v.detune + hc) * 0.01f;
        const double ratio = std::exp2 ((double) st / 12.0);
        const double step = ratio - dHumRate;
        const double target = std::exp2 ((double) ((float) shift + (v.detune + hc) * 0.01f) / 12.0) - dHumRate;
        table = Sinc::cutoffFor (ratio);

        // ---- the user's delay (a change moves the head with a splice)
        const double dU = (double) v.delayMs * 0.001 * sr;
        if (dUser < 0) { dUser = dU; r = (double) w - (dmin + dU + dHum) - 2.0; }
        if (std::abs (dU - dUser) > 0.5) dUser = dU;

        // ---- advance and read
        r += step;
        float y;
        if (fading)
        {
            r2 += step;
            // the pitch went up during the crossfade and the old head is about to run out: finish it quickly
            if ((double) w - r < absMin + 3.0 && xfLen - xfPos > 16) { const int left = 16; xfLen = xfPos + left; }
            const float g = 0.5f - 0.5f * std::cos ((float) kPiD * (float) xfPos / (float) xfLen);
            y = ring->read (r, table) * (1.f - g) + ring->read (r2, table) * g;
            if (++xfPos >= xfLen) { r = r2; fading = false; }
        }
        else y = ring->read (r, table);

        // ---- keep the head in its window
        if (! fading) plan (w, seg, step, target);
        guard (w);

        // ---- until the new note after an attack is known, a smart voice dips a little
        float dk = 1.f;
        if (v.smart)
        {
            const i64 o = tl->lastOnsetAtOrBefore ((i64) r);
            if (o >= 0 && o > tl->knownSince && (double) ((i64) r - o) < 0.045 * sr) dk = 0.55f;
        }
        duck += (dk - duck) * (dk < duck ? duckDown : duckUp);

        const double d = (double) w - r;
        stats.minDelay = std::min (stats.minDelay, d); stats.maxDelay = std::max (stats.maxDelay, d); stats.sumDelay += d; ++stats.n;
        return y * duck * humLevel;
    }

    void setRates()
    {
        duckDown = (float) (1.0 - std::exp (-1.0 / (0.0015 * sr)));
        duckUp = (float) (1.0 - std::exp (-1.0 / (0.004 * sr)));
    }

private:
    void plan (i64 w, const Timeline::Segment& seg, double step, double target)
    {
        const double d = (double) w - r;
        // a new note was picked here and isn't known yet: the segment's period is the old note's
        const i64 o0 = tl->lastOnsetAtOrBefore ((i64) r);
        const bool pending = o0 >= 0 && o0 > tl->knownSince && o0 > seg.start;
        const float hp = pending ? hintP : 0.f;
        const bool voiced = pending ? hp > 8.f : (seg.note >= 0 && seg.period > 8.f);
        const double P = voiced ? (double) (pending ? hp : seg.period) : pDef;
        const double X = voiced ? clampd (2.0 * P, 0.0015 * sr, 0.010 * sr) : 0.008 * sr;
        const double lo = dmin + dUser + dHum;
        const i64 ri = (i64) r;
        const int range = voiced ? 0 : (int) (0.5 * pDef);
        const double L = voiced ? clampd (P, 0.002 * sr, 0.010 * sr) : 0.006 * sr;
        auto rangeFor = [&] (double J) { return voiced ? (int) std::max (2.0, 0.06 * std::abs (J) + 2.0) : range; };

        // reading silence: just put the head where it belongs (nothing to splice)
        if (lastLoud >= 0 && (double) (w - lastLoud) > d + 0.004 * sr)
        {
            const double want = lo + 0.5 * P;
            if (std::abs (d - want) > 0.5 * P + 2.0) { r = (double) w - want; return; }
        }
        // the window moved (a new DELAY, or it was left behind): get back into it
        const double upMost = std::max (step, target) - 1.0, downMost = std::min (step, target) - 1.0;
        {
            const double hiWin = lo + 2.0 * P + X * std::max (std::abs (upMost), std::abs (downMost)) + 0.004 * sr;
            if (d < lo - P - 2.0 || d > hiWin + 0.020 * sr) { relocate (w, d, lo + 0.5 * P, voiced, P, X, step); return; }
        }

        if (upMost > 1e-5)
        {
            const double drift = upMost;          // plan with the pitch it's gliding to, if that's higher
            int k = std::max (1, (int) std::ceil ((X * drift * 1.12 + 2.0) / P));
            const bool need = d - X * drift < lo;
            const bool hard = d - X * drift < absMin + 2.0;
            bool pre = false; double Xp = X;
            // an attack is coming: make room now so it can pass without a splice
            const double A = std::min (attack, maxExtra / drift);
            const i64 o = tl->nextOnsetAfter (ri);
            if (o >= 0)
            {
                const double steps = (double) (o - ri) / std::max (step, 1e-3);
                const double atOnset = d - drift * steps;
                const double needed = lo + drift * (A + X);
                if (atOnset < needed && steps > 0.0015 * sr * 0.5 + 6.0)
                {
                    pre = true;
                    Xp = std::min (X, steps - 4.0);                  // the splice ends before the attack
                    k = std::max (k, (int) std::ceil ((needed - atOnset + 1.0) / P));
                }
            }
            if (! (need || pre || hard)) return;
            const double J = k * P;
            const double Xs = pre && ! need ? Xp : X;
            const bool clash = tl->onsetIn (ri - (i64) (J + Xs) - 2, ri + (i64) (step * Xs) + 2);
            if (clash && ! hard) { ++stats.postponed; return; }
            if (hard && clash) ++stats.forced;
            const double Xf = clampd (std::min (Xs, (d - absMin - 4.0) / drift), 16.0, X);   // must end before the head runs out
            splice (w, -J, rangeFor (J), L, Xf, step);
        }
        else if (downMost < -1e-5)
        {
            const double drift = downMost;
            const int k = std::max (1, (int) std::ceil ((X * -drift * 1.12 + 2.0) / P));
            const double J = k * P;
            const bool need = d - J >= lo;
            const bool hard = d > lo + J + 0.060 * sr;
            if (! (need || hard)) return;
            const double A = std::min (attack, maxExtra / -drift);
            const bool clash = tl->onsetIn (ri - (i64) A, ri + (i64) (J + step * X) + 2);
            if (clash && ! hard) { ++stats.postponed; return; }
            if (hard && clash) ++stats.forced;
            splice (w, J, rangeFor (J), L, X, step);
        }
        else
        {
            // unison: only move when the window moved
            const double hi = lo + P + 0.001 * sr;
            if (d < lo - 0.5 || d > hi) relocate (w, d, lo + 0.5 * P, voiced, P, X, step);
        }
    }

    void relocate (i64 w, double d, double target, bool voiced, double P, double X, double step)
    {
        double J = d - target;                          // + : move forward (less delay)
        if (voiced) { const double k = std::max (1.0, std::round (std::abs (J) / P)); J = (J < 0 ? -k : k) * P; }
        ++stats.relocations;
        splice (w, J, voiced ? (int) std::max (2.0, 0.06 * P + 2.0) : (int) (0.5 * pDef), voiced ? clampd (P, 0.002 * sr, 0.010 * sr) : 0.006 * sr, X, step);
    }

    /** Start a crossfade to a head `J` samples away (J < 0: back in time), aligned by
        cross-correlation within +-range of J. */
    void splice (i64 w, double J, int range, double L, double X, double step)
    {
        const i64 ri = (i64) std::floor (r);
        const int len = std::max (16, (int) L);
        const i64 newest = w - 1 - Sinc::kHalf - 2;            // the furthest forward a head may read
        const i64 oldest = w - ring->capacity() + len + Sinc::kHalf + 8;
        const i64 j0 = (i64) std::llround (J);
        double best = (double) j0; float bestC = -2.f;
        float cs[512]; int nc = 0; i64 firstOff = 0;
        const float* a = ring->at (ri - len);
        double ea = 0; for (int i = 0; i < len; ++i) ea += (double) a[i] * a[i];
        range = std::min (range, 250);
        for (int off = -range; off <= range; ++off)
        {
            const i64 jb = j0 + off;
            const i64 pos = ri + jb;
            if (pos > newest - (i64) (std::max (0.0, step - 1.0) * X) || pos - len < oldest) { if (nc == 0) firstOff = off + 1; continue; }
            if (nc == 0) firstOff = off;
            const float* b = ring->at (pos - len);
            double ab = 0, eb = 0;
            for (int i = 0; i < len; ++i) { ab += (double) a[i] * b[i]; eb += (double) b[i] * b[i]; }
            const float c = (ea > 1e-12 && eb > 1e-12) ? (float) (ab / std::sqrt (ea * eb)) : 0.f;
            cs[nc++] = c;
            if (c > bestC) { bestC = c; best = (double) jb; }
        }
        if (nc == 0) return;                                    // nowhere to go (shouldn't happen)
        if (bestC < 0.05f) best = (double) j0;                  // nothing to line up (silence): keep the planned jump
        // sub-sample refine
        const int bi = (int) (best - (double) (j0 + firstOff));
        if (bestC >= 0.05f && bi > 0 && bi < nc - 1)
        {
            const double y0 = cs[bi - 1], y1 = cs[bi], y2 = cs[bi + 1], den = y0 - 2.0 * y1 + y2;
            if (std::abs (den) > 1e-9) best += clampd (0.5 * (y0 - y2) / den, -0.5, 0.5);
        }
        r2 = r + best;
        xfLen = std::max (8, (int) X); xfPos = 0; fading = true;
       #ifdef HYDRA_DEBUG
        std::printf ("    splice at w=%lld d=%.1f J=%.1f -> %.2f (c=%.3f, range %d, len %d, X %d)\n", (long long) w, (double) w - r, J, best, bestC, range, len, xfLen);
       #endif
        ++stats.jumps;
    }

    /** Never read past the newest sample or off the end of the buffer. */
    void guard (i64 w)
    {
        const double newest = (double) (w - 1 - Sinc::kHalf - 1);
        const double oldest = (double) (w - ring->capacity() + Sinc::kHalf + 4);
        if (r > newest)
        {
           #ifdef HYDRA_DEBUG
            if (stats.forced % 50 == 0) std::printf ("    FORCED at w=%lld d=%.1f fading=%d\n", (long long) w, (double) w - r, (int) fading);
           #endif
            r = newest; ++stats.forced;
        }
        if (r < oldest) { r = oldest; ++stats.forced; }
        if (fading) { if (r2 > newest) r2 = newest; if (r2 < oldest) r2 = oldest; }
    }

    double sr = 48000;
    const Ring* ring = nullptr; const Timeline* tl = nullptr; int idx = 0;
    double r = 0, r2 = 0; bool fading = false; int xfPos = 0, xfLen = 1;
    double absMin = 19, dmin = 90, attack = 576, pDef = 384, maxExtra = 192;
    int table = 0;
    int segId = -1, shift = 0, lastShift = 0, rampLeft = 0;
    bool lastSmart = true; int lastStep = 999, lastSemi = 999, lastKey = -1, lastMode = -1;
    float semis = 0, semisTarget = 0, rampStep = 0, glideCoef = 0;
    float duck = 1, duckDown = 0.01f, duckUp = 0.005f, humLevel = 1, humanCents = 0;
    double dUser = -1, dHum = 0;
    i64 lastLoud = -1; float hintP = 0;
    Wander wPitch, wTime, wLevel;
    Stats stats;
};

// ---------------------------------------------------------------------------- CHORDS
/** Finds the notes of a chord on the clean guitar: spectral peaks, then (Klapuri style) the note
    with the strongest harmonic series, its partials taken away, the next, ... up to six. */
class ChordDetector
{
public:
    static constexpr int kMaxNotes = 6;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        decim = std::max (1, (int) std::lround (sr / 12000.0));
        dsr = sr / decim;
        aa1.set (Biquad::LP, sr, dsr * 0.42, 0.5412); aa2.set (Biquad::LP, sr, dsr * 0.42, 1.3066);
        hp.set (Biquad::HP, sr, 50.0, 0.707);
        for (int i = 0; i < kN; ++i) win[i] = (float) (0.5 - 0.5 * std::cos (2.0 * kPiD * (i + 0.5) / kN));
        buildFft();
        reset();
    }
    void reset()
    {
        aa1.reset(); aa2.reset(); hp.reset();
        std::fill (std::begin (ring), std::end (ring), 0.f);
        pos = 0; phase = 0; hopCount = 0;
        for (auto& c : cand) c = {};
        nActive = 0;
    }
    /** Host-rate clean guitar in; true when the chord was re-analysed. */
    inline bool push (float x)
    {
        const float s = aa2.process (aa1.process (hp.process (x)));
        if (++phase < decim) return false;
        phase = 0;
        ring[pos] = s; pos = (pos + 1) & (kRing - 1);
        if (++hopCount < kHop) return false;
        hopCount = 0;
        analyse();
        return true;
    }
    int count() const               { return nActive; }
    float noteHz (int i) const      { return activeHz[i]; }
    int noteMidi (int i) const      { return activeMidi[i]; }
    float noteWeight (int i) const  { return activeW[i]; }
    /** Latency of the analysis (centre of its window), host samples. */
    double latency() const          { return (kN * 0.5) * decim; }

private:
    static constexpr int kN = 2048, kHop = 128, kRing = 4096, kMidiLo = 35, kMidiHi = 90, kNotes = kMidiHi - kMidiLo + 1;
    struct Cand { float hold = 0; float hz = 0; float w = 0; int on = 0, miss = 0; };

    void buildFft()
    {
        for (int i = 0; i < kN / 2; ++i) { tw[i] = std::complex<float> ((float) std::cos (2.0 * kPiD * i / kN), (float) -std::sin (2.0 * kPiD * i / kN)); }
        int bits = 0; while ((1 << bits) < kN) ++bits;
        for (int i = 0; i < kN; ++i) { int rv = 0; for (int b = 0; b < bits; ++b) if (i & (1 << b)) rv |= 1 << (bits - 1 - b); rev[i] = rv; }
    }
    void fft (std::complex<float>* a)
    {
        for (int i = 0; i < kN; ++i) if (i < rev[i]) std::swap (a[i], a[rev[i]]);
        for (int len = 2; len <= kN; len <<= 1)
        {
            const int half = len >> 1, stp = kN / len;
            for (int i = 0; i < kN; i += len)
                for (int j = 0; j < half; ++j)
                {
                    const auto u = a[i + j], v = a[i + j + half] * tw[j * stp];
                    a[i + j] = u + v; a[i + j + half] = u - v;
                }
        }
    }

    void analyse()
    {
        for (int i = 0; i < kN; ++i) buf[i] = std::complex<float> (ring[(pos - kN + i) & (kRing - 1)] * win[i], 0.f);
        fft (buf);
        const int nb = kN / 2;
        double total = 0;
        for (int k = 0; k < nb; ++k) { mag[k] = std::abs (buf[k]); total += (double) mag[k] * mag[k]; }
        const float rms = (float) std::sqrt (total / nb);
        // peaks (parabolic on log magnitude)
        nPk = 0;
        float mxm = 0; for (int k = 2; k < nb - 1; ++k) mxm = std::max (mxm, mag[k]);
        const float floorMag = std::max (mxm * 0.003f, 1e-6f);
        for (int k = 2; k < nb - 2 && nPk < kPk; ++k)
            if (mag[k] > floorMag && mag[k] >= mag[k - 1] && mag[k] > mag[k + 1])
            {
                const double a = std::log (mag[k - 1] + 1e-12), b = std::log (mag[k] + 1e-12), c = std::log (mag[k + 1] + 1e-12);
                const double den = a - 2 * b + c, dl = std::abs (den) > 1e-12 ? clampd (0.5 * (a - c) / den, -0.5, 0.5) : 0.0;
                pkHz[nPk] = (float) ((k + dl) * dsr / kN);
                pkA[nPk] = (float) std::exp (b - 0.25 * (a - c) * dl);
                ++nPk;
            }
        // compress (so one loud partial can't take over) and iterate
        for (int i = 0; i < nPk; ++i) { pkW[i] = std::sqrt (pkA[i]); explained[i] = false; }
        int found = 0; float foundHz[kMaxNotes]; float foundW[kMaxNotes];
        float first = 0;
        const float binHz = (float) (dsr / kN);
        // is peak i a partial of note f0? (tolerance: 3 %, at least most of a bin, as low partials are coarse)
        auto near = [&] (float pk, float target) { return std::abs (pk - target) < std::max (target * 0.025f, binHz * 0.35f); };
        if (rms > 1e-5f)
            for (int it = 0; it < kMaxNotes; ++it)
            {
                int bestN = -1; float bestS = 0, bestHz = 0, bestOwn = 0;
                for (int n = 0; n < kNotes; ++n)
                {
                    const float hz0 = 440.f * std::pow (2.f, (kMidiLo + n - 69) / 12.f);
                    float s = 0, own = 0, f0 = hz0, odd = 0, even = 0; bool hasLow = false;
                    for (int h = 1; h <= 12; ++h)
                    {
                        const float target = f0 * h;
                        if (target > dsr * 0.45) break;
                        float best = 0; int bi = -1;
                        for (int i = 0; i < nPk; ++i) if (near (pkHz[i], target) && pkW[i] > best) { best = pkW[i]; bi = i; }
                        if (bi < 0) continue;
                        const float wgt = 1.f / (0.6f + 0.4f * h);
                        s += best * wgt;
                        (h % 2 ? odd : even) += best * wgt;
                        if (! explained[bi]) own += best * wgt;           // partials no note found so far accounts for
                        if (h <= 3) hasLow = true;
                        if (h == 1 && target > 3.f * binHz) f0 = pkHz[bi];
                    }
                    if (! hasLow || odd < 0.3f * even) continue;     // no odd partials: the octave above it is what's playing
                    if (s > bestS) { bestS = s; bestN = n; bestHz = f0; bestOwn = own; }
                }
                if (bestN < 0) break;
                if (it == 0) first = bestS;
                else if (bestS < 0.28f * first || bestOwn < 0.45f * bestS || bestOwn < 0.15f * first) break;   // mostly someone else's partials
                foundHz[found] = bestHz; foundW[found] = bestS; ++found;
                for (int h = 1; h <= 16; ++h)
                {
                    const float target = bestHz * h; if (target > dsr * 0.45) break;
                    for (int i = 0; i < nPk; ++i) if (near (pkHz[i], target)) { pkW[i] *= 0.3f; explained[i] = true; }
                }
            }
        // hysteresis over frames, by MIDI note
        bool seen[kNotes] = {};
        for (int i = 0; i < found; ++i)
        {
            const int n = (int) std::lround (12.0 * std::log2 (foundHz[i] / 440.0) + 69.0) - kMidiLo;
            if (n < 0 || n >= kNotes) continue;
            seen[n] = true;
            auto& c = cand[n]; c.hz = foundHz[i]; c.w = foundW[i]; c.miss = 0; c.on = std::min (c.on + 1, 8);
        }
        for (int n = 0; n < kNotes; ++n) if (! seen[n]) { auto& c = cand[n]; if (c.on > 0 && ++c.miss > 3) { c.on = 0; c.miss = 0; } }
        nActive = 0;
        for (int n = 0; n < kNotes && nActive < kMaxNotes; ++n)
            if (cand[n].on >= 2) { activeHz[nActive] = cand[n].hz; activeMidi[nActive] = n + kMidiLo; activeW[nActive] = cand[n].w; ++nActive; }
    }

    double sr = 48000, dsr = 12000;
    int decim = 4, pos = 0, phase = 0, hopCount = 0;
    Biquad aa1, aa2, hp;
    float ring[kRing] {};
    float win[kN] {};
    std::complex<float> buf[kN] {}, tw[kN / 2] {};
    int rev[kN] {};
    float mag[kN / 2] {};
    static constexpr int kPk = 160;
    float pkHz[kPk] {}, pkA[kPk] {}, pkW[kPk] {}; bool explained[kPk] {};
    int nPk = 0;
    Cand cand[kNotes] {};
    int nActive = 0; float activeHz[kMaxNotes] {}, activeW[kMaxNotes] {}; int activeMidi[kMaxNotes] {};
};

/** A CHORDS voice: Signalsmith Stretch with a frequency map from the chord's notes. */
class ChordVoice
{
public:
    void prepare (double s, int maxBlock)
    {
        sr = s;
        st.configure (1, (int) (sr * 0.085), (int) (sr * 0.021));
        st.reset();
        out.assign ((size_t) std::max (maxBlock, 64), 0.f);
        lat = st.inputLatency() + st.outputLatency();
        dl.assign ((size_t) (0.25 * sr) + 8, 0.f); dw = 0;
        wPitch.prepare (sr, 0.7, 77u); wLevel.prepare (sr, 0.35, 777u);
        st.setFreqMap ([this] (float f) { return map (f); });
    }
    void reset()                    { st.reset(); std::fill (dl.begin(), dl.end(), 0.f); }
    int latency() const             { return lat; }
    int seekLength() const          { return st.seekLength(); }
    /** Start (again) with the signal's recent past already inside, so the output fades in from real
        sound instead of jumping in. */
    void startFrom (const float* history, int len)
    {
        st.reset();
        const float* in[1] = { history };
        st.seek (in, len, 1.0);
        std::fill (dl.begin(), dl.end(), 0.f);
    }

    /** Set the notes and each one's shift (semitones), once per block. */
    void setNotes (int n, const float* hz, const float* shiftSemis, float fallbackShift, float detuneCents)
    {
        nNotes = n;
        for (int i = 0; i < n; ++i) { notesHz[i] = hz[i]; ratios[i] = std::exp2 ((shiftSemis[i] + detuneCents * 0.01f) / 12.f); }
        fallback = std::exp2 ((fallbackShift + detuneCents * 0.01f) / 12.f);
        uniform = true;
        for (int i = 0; i < n; ++i) if (std::abs (ratios[i] - fallback) > 1e-4f) uniform = false;
    }
    /** Shift a block (mono in, mono out into y, with the voice's extra delay). */
    void process (const float* x, float* y, int n, float delayMs)
    {
        float* in[1] = { const_cast<float*> (x) };
        float* o[1] = { out.data() };
        lastIn = 0; lastOut = 0;
        st.process (in, n, o, n);
        const int D = (int) clampd (delayMs * 0.001 * sr, 0, (double) dl.size() - 2);
        for (int i = 0; i < n; ++i)
        {
            float v = out[(size_t) i]; if (! std::isfinite (v)) v = 0;
            dl[(size_t) dw] = v;
            int rd = dw - D; if (rd < 0) rd += (int) dl.size();
            y[i] = dl[(size_t) rd];
            if (++dw >= (int) dl.size()) dw = 0;
        }
    }
private:
    float map (float f)
    {
        // f: cycles per sample. Called for each spectral peak in rising order: keep the map rising.
        if (f < lastIn) lastOut = 0;
        lastIn = f;
        float r = fallback;
        if (! uniform && nNotes > 0)
        {
            const float hz = f * (float) sr;
            float bestScore = 1e9f;
            for (int i = 0; i < nNotes; ++i)
            {
                const float h = std::max (1.f, std::round (hz / notesHz[i]));
                const float cents = std::abs (1200.f * std::log2 (hz / (h * notesHz[i])));
                const float tol = 35.f + 4.f * h;                      // strings are a little inharmonic
                if (cents < tol)
                {
                    const float score = cents + 3.f * h;                  // the closest partial wins, lower ones a little more
                    if (score < bestScore) { bestScore = score; r = ratios[i]; }
                }
            }
            if (bestScore > 1e8f)
            {
                // not a partial of any note: use the note just below it
                float below = 0;
                for (int i = 0; i < nNotes; ++i) if (notesHz[i] <= hz && notesHz[i] > below) { below = notesHz[i]; r = ratios[i]; }
            }
        }
        float o = f * r;
       #ifdef HYDRA_DEBUG
        static int dbgN = 0;
        if (getenv ("MAPDBG") && f * sr > 150 && f * sr < 1300 && (++dbgN % 7) == 0) std::printf ("      map %.1f Hz -> x%.3f (notes %d, uniform %d)\n", f * sr, r, nNotes, (int) uniform);
       #endif
        if (o <= lastOut) o = lastOut + 1e-6f;
        lastOut = o;
        return o;
    }

    double sr = 48000;
    signalsmith::stretch::SignalsmithStretch<float> st { 0x48a7 };
    std::vector<float> out, dl; int dw = 0;
    int lat = 0;
    int nNotes = 0; float notesHz[ChordDetector::kMaxNotes] {}, ratios[ChordDetector::kMaxNotes] {};
    float fallback = 1, lastIn = 0, lastOut = 0; bool uniform = true;
    Wander wPitch, wLevel;
};

// ---------------------------------------------------------------------------- the whole section
struct Params
{
    bool on = false;
    int key = 4, mode = 1, track = 0, place = 0; bool hold = false;
    float human = 0.15f, spread = 0.5f, mix = 0.5f, outDb = 0;
    VoiceSetup v[kVoices];
    float transpose = 0;            // the plugin's transpose (applied before), so notes match what you hear
};

class Harmonizer
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate; blk = std::max (16, maxBlock);
        ring.prepare ((int) (0.26 * sr) + 8192);
        tracker.prepare (sr); follower.prepare (sr); tl.reset();
        for (int i = 0; i < kVoices; ++i) { lead[i].prepare (sr, &ring, &tl, i); lead[i].setRates(); }
        chords.prepare (sr);
        for (int i = 0; i < kVoices; ++i) chordV[i].prepare (sr, blk);
        mono.assign ((size_t) blk, 0.f); vbuf.assign ((size_t) blk, 0.f);
        for (auto& b : cbuf) b.assign ((size_t) blk, 0.f);
        const double c20 = 1.0 - std::exp (-1.0 / (0.020 * sr));
        smoothC = (float) c20;
        secStep = (float) (1.0 / (0.025 * sr));
        reset();
    }
    void reset()
    {
        ring.clear(); tracker.reset(); follower.reset(); tl.reset(); chords.reset();
        for (auto& v : lead) v.reset (ring.written());
        for (auto& v : chordV) v.reset();
        for (auto& g : vg) g = 0; for (auto& g : cg) g = 0;
        for (int k = 0; k < kVoices; ++k) { chordIdle[k] = true; chordWarm[k] = 0; }
        secGain = 0; now = 0; warm = 0;
        for (int i = 0; i < kVoices; ++i) { panL[i] = panR[i] = 0.70710678f; }
    }
    /** Faded out (switched off or moving): safe to move or reset. */
    bool isQuiet() const            { return secGain <= 0.f; }
    /** Latency of the CHORDS voices (the LEAD voices trail by only their splice window). */
    int chordLatency() const        { return chordV[0].latency(); }
    const LeadVoice& leadVoice (int i) const { return lead[i]; }
    const Timeline& timeline() const { return tl; }

    /** di: the clean guitar (for listening). inL/inR: the signal to harmonize (inR may be null:
        mono). Writes outL/outR (outR may be null), may be the same buffers as inL/inR. */
    void process (const float* di, const float* inL, const float* inR, float* outL, float* outR, int n, const Params& p)
    {
        for (int start = 0; start < n; start += kSub)
        {
            const int m = std::min (kSub, n - start);
            run (di + start, inL + start, inR ? inR + start : nullptr, outL + start, outR ? outR + start : nullptr, m, p);
        }
    }

    // for the UI
    std::atomic<int> uiNote { -1 };
    std::atomic<int> uiShift[kVoices] {};
    std::atomic<int> uiChord[ChordDetector::kMaxNotes] {};
    std::atomic<int> uiChordCount { 0 };

private:
    static constexpr int kSub = 32;

    void run (const float* di, const float* inL, const float* inR, float* outL, float* outR, int n, const Params& p)
    {
        const bool chordMode = p.track == 1;
        Shared sh; sh.key = p.key; sh.mode = p.mode; sh.hold = p.hold; sh.human = p.human;

        // section on/off fade
        const float secTarget = p.on ? 1.f : 0.f;
        if (! p.on && secGain <= 0.f)
        {
            // off: pass through, keep the signal history and the tracker warm-free
            for (int i = 0; i < n; ++i)
            {
                ring.push (inR ? 0.5f * (inL[i] + inR[i]) : inL[i]);
                outL[i] = inL[i]; if (outR) outR[i] = inR ? inR[i] : inL[i];
                ++now;
            }
            secGain = 0; warm = 0;
            for (auto& v : lead) v.reset (ring.written());
            for (auto& g : vg) g = 0; for (auto& g : cg) g = 0;
            return;
        }

        // listen
        for (int i = 0; i < n; ++i)
        {
            bool onset = false;
            if (tracker.push (di[i], now + i, onset))
            {
                const auto& e = tracker.est();
                follower.update (e, p.transpose, tl);
                hint = e.clarity >= 0.6f && e.period > 0 ? e.period / std::exp2 (p.transpose / 12.f) : 0.f;
            }
            if (onset)
            {
                tl.addOnset (now + i - (i64) (0.0006 * sr));
               #ifdef HYDRA_DEBUG
                std::printf ("  ONSET at %lld\n", (long long) (now + i));
               #endif
            }
            if (chordMode) chords.push (di[i]);
        }
        if (now - follower.lastVoicedTime() > (i64) (0.12 * sr)) uiNote.store (-1); else uiNote.store (follower.current());

        // the signal to shift (mono)
        for (int i = 0; i < n; ++i) mono[(size_t) i] = inR ? 0.5f * (inL[i] + inR[i]) : inL[i];

        // mix law: both at full in the middle
        const float mix = clampf (p.mix, 0.f, 1.f);
        const float dryG = mix <= 0.5f ? 1.f : 2.f * (1.f - mix);
        const float wetG = mix >= 0.5f ? 1.f : 2.f * mix;
        const float outG = dbToGain (p.outDb);
        const float spread = clampf (p.spread, 0.f, 1.f) * 2.f;
        float tL[kVoices], tR[kVoices], tg[kVoices], tcg[kVoices];
        for (int k = 0; k < kVoices; ++k)
        {
            const auto& v = p.v[k];
            const float pn = clampf (v.pan * 0.01f * spread, -1.f, 1.f);
            const float a = (pn + 1.f) * 0.25f * (float) kPiD;
            tL[k] = std::cos (a); tR[k] = std::sin (a);
            const float lvl = v.on ? clampf (v.level, 0.f, 1.f) * wetG : 0.f;
            tg[k] = chordMode ? 0.f : lvl;
            tcg[k] = chordMode ? lvl : 0.f;
        }
        // warm-up after switching on: let the tracker and the history fill
        if (warm < (i64) (0.03 * sr)) { warm += n; for (int k = 0; k < kVoices; ++k) { tg[k] = 0; tcg[k] = 0; } }

        // CHORDS voices run block-wise
        if (chordMode || anyAbove (cg, 1e-5f))
        {
            const int nc = chords.count();
            float hz[ChordDetector::kMaxNotes], sh2[ChordDetector::kMaxNotes];
            for (int k = 0; k < kVoices; ++k)
            {
                if (tcg[k] <= 0 && cg[k] < 1e-5f) { chordIdle[k] = true; continue; }
                if (chordIdle[k])
                {
                    // starting up: clear the old state and stay silent until its first output is real
                    chordIdle[k] = false;
                    const int len = chordV[k].seekLength();
                    chordV[k].startFrom (ring.at (ring.written() - len), len);
                    chordWarm[k] = chordV[k].latency() + n + 64;
                }
                if (chordWarm[k] > 0) { chordWarm[k] -= n; tcg[k] = 0.f; }
                const auto& v = p.v[k];
                int fallback = v.smart ? 0 : v.semi;
                for (int i = 0; i < nc; ++i)
                {
                    hz[i] = chords.noteHz (i) * std::exp2 (p.transpose / 12.f);
                    const int note = (int) std::lround (12.0 * std::log2 (hz[i] / 440.0) + 69.0);
                    const int s = v.smart ? hd::smartShift (note, v.step, p.key, p.mode, p.hold, chordLast[k][i]) : v.semi;
                    chordLast[k][i] = s; sh2[i] = (float) s;
                    if (i == 0) fallback = s;
                }
                if (v.smart && nc == 0) fallback = lastFallback[k];
                lastFallback[k] = fallback;
                chordV[k].setNotes (nc, hz, sh2, (float) fallback, v.detune);
                chordV[k].process (mono.data(), cbuf[(size_t) k].data(), n, v.delayMs);
            }
            uiChordCount.store (nc);
            for (int i = 0; i < ChordDetector::kMaxNotes; ++i) uiChord[i].store (i < nc ? (int) std::lround (12.0 * std::log2 (chords.noteHz (i) * std::exp2 (p.transpose / 12.f) / 440.0) + 69.0) : -1);
        }
        else uiChordCount.store (0);

        for (int i = 0; i < n; ++i)
        {
            ring.push (mono[(size_t) i]);
            const i64 w = ring.written();
            if (std::abs (mono[(size_t) i]) > 3.2e-4f) lastLoud = w - 1;
            sh.lastLoud = lastLoud; sh.hint = hint;
            float wl = 0, wr = 0, wm = 0;
            for (int k = 0; k < kVoices; ++k)
            {
                vg[k] += (tg[k] - vg[k]) * smoothC;
                cg[k] += (tcg[k] - cg[k]) * smoothC;
                panL[k] += (tL[k] - panL[k]) * smoothC; panR[k] += (tR[k] - panR[k]) * smoothC;
                float y = 0;
                if (vg[k] > 1e-5f || tg[k] > 0) y += lead[k].process (w, p.v[k], sh) * vg[k];
                else if (! chordMode || cg[k] < 1e-5f) lead[k].reset (w);
                if (cg[k] > 1e-5f) y += cbuf[(size_t) k][(size_t) i] * cg[k];
                wl += y * panL[k]; wr += y * panR[k]; wm += y;
            }
            // the section fades linearly (25 ms), so switched off it reaches exactly 0 and steps aside
            secGain = secTarget > secGain ? std::min (secTarget, secGain + secStep) : std::max (secTarget, secGain - secStep);
            const float dl = inL[i], dr = inR ? inR[i] : inL[i];
            if (outR)
            {
                const float pl = (dl * dryG + wl * 1.41421356f) * outG, pr = (dr * dryG + wr * 1.41421356f) * outG;
                outL[i] = dl + (pl - dl) * secGain; outR[i] = dr + (pr - dr) * secGain;
            }
            else
            {
                const float pm = (dl * dryG + wm) * outG;          // before the amp: mono, no panning
                outL[i] = dl + (pm - dl) * secGain;
            }
        }
        for (int k = 0; k < kVoices; ++k) uiShift[k].store (lead[k].currentShift());
       #ifdef HYDRA_DEBUG
        if (getenv ("CGDBG") && now % 480 < (i64) n && now > (i64) (3.55 * sr) && now < (i64) (3.8 * sr))
            std::printf ("  t=%.3f cg0=%.4f vg0=%.4f tcg0=%.3f warm=%d idle=%d sec=%.3f\n", now / sr, cg[0], vg[0], tcg[0], chordWarm[0], (int) chordIdle[0], secGain);
       #endif
        now += n;
    }
    static bool anyAbove (const float* g, float t) { for (int k = 0; k < kVoices; ++k) if (g[k] > t) return true; return false; }

    double sr = 48000; int blk = 512;
    Ring ring; FastTracker tracker; NoteFollower follower; Timeline tl;
    LeadVoice lead[kVoices];
    ChordDetector chords; ChordVoice chordV[kVoices];
    std::vector<float> mono, vbuf; std::array<std::vector<float>, kVoices> cbuf;
    float vg[kVoices] {}, cg[kVoices] {}, panL[kVoices] {}, panR[kVoices] {};
    int chordLast[kVoices][ChordDetector::kMaxNotes] {}; int lastFallback[kVoices] {};
    bool chordIdle[kVoices] { true, true, true, true, true, true }; int chordWarm[kVoices] {};
    float secGain = 0, smoothC = 0.001f, secStep = 0.001f;
    i64 now = 0, warm = 0, lastLoud = -1;
    float hint = 0;
};
} // namespace obs::dsp::hydra
