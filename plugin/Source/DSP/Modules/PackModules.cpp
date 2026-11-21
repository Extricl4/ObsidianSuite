// =============================================================================
//  Pack pedals: pedals that come with their own captures or reverb IRs.
//  The files live in  Documents\Obsidian Amp Suite\Captures\Pedals\<Pedal>\  and
//  ...\IRs\Pedals\<Pedal>\  (the "Obsidian pedal pack"). The Rig loads a pedal's
//  whole set when it's added; the pedal's switches pick which file plays.
//
//  Capture pedals (NAM): Chiron, Emperor, Graven, Maw, XXXIII, Helios, Aeon.
//  IR reverbs:           Ossuary, Halcyon   (Crypt's RV modes live in TimeModules.cpp).
//  Looks: visuals/pedals/<key>.json; file map: visuals/registry.json.
// =============================================================================
#include "Module.h"
#include "IrCore.h"

namespace obs::dsp
{
namespace
{
/** Which knobs do what on a pack pedal (-1 = the pedal has no such knob). */
struct PackSpec
{
    int input = -1, output = -1, mix = -1;                  // capture pedals
    int decay = -1, tone = -1, lowCut = -1, preDelay = -1;  // IR pedals
    std::vector<int> selectors;                              // param indices, most significant first
};

/** File index from the selector knobs (stepped choices / switches), like digits. */
int packIndex (const PackSpec& s, const std::vector<ModuleParam>& params, const float* p, int firstChoice = 0)
{
    int idx = 0;
    for (int k : s.selectors)
    {
        const int size = selectorSize (params[(size_t) k]);
        const int v = juce::jlimit (0, size - 1, (int) std::lround (p[k]));
        idx = idx * size + v;
    }
    return idx - firstChoice;
}

// ---------------------------------------------------------------------------
class CapturePedal : public Module
{
public:
    CapturePedal (PackSpec s, std::vector<ModuleParam> p) : spec (std::move (s)), params (std::move (p)) {}

    void prepare (const ModuleContext& c) override
    {
        sr = c.sampleRate; block = c.maxBlock;
        if (auto* b = bank.get()) for (auto& e : b->engines) if (e) e->prepare (sr, block);
        inSm.reset (sr, 30.f, 1.f); outSm.reset (sr, 30.f, 1.f); mixSm.reset (sr, 30.f, 1.f);
        for (auto& m : mono) m.assign ((size_t) c.maxBlock, 0.f);
        fadeLen = juce::jmax (1, (int) (0.06 * sr));
    }
    void reset() override { fadeLeft = 0; }

    void setCaptureBank (std::vector<std::unique_ptr<NamEngine>> engines, juce::StringArray names) override
    {
        auto b = std::make_unique<Bank>();
        for (auto& e : engines) { if (e) e->prepare (sr, block); b->engines.push_back (std::move (e)); }
        b->names = names;
        bank.set (std::move (b));
        bank.collectGarbage (false);
    }
    juce::String fileName() const override
    {
        auto* b = bank.get();
        if (b == nullptr) return {};
        int loadedCount = 0; for (auto& e : b->engines) if (e) ++loadedCount;
        if (loadedCount == 0) return "PACK MISSING";
        const int i = shown.load (std::memory_order_relaxed);
        return juce::isPositiveAndBelow (i, b->names.size()) ? b->names[i] : juce::String();
    }

    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        auto* b = bank.get();
        if (b == nullptr || b->engines.empty() || n > (int) mono[0].size()) { bank.blockDone(); return; }   // not loaded yet: dry

        const int count = (int) b->engines.size();
        const int idx = juce::jlimit (0, count - 1, packIndex (spec, params, p));
        if (cur < 0 || cur >= count) { cur = idx; fadeLeft = 0; }
        else if (idx != cur)        { prev = cur; cur = idx; fadeLeft = fadeLen; }   // a new pick restarts the cross-fade
        shown.store (cur, std::memory_order_relaxed);

        inSm.set (spec.input >= 0 ? dbToGain (p[spec.input]) : 1.f);
        outSm.set (spec.output >= 0 ? dbToGain (p[spec.output]) : 1.f);
        mixSm.set (spec.mix >= 0 ? p[spec.mix] * 0.01f : 1.f);

        const bool fading = fadeLeft > 0;
        for (int i = 0; i < n; ++i) mono[0][(size_t) i] = ch[0][i] * inSm.next();
        if (fading) std::copy (mono[0].begin(), mono[0].begin() + n, mono[1].begin());

