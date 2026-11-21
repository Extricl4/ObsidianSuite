// =============================================================================
//  Time effects: delay, reverb, shimmer, swell, freeze, tape stop, looper, glitch.
// =============================================================================
#include "Module.h"
#include "IrCore.h"
#include "../PitchShifter.h"

namespace obs::dsp
{
namespace
{
// ---------------------------------------------------------------------------
/** 8-line feedback delay network reverb with input diffusion, damping,
    gentle modulation and an optional pitch-shifted (shimmer) feedback path. */
class FdnReverb
{
public:
    static constexpr int N = 8;
    void prepare (double s)
    {
        sr = s;
        for (int i = 0; i < N; ++i) { lines[i].prepare ((int) (0.2 * s)); damp[i].reset(); }
        for (int i = 0; i < 4; ++i) diff[i].prepare ((int) (0.03 * s));
        pre.prepare ((int) (0.5 * s));
        lfoA.setSampleRate (s); lfoA.setRate (0.47f); lfoB.setSampleRate (s); lfoB.setRate (0.31f); lfoB.setPhase (0.25f);
        shimmer.prepare (s, 60.f); shimmer.setSemitones (12.f);
        lowCut[0].set (Biquad::HP, s, 120.f, 0.7f); lowCut[1].set (Biquad::HP, s, 120.f, 0.7f);
        clear();
    }
    void clear()
    {
        for (auto& l : lines) l.clear();
        for (auto& d : diff) d.clear();
        pre.clear(); shimmer.reset(); shimmer.setSemitones (shimmerSt);
        for (auto& f : lowCut) f.reset();
        fbShimmer = 0;
    }
    void set (float decayS, float size, float preMs, float dampHz, float lowCutHz, float mod, float shimmerAmt = 0.f, float shimmerSemis = 12.f)
    {
        static const float baseMs[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f };
        const float sz = 0.35f + clampf (size, 0.f, 1.f) * 1.3f;
        for (int i = 0; i < N; ++i)
        {
            len[i] = baseMs[i] * sz * 0.001f * (float) sr;
            fb[i] = std::pow (10.f, -3.f * len[i] / (juce::jmax (0.1f, decayS) * (float) sr));
            damp[i].setLowpass (sr, dampHz);
        }
        preDelay = preMs * 0.001f * (float) sr;
        modDepth = mod * 0.001f * (float) sr * 0.6f;
        shimmerAmount = shimmerAmt;
        if (shimmerSemis != shimmerSt) { shimmerSt = shimmerSemis; shimmer.setSemitones (shimmerSt); }
        if (lowCutHz != lcCache) { lcCache = lowCutHz; for (auto& f : lowCut) f.set (Biquad::HP, sr, lowCutHz, 0.7f); }
    }
    /** Processes one stereo frame into wet L/R. */
    inline void tick (float inL, float inR, float& outL, float& outR)
    {
        static const float diffMs[4] = { 4.77f, 3.59f, 12.73f, 9.31f };
        float x = 0.5f * (inL + inR) + fbShimmer * shimmerAmount;
        pre.push (x);
        x = preDelay > 1.f ? pre.readLinear (preDelay) : x;
        for (int i = 0; i < 4; ++i) x = diff[i].process (x, diffMs[i] * 0.001f * (float) sr, 0.62f);

        const float mA = lfoA.next() * modDepth, mB = lfoB.next() * modDepth;
        float o[N];
        for (int i = 0; i < N; ++i)
        {
            const float d = len[i] + (i == 1 ? mA : i == 6 ? mB : 0.f);
            o[i] = damp[i].lp (lines[i].readLinear (d));
        }
        // Householder mix: y = o - 2/N * sum(o)
        float sum = 0; for (int i = 0; i < N; ++i) sum += o[i];
        const float k = sum * (2.f / N);
        for (int i = 0; i < N; ++i)
        {
            const float inj = (i & 1) ? inR * 0.3f : inL * 0.3f;
            lines[i].push ((o[i] - k) * fb[i] + x * 0.35f + inj);
        }
        outL = lowCut[0].process ((o[0] + o[2] + o[4] + o[6]) * 0.5f);
        outR = lowCut[1].process ((o[1] + o[3] + o[5] + o[7]) * 0.5f);
        if (shimmerAmount > 0.001f) fbShimmer = shimmer.process (0.5f * (outL + outR)) * 0.6f;
        else fbShimmer = 0;
    }
private:
    double sr = 48000;
    DelayLine lines[N], pre; Allpass diff[4]; OnePole damp[N];
    float len[N] {}, fb[N] {};
    float preDelay = 0, modDepth = 0, shimmerAmount = 0, shimmerSt = 12.f, fbShimmer = 0, lcCache = -1;
    Lfo lfoA, lfoB; GrainShifter shimmer; Biquad lowCut[2];
};

// ---------------------------------------------------------------------------
class Delay : public Module   // "Lethe"
{
public:
    void prepare (const ModuleContext& c) override
    {
        sr = c.sampleRate;
        for (auto& l : line) l.prepare ((int) (2.6 * sr));
        lfo.setSampleRate (sr); lfo.setRate (0.9f);
        timeSm.reset (sr, 250.f, 400.f);
        reset();
    }
    void reset() override { for (auto& l : line) l.clear(); for (auto& f : tone) f.reset(); for (auto& f : hp) f.reset(); toneCache = -1; }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext& ctx) override
    {
        // p: time ms, sync, division, feedback %, mix %, tone Hz, mod %, mode
        float t = p[0];
        if (p[1] > 0.5f) t = (float) (NoteDivisions::beats ((int) p[2]) * 60000.0 / juce::jlimit (20.0, 400.0, ctx.bpm));
        timeSm.set (clampf (t, 1.f, 2500.f));
        const float fbk = p[3] * 0.01f * 0.98f, mix = p[4] * 0.01f, mod = p[6] * 0.01f;
        const int mode = (int) p[7];
        if (p[5] != toneCache) { toneCache = p[5]; for (int k = 0; k < 2; ++k) { tone[k].set (Biquad::LP, sr, p[5], 0.6f); hp[k].set (Biquad::HP, sr, mode == 1 ? 180.f : 60.f, 0.6f); } }
        for (int i = 0; i < n; ++i)
        {
            const float tm = timeSm.next();
            const float wob = mode == 1 ? mod * (0.004f * lfo.next() + 0.0015f * noise.next()) : mod * 0.002f * lfo.next();
            const float d = juce::jmax (1.f, tm * 0.001f * (float) sr * (1.f + wob));
            const float inL = ch[0][i], inR = numCh > 1 ? ch[1][i] : inL;
            float yL = line[0].readCubic (d), yR = line[1].readCubic (d);
            yL = hp[0].process (tone[0].process (yL)); yR = hp[1].process (tone[1].process (yR));
            if (mode == 1) { yL = fastTanh (yL * 1.3f) * 0.77f; yR = fastTanh (yR * 1.3f) * 0.77f; }   // tape saturation
            if (mode == 2)
            {   // ping-pong: input to the left, repeats bounce across
                line[0].push (0.5f * (inL + inR) + yR * fbk);
                line[1].push (yL * fbk);
            }
            else
            {
                line[0].push (inL + yL * fbk);
                line[1].push (inR + yR * fbk);
            }
            ch[0][i] = inL * (1.f - mix * 0.5f) + yL * mix;
            if (numCh > 1) ch[1][i] = inR * (1.f - mix * 0.5f) + yR * mix;
            else ch[0][i] = inL * (1.f - mix * 0.5f) + 0.5f * (yL + yR) * mix;
        }
    }
private:
    double sr = 48000; DelayLine line[2]; Biquad tone[2], hp[2]; Lfo lfo; Noise noise; Smoother timeSm; float toneCache = -1;
};

// ---------------------------------------------------------------------------
class Reverb : public Module   // "Crypt" and "Seraph" (shimmer)
{
public:
    explicit Reverb (bool shimmerMode) : shimmer (shimmerMode) {}
    void prepare (const ModuleContext& c) override { fdn.prepare (c.sampleRate); mixSm.reset (c.sampleRate, 30.f, 0.3f); }
    void reset() override { fdn.clear(); }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        if (shimmer)
        {   // p: decay s, size, shimmer %, interval, tone Hz, pre-delay ms, mix %
            static const float iv[] = { 12.f, 7.f, 19.f, 24.f, -12.f };
            fdn.set (p[0], p[1] * 0.01f, p[5], p[4], 150.f, 0.6f, p[2] * 0.01f * 0.9f, iv[juce::jlimit (0, 4, (int) p[3])]);
            mixSm.set (p[6] * 0.01f);
        }
        else
        {   // p: decay s, size, pre-delay ms, tone Hz, low cut Hz, mod, mix %
            fdn.set (p[0], p[1] * 0.01f, p[2], p[3], p[4], p[5] * 0.1f);
            mixSm.set (p[6] * 0.01f);
        }
        for (int i = 0; i < n; ++i)
        {
            const float inL = ch[0][i], inR = numCh > 1 ? ch[1][i] : inL;
            float wl, wr; fdn.tick (inL, inR, wl, wr);
            const float m = mixSm.next();
            const float dryG = m < 0.5f ? 1.f : 2.f * (1.f - m);       // dry stays full until 50%, then fades
            ch[0][i] = inL * dryG + wl * m;
            if (numCh > 1) ch[1][i] = inR * dryG + wr * m;
            else ch[0][i] = inL * dryG + 0.5f * (wl + wr) * m;
        }
    }
private:
    bool shimmer; FdnReverb fdn; Smoother mixSm;
};

