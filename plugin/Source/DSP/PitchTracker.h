#pragma once
// =============================================================================
//  PitchTracker: YIN pitch detection on a decimated copy of the guitar signal.
//  Runs inside the audio thread (about 10 M multiply-adds per second), so the
//  tuner, the guitar synth and the harmonizer all see the same note with ~10 ms
//  latency. Range 38 Hz (drop tunings transposed down) to 1.5 kHz.
// =============================================================================
#include "DspCore.h"
#include <atomic>

namespace obs::dsp
{
class PitchTracker
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        decim = juce::jmax (1, (int) std::round (sampleRate / 12000.0));
        dsr = sr / decim;
        aa1.set (Biquad::LP, sr, (float) (dsr * 0.42), 0.707f);
        aa2.set (Biquad::LP, sr, (float) (dsr * 0.42), 0.707f);
        hpf.set (Biquad::HP, sr, 30.f, 0.707f);
        tauMax = juce::jmin (kBuf / 2 - 4, (int) (dsr / 38.0));
        tauMin = juce::jmax (2, (int) (dsr / 1500.0));
        win = juce::jmin (kBuf - tauMax - 2, tauMax + 8);
        std::fill (std::begin (ring), std::end (ring), 0.f);
        level.set (sr, 1.f, 120.f);
        reset();
    }

    void reset()
    {
        aa1.reset(); aa2.reset(); hpf.reset(); level.reset();
        pos = 0; phase = 0; hopCount = 0;
        freq.store (0.f); conf.store (0.f);
        lastF = 0;
    }

    /** Feed host-rate mono samples. */
    void process (const float* x, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float s = aa2.process (aa1.process (hpf.process (x[i])));
            level.process (x[i]);
            if (++phase >= decim)
            {
                phase = 0;
                ring[pos] = s;
                pos = (pos + 1) & (kBuf - 1);
                if (++hopCount >= kHop) { hopCount = 0; analyse(); }
            }
        }
        rms.store (level.value());
    }

    float frequency() const     { return freq.load (std::memory_order_relaxed); }   // 0 = none
    float confidence() const    { return conf.load (std::memory_order_relaxed); }
    float inputLevel() const    { return rms.load (std::memory_order_relaxed); }

    /** Midi note number (fractional) for a frequency, given the A4 reference. */
    static float toMidi (float hz, float a4 = 440.f)  { return hz > 0 ? 69.f + 12.f * std::log2 (hz / a4) : -1.f; }
    static float toHz (float midi, float a4 = 440.f)  { return a4 * std::pow (2.f, (midi - 69.f) / 12.f); }

private:
    static constexpr int kBuf = 1024, kHop = 96;

    void analyse()
    {
        // Silence: don't guess.
        if (level.value() < 0.0012f) { conf.store (0.f); freq.store (0.f); return; }

        // Copy the most recent window (oldest first).
        const int need = win + tauMax + 1;
        int start = (pos - need) & (kBuf - 1);
        for (int i = 0; i < need; ++i) frame[i] = ring[(start + i) & (kBuf - 1)];

        // Difference function + cumulative mean normalised difference.
        float runningSum = 0.f;
        d[0] = 1.f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float acc = 0.f;
            const float* a = frame; const float* b = frame + tau;
            for (int j = 0; j < win; ++j) { const float df = a[j] - b[j]; acc += df * df; }
            runningSum += acc;
            d[tau] = runningSum > 0.f ? acc * (float) tau / runningSum : 1.f;
        }

        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (d[tau] < 0.15f)
            {
                while (tau + 1 < tauMax && d[tau + 1] < d[tau]) ++tau;
                best = tau; break;
            }
        }
        if (best < 0)
        {
            // fall back to the global minimum if it is reasonably clear
            float mn = 1.f;
            for (int tau = tauMin; tau < tauMax; ++tau) if (d[tau] < mn) { mn = d[tau]; best = tau; }
            if (mn > 0.35f) { conf.store (juce::jmax (0.f, 1.f - mn)); return; }
        }

        // Parabolic interpolation for sub-sample accuracy.
        float t = (float) best;
        if (best > 1 && best < tauMax - 1)
        {
            const float s0 = d[best - 1], s1 = d[best], s2 = d[best + 1];
            const float den = s0 + s2 - 2.f * s1;
            if (std::abs (den) > 1e-9f) t += 0.5f * (s0 - s2) / den;
        }
        const float f = (float) (dsr / t);
        const float c = clampf (1.f - d[best], 0.f, 1.f);

        // Octave-jump guard: accept a jump only if confident.
        if (lastF > 0 && c < 0.85f)
        {
            const float ratio = f / lastF;
            if (ratio > 1.8f || ratio < 0.55f) { conf.store (c * 0.5f); return; }
        }
        lastF = f;
        freq.store (f); conf.store (c);
    }

    double sr = 48000, dsr = 12000;
    int decim = 4, phase = 0, pos = 0, hopCount = 0, tauMax = 300, tauMin = 8, win = 300;
    float ring[kBuf] {};
    float frame[kBuf] {};
    float d[kBuf / 2 + 8] {};
    float lastF = 0;
    Biquad aa1, aa2, hpf;
    EnvFollower level;
    std::atomic<float> freq { 0.f }, conf { 0.f }, rms { 0.f };
};
} // namespace obs::dsp
