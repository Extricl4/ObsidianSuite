#pragma once
// =============================================================================
//  Pitch shifters.
//
//  SolaShifter (the default, used by Transpose "Live", Whammy, Octave,
//  Harmonizer, Sub): one read head glides through a short delay line at the
//  new speed. When it runs out of room it jumps back (or forward), and the
//  jump target is chosen by cross-correlation so the old and new waveforms
//  line up before a short crossfade. That removes the warble / chorus of
//  naive shifters. Latency ~10-20 ms.
//
//  GrainShifter: the classic two-head crossfading shifter. Smeary on purpose;
//  used inside the shimmer reverb's feedback loop where smear sounds good.
//
//  (The polyphonic HQ transpose uses Signalsmith Stretch, see Sections.h.)
// =============================================================================
#include "DspCore.h"

namespace obs::dsp
{
class SolaShifter
{
public:
    void prepare (double sampleRate, float windowMs = 26.f)
    {
        sr = sampleRate;
        xfade = juce::jmax (32, (int) (sampleRate * 0.006));            // 6 ms crossfade
        win = juce::jmax (xfade * 3, (int) (sampleRate * windowMs * 0.001));
        search = juce::jmin ((int) (sampleRate * 0.0125), win / 2);      // up to 12.5 ms (periods down to ~80 Hz)
        corrLen = (int) (sampleRate * 0.007);
        line.prepare (win * 2 + search * 2 + corrLen + 64);
        reset();
    }
    void reset()
    {
        line.clear();
        dA = (float) (minDelay() + win / 2); dB = dA; fadePos = -1;
        ratioSm.reset (sr, 25.f, ratioSm.targetValue() > 0 ? ratioSm.targetValue() : 1.f);
    }
    void setSemitones (float st)       { ratioSm.set (std::pow (2.f, clampf (st, -36.f, 36.f) / 12.f)); }
    void setRatio (float r)            { ratioSm.set (clampf (r, 0.125f, 8.f)); }
    int  latencySamples() const        { return minDelay() + win / 2; }

    inline float process (float x)
    {
        line.push (x);
        const float r = ratioSm.next();
        const float step = 1.f - r;                       // delay change per sample
        dA += step; if (fadePos >= 0) dB += step;

        // Out of room? Start a correlated jump (only if not already crossfading).
        if (fadePos < 0)
        {
            const float lo = (float) minDelay(), hi = (float) (minDelay() + win);
            if (dA > hi - 1.f || dA < lo + 1.f)
            {
                const float target = dA > hi - 1.f ? dA - (float) win * 0.5f : dA + (float) win * 0.5f;
                dB = bestMatch (dA, target);
                fadePos = 0;
            }
        }
        // Hard safety: never read outside the line, even while crossfading at extreme ratios.
        dA = clampf (dA, 1.f, (float) (minDelay() + win + search));
        dB = clampf (dB, 1.f, (float) (minDelay() + win + search));

        float y = line.readCubic (dA);
        if (fadePos >= 0)
        {
            const float t = (float) fadePos / (float) xfade;
            const float g = 0.5f - 0.5f * std::cos (kPi * t);              // raised-cosine crossfade
            y = y * (1.f - g) + line.readCubic (dB) * g;
            if (++fadePos >= xfade) { dA = dB; fadePos = -1; }
        }
        return y;
    }

private:
    int minDelay() const               { return corrLen + 4; }

    /** Finds the delay near `target` whose recent waveform best matches the head at `current`. */
    float bestMatch (float current, float target) const
    {
        const float lo = (float) minDelay() + 1.f, hi = (float) (minDelay() + win) - 1.f;
        float best = clampf (target, lo, hi), bestScore = -1e30f;
        const int ci = (int) current;
        auto score = [&] (int cand) -> float
        {
            float acc = 0, ea = 0, eb = 0;
            for (int j = 0; j < corrLen; j += 2)
            {
                const float a = line.at (ci + j), b = line.at (cand + j);
                acc += a * b; ea += a * a; eb += b * b;
            }
            return acc / std::sqrt (ea * eb + 1e-12f);
        };
        const int t = (int) std::round (target);
        for (int k = -search; k <= search; k += 2)        // coarse search ...
        {
            const int cand = t + k;
            if (cand < (int) lo || cand > (int) hi) continue;
            const float s = score (cand);
            if (s > bestScore) { bestScore = s; best = (float) cand; }
        }
        const int coarse = (int) best;
        for (int k = -1; k <= 1; k += 2)                   // ... then refine
        {
            const int cand = coarse + k;
            if (cand < (int) lo || cand > (int) hi) continue;
            const float s = score (cand);
            if (s > bestScore) { bestScore = s; best = (float) cand; }
        }
        // keep the fractional part of the current head so the pitch stays continuous
        return best + (current - (float) ci);
    }

    double sr = 48000;
    DelayLine line;
    int win = 1200, xfade = 288, search = 600, corrLen = 336;
    float dA = 600, dB = 600; int fadePos = -1;
    Smoother ratioSm;
};

/** Two-head crossfading shifter (smeary; used in the shimmer feedback loop). */
class GrainShifter
{
public:
    void prepare (double sampleRate, float windowMs = 32.f)
    {
        sr = sampleRate;
        win = (float) (sampleRate * windowMs * 0.001);
        line.prepare ((int) (win * 3.0f) + 64);
        reset();
    }
    void reset()                       { line.clear(); phase = 0.f; ratioSm.reset (sr, 30.f, ratioSm.targetValue() > 0 ? ratioSm.targetValue() : 1.f); }
    void setSemitones (float st)       { ratioSm.set (std::pow (2.f, clampf (st, -36.f, 36.f) / 12.f)); }
    inline float process (float x)
    {
        line.push (x);
        const float ratio = ratioSm.next();
        phase += (1.f - ratio) / win;
        if (phase >= 1.f || phase < 0.f) phase -= std::floor (phase);
        const float p2 = phase + 0.5f - (phase + 0.5f >= 1.f ? 1.f : 0.f);
        const float w1 = std::sin (kPi * phase), w2 = std::sin (kPi * p2);
        return line.readCubic (phase * win + 2.f) * (w1 * w1) + line.readCubic (p2 * win + 2.f) * (w2 * w2);
    }
private:
    double sr = 48000; DelayLine line; float win = 1500, phase = 0; Smoother ratioSm;
};

/** Default shifter for pedals. */
using PitchShifter = SolaShifter;
} // namespace obs::dsp
