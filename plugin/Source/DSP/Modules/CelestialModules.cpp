// =============================================================================
//  Celestial pedals: lush reverbs, shimmers, delays and two extras.
//
//   Reverbs  Elysium (cathedral hall), Empyrean (two-voice shimmer), Nimbus (cloud),
//            Vesper (choir), Halo (reverse / bloom)
//   Delays   Astral (shimmer delay), Mnemosyne (multi-head tape echo + spring), Gemini (dual / reverse)
//   Others   Lyra (12-string shimmer), Icarus (octave-up fuzz)
//
//  The reverbs share LushTank: a "figure-8" tank in the style of the classic studio plate
//  algorithms (two cross-fed branches of modulated allpasses, delays and damping), scaled for size
//  and sample rate, with taps for a wide stereo output. It's smoother and denser than the FDN
//  that Crypt / Seraph use. Looks: visuals/pedals/<key>.json.
// =============================================================================
#include "Module.h"
#include "../PitchShifter.h"
#include <juce_dsp/juce_dsp.h>

namespace obs::dsp
{
namespace
{
constexpr float kRef = 29761.f;                         // the reference rate the tank lengths are given at

inline float softLimit (float x)                        // transparent below ~0.8, never above 1.5
{
    const float a = std::abs (x);
    return a < 0.8f ? x : std::copysign (0.8f + 0.7f * fastTanh ((a - 0.8f) / 0.7f), x);
}

/** Same dry/wet law as the other reverbs: full dry up to 50 %, then it fades out. */
inline float dryGain (float m) { return m < 0.5f ? 1.f : 2.f * (1.f - m); }

// ---------------------------------------------------------------------------
/** Allpass whose delay line can be tapped (for the tank's output taps). */
class TapAllpass
{
public:
    void prepare (int maxLen)                  { line.prepare (maxLen); }
    void clear()                               { line.clear(); }
    inline float process (float x, float d, float g)
    {
        const float del = line.readCubic (d);
        const float v = x + g * del;
        line.push (v);
        return del - g * v;
    }
    inline float tap (float d) const           { return line.readLinear (d); }
private:
    DelayLine line;
};

/** Delay that remembers its read length (taps are read from it too). */
class TankDelay
{
public:
    void prepare (int maxLen)                  { line.prepare (maxLen); }
    void clear()                               { line.clear(); }
    inline float process (float x, float d)    { line.push (x); return line.readLinear (d); }
    inline float tap (float d) const           { return line.readLinear (d); }
private:
    DelayLine line;
};

// ---------------------------------------------------------------------------
/** Lush stereo reverb tank (see the top of the file). tick() takes a mono input plus optional
    per-side injections (shimmer, formants...) and returns the wet stereo pair. */
class LushTank
{
public:
    static constexpr float kMaxSize = 2.4f;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        const float k = sr / kRef * kMaxSize;
        for (int i = 0; i < 4; ++i) inDiff[i].prepare ((int) (kInDiff[i] * k) + 16);
        for (int s = 0; s < 2; ++s)
        {
            apMod[s].prepare ((int) ((kApMod[s] + 64) * k) + 16);
            for (int j = 0; j < 2; ++j) apExtra[s][j].prepare ((int) ((kApExtra[s][j] + 64) * k) + 16);
            del1[s].prepare ((int) (kDel1[s] * k) + 16);
            ap2[s].prepare ((int) (kAp2[s] * k) + 16);
            del2[s].prepare ((int) (kDel2[s] * k) + 16);
            lfo[s].setSampleRate (sampleRate); lfo[s].setPhase (s * 0.31f);
            drift[s].setSampleRate (sampleRate); drift[s].setPhase (0.13f + s * 0.5f);
        }
        set (0.7f, 6.f, 7000.f, 12000.f, 0.5f, 0.f, 0.f, false);
        clear();
    }
    void clear()
    {
        for (auto& d : inDiff) d.clear();
        for (int s = 0; s < 2; ++s)
        {
            apMod[s].clear(); apExtra[s][0].clear(); apExtra[s][1].clear();
            del1[s].clear(); ap2[s].clear(); del2[s].clear(); damp[s].reset(); fbk[s] = 0;
            driftSm[s] = 0;
        }
        bw.reset();
    }

    /** size 0..1, decay seconds, damping / bandwidth Hz, mod 0..1, density 0..1 (extra allpasses),
        drift 0..1 (slow random pitch wander), infinite = no decay at all. */
    void set (float size, float decayS, float dampHz, float bandwidthHz, float mod, float density, float driftAmt, bool infinite)
    {
        scale = sr / kRef * (0.35f + clampf (size, 0.f, 1.f) * (kMaxSize - 0.35f));
        inScale = sr / kRef * (0.6f + clampf (size, 0.f, 1.f) * 0.9f);
        dens = clampf (density, 0.f, 1.f);
        for (int s = 0; s < 2; ++s)
        {
            lenAp[s] = kApMod[s] * scale; lenD1[s] = kDel1[s] * scale; lenAp2[s] = kAp2[s] * scale; lenD2[s] = kDel2[s] * scale;
            lenX[s][0] = kApExtra[s][0] * scale; lenX[s][1] = kApExtra[s][1] * scale;
        }
        // one branch (half the loop): the decay gain is applied twice per branch
        const float branch = (lenAp[0] + lenD1[0] + lenAp2[0] + lenD2[0] + (dens > 0.01f ? lenX[0][0] + lenX[0][1] : 0.f)) / sr;
        const float g = std::pow (10.f, -3.f * branch * 0.5f / juce::jmax (0.1f, decayS));
        decay = infinite ? 0.99995f : juce::jmin (g, 0.9993f);
        decayDiff2 = clampf (decay + 0.15f, 0.25f, 0.5f);
        damp[0].setLowpass (sr, infinite ? 18000.f : dampHz); damp[1].setLowpass (sr, infinite ? 18000.f : dampHz);
        bw.setLowpass (sr, bandwidthHz);
        excursion = (2.f + 22.f * clampf (mod, 0.f, 1.f)) * sr / kRef;
        driftDepth = clampf (driftAmt, 0.f, 1.f) * 60.f * sr / kRef;
        const float rate = 0.35f + 0.9f * clampf (mod, 0.f, 1.f);
        lfo[0].setRate (rate); lfo[1].setRate (rate * 1.13f);
        drift[0].setRate (0.11f); drift[1].setRate (0.137f);
    }

