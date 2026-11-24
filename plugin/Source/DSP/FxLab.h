#pragma once
// =============================================================================
//  The SFX tab's GLITCH page: KERAUNOS and KHAOS.
//
//  KERAUNOS  a multi-effect expression pedal. One "position" (0..1) sweeps the
//            effect: whammy up / down, dive bomb, fifth, oct + 5th, squeal, detune
//            drop, laser (a resonant comb that zaps up), zap comb (phaser + comb),
//            frequency shift, ring mod (swept, or tracking the note you play),
//            wah, talk box, filter, bit crush. The position comes from the treadle
//            (PEDAL), the playing (ENVELOPE), an LFO (AUTO) or a sweep fired by each
//            pick attack or the TRIGGER button (TRIGGER).
//  KHAOS     a tempo-synced glitch engine. Every step of the grid it may (RANDOM)
//            or must (SEQUENCE, 16 steps) play a glitch made from the last bars it
//            heard: stutter, roll, reverse, scatter, skip, freeze, pitch, tape stop,
//            spin up, half speed, warble, crush, chop, dropout, data corruption,
//            GSM buzz, radio, mains hum, static, grains, echo throws, vinyl.
//  Settings: FxLabDefs.h. Nothing is allocated on the audio thread.
// =============================================================================
#include <array>
#include <atomic>
#include "DspCore.h"
#include "FxLabDefs.h"

namespace obs::dsp
{
namespace fx
{
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    inline uint32_t u()  { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float uni()   { return (float) (u() >> 8) * (1.0f / 16777216.0f); }
    inline float bi()    { return uni() * 2.f - 1.f; }
};
inline float hash01 (uint32_t a, uint32_t b) { uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u; h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; return (float) (h >> 8) * (1.0f / 16777216.0f); }

/** A classic two-head delay-line pitch shifter (two read heads glide through a short window and cross-fade). */
class PitchShift
{
public:
    void prepare (double sr)
    {
        int n = 1; while (n < (int) (sr * 0.25)) n <<= 1;
        buf.assign ((size_t) n, 0.f); mask = n - 1; w = 0; ph = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.f); ph = 0; }
    inline float process (float x, float ratio, float win)
    {
        buf[(size_t) w] = x; w = (w + 1) & mask;
        ph += (1.f - ratio) / win;
        ph -= std::floor (ph);
        const float p2 = ph + 0.5f - std::floor (ph + 0.5f);
        const float g1 = 0.5f - 0.5f * std::cos (kTwoPi * ph), g2 = 0.5f - 0.5f * std::cos (kTwoPi * p2);
        return read (ph * win + 2.f) * g1 + read (p2 * win + 2.f) * g2;
    }
private:
    inline float read (float d) const
    {
        const float rp = (float) w - 1.f - d;
        const int i = (int) std::floor (rp); const float f = rp - (float) i;
        const float a = buf[(size_t) (i & mask)], b = buf[(size_t) ((i + 1) & mask)];
        return a + (b - a) * f;
    }
    std::vector<float> buf; int mask = 0, w = 0; float ph = 0;
};

/** 90-degree phase-difference network (two allpass chains), for frequency shifting. */
class Hilbert
{
public:
    void reset() { for (auto& s : st) s = {}; d1 = 0; }
    inline void process (float x, float& re, float& im)
    {
        static const float A[4] = { 0.6923878f, 0.9360654f, 0.9882295f, 0.9987488f };
        static const float B[4] = { 0.4021921f, 0.8561711f, 0.9722910f, 0.9952885f };
        float a = x, b = x;
        for (int k = 0; k < 4; ++k) a = st[k].tick (a, A[k] * A[k]);
        for (int k = 0; k < 4; ++k) b = st[4 + k].tick (b, B[k] * B[k]);
        re = a; im = d1; d1 = b;
    }
private:
    struct Sec { float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
                 inline float tick (float x, float c) { const float y = c * (x + y2) - x2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; } };
    Sec st[8]; float d1 = 0;
};
} // namespace fx

// ================================================================================== KERAUNOS
struct KeraunosParams
{
    float v[obs::fxdefs::ker::count] {};
    float guitarHz = 0, guitarConf = 0;
    float get (int i) const     { return v[i]; }
    int   choice (int i) const  { return (int) std::lround (v[i]); }
};

