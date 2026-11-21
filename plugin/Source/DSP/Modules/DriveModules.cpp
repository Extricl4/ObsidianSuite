// =============================================================================
//  Drive, boost, compressor, wah and NAM pedal captures (PEDALS tab).
// =============================================================================
#include "Module.h"
#include "../NamEngine.h"
#include <juce_dsp/juce_dsp.h>

namespace obs::dsp
{
namespace
{
/** Shared oversampling wrapper for nonlinear stages (factor follows Settings > Audio). */
struct Oversampler
{
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    int factor = 1;
    void prepare (int channels, int maxBlock, int oversampling)
    {
        factor = oversampling >= 4 ? 2 : (oversampling >= 2 ? 1 : 0);   // log2
        os = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) channels, (size_t) factor,
                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        os->initProcessing ((size_t) maxBlock);
    }
    template <typename Fn>
    void run (float* const* ch, int numCh, int n, Fn&& fn)
    {
        if (factor == 0) { for (int c = 0; c < numCh; ++c) for (int i = 0; i < n; ++i) ch[c][i] = fn (c, ch[c][i]); return; }
        juce::dsp::AudioBlock<float> blk (ch, (size_t) numCh, (size_t) n);
        auto up = os->processSamplesUp (blk);
        for (int c = 0; c < numCh; ++c) { float* u = up.getChannelPointer ((size_t) c); for (size_t i = 0; i < up.getNumSamples(); ++i) u[i] = fn (c, u[i]); }
        os->processSamplesDown (blk);
    }
    void reset() { if (os) os->reset(); }
    int latency() const { return os && factor > 0 ? (int) std::round (os->getLatencyInSamples()) : 0; }
};

// ---------------------------------------------------------------------------
class Overdrive : public Module
{
public:
    void prepare (const ModuleContext& c) override
    {
        sr = c.sampleRate; os.prepare (2, c.maxBlock, c.oversampling);
        for (int k = 0; k < 2; ++k) { pre[k].set (Biquad::HP, sr, 720.f, 0.5f); hump[k].set (Biquad::Peak, sr, 900.f, 0.9f, 4.f); dc[k].prepare (sr); dry[k].assign ((size_t) c.maxBlock, 0.f); }
        reset();
    }
    void reset() override { os.reset(); for (int k = 0; k < 2; ++k) { pre[k].reset(); hump[k].reset(); tone[k].reset(); dc[k].reset(); } }
    int latency() const override { return os.latency(); }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        const float drive = p[0], level = p[2], mix = p[3] * 0.01f;
        if (p[1] != toneCache) { toneCache = p[1]; for (auto& t : tone) t.set (Biquad::LP, sr, 800.f * std::pow (8.f, p[1] / 10.f), 0.7f); }
        const float g = dbToGain (drive * 4.f + 6.f);
        const int m = juce::jmin (n, (int) dry[0].size());
        for (int c = 0; c < numCh; ++c)
        {
            std::copy (ch[c], ch[c] + m, dry[c].data());
            for (int i = 0; i < n; ++i) { const float x = ch[c][i]; ch[c][i] = x * 0.45f + hump[c].process (pre[c].process (x)); }
        }
        os.run (ch, numCh, n, [g] (int, float x) { const float v = x * g; return v >= 0 ? fastTanh (v) : fastTanh (v * 0.8f) * 1.15f; });
        const float out = dbToGain (level);
        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float wet = dc[c].process (tone[c].process (ch[c][i])) * out * 0.7f;
                ch[c][i] = i < m ? dry[c][(size_t) i] * (1.f - mix) + wet * mix : wet;
            }
    }
private:
    double sr = 48000; Oversampler os; Biquad pre[2], hump[2], tone[2]; DcBlocker dc[2]; float toneCache = -1; std::vector<float> dry[2];
};