// ---------------------------------------------------------------------------
class CryptReverb : public Module   // "Crypt": its own reverb, or one of the Boss RV-6 modes (IRs from the pedal pack)
{
public:
    void prepare (const ModuleContext& c) override
    {
        algo.prepare (c); ir.prepare (c.sampleRate, c.maxBlock); mixSm.reset (c.sampleRate, 30.f, 0.3f);
        for (auto& w : wet) w.assign ((size_t) c.maxBlock, 0.f);
        for (auto& w : dry) w.assign ((size_t) c.maxBlock, 0.f);
    }
    void reset() override { algo.reset(); ir.reset(); }
    void setIrBank (std::vector<IrFile> irs) override { ir.setBank (std::move (irs)); ir.messageTick(); }
    void messageTick() override { ir.messageTick(); }
    juce::String fileName() const override { return ir.bankSize() > 0 ? ir.currentName() : juce::String(); }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext& ctx) override
    {
        // p: decay s, size, pre-delay ms, tone Hz, low cut Hz, mod, mix %, voice (0 = Crypt, 1.. = RV-6 modes)
        const int voice = (int) std::lround (p[7]);
        if (voice <= 0 || ir.bankSize() == 0) { algo.process (ch, numCh, n, p, ctx); return; }
        n = juce::jmin (n, (int) wet[0].size());
        ir.setParams (voice - 1, p[0], p[2], p[3], p[4]);
        mixSm.set (ir.ready() ? p[6] * 0.01f : 0.f);
        const float* inR = numCh > 1 ? ch[1] : ch[0];
        std::copy (ch[0], ch[0] + n, dry[0].begin()); std::copy (inR, inR + n, dry[1].begin());
        ir.processWet (dry[0].data(), dry[1].data(), wet[0].data(), wet[1].data(), n);
        for (int i = 0; i < n; ++i)
        {
            const float m = mixSm.next(), dryG = m < 0.5f ? 1.f : 2.f * (1.f - m);
            if (numCh > 1) { ch[0][i] = dry[0][(size_t) i] * dryG + wet[0][(size_t) i] * m; ch[1][i] = dry[1][(size_t) i] * dryG + wet[1][(size_t) i] * m; }
            else ch[0][i] = dry[0][(size_t) i] * dryG + 0.5f * (wet[0][(size_t) i] + wet[1][(size_t) i]) * m;
        }
    }