class Keraunos
{
public:
    static constexpr int kCtl = 16;
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& s : shift) s.prepare (sampleRate);
        int n = 1; while (n < (int) (sr * 0.03f)) n <<= 1;
        comb[0].assign ((size_t) n, 0.f); comb[1].assign ((size_t) n, 0.f); combMask = n - 1;
        for (auto* f : { &fA[0], &fA[1], &fB[0], &fB[1], &fC[0], &fC[1], &toneL, &toneR }) f->setSampleRate (sampleRate);
        envFast.set (sampleRate, 1.0f, 30.f); envSlow.set (sampleRate, 30.f, 160.f); envPlay.set (sampleRate, 3.f, 300.f);
        dcL.prepare (sampleRate); dcR.prepare (sampleRate);
        reset();
    }
    void reset()
    {
        for (auto& s : shift) s.reset();
        for (auto& c : comb) std::fill (c.begin(), c.end(), 0.f);
        combW = 0;
        for (auto* f : { &fA[0], &fA[1], &fB[0], &fB[1], &fC[0], &fC[1], &toneL, &toneR }) f->reset();
        for (auto& h : hil) h.reset();
        for (auto& a : ap) for (auto& z : a) z = 0;
        envFast.reset(); envSlow.reset(); envPlay.reset(); dcL.reset(); dcR.reset();
        posSm = 0; trigT = -1; lastTrigParam = false; refractory = 0; lfoPh = 0; rndFrom = rndTo = 0; carPh = 0; shiftPh = 0;
        crushHold[0] = crushHold[1] = 0; crushN = 0; algoNow = -1; algoFade = 0; ctlLeft = 0; dlyPrev = dlyNow = 0.002f * sr;
    }

    /** L (and R, or null for mono) processed in place. */
    void process (float* L, float* R, int n, const KeraunosParams& p)
    {
        namespace k = obs::fxdefs::ker;
        const int algo = juce::jlimit (0, (int) k::kAlgos - 1, p.choice (k::algo));
        const float mix = p.get (k::mix) * 0.01f, out = dbToGain (p.get (k::level)), color = p.get (k::color) * 0.01f;
        for (int i = 0; i < n; ++i)
        {
            const float inL = L[i], inR = R != nullptr ? R[i] : inL;
            const float mono = 0.5f * (inL + inR);
            if (--ctlLeft <= 0) { ctlLeft = kCtl; control (p, algo, color); }
            detect (mono);
            float wl = 0, wr = 0;
            render (algo, inL, inR, wl, wr, color, (float) (kCtl - ctlLeft) / (float) kCtl);
            // switching effects: a short dip, no click
            if (algoFade < 1.f) { algoFade = juce::jmin (1.f, algoFade + 1.f / (0.012f * sr)); wl *= algoFade; wr *= algoFade; }
            wl = dcL.process (wl); wr = dcR.process (wr);
            const float yl = (inL * (1.f - mix) + wl * mix) * out, yr = (inR * (1.f - mix) + wr * mix) * out;
            L[i] = std::isfinite (yl) ? softClip (yl) : 0.f;
            if (R != nullptr) R[i] = std::isfinite (yr) ? softClip (yr) : 0.f;
        }
        shownPos.store (posSm, std::memory_order_relaxed);
    }
    /** Where the effect is now (0..1): the page moves the treadle with it. */
    float position() const { return shownPos.load (std::memory_order_relaxed); }

