// =============================================================================
//  Pitch effects: whammy, octave, harmonizer, sub-octave, laser.
// =============================================================================
#include "Module.h"
#include "../PitchShifter.h"
#include "../PitchTracker.h"

namespace obs::dsp
{
namespace
{
// ---------------------------------------------------------------------------
class Whammy : public Module   // "Ascend": expression-style pitch bend
{
public:
    void prepare (const ModuleContext& c) override { for (auto& s : sh) s.prepare (c.sampleRate, 24.f); posSm.reset (c.sampleRate, 25.f, 0.f); }
    void reset() override { for (auto& s : sh) s.reset(); }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: shift st, position %, mix %
        posSm.set (p[1] * 0.01f);
        const float mix = p[2] * 0.01f;
        for (int i = 0; i < n; ++i)
        {
            const float st = p[0] * posSm.next();
            if ((i & 15) == 0) for (auto& s : sh) s.setSemitones (st);
            for (int c = 0; c < numCh; ++c) { const float d = ch[c][i]; ch[c][i] = d * (1.f - mix) + sh[c].process (d) * mix; }
        }
    }
private:
    PitchShifter sh[2]; Smoother posSm;
};

// ---------------------------------------------------------------------------
class Octave : public Module   // "Abyss": octave down + rectifier octave up
{
public:
    void prepare (const ModuleContext& c) override
    {
        sr = c.sampleRate;
        for (int k = 0; k < 2; ++k) { down[k].prepare (sr, 30.f); down[k].setSemitones (-12.f); down2[k].prepare (sr, 40.f); down2[k].setSemitones (-24.f); upHp[k].set (Biquad::HP, sr, 200.f, 0.7f); dc[k].prepare (sr); }
        reset();
    }
    void reset() override { for (int k = 0; k < 2; ++k) { down[k].reset(); down2[k].reset(); down[k].setSemitones (-12.f); down2[k].setSemitones (-24.f); tone[k].reset(); upHp[k].reset(); dc[k].reset(); } }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: dry, -1 oct, -2 oct, +1 oct, tone
        if (p[4] != toneCache) { toneCache = p[4]; for (auto& t : tone) t.set (Biquad::LP, sr, 300.f * std::pow (20.f, p[4] / 10.f), 0.7f); }
        const float dry = p[0] * 0.01f, o1 = p[1] * 0.01f, o2 = p[2] * 0.01f, up = p[3] * 0.01f;
        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float x = ch[c][i];
                const float d1 = down[c].process (x), d2 = down2[c].process (x);
                const float u = dc[c].process (upHp[c].process (std::abs (x) * 1.6f));
                ch[c][i] = x * dry + tone[c].process (d1 * o1 + d2 * o2) * 1.2f + u * up;
            }
    }
private:
    double sr = 48000; PitchShifter down[2], down2[2]; Biquad tone[2], upHp[2]; DcBlocker dc[2]; float toneCache = -1;
};