private:
    Reverb algo { false }; IrCore ir; Smoother mixSm;
    std::vector<float> wet[2], dry[2];
};

// ---------------------------------------------------------------------------
class Swell : public Module   // "Aurora": auto volume swells into a soft reverb
{
public:
    void prepare (const ModuleContext& c) override
    {
        sr = c.sampleRate; fdn.prepare (sr);
        fast.set (sr, 0.5f, 30.f); slow.set (sr, 20.f, 250.f);
        reset();
    }
    void reset() override { fdn.clear(); gain = 0; fast.reset(); slow.reset(); }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: attack ms, sensitivity, reverb %, decay s, mix %
        const float attack = juce::jmax (5.f, p[0]) * 0.001f * (float) sr;
        const float sens = 1.25f + (10.f - p[1]) * 0.12f;
        fdn.set (p[3], 0.85f, 20.f, 5500.f, 120.f, 0.8f);
        const float rv = p[2] * 0.01f, mix = p[4] * 0.01f;
        for (int i = 0; i < n; ++i)
        {
            float a = 0; for (int c = 0; c < numCh; ++c) a = std::max (a, std::abs (ch[c][i]));
            const float ef = fast.process (a), es = slow.process (a);
            if (ef > es * sens && ef > 0.004f && gain > 0.6f) gain = 0.f;      // new pick: duck, then swell
            gain = juce::jmin (1.f, gain + 1.f / attack);
            const float g = gain * gain;
            const float inL = ch[0][i] * g, inR = (numCh > 1 ? ch[1][i] : ch[0][i]) * g;
            float wl, wr; fdn.tick (inL, inR, wl, wr);
            const float oL = inL + wl * rv, oR = inR + wr * rv;
            ch[0][i] = ch[0][i] * (1.f - mix) + oL * mix;
            if (numCh > 1) ch[1][i] = ch[1][i] * (1.f - mix) + oR * mix;
        }
    }
