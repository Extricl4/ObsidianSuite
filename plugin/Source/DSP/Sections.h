#pragma once
// =============================================================================
//  Header-bar sections: noise gate, transpose, doubler, width, graphic EQ,
//  metronome and level meters. Each one: prepare() off the audio thread,
//  process() on it, nothing allocates while playing.
// =============================================================================
#include "DspCore.h"
#include "PitchShifter.h"
#include <signalsmith-stretch/signalsmith-stretch.h>
#include <atomic>

namespace obs::dsp
{
// ---------------------------------------------------------------------------
/** Noise gate with hysteresis, hold and smooth release (no chatter). */
class NoiseGate
{
public:
    void prepare (double s)
    {
        sr = s; det.set (s, 0.3f, 40.f);
        attackCoef = std::exp (-1.f / (float) (0.0008 * s));
        releaseCoef = std::exp (-1.f / (float) (0.090 * s));
        holdSamples = (int) (0.050 * s);
        reset();
    }
    void reset()            { det.reset(); gain = 1.f; open = true; hold = 0; }

    /** threshDb: open threshold. Gate closes 6 dB below it. */
    void process (float* x, int n, float threshDb, bool enabled)
    {
        if (! enabled)
        {
            // ramp to unity so toggling never clicks
            for (int i = 0; i < n; ++i) { gain = 1.f + (gain - 1.f) * attackCoef; x[i] *= gain; }
            open = true; return;
        }
        const float openLin = dbToGain (threshDb), closeLin = dbToGain (threshDb - 6.f);
        for (int i = 0; i < n; ++i)
        {
            const float e = det.process (x[i]);
            if (e > openLin) { open = true; hold = holdSamples; }
            else if (e < closeLin) { if (hold > 0) --hold; else open = false; }
            const float target = open ? 1.f : 0.f;
            gain = target + (gain - target) * (open ? attackCoef : releaseCoef);
            x[i] *= gain;
        }
        reduction.store (gain);
    }
    float currentGain() const { return reduction.load (std::memory_order_relaxed); }

private:
    double sr = 48000;
    EnvFollower det;
    float attackCoef = 0, releaseCoef = 0, gain = 1.f;
    bool open = true; int hold = 0, holdSamples = 2400;
    std::atomic<float> reduction { 1.f };
};

// ---------------------------------------------------------------------------
/** Transpose with two engines:
     Live : SolaShifter, ~15 ms, best for playing in real time (single notes / riffs)
     HQ   : Signalsmith Stretch (polyphonic, studio quality, ~70 ms). When HQ is on
            the delay is constant (even at 0 semitones) so a DAW can compensate it. */
class Transposer
{
public:
    void prepare (double s, int maxBlock)
    {
        sr = s;
        live.prepare (s, 26.f);
        hq.configure (1, (int) (s * 0.064), (int) (s * 0.016));
        hqLatency = hq.inputLatency() + hq.outputLatency();
        hqDelay.prepare (hqLatency + 8);
        hqOut.assign ((size_t) maxBlock, 0.f);
        fade.reset (s, 30.f, 0.f);
        reset();
    }
    void reset()
    {
        live.reset(); hq.reset(); hqDelay.clear(); fade.snap (current != 0 ? 1.f : 0.f);
    }
    int latency (bool hqMode) const       { return hqMode ? hqLatency : 0; }

