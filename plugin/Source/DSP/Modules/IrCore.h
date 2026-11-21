#pragma once
// =============================================================================
//  IrCore: convolution reverb built on one of a pack pedal's IR files.
//   * the audio thread asks for an IR (index) and a decay length
//   * the message thread (messageTick) trims/fades that IR and hands it to
//     juce::dsp::Convolution, which swaps it in on its own (cross-faded)
//   * pre-delay, tone (high cut) and low cut act on the wet signal only
//  Used by Ossuary, Halcyon (PackModules.cpp) and Crypt's RV modes (TimeModules.cpp).
// =============================================================================
#include "Module.h"
#include <juce_dsp/juce_dsp.h>

namespace obs::dsp
{
class IrCore
{
public:
    static constexpr float kMaxPreDelayMs = 250.f;

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        conv.prepare ({ sampleRate, (juce::uint32) juce::jmax (1, maxBlock), 2 });
        for (auto& b : scratch) b.assign ((size_t) juce::jmax (1, maxBlock), 0.f);
        for (auto& d : pre) d.prepare ((int) (kMaxPreDelayMs * 0.001 * sampleRate) + 8);
        lastTone = lastCut = -1.f;
        reset();
    }
    void reset()
    {
        conv.reset();
        for (auto& d : pre) d.clear();
        for (auto& f : tone) f.reset();
        for (auto& f : cut) f.reset();
    }

    // ---- audio thread -------------------------------------------------------------------
    /** index = which IR, decay = seconds of tail to keep (longer than the IR = all of it). */
    void setParams (int index, float decaySeconds, float preDelayMs, float toneHz, float lowCutHz)
    {
        wantIndex.store (index, std::memory_order_relaxed);
        wantDecay.store (decaySeconds, std::memory_order_relaxed);
        preSamples = clampf (preDelayMs, 0.f, kMaxPreDelayMs) * 0.001f * (float) sr;
        if (std::abs (toneHz - lastTone) > 0.5f) { lastTone = toneHz; for (auto& f : tone) f.set (Biquad::LP, sr, toneHz, 0.6f); }
        if (std::abs (lowCutHz - lastCut) > 0.5f) { lastCut = lowCutHz; for (auto& f : cut) f.set (Biquad::HP, sr, lowCutHz, 0.6f); }
    }
    bool ready() const { return loaded.load (std::memory_order_acquire); }

    /** Wet signal only. in/out may not alias. n <= maxBlock. */
    void processWet (const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        n = juce::jmin (n, (int) scratch[0].size());
        const float* in[2] = { inL, inR };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                pre[c].push (in[c][i]);
                scratch[c][(size_t) i] = preSamples > 0.5f ? pre[c].readLinear (preSamples) : in[c][i];
            }
        float* chans[2] = { scratch[0].data(), scratch[1].data() };
        juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
        conv.process (juce::dsp::ProcessContextReplacing<float> (block));
        float* out[2] = { outL, outR };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
                out[c][i] = cut[c].process (tone[c].process (scratch[c][(size_t) i])) * kWetTrim;
    }

    // ---- message thread -----------------------------------------------------------------
    void setBank (std::vector<IrFile> irs) { bank = std::move (irs); loadedIndex = -1; }
    int bankSize() const { return (int) bank.size(); }
    juce::String currentName() const { return juce::isPositiveAndBelow (loadedIndex, (int) bank.size()) ? bank[(size_t) loadedIndex].name : juce::String(); }

    /** Rebuilds the IR when the audio thread asked for another one or a different decay. */
    void messageTick()
    {
        if (bank.empty()) return;
        const int idx = wantIndex.load (std::memory_order_relaxed);
        if (! juce::isPositiveAndBelow (idx, (int) bank.size())) return;
        const auto& f = bank[(size_t) idx];
        if (f.buffer.getNumSamples() == 0) return;                       // missing file: stay as we are
        const float full = (float) (f.buffer.getNumSamples() / f.sampleRate);
        const float decay = juce::jlimit (0.1f, full, wantDecay.load (std::memory_order_relaxed));
        if (idx == loadedIndex && std::abs (decay - loadedDecay) <= 0.03f * loadedDecay + 0.01f) return;
        loadedIndex = idx; loadedDecay = decay;

        // Keep `decay` seconds; fade the last 35% out with a raised cosine so a short decay still sounds natural.
        const int keep = juce::jlimit (16, f.buffer.getNumSamples(), (int) std::ceil (decay * f.sampleRate));
        juce::AudioBuffer<float> ir (2, keep);
        for (int c = 0; c < 2; ++c) ir.copyFrom (c, 0, f.buffer, juce::jmin (c, f.buffer.getNumChannels() - 1), 0, keep);
        if (keep < f.buffer.getNumSamples())
        {
            const int fadeLen = juce::jmax (8, (int) (keep * 0.35f)), start = keep - fadeLen;
            for (int c = 0; c < 2; ++c)
            {
                auto* d = ir.getWritePointer (c);
                for (int i = 0; i < fadeLen; ++i) d[start + i] *= 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * (float) i / (float) fadeLen);
            }
        }
        conv.loadImpulseResponse (std::move (ir), f.sampleRate, juce::dsp::Convolution::Stereo::yes,
                                  juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::yes);
        loaded.store (true, std::memory_order_release);
    }

private:
    static constexpr float kWetTrim = 1.0f;     // set from the self-test level check
    double sr = 48000;
    juce::dsp::Convolution conv { juce::dsp::Convolution::NonUniform { 512 } };
    std::vector<float> scratch[2];
    DelayLine pre[2];
    Biquad tone[2], cut[2];
    float preSamples = 0, lastTone = -1, lastCut = -1;
    std::atomic<int> wantIndex { 0 };
    std::atomic<float> wantDecay { 100.f };
    std::atomic<bool> loaded { false };
    // message thread only
    std::vector<IrFile> bank;
    int loadedIndex = -1;
    float loadedDecay = 0;
};
} // namespace obs::dsp
