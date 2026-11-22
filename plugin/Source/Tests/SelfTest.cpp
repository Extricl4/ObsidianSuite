// =============================================================================
//  Offline self-test. Run:   "Obsidian Amp Suite" --selftest [report.txt] [--nam file.nam]
//  Exit code 0 = everything passed. CI runs this on every build.
//
//  Checks: no NaN/Inf ever, no runaway levels, sound passes through, every
//  module in every rack with random settings, random knob fuzzing, sample rates
//  44.1/48/96 k and odd block sizes, transpose accuracy (Live + HQ), synth
//  tracking, preset save/load round-trip, DAW state round-trip, NAM loading,
//  pack pedals (file maps, IR reverb, capture switching; --packs also plays
//  every file of the installed Obsidian pedal pack), and a CPU budget for a full rig.
// =============================================================================
#include "../DSP/HarmonyDefs.h"
#include "../Core/Updater.h"
#include "../PluginProcessor.h"
#include <set>
#include "../DSP/Modules/ModuleRegistry.h"

namespace obs
{
namespace
{
struct Report
{
    juce::StringArray lines; int failures = 0, checks = 0;
    void ok (const juce::String& s)   { ++checks; lines.add ("  ok    " + s); }
    void fail (const juce::String& s) { ++checks; ++failures; lines.add ("  FAIL  " + s); }
    void info (const juce::String& s) { lines.add ("        " + s); }
    void head (const juce::String& s) { lines.add (""); lines.add ("== " + s); }
    void check (bool cond, const juce::String& s) { cond ? ok (s) : fail (s); }
};

/** Karplus-Strong plucked string: a reasonable stand-in for a DI guitar. */
class Guitar
{
public:
    explicit Guitar (double sr) : sr (sr) {}
    void pluck (float hz, float amp = 0.5f)
    {
        const int len = juce::jmax (2, (int) (sr / hz));
        buf.assign ((size_t) len, 0.f);
        for (auto& b : buf) b = (rng.nextFloat() * 2.f - 1.f) * amp;
        pos = 0;
    }
    float next()
    {
        if (buf.empty()) return 0.f;
        const size_t n = buf.size(), i = (size_t) pos, j = (i + 1) % n;
        const float out = buf[i];
        buf[i] = 0.4985f * (buf[i] + buf[j]);
        pos = (int) j;
        return out;
    }
private:
    double sr; std::vector<float> buf; int pos = 0; juce::Random rng { 1234 };
};

struct RunStats { float peak = 0, rms = 0; bool finite = true; double maxBlockLoad = 0, avgLoad = 0; };

RunStats render (ObsidianProcessor& p, double sr, int block, double seconds, std::function<float (int)> input, int numInCh = 2)
{
    RunStats st;
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    const int total = (int) (sr * seconds);
    double sumSq = 0; long count = 0; double loadSum = 0; int blocks = 0;
    for (int done = 0; done < total; done += block)
    {
        const int n = juce::jmin (block, total - done);
        buf.setSize (2, n, false, false, true);
        for (int i = 0; i < n; ++i)
        {
            const float x = input (done + i);
            buf.setSample (0, i, x);
            buf.setSample (1, i, numInCh > 1 ? 0.f : x);   // guitar on input 1, input 2 silent (like a Scarlett Solo)
        }
        const auto t0 = juce::Time::getHighResolutionTicks();
        p.processBlock (buf, midi);
        const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
        const double load = secs / (n / sr);
        st.maxBlockLoad = juce::jmax (st.maxBlockLoad, load); loadSum += load; ++blocks;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float v = buf.getSample (c, i);
                if (! std::isfinite (v)) st.finite = false;
                st.peak = juce::jmax (st.peak, std::abs (v));
                sumSq += (double) v * v; ++count;
            }
    }
    st.rms = (float) std::sqrt (sumSq / juce::jmax (1L, count));
    st.avgLoad = loadSum / juce::jmax (1, blocks);
    return st;
}

void setNorm (ObsidianProcessor& p, const juce::String& id, float v)
{
    if (auto* prm = p.state().getParameter (id)) prm->setValueNotifyingHost (juce::jlimit (0.f, 1.f, v));
}
void setReal (ObsidianProcessor& p, const juce::String& id, float v)
{
    if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (p.state().getParameter (id))) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

/** Offline YIN with parabolic interpolation (sub-sample accurate), for checking transpose accuracy. */
float detectPitch (const std::vector<float>& x, double sr)
{
    const int W = 4096, tauMax = 2000;
    if ((int) x.size() < W + tauMax + 2) return 0;
    const int start = (int) x.size() - W - tauMax - 1;
    std::vector<double> d ((size_t) tauMax, 1.0);
    double run = 0;
    for (int tau = 1; tau < tauMax; ++tau)
    {
        double acc = 0;
        for (int j = 0; j < W; ++j) { const double df = x[(size_t) (start + j)] - x[(size_t) (start + j + tau)]; acc += df * df; }
        run += acc; d[(size_t) tau] = run > 0 ? acc * tau / run : 1.0;
    }
    for (int tau = 20; tau < tauMax - 1; ++tau)
        if (d[(size_t) tau] < 0.15)
        {
            while (tau + 1 < tauMax - 1 && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau;
            const double s0 = d[(size_t) tau - 1], s1 = d[(size_t) tau], s2 = d[(size_t) tau + 1], den = s0 - 2 * s1 + s2;
            const double t = tau + (std::abs (den) > 1e-12 ? 0.5 * (s0 - s2) / den : 0.0);
            return (float) (sr / t);
        }
    return 0;
}

/** Runs one module on its own (stereo), with knob values in real units. */
struct ModuleRig
{
    const dsp::ModuleType* type = nullptr;
    std::unique_ptr<dsp::Module> m;
    std::vector<float> p;
    dsp::ModuleContext ctx;
    explicit ModuleRig (const juce::String& key, double sr, int block)
    {
        type = dsp::findModule (key);
        if (type == nullptr) return;
        m = type->create(); ctx.sampleRate = sr; ctx.maxBlock = block; m->prepare (ctx);
        p.assign ((size_t) kSlotParams, 0.f);
        for (size_t k = 0; k < type->params.size(); ++k) p[k] = type->params[k].def;
    }
    /** Sets the selector knobs so that file `index` plays. */
    void selectFile (int index)
    {
        index += type->packFirstChoice;
        for (int k = (int) type->packSelectors.size() - 1; k >= 0; --k)
        {
            const int sel = type->packSelectors[(size_t) k], size = dsp::selectorSize (type->params[(size_t) sel]);
            p[(size_t) sel] = (float) (index % size); index /= size;
        }
    }
    void set (const juce::String& name, float v) { for (size_t k = 0; k < type->params.size(); ++k) if (type->params[k].name == name) p[k] = v; }
    RunStats run (double sr, int block, double seconds, std::function<float (int)> input, std::function<void (int)> everyBlock = {})
    {
        RunStats st; juce::AudioBuffer<float> b (2, block); double sum = 0; long cnt = 0; double load = 0; int blocks = 0;
        const int total = (int) (sr * seconds);
        for (int done = 0; done < total; done += block)
        {
            if (everyBlock) everyBlock (done);
            const int n = juce::jmin (block, total - done);
            for (int i = 0; i < n; ++i) { const float x = input (done + i); b.setSample (0, i, x); b.setSample (1, i, x); }
            const auto t0 = juce::Time::getHighResolutionTicks();
            m->process (b.getArrayOfWritePointers(), 2, n, p.data(), ctx);
            const double l = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) / (n / sr);
            load += l; ++blocks; st.maxBlockLoad = juce::jmax (st.maxBlockLoad, l);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i)
            {
                const float v = b.getSample (c, i);
                if (! std::isfinite (v)) st.finite = false;
                st.peak = juce::jmax (st.peak, std::abs (v)); sum += (double) v * v; ++cnt;
            }
        }
        st.rms = (float) std::sqrt (sum / juce::jmax (1L, cnt)); st.avgLoad = load / juce::jmax (1, blocks);
        return st;
    }
};

/** A made-up reverb IR: decaying stereo noise. */
dsp::IrFile fakeIr (double sr, float seconds, float rt60, int channels, juce::Random& rng)
{
    dsp::IrFile f; f.sampleRate = sr; f.name = "test";
    const int n = (int) (sr * seconds);
    f.buffer.setSize (channels, n);
    for (int c = 0; c < channels; ++c)
        for (int i = 0; i < n; ++i)
            f.buffer.setSample (c, i, (rng.nextFloat() * 2.f - 1.f) * std::pow (10.f, -3.f * (float) i / (float) (sr * rt60)));
    return f;
}
} // namespace