// ---------------------------------------------------------------------------
class Fuzz : public Module   // "Ember": two cascaded clipping stages and a mid-scoop tone
{
public:
    void prepare (const ModuleContext& c) override
    {
        sr = c.sampleRate; os.prepare (2, c.maxBlock, c.oversampling);
        for (int k = 0; k < 2; ++k) { inHp[k].set (Biquad::HP, sr, 60.f, 0.7f); dc[k].prepare (sr); gate[k].set (sr, 1.f, 80.f); }
        reset();
    }
    void reset() override { os.reset(); for (int k = 0; k < 2; ++k) { inHp[k].reset(); lp[k].reset(); hp[k].reset(); dc[k].reset(); gate[k].reset(); } }
    int latency() const override { return os.latency(); }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        const float fuzz = p[0], toneV = p[1], level = p[2], gateAmt = p[3];
        if (toneV != toneCache) { toneCache = toneV; for (int k = 0; k < 2; ++k) { lp[k].set (Biquad::LP, sr, 700.f, 0.6f); hp[k].set (Biquad::HP, sr, 1200.f, 0.6f); } }
        const float g = dbToGain (10.f + fuzz * 4.5f);
        for (int c = 0; c < numCh; ++c) for (int i = 0; i < n; ++i) ch[c][i] = inHp[c].process (ch[c][i]);
        os.run (ch, numCh, n, [g] (int, float x)
        {
            float v = fastTanh (x * g);
            v = fastTanh ((v + 0.25f) * 3.f) - fastTanh (0.75f);
            return v;
        });
        const float t = toneV / 10.f, out = dbToGain (level) * 0.6f;
        const float gThresh = dbToGain (-70.f + gateAmt * 4.f);
        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float x = dc[c].process (ch[c][i]);
                const float y = lp[c].process (x) * (1.f - t) + hp[c].process (x) * t;  // Big-Muff style tone blend
                const float e = gate[c].process (y);
                ch[c][i] = y * out * (gateAmt > 0.1f ? clampf ((e - gThresh) / (gThresh + 1e-6f) + 1.f, 0.f, 1.f) : 1.f);
            }
    }
private:
    double sr = 48000; Oversampler os; Biquad inHp[2], lp[2], hp[2]; DcBlocker dc[2]; EnvFollower gate[2]; float toneCache = -1;
};

// ---------------------------------------------------------------------------
class Boost : public Module
{
public:
    void prepare (const ModuleContext& c) override { sr = c.sampleRate; for (auto& v : cache) v = -999; gainSm.reset (sr, 30.f, 1.f); reset(); }
    void reset() override { for (int k = 0; k < 2; ++k) { tight[k].reset(); bass[k].reset(); treble[k].reset(); } }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        if (p[1] != cache[0]) { cache[0] = p[1]; for (auto& f : tight) f.set (Biquad::HP, sr, p[1], 0.7f); }
        if (p[2] != cache[1]) { cache[1] = p[2]; for (auto& f : bass) f.set (Biquad::LowShelf, sr, 150.f, 0.7f, p[2]); }
        if (p[3] != cache[2]) { cache[2] = p[3]; for (auto& f : treble) f.set (Biquad::HighShelf, sr, 2500.f, 0.7f, p[3]); }
        gainSm.set (dbToGain (p[0]));
        for (int i = 0; i < n; ++i)
        {
            const float g = gainSm.next();
            for (int c = 0; c < numCh; ++c)
                ch[c][i] = softClip (treble[c].process (bass[c].process (tight[c].process (ch[c][i]))) * g * 0.9f) * 1.1f;
        }
    }
private:
    double sr = 48000; Biquad tight[2], bass[2], treble[2]; float cache[3]; Smoother gainSm;
};

// ---------------------------------------------------------------------------
class Compressor : public Module   // used for the pedal compressor and the studio "dynamics" unit
{
public:
    explicit Compressor (bool studio) : isStudio (studio) {}
    void prepare (const ModuleContext& c) override { sr = c.sampleRate; reset(); }
    void reset() override { env = 0; gr = 1; }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: threshold dB, ratio, attack ms, release ms, makeup dB, mix %
        const float thr = p[0], ratio = juce::jmax (1.f, p[1]);
        const float at = std::exp (-1.f / (float) (p[2] * 0.001 * sr + 1)), rt = std::exp (-1.f / (float) (p[3] * 0.001 * sr + 1));
        const float makeup = dbToGain (p[4]), mix = p[5] * 0.01f;
        const float knee = isStudio ? 6.f : 10.f;
        for (int i = 0; i < n; ++i)
        {
            float peak = 0;
            for (int c = 0; c < numCh; ++c) peak = std::max (peak, std::abs (ch[c][i]));
            env = peak > env ? at * (env - peak) + peak : rt * (env - peak) + peak;
            const float lvl = gainToDb (env);
            float over = lvl - thr, red = 0;
            if (over > knee * 0.5f) red = over * (1.f - 1.f / ratio);
            else if (over > -knee * 0.5f) { const float x = over + knee * 0.5f; red = (1.f - 1.f / ratio) * x * x / (2.f * knee); }
            gr = dbToGain (-red);
            for (int c = 0; c < numCh; ++c) { const float d = ch[c][i]; ch[c][i] = d * (1.f - mix) + d * gr * makeup * mix; }
        }
    }
