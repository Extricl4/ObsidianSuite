// =============================================================================
//  Offline self-test. Run:   "Obsidian Suite" --selftest [report.txt] [--nam file.nam]
//  Exit code 0 = everything passed. CI runs this on every build.
//
//  Checks: no NaN/Inf ever, no runaway levels, sound passes through, every
//  module in every rack with random settings, random knob fuzzing, sample rates
//  44.1/48/96 k and odd block sizes, transpose accuracy (Live + HQ), synth
//  tracking, preset save/load round-trip, DAW state round-trip, NAM loading,
//  and a CPU budget for a full rig.
// =============================================================================
#include "../PluginProcessor.h"

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
} // namespace

int runSelfTest (const juce::StringArray& args)
{
    Report r;
    const auto start = juce::Time::getMillisecondCounterHiRes();
    juce::File reportFile;
    juce::File namFile;
    for (int i = 0; i < args.size(); ++i)
    {
        if (args[i] == "--nam" && i + 1 < args.size()) namFile = juce::File (args[i + 1].unquoted());
        else if (args[i] != "--selftest" && ! args[i].startsWith ("--") && (i == 0 || args[i - 1] != "--nam")) reportFile = juce::File::getCurrentWorkingDirectory().getChildFile (args[i].unquoted());
    }
    r.lines.add ("Obsidian Suite " OBSIDIAN_VERSION_STRING " self-test   (" + juce::SystemStats::getCpuModel() + ", " + juce::SystemStats::getOperatingSystemName() + ")");

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

    // ---------------------------------------------------------------- 6. synth
    r.head ("Guitar synth");
    {
        setNorm (p, "synth.on", 1); setNorm (p, "syn.arp.on", 0); setReal (p, "syn.mix", 80);
        float act = 0;
        render (p, SR, BLOCK, 1.0, [&] (int i) { if (i % (int) (SR * 0.5) == 0) g.pluck (110.f, 0.5f); act = juce::jmax (act, p.engine().synthActivity()); return g.next(); });
        r.check (act > 0.01f, "synth follows a plucked A2 (activity " + juce::String (act, 3) + ")");
        setNorm (p, "syn.arp.on", 1);
        auto st = render (p, SR, BLOCK, 1.0, riff);
        r.check (st.finite && st.peak <= 2.f, "arpeggiator runs cleanly");
        for (int slope = 0; slope < 4; ++slope)
        {
            setReal (p, "syn.flt.slope", (float) slope); setReal (p, "syn.flt.res", 100);
            auto s2 = render (p, SR, BLOCK, 0.5, riff);
            r.check (s2.finite && s2.peak <= 2.f, "filter slope " + juce::String (6 * (slope + 1)) + " dB at full resonance is stable");
        }
        p.presetManager().loadInit();
    }

    // ---------------------------------------------------------------- 7. NAM
    r.head ("NAM captures");
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
        p.engine().amp().setType (dsp::AmpSection::Core);
        p.prepareToPlay (SR, BLOCK);
    }
    else r.info ("(skipped: pass --nam <file.nam> to test capture loading)");

    // ---------------------------------------------------------------- 8. presets
    r.head ("Presets and DAW state");
    {
        auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("obsidian-selftest-presets");
        tmp.deleteRecursively(); tmp.createDirectory();
        const auto oldFolder = p.settings().get ("presetFolder");
        const auto oldLast = p.settings().get ("lastPreset");      // the test must not change your settings
        p.presetManager().setFolder (tmp);
        int factory = 0, loaded = 0;
        for (auto& pr : p.presetManager().list()) if (pr.factory) { ++factory; if (p.presetManager().load (pr.id)) ++loaded; }
        r.check (factory > 0 && loaded == factory, juce::String (loaded) + " of " + juce::String (factory) + " factory presets load");

        setReal (p, "amp.gain", 7.3f); setReal (p, "eq.b3", -4.5f); p.rig().setModule (1, 2, "tide");
        r.check (p.presetManager().saveAs ("Selftest Sound"), "save as");
        setReal (p, "amp.gain", 2.0f); p.rig().setModule (1, 2, {});
        bool found = false;
        for (auto& pr : p.presetManager().list()) if (pr.name == "Selftest Sound") found = p.presetManager().load (pr.id);
        auto* gain = dynamic_cast<juce::RangedAudioParameter*> (p.state().getParameter ("amp.gain"));
        r.check (found && std::abs (gain->convertFrom0to1 (gain->getValue()) - 7.3f) < 0.01f, "load restores knob values");
        r.check (p.rig().toJson()["racks"][1][2]["key"].toString() == "tide", "load restores the pedal/effect slots");

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
//  Factory preset generator. Run:  "Obsidian Suite" --write-factory-presets <folder>
//  Writes plugin/Presets/*.json. Edit the sounds below, run it, rebuild.
//  (Presets can also be made in the plugin with SAVE AS and copied in by hand.)
// =============================================================================
namespace obs
{
namespace
{
struct PresetBuilder
{
    ObsidianProcessor& p;
    void reset()                                   { p.presetManager().loadInit(); }
    void set (const juce::String& id, float real)  { setReal (p, id, real); }
    void on (const juce::String& id, bool b)       { setNorm (p, id, b ? 1.f : 0.f); }
    void module (int rack, int slot, const juce::String& key, std::initializer_list<std::pair<const char*, float>> values = {})
    {
        p.rig().setModule (rack, slot, key, true);
        const auto* type = dsp::findModule (key);
        if (type == nullptr) return;
        for (auto& kv : values)
            for (int i = 0; i < (int) type->params.size(); ++i)
                if (type->params[(size_t) i].name == kv.first)
                    setNorm (p, slotParamId (rack, slot, i), type->params[(size_t) i].toNorm (kv.second));
    }
    void eq (std::initializer_list<float> gains, const juce::String& name = "Custom")
    {
        int b = 1; for (auto g : gains) set ("eq.b" + juce::String (b++), g);
        p.rig().setMisc ("eqPreset", name);
    }
    bool write (const juce::File& dir, const juce::String& name, const juce::String& category)
    {
        auto v = p.presetManager().captureCurrent (name, category);
        return dir.getChildFile (name + ".json").replaceWithText (juce::JSON::toString (v, false));
    }
};
} // namespace

int writeFactoryPresets (const juce::File& dir)
{
    dir.createDirectory();
    auto proc = std::make_unique<ObsidianProcessor>();
    proc->setPlayConfigDetails (2, 2, 48000, 256);
    proc->prepareToPlay (48000, 256);
    PresetBuilder b { *proc };
    int written = 0;
    const int PRE = 0, POST = 1, SFX = 2;

    // ---- Rhythm ----------------------------------------------------------------------------
    b.reset();
    b.set ("amp.gain", 7.6f); b.set ("amp.bass", 6.0f); b.set ("amp.mid", 5.4f); b.set ("amp.treble", 6.4f);
    b.set ("amp.presence", 6.2f); b.set ("amp.depth", 6.6f); b.set ("amp.master", 5.0f);
    b.on ("gate.on", true); b.set ("gate.thresh", -54.f);
    b.module (PRE, 0, "overdrive", { { "Drive", 1.2f }, { "Tone", 6.5f }, { "Level", 6.f } });
    b.eq ({ -1.f, 1.f, 0.f, -2.f, 0.f, 0.5f, 1.5f, 0.f, -1.f }, "Tight Rhythm");
    b.on ("doubler.on", true); b.set ("doubler.time", 8.f); b.set ("width", 120.f);
    written += b.write (dir, "Tight Wall", "Rhythm");

    b.reset();
    b.set ("amp.gain", 5.2f); b.set ("amp.bass", 5.2f); b.set ("amp.mid", 6.6f); b.set ("amp.treble", 6.0f); b.set ("amp.presence", 5.6f);
    b.on ("gate.on", true); b.set ("gate.thresh", -62.f);
    b.module (PRE, 0, "overdrive", { { "Drive", 4.f }, { "Tone", 5.5f }, { "Level", 2.f } });
    b.module (POST, 0, "crypt", { { "Decay", 1.4f }, { "Size", 40.f }, { "Mix", 12.f } });
    b.on ("doubler.on", true); b.set ("doubler.time", 6.f);
    written += b.write (dir, "Legion Crunch", "Rhythm");

    b.reset();
    b.set ("amp.gain", 8.4f); b.set ("amp.bass", 6.6f); b.set ("amp.mid", 4.4f); b.set ("amp.treble", 6.2f); b.set ("amp.depth", 7.f);
    b.on ("gate.on", true); b.set ("gate.thresh", -50.f);
    b.module (PRE, 0, "boost", { { "Gain", 7.f }, { "Tight", 150.f } });
    b.eq ({ 0.f, 1.5f, -1.f, -3.f, -1.f, 1.f, 2.f, 0.f, -2.f }, "Djent");
    b.on ("doubler.on", true); b.set ("doubler.time", 9.f); b.set ("width", 130.f);
    written += b.write (dir, "Styx Chug", "Rhythm");

    b.reset();
    b.set ("amp.gain", 8.f); b.set ("amp.bass", 5.6f); b.set ("amp.mid", 5.f); b.set ("amp.treble", 6.f); b.set ("amp.depth", 7.4f); b.set ("amp.presence", 6.6f);
    b.on ("gate.on", true); b.set ("gate.thresh", -46.f);
    b.module (PRE, 0, "compressor", { { "Threshold", -20.f }, { "Ratio", 3.f }, { "Attack", 15.f }, { "Makeup", 3.f } });
    b.module (PRE, 1, "boost", { { "Gain", 8.f }, { "Tight", 190.f } });
    b.eq ({ 1.f, 0.f, -1.5f, -2.f, 0.f, 1.f, 1.f, -0.5f, -2.f }, "Thrash");
    b.on ("doubler.on", true); b.set ("doubler.time", 7.f);
    written += b.write (dir, "War Drum", "Rhythm");

    // ---- Clean / ambient -------------------------------------------------------------------
    b.reset();
    b.set ("amp.gain", 1.8f); b.set ("amp.bass", 5.6f); b.set ("amp.mid", 4.6f); b.set ("amp.treble", 6.6f); b.set ("amp.presence", 5.6f); b.set ("amp.master", 6.f);
    b.on ("gate.on", false);
    b.module (PRE, 0, "compressor", { { "Threshold", -26.f }, { "Ratio", 4.f }, { "Makeup", 7.f }, { "Blend", 70.f } });
    b.module (POST, 0, "tide", { { "Rate", 0.7f }, { "Depth", 45.f }, { "Mix", 45.f } });
    b.module (POST, 1, "crypt", { { "Decay", 2.4f }, { "Mix", 24.f } });
    b.on ("doubler.on", false);
    written += b.write (dir, "Glass Clean", "Clean");

    b.reset();
    b.set ("amp.gain", 2.6f); b.set ("amp.mid", 4.4f); b.set ("amp.treble", 6.2f);
    b.on ("gate.on", false);
    b.module (POST, 0, "tide", { { "Rate", 0.35f }, { "Depth", 55.f }, { "Mix", 40.f } });
    b.module (POST, 1, "lethe", { { "Time", 480.f }, { "Feedback", 45.f }, { "Mix", 30.f }, { "Mode", 1.f } });
    b.module (POST, 2, "crypt", { { "Decay", 5.f }, { "Size", 80.f }, { "Mix", 35.f } });
    b.on ("doubler.on", true); b.set ("doubler.time", 12.f); b.set ("width", 140.f);
    written += b.write (dir, "Night Drive", "Ambient");

    b.reset();
    b.set ("amp.gain", 2.2f); b.set ("amp.treble", 6.f);
    b.on ("gate.on", false);
    b.module (POST, 0, "aurora", { { "Attack", 500.f }, { "Reverb", 45.f } });
    b.module (SFX, 0, "seraph", { { "Decay", 8.f }, { "Shimmer", 60.f }, { "Mix", 40.f } });
    b.on ("doubler.on", false);
    written += b.write (dir, "Cathedral Swell", "Ambient");

    // ---- Lead ------------------------------------------------------------------------------
    b.reset();
    b.set ("amp.gain", 8.2f); b.set ("amp.bass", 5.2f); b.set ("amp.mid", 7.2f); b.set ("amp.treble", 6.2f); b.set ("amp.presence", 6.f);
    b.on ("gate.on", true); b.set ("gate.thresh", -60.f);
    b.module (PRE, 0, "overdrive", { { "Drive", 2.5f }, { "Tone", 6.f }, { "Level", 4.f } });
    b.module (POST, 0, "lethe", { { "Time", 380.f }, { "Feedback", 30.f }, { "Mix", 22.f }, { "Mode", 2.f } });
    b.module (POST, 1, "crypt", { { "Decay", 2.f }, { "Mix", 18.f } });
    b.eq ({ 0.f, 0.f, 0.5f, 1.f, 1.5f, 1.f, 0.f, -1.f, -2.f }, "Lead");
    b.on ("doubler.on", false);
    written += b.write (dir, "Solo Burn", "Lead");

    b.reset();
    b.set ("amp.gain", 7.f); b.set ("amp.mid", 6.6f); b.set ("amp.treble", 5.8f);
    b.on ("gate.on", true); b.set ("gate.thresh", -64.f);
    b.module (PRE, 0, "overdrive", { { "Drive", 3.f }, { "Level", 3.f } });
    b.module (POST, 0, "lethe", { { "Time", 520.f }, { "Feedback", 40.f }, { "Mix", 25.f } });
    b.module (SFX, 0, "seraph", { { "Decay", 6.f }, { "Shimmer", 35.f }, { "Mix", 22.f } });
    b.on ("doubler.on", false);
    written += b.write (dir, "Halo Lead", "Lead");

    b.reset();
    b.set ("amp.gain", 7.4f); b.set ("amp.mid", 6.f);
    b.on ("gate.on", true); b.set ("gate.thresh", -58.f);
    b.module (PRE, 0, "legion", { { "Voice 1", 5.f }, { "Key", 4.f }, { "Scale", 1.f }, { "Mix", 60.f } });
    b.module (POST, 0, "lethe", { { "Time", 400.f }, { "Feedback", 30.f }, { "Mix", 20.f } });
    b.module (POST, 1, "crypt", { { "Mix", 16.f } });
    b.on ("doubler.on", false);
    written += b.write (dir, "Twin Harmony", "Lead");

    // ---- Synth -----------------------------------------------------------------------------
    b.reset();
    b.on ("synth.on", true); b.on ("syn.arp.on", false); b.set ("syn.mix", 70.f); b.on ("syn.post", true);
    b.set ("syn.flt.cutoff", 2400.f); b.set ("syn.flt.res", 35.f); b.set ("syn.flt.env", 45.f); b.set ("syn.glide", 40.f);
    b.set ("amp.gain", 3.f);
    b.module (POST, 0, "lethe", { { "Time", 350.f }, { "Feedback", 35.f }, { "Mix", 25.f }, { "Mode", 2.f } });
    b.module (POST, 1, "crypt", { { "Decay", 3.f }, { "Mix", 25.f } });
    written += b.write (dir, "Neon Lead Synth", "Synth");

    b.reset();
    b.on ("synth.on", true); b.on ("syn.arp.on", true); b.set ("syn.mix", 55.f);
    for (int s = 1; s <= 8; ++s) b.on ("syn.arp.s" + juce::String (s), s == 1 || s == 3 || s == 5 || s == 8);
    b.set ("syn.arp.rate", 0.75f); b.on ("syn.arp.sync", true);
    b.set ("amp.gain", 2.6f);
    b.module (POST, 0, "crypt", { { "Decay", 4.f }, { "Mix", 30.f } });
    written += b.write (dir, "Arp Machine", "Synth");

    proc->releaseResources();
    std::cout << "Wrote " << written << " presets to " << dir.getFullPathName() << std::endl;
    return written > 0 ? 0 : 1;
}
} // namespace obs
