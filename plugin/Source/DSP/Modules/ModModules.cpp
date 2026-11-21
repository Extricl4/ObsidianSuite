// =============================================================================
//  Modulation & filter effects: chorus, flanger, phaser, tremolo, vibrato /
//  rotary, ring mod, bitcrusher, envelope filter, sweep filter, limiter.
// =============================================================================
#include "Module.h"

namespace obs::dsp
{
namespace
{
inline float syncedRate (float hz, bool sync, int div, double bpm)
{
    if (! sync) return hz;
    return (float) (juce::jlimit (20.0, 400.0, bpm) / 60.0 / NoteDivisions::beats (div));
}

// ---------------------------------------------------------------------------
class Chorus : public Module   // "Tide"
{
public:
    void prepare (const ModuleContext& c) override
    {
        sr = c.sampleRate;
        for (auto& l : line) l.prepare ((int) (0.05 * sr));
        for (int v = 0; v < 3; ++v) { lfo[v].setSampleRate (sr); lfo[v].setPhase (v / 3.f); }
        reset();
    }
    void reset() override { for (auto& l : line) l.clear(); for (auto& f : tone) f.reset(); toneCache = -1; }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: rate Hz, depth %, mix %, tone Hz, voices
        for (auto& l : lfo) l.setRate (p[0]);
        if (p[3] != toneCache) { toneCache = p[3]; for (auto& f : tone) f.set (Biquad::LP, sr, p[3], 0.6f); }
        const float depth = p[1] * 0.01f * 0.004f * (float) sr, base = 0.012f * (float) sr, mix = p[2] * 0.01f;
        const int voices = juce::jlimit (1, 3, (int) p[4] + 1);
        for (int i = 0; i < n; ++i)
        {
            const float inL = ch[0][i], inR = numCh > 1 ? ch[1][i] : inL;
            line[0].push (inL); line[1].push (inR);
            float wl = 0, wr = 0;
            for (int v = 0; v < voices; ++v)
            {
                const float m = lfo[v].next();
                wl += line[0].readCubic (base + depth * (0.5f + 0.5f * m));
                wr += line[1].readCubic (base + depth * (0.5f - 0.5f * m));
            }
            wl = tone[0].process (wl / (float) voices); wr = tone[1].process (wr / (float) voices);
            ch[0][i] = inL * (1.f - mix * 0.5f) + wl * mix;
            if (numCh > 1) ch[1][i] = inR * (1.f - mix * 0.5f) + wr * mix;
            else ch[0][i] = inL * (1.f - mix * 0.5f) + 0.5f * (wl + wr) * mix;
        }
    }
private:
    double sr = 48000; DelayLine line[2]; Lfo lfo[3]; Biquad tone[2]; float toneCache = -1;
};

// ---------------------------------------------------------------------------
class Flanger : public Module   // "Vortex"
{
public:
    void prepare (const ModuleContext& c) override { sr = c.sampleRate; for (auto& l : line) l.prepare ((int) (0.02 * sr)); lfo.setSampleRate (sr); reset(); }
    void reset() override { for (auto& l : line) l.clear(); fbL = fbR = 0; }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext& ctx) override
    {
        // p: rate Hz, depth %, feedback %, manual ms, mix %, sync, division
        lfo.setRate (syncedRate (p[0], p[5] > 0.5f, (int) p[6], ctx.bpm));
        const float depth = p[1] * 0.01f, fb = p[2] * 0.01f * 0.95f, manual = p[3] * 0.001f * (float) sr, mix = p[4] * 0.01f;
        for (int i = 0; i < n; ++i)
        {
            const float m = lfo.next (Lfo::Triangle);
            const float d = juce::jmax (1.f, manual * (1.f + depth * m * 0.95f));
            const float d2 = juce::jmax (1.f, manual * (1.f - depth * m * 0.95f));
            const float inL = ch[0][i], inR = numCh > 1 ? ch[1][i] : inL;
            const float yL = line[0].readCubic (d), yR = line[1].readCubic (d2);
            line[0].push (inL + fbL * fb); line[1].push (inR + fbR * fb);
            fbL = fastTanh (yL); fbR = fastTanh (yR);
            ch[0][i] = inL * (1.f - mix * 0.5f) + yL * mix;
            if (numCh > 1) ch[1][i] = inR * (1.f - mix * 0.5f) + yR * mix;
        }
    }
private:
    double sr = 48000; DelayLine line[2]; Lfo lfo; float fbL = 0, fbR = 0;
};