        auto run = [&] (int e, std::vector<float>& buf)
        {
            if (auto* eng = b->engines[(size_t) e].get()) eng->process (buf.data(), n);
            // a missing file plays the input through unchanged
        };
        run (cur, mono[0]);
        if (fading) run (prev, mono[1]);

        for (int i = 0; i < n; ++i)
        {
            float wet = mono[0][(size_t) i];
            if (fadeLeft > 0)
            {
                const float t = 1.f - (float) fadeLeft / (float) fadeLen;           // 0 -> 1
                const float a = std::sin (t * 1.5707963f), bb = std::cos (t * 1.5707963f);
                wet = wet * a + mono[1][(size_t) i] * bb;
                --fadeLeft;
            }
            const float o = wet * outSm.next(), m = mixSm.next();
            for (int c = 0; c < numCh; ++c) ch[c][i] = ch[c][i] * (1.f - m) + o * m;
        }
        bank.blockDone();
    }
    ~CapturePedal() override { bank.collectGarbage (true); }

private:
    struct Bank { std::vector<std::unique_ptr<NamEngine>> engines; juce::StringArray names; };
    PackSpec spec;
    std::vector<ModuleParam> params;
    RtSlot<Bank> bank;
    double sr = 48000; int block = 512;
    Smoother inSm, outSm, mixSm;
    std::vector<float> mono[2];
    int cur = -1, prev = 0, fadeLeft = 0, fadeLen = 2880;
    std::atomic<int> shown { 0 };
};

// ---------------------------------------------------------------------------
class IrPedal : public Module
{
public:
    IrPedal (PackSpec s, std::vector<ModuleParam> p) : spec (std::move (s)), params (std::move (p)) {}
    void prepare (const ModuleContext& c) override
    {
        core.prepare (c.sampleRate, c.maxBlock);
        mixSm.reset (c.sampleRate, 30.f, 0.3f);
        for (auto& w : wet) w.assign ((size_t) c.maxBlock, 0.f);
        for (auto& w : dryCopy) w.assign ((size_t) c.maxBlock, 0.f);
    }
    void reset() override { core.reset(); }
    void setIrBank (std::vector<IrFile> irs) override { core.setBank (std::move (irs)); core.messageTick(); }
    void messageTick() override { core.messageTick(); }
    juce::String fileName() const override { return core.bankSize() > 0 ? core.currentName() : juce::String(); }

    void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext&) override
    {
        n = juce::jmin (n, (int) wet[0].size());
        core.setParams (packIndex (spec, params, p), p[spec.decay], spec.preDelay >= 0 ? p[spec.preDelay] : 0.f,
                        p[spec.tone], spec.lowCut >= 0 ? p[spec.lowCut] : 20.f);
        mixSm.set (core.ready() ? p[spec.mix] * 0.01f : 0.f);
        const float* inR = numCh > 1 ? ch[1] : ch[0];
        std::copy (ch[0], ch[0] + n, dryCopy[0].begin());
        std::copy (inR, inR + n, dryCopy[1].begin());
        core.processWet (dryCopy[0].data(), dryCopy[1].data(), wet[0].data(), wet[1].data(), n);
        for (int i = 0; i < n; ++i)
        {
            const float m = mixSm.next();
            const float dryG = m < 0.5f ? 1.f : 2.f * (1.f - m);       // same mix law as the other reverbs
            if (numCh > 1)
            {
                ch[0][i] = dryCopy[0][(size_t) i] * dryG + wet[0][(size_t) i] * m;
                ch[1][i] = dryCopy[1][(size_t) i] * dryG + wet[1][(size_t) i] * m;
            }
            else ch[0][i] = dryCopy[0][(size_t) i] * dryG + 0.5f * (wet[0][(size_t) i] + wet[1][(size_t) i]) * m;
        }
    }
private:
    PackSpec spec;
    std::vector<ModuleParam> params;
    IrCore core;
    Smoother mixSm;
    std::vector<float> wet[2], dryCopy[2];
};

// ---------------------------------------------------------------------------
using P = ModuleParam;
P choice (const char* name, juce::StringArray c, float def = 0) { P p { name, "", 0, (float) juce::jmax (1, c.size() - 1), def }; p.choices = c; return p; }
P onOff (const char* name, float def = 0) { P p { name, "", 0, 1, def }; p.toggle = true; return p; }
P inGain (const char* name) { return P { name, "dB", -24, 24, 0 }; }
P outGain (const char* name) { return P { name, "dB", -24, 24, 0 }; }
P blend (const char* name, float def = 100) { return P { name, "%", 0, 100, def }; }

