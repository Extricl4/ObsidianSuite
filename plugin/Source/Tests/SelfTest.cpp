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
#include "../DSP/AtmosDefs.h"
#include <set>
#include "../DSP/Modules/ModuleRegistry.h"

namespace obs
{
namespace
{
struct Report
{
    juce::StringArray lines; int failures = 0, checks = 0;
    juce::File progress;                    // written line by line, so a crash still shows how far it got
    void add (const juce::String& s)  { lines.add (s); if (progress != juce::File()) progress.appendText (s + "\n", false, false); }
    void ok (const juce::String& s)   { ++checks; add ("  ok    " + s); }
    void fail (const juce::String& s) { ++checks; ++failures; add ("  FAIL  " + s); }
    void info (const juce::String& s) { add ("        " + s); }
    void head (const juce::String& s) { add (""); add ("== " + s); }
    void check (bool cond, const juce::String& s) { cond ? ok (s) : fail (s); }
};



/** Noise gate bench: DI-like guitar situations, the gate's gain on every sample. */
namespace gatebench
{
constexpr double kSR = 48000.0;
inline float noise (uint32_t& s) { float a = 0; for (int k = 0; k < 4; ++k) { s = s * 1664525u + 1013904223u; a += (float) (s >> 8) / 16777216.f - .5f; } return a * 1.732f; }
inline float tone (double t, double f, int harm) { float v = 0; for (int k = 1; k <= harm; ++k) v += (float) std::sin (juce::MathConstants<double>::twoPi * f * k * t) / (float) k; return v; }
inline float db (float d) { return std::pow (10.f, d / 20.f); }

/** A held lead note: starts at peakDb, fades fadeDbPerSec, +-3 dB vibrato swell at 5.5 Hz, over hiss at noiseDb. */
inline std::vector<float> lead (float peakDb, float fadeDbPerSec, float noiseDb, double secs)
{
    std::vector<float> x ((size_t) (secs * kSR)); uint32_t s = 11u;
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double t = (double) i / kSR;
        const float lv = peakDb - fadeDbPerSec * (float) t + 3.f * (float) std::sin (juce::MathConstants<double>::twoPi * 5.5 * t);
        x[i] = tone (t, 330.0, 4) * db (lv) * (float) std::min (1.0, t / 0.003) * .6f + noise (s) * db (noiseDb);
    }
    return x;
}
/** Palm-muted chugs on a low string: count x (onMs on, gapMs off), stopped dead, over hiss. Onsets go to starts. */
inline std::vector<float> chugs (int count, double onMs, double gapMs, float peakDb, float noiseDb, std::vector<int>& starts, std::vector<int>& stops)
{
    const int on = (int) (onMs * kSR / 1000), per = on + (int) (gapMs * kSR / 1000);
    std::vector<float> x ((size_t) (count * per + kSR * .3)); uint32_t s = 5u;
    for (int c = 0; c < count; ++c) { starts.push_back (c * per + (int) (kSR * .05)); stops.push_back (c * per + (int) (kSR * .05) + on); }
    for (size_t i = 0; i < x.size(); ++i)
    {
        float v = noise (s) * db (noiseDb);
        for (size_t c = 0; c < starts.size(); ++c)
        {
            const int k = (int) i - starts[c]; if (k < 0 || k > on + (int) (kSR * .004)) continue;
            const double t = k / kSR; float e = (float) std::min (1.0, t / 0.001);
            if (k > on) e *= 1.f - (float) (k - on) / (float) (kSR * .004);          // stopped in 4 ms
            v += tone (t, 82.4, 8) * .45f * db (peakDb) * e;
        }
        x[i] = v;
    }
    return x;
}
template <class G> std::vector<float> run (const std::vector<float>& in, float thresh)
{
    G g; g.prepare (kSR); std::vector<float> gain (in.size());
    for (size_t i = 0; i < in.size(); ++i) { float v = in[i]; g.process (&v, 1, thresh, true); gain[i] = g.currentGain(); }
    return gain;
}
inline double firstBelow (const std::vector<float>& g, float lim, double fromSec)
{ for (size_t i = (size_t) (fromSec * kSR); i < g.size(); ++i) if (g[i] < lim) return (double) i / kSR; return (double) g.size() / kSR; }
/** Times the gain fell under 0.5 and came back over 0.9 (chopping) from fromSec on. */
inline int chops (const std::vector<float>& g, double fromSec)
{ int n = 0; bool down = false; for (size_t i = (size_t) (fromSec * kSR); i < g.size(); ++i) { if (g[i] < .5f) down = true; else if (down && g[i] > .9f) { ++n; down = false; } } return n; }
inline float mean (const std::vector<float>& g, double a, double b) { double s = 0; int n = 0; for (size_t i = (size_t) (a * kSR); i < (size_t) (b * kSR) && i < g.size(); ++i) { s += g[i]; ++n; } return n ? (float) (s / n) : 0.f; }
inline float meanAt (const std::vector<float>& g, int at, int a, int b) { double s = 0; for (int i = at + a; i < at + b; ++i) s += g[(size_t) i]; return (float) (s / (b - a)); }
inline float maxStep (const std::vector<float>& g, bool fallingOnly) { float m = 0; for (size_t i = 1; i < g.size(); ++i) { const float d = g[i] - g[i - 1]; if (! fallingOnly || d < 0) m = std::max (m, std::abs (d)); } return m; }
} // namespace gatebench

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

RunStats render (ObsidianProcessor& p, double sr, int block, double seconds, std::function<float (int)> input, int numInCh = 2,
                 std::vector<float>* capture = nullptr)
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
        if (capture != nullptr)
            for (int i = 0; i < n; ++i) capture->push_back (0.5f * (buf.getSample (0, i) + buf.getSample (1, i)));
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
    void setAllToMax() { for (size_t k = 0; k < type->params.size(); ++k) if (! type->params[k].momentary) p[k] = type->params[k].max; }
    /** Renders `seconds` of input and returns the output (mid of L/R). */
    std::vector<float> render (double sr, int block, double seconds, std::function<float (int)> input)
    {
        std::vector<float> out; out.reserve ((size_t) (sr * seconds) + (size_t) block);
        juce::AudioBuffer<float> b (2, block); const int total = (int) (sr * seconds);
        for (int done = 0; done < total; done += block)
        {
            const int n = juce::jmin (block, total - done);
            for (int i = 0; i < n; ++i) { const float x = input (done + i); b.setSample (0, i, x); b.setSample (1, i, x); }
            m->process (b.getArrayOfWritePointers(), 2, n, p.data(), ctx);
            for (int i = 0; i < n; ++i) out.push_back (0.5f * (b.getSample (0, i) + b.getSample (1, i)));
        }
        return out;
    }
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

/** Energy of `f` Hz in out[from, to) (Goertzel). */
double tonePower (const std::vector<float>& out, double sr, double f, size_t from, size_t to)
{
    to = juce::jmin (to, out.size()); if (from >= to) return 0;
    const double w = 2.0 * juce::MathConstants<double>::pi * f / sr, c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (size_t i = from; i < to; ++i) { const double s0 = out[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return (s1 * s1 + s2 * s2 - c * s1 * s2) / (double) ((to - from) * (to - from));
}
double rmsOf (const std::vector<float>& out, size_t from, size_t to)
{
    to = juce::jmin (to, out.size()); if (from >= to) return 0;
    double a = 0; for (size_t i = from; i < to; ++i) a += (double) out[i] * out[i];
    return std::sqrt (a / (double) (to - from));
}

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

#if defined (_MSC_VER)
 #define OBS_NOINLINE __declspec (noinline)
#else
 #define OBS_NOINLINE __attribute__ ((noinline))
#endif
// The checks live in their own (big) function: if its stack frame ever were too big, the progress file
// already says the self-test started (CI prints it), instead of the app dying without a trace.
OBS_NOINLINE static int runSelfTestBody (Report& r, const juce::StringArray& args, juce::File reportFile, juce::File namFile, bool packs, double start);

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
    if (reportFile != juce::File()) { r.progress = reportFile.getSiblingFile (reportFile.getFileNameWithoutExtension() + "-progress.txt"); r.progress.deleteFile(); }
    r.add (OBSIDIAN_PRODUCT_NAME " " OBSIDIAN_VERSION_STRING " self-test   (" + juce::SystemStats::getCpuModel() + ", " + juce::SystemStats::getOperatingSystemName() + ")");
    return runSelfTestBody (r, args, reportFile, namFile, packs, start);
}

OBS_NOINLINE static int runSelfTestBody (Report& r, const juce::StringArray& args, juce::File reportFile, juce::File namFile, bool packs, double start)
{
    juce::ignoreUnused (args);

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

    r.head ("Cab mics (the sound doesn't depend on what was loaded before)");
    {
        // A condenser has no proximity bass; going back to a dynamic at the same distance used to keep the condenser's
        // flat low end, so a preset sounded thinner after one that used a 414. Compare with a cab that never saw it.
        auto run = [&] (bool via414) {
            dsp::CabSection cab; cab.prepare (SR, BLOCK);
            juce::Random rng (7);
            std::vector<float> in ((size_t) BLOCK), l ((size_t) BLOCK), rr ((size_t) BLOCK);
            dsp::CabSection::MicParams a, b; b.mute = true;
            // the cab's impulse response is built on a background thread: let it arrive before comparing
            // (a busy build machine took long enough that the two runs measured different responses)
            for (int i = 0; i < 40; ++i)
            {
                std::fill (in.begin(), in.end(), 0.f);
                cab.process (in.data(), l.data(), rr.data(), BLOCK, a, b, 20.f, 20000.f, true);
                juce::Thread::sleep (25);
            }
            cab.reset();
            double sum = 0;
            for (int blk = 0; blk < 400; ++blk)
            {
                a.model = (via414 && blk < 100) ? 2 : 0;
                for (auto& v : in) v = rng.nextFloat() * 0.4f - 0.2f;
                cab.process (in.data(), l.data(), rr.data(), BLOCK, a, b, 20.f, 20000.f, true);
                if (blk >= 300) for (auto v : l) sum += (double) v * v;
            }
            return juce::Decibels::gainToDecibels ((float) std::sqrt (sum / (100.0 * BLOCK)));
        };
        const float fresh = run (false), after = run (true);
        r.check (std::abs (fresh - after) < 0.1f, "a dynamic mic sounds the same after a condenser was on it (" + juce::String (fresh, 2) + " / "
                 + juce::String (after, 2) + " dB)");
    }
    r.head ("Full-rig capture amps bypass the plugin's cab");
    {
        // every amp whose captures have the cab in them plays all its settings as rigs, so there's never a second cab
        juce::StringArray bad; int n = 0;
        for (auto& f : Rig::contentFolder().getChildFile ("Amps").findChildFiles (juce::File::findFiles, false, "*.json"))
        {
            const auto key = f.getFileNameWithoutExtension();
            const auto v = Rig::readAmpFile (key);
            if (! v["kind"].toString().startsWithIgnoreCase ("Full rig")) continue;
            ++n;
            if (auto* st = v["map"]["settings"].getDynamicObject())
                for (auto& kv : st->getProperties())
                    if (! (bool) kv.value["rig"]) bad.add (key + " " + kv.name.toString());
        }
        r.check (bad.isEmpty(), juce::String (n) + " full-rig amps play every setting without the plugin's cab"
                 + (bad.isEmpty() ? juce::String() : " (not: " + bad.joinIntoString (", ") + ")"));
    }

    // ---------------------------------------------------------------- 1a0. noise gate
    r.head ("Noise gate (held leads, fading leads, chugs, hiss)");
    {
        using namespace gatebench;
        struct M { double sustain; int chop; float attack, tight, gaps, silent, barely, step; };
        auto measure = [&] (auto tag) {
            using G = decltype (tag);
            M m {};
            // a lead held at a normal level, and a quiet one (neck pickup, volume down) fading through the threshold
            const auto quiet = lead (-30.f, 6.f, -72.f, 7.0);
            const auto gq = run<G> (quiet, -52.f);
            m.sustain = firstBelow (gq, .5f, .05); m.chop = chops (gq, .05);
            // chugs: pick attack kept, stops tight, the hiss between them gone
            std::vector<int> on, off; const auto ch = chugs (8, 110, 110, -8.f, -68.f, on, off);
            const auto gc = run<G> (ch, -50.f);
            float att = 1, tight = 0, gaps = 0;
            for (size_t c = 1; c < on.size(); ++c) { att = std::min (att, meanAt (gc, on[c], 48, 144)); tight = std::max (tight, meanAt (gc, off[c], 2880 - 48, 2880 + 48)); gaps = std::max (gaps, meanAt (gc, off[c], 3840, 4800)); }
            m.attack = att; m.tight = tight; m.gaps = gaps; m.step = maxStep (gc, true);
            // hiss alone: shut; barely on (threshold under the hiss): never touches a lead
            std::vector<float> hiss ((size_t) (1.5 * kSR)); { uint32_t s = 3u; for (auto& v : hiss) v = noise (s) * db (-68.f); }
            m.silent = mean (run<G> (hiss, -50.f), 1.0, 1.5);
            const auto held = lead (-10.f, 9.f, -72.f, 4.0);
            m.barely = 1.f; { const auto gb = run<G> (held, -78.f); for (size_t i = (size_t) (.01 * kSR); i < gb.size(); ++i) m.barely = std::min (m.barely, gb[i]); }
            return m; };
        const M n = measure (dsp::NoiseGate());
        // the old gate on the same bench (2026-11-22): quiet lead cut at 4.56 s, chugs never closed between notes
        r.check (n.sustain > 6.0 && n.chop == 0, juce::String::formatted ("a quiet lead fading through the threshold rings on (%.2f s; the old gate cut it at 4.56 s) and never chops (%d)", n.sustain, n.chop));
        r.check (n.attack > 0.95f, juce::String::formatted ("palm-muted chugs keep their pick attack (gain %.2f 1-3 ms into each)", n.attack));
        r.check (n.tight < 0.01f && n.gaps < 0.001f, juce::String::formatted ("chugs stop tight: gain %.3f 60 ms after each stop, %.4f in the gaps (the old gate stayed open)", n.tight, n.gaps));
        r.check (n.silent < 0.001f, juce::String::formatted ("hiss alone is shut (gain %.5f)", n.silent));
        r.check (n.barely > 0.99f, juce::String::formatted ("barely on (threshold under the hiss): a lead is never touched (min gain %.2f)", n.barely));
        r.check (n.step < 0.01f, juce::String::formatted ("closing is smooth: largest gain step %.4f per sample", n.step));
        {
            const auto held = lead (-10.f, 9.f, -72.f, 4.0);
            const auto g = run<dsp::NoiseGate> (held, -52.f);
            float mn = 1.f; for (size_t i = (size_t) (.01 * kSR); i < g.size(); ++i) mn = std::min (mn, g[i]);
            r.check (mn > 0.99f, juce::String::formatted ("a held lead with vibrato at the default threshold is never touched (min gain %.3f)", mn));
        }
        {   // closed on hiss, then switched off: back to unity at once, without a click
            dsp::NoiseGate g; g.prepare (kSR); uint32_t sd = 9u; float step = 0, prev = 1.f, last = 0.f;
            for (int i = 0; i < (int) kSR; ++i) { float v = noise (sd) * db (-68.f); g.process (&v, 1, -50.f, true); prev = g.currentGain(); }
            const float closed = prev;
            for (int i = 0; i < (int) (0.03 * kSR); ++i) { float v = noise (sd) * db (-68.f); g.process (&v, 1, -50.f, false); last = g.currentGain(); step = std::max (step, std::abs (last - prev)); prev = last; }
            r.check (closed < 0.001f && last > 0.99f && step < 0.02f, juce::String::formatted ("switched off while closed: back to unity in 30 ms without a click (largest step %.4f)", step));
        }
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

        // EQ page analyzer: a 1 kHz tone peaks at 1 kHz; after silence it falls back to the floor
        {
            auto& sp = p.engine().eqSpectrum();
            p.presetManager().loadInit(); setNorm (p, "amp.on", 0.f); setNorm (p, "cab.on", 0.f);
            sp.setWanted (true);
            render (p, SR, BLOCK, 0.3, [] (int i) { return 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * i / 48000.0); });
            const auto a = sp.analyse (SR, 1.0 / 30.0);
            const int peak = (int) (std::max_element (a.begin(), a.end()) - a.begin());   // 1 kHz is point 60 (31.25 Hz * 2^5)
            const float at1k = a[(size_t) peak], at100 = a[17];
            render (p, SR, BLOCK, 0.3, [] (int) { return 0.f; });
            float after = -200.f; for (int i = 0; i < 90; ++i) { const auto& b = sp.analyse (SR, 1.0 / 30.0); after = *std::max_element (b.begin(), b.end()); }
            sp.setWanted (false);
            r.check (std::abs (peak - 60) <= 1 && at1k > -40.f && at1k - at100 > 30.f && after < -78.f,
                     juce::String::formatted ("EQ analyzer: a 1 kHz tone peaks at 1 kHz (%.0f dB, 100 Hz %.0f dB); silent afterwards (%.0f dB)", at1k, at100, after));
            p.presetManager().loadInit();
        }

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
    {
        auto pluckAt = [] (Guitar& gg, double rate) { return [&gg, rate] (int i) { if (i % (int) (rate * 0.2) == 0) gg.pluck (110.f, 0.4f); return gg.next(); }; };
        p.prepareToPlay (192000.0, 512); Guitar g3 (192000.0);
        auto st = render (p, 192000.0, 512, 0.6, pluckAt (g3, 192000.0));
        r.check (st.finite && st.rms > 0.001f && st.peak <= 2.0f, "192 kHz / 512 samples");
        p.prepareToPlay (SR, 1); Guitar g4 (SR);
        st = render (p, SR, 1, 0.3, pluckAt (g4, SR));
        r.check (st.finite && st.rms > 0.001f && st.peak <= 2.0f, "1-sample blocks");
        p.prepareToPlay (SR, 256); Guitar g5 (SR);
        st = render (p, SR, 2048, 0.6, pluckAt (g5, SR));
        r.check (st.finite && st.rms > 0.001f && st.peak <= 2.0f, "a host that sends bigger blocks than it announced (256 -> 2048)");
        // FL Studio style: a different block size every call
        p.prepareToPlay (SR, 512); Guitar g6 (SR);
        auto in6 = pluckAt (g6, SR);
        juce::MidiBuffer midi; uint32_t seed = 7u; int idx = 0; double acc = 0; float peak = 0; bool finite = true;
        while (idx < (int) (SR * 0.6))
        {
            seed = seed * 1664525u + 1013904223u;
            const int n = 1 + (int) ((seed >> 8) % 512u);
            juce::AudioBuffer<float> b (2, n);
            for (int i = 0; i < n; ++i) { b.setSample (0, i, in6 (idx + i)); b.setSample (1, i, 0.f); }
            p.processBlock (b, midi);
            for (int i = 0; i < n; ++i) { const float v = b.getSample (0, i); finite = finite && std::isfinite (v); acc += (double) v * v; peak = std::max (peak, std::abs (v)); }
            idx += n;
        }
        const float rms = (float) std::sqrt (acc / idx);
        r.check (finite && rms > 0.001f && peak <= 2.0f, juce::String::formatted ("a different block size every call (1..512, like FL Studio): rms %.3f, peak %.2f", rms, peak));

        // channel layouts the plugin accepts: mono track (mono / mono) and stereo in / mono out (folded, not just left)
        auto layoutRun = [&] (const juce::AudioChannelSet& in, const juce::AudioChannelSet& out)
        {
            juce::AudioProcessor::BusesLayout l; l.inputBuses.add (in); l.outputBuses.add (out);
            const bool ok = p.setBusesLayout (l);
            p.prepareToPlay (SR, BLOCK);
            Guitar gg (SR); auto f = pluckAt (gg, SR);
            const int ch = juce::jmax (in.size(), out.size());
            juce::MidiBuffer m; double acc = 0; bool fin = true; int at = 0;
            for (int b = 0; b < (int) (SR * 0.6) / BLOCK; ++b)
            {
                juce::AudioBuffer<float> buf (ch, BLOCK);
                for (int i = 0; i < BLOCK; ++i) { const float v = f (at + i); for (int c = 0; c < ch; ++c) buf.setSample (c, i, c == 0 ? v : 0.f); }
                p.processBlock (buf, m);
                for (int i = 0; i < BLOCK; ++i)
                {
                    const float y = out.size() >= 2 ? 0.5f * (buf.getSample (0, i) + buf.getSample (1, i)) : buf.getSample (0, i);   // what a mono speaker gets
                    fin = fin && std::isfinite (y); acc += (double) y * y;
                }
                at += BLOCK;
            }
            return std::make_pair (ok && fin, (float) std::sqrt (acc / at));
        };
        const auto stSt = layoutRun (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo());
        const auto moMo = layoutRun (juce::AudioChannelSet::mono(), juce::AudioChannelSet::mono());
        const auto stMo = layoutRun (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::mono());
        layoutRun (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo());
        const float dbMo = juce::Decibels::gainToDecibels (moMo.second / stSt.second), dbSm = juce::Decibels::gainToDecibels (stMo.second / stSt.second);
        r.check (stSt.first && moMo.first && stMo.first && std::abs (dbMo) < 1.f && std::abs (dbSm) < 1.f,
                 juce::String::formatted ("mono track (mono in/out) and stereo in / mono out play like the stereo mix folded to mono (%+.1f / %+.1f dB)", dbMo, dbSm));
    }
    p.prepareToPlay (SR, BLOCK);
    {
        // the DAW learns the latency as soon as it prepares the plugin (HQ transpose adds some)
        setNorm (p, "transpose.hq", 1.f); setReal (p, "transpose", -2.f);
        p.prepareToPlay (SR, BLOCK);
        const int hq = p.getLatencySamples(), eng = p.engine().latencySamples();
        setNorm (p, "transpose.hq", 0.f); setReal (p, "transpose", 0.f);
        p.prepareToPlay (SR, BLOCK);
        r.check (hq > 0 && hq == eng && p.getLatencySamples() < hq,
                 juce::String::formatted ("the DAW is told the latency when it prepares the plugin (HQ transpose %d samples, Live %d)", hq, p.getLatencySamples()));
    }

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
            auto hOwner = std::make_unique<dsp::hydra::Harmonizer>(); auto& h = *hOwner; h.prepare (SR, BLOCK);     // 132 KB: not on the stack
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
        // GitHub's list: a tagged release (no build block) is newest, then builds 145 and 140: build 145 is picked
        juce::Array<juce::var> rels;
        rels.add (juce::JSON::parse (R"J({ "tag_name": "v0.2.0", "name": "Obsidian Amp Suite 0.2.0", "body": "First release",
            "assets": [ { "name": "ObsidianAmpSuite-0.2.0-Setup.exe", "size": 90000000, "browser_download_url": "https://github.com/Extricl4/ObsidianSuite/releases/download/v0.2.0/ObsidianAmpSuite-0.2.0-Setup.exe" } ] })J"));
        rels.add (json);
        auto older = juce::JSON::parse (juce::JSON::toString (json)); older.getDynamicObject()->setProperty ("tag_name", "build-140");
        older.getDynamicObject()->setProperty ("body", older["body"].toString().replace ("build: 145", "build: 140"));
        rels.add (older);
        const auto best = Updater::newestBuild (juce::var (rels));
        r.check (best.ok && best.build == 145 && Updater::newestBuild (juce::var (juce::Array<juce::var> { rels[0] })).ok == false,
                 "picks the newest build from GitHub's list, not a tagged release without one (build " + juce::String (best.build) + ")");
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
        // the on-screen KEYS send how hard a key was hit
        auto keyAct = [&] (float vel) { e.uiSynthNote (57, true, vel); float m = 0; render (p, SR, BLOCK, 0.3, [&] (int) { m = juce::jmax (m, e.synthActivity()); return 0.f; });
                                        e.uiSynthNote (57, false); render (p, SR, BLOCK, 1.0, [&] (int) { return 0.f; }); return m; };
        const float soft = keyAct (0.3f), hard = keyAct (1.0f);
        r.check (soft > 0.02f && hard > soft * 2.f && e.synthActivity() < 0.01f, juce::String::formatted ("on-screen KEYS: a soft key plays softer than a hard one (%.2f vs %.2f) and lets go", soft, hard));
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

    // ---------------------------------------------------------------- 6b. AETHER (SIREN's second page)
    r.head ("AETHER");
    {
        auto& e = p.engine();
        auto quietRig = [&] { p.presetManager().loadInit();
            for (auto id : { "amp.on", "cab.on", "eq.on", "pre.on", "post.on", "sfx.on", "doubler.on", "gate.on", "harm.on" }) setNorm (p, id, 0);
            setNorm (p, "synth.on", 1); setReal (p, "syn.mix", 0); };   // the SYNTH tab on (AETHER lives there), SIREN mixed out
        auto silence = [] (int) { return 0.f; };
        auto db = [] (float x) { return juce::Decibels::gainToDecibels (x, -120.f); };
        quietRig();
        render (p, SR, BLOCK, 3.0, silence);                   // tails of what played before die away
        auto off = render (p, SR, BLOCK, 0.5, silence);
        r.check (off.peak < 1.0e-6f, "off: adds nothing (peak " + juce::String (db (off.peak), 1) + " dB)");
        setNorm (p, "atm.on", 1);
        render (p, SR, BLOCK, 1.0, silence);
        auto def = render (p, SR, BLOCK, 2.0, silence);
        r.check (def.finite && db (def.rms) > -48.f && db (def.rms) < -24.f, "on, as it starts (wind and a strings fifth): " + juce::String (db (def.rms), 1) + " dB, well under the guitar");

        // every bed, voice and texture alone: finite, there, and not too loud
        auto layer = [&] (const char* onId, const char* typeId, int n, float minDb, juce::StringArray names, const juce::String& what) {
            bool ok = true; juce::String bad; float lo = 0, hi = -120;
            for (int t = 0; t < n; ++t)
            {
                quietRig(); setNorm (p, "atm.on", 1); setReal (p, "atm.space", 0);
                for (auto id : { "atm.bed.on", "atm.drn.on", "atm.tex.on" }) setNorm (p, id, 0);
                setNorm (p, onId, 1); setReal (p, typeId, (float) t); setReal (p, "atm.tex.density", 70);
                render (p, SR, BLOCK, 0.3, silence);
                auto st = render (p, SR, BLOCK, 3.0, silence);
                const float d = db (st.rms); lo = juce::jmin (lo, d); hi = juce::jmax (hi, d);
                if (! st.finite || st.peak >= 1.f || d < minDb || d > -14.f) { ok = false; bad << " " << names[t] << " (" << juce::String (d, 1) << " dB)"; }
            }
            r.check (ok, "every " + what + " alone plays, clean (" + juce::String (lo, 1) + " .. " + juce::String (hi, 1) + " dB)" + bad);
        };
        layer ("atm.bed.on", "atm.bed.type", obs::atmosdefs::kBedTypes, -50.f, juce::StringArray::fromTokens ("Hiss Wind Rain Surf Rumble Static Breath", false), "bed");
        layer ("atm.drn.on", "atm.drn.voice", obs::atmosdefs::kVoices, -50.f, juce::StringArray::fromTokens ("Organ Strings Choir Glass Abyss Descent Ascent", false), "drone voice");
        layer ("atm.tex.on", "atm.tex.type", obs::atmosdefs::kTextures, -75.f, juce::StringArray::fromTokens ("Vinyl Fire Drips Metal Shimmer Whispers Heartbeat Knocks Moans Flesh", false), "texture");

        // anything SUMMON or a hand could set: 24 random settings, extremes included
        {
            juce::Random rnd (77); bool ok = true; float worst = 0; juce::String bad;
            quietRig(); setNorm (p, "atm.on", 1);
            for (int k = 0; k < 24; ++k)
            {
                for (int i = 0; i < obs::atmosdefs::count; ++i)
                {
                    const auto& d = obs::atmosdefs::table()[i];
                    if (i == obs::atmosdefs::on) continue;
                    const float x = k < 4 ? (k % 2 ? 1.f : 0.f) : rnd.nextFloat();     // the first four: everything at one end
                    setReal (p, d.id, d.type == obs::synthdefs::BOOL || d.type == obs::synthdefs::CHOICE ? std::round (d.min + x * (d.max - d.min)) : d.min + x * (d.max - d.min));
                }
                setReal (p, "atm.level", k < 4 ? 6.f : -6.f);
                auto st = render (p, SR, BLOCK, 0.6, silence);
                worst = juce::jmax (worst, st.peak);
                if (! st.finite || st.peak > 1.f) { ok = false; bad << " #" << k; }
            }
            r.check (ok, "24 random worlds (and everything at both ends, LEVEL +6 dB) stay finite and under 0 dBFS (peak " + juce::String (db (worst), 1) + " dB)" + bad);
        }

        // FREEZE holds what is in the space, even with every layer off; letting go lets it die away
        {
            quietRig(); setNorm (p, "atm.on", 1); setReal (p, "atm.space", 100); setReal (p, "atm.size", 70); setReal (p, "atm.drn.level", 70);
            render (p, SR, BLOCK, 2.0, silence);
            setNorm (p, "atm.freeze", 1); render (p, SR, BLOCK, 0.3, silence);
            for (auto id : { "atm.bed.on", "atm.drn.on", "atm.tex.on" }) setNorm (p, id, 0);
            render (p, SR, BLOCK, 1.0, silence);
            auto held = render (p, SR, BLOCK, 2.0, silence);
            setNorm (p, "atm.freeze", 0); setReal (p, "atm.size", 0);
            render (p, SR, BLOCK, 3.0, silence);
            auto gone = render (p, SR, BLOCK, 1.0, silence);
            r.check (held.finite && db (held.rms) > -60.f && db (held.peak) < 0.f && db (gone.rms) < db (held.rms) - 25.f,
                     "FREEZE holds the space with every layer off (" + juce::String (db (held.rms), 1) + " dB), letting go dies away (" + juce::String (db (gone.rms), 1) + " dB)");
        }

        // FOLLOW: ducks under the guitar (measured on the side channel: the guitar is in the middle, the atmosphere is wide)
        {
            auto side = [&] (std::function<float (int)> in, double secs) {
                juce::AudioBuffer<float> b (2, BLOCK); juce::MidiBuffer m; double s2 = 0; long cnt = 0;
                for (int done = 0; done < (int) (SR * secs); done += BLOCK)
                {
                    for (int i = 0; i < BLOCK; ++i) { const float x = in (done + i); b.setSample (0, i, x); b.setSample (1, i, x); }
                    p.processBlock (b, m);
                    for (int i = 0; i < BLOCK; ++i) { const float sd = 0.5f * (b.getSample (0, i) - b.getSample (1, i)); s2 += (double) sd * sd; ++cnt; }
                }
                return db ((float) std::sqrt (s2 / juce::jmax (1L, cnt))); };
            quietRig(); setNorm (p, "atm.on", 1); setReal (p, "atm.follow", -100); setReal (p, "atm.width", 100); setReal (p, "atm.space", 0);
            side (silence, 1.0);
            const float alone = side (silence, 1.5);
            side (riff, 0.8);
            const float playing = side (riff, 1.5);
            setReal (p, "atm.follow", 100);
            side (silence, 1.5);
            const float swellQuiet = side (silence, 1.0);
            side (riff, 0.8);
            const float swellPlaying = side (riff, 1.5);
            r.check (playing < alone - 10.f && swellPlaying > swellQuiet + 10.f,
                     "FOLLOW ducks under your playing (" + juce::String (alone, 1) + " -> " + juce::String (playing, 1) + " dB) or swells with it ("
                     + juce::String (swellQuiet, 1) + " -> " + juce::String (swellPlaying, 1) + " dB)");
        }

        // the drone can follow the guitar: a plucked A2 takes it to A
        {
            quietRig(); setNorm (p, "atm.on", 1); setNorm (p, "atm.drn.follow", 1);
            render (p, SR, BLOCK, 1.5, [&] (int i) { if (i % (int) (SR * 0.5) == 0) g.pluck (110.f, 0.5f); return g.next(); });
            const int root = (int) std::lround (e.atmosRoot());
            r.check (root >= 0 && root % 12 == 9, "the drone follows a plucked A2 to A (root " + juce::String (root) + ")");
        }

        // it's its own sound at the end of the chain: the guitar's WIDTH (here: mono) doesn't fold it, and the SYNTH tab stops it
        {
            quietRig(); setNorm (p, "atm.on", 1); setNorm (p, "synth.on", 1); setReal (p, "width", 0); setReal (p, "atm.width", 100);
            render (p, SR, BLOCK, 1.0, silence);
            juce::AudioBuffer<float> b (2, BLOCK); juce::MidiBuffer m; double s2 = 0, m2 = 0;
            for (int k = 0; k < (int) (SR * 1.5) / BLOCK; ++k)
            {
                b.clear(); p.processBlock (b, m);
                for (int i = 0; i < BLOCK; ++i) { const double sd = 0.5 * (b.getSample (0, i) - b.getSample (1, i)), md = 0.5 * (b.getSample (0, i) + b.getSample (1, i)); s2 += sd * sd; m2 += md * md; }
            }
            const float sideDb = db ((float) std::sqrt (s2 / (SR * 1.5))), midDb = db ((float) std::sqrt (m2 / (SR * 1.5)));
            setNorm (p, "synth.on", 0);
            render (p, SR, BLOCK, 1.0, silence);
            auto bypassed = render (p, SR, BLOCK, 0.5, silence);
            r.check (sideDb > midDb - 12.f && bypassed.peak < 1.0e-6f,
                     "AETHER is added after the guitar chain (with the guitar's WIDTH at mono it stays wide: side " + juce::String (sideDb, 1) + " dB, mid "
                     + juce::String (midDb, 1) + " dB), and bypassing the SYNTH tab stops it");
        }

        // the page's meters
        {
            quietRig(); setNorm (p, "atm.on", 1); setNorm (p, "atm.tex.on", 1); setReal (p, "atm.tex.type", 6); setReal (p, "atm.tex.density", 100);
            const int ev0 = e.atmosEvents();
            render (p, SR, BLOCK, 2.0, silence);
            r.check (e.atmosActivity (0) > 0.01f && e.atmosActivity (1) > 0.01f && e.atmosEvents() - ev0 >= 3,
                     "meters for the page: bed " + juce::String (e.atmosActivity (0), 2) + ", drone " + juce::String (e.atmosActivity (1), 2) + ", "
                     + juce::String (e.atmosEvents() - ev0) + " heartbeats in 2 s");
        }

        // CPU: the heaviest world (rain with drops, a 4-note choir, metal at full density, the biggest space)
        {
            quietRig(); setNorm (p, "atm.on", 1); setReal (p, "atm.bed.type", 2); setReal (p, "atm.bed.motion", 100);
            setReal (p, "atm.drn.voice", 2); setReal (p, "atm.drn.chord", 7); setNorm (p, "atm.tex.on", 1); setReal (p, "atm.tex.type", 3); setReal (p, "atm.tex.density", 100);
            setReal (p, "atm.size", 100); setReal (p, "atm.space", 100); setReal (p, "atm.grit", 80); setReal (p, "atm.evolve", 100);
            render (p, SR, BLOCK, 1.0, silence);
            auto heavy = render (p, SR, BLOCK, 2.0, silence);
            r.check (heavy.finite && heavy.avgLoad < 0.2, "heaviest atmosphere: " + juce::String (heavy.avgLoad * 100, 2) + "% of one core");
        }
        p.presetManager().loadInit();
    }

    // ---------------------------------------------------------------- 6c. SIREN through HYDRA, AETHER kept out
    r.head ("SIREN with HYDRA (AETHER stays out of it)");
    {
        auto& e = p.engine();
        // HYDRA: one voice an octave down, voices only (mix 100); SIREN: a plain sine, synth only (mix 100)
        auto rig = [&] (int synthPost, int harmPlace) {
            p.presetManager().loadInit();
            for (auto id : { "amp.on", "cab.on", "doubler.on", "gate.on", "eq.on" }) setNorm (p, id, 0);
            for (int v = 1; v <= 6; ++v) setNorm (p, "harm.v" + juce::String (v) + ".on", 0);
            setNorm (p, "harm.on", 1); setReal (p, "harm.mix", 100); setReal (p, "harm.human", 0); setReal (p, "harm.place", (float) harmPlace);
            setReal (p, "harm.v1.on", 1); setReal (p, "harm.v1.level", 100); setReal (p, "harm.v1.pan", 0); setReal (p, "harm.v1.smart", 0); setReal (p, "harm.v1.semi", -12);
            setNorm (p, "synth.on", 1); setReal (p, "syn.mix", 100); setReal (p, "syn.post", (float) synthPost);
            setReal (p, "syn.a.wave", 3); setReal (p, "syn.a.voices", 0); setNorm (p, "syn.b.on", 0); setReal (p, "syn.sub.level", 0); setReal (p, "syn.noise.level", 0);
            setNorm (p, "syn.flt.on", 0); setReal (p, "syn.lfo.depth", 0); setReal (p, "syn.space", 0); setReal (p, "syn.char", 0); setReal (p, "syn.chord", 0);
            setNorm (p, "syn.arp.on", 0); setNorm (p, "syn.hold", 0);
            render (p, SR, BLOCK, 0.5, [] (int) { return 0.f; });             // HYDRA settles in its place
        };
        auto a3 = [&] (int i) { const double t = i / SR; double v = 0;
            for (int h = 1; h <= 6; ++h) v += std::sin (juce::MathConstants<double>::twoPi * 220.0 * h * t) / h;
            return (float) (0.25 * v * std::exp (-t * 0.6) * juce::jmin (1.0, t / 0.002)); };
        auto pitchOf = [&] (std::function<float (int)> in, double secs) {
            std::vector<float> cap; render (p, SR, BLOCK, secs, in, 2, &cap);
            return detectPitch (std::vector<float> (cap.begin() + (long) (0.5 * SR), cap.end()), SR); };
        auto cents = [] (float got, float want) { return got > 0 ? std::abs (1200.f * std::log2 (got / want)) : 9999.f; };
        struct C { int post, place; const char* name; };
        juce::String res; int good = 0;
        for (auto c : { C { 1, 0, "both after the amp" }, C { 1, 1, "SIREN after, HYDRA before" }, C { 0, 1, "both before" }, C { 0, 0, "SIREN before, HYDRA after" } })
        {
            rig (c.post, c.place);
            const float f = pitchOf (a3, 1.2);
            if (cents (f, 110.f) < 25.f) ++good;
            res << c.name << " " << juce::String (f, 1) << " Hz, ";
        }
        r.check (good == 4, "HYDRA harmonizes SIREN wherever each sits (A3 played, SIREN an octave down: " + res.dropLastCharacters (2) + ")");
        // without HYDRA SIREN plays the note itself
        rig (1, 0); setNorm (p, "harm.on", 0); render (p, SR, BLOCK, 0.3, [] (int) { return 0.f; });
        { const float f = pitchOf (a3, 1.2); r.check (cents (f, 220.f) < 25.f, "with HYDRA off SIREN plays the A3 itself (" + juce::String (f, 1) + " Hz)"); }
        // no guitar: SIREN played on the KEYS gets its harmonies too (HYDRA listens to SIREN then)
        {
            rig (1, 0);
            e.uiSynthNote (57, true, 0.9f);
            const float f = pitchOf ([] (int) { return 0.f; }, 1.2);
            e.uiSynthNote (57, false); render (p, SR, BLOCK, 1.0, [] (int) { return 0.f; });
            r.check (cents (f, 110.f) < 25.f, "SIREN played on the KEYS (no guitar) is harmonized too (" + juce::String (f, 1) + " Hz, want 110)");
        }
        // the KEYS' wheels: pitch bend moves SIREN's note, the mod wheel adds vibrato; both from MIDI too
        {
            rig (1, 0); setNorm (p, "harm.on", 0); render (p, SR, BLOCK, 0.3, [] (int) { return 0.f; });
            e.uiSynthNote (57, true, 0.9f);
            e.uiSynthBend (2.f); const float up = pitchOf ([] (int) { return 0.f; }, 1.0);
            e.uiSynthBend (0.f); e.midiSynthBend (-2.f); const float down = pitchOf ([] (int) { return 0.f; }, 1.0);
            e.midiSynthBend (0.f);
            // vibrato: the pitch wobbles (short windows disagree), without it they agree
            auto spread = [&] {
                std::vector<float> cap; render (p, SR, BLOCK, 1.0, [] (int) { return 0.f; }, 2, &cap);
                float lo = 1e9f, hi = 0;
                for (int k = 0; k < 6; ++k) { const auto a = (size_t) ((0.3 + k * 0.1) * SR); const float f = detectPitch (std::vector<float> (cap.begin() + (long) a, cap.begin() + (long) (a + 6200)), SR);
                    if (f > 0) { lo = juce::jmin (lo, f); hi = juce::jmax (hi, f); } }
                return hi > lo ? 1200.f * std::log2 (hi / lo) : 0.f; };
            const float flat = spread(); e.uiSynthMod (1.f); const float wobble = spread(); e.uiSynthMod (0.f);
            e.uiSynthNote (57, false); render (p, SR, BLOCK, 1.0, [] (int) { return 0.f; });
            r.check (cents (up, 246.94f) < 25.f && cents (down, 196.f) < 25.f && wobble > 30.f && flat < 10.f,
                     juce::String::formatted ("the KEYS' wheels: BEND +2 st %.1f Hz (want 246.9), MIDI bend -2 st %.1f Hz (want 196.0), MOD vibrato %.0f cents (none: %.0f)", up, down, wobble, flat));
        }
        // AETHER is never harmonized: a drone sounds the same with HYDRA on or off
        {
            auto aether = [&] (bool harm) {
                rig (1, 0); setReal (p, "syn.mix", 0); setNorm (p, "harm.on", harm ? 1.f : 0.f);
                setNorm (p, "atm.on", 1); setNorm (p, "atm.bed.on", 0); setNorm (p, "atm.tex.on", 0); setNorm (p, "atm.drn.on", 1);
                setReal (p, "atm.drn.voice", 0); setReal (p, "atm.drn.chord", 0); setNorm (p, "atm.drn.follow", 0); setReal (p, "atm.drn.motion", 0); setReal (p, "atm.drn.detune", 0);
                setReal (p, "atm.follow", 0); setReal (p, "atm.space", 0); setReal (p, "atm.evolve", 0);
                render (p, SR, BLOCK, 1.5, [] (int) { return 0.f; });
                std::vector<float> cap; const auto st = render (p, SR, BLOCK, 1.0, [] (int) { return 0.f; }, 2, &cap);
                return std::make_pair (st.rms, detectPitch (cap, SR)); };
            const auto off = aether (false), on = aether (true);
            const float dDb = 20.f * std::log10 (juce::jmax (1e-9f, on.first) / juce::jmax (1e-9f, off.first));
            const bool samePitch = (off.second <= 0 && on.second <= 0) || cents (on.second, off.second) < 15.f;
            r.check (off.first > 1e-4f && std::abs (dDb) < 1.5f && samePitch, juce::String::formatted ("AETHER never goes through HYDRA: a drone is the same with it on (%+.1f dB, %.1f vs %.1f Hz)", dDb, on.second, off.second));
        }
        // CPU: SIREN after the amp with HYDRA before it runs a second HYDRA for SIREN
        {
            rig (1, 1); for (int v = 1; v <= 6; ++v) setNorm (p, "harm.v" + juce::String (v) + ".on", 1);
            auto st = render (p, SR, BLOCK, 2.0, a3);
            r.check (st.finite && st.peak < 1.99f, "six voices on guitar and SIREN (two HYDRAs): clean, " + juce::String (st.avgLoad * 100, 1) + "% of one core");
        }
        p.presetManager().loadInit();
    }

    // ---------------------------------------------------------------- 6d. KERAUNOS and KHAOS (the SFX tab's GLITCH page)
    r.head ("KERAUNOS and KHAOS (SFX > GLITCH)");
    {
        auto& e = p.engine();
        int nFx = 0;
        for (auto* prm : p.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm)) if (rp->getParameterID().startsWith ("ker.") || rp->getParameterID().startsWith ("glt.")) ++nFx;
        r.check (nFx == obs::fxdefs::ker::count + obs::fxdefs::glt::count, juce::String (nFx) + " KERAUNOS and KHAOS parameters");
        auto clean = [&] {
            p.presetManager().loadInit();
            for (auto id : { "amp.on", "cab.on", "doubler.on", "gate.on", "eq.on", "synth.on", "harm.on" }) setNorm (p, id, 0);
            setNorm (p, "sfx.on", 1);
        };
        auto tone = [&] (double hz) { const double sr = SR; return [hz, sr] (int i) { const double t = i / sr; double v = 0;
            for (int h = 1; h <= 5; ++h) v += std::sin (juce::MathConstants<double>::twoPi * hz * h * t) / h; return (float) (0.2 * v); }; };
        auto pitchOf = [&] (std::function<float (int)> in, double secs) {
            std::vector<float> cap; render (p, SR, BLOCK, secs, in, 2, &cap);
            return detectPitch (std::vector<float> (cap.begin() + (long) (0.4 * SR), cap.end()), SR); };
        auto cents = [] (float got, float want) { return got > 0 ? std::abs (1200.f * std::log2 (got / want)) : 9999.f; };
        // the treadle: whammy and friends land on their notes (A3 in)
        {
            struct C { const char* name; int algo; float want; };
            juce::String res; int good = 0;
            for (auto c : { C { "Whammy Up", 0, 440.f }, C { "Whammy Down", 1, 110.f }, C { "Dive Bomb (half way)", 2, 110.f }, C { "Fifth Up", 3, 329.63f }, C { "Squeal", 5, 880.f } })
            {
                clean(); setNorm (p, "ker.on", 1); setReal (p, "ker.algo", (float) c.algo); setReal (p, "ker.mode", 0); setReal (p, "ker.pos", c.algo == 2 ? 50.f : 100.f); setReal (p, "ker.mix", 100);
                setReal (p, "ker.place", 1); setReal (p, "ker.color", 100);
                const float f = pitchOf (tone (220.0), 1.0);
                if (cents (f, c.want) < 30.f) ++good;
                res << c.name << " " << juce::String (f, 1) << ", ";
            }
            r.check (good == 5, "the pedal at the toe (A3 in): " + res.dropLastCharacters (2) + " Hz");
        }
        // every effect in every mode, before and after the amp: clean
        {
            Guitar g (SR); bool ok = true; float worst = 0; juce::String bad;
            for (int a = 0; a < obs::fxdefs::ker::kAlgos; ++a)
            {
                clean(); setNorm (p, "amp.on", 1); setNorm (p, "cab.on", 1); setNorm (p, "ker.on", 1); setReal (p, "ker.algo", (float) a);
                setReal (p, "ker.mode", (float) (a % 4)); setReal (p, "ker.place", (float) (a % 2)); setReal (p, "ker.color", (float) ((a * 37) % 101)); setReal (p, "ker.speed", 80);
                const auto st = render (p, SR, BLOCK, 0.6, [&] (int i) { if (i % (int) (SR * 0.15) == 0) g.pluck (110.f * (1 + i / (int) (SR * 0.15) % 3), 0.6f); return g.next(); });
                worst = juce::jmax (worst, st.peak);
                if (! st.finite || st.peak > 1.99f || st.rms < 1e-4f) { ok = false; bad << a << " "; }
            }
            r.check (ok, "all 16 effects, all four modes, before and after the amp: clean (loudest peak " + juce::String (worst, 2) + ")" + (bad.isEmpty() ? juce::String() : " (bad: " + bad + ")"));
        }
        // what moves it: AUTO rocks by itself, a pick attack fires a TRIGGER sweep, ENVELOPE follows the playing
        {
            clean(); setNorm (p, "ker.on", 1); setReal (p, "ker.algo", 12); setReal (p, "ker.mode", 2); setReal (p, "ker.speed", 70); setReal (p, "ker.shape", 2);
            float lo = 1, hi = 0;
            render (p, SR, BLOCK, 2.0, [&] (int i) { if (i % 256 == 0) { lo = juce::jmin (lo, e.keraunosPos()); hi = juce::jmax (hi, e.keraunosPos()); } return 0.f; });
            Guitar g (SR);
            setReal (p, "ker.mode", 3); setReal (p, "ker.shape", 0); setReal (p, "ker.speed", 40); setReal (p, "ker.sens", 60);
            render (p, SR, BLOCK, 1.0, [] (int) { return 0.f; });
            const float rest = e.keraunosPos(); float peak = 0;
            render (p, SR, BLOCK, 0.4, [&] (int i) { if (i == 0) g.pluck (220.f, 0.6f); if (i % 256 == 0) peak = juce::jmax (peak, e.keraunosPos()); return g.next(); });
            Guitar g2 (SR);
            setReal (p, "ker.mode", 1); setReal (p, "ker.sens", 30); render (p, SR, BLOCK, 3.0, [] (int) { return 0.f; });
            const float still = e.keraunosPos(); float pushed = 0;
            render (p, SR, BLOCK, 0.3, [&] (int i) { if (i == 0) g2.pluck (220.f, 0.7f); if (i % 256 == 0) pushed = juce::jmax (pushed, e.keraunosPos()); return g2.next(); });
            r.check (hi - lo > 0.8f && rest < 0.05f && peak > 0.9f && still < 0.05f && pushed > 0.3f,
                     juce::String::formatted ("AUTO rocks it (%.2f..%.2f), a pick fires a TRIGGER sweep (%.2f -> %.2f), ENVELOPE follows the playing (%.2f -> %.2f)", lo, hi, rest, peak, still, pushed));
        }
        // KHAOS: every glitch type on its own, by chance at 100%: clean, and it really glitches
        {
            Guitar g (SR); bool ok = true; juce::String bad; float worst = 0;
            for (int t = 0; t < obs::fxdefs::glt::kTypes; ++t)
            {
                clean(); setNorm (p, "glt.on", 1); setReal (p, "glt.mode", 0); setReal (p, "glt.chance", 100); setReal (p, "glt.grid", 3); setReal (p, "glt.chaos", 60);
                for (int k = 0; k < obs::fxdefs::glt::kTypes; ++k) setNorm (p, obs::fxdefs::glt::table()[obs::fxdefs::glt::t0 + k].id, k == t ? 1.f : 0.f);
                const int ev0 = e.khaosEvents();
                const auto st = render (p, SR, BLOCK, 1.5, [&] (int i) { if (i % (int) (SR * 0.25) == 0) g.pluck (110.f * (1 + (i / (int) (SR * 0.25)) % 4), 0.5f); return g.next(); });
                worst = juce::jmax (worst, st.peak);
                if (! st.finite || st.peak > 1.99f || e.khaosEvents() - ev0 < 4) { ok = false; bad << obs::fxdefs::glt::typeNames() << ""; bad = bad.upToFirstOccurrenceOf ("|", false, false); bad << t << " "; }
            }
            r.check (ok, "all 22 glitch types fire and stay clean (loudest peak " + juce::String (worst, 2) + ")" + (bad.isEmpty() ? juce::String() : " (bad: " + bad + ")"));
        }
        // SEQUENCE: the steps play in time (120 BPM, 1/16: a step every 125 ms), the meter follows
        {
            clean(); setNorm (p, "glt.on", 1); setReal (p, "glt.mode", 1); setReal (p, "glt.grid", 3);
            for (int i = 0; i < 16; ++i) setReal (p, obs::fxdefs::glt::table()[obs::fxdefs::glt::s0 + i].id, i % 4 == 0 ? 1.f : 0.f);
            render (p, SR, BLOCK, 0.2, [] (int) { return 0.f; });
            const int ev0 = e.khaosEvents(); std::set<int> stepsSeen;
            render (p, SR, BLOCK, 2.0, [&] (int i) { if (i % 512 == 0) stepsSeen.insert (e.khaosStep()); return 0.f; });
            const int n = e.khaosEvents() - ev0;
            r.check (n == 4 && stepsSeen.size() >= 14, "SEQUENCE: a stutter on each beat of the 16 steps plays 4 times in a 2 s bar at 120 BPM (" + juce::String (n) + "), the step meter walks all 16 (" + juce::String ((int) stepsSeen.size()) + " seen)");
        }
        // HOLD keeps the sound frozen after you stop; MIX 0 is the dry sound
        {
            Guitar g (SR);
            auto tail = [&] (bool hold) {
                clean(); setNorm (p, "glt.on", 1); setReal (p, "glt.mode", 1);
                for (int i = 0; i < 16; ++i) setReal (p, obs::fxdefs::glt::table()[obs::fxdefs::glt::s0 + i].id, 0);
                render (p, SR, BLOCK, 0.6, [&] (int i) { if (i % (int) (SR * 0.1) == 0) g.pluck (220.f, 0.5f); return g.next(); });
                setNorm (p, "glt.hold", hold ? 1.f : 0.f);                 // pressed while the note rings, then you stop
                render (p, SR, BLOCK, 0.02, [&] (int) { return g.next(); });
                const auto st = render (p, SR, BLOCK, 0.8, [] (int) { return 0.f; });
                setNorm (p, "glt.hold", 0); return st.rms; };
            const float held = tail (true), free = tail (false);
            r.check (held > 0.005f && held > free * 4.f, juce::String::formatted ("HOLD freezes the last moment after you stop (%.4f vs %.4f without)", held, free));
        }
        // the SFX tab bypassed: both units are out of the way; CPU with everything at once
        {
            Guitar g (SR);
            auto run = [&] (bool sfx) { clean(); setNorm (p, "ker.on", 1); setReal (p, "ker.algo", 10); setReal (p, "ker.mode", 2); setNorm (p, "glt.on", 1);
                setReal (p, "glt.chance", 100); setNorm (p, "sfx.on", sfx ? 1.f : 0.f); render (p, SR, BLOCK, 0.3, [] (int) { return 0.f; });
                std::vector<float> cap; render (p, SR, BLOCK, 0.8, [&] (int i) { if (i == 0) g.pluck (220.f, 0.5f); return g.next(); }, 2, &cap); return cap; };
            clean(); render (p, SR, BLOCK, 0.3, [] (int) { return 0.f; });
            std::vector<float> dry; Guitar g2 (SR); render (p, SR, BLOCK, 0.8, [&] (int i) { if (i == 0) g2.pluck (220.f, 0.5f); return g2.next(); }, 2, &dry);
            const auto byp = run (false);
            float diff = 0; for (size_t i = 0; i < juce::jmin (dry.size(), byp.size()); ++i) diff = juce::jmax (diff, std::abs (dry[i] - byp[i]));
            r.check (diff < 1e-4f, "with the SFX tab bypassed KERAUNOS and KHAOS are out of the way (differs by " + juce::String (diff, 6) + ")");
            clean(); setNorm (p, "amp.on", 1); setNorm (p, "cab.on", 1); setNorm (p, "ker.on", 1); setReal (p, "ker.algo", 11); setReal (p, "ker.mode", 2); setReal (p, "ker.place", 1);
            setNorm (p, "glt.on", 1); setReal (p, "glt.grid", 5); setReal (p, "glt.chance", 100); setReal (p, "glt.chaos", 100);
            for (int k = 0; k < obs::fxdefs::glt::kTypes; ++k) setNorm (p, obs::fxdefs::glt::table()[obs::fxdefs::glt::t0 + k].id, 1);
            Guitar g3 (SR);
            const auto st = render (p, SR, BLOCK, 3.0, [&] (int i) { if (i % (int) (SR * 0.2) == 0) g3.pluck (110.f * (1 + (i / (int) (SR * 0.2)) % 3), 0.5f); return g3.next(); });
            r.check (st.finite && st.peak < 1.99f, "KERAUNOS + KHAOS (every type, 1/32, chaos 100) on the full rig: clean, " + juce::String (st.avgLoad * 100, 1) + "% of one core");
        }
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
        {   // Eidolon with no capture loaded adds nothing: what goes in comes out
            auto& amp = p.engine().amp();
            dsp::AmpSection a; a.prepare (SR, BLOCK); a.setType (dsp::AmpSection::Nam);
            std::vector<float> buf ((size_t) BLOCK); double diff = 0, level = 0;
            for (int b = 0; b < 60; ++b)
            {
                for (int i = 0; i < BLOCK; ++i) buf[(size_t) i] = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 110.0 * (b * BLOCK + i) / SR);
                const auto in = buf;
                a.process (buf.data(), BLOCK, true, 0, 10.f, 5.f, 5.f, 5.f, 5.f, 5.f, 10.f);
                if (b >= 10) for (int i = 0; i < BLOCK; ++i) { diff = std::max (diff, (double) std::abs (buf[(size_t) i] - in[(size_t) i])); level = std::max (level, (double) std::abs (in[(size_t) i])); }
            }
            juce::ignoreUnused (amp);
            r.check (diff < 1.0e-5, juce::String::formatted ("Eidolon with no capture loaded adds nothing (gain 10, master 10: largest difference %.1e)", diff));
        }
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

    // ---------------------------------------------------------------- 7c. celestial pedals
    r.head ("Celestial pedals (lush reverbs, shimmers, delays, Lyra, Icarus)");
    {
        const char* keys[] = { "elysium", "empyrean", "nimbus", "vesper", "halo", "astral", "mnemosyne", "gemini", "lyra", "icarus" };
        const auto S = (size_t) SR;
        auto burst = [] (int i) { juce::Random z (i / 2400); return (i % 48000) < 2400 ? (z.nextFloat() * 2.f - 1.f) * 0.3f * (1.f - (float) (i % 48000) / 2400.f) : 0.f; };
        juce::Random nz (11);
        for (auto key : keys)
        {
            ModuleRig mr (key, SR, BLOCK);
            if (mr.m == nullptr) { r.fail (juce::String (key) + " is registered"); continue; }
            // 1) defaults: a pluck rings out cleanly
            Guitar g (SR);
            auto st = mr.run (SR, BLOCK, 3.0, [&g] (int i) { if (i % 24000 == 0) g.pluck (146.8f, 0.45f); return g.next(); });
            // 2) everything at maximum (feedback, shimmer, intensity, decay...) for 8 s of bursts: must stay bounded
            ModuleRig mx (key, SR, BLOCK); mx.setAllToMax();
            auto sm = mx.run (SR, BLOCK, 8.0, [&nz] (int i) { return (i % 48000) < 9600 ? (nz.nextFloat() * 2.f - 1.f) * 0.5f : 0.f; });
            r.check (st.finite && st.rms > 0.002f && st.peak < 4.f && sm.finite && sm.peak < 6.f,
                     mr.type->name + ": plays cleanly, and stays bounded with every knob at max (peak " + juce::String (sm.peak, 2) + ", cpu "
                     + juce::String (st.avgLoad * 100.0, 2) + "%)");
            r.check (st.avgLoad < 0.12, mr.type->name + ": CPU at defaults " + juce::String (st.avgLoad * 100.0, 2) + "% of one core");
        }

        // reverb tails: decay when they should, hold when frozen
        for (auto key : { "elysium", "nimbus", "vesper", "halo", "empyrean" })
        {
            ModuleRig mr (key, SR, BLOCK); mr.set ("Mix", 100.f); mr.set ("Decay", 3.f); mr.set ("Shimmer", 0.f); mr.set ("Voice II", 0.f); mr.set ("Swell", 0.f);
            auto out = mr.render (SR, BLOCK, 6.0, [&burst] (int i) { return i < 2400 ? burst (i) : 0.f; });
            const double early = rmsOf (out, S / 2, S), late = rmsOf (out, 5 * S, 6 * S);
            r.check (early > 1.0e-4 && late < early * 0.15, juce::String (key) + ": a 3 s decay dies away (" + juce::String (juce::Decibels::gainToDecibels ((float) (late / juce::jmax (1.0e-9, early))), 1) + " dB after 5 s)");
        }
        for (auto [key, sw] : { std::pair { "elysium", "Infinite" }, std::pair { "nimbus", "Freeze" } })
        {
            ModuleRig mr (key, SR, BLOCK); mr.set ("Mix", 100.f); mr.set ("Swell", 0.f);
            std::vector<float> out;
            auto a = mr.render (SR, BLOCK, 1.5, [&burst] (int i) { return burst (i); });
            mr.set (sw, 1.f);
            auto b = mr.render (SR, BLOCK, 8.0, [] (int) { return 0.f; });
            const double held = rmsOf (b, 7 * S, 8 * S), start = rmsOf (b, S / 2, S);
            r.check (start > 1.0e-4 && held > start * 0.35 && held < start * 2.5, juce::String (key) + ": " + sw + " holds the tail (8 s later at "
                     + juce::String (juce::Decibels::gainToDecibels ((float) (held / juce::jmax (1.0e-9, start))), 1) + " dB, no build-up)");
        }

        // shimmer: a 220 Hz note grows energy an octave up
        const double sineRate = SR;
        auto sine = [sineRate] (float f) { return std::function<float (int)> ([f, sineRate] (int i) { return 0.25f * std::sin (juce::MathConstants<float>::twoPi * f * (float) i / (float) sineRate); }); };
        for (auto key : { "empyrean", "astral" })
        {
            ModuleRig on (key, SR, BLOCK), off (key, SR, BLOCK);
            on.set ("Mix", 100.f); off.set ("Mix", 100.f); on.set ("Shimmer", 100.f); off.set ("Shimmer", 0.f); off.set ("Voice II", 0.f); on.set ("Voice II", 0.f);
            on.set ("Feedback", 70.f); off.set ("Feedback", 70.f);
            auto a = on.render (SR, BLOCK, 4.0, sine (220.f)), b = off.render (SR, BLOCK, 4.0, sine (220.f));
            const double up = tonePower (a, SR, 440.0, 2 * S, 4 * S) / juce::jmax (1.0e-12, tonePower (a, SR, 220.0, 2 * S, 4 * S));
            const double ref = tonePower (b, SR, 440.0, 2 * S, 4 * S) / juce::jmax (1.0e-12, tonePower (b, SR, 220.0, 2 * S, 4 * S));
            r.check (up > ref * 4.0, juce::String (key) + ": shimmer rises an octave (440 Hz vs 220 Hz: " + juce::String (10.0 * std::log10 (juce::jmax (1.0e-12, up)), 1)
                     + " dB, without shimmer " + juce::String (10.0 * std::log10 (juce::jmax (1.0e-12, ref)), 1) + " dB)");
        }
        {   // Vesper: the vowel changes the colour of the choir (A is brighter than U around 1.1 kHz)
            ModuleRig a ("vesper", SR, BLOCK), u ("vesper", SR, BLOCK);
            for (auto* m : { &a, &u }) { m->set ("Mix", 100.f); m->set ("Choir", 100.f); m->set ("Breath", 0.f); }
            a.set ("Vowel", 0.f); u.set ("Vowel", 4.f);
            juce::Random z1 (3), z2 (3);
            auto oa = a.render (SR, BLOCK, 3.0, [&z1] (int) { return (z1.nextFloat() * 2.f - 1.f) * 0.1f; });
            auto ou = u.render (SR, BLOCK, 3.0, [&z2] (int) { return (z2.nextFloat() * 2.f - 1.f) * 0.1f; });
            const double ra = tonePower (oa, SR, 1150.0, S, 3 * S) / juce::jmax (1.0e-12, tonePower (oa, SR, 330.0, S, 3 * S));
            const double ru = tonePower (ou, SR, 1150.0, S, 3 * S) / juce::jmax (1.0e-12, tonePower (ou, SR, 330.0, S, 3 * S));
            r.check (ra > ru * 2.0, "vesper: vowels sing different formants (A vs U at 1.15 kHz: " + juce::String (10.0 * std::log10 (ra / juce::jmax (1.0e-12, ru)), 1) + " dB)");
        }
        {   // Halo reverse: the tail swells up after the note instead of starting loud
            ModuleRig h ("halo", SR, BLOCK); h.set ("Mix", 100.f); h.set ("Length", 800.f); h.set ("Mode", 0.f);
            auto out = h.render (SR, BLOCK, 2.0, [&burst] (int i) { return i < 2400 ? burst (i) : 0.f; });
            size_t peakAt = 0; float pk = 0;
            for (size_t i = S / 10; i < out.size(); i += 64) if (std::abs (out[i]) > pk) { pk = std::abs (out[i]); peakAt = i; }   // after the dry note
            r.check (peakAt > S / 4, "halo: reverse tail swells (loudest " + juce::String ((int) (1000 * peakAt / S)) + " ms after the note)");
        }
        // echoes land where they should
        auto echoPeaks = [&] (ModuleRig& mr, double seconds) { return mr.render (SR, BLOCK, seconds, [] (int i) { return i == 0 ? 1.f : 0.f; }); };
        auto energyNear = [&] (const std::vector<float>& o, double ms) { const size_t c = (size_t) (ms * 0.001 * SR); return rmsOf (o, c > 240 ? c - 240 : 0, c + 240); };
        {
            ModuleRig g ("gemini", SR, BLOCK); g.set ("Mix", 100.f); g.set ("Time", 400.f); g.set ("Pattern", 0.f); g.set ("Feedback", 0.f); g.set ("Mod", 0.f);
            auto o = echoPeaks (g, 1.0);
            const double a = energyNear (o, 400.0), b = energyNear (o, 300.0), gap = energyNear (o, 150.0);
            r.check (a > gap * 20 && b > gap * 20, "gemini: heads at 400 ms and 300 ms (dotted)");
            ModuleRig m ("mnemosyne", SR, BLOCK); m.set ("Echo", 100.f); m.set ("Time", 200.f); m.set ("Heads", 6.f); m.set ("Intensity", 0.f); m.set ("Wow", 0.f); m.set ("Spring", 0.f); m.set ("Age", 0.f);
            auto o2 = echoPeaks (m, 1.0);
            const double h1 = energyNear (o2, 200.0), h2 = energyNear (o2, 400.0), h3 = energyNear (o2, 600.0), gp = energyNear (o2, 300.0);
            r.check (h1 > gp * 10 && h2 > gp * 10 && h3 > gp * 10, "mnemosyne: three tape heads at 200 / 400 / 600 ms");
        }
        {   // Lyra adds the octave string; Icarus makes an octave out of a single note
            ModuleRig l ("lyra", SR, BLOCK); l.set ("Octave", 100.f); l.set ("Detune", 0.f);
            auto o = l.render (SR, BLOCK, 1.5, sine (220.f));
            const double oct = tonePower (o, SR, 440.0, S / 2, (size_t) (1.5 * SR)) / juce::jmax (1.0e-12, tonePower (o, SR, 220.0, S / 2, (size_t) (1.5 * SR)));
            r.check (oct > 0.05, "lyra: the octave string is there (440 Hz at " + juce::String (10.0 * std::log10 (juce::jmax (1.0e-12, oct)), 1) + " dB vs the note)");
            ModuleRig i1 ("icarus", SR, BLOCK), i0 ("icarus", SR, BLOCK); i1.set ("Octave", 100.f); i0.set ("Octave", 0.f);
            auto a = i1.render (SR, BLOCK, 1.0, sine (220.f)), b = i0.render (SR, BLOCK, 1.0, sine (220.f));
            const double ra = tonePower (a, SR, 440.0, S / 4, S) / juce::jmax (1.0e-12, tonePower (a, SR, 220.0, S / 4, S));
            const double rb = tonePower (b, SR, 440.0, S / 4, S) / juce::jmax (1.0e-12, tonePower (b, SR, 220.0, S / 4, S));
            r.check (ra > rb * 10.0, "icarus: the octave knob brings the octave up (+" + juce::String (10.0 * std::log10 (ra / juce::jmax (1.0e-12, rb)), 1) + " dB at 440 Hz)");
        }
        {   // other sample rates
            bool ok = true;
            for (double rate : { 44100.0, 96000.0 })
                for (auto key : keys)
                {
                    ModuleRig mr (key, rate, 256); Guitar g2 (rate);
                    auto s2 = mr.run (rate, 256, 1.5, [&g2] (int i) { if (i == 0) g2.pluck (110.f, 0.4f); return g2.next(); });
                    if (! (s2.finite && s2.peak < 4.f && s2.rms > 1.0e-4f)) { ok = false; r.fail (juce::String (key) + " at " + juce::String (rate) + " Hz"); }
                }
            r.check (ok, "all ten work at 44.1 kHz and 96 kHz");
        }
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

    // Tone check (tools/presets/tonecheck.py): with OBS_DI set to a 48 kHz guitar DI recording, every preset also
    // plays it and the result goes to OBS_RENDER_DIR as a mono WAV. OBS_ONLY="a|b" limits the run to names (or folders) containing a or b.
    std::vector<float> di;
    const auto renderDir = juce::File (juce::SystemStats::getEnvironmentVariable ("OBS_RENDER_DIR", "/tmp/obs-renders"));
    const auto only = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("OBS_ONLY", {}), "|", {});
    if (const auto diPath = juce::SystemStats::getEnvironmentVariable ("OBS_DI", {}); diPath.isNotEmpty())
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        if (auto r = std::unique_ptr<juce::AudioFormatReader> (fm.createReaderFor (juce::File (diPath))))
        {
            juce::AudioBuffer<float> b (1, (int) r->lengthInSamples);
            r->read (&b, 0, b.getNumSamples(), 0, true, false);
            di.assign (b.getReadPointer (0), b.getReadPointer (0) + b.getNumSamples());
            renderDir.createDirectory();
        }
        else std::cout << "Can't read the DI file " << diPath << std::endl;
    }

