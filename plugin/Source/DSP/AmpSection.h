#pragma once
// =============================================================================
//  AmpSection: the AMP tab.
//   * "nam"  : NAM captures, one per channel (up to 4), switched instantly
//   * "core" : built-in Obsidian Core high-gain amp (no files needed), so the
//              plugin sounds like an amp before any capture is loaded
//   * "none" : no amp (signal passes straight to the cab)
//  Gain drives the capture's input; Bass/Mid/Treble/Presence/Depth/Master are a
//  tone stack after it. Switching captures crossfades, so there are no clicks.
// =============================================================================
#include "DspCore.h"
#include "NamEngine.h"
#include <juce_dsp/juce_dsp.h>

namespace obs::dsp
{
/** Obsidian Core: 3 cascaded asymmetric gain stages, 4x oversampled. */
class CoreAmp
{
public:
    void prepare (double s, int maxBlock)
    {
        sr = s;
        os = std::make_unique<juce::dsp::Oversampling<float>> (1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        os->initProcessing ((size_t) maxBlock);
        const double osr = s * 4.0;
        tight.set (Biquad::HP, s, 95.f, 0.6f);
        push.set (Biquad::Peak, s, 720.f, 0.8f, 6.f);
        inter1.set (Biquad::HP, osr, 140.f, 0.6f);
        inter2.set (Biquad::LP, osr, 6500.f, 0.6f);
        inter3.set (Biquad::LP, osr, 5200.f, 0.6f);
        post.set (Biquad::LP, s, 7500.f, 0.6f);
        dc.prepare (s);
        reset();
    }
    void reset()            { if (os) os->reset(); tight.reset(); push.reset(); inter1.reset(); inter2.reset(); inter3.reset(); post.reset(); dc.reset(); }
    int latency() const     { return os ? (int) std::round (os->getLatencyInSamples()) : 0; }

    void process (float* x, int n, float gain01)
    {
        const float driveDb = -4.f + gain01 * 50.f;           // 0..1 -> -4..+46 dB
        const float g1 = dbToGain (driveDb * 0.55f), g2 = dbToGain (driveDb * 0.45f + 6.f);
        for (int i = 0; i < n; ++i) x[i] = push.process (tight.process (x[i]));
        float* chans[1] = { x };
        juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
        auto up = os->processSamplesUp (block);
        float* u = up.getChannelPointer (0);
        const int m = (int) up.getNumSamples();
        for (int i = 0; i < m; ++i)
        {
            float v = u[i] * g1;
            v = fastTanh (v + 0.18f) - 0.1781f;               // stage 1, asymmetric
            v = inter1.process (v) * g2;
            v = fastTanh (v - 0.12f) + 0.1194f;               // stage 2
            v = inter2.process (v);
            v = softClip (v * 2.2f) * 0.9f;                   // power stage
            u[i] = inter3.process (v);
        }
        os->processSamplesDown (block);
        for (int i = 0; i < n; ++i) x[i] = dc.process (post.process (x[i])) * 0.8f;
    }
private:
    double sr = 48000;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    Biquad tight, push, inter1, inter2, inter3, post;
    DcBlocker dc;
};

/** Passive-style tone stack + presence/depth + master, applied after the amp. */
class ToneStack
{
public:
    void prepare (double s)  { sr = s; for (auto& v : cache) v = -99.f; reset(); }
    void reset()             { bass.reset(); mid.reset(); treble.reset(); presence.reset(); depth.reset(); }
    void process (float* x, int n, float b, float m, float t, float p, float d, float master)
    {
        auto upd = [this] (int idx, float v) { if (std::abs (cache[idx] - v) < 1e-4f) return false; cache[idx] = v; return true; };
        if (upd (0, b)) bass.set (Biquad::LowShelf, sr, 120.f, 0.7f, (b - 5.f) * 2.4f);
        if (upd (1, m)) mid.set (Biquad::Peak, sr, 650.f, 0.7f, (m - 5.f) * 2.0f);
        if (upd (2, t)) treble.set (Biquad::HighShelf, sr, 3000.f, 0.7f, (t - 5.f) * 2.4f);
        if (upd (3, p)) presence.set (Biquad::HighShelf, sr, 5500.f, 0.8f, (p - 5.f) * 1.8f);
        if (upd (4, d)) depth.set (Biquad::Peak, sr, 90.f, 1.1f, (d - 5.f) * 1.8f);
        masterSm.set (dbToGain ((master - 5.f) * 3.f));
        for (int i = 0; i < n; ++i)
            x[i] = depth.process (presence.process (treble.process (mid.process (bass.process (x[i]))))) * masterSm.next();
    }
    void initSmoother (double s) { masterSm.reset (s, 30.f, 1.f); }
private:
    double sr = 48000;
    Biquad bass, mid, treble, presence, depth;
    float cache[5] {};
    Smoother masterSm;
};

class AmpSection
{
public:
    enum Type { None = 0, Core = 1, Nam = 2 };
    static constexpr int kChannels = 4;