private:
    static inline float softClip (float x) { return std::abs (x) < 1.2f ? x : (x > 0 ? 1.2f + 0.3f * fastTanh ((x - 1.2f) * 3.f) : -1.2f - 0.3f * fastTanh ((-x - 1.2f) * 3.f)); }

    void control (const KeraunosParams& p, int algo, float color)
    {
        namespace k = obs::fxdefs::ker;
        const float ctl = sr / (float) kCtl;
        const int mode = p.choice (k::mode), shape = p.choice (k::shape);
        const float speed = p.get (k::speed) * 0.01f, sens = p.get (k::sens) * 0.01f;
        sensNow = sens;
        if (algo != algoNow) { if (algoNow >= 0) algoFade = 0; algoNow = algo; }
        float target = posSm;
        switch (mode)
        {
            case k::Pedal: target = clampf (p.get (k::pos) * 0.01f, 0.f, 1.f); break;
            case k::Envelope:
            {
                envPlay.set (sr, 2.f + (1.f - speed) * 20.f, 40.f + (1.f - speed) * 1200.f);
                const float gain = 1.f + sens * sens * 60.f;
                target = clampf (playEnv * gain, 0.f, 1.f);
                break;
            }
            case k::Auto:
            {
                const float hz = 0.05f * std::pow (2.f, speed * 7.3f);
                lfoPh += hz / ctl;
                if (lfoPh >= 1.f) { lfoPh -= 1.f; rndFrom = rndTo; rndTo = rng.uni(); }
                const float ph = lfoPh;
                target = shape == 0 ? ph : shape == 1 ? 1.f - ph : shape == 2 ? 1.f - std::abs (2.f * ph - 1.f)
                       : rndFrom + (rndTo - rndFrom) * (ph * ph * (3.f - 2.f * ph));
                break;
            }
            case k::Trigger:
            {
                const bool trigParam = p.get (k::trig) > 0.5f;
                if (trigParam && ! lastTrigParam) fire (shape);
                lastTrigParam = trigParam;
                if (onset) { onset = false; fire (shape); }
                const float T = 1.5f * std::pow (2.f, -speed * 6.2f);          // sweep time: 1.5 s .. 20 ms
                if (trigT >= 0.f)
                {
                    trigT += 1.f / ctl;
                    const float u = trigT / T;
                    switch (shape)
                    {
                        case 0: target = u < 1.f ? u : u < 3.f ? 1.f : u < 4.f ? 4.f - u : 0.f; break;   // up, hold, back
                        case 1: target = u < 1.f ? 1.f - u : u < 3.f ? 0.f : u < 4.f ? u - 3.f : 1.f; break;
                        case 2: target = u < 1.f ? u : u < 2.f ? 2.f - u : 0.f; break;
                        default: target = u < 1.f ? trigFrom + (trigTo - trigFrom) * u : trigTo; break;
                    }
                    if (u > 4.f) trigT = -1.f;
                }
                else target = shape == 1 ? 1.f : shape == 3 ? trigTo : 0.f;
                break;
            }
            default: break;
        }
        // the treadle itself is smoothed lightly; the automatic modes already move smoothly
        const float tc = mode == k::Pedal ? 0.012f : 0.004f;
        posSm += (clampf (target, 0.f, 1.f) - posSm) * (1.f - std::exp (-1.f / (tc * ctl)));
        const float P = posSm;

        // per-effect settings for this block
        semisA = semisB = 0; blendB = 0;
        switch (algo)
        {
            case k::WhammyUp:   semisA = 12.f * P; break;
            case k::WhammyDown: semisA = -12.f * P; break;
            case k::DiveBomb:   semisA = -24.f * P * P * (3.f - 2.f * P) ; break;
            case k::FifthUp:    semisA = 7.f * P; break;
            case k::OctFifth:   semisA = 12.f; semisB = 7.f; blendB = P; break;
            case k::Squeal:     semisA = 12.f + 12.f * P; break;
            case k::DetuneDrop: semisA = -1.6f * P; semisB = 0.18f * P; blendB = P; break;
            default: break;
        }
        dlyPrev = dlyNow;
        if (algo == k::Laser)   dlyNow = 0.010f * sr * std::pow (2.f, -P * 5.2f);      // 10 ms .. 0.27 ms: the resonance zaps up
        if (algo == k::ZapComb) dlyNow = 0.006f * sr * std::pow (2.f, -P * 4.f);
        fb = algo == k::Laser ? 0.62f + 0.36f * color : -(0.45f + 0.45f * color);
        apCoef = [&] { const float fc = 180.f * std::pow (2.f, P * 5.5f); const float t = std::tan (kPi * juce::jmin (fc, sr * 0.45f) / sr); return (t - 1.f) / (t + 1.f); }();
        shiftHz = (P - 0.5f) * 2.f * 900.f;                                              // -900 .. +900 Hz, none in the middle
        if (algo == k::RingMod) carHz = 30.f * std::pow (2.f, P * 6.1f);
        if (algo == k::RingTrack)
        {
            if (p.guitarHz > 0 && p.guitarConf > 0.6f) trackHz = p.guitarHz;
            carHz = (trackHz > 0 ? trackHz : 110.f) * 0.5f * std::pow (2.f, P * 3.f);
        }
        if (algo == k::Wah)
        {
            const float f = 350.f * std::pow (2.f, P * 3.f), q = 2.5f + color * 10.f;
            for (int c = 0; c < 2; ++c) fA[c].set (f, q);
            wahGain = 1.8f / std::sqrt (q);
        }
        if (algo == k::TalkBox)
        {
            static const float V[5][3] = { { 325, 700, 2530 }, { 450, 800, 2830 }, { 730, 1090, 2440 }, { 400, 1700, 2600 }, { 280, 2250, 2950 } };  // u o a e i
            const float x = P * 4.f; const int a = juce::jlimit (0, 3, (int) x); const float fr = x - (float) a;
            const float q = 6.f + color * 10.f;
            for (int c = 0; c < 2; ++c)
            {
                fA[c].set (V[a][0] + (V[a + 1][0] - V[a][0]) * fr, q);
                fB[c].set (V[a][1] + (V[a + 1][1] - V[a][1]) * fr, q);
                fC[c].set (V[a][2] + (V[a + 1][2] - V[a][2]) * fr, q);
            }
            wahGain = 1.f / std::sqrt (q);
        }
        if (algo == k::Filter)
        {
            const float f = 80.f * std::pow (2.f, P * 8.f), q = 0.7f + color * 9.f;
            for (int c = 0; c < 2; ++c) { fA[c].set (f, q); fB[c].set (f, 0.7f); }
        }
        if (algo <= k::DetuneDrop)
        {
            const float f = 1500.f * std::pow (2.f, color * 3.5f);                        // the pitch effects' TONE
            toneL.set (f, 0.6f); toneR.set (f, 0.6f);
        }
        crushH = 1 + (int) (P * P * 48.f);
        crushQ = std::pow (2.f, 12.f - color * 9.f);
    }

    void fire (int shape)
    {
        trigT = 0.f;
        trigFrom = posSm; trigTo = shape == 3 ? rng.uni() : trigTo;
        if (shape == 3) trigFrom = posSm;
    }

    inline void detect (float x)
    {
        const float a = std::abs (x);
        const float f = envFast.process (a), s = envSlow.process (a);
        playEnv = envPlay.process (a);
        if (refractory > 0) --refractory;
        else if (f > 0.01f + 0.04f * (1.f - sensNow) && f > s * (1.6f + 2.f * (1.f - sensNow))) { onset = true; refractory = (int) (0.08f * sr); }
    }

    inline float combRead (int c, float d) const
    {
        const float rp = (float) combW - d;
        const int i = (int) std::floor (rp); const float f = rp - (float) i;
        const float a = comb[(size_t) c][(size_t) (i & combMask)], b = comb[(size_t) c][(size_t) ((i + 1) & combMask)];
        return a + (b - a) * f;
    }

    inline void render (int algo, float inL, float inR, float& wl, float& wr, float color, float frac)
    {
        namespace k = obs::fxdefs::ker;
        if (algo <= k::DetuneDrop)
        {
            const float win = 0.045f * sr;
            const float ra = std::exp2 (semisA / 12.f), rb = std::exp2 (semisB / 12.f);
            float a0 = shift[0].process (inL, ra, win), a1 = shift[1].process (inR, ra, win * 1.07f);
            if (algo == k::OctFifth || blendB > 0.f)
            {
                const float b0 = shift[2].process (inL, rb, win * 0.93f), b1 = shift[3].process (inR, rb, win * 1.13f);
                if (algo == k::OctFifth) { a0 = inL * (1.f - 0.5f * blendB) + (a0 * 0.6f + b0 * 0.55f) * blendB; a1 = inR * (1.f - 0.5f * blendB) + (a1 * 0.6f + b1 * 0.55f) * blendB; }
                else { a0 = inL * (1.f - 0.5f * blendB) + (a0 + b1) * 0.5f * blendB; a1 = inR * (1.f - 0.5f * blendB) + (a1 + b0) * 0.5f * blendB; }
            }
            else if (algo != k::OctFifth) { shift[2].process (inL, 1.f, win); shift[3].process (inR, 1.f, win); }
            toneL.tick (a0); toneR.tick (a1);
            wl = toneL.low; wr = toneR.low; return;
        }
        switch (algo)
        {
            case k::Laser: case k::ZapComb:
            {
                const float d = dlyPrev + (dlyNow - dlyPrev) * frac;
                float x[2] = { inL, inR };
                float y[2];
                for (int c = 0; c < 2; ++c)
                {
                    const float dd = d * (c == 0 ? 1.f : 1.013f);
                    float s = x[c] + fb * combRead (c, dd);
                    if (algo == k::ZapComb)
                    {
                        for (int st = 0; st < 4; ++st) { const float z = ap[c][st]; const float o = apCoef * s + z; ap[c][st] = s - apCoef * o; s = o; }
                        s = 0.5f * (s + x[c]);
                    }
                    comb[(size_t) c][(size_t) combW] = clampf (s, -4.f, 4.f);
                    y[c] = s * (1.f - std::abs (fb) * 0.85f);
                }
                combW = (combW + 1) & combMask;
                wl = y[0]; wr = y[1]; return;
            }
            case k::FreqShift:
            {
                shiftPh += shiftHz / sr; shiftPh -= std::floor (shiftPh);
                const float c = std::cos (kTwoPi * shiftPh), s = std::sin (kTwoPi * shiftPh);
                float re, im;
                hil[0].process (inL + fsFb[0] * color * 0.6f, re, im); wl = re * c - im * s; fsFb[0] = wl;
                hil[1].process (inR + fsFb[1] * color * 0.6f, re, im); wr = re * c + im * s; fsFb[1] = wr;       // the other way round on the right: it spreads
                return;
            }
            case k::RingMod: case k::RingTrack:
            {
                carPh += carHz / sr; carPh -= std::floor (carPh);
                const float sn = std::sin (kTwoPi * carPh);
                const float car = color < 0.5f ? sn : sn * (1.f - (color - 0.5f) * 2.f) + fastTanh (sn * 8.f) * (color - 0.5f) * 2.f;
                wl = inL * car * 1.3f; wr = inR * car * 1.3f; return;
            }
            case k::Wah:
                fA[0].tick (inL); fA[1].tick (inR);
                wl = fA[0].band * wahGain * 2.f; wr = fA[1].band * wahGain * 2.f; return;
            case k::TalkBox:
            {
                fA[0].tick (inL); fB[0].tick (inL); fC[0].tick (inL);
                fA[1].tick (inR); fB[1].tick (inR); fC[1].tick (inR);
                wl = (fA[0].band + 0.7f * fB[0].band + 0.35f * fC[0].band) * wahGain * 2.2f;
                wr = (fA[1].band + 0.7f * fB[1].band + 0.35f * fC[1].band) * wahGain * 2.2f; return;
            }
            case k::Filter:
                fA[0].tick (inL); fB[0].tick (fA[0].low); fA[1].tick (inR); fB[1].tick (fA[1].low);
                wl = fB[0].low; wr = fB[1].low; return;
            case k::BitCrush:
            {
                if (++crushN >= crushH) { crushN = 0; crushHold[0] = std::round (inL * crushQ) / crushQ; crushHold[1] = std::round (inR * crushQ) / crushQ; }
                wl = crushHold[0]; wr = crushHold[1]; return;
            }
            default: wl = inL; wr = inR; return;
        }
    }

    float sr = 48000.f; int ctlLeft = 0;
    fx::Rng rng;
    fx::PitchShift shift[4];
    std::vector<float> comb[2]; int combMask = 0, combW = 0;
    float ap[2][4] {}; float apCoef = 0;
    fx::Hilbert hil[2]; float fsFb[2] {};
    Svf fA[2], fB[2], fC[2], toneL, toneR;
    EnvFollower envFast, envSlow, envPlay; DcBlocker dcL, dcR;
    float posSm = 0, playEnv = 0, sensNow = 0.5f; bool onset = false; int refractory = 0;
    float trigT = -1.f, trigFrom = 0, trigTo = 0; bool lastTrigParam = false;
    float lfoPh = 0, rndFrom = 0, rndTo = 0;
    float semisA = 0, semisB = 0, blendB = 0, dlyPrev = 96, dlyNow = 96, fb = 0;
    float shiftHz = 0, shiftPh = 0, carHz = 100, carPh = 0, trackHz = 0, wahGain = 1;
    int crushH = 1, crushN = 0; float crushQ = 4096, crushHold[2] {};
    int algoNow = -1; float algoFade = 1;
    std::atomic<float> shownPos { 0.f };
};