private:
    double sr = 48000; FdnReverb fdn; EnvFollower fast, slow; float gain = 1;
};

// ---------------------------------------------------------------------------
class Freeze : public Module   // "Stasis": hold the current sound forever
{
public:
    void prepare (const ModuleContext& c) override
    {
        sr = c.sampleRate;
        histLen = (int) (sr * 1.0);
        for (auto& h : hist) h.assign ((size_t) histLen, 0.f);
        for (auto& f : frozen) f.assign ((size_t) (sr * 0.5), 0.f);
        reset();
    }
    void reset() override { w = 0; active = false; level = 0; for (auto& g : grains) g = {}; }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: freeze (hold), fade ms, level %, dry %, texture %
        const bool on = p[0] > 0.5f;
        const int fl = (int) frozen[0].size();
        if (on && ! active)
        {   // capture the last 0.5 s
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < fl; ++i) frozen[c][(size_t) i] = hist[c][(size_t) ((w - fl + i + histLen) % histLen)];
            for (int g = 0; g < 4; ++g) grains[g] = { rng.nextFloat() * fl * 0.5f, (float) g / 4.f };
        }
        active = on;
        const float fadeStep = 1.f / juce::jmax (1.f, p[1] * 0.001f * (float) sr);
        const float lvl = p[2] * 0.01f, dry = p[3] * 0.01f, tex = p[4] * 0.01f;
        const float grainLen = (0.12f + 0.25f * (1.f - tex)) * (float) sr;
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < 2; ++c) hist[c][(size_t) w] = ch[juce::jmin (c, numCh - 1)][i];
            w = (w + 1) % histLen;
            level = on ? juce::jmin (1.f, level + fadeStep) : juce::jmax (0.f, level - fadeStep);
            float oL = 0, oR = 0;
            if (level > 0.f)
            {
                for (auto& g : grains)
                {
                    g.phase += 1.f / grainLen;
                    if (g.phase >= 1.f) { g.phase -= 1.f; g.start = rng.nextFloat() * (fl - grainLen - 2.f) * (0.3f + tex * 0.7f); }
                    const float pos = g.start + g.phase * grainLen;
                    const int ip = juce::jlimit (0, fl - 2, (int) pos);
                    const float win = hann (g.phase) * 0.5f;
                    oL += frozen[0][(size_t) ip] * win; oR += frozen[1][(size_t) ip] * win;
                }
            }
            for (int c = 0; c < numCh; ++c)
                ch[c][i] = ch[c][i] * dry + (c == 0 ? oL : oR) * level * lvl * 1.4f;
        }
    }