    inline void tick (float in, float& outL, float& outR, float injL = 0.f, float injR = 0.f)
    {
        float x = bw.lp (in);
        static constexpr float g1[4] = { 0.75f, 0.75f, 0.625f, 0.625f };
        for (int i = 0; i < 4; ++i) x = inDiff[i].process (x, kInDiff[i] * inScale, g1[i]);

        float end[2];
        for (int s = 0; s < 2; ++s)
        {
            driftSm[s] += 0.0004f * (drift[s].next (Lfo::SampleHold) - driftSm[s]);       // smoothed random walk
            const float mod = lfo[s].next() * excursion + driftSm[s] * driftDepth;
            float a = softLimit (x + fbk[1 - s] + (s == 0 ? injL : injR));
            a = apMod[s].process (a, juce::jmax (2.f, lenAp[s] + mod), 0.7f);
            if (dens > 0.01f)
            {
                a = apExtra[s][0].process (a, lenX[s][0] - 0.5f * mod, 0.55f * dens);
                a = apExtra[s][1].process (a, lenX[s][1] + 0.3f * mod, 0.5f * dens);
            }
            float b = del1[s].process (a, lenD1[s]);
            b = damp[s].lp (b) * decay;
            b = ap2[s].process (b, lenAp2[s], decayDiff2);
            end[s] = del2[s].process (b, lenD2[s]) * decay;
        }
        fbk[0] = end[0]; fbk[1] = end[1];

        // output taps (classic plate tap set, scaled with the tank)
        const float k = scale;
        outL = 0.6f * ( del1[1].tap (266 * k) + del1[1].tap (2974 * k) - ap2[1].tap (1913 * k) + del2[1].tap (1996 * k)
                      - del1[0].tap (1990 * k) - ap2[0].tap (187 * k) - del2[0].tap (1066 * k));
        outR = 0.6f * ( del1[0].tap (353 * k) + del1[0].tap (3627 * k) - ap2[0].tap (1228 * k) + del2[0].tap (2673 * k)
                      - del1[1].tap (2111 * k) - ap2[1].tap (335 * k) - del2[1].tap (121 * k));
    }

private:
    static constexpr float kInDiff[4] = { 142.f, 107.f, 379.f, 277.f };
    static constexpr float kApMod[2]  = { 672.f, 908.f };
    static constexpr float kApExtra[2][2] = { { 1213.f, 1597.f }, { 1361.f, 1789.f } };
    static constexpr float kDel1[2]   = { 4453.f, 4217.f };
    static constexpr float kAp2[2]    = { 1800.f, 2656.f };
    static constexpr float kDel2[2]   = { 3720.f, 3163.f };

    float sr = 48000, scale = 1, inScale = 1, decay = 0.5f, decayDiff2 = 0.5f, excursion = 16, driftDepth = 0, dens = 0;
    float lenAp[2] {}, lenD1[2] {}, lenAp2[2] {}, lenD2[2] {}, lenX[2][2] {}, fbk[2] {}, driftSm[2] {};
    Allpass inDiff[4];
    TapAllpass apMod[2], apExtra[2][2], ap2[2];
    TankDelay del1[2], del2[2];
    OnePole damp[2], bw;
    Lfo lfo[2], drift[2];
};

// ---------------------------------------------------------------------------
/** Plays the last `len` samples backwards, two overlapping heads with sin^2 windows (no clicks). */
class ReverseReader
{
public:
    void prepare (int maxLen)                  { line.prepare (maxLen * 2 + 64); }
    void clear()                               { line.clear(); ph = 0; }
    inline float process (float x, float len)
    {
        line.push (x);
        len = juce::jmax (64.f, len);
        ph += 1.f / len; if (ph >= 1.f) ph -= 1.f;
        const float p2 = ph + 0.5f - (ph >= 0.5f ? 1.f : 0.f);
        const float w1 = std::sin (kPi * ph), w2 = std::sin (kPi * p2);
        return line.readLinear (2.f * len * ph) * w1 * w1 + line.readLinear (2.f * len * p2) * w2 * w2;
    }
private:
    DelayLine line; float ph = 0;
};

/** Spring reverb tank: dispersive allpass chains inside two short feedback loops ("drip" and "boing"). */
class SpringModel
{
public:
    void prepare (double s)
    {
        sr = (float) s;
        for (auto& l : loop) l.prepare ((int) (0.08 * s));
        for (auto& c : chain) for (auto& z : c) z = 0;
        lp[0].setLowpass (s, 4200.f); lp[1].setLowpass (s, 3600.f); hp.set (Biquad::HP, s, 300.f, 0.7f);
        lfo.setSampleRate (s); lfo.setRate (0.7f);
        clear();
    }
    void clear() { for (auto& l : loop) l.clear(); for (auto& c : chain) for (auto& z : c) z = 0; fb[0] = fb[1] = 0; lp[0].reset(); lp[1].reset(); hp.reset(); }
    inline float process (float x)
    {
        static constexpr float lens[2] = { 0.0373f, 0.0417f };
        x = hp.process (x);
        float out = 0;
        const float wob = lfo.next() * 0.0006f;
        for (int k = 0; k < 2; ++k)
        {
            float v = x + fb[k] * 0.72f;
            // 12 first-order allpasses with a high coefficient: high frequencies arrive first -> the chirp
            for (int i = 0; i < kStages; ++i)
            {
                const float a = 0.62f;
                const float y = -a * v + chain[k][i];
                chain[k][i] = v + a * y;
                v = y;
            }
            loop[k].push (v);
            fb[k] = lp[k].lp (loop[k].readLinear ((lens[k] + wob) * sr));
            out += fb[k];
        }
        return out * 0.6f;
    }
private:
    static constexpr int kStages = 12;
    float sr = 48000; DelayLine loop[2]; float chain[2][kStages] {}; float fb[2] {}; OnePole lp[2]; Biquad hp; Lfo lfo;
};

// ---------------------------------------------------------------------------
/** Shared bits of the tank reverbs: pre-delay, low cut on the wet signal, mix. */
struct ReverbFrame
{
    void prepare (double s, int maxBlock)
    {
        sr = s; for (auto& d : pre) d.prepare ((int) (0.3 * s) + 8);
        mixSm.reset (s, 30.f, 0.3f); juce::ignoreUnused (maxBlock);
        cutCache = -1;
    }
    void clear() { for (auto& d : pre) d.clear(); for (auto& f : cut) f.reset(); }
    void setLowCut (float hz) { if (std::abs (hz - cutCache) > 0.5f) { cutCache = hz; for (auto& f : cut) f.set (Biquad::HP, sr, hz, 0.6f); } }
    double sr = 48000; DelayLine pre[2]; Biquad cut[2]; Smoother mixSm; float cutCache = -1;
};

// ===========================================================================
//  ELYSIUM: huge cathedral hall, infinite hold
// ===========================================================================
class Elysium : public Module
{
public:
    void prepare (const ModuleContext& c) override
    {
        tank.prepare (c.sampleRate); fr.prepare (c.sampleRate, c.maxBlock);
        inSm.reset (c.sampleRate, 120.f, 1.f);
        for (auto& e : er) e.prepare ((int) (0.12 * c.sampleRate));
        for (auto& a : erDiff) a.prepare ((int) (0.01 * c.sampleRate));
        reset();
    }
    void reset() override { primed = false; tank.clear(); fr.clear(); for (auto& e : er) e.clear(); for (auto& a : erDiff) a.clear(); }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: decay s, size %, pre-delay ms, tone Hz, low cut Hz, mod, mix %, infinite
        const bool inf = p[7] > 0.5f;
        tank.set (p[1] * 0.01f, p[0], p[3], juce::jmin (16000.f, p[3] * 2.2f), p[5] * 0.1f, 0.6f, 0.15f, inf);
        fr.setLowCut (p[4]);
        fr.mixSm.set (p[6] * 0.01f);
        inSm.set (inf ? 0.f : 1.f);                     // infinite: the tail holds, new notes play dry over it
        const float preS = p[2] * 0.001f * (float) fr.sr;
        if (! primed) { fr.mixSm.snap (fr.mixSm.targetValue()); inSm.snap (inSm.targetValue()); primed = true; }   // a new pedal / preset starts at its settings (no glide)
        const float erLen = (8.f + 22.f * p[1] * 0.01f) * 0.001f * (float) fr.sr;
        const float msToS = 0.001f * (float) fr.sr;
        for (int i = 0; i < n; ++i)
        {
            const float inL = ch[0][i], inR = numCh > 1 ? ch[1][i] : inL;
            fr.pre[0].push (0.5f * (inL + inR));
            const float x = (preS > 1.f ? fr.pre[0].readLinear (preS) : 0.5f * (inL + inR)) * inSm.next();
            // early reflections: the hall's walls (a dozen taps, softened by two short allpasses) before the tail blooms
            er[0].push (x);
            float eL = 0, eR = 0;
            for (int k = 0; k < kEr; ++k) { eL += kErG[k] * er[0].readLinear (erLen * kErL[k]); eR += kErG[k] * er[0].readLinear (erLen * kErR[k]); }
            eL = erDiff[0].process (erDiff[1].process (eL, 3.1f * msToS, 0.6f), 5.3f * msToS, 0.55f);
            eR = erDiff[2].process (erDiff[3].process (eR, 3.7f * msToS, 0.6f), 4.9f * msToS, 0.55f);
            float wl, wr; tank.tick (x * 0.8f, wl, wr);
            wl = fr.cut[0].process (wl + eL * 0.5f); wr = fr.cut[1].process (wr + eR * 0.5f);
            const float m = fr.mixSm.next(), dg = dryGain (m);
            if (numCh > 1) { ch[0][i] = inL * dg + wl * m; ch[1][i] = inR * dg + wr * m; }
            else ch[0][i] = inL * dg + 0.5f * (wl + wr) * m;
        }
    }
private:
    static constexpr int kEr = 12;
    static constexpr float kErL[kEr] = { 0.31f, 0.47f, 0.62f, 0.79f, 0.97f, 1.13f, 1.37f, 1.58f, 1.81f, 2.03f, 2.37f, 2.71f };
    static constexpr float kErR[kEr] = { 0.37f, 0.53f, 0.71f, 0.83f, 1.03f, 1.19f, 1.43f, 1.67f, 1.93f, 2.17f, 2.49f, 2.83f };
    static constexpr float kErG[kEr] = { 0.16f, 0.15f, 0.14f, 0.13f, 0.12f, 0.11f, 0.10f, 0.09f, 0.08f, 0.07f, 0.06f, 0.05f };
    LushTank tank; ReverbFrame fr; Smoother inSm; DelayLine er[2]; Allpass erDiff[4];
    bool primed = false;
};

