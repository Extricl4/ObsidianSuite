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
        r.check (loaded == factory, juce::String (loaded) + " of " + juce::String (factory) + " factory presets load");

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
//  Factory preset generator. Run:  "Obsidian Amp Suite" --write-factory-presets <folder>
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
    b.set ("syn.arp.rate", 3.f); b.set ("syn.chord", 3.f);
    b.set ("amp.gain", 2.6f);
    b.module (POST, 0, "crypt", { { "Decay", 4.f }, { "Mix", 30.f } });
    written += b.write (dir, "Arp Machine", "Synth");

    proc->releaseResources();
    std::cout << "Wrote " << written << " presets to " << dir.getFullPathName() << std::endl;
    return written > 0 ? 0 : 1;
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