// ---------------------------------------------------------------------------
class Phaser : public Module   // "Prism"
{
public:
    void prepare (const ModuleContext& c) override { sr = c.sampleRate; lfo.setSampleRate (sr); reset(); }
    void reset() override { for (auto& s : z) for (auto& v : s) v = 0; fb[0] = fb[1] = 0; }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext& ctx) override
    {
        // p: rate Hz, depth %, feedback %, stages, mix %, sync, division
        static const int stageCounts[] = { 4, 6, 8, 12 };
        const int stages = stageCounts[juce::jlimit (0, 3, (int) p[3])];
        lfo.setRate (syncedRate (p[0], p[5] > 0.5f, (int) p[6], ctx.bpm));
        const float depth = p[1] * 0.01f, fbk = p[2] * 0.01f * 0.9f, mix = p[4] * 0.01f;
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f + 0.5f * lfo.next();
            for (int c = 0; c < numCh; ++c)
            {
                const float mm = c == 0 ? m : 1.f - m * 0.85f;
                const float f = 200.f * std::pow (20.f, mm * depth + (1.f - depth) * 0.3f);
                const float t = std::tan (kPi * clampf (f, 20.f, (float) sr * 0.45f) / (float) sr);
                const float a = (t - 1.f) / (t + 1.f);
                float x = ch[c][i] + fb[c] * fbk;
                for (int s = 0; s < stages; ++s) { const float y = a * x + z[c][s]; z[c][s] = x - a * y; x = y; }
                fb[c] = x;
                ch[c][i] = ch[c][i] * (1.f - mix * 0.5f) + x * mix;
            }
        }
    }
private:
    double sr = 48000; Lfo lfo; float z[2][12] {}; float fb[2] {};
};

// ---------------------------------------------------------------------------
class Tremolo : public Module   // "Pulse"
{
public:
    void prepare (const ModuleContext& c) override { sr = c.sampleRate; lfo.setSampleRate (sr); smooth.reset(); smooth.setLowpass (sr, 200.f); }
    void reset() override { smooth.reset (1.f); }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext& ctx) override
    {
        // p: rate Hz, depth %, shape, stereo (pan) toggle, sync, division
        lfo.setRate (syncedRate (p[0], p[4] > 0.5f, (int) p[5], ctx.bpm));
        const float depth = p[1] * 0.01f; const int shape = (int) p[2]; const bool pan = p[3] > 0.5f;
        const Lfo::Shape sh = shape == 0 ? Lfo::Sine : shape == 1 ? Lfo::Triangle : Lfo::Square;
        for (int i = 0; i < n; ++i)
        {
            const float m = smooth.lp (lfo.next (sh));
            const float gL = 1.f - depth * (0.5f + 0.5f * m);
            const float gR = pan ? 1.f - depth * (0.5f - 0.5f * m) : gL;
            ch[0][i] *= gL; if (numCh > 1) ch[1][i] *= gR;
        }
    }
private:
    double sr = 48000; Lfo lfo; OnePole smooth;
};