// ===========================================================================
//  EMPYREAN: two-voice shimmer (octave + a second interval) inside the lush tank
// ===========================================================================
class Empyrean : public Module
{
public:
    void prepare (const ModuleContext& c) override
    {
        tank.prepare (c.sampleRate); fr.prepare (c.sampleRate, c.maxBlock);
        shA.prepare (c.sampleRate, 62.f); shB.prepare (c.sampleRate, 74.f);
        hp.set (Biquad::HP, c.sampleRate, 220.f, 0.6f);
        aSm.reset (c.sampleRate, 60.f, 0.f); bSm.reset (c.sampleRate, 60.f, 0.f);
        reset();
    }
    void reset() override { primed = false; tank.clear(); fr.clear(); shA.reset(); shB.reset(); hp.reset(); lp.reset(); fb = 0; shA.setSemitones (12.f); shB.setSemitones (semB); }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: decay s, size %, shimmer % (octave voice), voice II %, interval II, tone Hz, mod, mix %
        static constexpr float intervals[] = { 7.f, 5.f, 24.f, -12.f, 4.f };
        const int iv = juce::jlimit (0, 4, (int) std::lround (p[4]));
        if (intervals[iv] != semB) { semB = intervals[iv]; shB.setSemitones (semB); }
        tank.set (p[1] * 0.01f, p[0], p[5], juce::jmin (16000.f, p[5] * 1.8f), p[6] * 0.1f, 0.35f, 0.1f, false);
        lp.setLowpass (fr.sr, p[5]);
        aSm.set (p[2] * 0.01f); bSm.set (p[3] * 0.01f);
        fr.setLowCut (120.f); fr.mixSm.set (p[7] * 0.01f);
        if (! primed) { fr.mixSm.snap (fr.mixSm.targetValue()); aSm.snap (aSm.targetValue()); bSm.snap (bSm.targetValue()); primed = true; }   // a new pedal / preset starts at its settings (no glide)
        for (int i = 0; i < n; ++i)
        {
            const float inL = ch[0][i], inR = numCh > 1 ? ch[1][i] : inL;
            const float a = aSm.next(), b = bSm.next();
            float wl, wr; tank.tick (0.5f * (inL + inR) * 0.8f + fb, wl, wr);
            // the shimmer loop: the tail, without its lows, pitched up and fed back in -> it keeps rising
            const float s = lp.lp (hp.process (0.5f * (wl + wr)));
            fb = softLimit ((shA.process (s) * a * 0.62f + shB.process (s) * b * 0.5f));
            wl = fr.cut[0].process (wl); wr = fr.cut[1].process (wr);
            const float m = fr.mixSm.next(), dg = dryGain (m);
            if (numCh > 1) { ch[0][i] = inL * dg + wl * m; ch[1][i] = inR * dg + wr * m; }
            else ch[0][i] = inL * dg + 0.5f * (wl + wr) * m;
        }
    }
private:
    LushTank tank; ReverbFrame fr; GrainShifter shA, shB; Biquad hp; OnePole lp; Smoother aSm, bSm; float fb = 0, semB = 7.f;
    bool primed = false;
};