private:
    struct Grain { float start = 0, phase = 0; };
    double sr = 48000; std::vector<float> hist[2], frozen[2]; int histLen = 1, w = 0; bool active = false; float level = 0;
    Grain grains[4]; juce::Random rng;
};

// ---------------------------------------------------------------------------
class TapeStop : public Module   // "Gravity": the sound slows down to a stop
{
public:
    void prepare (const ModuleContext& c) override
    {
        sr = c.sampleRate; len = (int) (sr * 4.0);
        for (auto& b : buf) b.assign ((size_t) len, 0.f);
        reset();
    }
    void reset() override { w = 0; rate = 1.f; readPos = 0; engaged = false; wet = 0; }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: stop (hold), stop time ms, start time ms, mix %
        const bool on = p[0] > 0.5f;
        if (on && ! engaged) { readPos = (float) w; rate = 1.f; }
        engaged = on;
        const float down = 1.f / juce::jmax (1.f, p[1] * 0.001f * (float) sr), up = 1.f / juce::jmax (1.f, p[2] * 0.001f * (float) sr);
        const float mix = p[3] * 0.01f;
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < 2; ++c) buf[c][(size_t) w] = ch[juce::jmin (c, numCh - 1)][i];
            w = (w + 1) % len;
            if (engaged) { rate = juce::jmax (0.f, rate - down); wet = juce::jmin (1.f, wet + 0.01f); }
            else         { rate = juce::jmin (1.f, rate + up); if (rate >= 1.f) wet = juce::jmax (0.f, wet - up); }
            if (wet <= 0.f) continue;
            readPos += rate;
            if (readPos >= (float) len) readPos -= (float) len;
            const int i0 = (int) readPos; const float fr = readPos - (float) i0; const int i1 = (i0 + 1) % len;
            for (int c = 0; c < numCh; ++c)
            {
                const float v = buf[c][(size_t) i0] + (buf[c][(size_t) i1] - buf[c][(size_t) i0]) * fr;
                const float g = wet * mix;
                ch[c][i] = ch[c][i] * (1.f - g) + v * g * juce::jmin (1.f, rate * 4.f + 0.05f);
            }
        }
    }
private:
    double sr = 48000; std::vector<float> buf[2]; int len = 1, w = 0; float rate = 1, readPos = 0, wet = 0; bool engaged = false;
};

// ---------------------------------------------------------------------------
class Looper : public Module
{
public:
    void prepare (const ModuleContext& c) override
    {
        sr = c.sampleRate; maxLen = (int) (sr * 60.0);
        for (auto& b : buf) b.assign ((size_t) maxLen, 0.f);
        reset();
    }
    void reset() override { state = Empty; loopLen = 0; pos = 0; lastRec = lastStop = lastClear = false; fadeIn = 0; }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: rec/play/dub (tap), stop (tap), clear (tap), loop level %, reverse, half speed, dry %
        const bool rec = p[0] > 0.5f, stop = p[1] > 0.5f, clr = p[2] > 0.5f;
        if (rec && ! lastRec)
        {
            switch (state)
            {
                case Empty:     state = Recording; loopLen = 0; pos = 0; break;
                case Recording: state = Playing; if (loopLen < (int) (0.05 * sr)) state = Empty; break;
                case Playing:   state = Overdub; break;
                case Overdub:   state = Playing; break;
                case Stopped:   state = Playing; pos = 0; break;
            }
        }
        if (stop && ! lastStop && state != Empty) state = (state == Recording ? Playing : Stopped);
        if (clr && ! lastClear) { state = Empty; loopLen = 0; pos = 0; for (auto& b : buf) std::fill (b.begin(), b.end(), 0.f); }
        lastRec = rec; lastStop = stop; lastClear = clr;