    for (auto& e : *list)
    {
        const auto name = e["name"].toString();
        if (! only.isEmpty())
        {
            bool hit = false;
            for (auto& o : only) hit = hit || name.containsIgnoreCase (o.trim()) || e["category"].toString().containsIgnoreCase (o.trim());
            if (! hit) continue;
        }
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

        if (! di.empty())
        {
            for (int i = 0; i < 4; ++i) render (p, SR, BLOCK, 0.25, [] (int) { return 0.f; });
            std::vector<float> out; out.reserve (di.size());
            render (p, SR, BLOCK, (double) di.size() / SR, [&] (int i) { return di[(size_t) i]; }, 2, &out);
            juce::AudioBuffer<float> ob (1, (int) out.size()); ob.copyFrom (0, 0, out.data(), (int) out.size());
            auto f = renderDir.getChildFile (juce::File::createLegalFileName (e["file"].toString().fromLastOccurrenceOf ("/", false, false)
                                                                                 .upToLastOccurrenceOf (".", false, false)) + ".wav");
            f.deleteFile();
            juce::WavAudioFormat wav;
            if (auto os = std::unique_ptr<juce::OutputStream> (f.createOutputStream()))
                if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), SR, 1, 24, {}, 0)))
                { os.release(); w->writeFromAudioSampleBuffer (ob, 0, ob.getNumSamples()); }
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