// ===========================================================================
//  NIMBUS: cloud reverb (long smeared diffusion, slow swell, drifting pitch, freeze)
// ===========================================================================
class Nimbus : public Module
{
public:
    void prepare (const ModuleContext& c) override
    {
        sr = (float) c.sampleRate;
        tank.prepare (c.sampleRate); fr.prepare (c.sampleRate, c.maxBlock);
        for (int i = 0; i < kAps; ++i) { cloud[i].prepare ((int) (0.2 * sr)); lfo[i].setSampleRate (sr); lfo[i].setRate (0.07f + 0.031f * i); lfo[i].setPhase (0.17f * i); }
        fast.set (c.sampleRate, 1.f, 120.f); slow.set (c.sampleRate, 500.f, 160.f);
        inSm.reset (c.sampleRate, 200.f, 1.f);
        reset();
    }
    void reset() override { primed = false; tank.clear(); fr.clear(); for (auto& a : cloud) a.clear(); fast.reset(); slow.reset(); }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: decay s, cloud %, drift %, swell ms, tone Hz, low cut Hz, mix %, freeze
        const bool frz = p[7] > 0.5f;
        const float cl = p[1] * 0.01f, dr = p[2] * 0.01f;
        tank.set (0.85f, p[0], p[4], p[4] * 1.4f, 0.35f + 0.4f * dr, 0.4f + 0.6f * cl, dr, frz);
        if (std::abs (p[3] - swellCache) > 1.f) { swellCache = p[3]; slow.set (sr, juce::jmax (1.f, p[3]), 160.f); }
        fr.setLowCut (p[5]); fr.mixSm.set (p[6] * 0.01f); inSm.set (frz ? 0.f : 1.f);
        const float g = 0.45f + 0.27f * cl;
        if (! primed) { fr.mixSm.snap (fr.mixSm.targetValue()); inSm.snap (inSm.targetValue()); primed = true; }   // a new pedal / preset starts at its settings (no glide)
        for (int i = 0; i < n; ++i)
        {
            const float inL = ch[0][i], inR = numCh > 1 ? ch[1][i] : inL;
            float x = 0.5f * (inL + inR);
            // swell: the reverb hears each note fade in (pick attack removed)
            const float f = fast.process (x), sl = slow.process (x);
            if (swellCache > 5.f) x *= clampf (sl / (f + 1.0e-4f), 0.f, 1.f);
            x *= inSm.next();
            // the cloud: long, slowly drifting diffusion before the tank
            for (int k = 0; k < kAps; ++k)
            {
                const float len = (kCloudMs[k] * (0.35f + 0.65f * cl) + lfo[k].next() * 1.6f * dr) * 0.001f * sr;
                x = cloud[k].process (x, juce::jmax (2.f, len), g);
            }
            float wl, wr; tank.tick (x * 0.85f, wl, wr);
            wl = fr.cut[0].process (wl); wr = fr.cut[1].process (wr);
            const float m = fr.mixSm.next(), dg = dryGain (m);
            if (numCh > 1) { ch[0][i] = inL * dg + wl * m; ch[1][i] = inR * dg + wr * m; }
            else ch[0][i] = inL * dg + 0.5f * (wl + wr) * m;
        }
    }
private:
    static constexpr int kAps = 8;
    static constexpr float kCloudMs[kAps] = { 23.1f, 37.7f, 49.3f, 61.9f, 79.1f, 97.3f, 113.9f, 139.7f };
    float sr = 48000, swellCache = -1;
    LushTank tank; ReverbFrame fr; Allpass cloud[kAps]; Lfo lfo[kAps]; EnvFollower fast, slow; Smoother inSm;
    bool primed = false;
};

// ===========================================================================
//  VESPER: choir reverb (the tail sings through vowel formants)
// ===========================================================================
class Vesper : public Module
{
public:
    void prepare (const ModuleContext& c) override
    {
        sr = c.sampleRate;
        tank.prepare (c.sampleRate); fr.prepare (c.sampleRate, c.maxBlock);
        sh.prepare (c.sampleRate, 70.f);
        breath.setSampleRate (sr); breath.setRate (0.13f);
        choirSm.reset (sr, 60.f, 0.5f);
        hp.set (Biquad::HP, sr, 180.f, 0.6f);
        reset();
    }
    void reset() override
    {
        tank.clear(); fr.clear(); sh.reset(); fb = 0; counter = 0; hp.reset();
        for (auto& side : form) for (auto& b : side) b.reset();
    }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: decay s, size %, vowel 0..4 (A E I O U), choir %, voices, breath %, tone Hz, mix %
        tank.set (p[1] * 0.01f, p[0], p[6], p[6] * 1.5f, 0.45f + 0.3f * p[5] * 0.01f, 0.5f, 0.2f * p[5] * 0.01f, false);
        const int voices = juce::jlimit (0, 2, (int) std::lround (p[4]));
        const float st = voices == 1 ? 12.f : voices == 2 ? 7.f : 0.f;
        if (st != semis) { semis = st; sh.setSemitones (st); }
        choirSm.set (p[3] * 0.01f); fr.setLowCut (100.f); fr.mixSm.set (p[7] * 0.01f);
        vowel = p[2]; breathAmt = p[5] * 0.01f;
        if (! primed) { fr.mixSm.snap (fr.mixSm.targetValue()); choirSm.snap (choirSm.targetValue()); primed = true; }   // a new pedal / preset starts at its settings (no glide)
        for (int i = 0; i < n; ++i)
        {
            if ((counter++ & 31) == 0) updateFormants (breath.next() * 0.9f * breathAmt);
            else breath.next();
            const float inL = ch[0][i], inR = numCh > 1 ? ch[1][i] : inL;
            const float c = choirSm.next();
            float wl, wr; tank.tick (0.5f * (inL + inR) * 0.8f + fb, wl, wr);
            // three formant band-passes per side: the vowel the "choir" sings
            float fl = 0, frr = 0;
            for (int k = 0; k < 3; ++k) { fl += form[0][k].process (wl) * fg[k]; frr += form[1][k].process (wr) * fg[k]; }
            fl *= 2.6f; frr *= 2.6f;
            // a little of the sung tail goes round again (voices: pitched up an octave or a fifth)
            const float sung = hp.process (0.5f * (fl + frr));
            fb = softLimit ((voices > 0 ? sh.process (sung) * 0.4f : sung * 0.32f) * c);
            wl = fr.cut[0].process (wl * (1.f - c * 0.8f) + fl * c);
            wr = fr.cut[1].process (wr * (1.f - c * 0.8f) + frr * c);
            const float m = fr.mixSm.next(), dg = dryGain (m);
            if (numCh > 1) { ch[0][i] = inL * dg + wl * m; ch[1][i] = inR * dg + wr * m; }
            else ch[0][i] = inL * dg + 0.5f * (wl + wr) * m;
        }
    }