juce::StringArray files (const juce::String& folder, std::initializer_list<const char*> names)
{
    juce::StringArray a; for (auto* n : names) a.add ("Pedals/" + folder + "/" + n); return a;
}

void addCapturePedal (std::vector<ModuleType>& t, const char* key, const char* name, const char* category, const char* desc,
                      std::vector<ModuleParam> params, PackSpec spec, juce::StringArray caps)
{
    ModuleType m;
    m.key = key; m.name = name; m.category = category; m.racks = RackPre | RackPost; m.description = desc;
    m.params = std::move (params); m.fileKind = "pack"; m.packSelectors = spec.selectors;
    for (auto& c : caps) m.captures.push_back (c);
    m.create = [spec, params = m.params] { return std::make_unique<CapturePedal> (spec, params); };
    t.push_back (std::move (m));
}

void addIrPedal (std::vector<ModuleType>& t, const char* key, const char* name, const char* desc,
                 std::vector<ModuleParam> params, PackSpec spec, juce::StringArray irFiles)
{
    ModuleType m;
    m.key = key; m.name = name; m.category = "reverb"; m.racks = RackPost | RackSfx; m.description = desc;
    m.params = std::move (params); m.fileKind = "pack"; m.packSelectors = spec.selectors;
    for (auto& f : irFiles) m.irs.push_back (f);
    m.create = [spec, params = m.params] { return std::make_unique<IrPedal> (spec, params); };
    t.push_back (std::move (m));
}
} // namespace