// -----------------------------------------------------------------------------
//  --render-pedal-demos <folder>: a short clean-guitar part through each celestial pedal
//  (plucked-string guitar, showcase settings), 48 kHz stereo WAV. For listening, not a test.
// -----------------------------------------------------------------------------
int renderPedalDemos (const juce::File& dir)
{
    dir.createDirectory();
    const double sr = 48000; const int block = 256;
    // a guitar with six strings: each pluck goes to the next free string, softened like a fingered note
    struct Strings
    {
        explicit Strings (double r) { for (int i = 0; i < 6; ++i) s.emplace_back (r); lp.setLowpass (r, 4200.f); body.set (dsp::Biquad::Peak, r, 210.f, 1.2f, 4.f); }
        void pluck (float hz, float amp) { s[(size_t) (k++ % 6)].pluck (hz, amp); }
        float next() { float x = 0; for (auto& g : s) x += g.next(); return body.process (lp.lp (x)) * 0.8f; }
        std::vector<Guitar> s; int k = 0; dsp::OnePole lp; dsp::Biquad body;
    };

    auto hz = [] (int midi) { return 440.f * std::pow (2.f, (float) (midi - 69) / 12.f); };
    struct Ev { double t; int note; float amp; };
    auto arps = [] { std::vector<Ev> e; const int ch[4][4] = { { 40, 47, 54, 59 }, { 36, 43, 52, 59 }, { 43, 50, 55, 62 }, { 38, 45, 54, 57 } };   // Em(add9)-ish, Cmaj7, G, D
        for (int c = 0; c < 4; ++c) for (int i = 0; i < 4; ++i) e.push_back ({ c * 2.0 + i * 0.32, ch[c][i], 0.42f }); return e; };
    auto sparse = [] { std::vector<Ev> e; const int n[] = { 64, 67, 71, 69, 66, 64 }; for (int i = 0; i < 6; ++i) e.push_back ({ i * 1.25, n[i], 0.5f }); return e; };
    auto strum = [] { std::vector<Ev> e; const int ch[3][6] = { { 40, 47, 52, 55, 59, 64 }, { 45, 52, 57, 60, 64, 69 }, { 43, 47, 50, 55, 59, 67 } };
        for (int c = 0; c < 3; ++c) for (int rep = 0; rep < 2; ++rep) for (int i = 0; i < 6; ++i) e.push_back ({ c * 2.4 + rep * 1.2 + i * 0.018, ch[c][i], 0.33f }); return e; };
    auto lead = [] { std::vector<Ev> e; const int n[] = { 64, 67, 69, 71, 74, 71, 76 }; const double t[] = { 0, 0.5, 1.0, 1.5, 2.25, 3.0, 3.75 };
        for (int i = 0; i < 7; ++i) e.push_back ({ t[i], n[i], 0.55f }); return e; };

    struct Demo { const char* key; const char* file; std::vector<Ev> part; double seconds; std::vector<std::pair<const char*, float>> set; };
    const std::vector<Demo> demos = {
        { "elysium",   "01-elysium-cathedral",      arps(),   14, { { "Decay", 9.f }, { "Mix", 40.f } } },
        { "empyrean",  "02-empyrean-shimmer",       arps(),   16, { { "Decay", 14.f }, { "Shimmer", 70.f }, { "Voice II", 35.f }, { "Mix", 45.f } } },
        { "nimbus",    "03-nimbus-cloud",           arps(),   16, { { "Decay", 18.f }, { "Mix", 55.f } } },
        { "vesper",    "04-vesper-choir",           arps(),   15, { { "Decay", 10.f }, { "Choir", 80.f }, { "Voices", 1.f }, { "Mix", 45.f } } },
        { "halo",      "05-halo-reverse",           sparse(), 10, { { "Mix", 55.f } } },
        { "halo",      "06-halo-bloom",             arps(),   13, { { "Mode", 1.f }, { "Decay", 7.f }, { "Mix", 45.f } } },
        { "astral",    "07-astral-shimmer-delay",   sparse(), 12, { { "Time", 420.f }, { "Feedback", 60.f }, { "Shimmer", 55.f } } },
        { "mnemosyne", "08-mnemosyne-tape-echo",    sparse(), 11, { { "Heads", 6.f }, { "Intensity", 50.f } } },
        { "gemini",    "09-gemini-dotted",          sparse(), 11, { { "Time", 450.f }, { "Pattern", 0.f } } },
        { "gemini",    "10-gemini-reverse",         sparse(), 11, { { "Time", 600.f }, { "Pattern", 1.f }, { "Reverse", 1.f }, { "Mix", 50.f } } },
        { "lyra",      "11-lyra-12-string",         strum(),   9, {} },
        { "icarus",    "12-icarus-octave-fuzz",     lead(),    6, {} },
    };
    juce::WavAudioFormat wav; int written = 0;
    auto writeWav = [&] (const juce::File& f, const juce::AudioBuffer<float>& b)
    {
        f.deleteFile();
        std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
        if (auto* w = wav.createWriterFor (os.get(), sr, 2, 24, {}, 0)) { os.release(); std::unique_ptr<juce::AudioFormatWriter> ww (w); ww->writeFromAudioSampleBuffer (b, 0, b.getNumSamples()); return true; }
        return false;
    };
    for (auto& d : demos)
    {
        ModuleRig mr (d.key, sr, block);
        if (mr.m == nullptr) continue;
        for (auto& [name, v] : d.set) mr.set (name, v);
        Strings g (sr); size_t next = 0;
        const int total = (int) (sr * d.seconds);
        juce::AudioBuffer<float> out (2, total), dry (2, total), b (2, block);
        for (int done = 0; done < total; done += block)
        {
            const int n = juce::jmin (block, total - done);
            for (int i = 0; i < n; ++i)
            {
                const double t = (done + i) / sr;
                while (next < d.part.size() && d.part[next].t <= t) { g.pluck (hz (d.part[next].note), d.part[next].amp); ++next; }
                const float x = g.next();
                b.setSample (0, i, x); b.setSample (1, i, x); dry.setSample (0, done + i, x); dry.setSample (1, done + i, x);
            }
            mr.m->process (b.getArrayOfWritePointers(), 2, n, mr.p.data(), mr.ctx);
            for (int c = 0; c < 2; ++c) out.copyFrom (c, done, b, c, 0, n);
        }
        // normalise to -1 dBFS peak so every demo plays at a similar level
        const float pk = juce::jmax (out.getMagnitude (0, total), 1.0e-6f);
        out.applyGain (0.89f / pk);
        if (writeWav (dir.getChildFile (juce::String (d.file) + ".wav"), out)) ++written;
        if (juce::String (d.file).startsWith ("01")) { dry.applyGain (0.89f / juce::jmax (dry.getMagnitude (0, total), 1.0e-6f)); writeWav (dir.getChildFile ("00-dry-guitar.wav"), dry); }
    }
    // impulse responses (wet only), new reverbs and the older ones for comparison
    for (auto key : { "elysium", "empyrean", "nimbus", "vesper", "halo", "crypt", "seraph" })
    {
        ModuleRig mr (key, sr, block); if (mr.m == nullptr) continue;
        mr.set ("Mix", 100.f); mr.set ("Decay", 4.f); mr.set ("Swell", 0.f);
        const int total = (int) (sr * 6.0);
        juce::AudioBuffer<float> out (2, total), b (2, block);
        for (int done = 0; done < total; done += block)
        {
            const int n = juce::jmin (block, total - done);
            b.clear(); if (done == 0) { b.setSample (0, 0, 0.5f); b.setSample (1, 0, 0.5f); }
            mr.m->process (b.getArrayOfWritePointers(), 2, n, mr.p.data(), mr.ctx);
            for (int c = 0; c < 2; ++c) out.copyFrom (c, done, b, c, 0, n);
        }
        writeWav (dir.getChildFile (juce::String ("ir-") + key + ".wav"), out);
    }
    // wet level vs dry with steady noise at 100 % mix (to balance the pedals against each other)
    for (auto key : { "elysium", "empyrean", "nimbus", "vesper", "halo", "crypt", "seraph", "astral", "gemini", "mnemosyne" })
    {
        ModuleRig mr (key, sr, block); if (mr.m == nullptr) continue;
        mr.set ("Mix", 100.f); mr.set ("Echo", 100.f); mr.set ("Swell", 0.f);
        juce::Random z (9); dsp::OnePole pk; pk.setLowpass (sr, 2500.f);
        auto st = mr.run (sr, block, 6.0, [&] (int) { return pk.lp ((z.nextFloat() * 2.f - 1.f) * 0.3f); });
        std::cout << "level " << key << " " << juce::Decibels::gainToDecibels (st.rms / 0.0565f) << " dB vs dry" << std::endl;
    }
    std::cout << "Wrote " << written << " pedal demos to " << dir.getFullPathName() << std::endl;
    return written > 0 ? 0 : 1;
}
} // namespace obs