private:
    void updateFormants (float wobble)
    {
        // soprano-ish formants (Hz) and levels for A E I O U
        static constexpr float F[5][3] = { { 800, 1150, 2900 }, { 400, 1700, 2600 }, { 300, 2100, 2900 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
        static constexpr float G[5][3] = { { 1.f, 0.6f, 0.25f }, { 1.f, 0.45f, 0.3f }, { 1.f, 0.3f, 0.25f }, { 1.f, 0.55f, 0.15f }, { 1.f, 0.4f, 0.12f } };
        const float v = clampf (vowel + wobble, 0.f, 3.999f);
        const int a = (int) v; const float t = v - (float) a;
        for (int k = 0; k < 3; ++k)
        {
            const float f = lerp (F[a][k], F[a + 1][k], t);
            fg[k] = lerp (G[a][k], G[a + 1][k], t);
            const float q = k == 0 ? 6.f : 9.f;
            form[0][k].set (Biquad::BP, sr, f, q); form[1][k].set (Biquad::BP, sr, f * 1.012f, q);
        }
    }
    double sr = 48000;
    LushTank tank; ReverbFrame fr; GrainShifter sh; Lfo breath; Smoother choirSm; Biquad form[2][3], hp;
    float fg[3] { 1, 0.5f, 0.2f }, vowel = 0, breathAmt = 0, fb = 0, semis = 0; unsigned counter = 0;
    bool primed = false;
};

// ===========================================================================
//  HALO: reverse reverb, or bloom (the tail swells and re-enters itself)
// ===========================================================================
class Halo : public Module
{
public:
    void prepare (const ModuleContext& c) override
    {
        sr = (float) c.sampleRate;
        tank.prepare (c.sampleRate); fr.prepare (c.sampleRate, c.maxBlock);
        for (auto& r : rev) r.prepare ((int) (2.1 * sr));
        bloomLine.prepare ((int) (2.1 * sr)); bloomLp.setLowpass (sr, 5000.f); bloomHp.set (Biquad::HP, sr, 200.f, 0.6f);
        lenSm.reset (sr, 300.f, 0.8f * sr);
        reset();
    }
    void reset() override { primed = false; tank.clear(); fr.clear(); for (auto& r : rev) r.clear(); bloomLine.clear(); bloomHp.reset(); bloomLp.reset(); bfb = 0; }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: mode (reverse / bloom), length ms, decay s, tone Hz, mod, low cut Hz, mix %
        const bool bloom = p[0] > 0.5f;
        tank.set (0.75f, p[2], p[3], p[3] * 1.6f, p[4] * 0.1f, bloom ? 0.8f : 0.4f, 0.1f, false);
        lenSm.set (p[1] * 0.001f * sr);
        fr.setLowCut (p[5]); fr.mixSm.set (p[6] * 0.01f);
        const float bloomG = clampf (0.35f + p[2] * 0.03f, 0.f, 0.72f);
        if (! primed) { fr.mixSm.snap (fr.mixSm.targetValue()); lenSm.snap (lenSm.targetValue()); primed = true; }   // a new pedal / preset starts at its settings (no glide)
        for (int i = 0; i < n; ++i)
        {
            const float inL = ch[0][i], inR = numCh > 1 ? ch[1][i] : inL;
            const float len = lenSm.next();
            float wl, wr;
            if (bloom)
            {
                // the tail comes back in after `length` and builds on itself
                tank.tick (0.5f * (inL + inR) * 0.7f + bfb, wl, wr);
                bloomLine.push (0.5f * (wl + wr));
                bfb = softLimit (bloomLp.lp (bloomHp.process (bloomLine.readLinear (len))) * bloomG);
            }
            else
            {
                tank.tick (0.5f * (inL + inR) * 0.8f, wl, wr);
                wl = rev[0].process (wl, len) * 1.25f; wr = rev[1].process (wr, len * 1.03f) * 1.25f;
            }
            wl = fr.cut[0].process (wl); wr = fr.cut[1].process (wr);
            const float m = fr.mixSm.next(), dg = dryGain (m);
            if (numCh > 1) { ch[0][i] = inL * dg + wl * m; ch[1][i] = inR * dg + wr * m; }
            else ch[0][i] = inL * dg + 0.5f * (wl + wr) * m;
        }
    }
private:
    float sr = 48000, bfb = 0;
    LushTank tank; ReverbFrame fr; ReverseReader rev[2]; DelayLine bloomLine; OnePole bloomLp; Biquad bloomHp; Smoother lenSm;
    bool primed = false;
};

// ===========================================================================
//  ASTRAL: shimmer delay (each repeat climbs; repeats smear into a reverb-like wash)
// ===========================================================================
class Astral : public Module
{
public:
    void prepare (const ModuleContext& c) override
    {
        sr = (float) c.sampleRate;
        for (auto& l : line) l.prepare ((int) (2.2 * sr));
        for (auto& s : sh) s.prepare (c.sampleRate, 48.f);
        for (auto& side : diff) for (auto& a : side) a.prepare ((int) (0.05 * sr));
        lfo.setSampleRate (sr); lfo.setRate (0.6f);
        timeSm.reset (sr, 250.f, 0.48f * sr); mixSm.reset (sr, 30.f, 0.4f);
        reset();
    }
    void reset() override
    {
        for (auto& l : line) l.clear(); for (auto& s : sh) s.reset();
        for (auto& side : diff) for (auto& a : side) a.clear();
        for (auto& f : tone) f.reset(); for (auto& f : hp) f.reset(); toneCache = -1; semis = 0;
    }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: time ms, feedback %, shimmer %, interval, diffuse %, mod %, tone Hz, mix %
        static constexpr float iv[] = { 12.f, 7.f, 24.f, -12.f };
        const float st = iv[juce::jlimit (0, 3, (int) std::lround (p[3]))];
        if (st != semis) { semis = st; sh[0].setSemitones (st); sh[1].setSemitones (st); }
        if (std::abs (p[6] - toneCache) > 0.5f) { toneCache = p[6]; for (int k = 0; k < 2; ++k) { tone[k].set (Biquad::LP, sr, p[6], 0.6f); hp[k].set (Biquad::HP, sr, 140.f, 0.6f); } }
        timeSm.set (clampf (p[0], 20.f, 2000.f) * 0.001f * sr);
        mixSm.set (p[7] * 0.01f);
        const float fbk = p[1] * 0.01f * 0.95f, shim = p[2] * 0.01f, dif = p[4] * 0.01f, mod = p[5] * 0.01f;
        if (! primed) { timeSm.snap (timeSm.targetValue()); mixSm.snap (mixSm.targetValue()); primed = true; }   // a new pedal / preset starts at its settings (no glide)
        static constexpr float dms[2][4] = { { 4.1f, 7.3f, 11.9f, 17.3f }, { 4.7f, 6.7f, 12.7f, 16.1f } };
        for (int i = 0; i < n; ++i)
        {
            const float inL = ch[0][i], inR = numCh > 1 ? ch[1][i] : inL;
            const float t = timeSm.next(), wob = 1.f + mod * 0.004f * lfo.next();
            float y[2] = { line[0].readCubic (t * wob), line[1].readCubic (t * 1.031f * wob) };
            for (int k = 0; k < 2; ++k)
            {
                float v = hp[k].process (tone[k].process (y[k]));
                v = v * (1.f - shim) + sh[k].process (v) * shim * 1.15f;      // the repeat, part of it pitched up
                for (int j = 0; j < 4; ++j) v = diff[k][j].process (v, dms[k][j] * 0.001f * sr, 0.62f * dif);
                line[k].push ((k == 0 ? inL : inR) + softLimit (v * fbk));
            }
            const float m = mixSm.next();
            if (numCh > 1) { ch[0][i] = inL * (1.f - m * 0.5f) + y[0] * m; ch[1][i] = inR * (1.f - m * 0.5f) + y[1] * m; }
            else ch[0][i] = inL * (1.f - m * 0.5f) + 0.5f * (y[0] + y[1]) * m;
        }
    }
private:
    float sr = 48000, toneCache = -1, semis = 0;
    DelayLine line[2]; GrainShifter sh[2]; Allpass diff[2][4]; Biquad tone[2], hp[2]; Lfo lfo; Smoother timeSm, mixSm;
    bool primed = false;
};