    void process (float* x, int n, int semitones, bool hqMode)
    {
        if (hqMode != lastHq) { lastHq = hqMode; hq.reset(); live.reset(); hqDelay.clear(); }
        if (semitones != 0) current = semitones;
        fade.set (semitones != 0 ? 1.f : 0.f);

        if (hqMode)
        {
            hq.setTransposeSemitones ((float) current);
            float* in[1] = { x };
            float* out[1] = { hqOut.data() };
            n = juce::jmin (n, (int) hqOut.size());
            hq.process (in, n, out, n);
            for (int i = 0; i < n; ++i)
            {
                hqDelay.push (x[i]);
                const float dry = hqDelay.at (hqLatency - 1);   // keep dry aligned with the shifted path
                const float g = fade.next();
                float w = hqOut[(size_t) i];
                if (! std::isfinite (w)) w = 0.f;
                x[i] = dry * (1.f - g) + w * g;
            }
            return;
        }

        if (semitones != 0) live.setSemitones ((float) semitones);
        if (semitones == 0 && ! fade.isSmoothing() && fade.current() == 0.f)
        {
            for (int i = 0; i < n; ++i) live.process (x[i]);   // keep it fed so switching on is clean
            return;
        }
        for (int i = 0; i < n; ++i)
        {
            const float wet = live.process (x[i]);
            const float g = fade.next();
            x[i] = x[i] * (1.f - g) + wet * g;
        }
    }
private:
    double sr = 48000;
    SolaShifter live;
    signalsmith::stretch::SignalsmithStretch<float> hq { 0x0b5d };
    DelayLine hqDelay;
    std::vector<float> hqOut;
    int hqLatency = 0, current = 0; bool lastHq = false;
    Smoother fade;
};

// ---------------------------------------------------------------------------
/** Doubler: fakes a second take: the right side is a slightly delayed, gently
    drifting copy, so a mono guitar turns into a wide double-tracked one. */
class Doubler
{
public:
    void prepare (double s)
    {
        sr = s; lineL.prepare ((int) (0.05 * s)); lineR.prepare ((int) (0.05 * s));
        lfoA.setSampleRate (s); lfoA.setRate (0.31f); lfoB.setSampleRate (s); lfoB.setRate (0.23f); lfoB.setPhase (0.37f);
        tone.set (Biquad::HighShelf, s, 4500.f, 0.7f, -1.5f);
        mixSm.reset (s, 40.f, 1.f); timeSm.reset (s, 60.f, 6.f);
    }
    void reset()                         { lineL.clear(); lineR.clear(); tone.reset(); }
    void process (float* L, float* R, int n, float timeMs, bool on)
    {
        mixSm.set (on ? 1.f : 0.f);
        timeSm.set (clampf (timeMs, 0.f, 24.f));
        if (! on && ! mixSm.isSmoothing() && mixSm.current() == 0.f)
        {
            for (int i = 0; i < n; ++i) { lineL.push (L[i]); lineR.push (R[i]); }
            return;
        }
        for (int i = 0; i < n; ++i)
        {
            lineL.push (L[i]); lineR.push (R[i]);
            const float t = timeSm.next(), m = mixSm.next();
            const float dR = (t + 0.35f * lfoA.next()) * 0.001f * (float) sr;
            const float dL = (0.6f + 0.25f * lfoB.next()) * 0.001f * (float) sr;
            const float wetR = tone.process (lineR.readCubic (juce::jmax (1.f, dR)));
            const float wetL = lineL.readCubic (juce::jmax (1.f, dL));
            L[i] = L[i] * (1.f - m) + wetL * m;
            R[i] = R[i] * (1.f - m) + wetR * m;
        }
    }
private:
    double sr = 48000;
    DelayLine lineL, lineR;
    Lfo lfoA, lfoB;
    Biquad tone;
    Smoother mixSm, timeSm;
};

// ---------------------------------------------------------------------------
/** Mid/side stereo width: 0% mono, 100% unchanged, 200% extra wide. */
class StereoWidth
{
public:
    void prepare (double s)             { sm.reset (s, 40.f, 1.f); }
    void process (float* L, float* R, int n, float widthPct)
    {
        sm.set (clampf (widthPct, 0.f, 200.f) * 0.01f);
        if (! sm.isSmoothing() && std::abs (sm.current() - 1.f) < 1e-4f) return;
        for (int i = 0; i < n; ++i)
        {
            const float w = sm.next();
            const float m = 0.5f * (L[i] + R[i]), s = 0.5f * (L[i] - R[i]) * w;
            // gentle make-up so very wide settings don't jump in level
            const float comp = w > 1.f ? 1.f / std::sqrt (0.5f + 0.5f * w) : 1.f;
            L[i] = (m + s) * comp; R[i] = (m - s) * comp;
        }
    }
private:
    Smoother sm;
};

// ---------------------------------------------------------------------------
/** 9-band graphic EQ (63 Hz ... 16 kHz, +-12 dB) with 12 dB/oct low and high cuts. */
class GraphicEq
{
public:
    static constexpr int kBands = 9;
    void prepare (double s)             { sr = s; for (auto& c : cache) c = 999.f; lcCache = hcCache = -1.f; step = (float) (1.0 / (0.02 * sr)); reset(); }
    void reset()                        { for (auto& ch : bands) for (auto& b : ch) b.reset(); for (auto& f : lc) f.reset(); for (auto& f : hc) f.reset(); lcMix = lcWant; hcMix = hcWant; }