private:
    bool isStudio; double sr = 48000; float env = 0, gr = 1;
};

// ---------------------------------------------------------------------------
class Wah : public Module   // "Lament": manual, auto (LFO) or envelope controlled
{
public:
    void prepare (const ModuleContext& c) override { sr = c.sampleRate; for (auto& f : svf) f.setSampleRate (sr); lfo.setSampleRate (sr); env.set (sr, 4.f, 120.f); posSm.reset (sr, 20.f, 0.5f); reset(); }
    void reset() override { for (auto& f : svf) f.reset(); env.reset(); }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: position %, mode, rate Hz, sensitivity %, range, Q, mix %
        const int mode = (int) p[1];
        const float lo = 350.f, hi = 350.f + p[4] * 220.f;    // range 0..10 -> up to ~2.5 kHz
        const float q = p[5], mix = p[6] * 0.01f;
        lfo.setRate (p[2]);
        for (int i = 0; i < n; ++i)
        {
            float pos;
            if (mode == 1) pos = 0.5f + 0.5f * lfo.next();
            else if (mode == 2) { float a = 0; for (int c = 0; c < numCh; ++c) a = std::max (a, std::abs (ch[c][i])); pos = clampf (env.process (a) * (0.5f + p[3] * 0.4f), 0.f, 1.f); }
            else { posSm.set (p[0] * 0.01f); pos = posSm.next(); }
            if ((i & 15) == 0) { const float f = lo * std::pow (hi / lo, pos); for (auto& s : svf) s.set (f, q); }
            for (int c = 0; c < numCh; ++c)
            {
                svf[c].tick (ch[c][i]);
                ch[c][i] = ch[c][i] * (1.f - mix) + svf[c].band * 2.2f * mix;
            }
        }
    }
private:
    double sr = 48000; Svf svf[2]; Lfo lfo; EnvFollower env; Smoother posSm;
};

// ---------------------------------------------------------------------------
class NamPedal : public Module   // load a .nam capture of any drive / boost pedal
{
public:
    void prepare (const ModuleContext& c) override
    {
        sr = c.sampleRate; block = c.maxBlock;
        if (auto* e = slot.get()) e->prepare (sr, block);
        inSm.reset (sr, 30.f, 1.f); outSm.reset (sr, 30.f, 1.f);
        mono.assign ((size_t) c.maxBlock, 0.f);
    }
    void reset() override {}
    juce::String loadFile (const juce::File& f) override
    {
        juce::String err;
        auto e = NamEngine::load (f, sr, block, err);
        if (e == nullptr) return err;
        name = e->name();
        slot.set (std::move (e));
        slot.collectGarbage (false);
        return {};
    }
    juce::String fileName() const override { return name; }
    void setNamEngine (std::unique_ptr<NamEngine> e, const juce::String& nm) override
    {
        if (e == nullptr) return;
        e->prepare (sr, block);
        name = nm; slot.set (std::move (e)); slot.collectGarbage (false);
    }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        auto* e = slot.get();
        if (e != nullptr && n <= (int) mono.size())
        {
            inSm.set (dbToGain (p[0])); outSm.set (dbToGain (p[1]));
            for (int i = 0; i < n; ++i) mono[(size_t) i] = ch[0][i] * inSm.next();
            e->process (mono.data(), n);
            const float mix = p[2] * 0.01f;
            for (int i = 0; i < n; ++i)
            {
                const float o = mono[(size_t) i] * outSm.next();
                for (int c = 0; c < numCh; ++c) ch[c][i] = ch[c][i] * (1.f - mix) + o * mix;
            }
        }
        slot.blockDone();
    }
    ~NamPedal() override { slot.collectGarbage (true); }