// ---------------------------------------------------------------------------
class Harmonizer : public Module   // "Legion": in-key harmony using the guitar's detected note
{
public:
    void prepare (const ModuleContext& c) override { for (auto& s : sh) s.prepare (c.sampleRate, 26.f); for (auto& s : sh2) s.prepare (c.sampleRate, 26.f); reset(); }
    void reset() override { for (auto& s : sh) s.reset(); for (auto& s : sh2) s.reset(); }
    static int scaleShift (int note, int key, int scale, int degrees)
    {
        static const int maj[7] = { 0, 2, 4, 5, 7, 9, 11 }, mnr[7] = { 0, 2, 3, 5, 7, 8, 10 };
        const int* sc = scale == 0 ? maj : mnr;
        const int rel = ((note - key) % 12 + 12) % 12;
        int deg = 0, best = 99;
        for (int d = 0; d < 7; ++d) { const int df = std::abs (sc[d] - rel); if (df < best) { best = df; deg = d; } }
        const int target = deg + degrees;
        const int oct = (int) std::floor (target / 7.0);
        const int td = target - oct * 7;
        return (sc[td] + 12 * oct) - sc[deg];
    }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext& ctx) override
    {
        // p: voice 1 interval, voice 2 interval, key, scale, mix, voice 2 level
        static const int intervals[] = { -7, -4, -3, -2, 0, 2, 3, 4, 5, 7 };   // scale degrees: oct down .. oct up
        int note = lastNote;
        if (ctx.pitch != nullptr && ctx.pitch->confidence() > 0.7f && ctx.pitch->frequency() > 0)
            note = lastNote = (int) std::round (PitchTracker::toMidi (ctx.pitch->frequency()));
        const int key = (int) p[2], scale = (int) p[3];
        const int s1 = note > 0 ? scaleShift (note, key, scale, intervals[juce::jlimit (0, 9, (int) p[0])]) : 0;
        const int s2 = note > 0 ? scaleShift (note, key, scale, intervals[juce::jlimit (0, 9, (int) p[1])]) : 0;
        for (int c = 0; c < numCh; ++c) { sh[c].setSemitones ((float) s1); sh2[c].setSemitones ((float) s2); }
        const float mix = p[4] * 0.01f, v2 = p[5] * 0.01f;
        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float x = ch[c][i];
                ch[c][i] = x + (sh[c].process (x) + sh2[c].process (x) * v2) * mix;
            }
    }
private:
    PitchShifter sh[2], sh2[2]; int lastNote = -1;
};

// ---------------------------------------------------------------------------
class SubOctave : public Module   // "Richter": earthquake sub with low-pass
{
public:
    void prepare (const ModuleContext& c) override
    {
        sr = c.sampleRate;
        for (int k = 0; k < 2; ++k) { s1[k].prepare (sr, 40.f); s1[k].setSemitones (-12.f); s2[k].prepare (sr, 50.f); s2[k].setSemitones (-24.f); }
        reset();
    }
    void reset() override { for (int k = 0; k < 2; ++k) { s1[k].reset(); s2[k].reset(); s1[k].setSemitones (-12.f); s2[k].setSemitones (-24.f); lp[k].reset(); lp2[k].reset(); } }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: dry, sub 1, sub 2, low pass Hz, drive
        if (p[3] != lpCache) { lpCache = p[3]; for (int k = 0; k < 2; ++k) { lp[k].set (Biquad::LP, sr, p[3], 0.7f); lp2[k].set (Biquad::LP, sr, p[3], 0.7f); } }
        const float dry = p[0] * 0.01f, a = p[1] * 0.01f, b = p[2] * 0.01f, drive = 1.f + p[4] * 0.4f;
        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float x = ch[c][i];
                const float sub = lp2[c].process (lp[c].process (s1[c].process (x) * a + s2[c].process (x) * b));
                ch[c][i] = x * dry + fastTanh (sub * drive) * 1.3f;
            }
    }
private:
    double sr = 48000; PitchShifter s1[2], s2[2]; Biquad lp[2], lp2[2]; float lpCache = -1;
};

// ---------------------------------------------------------------------------
class Laser : public Module   // "Photon": every pick attack fires a falling resonant sweep ("pew")
{
public:
    void prepare (const ModuleContext& c) override
    {
        sr = c.sampleRate;
        for (int k = 0; k < 2; ++k) { line[k].prepare ((int) (sr * 0.03)); }
        fast.set (sr, 0.5f, 20.f); slow.set (sr, 15.f, 200.f);
        reset();
    }
    void reset() override { for (auto& l : line) l.clear(); fast.reset(); slow.reset(); sweep = 1.f; }
    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        // p: start Hz, end Hz, sweep time ms, feedback %, mix %, sensitivity
        const float f0 = p[0], f1 = p[1], timeS = juce::jmax (0.01f, p[2] * 0.001f);
        const float fb = p[3] * 0.01f * 0.97f, mix = p[4] * 0.01f, sens = 1.2f + (10.f - p[5]) * 0.15f;
        const float stepPerSample = 1.f / (timeS * (float) sr);
        for (int i = 0; i < n; ++i)
        {
            float a = 0; for (int c = 0; c < numCh; ++c) a = std::max (a, std::abs (ch[c][i]));
            const float ef = fast.process (a), es = slow.process (a);
            if (hold > 0) --hold; else if (ef > es * sens && ef > 0.01f) { sweep = 0.f; hold = (int) (0.05 * sr); }
            sweep = juce::jmin (1.f, sweep + stepPerSample);
            const float f = f0 * std::pow (f1 / f0, sweep);
            const float d = clampf ((float) sr / f, 2.f, (float) (sr * 0.029));
            for (int c = 0; c < numCh; ++c)
            {
                const float x = ch[c][i];
                const float y = line[c].readLinear (d);
                line[c].push (fastTanh (x + y * fb));
                ch[c][i] = x * (1.f - mix * 0.5f) + y * mix;
            }
        }
    }