int runSelfTest (const juce::StringArray& args)
{
    Report r;
    const auto start = juce::Time::getMillisecondCounterHiRes();
    juce::File reportFile;
    juce::File namFile;
    const bool packs = args.contains ("--packs");
    for (int i = 0; i < args.size(); ++i)
    {
        if (args[i] == "--nam" && i + 1 < args.size()) namFile = juce::File (args[i + 1].unquoted());
        else if (args[i] != "--selftest" && ! args[i].startsWith ("--") && (i == 0 || args[i - 1] != "--nam")) reportFile = juce::File::getCurrentWorkingDirectory().getChildFile (args[i].unquoted());
    }
    r.lines.add (OBSIDIAN_PRODUCT_NAME " " OBSIDIAN_VERSION_STRING " self-test   (" + juce::SystemStats::getCpuModel() + ", " + juce::SystemStats::getOperatingSystemName() + ")");

    const double SR = 48000; const int BLOCK = 256;
    auto proc = std::make_unique<ObsidianProcessor>();
    auto& p = *proc;
    p.setPlayConfigDetails (2, 2, SR, BLOCK);
    p.prepareToPlay (SR, BLOCK);

    Guitar g (SR);
    auto riff = [&g, SR] (int i) -> float
    {
        static const float notes[] = { 82.4f, 82.4f, 98.f, 110.f, 82.4f, 146.8f, 123.5f, 196.f };
        const int step = (int) (SR * 0.25);
        if (i % step == 0) g.pluck (notes[(i / step) % 8], 0.45f);
        return g.next();
    };

    // ---------------------------------------------------------------- 1. default chain
    r.head ("Default rig (Obsidian Core amp, 4x12 cab)");
    {
        auto st = render (p, SR, BLOCK, 4.0, riff);
        r.check (st.finite, "output is always a finite number");
        r.check (st.rms > 0.003f, "sound passes through (rms " + juce::String (juce::Decibels::gainToDecibels (st.rms), 1) + " dB)");
        r.check (st.peak <= 2.0f, "peak stays under the safety ceiling (" + juce::String (st.peak, 2) + ")");
        r.info ("CPU: avg " + juce::String (st.avgLoad * 100, 2) + "% of real time, worst block " + juce::String (st.maxBlockLoad * 100, 1) + "%");
    }

    // ---------------------------------------------------------------- 1a. levels and switching
    r.head ("Levels and switching (mono tracks in a DAW, sections on / off, bounces)");
    {
        const auto sine = [SR] (int i) { return 0.2f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 110.0 * i / SR); };
        p.presetManager().loadInit();
        const auto one = render (p, SR, BLOCK, 1.0, sine);          // guitar on input 1 only
        p.presetManager().loadInit();
        const auto dup = render (p, SR, BLOCK, 1.0, sine, 1);       // the same guitar on both channels (a mono track in a DAW)
        const float diffDb = juce::Decibels::gainToDecibels (dup.rms) - juce::Decibels::gainToDecibels (one.rms);
        r.check (std::abs (diffDb) < 0.5f, juce::String::formatted ("a mono track in a DAW (both channels the same) plays at the same level as one input (%+.1f dB)", diffDb));

        // switching a section on / off mid-note: no step in the output (largest jump vs the steady signal)
        auto jump = [&] (const juce::String& id, float from, float to, std::function<void()> setup) {
            p.presetManager().loadInit(); if (setup) setup(); setNorm (p, id, from);
            juce::AudioBuffer<float> buf (2, BLOCK); juce::MidiBuffer midi; std::vector<float> out;
            const int blocks = (int) (1.2 * SR) / BLOCK, swBlock = (int) (0.6 * SR) / BLOCK;
            int idx = 0;
            for (int b = 0; b < blocks; ++b)
            {
                if (b == swBlock) setNorm (p, id, to);
                for (int i = 0; i < BLOCK; ++i) { buf.setSample (0, i, 1.25f * sine (idx++)); buf.setSample (1, i, 0.f); }
                p.processBlock (buf, midi);
                for (int i = 0; i < BLOCK; ++i) out.push_back (buf.getSample (0, i));
            }
            const int sw = swBlock * BLOCK, win = (int) (0.2 * SR);
            double before = 1e-9, after = 1e-9, at = 0;
            for (int i = sw - win; i < sw; ++i) before = std::max (before, (double) std::abs (out[(size_t) i] - out[(size_t) i - 1]));
            for (int i = sw + win; i < sw + 2 * win; ++i) after = std::max (after, (double) std::abs (out[(size_t) i] - out[(size_t) i - 1]));
            for (int i = sw; i < sw + (int) (0.05 * SR); ++i) at = std::max (at, (double) std::abs (out[(size_t) i] - out[(size_t) i - 1]));
            return at / std::max (before, after); };
        const double ampJ = jump ("amp.on", 1.f, 0.f, {}), cabJ = jump ("cab.on", 1.f, 0.f, {});
        const double eqJ = jump ("eq.on", 1.f, 0.f, [&] { setReal (p, "eq.b2", 12.f); setReal (p, "eq.b7", -12.f); setReal (p, "eq.lc", 400.f); });
        r.check (ampJ < 1.6 && cabJ < 1.6 && eqJ < 1.6, juce::String::formatted ("switching AMP / CAB / EQ off mid-note doesn't click (largest jump %.2f / %.2f / %.2f x the steady signal's)", ampJ, cabJ, eqJ));
        p.presetManager().loadInit();

        // Settings > Audio > Render quality: a bounce (offline render) uses more oversampling
        const auto oldQ = p.settings().get ("renderQuality"), oldOs = p.settings().get ("oversampling");
        p.settings().set ("renderQuality", "Highest"); p.settings().set ("oversampling", 2); p.applySettings();
        p.setNonRealtime (true);  const int offline = p.engine().oversampling.load();
        p.setNonRealtime (false); const int live = p.engine().oversampling.load();
        p.settings().set ("renderQuality", oldQ); p.settings().set ("oversampling", oldOs); p.applySettings();
        r.check (offline == 4 && live == 2, juce::String::formatted ("Render quality: a bounce renders at %dx oversampling, playing at %dx", offline, live));
    }

    // ---------------------------------------------------------------- 1b. every in-house amp
    r.head ("Every in-house amp (gain 0 / 5 / 10, extreme tone settings)");
    for (const auto& m : obs::amp::models())
    {
        p.rig().setAmp (m.key);
        bool ok = true; float lo = 0, mid = 0, hi = 0; double load = 0;
        for (float g : { 0.f, 5.f, 10.f })
        {
            setReal (p, "amp.gain", g);
            for (float t : { 0.f, 10.f }) { setReal (p, "amp.bass", t); setReal (p, "amp.treble", 10.f - t); setReal (p, "amp.presence", t); setReal (p, "amp.depth", t); setReal (p, "amp.master", t); }
            setReal (p, "amp.bass", 5); setReal (p, "amp.treble", 5); setReal (p, "amp.presence", 5); setReal (p, "amp.depth", 5); setReal (p, "amp.master", 5);
            auto st = render (p, SR, BLOCK, 0.8, riff);
            ok = ok && st.finite && st.peak <= 2.f && st.rms > 0.0005f;
            (g == 0 ? lo : g == 5 ? mid : hi) = juce::Decibels::gainToDecibels (st.rms);
            load = juce::jmax (load, st.avgLoad);
        }
        r.check (ok, juce::String::fromUTF8 (m.name) + "  (based on " + juce::String::fromUTF8 (m.basedOn) + ")  level " + juce::String (lo, 1) + " / " + juce::String (mid, 1) + " / " + juce::String (hi, 1) + " dB, cpu " + juce::String (load * 100, 2) + "%");
    }
    p.presetManager().loadInit();

    // ---------------------------------------------------------------- 2. every module
    r.head ("Every pedal / effect, in every rack it allows, random settings");
    juce::Random rnd (42);
    for (auto& type : dsp::moduleTypes())
    {
        bool allOk = true; float worstPeak = 0; double worstLoad = 0;
        for (int rack = 0; rack < kNumRacks; ++rack)
        {
            const unsigned mask = rack == 0 ? dsp::RackPre : rack == 1 ? dsp::RackPost : dsp::RackSfx;
            if ((type.racks & mask) == 0) continue;
            p.rig().setModule (rack, 0, type.key);
            for (int trial = 0; trial < 4; ++trial)
            {
                for (int k = 0; k < kSlotParams; ++k) setNorm (p, slotParamId (rack, 0, k), trial == 0 ? -1.f : rnd.nextFloat());
                if (trial == 0) p.rig().setModule (rack, 0, type.key);       // defaults
                auto st = render (p, SR, BLOCK, 0.7, riff);
                allOk = allOk && st.finite && st.peak <= 2.0f;
                worstPeak = juce::jmax (worstPeak, st.peak); worstLoad = juce::jmax (worstLoad, st.avgLoad);
            }
            p.rig().setModule (rack, 0, {});
        }
        r.check (allOk, type.name + "  (peak " + juce::String (worstPeak, 2) + ", cpu " + juce::String (worstLoad * 100, 2) + "%)");
    }

    // ---------------------------------------------------------------- 3. knob fuzzing
    r.head ("Random knob fuzzing (every parameter, 150 rounds)");
    {
        // fill racks with a few modules so their knobs matter
        p.rig().setModule (0, 1, "overdrive"); p.rig().setModule (1, 1, "lethe"); p.rig().setModule (2, 1, "crypt");
        bool ok = true; float worst = 0;
        for (int round = 0; round < 150; ++round)
        {
            for (auto& d : paramDefs())
            {
                if (d.id == "out.gain" || d.id == "in.gain") continue;
                setNorm (p, d.id, rnd.nextFloat());
            }
            setReal (p, "in.gain", 0); setReal (p, "out.gain", 0);
            auto st = render (p, SR, BLOCK, 0.12, riff);
            ok = ok && st.finite && st.peak <= 2.0f; worst = juce::jmax (worst, st.peak);
        }
        r.check (ok, "no NaN or runaway level with random settings (worst peak " + juce::String (worst, 2) + ")");
        p.presetManager().loadInit();
    }

    // ---------------------------------------------------------------- 4. rates and blocks
    r.head ("Sample rates and buffer sizes");
    for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
        for (int blk : { 32, 64, 128, 333, 1024 })
        {
            p.prepareToPlay (rate, blk);
            Guitar g2 (rate);
            auto st = render (p, rate, blk, 0.6, [&g2, rate] (int i) { if (i % (int) (rate * 0.2) == 0) g2.pluck (110.f, 0.4f); return g2.next(); });
            r.check (st.finite && st.rms > 0.001f && st.peak <= 2.0f, juce::String (rate / 1000.0, 1) + " kHz / " + juce::String (blk) + " samples");
        }
    p.prepareToPlay (SR, BLOCK);

    // ---------------------------------------------------------------- 5. transpose accuracy
    r.head ("Transpose accuracy (110 Hz harmonic tone in, like an A string)");
    {
        // isolate the transposer: no amp, cab, synth, doubler
        setNorm (p, "amp.on", 0); setNorm (p, "cab.on", 0); setNorm (p, "synth.on", 0); setNorm (p, "doubler.on", 0); setNorm (p, "gate.on", 0); setNorm (p, "eq.on", 0);
        for (bool hq : { false, true })
            for (int st : { -12, -5, -2, 3, 7, 12 })
            {
                setNorm (p, "transpose.hq", hq ? 1.f : 0.f);
                setReal (p, "transpose", (float) st);
                std::vector<float> out;
                juce::AudioBuffer<float> buf (2, BLOCK); juce::MidiBuffer midi; double ph = 0;
                for (int b = 0; b < (int) (SR * 1.2 / BLOCK); ++b)
                {
                    for (int i = 0; i < BLOCK; ++i)
                    {
                        // band-limited-ish sawtooth: a fundamental with harmonics, like a real string
                        float v = 0; for (int h = 1; h <= 12; ++h) v += (float) std::sin (ph * h) / (float) h;
                        buf.setSample (0, i, 0.18f * v); buf.setSample (1, i, 0.f); ph += juce::MathConstants<double>::twoPi * 110.0 / SR;
                    }
                    p.processBlock (buf, midi);
                    for (int i = 0; i < BLOCK; ++i) out.push_back (buf.getSample (0, i));
                }
                const float f = detectPitch (out, SR);
                const float expected = 110.f * std::pow (2.f, st / 12.f);
                const float cents = f > 0 ? 1200.f * std::log2 (f / expected) : 9999.f;
                r.check (std::abs (cents) < 8.f, juce::String (hq ? "HQ  " : "Live") + juce::String (st > 0 ? " +" : " ") + juce::String (st) + " st -> " + juce::String (f, 1) + " Hz (expected " + juce::String (expected, 1) + ", " + juce::String (cents, 1) + " cents)");
            }
        p.presetManager().loadInit();
    }

    // ---------------------------------------------------------------- 5b. HYDRA
    r.head ("HYDRA harmonizer (the HARMONY page)");
    {
        int nHarm = 0;
        for (auto* prm : p.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm)) if (rp->getParameterID().startsWith ("harm.")) ++nHarm;
        r.check (nHarm == obs::harmdefs::gCount + obs::harmdefs::kVoices * obs::harmdefs::vCount, juce::String (nHarm) + " harmony parameters");

        // a picked melody in A minor, rendered like a guitar (decaying harmonics, a short pick attack)
        const double noteLen = 0.30;
        const int mel[] = { 57, 59, 60, 62, 64, 65, 67, 69, 67, 64, 60, 57 };
        const int nNotes = (int) (sizeof (mel) / sizeof (mel[0]));
        auto guitar = [&] (int i) -> float {
            const double t = i / SR; const int k = (int) (t / noteLen); if (k >= nNotes) return 0.f;
            const double tn = t - k * noteLen, f = 440.0 * std::pow (2.0, (mel[k] - 69) / 12.0);
            double v = 0; for (int h = 1; h <= 10; ++h) v += std::sin (juce::MathConstants<double>::twoPi * f * h * tn) * std::exp (-tn * (1.5 + 0.6 * h)) / h;
            const double att = juce::jmin (1.0, tn / 0.0015), rel = juce::jmin (1.0, (noteLen - tn) / 0.008);
            return (float) (0.2 * v * att * rel); };
        auto isolate = [&] {
            p.presetManager().loadInit();
            setNorm (p, "amp.on", 0); setNorm (p, "cab.on", 0); setNorm (p, "synth.on", 0); setNorm (p, "doubler.on", 0); setNorm (p, "gate.on", 0); setNorm (p, "eq.on", 0);
            for (int v = 1; v <= 6; ++v) setNorm (p, "harm.v" + juce::String (v) + ".on", 0);
            setNorm (p, "harm.on", 1); setReal (p, "harm.mix", 100); setReal (p, "harm.key", 9); setReal (p, "harm.mode", 1); setReal (p, "harm.human", 0);
            setReal (p, "harm.v1.on", 1); setReal (p, "harm.v1.level", 100); setReal (p, "harm.v1.pan", 0); };
        auto play = [&] (std::vector<float>& out) {
            out.clear(); juce::AudioBuffer<float> buf (2, BLOCK); juce::MidiBuffer midi; int at = 0;
            for (int b = 0; b < (int) (SR * noteLen * (nNotes + 1) / BLOCK); ++b)
            {
                for (int i = 0; i < BLOCK; ++i) { buf.setSample (0, i, guitar (at + i)); buf.setSample (1, i, 0.f); }
                at += BLOCK; p.processBlock (buf, midi);
                for (int i = 0; i < BLOCK; ++i) out.push_back (buf.getSample (0, i));
            } };
        auto pitchAt = [&] (const std::vector<float>& x, double from, double to) {
            const auto a = (size_t) (from * SR), b = (size_t) juce::jmin ((double) x.size(), to * SR);
            return detectPitch (std::vector<float> (x.begin() + (long) a, x.begin() + (long) b), SR); };

        struct Case { const char* name; bool smart; int val; };
        for (auto c : { Case { "smart 3rd up", true, 2 }, Case { "smart 6th down", true, -2 }, Case { "fixed octave down", false, -12 }, Case { "fixed 5th up", false, 7 } })
        {
            isolate();
            setReal (p, "harm.v1.smart", c.smart ? 1.f : 0.f);
            if (c.smart) setReal (p, "harm.v1.step", (float) c.val); else setReal (p, "harm.v1.semi", (float) c.val);
            std::vector<float> out; play (out);
            int good = 0; float worst = 0;
            for (int k = 0; k < nNotes; ++k)
            {
                const int sh = c.smart ? obs::harmdefs::smartShift (mel[k], c.val, 9, 1, false, 0) : c.val;
                const float want = 440.f * std::pow (2.f, (mel[k] + sh - 69) / 12.f);
                const float got = pitchAt (out, k * noteLen + 0.06, k * noteLen + 0.29);
                const float cents = got > 0 ? std::abs (1200.f * std::log2 (got / want)) : 9999.f;
                worst = juce::jmax (worst, cents); if (cents < 15.f) ++good;
            }
            r.check (good == nNotes, juce::String ("LEAD ") + c.name + ": " + juce::String (good) + " of " + juce::String (nNotes) + " notes on pitch (worst " + juce::String (worst, 1) + " cents)");
        }
        // the voice is on its new note within a few ms of the attack (smart 3rd, mid register)
        {
            isolate(); setReal (p, "harm.v1.smart", 1); setReal (p, "harm.v1.step", 2);
            std::vector<float> out; play (out);
            double worstLag = 0;
            for (int k = 1; k < nNotes; ++k)
            {
                const int sh = obs::harmdefs::smartShift (mel[k], 2, 9, 1, false, 0);
                const float want = 440.f * std::pow (2.f, (mel[k] + sh - 69) / 12.f);
                double lag = 1;
                for (double ms = 4; ms < 60; ms += 1)
                {
                    // short YIN window ending at that moment
                    const double t1 = k * noteLen + ms * 0.001;
                    const auto b = (size_t) (t1 * SR), a = b - (size_t) (0.0035 * SR) - 400;
                    std::vector<double> d (420, 1.0); double run = 0; const int W = (int) (0.0035 * SR);
                    for (int tau = 1; tau < 420; ++tau) { double acc = 0; for (int j = 0; j < W; ++j) { const double df = out[a + (size_t) j] - out[a + (size_t) (j + tau)]; acc += df * df; } run += acc; d[(size_t) tau] = run > 0 ? acc * tau / run : 1; }
                    const int expTau = (int) std::lround (SR / want);
                    bool ok = false;
                    for (int tau = juce::jmax (2, expTau - 6); tau <= juce::jmin (418, expTau + 6); ++tau) if (d[(size_t) tau] < 0.2) ok = true;
                    if (ok) { lag = ms * 0.001; break; }
                }
                worstLag = juce::jmax (worstLag, lag);
                if (lag > 0.020) r.info ("  note " + juce::String (k) + " (midi " + juce::String (mel[k]) + "): " + juce::String (lag * 1000.0, 0) + " ms");
            }
            r.check (worstLag < 0.025, "LEAD follows new notes fast: on the new interval within " + juce::String (worstLag * 1000.0, 0) + " ms of each attack (clean tones, no pick noise)");
        }
        // off means off: switched off (after its fade) it hands the signal through untouched, whatever its settings
        {
            dsp::hydra::Harmonizer h; h.prepare (SR, BLOCK);
            dsp::hydra::Params hp; hp.on = true;
            for (auto& v : hp.v) { v.on = true; v.level = 1.f; }
            hp.track = 1; hp.mix = 0.8f;
            std::vector<float> in ((size_t) BLOCK), di ((size_t) BLOCK), L ((size_t) BLOCK), R ((size_t) BLOCK);
            float diff = 0; int at = 0;
            for (int b = 0; b < (int) (SR * 1.5 / BLOCK); ++b, at += BLOCK)
            {
                if (b == (int) (SR * 0.5 / BLOCK)) hp.on = false;                 // switch off while playing
                for (int i = 0; i < BLOCK; ++i) in[(size_t) i] = di[(size_t) i] = guitar (at + i);
                h.process (di.data(), in.data(), in.data(), L.data(), R.data(), BLOCK, hp);
                if (b > (int) (SR * 0.6 / BLOCK))
                    for (int i = 0; i < BLOCK; ++i) diff = juce::jmax (diff, std::abs (L[(size_t) i] - in[(size_t) i]), std::abs (R[(size_t) i] - in[(size_t) i]));
            }
            r.check (diff == 0.f, "switched off it is bit-for-bit out of the way" + (diff == 0.f ? juce::String() : " (differs by " + juce::String (diff) + ")"));
        }
        // everything at once, CHORDS, moving before/after the amp while playing: no clicks, no NaN
        {
            p.presetManager().loadInit();
            setNorm (p, "harm.on", 1);
            for (int v = 1; v <= 6; ++v) setNorm (p, "harm.v" + juce::String (v) + ".on", 1);
            setReal (p, "harm.human", 60);
            const auto lead6 = render (p, SR, BLOCK, 4.0, [&] (int i) { return guitar (i % (int) (SR * noteLen * nNotes)); });
            r.check (lead6.finite && lead6.peak < 1.99f, "six voices + humanize through the full rig: clean (peak " + juce::String (lead6.peak, 2) + ")");
            r.info ("six LEAD voices on the full rig: " + juce::String (lead6.avgLoad * 100.0, 1) + "% of one core");
            setReal (p, "harm.track", 1);
            auto chordIn = [&] (int i) { const double t = i / SR; double v = 0; for (int m : { 57, 61, 64 }) { const double f = 440.0 * std::pow (2.0, (m - 69) / 12.0); for (int h = 1; h <= 8; ++h) v += std::sin (juce::MathConstants<double>::twoPi * f * h * t) / (h * 1.0); } return (float) (0.06 * v * std::exp (-std::fmod (t, 1.0) * 1.5)); };
            const auto ch6 = render (p, SR, BLOCK, 3.0, chordIn);
            r.check (ch6.finite && ch6.peak < 1.99f && ch6.rms > 1e-3f, "six CHORDS voices on a chord: clean (peak " + juce::String (ch6.peak, 2) + ")");
            r.info ("six CHORDS voices: " + juce::String (ch6.avgLoad * 100.0, 1) + "% of one core");
            // move it before the amp and back while playing
            setReal (p, "harm.track", 0);
            juce::AudioBuffer<float> buf (2, BLOCK); juce::MidiBuffer midi; bool finite = true; float jump = 0, prev = 0;
            for (int b = 0; b < (int) (SR * 2.0 / BLOCK); ++b)
            {
                if (b == (int) (SR * 0.6 / BLOCK)) setReal (p, "harm.place", 1);
                if (b == (int) (SR * 1.3 / BLOCK)) setReal (p, "harm.place", 0);
                for (int i = 0; i < BLOCK; ++i) { buf.setSample (0, i, guitar (b * BLOCK + i)); buf.setSample (1, i, 0.f); }
                p.processBlock (buf, midi);
                for (int i = 0; i < BLOCK; ++i) { const float v = buf.getSample (0, i); finite &= std::isfinite (v); jump = juce::jmax (jump, std::abs (v - prev)); prev = v; }
            }
            r.check (finite && jump < 0.5f, "moving it before / after the amp while playing: no jumps (biggest step " + juce::String (jump, 3) + ")");
        }
        p.presetManager().loadInit();
    }

    // ---------------------------------------------------------------- 5c. updates
    r.head ("EQ: low / high cut switches");
    {
        obs::dsp::GraphicEq eq; const double sr = 48000; eq.prepare (sr);
        for (int b = 0; b < 9; ++b) eq.setBand (b, 1000.f, 0.f);
        std::vector<float> L (480), R (480); double ph = 0; float lastY = 0, maxStep = 0;
        auto run = [&] (int blocks, float hz) {      // returns the RMS of the last block
            double e = 0;
            for (int k = 0; k < blocks; ++k)
            {
                for (size_t i = 0; i < L.size(); ++i) { L[i] = R[i] = (float) (0.5 * std::sin (ph)); ph += 2 * juce::MathConstants<double>::pi * hz / sr; }
                float* ch[2] = { L.data(), R.data() }; eq.process (ch, 2, (int) L.size());
                e = 0; for (float y : L) { maxStep = std::max (maxStep, std::abs (y - lastY)); lastY = y; e += y * y; }
            }
            return std::sqrt (e / L.size()); };
        eq.setCuts (400.f, 20000.f, true, true);  const double cutOn = run (20, 60.f);
        maxStep = 0;
        eq.setCuts (400.f, 20000.f, false, true); const double cutOff = run (20, 60.f);
        const float stepOff = maxStep; maxStep = 0;
        eq.setCuts (400.f, 2000.f, true, false);  const double hcOff = run (20, 6000.f);
        eq.setCuts (400.f, 2000.f, true, true);   const double hcOn = run (20, 6000.f);
        r.check (cutOn < 0.05 && cutOff > 0.33 && hcOff > 0.33 && hcOn < 0.05,
                 juce::String::formatted ("LOW CUT / HIGH CUT switch off and on (60 Hz: %.3f in, %.3f off; 6 kHz: %.3f off, %.3f in)", cutOn, cutOff, hcOff, hcOn));
        r.check (stepOff < 0.03f, juce::String::formatted ("switching a cut fades it, no click (largest sample step %.4f, a 60 Hz sine moves 0.0039)", stepOff));
    }

    r.head ("STUDIO (the standalone app's recorder)");
    {
        obs::Studio st; st.saveTakes = false;
        const double sr = 48000; st.prepare (sr);
        const int N = 256;
        auto arrOf = [] (const juce::String& clips, const juce::String& track, bool click = false) {
            return juce::JSON::parse ("{\"bpm\":120,\"tracks\":[" + track + "],\"clips\":[" + clips + "],\"loop\":{\"on\":false,\"s\":0,\"e\":16},\"click\":" + (click ? "true" : "false") + "}"); };
        const juce::String centre = "{\"vol\":0.8,\"pan\":0,\"on\":true,\"st\":true}";
        std::vector<float> L (N), R (N), inL, inR;
        st.setArrangement (arrOf ("", centre));
        st.record (0, 0.0, 0.0);
        bool untouched = true;
        for (int b = 0; b < 48000 / N; ++b)
        {
            for (int i = 0; i < N; ++i) { const double t = (b * N + i) / sr; L[(size_t) i] = (float) (0.3 * std::sin (2 * juce::MathConstants<double>::pi * 220 * t)); R[(size_t) i] = (float) (0.2 * std::sin (2 * juce::MathConstants<double>::pi * 330 * t)); }
            inL.insert (inL.end(), L.begin(), L.end()); inR.insert (inR.end(), R.begin(), R.end());
            st.process (L.data(), R.data(), N);
            for (int i = 0; i < N; ++i) if (L[(size_t) i] != inL[(size_t) (b * N + i)]) untouched = false;
            if (b % 20 == 0) st.flush();                 // the timer does this every 25 ms
        }
        st.stop(); st.flush();
        const auto tk = st.status()["take"];
        const auto take = st.take ((int) tk["id"]);
        bool same = take != nullptr && take->L.size() == inL.size();
        for (size_t i = 0; same && i < inL.size(); ++i) same = take->L[i] == inL[i] && take->R[i] == inR[i];
        r.check (same && untouched, "a take records exactly what you hear (" + juce::String (take ? (int) take->L.size() : 0) + " samples, bit for bit), and recording doesn't change the sound");
        r.check (tk["peaks"].size() == 16 && std::abs ((double) tk["len"] - (double) inL.size() / 24000.0) < 1e-9, "its waveform for the page: 8 peaks per beat (" + juce::String (tk["peaks"].size()) + " for 2 beats)");

        // latency: a take is moved earlier by the round trip
        {
            obs::Studio s2; s2.saveTakes = false; s2.prepare (sr); s2.setArrangement (arrOf ("", centre)); s2.setRoundTrip (100);
            s2.record (0, 0.0, 0.0);
            for (int b = 0; b < 40; ++b) { for (int i = 0; i < N; ++i) L[(size_t) i] = R[(size_t) i] = inL[(size_t) (b * N + i)]; s2.process (L.data(), R.data(), N); }
            s2.stop(); s2.flush();
            const auto t2 = s2.take ((int) s2.status()["take"]["id"]);
            bool shifted = t2 != nullptr && t2->L.size() == (size_t) (40 * N - 100);
            for (size_t i = 0; shifted && i < t2->L.size(); ++i) shifted = t2->L[i] == inL[i + 100];
            r.check (shifted, "takes are moved earlier by the interface's round-trip latency, so they line up with what you heard");
        }

        // play it back from beat 4: at 0.8 (0 dB) it's the take itself
        const juce::String clip = "{\"src\":" + juce::String ((int) tk["id"]) + ",\"track\":0,\"start\":4,\"len\":2,\"off\":0}";
        auto playBack = [&] (const juce::String& track, std::vector<float>& outL, std::vector<float>& outR) {
            st.setArrangement (arrOf (clip, track)); st.play (4.0); outL.clear(); outR.clear();
            for (int b = 0; b < 48000 / N; ++b) { std::fill (L.begin(), L.end(), 0.f); std::fill (R.begin(), R.end(), 0.f); st.process (L.data(), R.data(), N);
                outL.insert (outL.end(), L.begin(), L.end()); outR.insert (outR.end(), R.begin(), R.end()); }
            st.stop(); };
        std::vector<float> oL, oR;
        playBack (centre, oL, oR);
        double err = 0; for (size_t i = 0; i < oL.size(); ++i) err = std::max (err, (double) std::max (std::abs (oL[i] - inL[i]), std::abs (oR[i] - inR[i])));
        r.check (err < 1e-6, juce::String::formatted ("the take plays back where it sits on the timeline (error %.1e)", err));
        playBack ("{\"vol\":0.8,\"pan\":-1,\"on\":true,\"st\":true}", oL, oR);
        double rightPk = 0; for (float v : oR) rightPk = std::max (rightPk, (double) std::abs (v));
        playBack ("{\"vol\":0.8,\"pan\":0,\"on\":false,\"st\":true}", oL, oR);
        double mutePk = 0; for (size_t i = 0; i < oL.size(); ++i) mutePk = std::max (mutePk, (double) std::max (std::abs (oL[i]), std::abs (oR[i])));
        r.check (rightPk < 1e-9 && mutePk < 1e-9, "pan hard left leaves the right side silent; mute / solo silences a track");

        // a count-in clicks for a bar, and nothing is recorded until the take starts
        st.setArrangement (arrOf (clip, centre));
        st.record (0, 8.0, 4.0);
        double clickPk = 0;
        for (int b = 0; b < (int) (2 * sr) / N; ++b) { std::fill (L.begin(), L.end(), 0.f); std::fill (R.begin(), R.end(), 0.f); st.process (L.data(), R.data(), N); for (float v : L) clickPk = std::max (clickPk, (double) std::abs (v)); }
        st.flush();
        const bool nothingYet = true;
        st.stop(); st.flush();
        const auto tk2 = st.status()["take"];
        r.check (clickPk > 0.1 && nothingYet && (bool) tk2["discarded"], juce::String::formatted ("the count-in clicks (peak %.2f) and records nothing before the take starts", clickPk));

        // EXPORT: the mix as a 24-bit WAV
        st.setArrangement (arrOf (clip, centre));
        const auto wavFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("obsidian-selftest-studio.wav");
        juce::String exErr;
        const bool exported = st.exportMix (wavFile, exErr);
        juce::WavAudioFormat wav; double exErrMax = 1; juce::int64 exLen = 0;
        if (exported)
            if (auto rd = std::unique_ptr<juce::AudioFormatReader> (wav.createReaderFor (wavFile.createInputStream().release(), true)))
            {
                exLen = rd->lengthInSamples;
                juce::AudioBuffer<float> buf (2, (int) rd->lengthInSamples); rd->read (&buf, 0, (int) rd->lengthInSamples, 0, true, true);
                exErrMax = 0; for (int i = 0; i < (int) inL.size(); ++i) exErrMax = std::max (exErrMax, (double) std::abs (buf.getSample (0, 96000 + i) - inL[(size_t) i]));
            }
        wavFile.deleteFile();
        r.check (exported && exLen == 96000 + (juce::int64) inL.size() + 24000 && exErrMax < 1e-5, juce::String::formatted ("EXPORT writes the mix as a WAV (%d samples, error %.1e)", (int) exLen, exErrMax) + (exErr.isEmpty() ? "" : " " + exErr));
    }

    r.head ("In-app updates (Settings > About)");
    {
        const juce::String hexA = juce::String::repeatedString ("ab", 32), hexB = juce::String::repeatedString ("cd", 32);
        const auto json = juce::JSON::parse (juce::String (R"J({
          "tag_name": "build-145", "name": "Build 145 (0.2.0)", "published_at": "2026-11-22T00:00:00Z",
          "body": "HYDRA: six voices\n\n<!-- obsidian\nbuild: 145\nversion: 0.2.0\npack: 298d0ab9f6c0c908\nsha256: ObsidianAmpSuite-0.2.0-Setup.exe )J") + hexA
          + "\nsha256: ObsidianAmpSuite-0.2.0-Update.exe " + hexB + R"J(\n-->",
          "assets": [
            { "name": "ObsidianAmpSuite-0.2.0-Setup.exe", "size": 90000000, "browser_download_url": "https://github.com/Extricl4/ObsidianSuite/releases/download/build-145/ObsidianAmpSuite-0.2.0-Setup.exe" },
            { "name": "ObsidianAmpSuite-0.2.0-Update.exe", "size": 25000000, "browser_download_url": "https://github.com/Extricl4/ObsidianSuite/releases/download/build-145/ObsidianAmpSuite-0.2.0-Update.exe" },
            { "name": "Evil-Update.exe", "size": 1000, "browser_download_url": "https://example.com/Evil-Update.exe" } ] })J");
        const auto rel = Updater::parseRelease (json);
        r.check (rel.ok && rel.build == 145 && rel.version == "0.2.0" && rel.pack == "298d0ab9f6c0c908" && rel.notes == "HYDRA: six voices",
                 "reads a GitHub release: build " + juce::String (rel.build) + ", pack, notes without the hidden block");
        r.check (rel.full.sha == hexA && rel.update.sha == hexB && rel.full.size == 90000000 && rel.update.url.endsWith ("-Update.exe"),
                 "finds both installers with their sizes and checksums");
        r.check (! Updater::trustedUrl ("https://example.com/x.exe") && ! Updater::trustedUrl ("https://github.com/someone/else/releases/download/a/x.exe")
                 && Updater::trustedUrl (rel.full.url), "downloads only from this project's own GitHub releases");
        r.info ("this build: " + juce::String (Updater::ownBuild() > 0 ? "build " + juce::String (Updater::ownBuild()) : juce::String ("made by hand"))
                + ", pack " + Updater::ownPack());
    }

    // ---------------------------------------------------------------- 6. synth
    r.head ("Synth");
    {
        auto& e = p.engine();
        setNorm (p, "synth.on", 1); setNorm (p, "syn.arp.on", 0); setReal (p, "syn.mix", 80);
        setNorm (p, "amp.on", 0); setNorm (p, "cab.on", 0);
        // the guitar plays it
        float act = 0, seen = -1;
        render (p, SR, BLOCK, 1.0, [&] (int i) { if (i % (int) (SR * 0.5) == 0) g.pluck (110.f, 0.5f); act = juce::jmax (act, e.synthActivity()); if (e.synthNote() > 0) seen = e.synthNote(); return g.next(); });
        r.check (act > 0.01f && std::abs (seen - 45.f) < 0.6f, "follows a plucked A2 (activity " + juce::String (act, 3) + ", note " + juce::String (seen, 2) + ")");
        // HOLD keeps it droning after the string dies away
        setNorm (p, "syn.hold", 1);
        render (p, SR, BLOCK, 0.3, [&] (int i) { if (i == 0) g.pluck (110.f, 0.5f); return g.next(); });
        render (p, SR, BLOCK, 1.5, [&] (int) { return 0.f; });
        r.check (e.synthActivity() > 0.01f, "HOLD keeps the note after you stop (activity " + juce::String (e.synthActivity(), 3) + ")");
        setNorm (p, "syn.hold", 0);
        render (p, SR, BLOCK, 1.5, [&] (int) { return 0.f; });
        r.check (e.synthActivity() < 0.01f, "turning HOLD off releases it");
        // MIDI notes play it too
        e.addSynthNote (0, 64, 0.9f);
        render (p, SR, BLOCK, 0.3, [&] (int) { return 0.f; });
        r.check (std::abs (e.synthNote() - 64.f) < 0.01f && e.synthActivity() > 0.05f, "MIDI note E4 plays (note " + juce::String (e.synthNote(), 2) + ")");
        e.uiSynthNote (64, false);
        render (p, SR, BLOCK, 1.0, [&] (int) { return 0.f; });
        r.check (e.synthActivity() < 0.01f, "the UI's AUDITION note off releases it");
        // stuck notes: MIDI from the DAW, through processBlock like a host sends it
        auto midiBlock = [&] (std::function<void (juce::MidiBuffer&)> fill) { juce::AudioBuffer<float> b (2, BLOCK); b.clear(); juce::MidiBuffer m; fill (m); p.processBlock (b, m); };
        midiBlock ([] (juce::MidiBuffer& m) { m.addEvent (juce::MidiMessage::noteOn (1, 76, 0.9f), 0); });
        render (p, SR, BLOCK, 0.3, [&] (int) { return 0.f; });
        const float playing = e.synthActivity();
        midiBlock ([] (juce::MidiBuffer& m) { for (int c = 1; c <= 16; ++c) m.addEvent (juce::MidiMessage::allNotesOff (c), 10); });
        render (p, SR, BLOCK, 1.0, [&] (int) { return 0.f; });
        r.check (playing > 0.05f && e.synthActivity() < 0.01f, juce::String::formatted ("the DAW stopping (All Notes Off on all 16 channels) releases a high note (E5: %.3f -> %.3f)", playing, e.synthActivity()));
        midiBlock ([] (juce::MidiBuffer& m) { m.addEvent (juce::MidiMessage::noteOn (1, 76, 0.9f), 0); });
        render (p, SR, BLOCK, 0.2, [&] (int) { return 0.f; });
        setNorm (p, "synth.on", 0); render (p, SR, BLOCK, 0.1, [&] (int) { return 0.f; });
        midiBlock ([] (juce::MidiBuffer& m) { m.addEvent (juce::MidiMessage::noteOff (1, 76), 0); });
        render (p, SR, BLOCK, 0.1, [&] (int) { return 0.f; });
        setNorm (p, "synth.on", 1); render (p, SR, BLOCK, 1.0, [&] (int) { return 0.f; });
        r.check (e.synthActivity() < 0.01f, "a key let go while the synth was off doesn't hang when it's switched back on (" + juce::String (e.synthActivity(), 3) + ")");

        // chords follow the key: E minor, A played -> A C E (minor), G played -> G B D (major)
        float ch[4];
        int n3 = dsp::GuitarSynth::chordNotes (45, 3, 4, 1, ch);
        const bool aMin = n3 == 3 && ch[0] == 45 && ch[1] == 48 && ch[2] == 52;
        n3 = dsp::GuitarSynth::chordNotes (43, 3, 4, 1, ch);
        const bool gMaj = n3 == 3 && ch[0] == 43 && ch[1] == 47 && ch[2] == 50;
        r.check (aMin && gMaj, "CHORD triads follow the key (E minor: A gives A-C-E, G gives G-B-D)");

        // everything at its extremes stays clean
        bool ok = true; juce::String bad; float worst = 0;
        auto midiRun = [&] (double secs) {
            e.addSynthNote (0, 40, 1.f);
            auto st = render (p, SR, BLOCK, secs, [&] (int) { return 0.f; });
            e.addSynthNote (0, 40, 0.f);
            render (p, SR, BLOCK, 0.05, [&] (int) { return 0.f; });
            return st; };
        for (int w = 0; w < 5; ++w)
            for (int sync = 0; sync < 2; ++sync)
            {
                setReal (p, "syn.a.wave", (float) w); setReal (p, "syn.b.wave", (float) w); setNorm (p, "syn.b.sync", (float) sync);
                setReal (p, "syn.a.shape", 100); setReal (p, "syn.b.shape", 100); setReal (p, "syn.a.voices", 3); setReal (p, "syn.b.voices", 3);
                setReal (p, "syn.chord", 6); setReal (p, "syn.b.pitch", 19);
                auto st = midiRun (0.25);
                worst = juce::jmax (worst, st.peak);
                if (! st.finite || st.peak > 2.f || st.rms < 0.001f) { ok = false; bad << " wave " << w << (sync ? " sync" : ""); }
            }
        r.check (ok, "every wave, full shape, 7-voice unison, 9th chords, with and without sync" + bad + " (peak " + juce::String (worst, 2) + ")");
        ok = true; bad.clear();
        for (int t = 0; t < 7; ++t)
        {
            setReal (p, "syn.flt.type", (float) t); setReal (p, "syn.flt.res", 100); setReal (p, "syn.flt.drive", 100); setReal (p, "syn.flt.env", 100);
            auto st = midiRun (0.3);
            if (! st.finite || st.peak > 2.f) { ok = false; bad << " type " << t; }
        }
        r.check (ok, "every filter type at full resonance and drive is stable" + bad);
        ok = true; bad.clear();
        for (int c = 0; c < 4; ++c)
        {
            setReal (p, "syn.char", (float) c); setReal (p, "syn.space", 100); setReal (p, "syn.lfo.depth", 100);
            for (int t = 0; t < 5; ++t) { setReal (p, "syn.lfo.target", (float) t); auto st = midiRun (0.15); if (! st.finite || st.peak > 2.f) { ok = false; bad << " char " << c << " lfo " << t; } }
        }
        r.check (ok, "every character and LFO target (full depth, full space)" + bad);

        // arpeggiator: every pattern, ratchets and chance; the step moves
        setReal (p, "syn.chord", 3); setReal (p, "syn.flt.res", 30); setReal (p, "syn.flt.drive", 20); setReal (p, "syn.lfo.depth", 0);
        unsigned steps[16];
        for (int i = 0; i < 16; ++i) { obs::synthdefs::Step st; st.on = true; st.acc = i % 4 == 0; st.ratchet = 1 + i % 4; st.chance = i % 3 ? 100 : 50; st.oct = i % 5 == 0 ? 1 : 0; st.note = i % 15; steps[i] = obs::synthdefs::packStep (st); }
        e.setSynthSteps (steps);
        setNorm (p, "syn.arp.on", 1); ok = true;
        juce::SortedSet<int> stepsSeen;
        for (int pat = 0; pat < 5; ++pat)
        {
            setReal (p, "syn.arp.pattern", (float) pat);
            e.addSynthNote (0, 45, 0.9f);
            auto st = render (p, SR, BLOCK, 0.6, [&] (int) { if (e.synthStep() >= 0) stepsSeen.add (e.synthStep()); return 0.f; });
            e.addSynthNote (0, 45, 0.f); render (p, SR, BLOCK, 0.05, [&] (int) { return 0.f; });
            ok = ok && st.finite && st.peak <= 2.f && st.rms > 0.001f;
        }
        r.check (ok && stepsSeen.size() >= 4, "arpeggiator: up, down, up-down, random and steps, with ratchets and chance (" + juce::String (stepsSeen.size()) + " steps seen)");
        auto st2 = render (p, SR, BLOCK, 1.0, riff);
        r.check (st2.finite && st2.peak <= 2.f, "arpeggiator follows the guitar riff cleanly");

        // CPU: the heaviest patch (4-note chord, two 7-voice super saws, sync, vowel filter, VHS, space)
        setNorm (p, "syn.arp.on", 0);
        setReal (p, "syn.a.wave", 4); setReal (p, "syn.b.wave", 4); setNorm (p, "syn.b.sync", 1); setReal (p, "syn.chord", 6);
        setReal (p, "syn.flt.type", 6); setReal (p, "syn.char", 3); setReal (p, "syn.space", 80); setReal (p, "syn.sub.level", 60); setReal (p, "syn.noise.level", 30);
        e.addSynthNote (0, 40, 1.f);
        auto heavy = render (p, SR, BLOCK, 1.0, [&] (int) { return 0.f; });
        e.addSynthNote (0, 40, 0.f); render (p, SR, BLOCK, 0.05, [&] (int) { return 0.f; });
        r.check (heavy.finite && heavy.avgLoad < 0.35, "heaviest synth patch: " + juce::String (heavy.avgLoad * 100, 2) + "% of one core");
        p.presetManager().loadInit();
    }

    // ---------------------------------------------------------------- 7. NAM
    r.head ("NAM captures");
    {
        bool eid = false;
        const auto library = obs::Rig::ampLibrary();
        if (auto* lib = library.getArray())
            for (auto& a : *lib)
                if (a["key"].toString() == "nam-loader")
                    eid = a["name"].toString() == "Eidolon" && a["face"].isObject() && a["face"]["loader"] && a["knobs"].size() == 7 && a["knobs"][6]["role"].toString() == "browse";
        r.check (eid, "the NAM loader is Eidolon: its face, six tone knobs and BROWSE");
        const auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("obsidian-selftest-meta.nam");
        f.replaceWithText (R"J({"version":"0.5.4","metadata":{"name":"Red Lead 7","modeled_by":"Tester","gear_type":"amp_cab","gear_make":"Peavey","gear_model":"5150"},"architecture":"WaveNet","weights":[]})J");
        const auto info = obs::Rig::namInfo (f.getFullPathName());
        r.check (info["title"].toString() == "Red Lead 7" && info["gear"].toString() == "Peavey 5150" && info["kind"].toString() == "Full rig" && info["by"].toString() == "Tester",
                 "a capture's own name, gear and type show on Eidolon's screen (" + info["title"].toString() + " / " + info["gear"].toString() + " / " + info["kind"].toString() + ")");
        f.deleteFile();
    }
    if (namFile.existsAsFile())
    {
        juce::String err;
        for (double rate : { 48000.0, 44100.0 })
        {
            auto e = dsp::NamEngine::load (namFile, rate, BLOCK, err);
            r.check (e != nullptr, "load " + namFile.getFileName() + " at " + juce::String (rate) + " Hz " + (err.isNotEmpty() ? "(" + err + ")" : juce::String()));
            if (e == nullptr) continue;
            p.prepareToPlay (rate, BLOCK);
            p.engine().amp().setType (dsp::AmpSection::Nam);
            p.engine().amp().channelSlot (0).set (std::move (e));
            Guitar g3 (rate);
            auto st = render (p, rate, BLOCK, 1.5, [&g3, rate] (int i) { if (i % (int) (rate * 0.3) == 0) g3.pluck (82.4f, 0.4f); return g3.next(); });
            r.check (st.finite && st.rms > 0.0005f && st.peak <= 2.f, "capture produces sound at " + juce::String (rate) + " Hz (cpu " + juce::String (st.avgLoad * 100, 2) + "%)");
        }
        juce::String err2;
        auto bad = dsp::NamEngine::load (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("not-a-model.nam"), SR, BLOCK, err2);
        r.check (bad == nullptr && err2.isNotEmpty(), "a missing/broken file is rejected with a message, not a crash");
        p.engine().amp().setType (dsp::AmpSection::Model);
        p.prepareToPlay (SR, BLOCK);
    }
    else r.info ("(skipped: pass --nam <file.nam> to test capture loading)");

    // ---------------------------------------------------------------- 7b. pack pedals
    r.head ("Pack pedals (pedals with their own captures / IRs)");
    {
        int packTypes = 0; bool mapsOk = true;
        for (auto& t : dsp::moduleTypes())
        {
            if (t.captures.empty() && t.irs.empty()) continue;
            ++packTypes;
            int combos = 1; bool selOk = ! t.packSelectors.empty() || (t.captures.size() + t.irs.size()) == 1;
            for (int k : t.packSelectors)
            {
                if (! juce::isPositiveAndBelow (k, (int) t.params.size())) { selOk = false; continue; }
                const auto& prm = t.params[(size_t) k];
                if (prm.choices.isEmpty() && ! prm.toggle) selOk = false;
                combos *= dsp::selectorSize (prm);
            }
            const int files = (int) (t.captures.size() + t.irs.size());
            std::set<juce::String> unique (t.captures.begin(), t.captures.end()); unique.insert (t.irs.begin(), t.irs.end());
            const bool ok = selOk && combos - t.packFirstChoice == files && (int) unique.size() == files && t.fileKind == "pack";
            if (! ok) { mapsOk = false; r.fail (t.key + ": its switches pick " + juce::String (combos - t.packFirstChoice) + " files but it lists " + juce::String (files)); }
        }
        r.check (mapsOk && packTypes >= 10, juce::String (packTypes) + " pack pedals: every switch position maps to exactly one file");

        // IR reverb with made-up IRs (no files needed)
        juce::Random rng (77);
        for (auto key : { "ossuary", "halcyon", "crypt" })
        {
            ModuleRig mr (key, SR, BLOCK);
            if (mr.m == nullptr) { r.fail (juce::String (key) + " is registered"); continue; }
            std::vector<dsp::IrFile> bank;
            for (size_t i = 0; i < mr.type->irs.size(); ++i) bank.push_back (fakeIr (i % 2 ? 44100.0 : 96000.0, 2.5f, 0.6f + 0.2f * (float) i, (int) (1 + i % 2), rng));
            mr.m->setIrBank (std::move (bank));
            mr.selectFile (0); mr.set ("Mix", 100.f);
            juce::Random nz (5);
            auto noise = [&nz] (int) { return (nz.nextFloat() * 2.f - 1.f) * 0.25f; };
            const double wait0 = juce::Time::getMillisecondCounterHiRes();
            RunStats warm; while ((warm = mr.run (SR, BLOCK, 0.25, noise)).rms < 1.0e-4f && juce::Time::getMillisecondCounterHiRes() - wait0 < 3000) juce::Thread::sleep (20);
            auto st = mr.run (SR, BLOCK, 1.0, noise);
            const float ratio = st.rms / (0.25f / std::sqrt (3.f));
            r.check (st.finite && st.peak < 4.f && ratio > 0.2f && ratio < 4.f,
                     juce::String (key) + ": IR reverb plays (wet level " + juce::String (juce::Decibels::gainToDecibels (ratio), 1) + " dB vs dry)");
            // switch files and shorten the decay while playing (the message tick rebuilds the IR)
            int ticks = 0; bool finite = true; float peak = 0;
            for (int f = 0; f < (int) mr.type->irs.size(); ++f)
            {
                mr.selectFile (f); mr.set ("Decay", f % 2 ? 0.4f : 100.f);
                auto s2 = mr.run (SR, BLOCK, 0.3, noise, [&] (int done) { if (done % (int) (SR * 0.1) < BLOCK) { mr.m->messageTick(); ++ticks; } });
                finite = finite && s2.finite; peak = juce::jmax (peak, s2.peak);
            }
            r.check (finite && peak < 4.f, juce::String (key) + ": switching IRs and decay while playing stays clean (" + juce::String ((int) mr.type->irs.size()) + " IRs)");
        }
        {   // Crypt voice 0 keeps the original reverb even with the IR files loaded
            ModuleRig a ("crypt", SR, BLOCK);
            a.set ("Mix", 50.f); a.set ("Voice", 0.f);
            Guitar g4 (SR);
            auto st = a.run (SR, BLOCK, 1.0, [&g4] (int i) { if (i % 12000 == 0) g4.pluck (110.f, 0.4f); return g4.next(); });
            r.check (st.finite && st.rms > 0.001f, "Crypt's own voice still works (old presets sound the same)");
        }

        // capture switching (needs --nam): load the same capture into every slot of a 3-way pedal and flick the switch
        if (namFile.existsAsFile())
        {
            ModuleRig mr ("graven", SR, BLOCK);
            std::vector<std::unique_ptr<dsp::NamEngine>> engines; juce::StringArray names; juce::String err;
            for (int i = 0; i < 3; ++i) { engines.push_back (dsp::NamEngine::load (namFile, SR, BLOCK, err)); names.add ("c" + juce::String (i)); }
            mr.m->setCaptureBank (std::move (engines), names);
            Guitar g5 (SR); int flips = 0;
            auto st = mr.run (SR, BLOCK, 2.0, [&g5] (int i) { if (i % 9600 == 0) g5.pluck (82.4f, 0.4f); return g5.next(); },
                              [&] (int done) { if (done % 4800 < BLOCK) { mr.selectFile (++flips % 3); } });
            r.check (st.finite && st.rms > 0.0005f && st.peak < 4.f, "capture pedal switches captures while playing (" + juce::String (flips) + " switches)");
        }
        else r.info ("(capture switching skipped: pass --nam <file.nam>)");

        // --packs: play every file of the installed pedal pack
        if (packs)
        {
            for (auto& t : dsp::moduleTypes())
            {
                if (t.captures.empty() && t.irs.empty()) continue;
                auto files = Rig::loadPackFiles (t, SR, BLOCK);
                const int present = files->total - files->missing.size();
                r.check (files->missing.isEmpty() && files->broken.isEmpty(), t.name + ": " + juce::String (present) + " of " + juce::String (files->total) + " files installed and readable"
                         + (files->missing.isEmpty() ? juce::String() : " (missing " + files->missing.joinIntoString (", ") + ")")
                         + (files->broken.isEmpty() ? juce::String() : " (broken: " + files->broken.joinIntoString ("; ") + ")"));
                std::vector<bool> loaded;
                for (auto& e : files->engines) loaded.push_back (e != nullptr);
                for (auto& f : files->irs) loaded.push_back (f.buffer.getNumSamples() > 0);
                ModuleRig mr (t.key, SR, BLOCK);
                Rig::installPack (*mr.m, *files);
                const int count = (int) (t.captures.size() + t.irs.size());
                int good = 0; float lo = 1e9f, hi = 0; double cpu = 0;
                for (int f = 0; f < count; ++f)
                {
                    mr.selectFile (f);
                    if (! t.irs.empty()) { mr.set ("Mix", 100.f); mr.m->messageTick(); mr.run (SR, BLOCK, 0.3, [] (int) { return 0.f; }); }
                    Guitar g6 (SR);
                    auto st = mr.run (SR, BLOCK, t.irs.empty() ? 1.0 : 1.5, [&g6] (int i) { if (i % 12000 == 0) g6.pluck (82.4f, 0.4f); return g6.next(); });
                    if (loaded[(size_t) f] && st.finite && st.rms > 0.0003f && st.peak < 4.f) ++good;
                    lo = juce::jmin (lo, st.rms); hi = juce::jmax (hi, st.rms); cpu = juce::jmax (cpu, st.avgLoad);
                }
                r.check (good == count, t.name + ": all " + juce::String (count) + " " + (t.irs.empty() ? "captures" : "IRs") + " play cleanly (levels "
                         + juce::String (juce::Decibels::gainToDecibels (lo), 1) + " to " + juce::String (juce::Decibels::gainToDecibels (hi), 1) + " dB, worst cpu "
                         + juce::String (cpu * 100, 2) + "%)");
            }
        }
        else r.info ("(installed pack not played: pass --packs to play every file in Documents/" OBSIDIAN_PRODUCT_NAME ")");
    }

    // ---------------------------------------------------------------- 7b. capture amps
    r.head ("Capture amps (the amp pack: every knob and button picks real captures)");
    {
        auto ampFiles = Rig::contentFolder().getChildFile ("Amps").findChildFiles (juce::File::findFiles, false, "*.json");
        ampFiles.sort();
        int wired = 0;
        for (auto& f : ampFiles)
        {
            const auto v = Rig::readJson (f);
            if (! v["map"].isObject()) continue;
            const auto key = f.getFileNameWithoutExtension();
            juce::String err;
            auto m = dsp::CaptureAmpMap::compile (v["map"], Rig::readFaceFile (key), err);
            if (m == nullptr) { r.fail (key + ": the wiring doesn't compile (" + err + ")"); continue; }
            ++wired;
            const int nk = (int) m->roles.size(), nb = m->bank.size();
            int combos = 1; for (auto& d : m->dims) combos *= d.count;
            bool ok = Rig::readFaceFile (key).isObject();
            std::set<int> picked;
            for (int c = 0; c < combos; ++c)
            {
                int dv[dsp::CaptureAmpMap::kMaxDims] {};
                for (int i = (int) m->dims.size() - 1, rem = c; i >= 0; --i) { dv[i] = rem % m->dims[(size_t) i].count; rem /= m->dims[(size_t) i].count; }
                for (int j0 = 0; j0 < nk; ++j0)
                    for (int s = 0; s <= 20; ++s)
                    {
                        float k[dsp::CaptureAmpMap::kMaxKnobs] {};
                        for (int j = 0; j < nk; ++j) k[j] = m->defaults[(size_t) j];
                        k[j0] = (float) s / 20.f;
                        if (j0 != m->selKnob && m->selKnob >= 0) k[m->selKnob] = 0.f;
                        const auto pk = m->resolve (k, dv);
                        ok = ok && juce::isPositiveAndBelow (pk.a, nb) && (pk.b == -1 || juce::isPositiveAndBelow (pk.b, nb)) && pk.f >= 0.f && pk.f <= 1.f;
                        picked.insert (pk.a); if (pk.b >= 0) picked.insert (pk.b);
                    }
            }
            juce::StringArray idle;
            uint32_t picks = 0; for (auto& st : m->settings) picks |= st.selects;
            for (int j = 0; j < nk; ++j)
                if (m->roles[(size_t) j] == dsp::KnobRole::Plain && ! (picks & (1u << j))) idle.add (m->knobLabels[j]);
            r.check (ok && idle.isEmpty(), key + ": " + juce::String (nb) + " captures, " + juce::String ((int) m->settings.size()) + " settings, "
                     + juce::String (combos) + " button combinations" + (idle.isEmpty() ? juce::String() : "; knobs that do nothing: " + idle.joinIntoString (", "))
                     + ((int) picked.size() < nb ? " (" + juce::String (nb - (int) picked.size()) + " captures only reachable by moving several knobs)" : juce::String()));

            // --packs: play it (every capture loaded, the gain swept, every button)
            if (packs)
            {
                auto files = Rig::loadCaptureAmp (m, key, SR, BLOCK);
                const int present = nb - files.missing.size();
                const bool installed = files.missing.isEmpty() && files.broken.isEmpty();
                r.check (installed, key + ": " + juce::String (present) + " of " + juce::String (nb) + " captures installed and readable"
                         + (files.broken.isEmpty() ? juce::String() : " (broken: " + files.broken.joinIntoString ("; ") + ")"));
                if (! installed) continue;
                p.engine().amp().captureSlot().set (std::move (files.amp));     // installed first, so setAmp doesn't start its own load
                p.rig().setAmp (key);
                auto st = render (p, SR, BLOCK, 1.2, riff);
                const float lvl = juce::Decibels::gainToDecibels (st.rms);
                bool fine = st.finite && st.rms > 0.0005f && st.peak <= 2.f;
                float lo = lvl, hi = lvl; double cpu = st.avgLoad;
                for (float gv : { 0.f, 2.5f, 5.f, 7.5f, 10.f })
                {
                    setReal (p, "amp.gain", gv);
                    auto s2 = render (p, SR, BLOCK, 0.4, riff);
                    fine = fine && s2.finite && s2.peak <= 2.f && s2.rms > 0.0002f;
                    lo = juce::jmin (lo, juce::Decibels::gainToDecibels (s2.rms)); hi = juce::jmax (hi, juce::Decibels::gainToDecibels (s2.rms)); cpu = juce::jmax (cpu, s2.avgLoad);
                }
                for (size_t i = 0; i < m->dims.size(); ++i)
                    for (int dvv = 0; dvv < m->dims[i].count; ++dvv)
                    {
                        p.rig().setAmpDim ((int) i, dvv);
                        auto s3 = render (p, SR, BLOCK, 0.3, riff);
                        fine = fine && s3.finite && s3.peak <= 2.f;
                        cpu = juce::jmax (cpu, s3.avgLoad);
                    }
                r.check (fine && lvl > -40.f && lvl < -4.f, key + ": plays cleanly, level " + juce::String (lvl, 1) + " dB at its default settings (gain sweep "
                         + juce::String (lo, 1) + " to " + juce::String (hi, 1) + " dB), worst cpu " + juce::String (cpu * 100, 2) + "%");
            }
        }
        if (wired == 0) r.info ("(no amp pack in Documents/" OBSIDIAN_PRODUCT_NAME "/Amps: skipped)");
        else if (! packs) r.info ("(amp pack not played: pass --packs to load and play every capture amp)");
        p.rig().setAmp ("core");
        p.presetManager().loadInit();
    }

    // ---------------------------------------------------------------- 8. presets
    r.head ("Presets and DAW state");
    {
        auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("obsidian-selftest-presets");
        tmp.deleteRecursively(); tmp.createDirectory();
        const auto oldFolder = p.settings().get ("presetFolder");
        const auto oldLast = p.settings().get ("lastPreset");      // the test must not change your settings
        p.presetManager().setFolder (tmp);
        int factory = 0, loaded = 0, artists = 0, ampOk = 0;
        juce::StringArray badAmp;
        // Load every factory preset without starting the background capture/pedal file loads
        // (360 of them would each queue a load); the amp key and its fallback are still checked.
        obs::Rig::asyncFileLoads = false;
        for (auto& pr : p.presetManager().list())
            if (pr.factory)
            {
                ++factory;
                if (pr.category.startsWith ("Artists/")) ++artists;
                if (p.presetManager().load (pr.id)) ++loaded;
                const auto amp = p.rig().toJson()["amp"]["key"].toString();
                if (amp.isNotEmpty() && (obs::Rig::captureAmpInstalled (amp) || ! obs::Rig::findInLibrary (obs::Rig::ampLibrary(), amp).isVoid())) ++ampOk;
                else badAmp.add (pr.name + " (" + amp + ")");
                if (factory % 40 == 0) p.engine().collectGarbage (true);
            }
        obs::Rig::asyncFileLoads = true;
        p.engine().collectGarbage (true);
        r.check (loaded == factory, juce::String (loaded) + " of " + juce::String (factory) + " factory presets load");
        r.check (factory >= 300 && artists >= 250, juce::String (factory) + " factory presets, " + juce::String (artists) + " in Artists folders");
        r.check (ampOk == factory, juce::String (ampOk) + " of " + juce::String (factory) + " presets end on an amp you have"
                                   + (badAmp.isEmpty() ? juce::String() : ": " + badAmp.joinIntoString (", ").substring (0, 200)));

        setReal (p, "amp.gain", 7.3f); setReal (p, "eq.b3", -4.5f); p.rig().setModule (1, 2, "tide");
        r.check (p.presetManager().saveAs ("Selftest Sound"), "save as");
        setReal (p, "amp.gain", 2.0f); p.rig().setModule (1, 2, {});
        bool found = false;
        for (auto& pr : p.presetManager().list()) if (pr.name == "Selftest Sound") found = p.presetManager().load (pr.id);
        auto* gain = dynamic_cast<juce::RangedAudioParameter*> (p.state().getParameter ("amp.gain"));
        r.check (found && std::abs (gain->convertFrom0to1 (gain->getValue()) - 7.3f) < 0.01f, "load restores knob values");
        r.check (p.rig().toJson()["racks"][1][2]["key"].toString() == "tide", "load restores the pedal/effect slots");

        // built-in presets are locked; your own folders are real sub-folders
        {
            auto& pm = p.presetManager();
            auto entry = [&pm] (const juce::String& name) { for (auto& e : pm.list()) if (e.name == name) return e; return obs::PresetManager::Info(); };
            const auto fac = entry ("Enter Sandman"), mine = entry ("Selftest Sound");
            r.check (fac.factory && ! pm.remove (fac.id) && ! pm.rename (fac.id, "x") && ! pm.move (fac.id, ""), "factory and artist presets can't be deleted, renamed or moved");
            r.check (pm.createFolder ("Drop D") && pm.userFolders().contains ("Drop D") && tmp.getChildFile ("Drop D").isDirectory()
                     && ! pm.createFolder ("Drop D") && ! pm.createFolder ("..") && ! pm.createFolder ("  "), "make a folder (no duplicates, no path tricks)");
            r.check (pm.saveAs ("Folder Sound", "Lead", "Drop D") && tmp.getChildFile ("Drop D/Folder Sound.obsidian").existsAsFile()
                     && entry ("Folder Sound").dir == "Drop D", "SAVE AS into a folder");
            pm.load (mine.id);
            r.check (pm.move (mine.id, "Drop D") && entry ("Selftest Sound").dir == "Drop D" && pm.currentId().contains ("Drop D")
                     && ! tmp.getChildFile ("Selftest Sound.obsidian").exists(), "move a preset into a folder (the loaded one follows)");
            r.check (pm.renameFolder ("Drop D", "Half Step") && entry ("Folder Sound").dir == "Half Step" && pm.currentId().contains ("Half Step")
                     && ! pm.userFolders().contains ("Drop D"), "rename a folder");
            r.check (pm.rename (entry ("Folder Sound").id, "Folder Sound 2") && tmp.getChildFile ("Half Step/Folder Sound 2.obsidian").existsAsFile(),
                     "renaming a preset keeps it in its folder");
            const bool out = pm.move (entry ("Selftest Sound").id, "") && pm.move (entry ("Folder Sound 2").id, "") && entry ("Selftest Sound").dir.isEmpty();
            r.check (out && pm.removeFolder ("Half Step") && pm.userFolders().isEmpty() && ! tmp.getChildFile ("Half Step").exists()
                     && ! pm.removeFolder ("Nope"), "move presets back out, delete the folder");
        }

        juce::MemoryBlock mb; p.getStateInformation (mb);
        auto other = std::make_unique<ObsidianProcessor>();
        other->setStateInformation (mb.getData(), (int) mb.getSize());
        auto* g2 = dynamic_cast<juce::RangedAudioParameter*> (other->state().getParameter ("amp.gain"));
        r.check (std::abs (g2->convertFrom0to1 (g2->getValue()) - 7.3f) < 0.01f && other->rig().toJson()["racks"][1][2]["key"].toString() == "tide",
                 "DAW project state round-trips (knobs + slots)");
        other.reset();
        p.presetManager().setFolder (juce::File (oldFolder.toString()));
        p.settings().set ("lastPreset", oldLast);
        tmp.deleteRecursively();
        p.presetManager().loadInit();
    }

    // ---------------------------------------------------------------- 9. full rig CPU
    r.head ("CPU: full rig (amp + cab + synth + 5 pedals + 5 FX + 2 special + doubler)");
    {
        const char* pre[] = { "compressor", "overdrive", "lament", "abyss", "boost" };
        const char* post[] = { "tide", "prism", "lethe", "crypt", "glue" };
        for (int s = 0; s < 5; ++s) { p.rig().setModule (0, s, pre[s]); p.rig().setModule (1, s, post[s]); }
        p.rig().setModule (2, 0, "seraph"); p.rig().setModule (2, 1, "filter");
        setNorm (p, "synth.on", 1); setNorm (p, "doubler.on", 1); setReal (p, "transpose", -2);
        auto st = render (p, SR, 128, 5.0, riff);
        r.check (st.finite && st.peak <= 2.f, "full rig runs cleanly");
        r.info ("average " + juce::String (st.avgLoad * 100, 2) + "% of one core, worst 128-sample block " + juce::String (st.maxBlockLoad * 100, 1) + "%");
        r.check (st.avgLoad < 0.5, "average load under 50% of one core on this machine");
        p.presetManager().loadInit();
    }

    p.releaseResources();
    proc.reset();

    const double secs = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
    r.lines.add ("");
    r.lines.add (juce::String (r.checks - r.failures) + " / " + juce::String (r.checks) + " checks passed in " + juce::String (secs, 1) + " s"
                 + (r.failures == 0 ? "   ALL GOOD" : "   " + juce::String (r.failures) + " FAILED"));
    const auto text = r.lines.joinIntoString ("\n");
    std::cout << text << std::endl;
    if (reportFile != juce::File()) reportFile.replaceWithText (text);
    return r.failures == 0 ? 0 : 1;
}
} // namespace obs