// ===========================================================================
//  MNEMOSYNE: three-head tape echo with wow, age and a spring reverb
// ===========================================================================
class Mnemosyne : public Module
{
public:
    void prepare (const ModuleContext& c) override
    {
        sr = (float) c.sampleRate;
        tape.prepare ((int) (2.0 * sr));
        spring.prepare (c.sampleRate);
        wow.setSampleRate (sr); wow.setRate (0.55f); flutter.setSampleRate (sr); flutter.setRate (6.3f);
        timeSm.reset (sr, 400.f, 0.25f * sr); mixSm.reset (sr, 30.f, 0.4f);
        reset();
    }
    void reset() override { primed = false; tape.clear(); spring.clear(); lp.reset(); hp.reset(); fb = 0; ageCache = -1; drift = 0; }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: time ms, intensity %, heads, wow %, age %, spring %, echo %, tone Hz
        static constexpr int heads[7] = { 1, 2, 4, 3, 6, 5, 7 };     // bit 0 = head 1 (x1), bit 1 = head 2 (x2), bit 2 = head 3 (x3)
        const int h = heads[juce::jlimit (0, 6, (int) std::lround (p[2]))];
        const int count = (h & 1) + ((h >> 1) & 1) + ((h >> 2) & 1);
        const float norm = 1.f / std::sqrt ((float) count);
        timeSm.set (clampf (p[0], 40.f, 650.f) * 0.001f * sr);
        mixSm.set (p[6] * 0.01f);
        const float inten = p[1] * 0.01f * 1.02f, wowAmt = p[3] * 0.01f, age = p[4] * 0.01f, spr = p[5] * 0.01f;
        if (std::abs (age - ageCache) > 0.005f || std::abs (p[7] - toneCache) > 1.f)
        {
            ageCache = age; toneCache = p[7];
            lp.set (Biquad::LP, sr, p[7] * (1.f - 0.6f * age), 0.6f); hp.set (Biquad::HP, sr, 90.f + 120.f * age, 0.6f);
        }
        const float drive = 1.1f + 2.2f * age;
        if (! primed) { timeSm.snap (timeSm.targetValue()); mixSm.snap (mixSm.targetValue()); primed = true; }   // a new pedal / preset starts at its settings (no glide)
        for (int i = 0; i < n; ++i)
        {
            const float inL = ch[0][i], inR = numCh > 1 ? ch[1][i] : inL;
            const float x = 0.5f * (inL + inR);
            drift += 0.00003f * (noise.next() - drift * 4.f);
            const float speed = 1.f + wowAmt * (0.0045f * wow.next() + 0.0009f * flutter.next() + 0.02f * drift);
            const float T = timeSm.next() * speed;
            float out = 0;
            if (h & 1) out += tape.readCubic (T);
            if (h & 2) out += tape.readCubic (T * 2.f);
            if (h & 4) out += tape.readCubic (T * 3.f);
            out *= norm;
            // record head: the tape saturates more as it ages, and hisses a little
            const float rec = x + fb;
            tape.push (fastTanh (rec * drive) / drive * 1.05f + noise.next() * 0.0015f * age);
            fb = hp.process (lp.process (out)) * inten;
            const float s = spr > 0.001f ? spring.process (x * 0.6f + out * 0.5f) * spr : 0.f;
            const float m = mixSm.next();
            const float wet = out * m + s;
            if (numCh > 1) { ch[0][i] = inL + wet; ch[1][i] = inR + wet; }
            else ch[0][i] = inL + wet;
        }
    }
private:
    float sr = 48000, fb = 0, ageCache = -1, toneCache = -1, drift = 0;
    DelayLine tape; SpringModel spring; Biquad lp, hp; Lfo wow, flutter; Noise noise; Smoother timeSm, mixSm;
    bool primed = false;
};

// ===========================================================================
//  GEMINI: two delay heads in a rhythm (dotted, triplet, golden...), cross-fed; head B can play backwards
// ===========================================================================
class Gemini : public Module
{
public:
    void prepare (const ModuleContext& c) override
    {
        sr = (float) c.sampleRate;
        for (auto& l : line) l.prepare ((int) (3.2 * sr));
        rev.prepare ((int) (3.2 * sr));
        lfo[0].setSampleRate (sr); lfo[0].setRate (0.43f); lfo[1].setSampleRate (sr); lfo[1].setRate (0.57f); lfo[1].setPhase (0.3f);
        aSm.reset (sr, 300.f, 0.4f * sr); bSm.reset (sr, 300.f, 0.3f * sr); mixSm.reset (sr, 30.f, 0.4f);
        reset();
    }
    void reset() override { primed = false; for (auto& l : line) l.clear(); rev.clear(); for (auto& f : tone) f.reset(); yA = yB = 0; toneCache = -1; }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: time ms, pattern, feedback %, cross %, tone Hz, mod %, mix %, reverse
        static constexpr float ratio[6] = { 0.75f, 0.5f, 2.f / 3.f, 1.f / 3.f, 1.5f, 0.618034f };
        const float rb = ratio[juce::jlimit (0, 5, (int) std::lround (p[1]))];
        aSm.set (clampf (p[0], 20.f, 2000.f) * 0.001f * sr); bSm.set (clampf (p[0] * rb, 10.f, 3000.f) * 0.001f * sr);
        if (std::abs (p[4] - toneCache) > 0.5f) { toneCache = p[4]; for (auto& f : tone) f.set (Biquad::LP, sr, p[4], 0.6f); }
        const float fbk = p[2] * 0.01f * 0.95f, cross = p[3] * 0.01f, mod = p[5] * 0.01f;
        const bool reverse = p[7] > 0.5f;
        mixSm.set (p[6] * 0.01f);
        if (! primed) { aSm.snap (aSm.targetValue()); bSm.snap (bSm.targetValue()); mixSm.snap (mixSm.targetValue()); primed = true; }   // a new pedal / preset starts at its settings (no glide)
        for (int i = 0; i < n; ++i)
        {
            const float inL = ch[0][i], inR = numCh > 1 ? ch[1][i] : inL;
            const float x = 0.5f * (inL + inR);
            const float ta = aSm.next() * (1.f + mod * 0.003f * lfo[0].next()), tb = bSm.next() * (1.f + mod * 0.003f * lfo[1].next());
            const float a = tone[0].process (line[0].readCubic (ta));
            const float bFwd = tone[1].process (line[1].readCubic (tb));
            const float b = reverse ? rev.process (bFwd, tb) * 1.2f : bFwd;
            line[0].push (x + softLimit ((a * (1.f - cross) + b * cross) * fbk));
            line[1].push (x + softLimit ((bFwd * (1.f - cross) + a * cross) * fbk));
            const float m = mixSm.next();
            const float l = a + 0.35f * b, r = b + 0.35f * a;
            if (numCh > 1) { ch[0][i] = inL * (1.f - m * 0.5f) + l * m * 0.8f; ch[1][i] = inR * (1.f - m * 0.5f) + r * m * 0.8f; }
            else ch[0][i] = inL * (1.f - m * 0.5f) + 0.5f * (l + r) * m * 0.8f;
        }
    }
private:
    float sr = 48000, yA = 0, yB = 0, toneCache = -1;
    DelayLine line[2]; ReverseReader rev; Biquad tone[2]; Lfo lfo[2]; Smoother aSm, bSm, mixSm;
    bool primed = false;
};

