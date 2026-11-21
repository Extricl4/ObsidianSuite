#pragma once
// =============================================================================
//  AmpSection: the AMP tab.
//   * "model": one of the in-house amps in AmpModels.h (modelled circuit by
//              circuit; no files needed)
//   * "nam"  : NAM captures, one per channel (up to 4), switched instantly
//   * "none" : no amp (signal passes straight to the cab)
//  For models, the knobs are the amp's own controls (its real tone stack,
//  presence/depth in the power amp, master into the power tubes). For captures,
//  Gain drives the capture's input and the other knobs are an EQ after it.
//  Every switch crossfades, so there are no clicks.
// =============================================================================
#include "DspCore.h"
#include "NamEngine.h"
#include "AmpModels.h"
#include <juce_dsp/juce_dsp.h>

namespace obs::dsp
{
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
    enum Type { None = 0, Model = 1, Nam = 2 };
    static constexpr int kChannels = 4;

    void prepare (double s, int maxBlock)
    {
        sr = s; block = maxBlock;
        model.prepare (s, maxBlock);
        if (model.model() == nullptr) model.setModel (&amp::models()[(size_t) juce::jlimit (0, (int) amp::models().size() - 1, modelIndex.load())]);
        tone.prepare (s); tone.initSmoother (s);
        inGain.reset (s, 30.f, 1.f);
        fade.reset (s, 12.f, 1.f);
        scratch.assign ((size_t) maxBlock, 0.f);
    }
    void reset()             { model.reset(); tone.reset(); }

    /** Message thread. */
    void setType (Type t)    { type.store ((int) t); }
    Type getType() const     { return (Type) type.load(); }
    /** Message thread: which in-house amp (index into amp::models()). */
    void setModel (int index) { modelIndex.store (juce::jlimit (0, (int) amp::models().size() - 1, index)); }
    RtSlot<NamEngine>& channelSlot (int ch) { return captures[juce::jlimit (0, kChannels - 1, ch)]; }

    /** Audio thread. gain..master in 0..10 knob units. */
    void process (float* x, int n, bool enabled, int channel, float gain, float bass, float mid, float treble, float presence, float depth, float master)
    {
        const int t = enabled ? type.load() : (int) None;
        channel = juce::jlimit (0, kChannels - 1, channel);
        NamEngine* eng = t == Nam ? captures[channel].get() : nullptr;

        // The in-house amp changed? Switch it here, on the audio thread (no allocation).
        const auto* wanted = &amp::models()[(size_t) modelIndex.load()];
        if (t == Model && model.model() != wanted) model.setModel (wanted);
        // Crossfade (dip) whenever the processing path changes: type, model, channel or capture.
        const void* path = t == Nam ? (const void*) eng : t == Model ? (const void*) wanted : (const void*) (intptr_t) (t + 1);
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
                // Channel selected but no capture loaded yet: play the in-house amp so you still hear an amp.
                if (model.model() != wanted) model.setModel (wanted);
                model.process (x, n, gain, bass, mid, treble, presence, depth, master);
            }
            else
            {
                inGain.set (dbToGain ((gain - 5.f) * 4.f));      // 0..10 -> -20..+20 dB into the capture
                for (int i = 0; i < n; ++i) x[i] *= inGain.next();
                eng->process (x, n);
                tone.process (x, n, bass, mid, treble, presence, depth, master);
            }
        }
        else model.process (x, n, gain, bass, mid, treble, presence, depth, master);

        if (fade.isSmoothing()) for (int i = 0; i < n; ++i) x[i] *= fade.next();
        for (auto& c : captures) c.blockDone();
    }

    void collectGarbage (bool force)  { for (auto& c : captures) c.collectGarbage (force); }
    int latency() const
    {
        const int t = type.load();
        if (t == Model) return model.latency();
        return 0;   // NAM resampler latency is small and varies per capture; reported separately
    }
    int namLatency (int channel)      { auto* e = captures[juce::jlimit (0, kChannels - 1, channel)].get(); return e ? e->latencySamples() : 0; }
    double sampleRate() const         { return sr; }
    int maxBlock() const              { return block; }

private:
    double sr = 48000; int block = 512;
    std::atomic<int> type { (int) Model };
    std::atomic<int> modelIndex { amp::findModel ("core") >= 0 ? amp::findModel ("core") : 0 };
    RtSlot<NamEngine> captures[kChannels];
    amp::AmpModel model;
    ToneStack tone;
    Smoother inGain, fade;
    const void* lastPath = nullptr;
    std::vector<float> scratch;
};
} // namespace obs::dsp