// ---------------------------------------------------------------------------
class Vibrato : public Module   // "Spectre": vibrato, rotary speaker or uni-vibe
{
public:
    void prepare (const ModuleContext& c) override
    {
        sr = c.sampleRate; for (auto& l : line) l.prepare ((int) (0.02 * sr));
        lfo.setSampleRate (sr); horn.setSampleRate (sr); drum.setSampleRate (sr); speedSm.reset (sr, 1500.f, 1.f);
        reset();
    }
    void reset() override { for (auto& l : line) l.clear(); for (auto& s : z) for (auto& v : s) v = 0; }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: mode, rate Hz, depth %, mix %, fast (rotary toggle)
        const int mode = (int) p[0]; const float rate = p[1], depth = p[2] * 0.01f, mix = p[3] * 0.01f;
        speedSm.set (p[4] > 0.5f ? 6.5f : 0.8f);
        for (int i = 0; i < n; ++i)
        {
            const float inL = ch[0][i], inR = numCh > 1 ? ch[1][i] : inL;
            line[0].push (inL); line[1].push (inR);
            float oL, oR;
            if (mode == 0)
            {   // vibrato: pure pitch wobble
                lfo.setRate (rate);
                const float m = lfo.next();
                const float d = (0.004f + 0.003f * depth * m) * (float) sr;
                oL = line[0].readCubic (d); oR = line[1].readCubic (d);
            }
            else if (mode == 1)
            {   // rotary: horn + drum with doppler and amplitude modulation
                const float sp = speedSm.next();
                horn.setRate (sp); drum.setRate (sp * 0.85f);
                const float h = horn.next(), dr = drum.next();
                const float dh = (0.003f + 0.0012f * depth * h) * (float) sr;
                oL = line[0].readCubic (dh) * (1.f - 0.35f * depth * (0.5f + 0.5f * h)) + inL * 0.5f * (1.f - 0.3f * depth * (0.5f + 0.5f * dr));
                oR = line[1].readCubic (dh) * (1.f - 0.35f * depth * (0.5f - 0.5f * h)) + inR * 0.5f * (1.f - 0.3f * depth * (0.5f - 0.5f * dr));
            }
            else
            {   // uni-vibe: 4 allpass stages swept by a lopsided LFO
                lfo.setRate (rate);
                const float m = lfo.next();
                const float lop = 0.5f + 0.5f * (m > 0 ? m * m : -m * 0.6f);
                const float f = 300.f + 2800.f * lop * depth;
                const float t = std::tan (kPi * f / (float) sr), a = (t - 1.f) / (t + 1.f);
                float x[2] = { inL, inR };
                for (int c = 0; c < 2; ++c) for (int s = 0; s < 4; ++s) { const float y = a * x[c] + z[c][s]; z[c][s] = x[c] - a * y; x[c] = y; }
                oL = 0.5f * (inL + x[0]); oR = 0.5f * (inR + x[1]);
            }
            ch[0][i] = inL * (1.f - mix) + oL * mix;
            if (numCh > 1) ch[1][i] = inR * (1.f - mix) + oR * mix;
        }
    }
private:
    double sr = 48000; DelayLine line[2]; Lfo lfo, horn, drum; float z[2][4] {}; Smoother speedSm;
};

// ---------------------------------------------------------------------------
class RingMod : public Module   // "Tesla"
{
public:
    void prepare (const ModuleContext& c) override { sr = c.sampleRate; lfo.setSampleRate (sr); reset(); }
    void reset() override { ph = 0; }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: frequency Hz, mix %, lfo rate Hz, lfo depth %, mode (ring / AM)
        lfo.setRate (p[2]);
        const float mix = p[1] * 0.01f, ld = p[3] * 0.01f; const bool am = (int) p[4] == 1;
        for (int i = 0; i < n; ++i)
        {
            const float f = p[0] * std::pow (2.f, lfo.next() * ld * 2.f);
            ph += f / (float) sr; if (ph >= 1.f) ph -= 1.f;
            const float car = std::sin (kTwoPi * ph);
            const float m = am ? 0.5f + 0.5f * car : car;
            for (int c = 0; c < numCh; ++c) ch[c][i] = ch[c][i] * (1.f - mix) + ch[c][i] * m * mix;
        }
    }