        const float lvl = p[3] * 0.01f, dry = p[6] * 0.01f;
        const bool reverse = p[4] > 0.5f, half = p[5] > 0.5f;
        for (int i = 0; i < n; ++i)
        {
            const float inL = ch[0][i], inR = numCh > 1 ? ch[1][i] : inL;
            float oL = 0, oR = 0;
            if (state == Recording)
            {
                if (loopLen < maxLen) { buf[0][(size_t) loopLen] = inL; buf[1][(size_t) loopLen] = inR; ++loopLen; }
                else state = Playing;
            }
            else if ((state == Playing || state == Overdub) && loopLen > 0)
            {
                const double step = half ? 0.5 : 1.0;
                double rp = reverse ? (double) loopLen - 1.0 - pos : pos;
                const int ip = juce::jlimit (0, loopLen - 1, (int) rp);
                // 5 ms fade at the loop seam
                const int fadeLen = juce::jmin (loopLen / 4, (int) (0.005 * sr));
                float seam = 1.f;
                if (fadeLen > 0) { const int fromEnd = loopLen - 1 - (int) pos; seam = juce::jmin (1.f, juce::jmin ((float) pos, (float) fromEnd) / (float) fadeLen); }
                oL = buf[0][(size_t) ip] * seam; oR = buf[1][(size_t) ip] * seam;
                if (state == Overdub) { buf[0][(size_t) ip] = clampf (buf[0][(size_t) ip] + inL, -4.f, 4.f); buf[1][(size_t) ip] = clampf (buf[1][(size_t) ip] + inR, -4.f, 4.f); }
                pos += step; if (pos >= loopLen) pos -= loopLen;
            }
            ch[0][i] = inL * dry + oL * lvl;
            if (numCh > 1) ch[1][i] = inR * dry + oR * lvl;
        }
        stateOut.store ((int) state);
    }
    int loopState() const { return stateOut.load(); }
private:
    enum State { Empty, Recording, Playing, Overdub, Stopped };
    double sr = 48000; std::vector<float> buf[2]; int maxLen = 1, loopLen = 0; double pos = 0;
    State state = Empty; bool lastRec = false, lastStop = false, lastClear = false; float fadeIn = 0;
    std::atomic<int> stateOut { 0 };
};

// ---------------------------------------------------------------------------
class Glitch : public Module   // "Shatter": tempo-synced stutter / reverse / half-speed slices
{
public:
    void prepare (const ModuleContext& c) override
    {
        sr = c.sampleRate; len = (int) (sr * 4.0);
        for (auto& b : buf) b.assign ((size_t) len, 0.f);
        reset();
    }
    void reset() override { w = 0; sliceLeft = 0; action = 0; sliceLen = 1; playPos = 0; sliceStart = 0; }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext& ctx) override
    {
        // p: mode, division, hold (glitch while held), auto %, mix %
        static const int divIdx[] = { 3, 6, 9, 12 };   // 1/4, 1/8, 1/16, 1/32
        const int mode = (int) p[0];
        const double beats = NoteDivisions::beats (divIdx[juce::jlimit (0, 3, (int) p[1])]);
        const int slice = juce::jmax (64, (int) (beats * 60.0 / juce::jlimit (20.0, 400.0, ctx.bpm) * sr));
        const bool hold = p[2] > 0.5f; const float autoProb = p[3] * 0.01f, mix = p[4] * 0.01f;
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < 2; ++c) buf[c][(size_t) w] = ch[juce::jmin (c, numCh - 1)][i];
            if (sliceLeft <= 0)
            {
                // new slice: decide what to do with it
                sliceLen = slice; sliceLeft = slice;
                const bool fire = hold || rng.nextFloat() < autoProb;
                action = fire ? (mode == 3 ? 1 + rng.nextInt (3) : mode + 1) : 0;
                sliceStart = (w - slice + len) % len;      // the slice that just went by
                playPos = 0;
            }
            float oL = 0, oR = 0;
            if (action != 0)
            {
                double rp;
                if (action == 1) rp = std::fmod (playPos, (double) juce::jmax (64, sliceLen / 4));          // stutter (repeat 1/4 of slice)
                else if (action == 2) rp = (double) sliceLen - 1.0 - playPos;                               // reverse
                else rp = playPos * 0.5;                                                                     // half speed
                const int idx = (sliceStart + juce::jlimit (0, sliceLen - 1, (int) rp)) % len;
                const float edge = juce::jmin (1.f, (float) juce::jmin (sliceLeft, sliceLen - sliceLeft) / (float) (0.003 * sr));
                oL = buf[0][(size_t) idx] * edge; oR = buf[1][(size_t) idx] * edge;
                playPos += 1.0;
            }
            w = (w + 1) % len; --sliceLeft;
            const float g = action != 0 ? mix : 0.f;
            for (int c = 0; c < numCh; ++c) ch[c][i] = ch[c][i] * (1.f - g) + (c == 0 ? oL : oR) * g;
        }
    }