private:
    double sr = 48000; int block = 512;
    RtSlot<NamEngine> slot; juce::String name;
    Smoother inSm, outSm; std::vector<float> mono;
};
} // namespace

void registerDriveModules (std::vector<ModuleType>& t)
{
    using P = ModuleParam;
    t.push_back ({ "overdrive", "Overdrive", "drive", RackPre | RackPost,
        "Tube-screamer style mid push. Great in front of a high-gain amp.",
        { P { "Drive", "", 0, 10, 3 }, P { "Tone", "", 0, 10, 6 }, P { "Level", "dB", -24, 12, 0 }, P { "Mix", "%", 0, 100, 100 } },
        [] { return std::make_unique<Overdrive>(); } });
    t.push_back ({ "ember", "Ember Fuzz", "drive", RackPre | RackPost,
        "Thick, saturated fuzz with a mid-scoop tone control.",
        { P { "Fuzz", "", 0, 10, 7 }, P { "Tone", "", 0, 10, 5 }, P { "Level", "dB", -24, 12, -6 }, P { "Gate", "", 0, 10, 0 } },
        [] { return std::make_unique<Fuzz>(); } });
    t.push_back ({ "boost", "Clean Boost", "boost", RackPre | RackPost,
        "Clean gain with a tight low cut. Pushes the amp without changing its character.",
        { P { "Gain", "dB", 0, 20, 8 }, P { "Tight", "Hz", 20, 400, 120, 2.f }, P { "Bass", "dB", -12, 12, 0 }, P { "Treble", "dB", -12, 12, 0 } },
        [] { return std::make_unique<Boost>(); } });
    t.push_back ({ "compressor", "Compressor", "compressor", RackPre | RackPost,
        "Evens out your picking and adds sustain.",
        { P { "Threshold", "dB", -50, 0, -24 }, P { "Ratio", "x", 1, 20, 4, 2.f }, P { "Attack", "ms", 0.1f, 100, 8, 2.f }, P { "Release", "ms", 20, 1000, 150, 2.f },
          P { "Makeup", "dB", 0, 24, 6 }, P { "Blend", "%", 0, 100, 100 } },
        [] { return std::make_unique<Compressor> (false); } });
    t.push_back ({ "glue", "Glue Compressor", "dynamics", RackPost | RackSfx,
        "Smooth bus compressor that glues the whole rig together.",
        { P { "Threshold", "dB", -40, 0, -18 }, P { "Ratio", "x", 1, 10, 2.5f, 2.f }, P { "Attack", "ms", 0.1f, 100, 20, 2.f }, P { "Release", "ms", 20, 1000, 200, 2.f },
          P { "Makeup", "dB", 0, 18, 2 }, P { "Mix", "%", 0, 100, 100 } },
        [] { return std::make_unique<Compressor> (true); } });
    t.push_back ({ "lament", "Lament Wah", "wah", RackPre | RackPost,
        "Wah: set it by hand, let it sweep by itself (Auto) or follow your picking (Envelope).",
        { P { "Position", "%", 0, 100, 50 }, P { "Mode", "", 0, 2, 0, 1, { "Manual", "Auto", "Envelope" } }, P { "Rate", "Hz", 0.1f, 8, 1.5f, 2.f },
          P { "Sensitivity", "", 0, 10, 5 }, P { "Range", "", 0, 10, 6 }, P { "Q", "", 1, 12, 5 }, P { "Mix", "%", 0, 100, 100 } },
        [] { return std::make_unique<Wah>(); } });
    t.push_back ({ "nampedal", "NAM Pedal", "drive", RackPre | RackPost,
        "Load a .nam capture of any drive, fuzz or boost pedal.",
        { P { "Input", "dB", -24, 24, 0 }, P { "Output", "dB", -24, 24, 0 }, P { "Mix", "%", 0, 100, 100 } },
        [] { return std::make_unique<NamPedal>(); }, "nam" });
}
} // namespace obs::dsp