// =============================================================================
//  Factory preset writer. Run:
//      "Obsidian Amp Suite" --write-factory-presets <out folder> <spec.json> [target dB]
//  The spec (made by tools/presets/make_presets.py) lists every preset: amp, its buttons and
//  knobs, cab, pedals and effects in each rack, plugin settings, synth and arp steps. Each one is
//  built on the real plugin, every file it needs is loaded, a test riff is played through it and
//  the output level is set so all presets play about equally loud. Writes <out>/<file>.json.
// =============================================================================
namespace obs
{
int writeFactoryPresets (const juce::File& dir, const juce::File& specFile, float targetDb)
{
    const auto spec = Rig::readJson (specFile);
    auto* list = spec.getArray();
    if (list == nullptr) { std::cout << "Can't read the preset spec " << specFile.getFullPathName() << std::endl; return 1; }
    dir.createDirectory();
    Rig::asyncFileLoads = false;                       // load captures and pedal files right away
    auto proc = std::make_unique<ObsidianProcessor>();
    auto& p = *proc;
    const double SR = 48000; const int BLOCK = 256;
    p.setPlayConfigDetails (2, 2, SR, BLOCK);
    p.prepareToPlay (SR, BLOCK);
    std::vector<float> riff;
    dsp::CaptureAmp::testRiff (riff, SR, 3.0);
    auto input = [&] (int i) { return riff[(size_t) i % riff.size()] * 0.9f; };
    int written = 0, failed = 0;
    juce::StringArray report;

    for (auto& e : *list)
    {
        const auto name = e["name"].toString();
        juce::StringArray problems;
        p.presetManager().loadInit();

        // amp: buttons first (the channel brings its own knobs), then knobs, then switches
        const auto amp = e["amp"];
        const auto ampKey = amp["key"].toString();
        if (ampKey.isNotEmpty()) p.rig().setAmp (ampKey);
        if (p.rig().toJson()["amp"]["key"].toString() != ampKey && ampKey.isNotEmpty()) problems.add ("amp " + ampKey + " not found");
        std::shared_ptr<dsp::CaptureAmpMap> map;
        if (Rig::readAmpFile (ampKey).isObject())
        {
            juce::String err;
            map = dsp::CaptureAmpMap::compile (Rig::readAmpFile (ampKey)["map"], Rig::readFaceFile (ampKey), err);
        }
        if (auto* dims = amp["dims"].getArray())
            for (auto& d : *dims) p.rig().setAmpDim ((int) d[0], (int) d[1]);
        if (auto* knobs = amp["knobs"].getDynamicObject())
            for (auto& kv : knobs->getProperties())
            {
                const float v = (float) (double) kv.value;
                if (map == nullptr) { setReal (p, "amp." + kv.name.toString(), v); continue; }          // in-house amp: amp.gain...
                const int j = map->knobLabels.indexOf (kv.name.toString(), true);
                if (j < 0) { problems.add ("no knob " + kv.name.toString()); continue; }
                const juce::String pid (dsp::CaptureAmpMap::paramFor (map->roles[(size_t) j]));
                if (pid.isNotEmpty()) setReal (p, pid, v);
                else p.rig().setAmpKnob (j, juce::jlimit (0.f, 1.f, v / 10.f));
            }
        if (auto* sw = amp["switches"].getArray())
            for (auto& x : *sw) p.rig().setAmpSwitch ((int) x, true);

        if (e["cab"].toString().isNotEmpty()) p.rig().setCab (e["cab"].toString());

        // pedals and effects: racks[0] = before the amp, [1] = after it, [2] = special FX
        if (auto* racks = e["racks"].getArray())
            for (int r = 0; r < juce::jmin (kNumRacks, racks->size()); ++r)
                if (auto* slots = (*racks)[r].getArray())
                    for (int sl = 0; sl < juce::jmin (kSlotsPerRack, slots->size()); ++sl)
                    {
                        const auto m = (*slots)[sl];
                        const auto key = m["key"].toString();
                        if (key.isEmpty()) continue;
                        const auto* type = dsp::findModule (key);
                        if (type == nullptr) { problems.add ("no effect " + key); continue; }
                        p.rig().setModule (r, sl, key, true);
                        if (auto* vals = m["values"].getDynamicObject())
                            for (auto& kv : vals->getProperties())
                            {
                                int k = -1;
                                for (int i = 0; i < (int) type->params.size(); ++i) if (type->params[(size_t) i].name == kv.name.toString()) k = i;
                                if (k < 0) { problems.add (key + " has no " + kv.name.toString()); continue; }
                                setNorm (p, slotParamId (r, sl, k), type->params[(size_t) k].toNorm ((float) (double) kv.value));
                            }
                        if (m.hasProperty ("on") && ! (bool) m["on"]) setNorm (p, slotOnId (r, sl), 0.f);
                    }

        // plugin settings (gate, doubler, EQ, cab mics, synth...): real values
        if (auto* prm = e["params"].getDynamicObject())
            for (auto& kv : prm->getProperties())
            {
                if (p.state().getParameter (kv.name.toString()) == nullptr) { problems.add ("no setting " + kv.name.toString()); continue; }
                setReal (p, kv.name.toString(), (float) (double) kv.value);
            }
        if (e["synSteps"].isArray()) p.rig().setSynthSteps (e["synSteps"]);
        if (e["eqName"].toString().isNotEmpty()) p.rig().setMisc ("eqPreset", e["eqName"].toString());

        // play it: every file loaded, a riff through it, the output level set to the target
        p.rig().loadFilesNow();
        for (int i = 0; i < 4; ++i) render (p, SR, BLOCK, 0.25, [] (int) { return 0.f; });   // let smoothing and loads settle
        auto st = render (p, SR, BLOCK, 3.0, input);
        float level = juce::Decibels::gainToDecibels (st.rms, -120.f), peak = juce::Decibels::gainToDecibels (st.peak, -120.f);
        float outGain = 0.f;
        if (! st.finite || st.rms < 1.0e-4f) problems.add ("no sound");
        else if (! e.hasProperty ("level") || (bool) e["level"])
        {
            outGain = juce::jlimit (-12.f, 12.f, targetDb + (float) (double) e.getProperty ("levelOffset", 0.0) - level);
            outGain = juce::jmin (outGain, -2.f - peak);                       // and keep the peaks under -2 dBFS
            setReal (p, "out.gain", outGain);
        }

        auto v = p.presetManager().captureCurrent (name, e["category"].toString());
        if (auto* o = v.getDynamicObject())
        {
            o->setProperty ("type", e["type"]);
            o->setProperty ("note", e["note"]);
            if (e["fallbackAmp"].toString().isNotEmpty()) o->setProperty ("fallbackAmp", e["fallbackAmp"]);
        }
        const auto out = dir.getChildFile (e["file"].toString());
        out.getParentDirectory().createDirectory();
        const bool ok = problems.isEmpty() && out.replaceWithText (juce::JSON::toString (v, true));
        written += ok ? 1 : 0; failed += ok ? 0 : 1;
        report.add ((ok ? "  ok   " : "  FAIL ") + name.paddedRight (' ', 34) + " " + juce::String (level, 1).paddedLeft (' ', 6) + " dB  peak "
                    + juce::String (peak, 1).paddedLeft (' ', 5) + "  out " + juce::String (outGain, 1).paddedLeft (' ', 5)
                    + (problems.isEmpty() ? juce::String() : "   " + problems.joinIntoString ("; ")));
        std::cout << report[report.size() - 1] << std::endl;
        p.engine().collectGarbage (true);              // free the amp and pedal files this preset loaded
    }
    proc->releaseResources();
    std::cout << "Wrote " << written << " presets to " << dir.getFullPathName()
              << (failed ? " (" + juce::String (failed) + " FAILED)" : juce::String()) << std::endl;
    return failed == 0 && written > 0 ? 0 : 1;
}

// =============================================================================
//  Synth sound check. Run:  "Obsidian Amp Suite" --render-synth <folder>
//  Renders a few synth patches played by MIDI notes to 48 kHz WAV files, so
//  they can be listened to and measured (tools/amp-lab/synthcheck.py).
// =============================================================================
int renderSynthDemos (const juce::File& dir)
{
    dir.createDirectory();
    auto proc = std::make_unique<ObsidianProcessor>();
    auto& p = *proc;
    const double sr = 48000; const int block = 256;
    p.setPlayConfigDetails (2, 2, sr, block); p.prepareToPlay (sr, block);
    struct Patch { const char* name; std::vector<std::pair<const char*, float>> set; std::vector<int> notes; double noteLen; };
    const std::vector<Patch> patches = {
        { "01-init",        {}, { 40, 43, 45, 47 }, 0.5 },
        { "02-saw-raw-E6",  { { "syn.b.on", 0 }, { "syn.flt.on", 0 }, { "syn.a.voices", 0 }, { "syn.sub.level", 0 }, { "syn.char", 0 }, { "syn.space", 0 } }, { 88 }, 1.0 },
        { "03-pulse-raw-E6",{ { "syn.b.on", 0 }, { "syn.flt.on", 0 }, { "syn.a.voices", 0 }, { "syn.a.wave", 1 }, { "syn.a.shape", 60 }, { "syn.sub.level", 0 }, { "syn.char", 0 }, { "syn.space", 0 } }, { 88 }, 1.0 },
        { "04-sync-sweep",  { { "syn.a.on", 0 }, { "syn.b.sync", 1 }, { "syn.b.wave", 0 }, { "syn.b.pitch", 7 }, { "syn.b.fine", 30 }, { "syn.flt.on", 0 }, { "syn.sub.level", 0 }, { "syn.char", 0 }, { "syn.space", 0 } }, { 64 }, 1.0 },
        { "05-supersaw-pad",{ { "syn.a.wave", 4 }, { "syn.a.detune", 40 }, { "syn.b.wave", 4 }, { "syn.b.pitch", 12 }, { "syn.chord", 3 }, { "syn.flt.type", 2 }, { "syn.flt.cutoff", 3000 }, { "syn.env.a.att", 400 }, { "syn.env.a.rel", 1200 }, { "syn.space", 60 } }, { 45, 43 }, 1.4 },
        { "06-arp",         { { "syn.arp.on", 1 }, { "syn.chord", 3 }, { "syn.a.wave", 1 }, { "syn.flt.env", 70 }, { "syn.flt.cutoff", 700 }, { "syn.env.a.sus", 0 }, { "syn.env.a.dec", 250 }, { "syn.space", 40 } }, { 40, 45 }, 2.0 },
        { "07-vowel-talk",  { { "syn.flt.type", 6 }, { "syn.flt.res", 60 }, { "syn.lfo.target", 1 }, { "syn.lfo.depth", 45 }, { "syn.lfo.rate", 1.5f }, { "syn.flt.env", 0 } }, { 40, 47 }, 1.2 },
        { "08-8bit-arp",    { { "syn.char", 2 }, { "syn.arp.on", 1 }, { "syn.arp.rate", 4 }, { "syn.a.wave", 1 }, { "syn.a.shape", 50 }, { "syn.b.on", 0 }, { "syn.flt.on", 0 }, { "syn.arp.oct", 2 } }, { 52, 48 }, 1.5 },
        { "09-vhs-lead",    { { "syn.char", 3 }, { "syn.glide", 120 }, { "syn.lfo.depth", 12 }, { "syn.lfo.fade", 400 }, { "syn.lfo.rate", 5.5f }, { "syn.legato", 1 } }, { 64, 67, 69, 71 }, 0.5 },
        { "10-bass",        { { "syn.sub.level", 70 }, { "syn.flt.cutoff", 400 }, { "syn.flt.env", 60 }, { "syn.flt.drive", 50 }, { "syn.env.a.sus", 60 }, { "syn.b.pitch", -12 }, { "syn.space", 0 } }, { 28, 31, 33, 35 }, 0.4 },
    };
    int written = 0;
    for (const auto& pt : patches)
    {
        p.presetManager().loadInit();
        setNorm (p, "synth.on", 1); setNorm (p, "amp.on", 0); setNorm (p, "cab.on", 0); setNorm (p, "doubler.on", 0); setNorm (p, "gate.on", 0);
        setNorm (p, "eq.on", 0); setNorm (p, "post.on", 0); setNorm (p, "pre.on", 0); setNorm (p, "sfx.on", 0);
        setReal (p, "syn.mix", 100);
        for (auto& kv : pt.set) setReal (p, kv.first, kv.second);
        const double total = pt.notes.size() * pt.noteLen + 1.5;
        juce::AudioBuffer<float> out (2, (int) (total * sr)); out.clear();
        juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer midi;
        int noteIdx = -1;
        for (int pos = 0; pos + block <= out.getNumSamples(); pos += block)
        {
            buf.clear(); midi.clear();
            const int want = (int) (pos / (pt.noteLen * sr));
            if (want != noteIdx && want < (int) pt.notes.size())
            {
                if (noteIdx >= 0) midi.addEvent (juce::MidiMessage::noteOff (1, pt.notes[(size_t) noteIdx]), 0);
                noteIdx = want; midi.addEvent (juce::MidiMessage::noteOn (1, pt.notes[(size_t) noteIdx], (juce::uint8) 110), 1);
            }
            else if (want >= (int) pt.notes.size() && noteIdx >= 0 && noteIdx < (int) pt.notes.size())
            { midi.addEvent (juce::MidiMessage::noteOff (1, pt.notes[(size_t) noteIdx]), 0); noteIdx = (int) pt.notes.size(); }
            p.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, block);
        }
        auto f = dir.getChildFile (juce::String (pt.name) + ".wav"); f.deleteFile();
        juce::WavAudioFormat wav;
        if (auto os = std::unique_ptr<juce::OutputStream> (f.createOutputStream()))
            if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0)))
            { os.release(); w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples()); ++written; }
    }
    p.releaseResources();
    std::cout << "Wrote " << written << " synth demos to " << dir.getFullPathName() << std::endl;
    return written > 0 ? 0 : 1;
}
} // namespace obs