private:
    double sr = 48000; DelayLine line[2]; EnvFollower fast, slow; float sweep = 1.f; int hold = 0;
};
} // namespace

void registerPitchModules (std::vector<ModuleType>& t)
{
    using P = ModuleParam;
    const juce::StringArray keys { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const juce::StringArray harm { "Oct down", "5th down", "4th down", "3rd down", "Unison", "3rd up", "4th up", "5th up", "6th up", "Oct up" };
    t.push_back ({ "ascend", "Ascend Whammy", "pitch", RackAll,
        "Bends your guitar up or down. Sweep Position like an expression pedal.",
        { P { "Shift", "st", -24, 24, 12 }, P { "Position", "%", 0, 100, 100 }, P { "Mix", "%", 0, 100, 100 } },
        [] { return std::make_unique<Whammy>(); } });
    t.push_back ({ "abyss", "Abyss Octave", "pitch", RackAll,
        "Adds an octave below, two octaves below and an octave above.",
        { P { "Dry", "%", 0, 100, 100 }, P { "-1 Oct", "%", 0, 100, 70 }, P { "-2 Oct", "%", 0, 100, 0 }, P { "+1 Oct", "%", 0, 100, 0 }, P { "Tone", "", 0, 10, 6 } },
        [] { return std::make_unique<Octave>(); } });
    t.push_back ({ "legion", "Legion Harmonizer", "pitch", RackAll,
        "Two harmony voices that stay in key (it listens to the note you play).",
        { P { "Voice 1", "", 0, 9, 5, 1, harm }, P { "Voice 2", "", 0, 9, 4, 1, harm }, P { "Key", "", 0, 11, 4, 1, keys }, P { "Scale", "", 0, 1, 1, 1, { "Major", "Minor" } },
          P { "Mix", "%", 0, 100, 70 }, P { "Voice 2 Level", "%", 0, 100, 0 } },
        [] { return std::make_unique<Harmonizer>(); } });
    t.push_back ({ "richter", "Richter Sub", "pitch", RackAll,
        "Earthquake sub-octave for huge low end.",
        { P { "Dry", "%", 0, 100, 100 }, P { "Sub 1", "%", 0, 100, 80 }, P { "Sub 2", "%", 0, 100, 30 }, P { "Low Pass", "Hz", 60, 800, 220, 2.f }, P { "Drive", "", 0, 10, 2 } },
        [] { return std::make_unique<SubOctave>(); } });
    t.push_back ({ "photon", "Photon Laser", "filter", RackPost | RackSfx,
        "Every pick attack fires a falling resonant sweep: sci-fi lasers.",
        { P { "Start", "Hz", 200, 4000, 2400, 2.f }, P { "End", "Hz", 40, 1000, 90, 2.f }, P { "Time", "ms", 20, 1500, 280, 2.f },
          P { "Feedback", "%", 0, 100, 85 }, P { "Mix", "%", 0, 100, 60 }, P { "Sensitivity", "", 0, 10, 6 } },
        [] { return std::make_unique<Laser>(); } });
}
} // namespace obs::dsp