private:
    double sr = 48000; Lfo lfo; float ph = 0;
};

// ---------------------------------------------------------------------------
class Bitcrusher : public Module   // "Hex"
{
public:
    void prepare (const ModuleContext& c) override { sr = c.sampleRate; reset(); }
    void reset() override { hold[0] = hold[1] = 0; cnt = 0; for (auto& f : tone) f.reset(); toneCache = -1; }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: bits, downsample x, mix %, tone Hz
        const float levels = std::pow (2.f, juce::jlimit (1.f, 16.f, p[0]) - 1.f);
        const float ds = juce::jmax (1.f, p[1]), mix = p[2] * 0.01f;
        if (p[3] != toneCache) { toneCache = p[3]; for (auto& f : tone) f.set (Biquad::LP, sr, p[3], 0.7f); }
        for (int i = 0; i < n; ++i)
        {
            cnt += 1.f;
            const bool take = cnt >= ds;
            if (take) cnt -= ds;
            for (int c = 0; c < numCh; ++c)
            {
                if (take) hold[c] = std::round (clampf (ch[c][i], -1.f, 1.f) * levels) / levels;
                ch[c][i] = ch[c][i] * (1.f - mix) + tone[c].process (hold[c]) * mix;
            }
        }
    }
private:
    double sr = 48000; float hold[2] {}, cnt = 0, toneCache = -1; Biquad tone[2];
};

// ---------------------------------------------------------------------------
class EnvelopeFilter : public Module   // "Bloom" (auto-wah / quack)
{
public:
    void prepare (const ModuleContext& c) override { sr = c.sampleRate; for (auto& f : svf) f.setSampleRate (sr); env.set (sr, 3.f, 140.f); reset(); }
    void reset() override { for (auto& f : svf) f.reset(); env.reset(); }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: sensitivity, range, Q, type, direction, mix %
        const float sens = 1.f + p[0] * 1.5f, range = p[1], q = p[2]; const int type = (int) p[3]; const bool down = (int) p[4] == 1;
        const float mix = p[5] * 0.01f;
        for (int i = 0; i < n; ++i)
        {
            float a = 0; for (int c = 0; c < numCh; ++c) a = std::max (a, std::abs (ch[c][i]));
            float e = clampf (env.process (a) * sens, 0.f, 1.f);
            if (down) e = 1.f - e;
            if ((i & 15) == 0) { const float f = 200.f * std::pow (2.f, e * (1.f + range * 0.5f)); for (auto& s : svf) s.set (f, q); }
            for (int c = 0; c < numCh; ++c)
            {
                svf[c].tick (ch[c][i]);
                const float y = type == 0 ? svf[c].low : type == 1 ? svf[c].band * 1.8f : svf[c].high;
                ch[c][i] = ch[c][i] * (1.f - mix) + y * mix;
            }
        }
    }
private:
    double sr = 48000; Svf svf[2]; EnvFollower env;
};

// ---------------------------------------------------------------------------
class SweepFilter : public Module   // special FX filter: LFO / manual sweep
{
public:
    void prepare (const ModuleContext& c) override { sr = c.sampleRate; for (auto& f : svf) f.setSampleRate (sr); lfo.setSampleRate (sr); cutSm.reset (sr, 40.f, 1000.f); reset(); }
    void reset() override { for (auto& f : svf) f.reset(); }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext& ctx) override
    {
        // p: cutoff Hz, resonance, type, lfo rate, lfo depth %, sync, division, mix %
        lfo.setRate (syncedRate (p[3], p[5] > 0.5f, (int) p[6], ctx.bpm));
        cutSm.set (p[0]);
        const float q = 0.5f + p[1] * 1.5f, depth = p[4] * 0.01f, mix = p[7] * 0.01f; const int type = (int) p[2];
        for (int i = 0; i < n; ++i)
        {
            const float m = lfo.next();
            const float fc = cutSm.next();
            if ((i & 15) == 0) { const float f = clampf (fc * std::pow (2.f, m * depth * 3.f), 30.f, 18000.f); for (auto& s : svf) s.set (f, q); }
            for (int c = 0; c < numCh; ++c)
            {
                svf[c].tick (ch[c][i]);
                const float y = type == 0 ? svf[c].low : type == 1 ? svf[c].band : svf[c].high;
                ch[c][i] = ch[c][i] * (1.f - mix) + y * mix;
            }
        }
    }