// ================================================================================== KHAOS
struct KhaosParams
{
    float v[obs::fxdefs::glt::count] {};
    double bpm = 120, ppq = 0; bool hostPlaying = false;
    float get (int i) const     { return v[i]; }
    int   choice (int i) const  { return (int) std::lround (v[i]); }
    bool  on (int i) const      { return v[i] >= 0.5f; }
};

class Khaos
{
public:
    static constexpr int kGrains = 8;
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        int n = 1; while (n < (int) (sr * 11.f)) n <<= 1;                 // ~11 s (4 bars at 90 BPM)
        hist[0].assign ((size_t) n, 0.f); hist[1].assign ((size_t) n, 0.f); mask = n - 1;
        int e = 1; while (e < (int) (sr * 2.f)) e <<= 1;
        echo[0].assign ((size_t) e, 0.f); echo[1].assign ((size_t) e, 0.f); echoMask = e - 1;
        shiftL.prepare (sampleRate); shiftR.prepare (sampleRate);
        for (auto* f : { &djL, &djR, &bpL, &bpR, &echoLp[0], &echoLp[1] }) f->setSampleRate (sampleRate);
        echoLp[0].set (3200.f, 0.6f); echoLp[1].set (3200.f, 0.6f);
        reset();
    }
    void reset()
    {
        for (auto& h : hist) std::fill (h.begin(), h.end(), 0.f);
        for (auto& h : echo) std::fill (h.begin(), h.end(), 0.f);
        wc = 0; echoW = 0; beat = 0; lastStep = -1; ev[0].type = ev[1].type = -1; cur = 0;
        shiftL.reset(); shiftR.reset();
        for (auto* f : { &djL, &djR, &bpL, &bpR, &echoLp[0], &echoLp[1] }) f->reset();
        for (auto& g : grains) g.left = 0;
        heldSample[0] = heldSample[1] = 0;
    }

    void process (float* L, float* R, int n, const KhaosParams& p)
    {
        namespace g = obs::fxdefs::glt;
        const double bpm = juce::jlimit (40.0, 300.0, p.bpm);
        if (p.hostPlaying) beat = p.ppq;                                     // follow the DAW's bar line
        static const double GRID[6] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        const double stepBeats = GRID[juce::jlimit (0, 5, p.choice (g::grid))];
        stepLen = (float) (stepBeats * 60.0 / bpm * sr);
        static const double LOOP[4] = { 2.0, 4.0, 8.0, 16.0 };
        loopSteps = juce::jmax (2, (int) std::lround (LOOP[juce::jlimit (0, 3, p.choice (g::loop))] / stepBeats));
        const float mix = p.get (g::mix) * 0.01f, out = dbToGain (p.get (g::level));
        mixNow = mix; chaos = p.get (g::chaos) * 0.01f; pitchSt = p.get (g::pitch); dir = p.choice (g::dir);
        setFilter (p.get (g::filter));
        const double inc = bpm / 60.0 / sr;
        const bool hold = p.on (g::hold);
        for (int i = 0; i < n; ++i)
        {
            const float dl = L[i], dr = R[i];
            hist[0][(size_t) (wc & mask)] = dl; hist[1][(size_t) (wc & mask)] = dr; ++wc;
            const long long step = (long long) std::floor (beat / stepBeats + 1.0e-9);
            if (step != lastStep) { lastStep = step; onStep (step, p, hold); }
            beat += inc;
            if (hold && ev[cur].type >= 0) ev[cur].dur = juce::jmax (ev[cur].dur, ev[cur].t + 4.f * stepLen);      // HOLD keeps it going

            // the event(s): the current one, and the last one fading out (3 ms) so nothing clicks
            float wl = 0, wr = 0, envSum = 0;
            for (int s = 0; s < 2; ++s)
            {
                auto& e = ev[s];
                if (e.type < 0) continue;
                float el, er;
                render (e, dl, dr, el, er);
                const float fadeIn = juce::jmin (1.f, e.t / (0.003f * sr)), fadeOut = juce::jmin (1.f, (e.dur - e.t) / (0.004f * sr));
                const float env = juce::jmax (0.f, juce::jmin (fadeIn, fadeOut)) * e.gain;
                wl += el * env; wr += er * env; envSum += env;
                e.t += 1.f;
                if (e.t >= e.dur) e.type = -1;
            }
            envSum = juce::jmin (1.f, envSum);
            // the filter shapes what the glitch plays (DJ style: left darker, right thinner)
            if (filterOn) { djL.tick (wl); djR.tick (wr); wl = filterLow ? djL.low : djL.high; wr = filterLow ? djR.low : djR.high; }
            // echo throws keep ringing after the glitch
            const int ed = juce::jlimit (1, echoMask - 1, (int) (stepLen * 3.f));
            const float el = echo[0][(size_t) ((echoW - ed) & echoMask)], er = echo[1][(size_t) ((echoW - ed) & echoMask)];
            echoLp[0].tick (echoSend[0] + el * 0.62f); echoLp[1].tick (echoSend[1] + er * 0.62f);
            echo[0][(size_t) echoW] = echoLp[0].low; echo[1][(size_t) echoW] = echoLp[1].low;
            echoW = (echoW + 1) & echoMask; echoSend[0] = echoSend[1] = 0;
            float yl = dl * (1.f - envSum * mix) + wl * mix + er * 0.7f * mix;          // ping-pong: the left echo comes back right
            float yr = dr * (1.f - envSum * mix) + wr * mix + el * 0.7f * mix;
            yl *= out; yr *= out;
            L[i] = std::isfinite (yl) ? clampf (yl, -2.f, 2.f) : 0.f;
            R[i] = std::isfinite (yr) ? clampf (yr, -2.f, 2.f) : 0.f;
        }
        const auto& e = ev[cur];
        shownStep.store ((int) (((lastStep % 16) + 16) % 16), std::memory_order_relaxed);
        shownType.store (e.type, std::memory_order_relaxed);
    }

    int   step() const     { return shownStep.load (std::memory_order_relaxed); }        // 0..15
    int   glitch() const   { return shownType.load (std::memory_order_relaxed); }        // -1: none
    int   events() const   { return shownCount.load (std::memory_order_relaxed); }