// ===========================================================================
//  LYRA: 12-string shimmer (an octave course and a detuned unison course, strummed apart)
// ===========================================================================
class Lyra : public Module
{
public:
    void prepare (const ModuleContext& c) override
    {
        sr = (float) c.sampleRate;
        oct.prepare (c.sampleRate, 22.f); oct.setSemitones (12.f);
        for (auto& d : course) d.prepare ((int) (0.06 * sr));
        for (auto& a : chime) a.prepare ((int) (0.02 * sr));
        lfo[0].setSampleRate (sr); lfo[0].setRate (0.33f); lfo[1].setSampleRate (sr); lfo[1].setRate (0.41f); lfo[1].setPhase (0.5f);
        mixSm.reset (sr, 30.f, 1.f);
        shelf.set (Biquad::HighShelf, sr, 3000.f, 0.7f, 6.f); octHp.set (Biquad::HP, sr, 250.f, 0.7f);
        reset();
    }
    void reset() override { primed = false; oct.reset(); oct.setSemitones (12.f); for (auto& d : course) d.clear(); for (auto& a : chime) a.clear(); shelf.reset(); octHp.reset(); chimeCache = -1; }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: octave %, detune cents, spread ms, chime %, width %, mix %
        const float octLvl = p[0] * 0.01f, cents = p[1], spread = p[2] * 0.001f * sr, chm = p[3] * 0.01f, width = p[4] * 0.01f;
        if (std::abs (chm - chimeCache) > 0.005f) { chimeCache = chm; shelf.set (Biquad::HighShelf, sr, 3000.f, 0.7f, 2.f + 8.f * chm); }
        mixSm.set (p[5] * 0.01f);
        if (! primed) { mixSm.snap (mixSm.targetValue()); primed = true; }   // a new pedal / preset starts at its settings (no glide)
        // detune by vibrato on a short delay: depth (samples) for +-cents at the LFO rate
        const float depth = (std::pow (2.f, cents / 1200.f) - 1.f) * sr / (kTwoPi * 0.37f);
        for (int i = 0; i < n; ++i)
        {
            const float inL = ch[0][i], inR = numCh > 1 ? ch[1][i] : inL;
            const float x = 0.5f * (inL + inR);
            float o = shelf.process (octHp.process (oct.process (x)));
            for (int k = 0; k < 3; ++k) o = chime[k].process (o, (1.3f + 1.7f * k) * 0.001f * sr, 0.45f * chm);
            course[0].push (o); course[1].push (x);
            const float octC = course[0].readCubic (2.f + spread);                                       // the octave string, a hair later
            const float uni1 = course[1].readCubic (2.f + spread * 0.5f + depth * (1.f + lfo[0].next())); // detuned unison
            const float uni2 = course[1].readCubic (2.f + spread * 0.8f + depth * (1.f + lfo[1].next()));
            const float wetL = octC * octLvl * 0.9f + uni1 * 0.45f, wetR = octC * octLvl * 0.9f + uni2 * 0.45f;
            const float m = mixSm.next();
            const float mid = 0.5f * (wetL + wetR), side = 0.5f * (wetL - wetR) * width * 2.f;
            if (numCh > 1) { ch[0][i] = inL * (1.f - 0.4f * m) + (mid + side) * m; ch[1][i] = inR * (1.f - 0.4f * m) + (mid - side) * m; }
            else ch[0][i] = inL * (1.f - 0.4f * m) + mid * m;
        }
    }
private:
    float sr = 48000, chimeCache = -1;
    SolaShifter oct; DelayLine course[2]; Allpass chime[3]; Lfo lfo[2]; Smoother mixSm; Biquad shelf, octHp;
    bool primed = false;
};

// ===========================================================================
//  ICARUS: octave-up fuzz (rectified + clipped, 4x oversampled)
// ===========================================================================
class Icarus : public Module
{
public:
    void prepare (const ModuleContext& c) override
    {
        sr = (float) c.sampleRate;
        os = std::make_unique<juce::dsp::Oversampling<float>> (2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        os->initProcessing ((size_t) c.maxBlock);
        for (int k = 0; k < 2; ++k) { dc[k].prepare (sr * 4.0); pre[k].set (Biquad::HP, sr * 4.0, 120.f, 0.7f); dry[k].assign ((size_t) c.maxBlock + 8, 0.f); }
        toneCache = -1; reset();
    }
    void reset() override { if (os) os->reset(); for (int k = 0; k < 2; ++k) { dc[k].reset(); pre[k].reset(); tone[k].reset(); post[k].reset(); } }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: fuzz %, octave %, tone Hz, level dB, mix %
        if (std::abs (p[2] - toneCache) > 0.5f) { toneCache = p[2]; for (int k = 0; k < 2; ++k) { tone[k].set (Biquad::LP, sr, p[2], 0.7f); post[k].set (Biquad::Peak, sr, 900.f, 0.8f, -3.f); } }
        const float gain = dbToGain (6.f + 40.f * p[0] * 0.01f), octv = p[1] * 0.01f, level = dbToGain (p[3]) * 0.5f, mix = p[4] * 0.01f;
        const int nc = juce::jmin (2, numCh);
        n = juce::jmin (n, (int) dry[0].size());
        // keep the dry signal for the blend
        for (int c = 0; c < nc; ++c) std::copy (ch[c], ch[c] + n, dry[c].begin());
        juce::dsp::AudioBlock<float> block (ch, (size_t) nc, (size_t) n);
        auto up = os->processSamplesUp (block);
        for (size_t c = 0; c < up.getNumChannels(); ++c)
        {
            auto* d = up.getChannelPointer (c);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
            {
                const float x = pre[c].process (d[i]) * gain;
                const float rect = dc[c].process (std::abs (x) * 1.8f);            // full-wave rectifier: the octave up
                const float mixd = x * (1.f - octv) + rect * octv;
                d[i] = fastTanh (mixd + 0.12f) - 0.119f;                           // a little asymmetry, like silicon
            }
        }
        os->processSamplesDown (block);
        for (int c = 0; c < nc; ++c)
            for (int i = 0; i < n; ++i)
                ch[c][i] = dry[c][(size_t) i] * (1.f - mix) + post[c].process (tone[c].process (ch[c][i])) * level * mix;
    }
private:
    float sr = 48000, toneCache = -1;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    DcBlocker dc[2]; Biquad pre[2], tone[2], post[2];
    std::vector<float> dry[2];
};
} // namespace