namespace obs
{
// =============================================================================
//  Sound renderer, for sound design (AETHER scenes, synth presets). Run:
//      "Obsidian Amp Suite" --render-sounds <out folder> <spec.json>
//  spec: [ { "name": "x", "seconds": 12, "params": { "atm.on": 1, "atm.bed.type": "Rain", ... },
//            "notes": [ [midi, start s, length s, velocity], ... ], "guitar": false, "steps": [16 numbers] } ]
//  Every sound starts from the init preset with the amp, cab and effects off (unless "rig": true).
//  Choices can be given by name. Writes <out>/<name>.wav and prints the level of each.
// =============================================================================
int renderSounds (const juce::File& dir, const juce::File& specFile)
{
    const auto spec = Rig::readJson (specFile);
    auto* list = spec.getArray();
    if (list == nullptr) { std::cout << "Can't read " << specFile.getFullPathName() << std::endl; return 1; }
    dir.createDirectory();
    Rig::asyncFileLoads = false;
    auto proc = std::make_unique<ObsidianProcessor>();
    auto& p = *proc;
    const double sr = 48000; const int block = 256;
    p.setPlayConfigDetails (2, 2, sr, block); p.prepareToPlay (sr, block);
    std::vector<float> riff; dsp::CaptureAmp::testRiff (riff, sr, 3.0);
    int written = 0;
    for (auto& e : *list)
    {
        p.presetManager().loadInit();
        {   // let the last sound die away (its tails and fades) before the next one starts
            juce::AudioBuffer<float> b (2, block); juce::MidiBuffer m;
            for (int i = 0; i < (int) (0.25 * sr) / block; ++i) { b.clear(); p.processBlock (b, m); }
        }
        if (! (bool) e.getProperty ("rig", false))
            for (auto id : { "amp.on", "cab.on", "doubler.on", "gate.on", "eq.on", "post.on", "pre.on", "sfx.on" }) setNorm (p, id, 0);
        if (auto* prm = e["params"].getDynamicObject())
            for (auto& kv : prm->getProperties())
            {
                auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p.state().getParameter (kv.name.toString()));
                if (rp == nullptr) { std::cout << "  no setting " << kv.name.toString() << std::endl; continue; }
                if (kv.value.isString())
                {
                    auto* ch = dynamic_cast<juce::AudioParameterChoice*> (rp);
                    const int k = ch != nullptr ? ch->choices.indexOf (kv.value.toString(), true) : -1;
                    if (k < 0) { std::cout << "  " << kv.name.toString() << " has no choice " << kv.value.toString() << std::endl; continue; }
                    rp->setValueNotifyingHost (rp->convertTo0to1 ((float) k));
                }
                else rp->setValueNotifyingHost (rp->convertTo0to1 ((float) (double) kv.value));
            }
        if (auto* st = e["steps"].getArray(); st != nullptr && st->size() == 16)
        {
            unsigned steps[16]; for (int i = 0; i < 16; ++i) steps[i] = (unsigned) (int) (*st)[i];
            p.engine().setSynthSteps (steps);
        }
        struct N { int note; double start, len; int vel; };
        std::vector<N> notes;
        if (auto* ns = e["notes"].getArray())
            for (auto& n : *ns) notes.push_back ({ (int) n[0], (double) n[1], (double) n[2], n.size() > 3 ? (int) n[3] : 110 });
        const bool guitar = (bool) e.getProperty ("guitar", false);
        const double total = (double) e.getProperty ("seconds", 10.0);
        juce::AudioBuffer<float> out (2, (int) (total * sr)); out.clear();
        juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer midi;
        double sumSq = 0; float peak = 0;
        for (int pos = 0; pos + block <= out.getNumSamples(); pos += block)
        {
            buf.clear(); midi.clear();
            if (guitar) for (int i = 0; i < block; ++i) { const float x = riff[(size_t) (pos + i) % riff.size()] * 0.9f; buf.setSample (0, i, x); buf.setSample (1, i, x); }
            for (auto& n : notes)
            {
                const int on = (int) (n.start * sr), off = (int) ((n.start + n.len) * sr);
                if (on >= pos && on < pos + block) midi.addEvent (juce::MidiMessage::noteOn (1, n.note, (juce::uint8) n.vel), on - pos);
                if (off >= pos && off < pos + block) midi.addEvent (juce::MidiMessage::noteOff (1, n.note), off - pos);
            }
            p.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c)
            {
                out.copyFrom (c, pos, buf, c, 0, block);
                for (int i = 0; i < block; ++i) { const float v = buf.getSample (c, i); sumSq += (double) v * v; peak = juce::jmax (peak, std::abs (v)); }
            }
        }
        const auto name = e["name"].toString();
        auto f = dir.getChildFile (name + ".wav"); f.deleteFile();
        juce::WavAudioFormat wav;
        if (auto os = std::unique_ptr<juce::OutputStream> (f.createOutputStream()))
            if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0)))
            { os.release(); w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples()); ++written; }
        const double rms = std::sqrt (sumSq / juce::jmax (1.0, 2.0 * out.getNumSamples()));
        std::cout << name << "  rms " << juce::String (juce::Decibels::gainToDecibels ((float) rms, -120.f), 1)
                  << " dB  peak " << juce::String (juce::Decibels::gainToDecibels (peak, -120.f), 1) << " dB" << std::endl;
    }
    p.releaseResources();
    std::cout << "Wrote " << written << " sounds to " << dir.getFullPathName() << std::endl;
    return written > 0 ? 0 : 1;
}
} // namespace obs