private:
    double sr = 48000; Svf svf[2]; Lfo lfo; Smoother cutSm;
};

// ---------------------------------------------------------------------------
class Limiter : public Module   // "dynamics": brickwall-ish safety limiter with ceiling
{
public:
    void prepare (const ModuleContext& c) override { sr = c.sampleRate; reset(); }
    void reset() override { gain = 1.f; }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: input gain dB, ceiling dB, release ms
        const float in = dbToGain (p[0]), ceil = dbToGain (p[1]);
        const float rel = std::exp (-1.f / (float) (p[2] * 0.001 * sr + 1));
        for (int i = 0; i < n; ++i)
        {
            float pk = 0; for (int c = 0; c < numCh; ++c) pk = std::max (pk, std::abs (ch[c][i] * in));
            const float target = pk > ceil ? ceil / pk : 1.f;
            gain = target < gain ? target : target + (gain - target) * rel;
            for (int c = 0; c < numCh; ++c) ch[c][i] = clampf (ch[c][i] * in * gain, -ceil, ceil);
        }
    }
private:
    double sr = 48000; float gain = 1.f;
};
} // namespace

void registerModModules (std::vector<ModuleType>& t)
{
    using P = ModuleParam;
    const auto divs = NoteDivisions::names();
    P tog { "Sync", "", 0, 1, 0 }; tog.toggle = true;
    auto toggle = [tog] (const char* name, float def) { P x = tog; x.name = name; x.def = def; return x; };

    t.push_back ({ "tide", "Tide Chorus", "modulation", RackAll,
        "Lush stereo chorus, from subtle shimmer to thick 80s.",
        { P { "Rate", "Hz", 0.05f, 8.f, 0.8f, 2.f }, P { "Depth", "%", 0, 100, 45 }, P { "Mix", "%", 0, 100, 50 }, P { "Tone", "Hz", 1500, 16000, 9000, 2.f },
          P { "Voices", "", 0, 2, 1, 1, { "1", "2", "3" } } },
        [] { return std::make_unique<Chorus>(); } });
    t.push_back ({ "vortex", "Vortex Flanger", "modulation", RackAll,
        "Jet-plane flanger with feedback.",
        { P { "Rate", "Hz", 0.02f, 6.f, 0.25f, 2.f }, P { "Depth", "%", 0, 100, 70 }, P { "Feedback", "%", 0, 100, 55 }, P { "Manual", "ms", 0.3f, 8.f, 2.5f },
          P { "Mix", "%", 0, 100, 50 }, toggle ("Sync", 0), P { "Division", "", 0, 12, 3, 1, divs } },
        [] { return std::make_unique<Flanger>(); } });
    t.push_back ({ "prism", "Prism Phaser", "modulation", RackAll,
        "Swirling 4 to 12-stage phaser.",
        { P { "Rate", "Hz", 0.02f, 8.f, 0.5f, 2.f }, P { "Depth", "%", 0, 100, 75 }, P { "Feedback", "%", 0, 100, 40 }, P { "Stages", "", 0, 3, 0, 1, { "4", "6", "8", "12" } },
          P { "Mix", "%", 0, 100, 50 }, toggle ("Sync", 0), P { "Division", "", 0, 12, 3, 1, divs } },
        [] { return std::make_unique<Phaser>(); } });
    t.push_back ({ "pulse", "Pulse Tremolo", "modulation", RackAll,
        "Volume tremolo: smooth, triangle or choppy square. Stereo mode pans it.",
        { P { "Rate", "Hz", 0.1f, 15.f, 4.f, 2.f }, P { "Depth", "%", 0, 100, 60 }, P { "Shape", "", 0, 2, 0, 1, { "Sine", "Triangle", "Square" } }, toggle ("Stereo", 0),
          toggle ("Sync", 0), P { "Division", "", 0, 12, 6, 1, divs } },
        [] { return std::make_unique<Tremolo>(); } });
    t.push_back ({ "spectre", "Spectre Mod", "modulation", RackAll,
        "Vibrato, rotary speaker or uni-vibe.",
        { P { "Mode", "", 0, 2, 1, 1, { "Vibrato", "Rotary", "Uni-Vibe" } }, P { "Rate", "Hz", 0.1f, 10.f, 3.f, 2.f }, P { "Depth", "%", 0, 100, 60 }, P { "Mix", "%", 0, 100, 100 }, toggle ("Fast", 0) },
        [] { return std::make_unique<Vibrato>(); } });
    t.push_back ({ "tesla", "Tesla Ring Mod", "modulation", RackPost | RackSfx,
        "Metallic, bell-like ring modulation with a wobbling carrier.",
        { P { "Frequency", "Hz", 20, 2000, 320, 2.f }, P { "Mix", "%", 0, 100, 60 }, P { "LFO Rate", "Hz", 0.05f, 10.f, 0.5f, 2.f }, P { "LFO Depth", "%", 0, 100, 0 }, P { "Mode", "", 0, 1, 0, 1, { "Ring", "AM" } } },
        [] { return std::make_unique<RingMod>(); } });
    t.push_back ({ "hex", "Hex Bitcrusher", "glitch", RackPost | RackSfx,
        "Lo-fi bit and sample-rate reduction.",
        { P { "Bits", "", 1, 16, 8 }, P { "Downsample", "x", 1, 40, 4, 2.f }, P { "Mix", "%", 0, 100, 60 }, P { "Tone", "Hz", 1000, 20000, 12000, 2.f } },
        [] { return std::make_unique<Bitcrusher>(); } });
    t.push_back ({ "bloom", "Bloom Envelope Filter", "filter", RackAll,
        "Auto-wah that opens with your picking: funky quack to synth sweeps.",
        { P { "Sensitivity", "", 0, 10, 6 }, P { "Range", "", 0, 10, 6 }, P { "Q", "", 0.7f, 12, 4 }, P { "Type", "", 0, 2, 1, 1, { "Low Pass", "Band Pass", "High Pass" } },
          P { "Direction", "", 0, 1, 0, 1, { "Up", "Down" } }, P { "Mix", "%", 0, 100, 100 } },
        [] { return std::make_unique<EnvelopeFilter>(); } });
    t.push_back ({ "filter", "Sweep Filter", "filter", RackAll,
        "Resonant filter with an LFO, free or synced to the tempo.",
        { P { "Cutoff", "Hz", 40, 16000, 1200, 2.5f }, P { "Resonance", "", 0, 10, 4 }, P { "Type", "", 0, 2, 0, 1, { "Low Pass", "Band Pass", "High Pass" } },
          P { "LFO Rate", "Hz", 0.05f, 12.f, 0.5f, 2.f }, P { "LFO Depth", "%", 0, 100, 50 }, toggle ("Sync", 0), P { "Division", "", 0, 12, 3, 1, divs }, P { "Mix", "%", 0, 100, 100 } },
        [] { return std::make_unique<SweepFilter>(); } });
    t.push_back ({ "limiter", "Limiter", "dynamics", RackPost | RackSfx,
        "Catches peaks so nothing ever clips. Put it last.",
        { P { "Input", "dB", -12, 24, 0 }, P { "Ceiling", "dB", -24, 0, -1 }, P { "Release", "ms", 5, 500, 80, 2.f } },
        [] { return std::make_unique<Limiter>(); } });
}
} // namespace obs::dsp