    void setBand (int i, float freq, float gainDb)
    {
        if (std::abs (cache[i] - gainDb) < 1e-3f) return;
        cache[i] = gainDb;
        for (auto& ch : bands) ch[i].set (Biquad::Peak, sr, freq, 1.4f, gainDb);
    }
    /** lcOn / hcOn: the cuts' own switches. A cut at its end (20 Hz / 20 kHz) is off too.
        Switching fades the cut in or out over 20 ms, so it never clicks. */
    void setCuts (float lcHz, float hcHz, bool lcOn = true, bool hcOn = true)
    {
        if (lcHz != lcCache) { lcCache = lcHz; for (auto& f : lc) f.set (Biquad::HP, sr, lcHz, 0.707f); }
        if (hcHz != hcCache) { hcCache = hcHz; for (auto& f : hc) f.set (Biquad::LP, sr, hcHz, 0.707f); }
        lcWant = lcOn && lcHz > 21.f ? 1.f : 0.f;
        hcWant = hcOn && hcHz < 19500.f ? 1.f : 0.f;
    }
    void process (float* const* ch, int numCh, int n)
    {
        // a cut that starts fading in begins from a clean filter state
        if (lcMix <= 0.f && lcWant > 0.f) for (auto& f : lc) f.reset();
        if (hcMix <= 0.f && hcWant > 0.f) for (auto& f : hc) f.reset();
        float lcEnd = lcMix, hcEnd = hcMix;
        for (int c = 0; c < juce::jmin (2, numCh); ++c)
        {
            float* x = ch[c];
            for (int b = 0; b < kBands; ++b)
            {
                if (std::abs (cache[b]) < 0.01f) continue;      // flat band: skip
                auto& bq = bands[c][b];
                for (int i = 0; i < n; ++i) x[i] = bq.process (x[i]);
            }
            lcEnd = cut (lc[c], x, n, lcMix, lcWant);
            hcEnd = cut (hc[c], x, n, hcMix, hcWant);
        }
        lcMix = lcEnd; hcMix = hcEnd;
    }
    float lowCutAmount() const          { return lcMix; }       // 0 off .. 1 in (for tests)
    float highCutAmount() const         { return hcMix; }
private:
    double sr = 48000;
    Biquad bands[2][kBands];
    Biquad lc[2], hc[2];
    float cache[kBands] {};
    float lcCache = -1.f, hcCache = -1.f;
    float lcWant = 0.f, hcWant = 0.f, lcMix = 0.f, hcMix = 0.f, step = 1.f / 960.f;

    /** One cut on one channel, faded by mix (0..1) towards want. Returns where the fade ended. */
    float cut (Biquad& f, float* x, int n, float mix, float want) const
    {
        if (mix <= 0.f && want <= 0.f) return 0.f;                       // off: nothing to do
        if (mix >= 1.f && want >= 1.f) { for (int i = 0; i < n; ++i) x[i] = f.process (x[i]); return 1.f; }
        for (int i = 0; i < n; ++i)
        {
            mix = want > mix ? juce::jmin (want, mix + step) : juce::jmax (want, mix - step);
            const float y = f.process (x[i]);
            x[i] += mix * (y - x[i]);
        }
        return mix;
    }
};

// ---------------------------------------------------------------------------
/** Metronome: click on every beat, accented first beat. Follows the host's
    tempo/position when the host is playing, otherwise its own BPM. */
class Metronome
{
public:
    void prepare (double s)             { sr = s; phase = 0; clickLeft = 0; beatIndex = -1; }
    void setRunning (bool r)            { if (r && ! running) { samplesToBeat = 0; beatIndex = -1; } running = r; }
    bool isRunning() const              { return running; }

    /** Adds the click into L/R. bpm and beatsPerBar from the UI; volume 0..1. */
    void process (float* L, float* R, int n, double bpm, int beatsPerBar, bool accentFirst, float volume)
    {
        if (! running) return;
        bpm = juce::jlimit (20.0, 400.0, bpm);
        const double spb = sr * 60.0 / bpm;
        for (int i = 0; i < n; ++i)
        {
            if (samplesToBeat <= 0.0)
            {
                beatIndex = (beatIndex + 1) % juce::jmax (1, beatsPerBar);
                const bool acc = accentFirst && beatIndex == 0;
                clickFreq = acc ? 1760.f : 1175.f; clickAmp = (acc ? 0.9f : 0.6f) * volume;
                clickLeft = (int) (0.035 * sr); phase = 0;
                samplesToBeat += spb;
                lastBeat.store (beatIndex);
            }
            samplesToBeat -= 1.0;
            if (clickLeft > 0)
            {
                const float env = (float) clickLeft / (float) (0.035 * sr);
                const float s = std::sin (phase) * clickAmp * env * env;
                phase += kTwoPi * clickFreq / (float) sr;
                L[i] += s; R[i] += s; --clickLeft;
            }
        }
    }
    int currentBeat() const             { return lastBeat.load(); }
private:
    double sr = 48000, samplesToBeat = 0;
    bool running = false;
    int beatIndex = -1, clickLeft = 0;
    float phase = 0, clickFreq = 1000, clickAmp = 0;
    std::atomic<int> lastBeat { -1 };
};

// ---------------------------------------------------------------------------
/** Peak meter with fall-off, readable from the UI thread. */
class PeakMeter
{
public:
    void process (const float* x, int n)
    {
        float pk = 0.f;
        for (int i = 0; i < n; ++i) pk = std::max (pk, std::abs (x[i]));
        float cur = value.load (std::memory_order_relaxed);
        cur = pk > cur ? pk : cur * 0.93f;
        value.store (cur, std::memory_order_relaxed);
        if (pk > 0.999f) clip.store (true);
    }
    float read() const                  { return value.load (std::memory_order_relaxed); }
    bool  takeClip()                    { return clip.exchange (false); }
private:
    std::atomic<float> value { 0.f };
    std::atomic<bool> clip { false };
};
} // namespace obs::dsp