// ---------------------------------------------------------------------------
void registerCelestialModules (std::vector<ModuleType>& t)
{
    using P = ModuleParam;
    auto choice = [] (const char* name, juce::StringArray c, float def = 0) { P x { name, "", 0, (float) (c.size() - 1), def }; x.choices = c; return x; };
    auto onOff = [] (const char* name, float def = 0) { P x { name, "", 0, 1, def }; x.toggle = true; return x; };

    t.push_back ({ "elysium", "Elysium Hall", "reverb", RackPost | RackSfx,
        "Huge, lush cathedral hall: early reflections, then a long, softly moving tail. INFINITE holds the tail forever while you play over it.",
        { P { "Decay", "s", 0.5f, 30.f, 7.f, 2.f }, P { "Size", "%", 0, 100, 85 }, P { "Pre-delay", "ms", 0, 250, 40, 2.f }, P { "Tone", "Hz", 1000, 16000, 6500, 2.f },
          P { "Low Cut", "Hz", 20, 600, 150, 2.f }, P { "Mod", "", 0, 10, 5 }, P { "Mix", "%", 0, 100, 35 }, onOff ("Infinite") },
        [] { return std::make_unique<Elysium>(); } });
    t.push_back ({ "empyrean", "Empyrean Shimmer", "reverb", RackPost | RackSfx,
        "Two-voice shimmer: an octave voice and a second interval rise through a lush tail, like a choir of strings.",
        { P { "Decay", "s", 1.f, 30.f, 10.f, 2.f }, P { "Size", "%", 0, 100, 90 }, P { "Shimmer", "%", 0, 100, 60 }, P { "Voice II", "%", 0, 100, 30 },
          choice ("Interval", { "5TH", "4TH", "2 OCT", "SUB", "3RD" }), P { "Tone", "Hz", 2000, 12000, 6000, 2.f }, P { "Mod", "", 0, 10, 6 }, P { "Mix", "%", 0, 100, 40 } },
        [] { return std::make_unique<Empyrean>(); } });
    t.push_back ({ "nimbus", "Nimbus Cloud", "reverb", RackPost | RackSfx,
        "Cloud reverb: notes dissolve into a slowly drifting haze. Swell fades the reverb in under each note; FREEZE holds the cloud.",
        { P { "Decay", "s", 1.f, 40.f, 14.f, 2.f }, P { "Cloud", "%", 0, 100, 75 }, P { "Drift", "%", 0, 100, 40 }, P { "Swell", "ms", 0, 3000, 200, 2.f },
          P { "Tone", "Hz", 1000, 12000, 5000, 2.f }, P { "Low Cut", "Hz", 20, 600, 120, 2.f }, P { "Mix", "%", 0, 100, 45 }, onOff ("Freeze") },
        [] { return std::make_unique<Nimbus>(); } });
    t.push_back ({ "vesper", "Vesper Choir", "reverb", RackPost | RackSfx,
        "Choir reverb: the tail sings through vowel formants (A E I O U). Voices adds an octave or a fifth; Breath makes the choir move.",
        { P { "Decay", "s", 1.f, 30.f, 9.f, 2.f }, P { "Size", "%", 0, 100, 80 }, P { "Vowel", "", 0, 4, 0 }, P { "Choir", "%", 0, 100, 70 },
          choice ("Voices", { "UNISON", "OCTAVE", "FIFTH" }), P { "Breath", "%", 0, 100, 40 }, P { "Tone", "Hz", 1000, 12000, 7000, 2.f }, P { "Mix", "%", 0, 100, 40 } },
        [] { return std::make_unique<Vesper>(); } });
    t.push_back ({ "halo", "Halo Reverse", "reverb", RackPost | RackSfx,
        "REVERSE: every note's reverb plays backwards, swelling into the next. BLOOM: the tail comes back around and builds on itself.",
        { choice ("Mode", { "REVERSE", "BLOOM" }), P { "Length", "ms", 150, 2000, 800, 2.f }, P { "Decay", "s", 0.5f, 20.f, 5.f, 2.f }, P { "Tone", "Hz", 1000, 16000, 7000, 2.f },
          P { "Mod", "", 0, 10, 4 }, P { "Low Cut", "Hz", 20, 600, 150, 2.f }, P { "Mix", "%", 0, 100, 45 } },
        [] { return std::make_unique<Halo>(); } });
    t.push_back ({ "astral", "Astral Shimmer Delay", "delay", RackPost | RackSfx,
        "Shimmer delay: each repeat climbs an octave (or a fifth) and can smear into a reverb-like wash with Diffuse.",
        { P { "Time", "ms", 20, 2000, 480, 2.f }, P { "Feedback", "%", 0, 100, 55 }, P { "Shimmer", "%", 0, 100, 50 }, choice ("Interval", { "+OCT", "+5TH", "+2 OCT", "-OCT" }),
          P { "Diffuse", "%", 0, 100, 40 }, P { "Mod", "%", 0, 100, 25 }, P { "Tone", "Hz", 1000, 12000, 6000, 2.f }, P { "Mix", "%", 0, 100, 40 } },
        [] { return std::make_unique<Astral>(); } });
    t.push_back ({ "mnemosyne", "Mnemosyne Tape Echo", "delay", RackPost | RackSfx,
        "Three-head tape echo with a spring reverb: pick the heads, age the tape, add wow. Intensity past 95% runs away like the real thing.",
        { P { "Time", "ms", 40, 650, 250, 2.f }, P { "Intensity", "%", 0, 100, 45 }, choice ("Heads", { "1", "2", "3", "1+2", "2+3", "1+3", "1+2+3" }, 3),
          P { "Wow", "%", 0, 100, 30 }, P { "Age", "%", 0, 100, 35 }, P { "Spring", "%", 0, 100, 25 }, P { "Echo", "%", 0, 100, 45 }, P { "Tone", "Hz", 1500, 12000, 6500, 2.f } },
        [] { return std::make_unique<Mnemosyne>(); } });
    t.push_back ({ "gemini", "Gemini Dual Delay", "delay", RackPost | RackSfx,
        "Two delay heads in a rhythm (dotted, triplet, golden ratio...), cross-fed across the stereo field. REVERSE plays head B backwards.",
        { P { "Time", "ms", 20, 2000, 400, 2.f }, choice ("Pattern", { "DOTTED", "HALF", "TRIPLET", "THIRD", "1.5X", "GOLDEN" }), P { "Feedback", "%", 0, 100, 45 },
          P { "Cross", "%", 0, 100, 35 }, P { "Tone", "Hz", 1000, 12000, 5500, 2.f }, P { "Mod", "%", 0, 100, 15 }, P { "Mix", "%", 0, 100, 40 }, onOff ("Reverse") },
        [] { return std::make_unique<Gemini>(); } });
    t.push_back ({ "lyra", "Lyra 12-String", "pitch", RackPre | RackPost,
        "12-string shimmer: adds a chiming octave string and a detuned twin to every note, strummed a hair apart.",
        { P { "Octave", "%", 0, 100, 55 }, P { "Detune", "ct", 0, 25, 8 }, P { "Spread", "ms", 0, 25, 9 }, P { "Chime", "%", 0, 100, 50 },
          P { "Width", "%", 0, 100, 60 }, P { "Mix", "%", 0, 100, 100 } },
        [] { return std::make_unique<Lyra>(); } });
    t.push_back ({ "icarus", "Icarus Octave Fuzz", "drive", RackPre | RackPost,
        "Octave-up fuzz: a searing fuzz with a ringing octave above it (best on the neck pickup, above the 7th fret).",
        { P { "Fuzz", "%", 0, 100, 60 }, P { "Octave", "%", 0, 100, 70 }, P { "Tone", "Hz", 800, 8000, 3000, 2.f }, P { "Level", "dB", -24, 12, 0 }, P { "Mix", "%", 0, 100, 100 } },
        [] { return std::make_unique<Icarus>(); } });
}
} // namespace obs::dsp