void registerPackModules (std::vector<ModuleType>& t)
{
    // ---- capture pedals -----------------------------------------------------------------
    {   PackSpec s; s.input = 0; s.mix = 1; s.output = 2; s.selectors = { 3 };
        addCapturePedal (t, "chiron", "Chiron", "drive", "Transparent overdrive (Klon-style Centaur captures). Blend mixes your clean signal back in.",
            { inGain ("Drive"), blend ("Blend"), outGain ("Output"), choice ("Capture", { "MID", "HIGH" }) }, s,
            files ("Chiron", { "Centaur OD (mid gain).nam", "Centaur OD (high gain).nam" })); }
    {   PackSpec s; s.input = 0; s.output = 1; s.mix = 3; s.selectors = { 2 };
        addCapturePedal (t, "emperor", "Emperor", "drive", "Three-mode drive (Duke of Tone captures): boost, overdrive or distortion.",
            { inGain ("Drive"), outGain ("Volume"), choice ("Mode", { "BST", "OD", "DIST" }, 1), blend ("Mix") }, s,
            files ("Emperor", { "KOT BST Gain=5.nam", "KOT OD Gain=5.nam", "KOT DIST Gain=5.nam" })); }
    {   PackSpec s; s.input = 0; s.output = 1; s.mix = 3; s.selectors = { 2 };
        addCapturePedal (t, "graven", "Graven", "drive", "Tight metal overdrive (Fortin-modded TS-9 captures). Put it in front of a high-gain amp.",
            { inGain ("Drive"), outGain ("Level"), choice ("Capture", { "I", "II", "III" }), blend ("Mix") }, s,
            files ("Graven", { "Fortin_TS9_1.nam", "Fortin_TS9_2.nam", "Fortin_TS9_3.nam" })); }
    {   PackSpec s; s.input = 0; s.output = 2; s.mix = 3; s.selectors = { 1 };
        addCapturePedal (t, "maw", "Maw", "drive", "High-gain grinder (Fortin Grind captures).",
            { inGain ("Gain"), choice ("Capture", { "SLAM", "KODY" }), outGain ("Level"), blend ("Mix") }, s,
            files ("Maw", { "SLAMMIN_FORTIN_GRIND.nam", "SLAMMIN_FORTIN_GRIND_KODY.nam" })); }
    {   PackSpec s; s.input = 0; s.output = 1; s.mix = 2;
        addCapturePedal (t, "xxxiii", "XXXIII", "boost", "Fortin 33 boost (Thordendal signature capture). Tightens and pushes the amp.",
            { inGain ("Drive"), outGain ("Level"), blend ("Mix") }, s,
            files ("XXXIII", { "Fortin 33 - 1_30.nam" })); }
    {   PackSpec s; s.output = 0; s.input = 2; s.mix = 3; s.selectors = { 1 };
        addCapturePedal (t, "helios", "Helios", "drive", "Super-overdrive (Boss SD-1 captures): three boost and six drive voices.",
            { outGain ("Level"), choice ("Voice", { "B I", "B II", "B III", "D I", "D II", "D III", "D IV", "D V", "D VI" }, 4), inGain ("Drive"), blend ("Mix") }, s,
            files ("Helios", { "APP-SD1-Boost-I.nam", "APP-SD1-Boost-II.nam", "APP-SD1-Boost-III.nam",
                               "APP-SD1-Drive-I.nam", "APP-SD1-Drive-II.nam", "APP-SD1-Drive-III.nam",
                               "APP-SD1-Drive-IV.nam", "APP-SD1-Drive-V.nam", "APP-SD1-Drive-VI.nam" })); }
    {   // Aeon: Voice (alpha/omega) x Bite x Growl x Drive step  = 2 x 2 x 2 x 7 = 56 captures
        PackSpec s; s.mix = 0; s.output = 1; s.input = 6; s.selectors = { 3, 5, 4, 2 };
        juce::StringArray caps;
        const char* steps[] = { "G0", "G1", "G3", "G5", "G7", "G8", "G10" };
        for (int voice = 0; voice < 2; ++voice)
            for (int bite = 0; bite < 2; ++bite)
                for (int growl = 0; growl < 2; ++growl)
                {
                    const char* mode = bite ? (growl ? "Bite-Growl" : "BITE") : (growl ? "Growl" : "Neutral");
                    for (auto* g : steps)
                        caps.add (voice == 0 ? juce::String ("Pedals/Aeon/Alpha-") + mode + " " + g + ".nam"
                                             : juce::String ("Pedals/Aeon/Omega - Omega-") + mode + " " + g + ".nam");
                }
        addCapturePedal (t, "aeon", "Aeon", "drive", "Bass/guitar drive (Darkglass Alpha Omega Ultra captures): alpha or omega voice, Growl and Bite, seven drive steps.",
            { blend ("Blend"), outGain ("Level"), choice ("Drive", { "0", "1", "3", "5", "7", "8", "10" }, 3), choice ("Voice", { "ALPHA", "OMEGA" }),
              onOff ("Growl"), onOff ("Bite"), inGain ("Input") }, s, caps); }

    // ---- IR reverbs ---------------------------------------------------------------------
    {   PackSpec s; s.mix = 0; s.decay = 1; s.tone = 2; s.preDelay = 5; s.lowCut = 6; s.selectors = { 3, 4 };
        juce::StringArray irs;
        for (auto* pos : { "Close", "Middle", "Far" })
            for (auto* mic : { "Neumann MA1 _ Mono", "Lauten LA120 _ Stereo", "Zoom XYH-6 _ Stereo" })
                irs.add (juce::String ("Pedals/Ossuary/Greenwood Catacombs _ ") + pos + " + " + mic + ".wav");
        addIrPedal (t, "ossuary", "Ossuary", "Real catacomb reverb (Green-Wood Cemetery Catacombs IRs). Pick the room position and the mic.",
            { blend ("Mix", 35), P { "Decay", "s", 0.2f, 4.f, 4.f, 1.f }, P { "Tone", "Hz", 1000, 16000, 9000, 2.f },
              choice ("Room", { "CLOSE", "MID", "FAR" }, 1), choice ("Mic", { "I", "II", "III" }, 1),
              P { "Pre-delay", "ms", 0, 250, 0, 2.f }, P { "Low Cut", "Hz", 20, 600, 80, 2.f } }, s, irs); }
    {   PackSpec s; s.mix = 0; s.decay = 1; s.tone = 2; s.preDelay = 4; s.lowCut = 5; s.selectors = { 3 };
        addIrPedal (t, "halcyon", "Halcyon", "Ambient shoegaze reverb (Tone Charm Audio IRs): a slap-back echo or a long percussive wash.",
            { blend ("Mix", 40), P { "Decay", "s", 0.2f, 20.f, 20.f, 2.f }, P { "Tone", "Hz", 1000, 16000, 8000, 2.f },
              choice ("Mode", { "SLAP", "VERB" }, 1), P { "Pre-delay", "ms", 0, 250, 0, 2.f }, P { "Low Cut", "Hz", 20, 600, 120, 2.f } }, s,
            files ("Halcyon", { "Delay And Reverb Ambient Shoegaze Machine slap back.wav", "Delay And Reverb Ambient Shoegaze Machine percussive verb 1.wav" })); }
}
} // namespace obs::dsp