private:
    struct Ev
    {
        int type = -1; float t = 0, dur = 1, gain = 1;
        long long start = 0, src = 0; uint32_t seed = 0;
        float a = 0, b = 0, c = 0, rp = 0, held[2] {};
        int n = 0;
    };
    struct Grain { float pos = 0, inc = 1, len = 1, t = 0; int left = 0; float pan = 0.5f; };

    void setFilter (float f)
    {
        filterOn = std::abs (f) > 2.f; filterLow = f < 0;
        if (! filterOn) return;
        const float x = std::abs (f) * 0.01f;
        const float fc = filterLow ? 20000.f * std::pow (2.f, -x * 7.f) : 20.f * std::pow (2.f, x * 7.f);
        djL.set (fc, 0.9f + x * 1.5f); djR.set (fc, 0.9f + x * 1.5f);
    }

    void onStep (long long step, const KhaosParams& p, bool hold)
    {
        namespace g = obs::fxdefs::glt;
        if (hold) { if (ev[cur].type < 0) start (g::Freeze, 4.f * stepLen); return; }
        if (ev[cur].type >= 0 && ev[cur].t < ev[cur].dur - stepLen * 0.5f) return;      // a longer glitch is still playing
        int type = -1; float steps = 1;
        if (p.choice (g::mode) == g::Sequence)
        {
            const int s = (int) (((step % 16) + 16) % 16);
            type = p.choice (g::s0 + s) - 1;
            // a run of the same type in the lane plays as one longer glitch
            if (type >= 0) { int k = 1; while (k < 4 && p.choice (g::s0 + (s + k) % 16) - 1 == type) ++k; steps = (float) k; }
        }
        else
        {
            if (rng.uni() * 100.f >= p.get (g::chance)) return;
            int pool[g::kTypes], m = 0;
            for (int t = 0; t < g::kTypes; ++t) if (p.on (g::t0 + t)) pool[m++] = t;
            if (m == 0) return;
            type = pool[(int) (rng.uni() * (float) m) % m];
            static const float LEN[4] = { 1, 2, 4, 8 };
            steps = LEN[juce::jlimit (0, 3, p.choice (g::len))];
            if (chaos > 0.3f && rng.uni() < chaos) steps = juce::jmax (1.f, std::round (steps * (0.25f + rng.uni())));
        }
        if (type >= 0) start (type, steps * stepLen);
    }

    void start (int type, float dur)
    {
        namespace g = obs::fxdefs::glt;
        cur ^= 1;                                             // the old one keeps fading in the other slot
        auto& o = ev[cur ^ 1]; if (o.type >= 0) o.dur = juce::jmin (o.dur, o.t + 0.004f * sr);
        auto& e = ev[cur]; e = {};
        e.type = type; e.dur = juce::jmax (0.01f * sr, dur); e.start = wc; e.seed = rng.u();
        const float st = stepLen;
        switch (type)
        {
            case g::Stutter:  e.a = st / (float) (1 << (int) (rng.uni() * (1.f + chaos * 2.99f))); break;     // slice: a step, a half, a quarter
            case g::Scatter:  { const int k = 1 + (int) (rng.uni() * (float) (loopSteps - 1)); e.src = juce::jmax (wc - (long long) mask + (long long) e.dur, wc - (long long) (k * st)); break; }
            case g::Skip:     e.a = (0.02f + 0.07f * rng.uni()) * sr; e.b = (0.06f + 0.09f * rng.uni()) * sr; break;
            case g::Freeze:   e.a = (0.03f + 0.06f * rng.uni()) * sr; break;
            case g::Pitch:    { float s = pitchSt * (0.5f + 0.5f * (1.f - chaos) + chaos * rng.uni()); s = std::round (s);
                                const bool up = dir == 0 || (dir == 2 && rng.uni() < 0.5f); e.a = std::exp2 ((up ? s : -s) / 12.f); break; }
            case g::Warble:   e.a = 4.f + 5.f * rng.uni(); e.b = (0.002f + 0.01f * (0.3f + chaos)) * sr; break;
            case g::Crush:    e.n = 4 + (int) (chaos * 26.f * rng.uni() + 4.f); e.a = std::pow (2.f, 3.f + 5.f * (1.f - chaos) * rng.uni()); break;
            case g::Chop:     e.a = st / (rng.uni() < 0.5f + 0.4f * chaos ? 8.f : 4.f); break;
            case g::Gsm:      e.a = 0.08f + 0.12f * (0.3f + chaos); break;
            case g::Radio:    e.a = 1500.f + 2500.f * rng.uni(); e.b = (rng.uni() < 0.5f ? -1.f : 1.f) * (500.f + 2000.f * chaos); break;
            case g::Grains:   for (auto& gr : grains) gr.left = 0; break;
            default: break;
        }
        e.gain = type == g::Gsm || type == g::Radio || type == g::Hum || type == g::Static ? 1.f : 1.f;
        shownCount.store (shownCount.load (std::memory_order_relaxed) + 1, std::memory_order_relaxed);
    }

    inline float readH (int c, double pos) const
    {
        pos = juce::jlimit ((double) (wc - (long long) mask + 2), (double) (wc - 1), pos);
        const long long i = (long long) std::floor (pos); const float f = (float) (pos - (double) i);
        const float a = hist[(size_t) c][(size_t) (i & mask)], b = hist[(size_t) c][(size_t) ((i + 1) & mask)];
        return a + (b - a) * f;
    }

    inline void render (Ev& e, float dl, float dr, float& ol, float& orr)
    {
        namespace g = obs::fxdefs::glt;
        const float t = e.t, D = e.dur, st = stepLen;
        switch (e.type)
        {
            case g::Stutter:
            {
                const double pos = (double) e.start + std::fmod (t, e.a);
                ol = readH (0, pos); orr = readH (1, pos); break;
            }
            case g::Roll:
            {
                const int q = juce::jmin (3, (int) (4.f * t / D));
                const float local = t - (float) q * D * 0.25f, Lq = juce::jmax (0.004f * sr, st / (float) (1 << q));
                const double pos = (double) e.start + std::fmod (local, Lq);
                const float gup = 0.7f + 0.3f * (t / D);
                ol = readH (0, pos) * gup; orr = readH (1, pos) * gup; break;
            }
            case g::Reverse:
            {
                const float span = juce::jmin (D, 2.f * st);
                const double pos = (double) e.start - 1.0 - std::fmod (t, span);
                ol = readH (0, pos); orr = readH (1, pos); break;
            }
            case g::Scatter:
            {
                const double pos = (double) e.src + t;
                ol = readH (0, pos); orr = readH (1, pos); break;
            }
            case g::Skip:
            {
                const int seg = (int) (t / e.b); const float local = t - (float) seg * e.b;
                const double base = (double) e.start + (double) seg * e.b * 0.5 - fx::hash01 (e.seed, (uint32_t) seg) * e.a;
                const double pos = base + std::fmod (local, e.a);
                ol = readH (0, pos); orr = readH (1, pos); break;
            }
            case g::Freeze:
            {
                const float G = e.a;
                const float u1 = std::fmod (t, G) / G, u2 = std::fmod (t + 0.5f * G, G) / G;
                const double p1 = (double) e.start - G + u1 * G, p2 = (double) e.start - G + u2 * G;
                const float w1 = std::sin (kPi * u1), w2 = std::sin (kPi * u2);
                ol = readH (0, p1) * w1 + readH (0, p2) * w2; orr = readH (1, p1) * w1 + readH (1, p2) * w2;
                ol *= 0.75f; orr *= 0.75f; break;
            }
            case g::Pitch:
                ol = shiftL.process (dl, e.a, 0.04f * sr); orr = shiftR.process (dr, e.a, 0.043f * sr); return;
            case g::TapeStop:
            {
                const float r = std::pow (juce::jmax (0.f, 1.f - t / D), 1.3f);
                e.rp += r;
                const double pos = (double) e.start + e.rp;
                const float gq = juce::jmin (1.f, (1.f - t / D) * 6.f);
                ol = readH (0, pos) * gq; orr = readH (1, pos) * gq; break;
            }
            case g::SpinUp:
            {
                const float r = std::pow (t / D, 1.3f);
                e.rp += r;
                const double pos = (double) e.start - D * 0.44f + e.rp;
                const float gq = juce::jmin (1.f, (t / D) * 5.f);
                ol = readH (0, pos) * gq; orr = readH (1, pos) * gq; break;
            }
            case g::HalfSpeed:
            {
                const double pos = (double) e.start - D * 0.5f + t * 0.5f;
                ol = readH (0, pos); orr = readH (1, pos); break;
            }
            case g::Warble:
            {
                const float d = 0.006f * sr + e.b * (1.f + std::sin (kTwoPi * e.a * t / sr));
                const double pos = (double) wc - 1.0 - d;
                ol = readH (0, pos); orr = readH (1, pos - 0.0007 * sr); break;
            }
            case g::Crush:
            {
                if (((int) t) % juce::jmax (1, e.n) == 0) { e.held[0] = std::round (dl * e.a) / e.a; e.held[1] = std::round (dr * e.a) / e.a; }
                ol = e.held[0]; orr = e.held[1]; break;
            }
            case g::Chop:
            {
                const float u = std::fmod (t, e.a) / e.a;
                const float a = u < 0.45f ? juce::jmin (1.f, u * 40.f) : juce::jmax (0.f, 1.f - (u - 0.45f) * 40.f);
                ol = dl * a; orr = dr * a; break;
            }
            case g::Dropout:
            {
                const float u = t / D;
                const float a = u < 0.75f ? 0.f : (u - 0.75f) * 4.f;
                const float click = t < 12.f ? (fx::hash01 (e.seed, (uint32_t) t) - 0.5f) * 0.25f : 0.f;
                ol = dl * a + click; orr = dr * a + click; break;
            }
            case g::Data:
            {
                const int chunk = (int) (t / (0.004f * sr));
                const float h = fx::hash01 (e.seed, (uint32_t) chunk);
                if (h < 0.3f) { ol = dl; orr = dr; }
                else if (h < 0.5f) { if (((int) t) % (int) (0.004f * sr) == 0) { e.held[0] = dl; e.held[1] = dr; } ol = e.held[0]; orr = e.held[1]; }
                else if (h < 0.75f) { const float q = 6.f; ol = -std::round (dl * q) / q; orr = -std::round (dr * q) / q; }
                else { const double back = (double) e.start + (double) chunk * 0.004 * sr - 0.012 * sr * fx::hash01 (e.seed ^ 77u, (uint32_t) chunk) + std::fmod (t, 0.002f * sr); ol = readH (0, back); orr = readH (1, back); }
                break;
            }
            case g::Gsm:
            {
                const float tt = t / sr;
                const float frame = std::fmod (tt, 0.004615f) < 0.000577f ? 1.f : 0.f;              // 217 Hz bursts
                const float multi = std::fmod (tt, 0.24f) < 0.11f ? 1.f : (std::fmod (tt, 0.06f) < 0.02f ? 0.6f : 0.f);
                const float buzz = (frame * 2.f - 0.25f) * multi * e.a;
                bpL.set (2200.f, 0.9f); bpL.tick (buzz);
                ol = dl + bpL.band * 2.f + buzz * 0.3f; orr = dr + bpL.band * 2.f + buzz * 0.3f; break;
            }
            case g::Radio:
            {
                const float f = e.a + e.b * (t / D);
                e.c += f / sr; e.c -= std::floor (e.c);
                bpL.set (1200.f, 0.8f); bpR.set (1300.f, 0.8f);
                bpL.tick (dl); bpR.tick (dr);
                const float am = 0.75f + 0.25f * std::sin (kTwoPi * 6.f * t / sr);
                const float noise = rng.bi() * 0.03f;
                const float wh = std::sin (kTwoPi * e.c) * 0.05f;
                ol = (bpL.band * 1.6f) * am + noise + wh; orr = (bpR.band * 1.6f) * am + noise + wh; break;
            }
            case g::Hum:
            {
                const float tt = t / sr;
                const float gate = 0.55f + 0.45f * (std::sin (kTwoPi * 100.f * tt) > 0.f ? 1.f : -1.f);
                float buzz = 0; for (int h = 1; h <= 6; ++h) buzz += std::sin (kTwoPi * 50.f * (float) h * tt) / (float) h;
                ol = dl * gate + buzz * 0.035f; orr = dr * gate + buzz * 0.035f; break;
            }
            case g::Static:
            {
                const float burst = fx::hash01 (e.seed, (uint32_t) (t / (0.03f * sr))) < 0.5f ? 1.f : 0.2f;
                const float crack = rng.uni() < 0.004f ? rng.bi() * 0.6f : 0.f;
                const float hiss = rng.bi() * 0.04f * burst;
                ol = dl * 0.8f + crack + hiss; orr = dr * 0.8f + crack * 0.8f + hiss; break;
            }
            case g::Grains:
            {
                ol = orr = 0;
                for (auto& gr : grains)
                {
                    if (gr.left <= 0)
                    {
                        gr.len = (0.03f + 0.09f * rng.uni()) * sr; gr.left = (int) gr.len; gr.t = 0;
                        static const float R[5] = { 1.f, 2.f, 0.5f, 1.4983f, 0.7491f };
                        gr.inc = rng.uni() < chaos ? R[(int) (rng.uni() * 4.99f)] : 1.f;
                        gr.pos = (float) (wc - 1) - gr.len * gr.inc - rng.uni() * 0.4f * sr;
                        gr.pan = rng.uni();
                    }
                    const float u = gr.t / gr.len, w = std::sin (kPi * u);
                    const float s = readH (0, (double) gr.pos + gr.t * gr.inc) * w;
                    ol += s * (1.f - gr.pan); orr += s * gr.pan;
                    gr.t += 1.f; --gr.left;
                }
                ol *= 0.55f; orr *= 0.55f; break;
            }
            case g::Echo:
                ol = dl; orr = dr; echoSend[0] += dl; echoSend[1] += dr; return;
            case g::Vinyl:
            {
                const float d = 0.003f * sr * (1.f + std::sin (kTwoPi * 0.6f * t / sr));
                const double pos = (double) wc - 1.0 - d - 0.001 * sr;
                bpL.set (3500.f, 0.7f); bpR.set (3500.f, 0.7f);
                bpL.tick (readH (0, pos)); bpR.tick (readH (1, pos));
                const float crack = rng.uni() < 0.002f ? rng.bi() * 0.4f : 0.f;
                ol = bpL.low * 0.9f + crack; orr = bpR.low * 0.9f + crack; break;
            }
            default: ol = dl; orr = dr; break;
        }
        // keep the pitch shifter fed so a PITCH glitch starts smoothly
        if (e.type != g::Pitch) { shiftL.process (dl, 1.f, 0.04f * sr); shiftR.process (dr, 1.f, 0.043f * sr); }
    }

    float sr = 48000.f, stepLen = 6000.f, mixNow = 1, chaos = 0.4f, pitchSt = 12; int dir = 2, loopSteps = 16;
    std::vector<float> hist[2]; long long wc = 0; int mask = 0;
    std::vector<float> echo[2]; int echoMask = 0, echoW = 0; float echoSend[2] {};
    double beat = 0; long long lastStep = -1;
    Ev ev[2]; int cur = 0;
    Grain grains[kGrains];
    fx::Rng rng; fx::PitchShift shiftL, shiftR;
    Svf djL, djR, bpL, bpR, echoLp[2]; bool filterOn = false, filterLow = true;
    float heldSample[2] {};
    std::atomic<int> shownStep { 0 }, shownType { -1 }, shownCount { 0 };
};
} // namespace obs::dsp