    void prepare (double s, int maxBlock)
    {
        sr = s; block = maxBlock;
        core.prepare (s, maxBlock);
        tone.prepare (s); tone.initSmoother (s);
        inGain.reset (s, 30.f, 1.f);
        fade.reset (s, 12.f, 1.f);
        scratch.assign ((size_t) maxBlock, 0.f);
    }
    void reset()             { core.reset(); tone.reset(); }

    /** Message thread. */
    void setType (Type t)    { type.store ((int) t); }
    Type getType() const     { return (Type) type.load(); }
    RtSlot<NamEngine>& channelSlot (int ch) { return captures[juce::jlimit (0, kChannels - 1, ch)]; }

    /** Audio thread. gain..master in 0..10 knob units. */
    void process (float* x, int n, bool enabled, int channel, float gain, float bass, float mid, float treble, float presence, float depth, float master)
    {
        const int t = enabled ? type.load() : (int) None;
        channel = juce::jlimit (0, kChannels - 1, channel);
        NamEngine* eng = t == Nam ? captures[channel].get() : nullptr;

        // Crossfade (dip) whenever the processing path changes: type, channel or capture.
        const void* path = t == Nam ? (const void*) eng : (const void*) (intptr_t) (t + 1);
        if (path != lastPath) { lastPath = path; fade.snap (0.f); fade.set (1.f); }

        if (t == None)
        {
            if (fade.isSmoothing()) for (int i = 0; i < n; ++i) x[i] *= fade.next();
            for (auto& c : captures) c.blockDone();
            return;
        }

        if (t == Nam)
        {
            if (eng == nullptr)
            {
                // Channel selected but no capture loaded yet: fall back to the built-in amp so you still hear an amp.
                core.process (x, n, gain * 0.1f);
            }
            else
            {
                inGain.set (dbToGain ((gain - 5.f) * 4.f));      // 0..10 -> -20..+20 dB into the capture
                for (int i = 0; i < n; ++i) x[i] *= inGain.next();
                eng->process (x, n);
            }
        }
        else core.process (x, n, gain * 0.1f);

        tone.process (x, n, bass, mid, treble, presence, depth, master);
        if (fade.isSmoothing()) for (int i = 0; i < n; ++i) x[i] *= fade.next();
        for (auto& c : captures) c.blockDone();
    }

    void collectGarbage (bool force)  { for (auto& c : captures) c.collectGarbage (force); }
    int latency() const
    {
        const int t = type.load();
        if (t == Core) return core.latency();
        return 0;   // NAM resampler latency is small and varies per capture; reported separately
    }
    int namLatency (int channel)      { auto* e = captures[juce::jlimit (0, kChannels - 1, channel)].get(); return e ? e->latencySamples() : 0; }
    double sampleRate() const         { return sr; }
    int maxBlock() const              { return block; }

private:
    double sr = 48000; int block = 512;
    std::atomic<int> type { (int) Core };
    RtSlot<NamEngine> captures[kChannels];
    CoreAmp core;
    ToneStack tone;
    Smoother inGain, fade;
    const void* lastPath = nullptr;
    std::vector<float> scratch;
};
} // namespace obs::dsp