private:
    double sr = 48000; std::vector<float> buf[2]; int len = 1, w = 0, sliceLeft = 0, sliceLen = 1, action = 0, sliceStart = 0; double playPos = 0;
    juce::Random rng;
};
} // namespace

void registerTimeModules (std::vector<ModuleType>& t)
{
    using P = ModuleParam;
    const auto divs = NoteDivisions::names();
    P mom { "Hold", "", 0, 1, 0 }; mom.momentary = true;
    P tog { "", "", 0, 1, 0 }; tog.toggle = true;
    auto toggle = [tog] (const char* name, float def) { P x = tog; x.name = name; x.def = def; return x; };
    auto momentary = [mom] (const char* name) { P x = mom; x.name = name; return x; };

    t.push_back ({ "lethe", "Lethe Delay", "delay", RackPost | RackSfx | RackPre,
        "Digital, tape or ping-pong delay. Sync locks the repeats to the tempo.",
        { P { "Time", "ms", 1, 2000, 420, 2.f }, toggle ("Sync", 0), P { "Division", "", 0, 12, 7, 1, divs }, P { "Feedback", "%", 0, 100, 35 },
          P { "Mix", "%", 0, 100, 30 }, P { "Tone", "Hz", 800, 16000, 5500, 2.f }, P { "Mod", "%", 0, 100, 15 }, P { "Mode", "", 0, 2, 0, 1, { "Digital", "Tape", "Ping-Pong" } } },
        [] { return std::make_unique<Delay>(); } });
    {   // Voice 0 = Crypt's own reverb; 1-6 = the Boss RV-6 modes (IRs from the pedal pack; Size and Mod only affect Crypt's own voice)
        P voice { "Voice", "", 0, 6, 0 }; voice.choices = { "CRYPT", "SPRING", "DELAY", "HALL", "PLATE", "ROOM", "MOD" };
        ModuleType m { "crypt", "Crypt Reverb", "reverb", RackPost | RackSfx,
            "Deep, smooth room-to-cathedral reverb. Voice switches to the six Boss RV-6 modes (from the pedal pack).",
            { P { "Decay", "s", 0.2f, 20.f, 2.6f, 2.f }, P { "Size", "%", 0, 100, 60 }, P { "Pre-delay", "ms", 0, 250, 18, 2.f }, P { "Tone", "Hz", 1000, 16000, 6500, 2.f },
              P { "Low Cut", "Hz", 20, 600, 120, 2.f }, P { "Mod", "", 0, 10, 4 }, P { "Mix", "%", 0, 100, 25 }, voice },
            [] { return std::make_unique<CryptReverb>(); } };
        m.fileKind = "pack"; m.packSelectors = { 7 }; m.packFirstChoice = 1;
        for (auto* f : { "1 Rv-6 Spring Reverb.wav", "2 Rv-6 Delay.wav", "3 Rv-6 Hall 1 Reverb.wav", "4 Rv-6 Plate Reverb.wav", "5 Rv-6 Room Reverb.wav", "6 Rv-6 Mod Reverb.wav" })
            m.irs.push_back (juce::String ("Pedals/Crypt/") + f);
        t.push_back (std::move (m));
    }
    t.push_back ({ "seraph", "Seraph Shimmer", "reverb", RackPost | RackSfx,
        "Reverb whose tail rises an octave (or a fifth): angelic pads.",
        { P { "Decay", "s", 0.5f, 20.f, 6.f, 2.f }, P { "Size", "%", 0, 100, 80 }, P { "Shimmer", "%", 0, 100, 55 }, P { "Interval", "", 0, 4, 0, 1, { "+Oct", "+5th", "+Oct+5th", "+2 Oct", "-Oct" } },
          P { "Tone", "Hz", 1000, 16000, 7000, 2.f }, P { "Pre-delay", "ms", 0, 250, 30, 2.f }, P { "Mix", "%", 0, 100, 35 } },
        [] { return std::make_unique<Reverb> (true); } });
    t.push_back ({ "aurora", "Aurora Swell", "reverb", RackPost | RackSfx,
        "Removes the pick attack and swells each note in, violin-style, into a soft reverb.",
        { P { "Attack", "ms", 20, 2000, 450, 2.f }, P { "Sensitivity", "", 0, 10, 6 }, P { "Reverb", "%", 0, 100, 40 }, P { "Decay", "s", 0.5f, 12.f, 4.f, 2.f }, P { "Mix", "%", 0, 100, 100 } },
        [] { return std::make_unique<Swell>(); } });
    t.push_back ({ "stasis", "Stasis Freeze", "freeze", RackSfx | RackPost,
        "Hold the button to freeze the sound into an endless pad, then play over it.",
        { momentary ("Freeze"), P { "Fade", "ms", 5, 2000, 250, 2.f }, P { "Level", "%", 0, 100, 80 }, P { "Dry", "%", 0, 100, 100 }, P { "Texture", "%", 0, 100, 40 } },
        [] { return std::make_unique<Freeze>(); } });
    t.push_back ({ "gravity", "Gravity Tape Stop", "glitch", RackSfx | RackPost,
        "Hold to slow the sound down to a stop like a tape machine losing power.",
        { momentary ("Stop"), P { "Stop Time", "ms", 50, 3000, 700, 2.f }, P { "Start Time", "ms", 20, 2000, 250, 2.f }, P { "Mix", "%", 0, 100, 100 } },
        [] { return std::make_unique<TapeStop>(); } });
    t.push_back ({ "looper", "Looper", "looper", RackSfx | RackPost,
        "Tap REC to record, tap again to play, again to overdub. Up to 60 seconds.",
        { momentary ("Rec / Play"), momentary ("Stop"), momentary ("Clear"), P { "Loop Level", "%", 0, 100, 90 }, toggle ("Reverse", 0), toggle ("Half Speed", 0), P { "Dry", "%", 0, 100, 100 } },
        [] { return std::make_unique<Looper>(); } });
    t.push_back ({ "shatter", "Shatter Glitch", "glitch", RackSfx | RackPost,
        "Tempo-synced stutters, reverses and half-speed drops. Hold to glitch, or let Auto surprise you.",
        { P { "Mode", "", 0, 3, 0, 1, { "Stutter", "Reverse", "Half", "Random" } }, P { "Division", "", 0, 3, 1, 1, { "1/4", "1/8", "1/16", "1/32" } },
          momentary ("Glitch"), P { "Auto", "%", 0, 100, 0 }, P { "Mix", "%", 0, 100, 100 } },
        [] { return std::make_unique<Glitch>(); } });
}
} // namespace obs::dsp
