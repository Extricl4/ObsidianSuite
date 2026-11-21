#include "Params.h"

namespace obs
{
const float kEqFreqs[kEqBands] = { 63.f, 125.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f, 16000.f };

const char* rackKey (int rack)
{
    switch (rack) { case 0: return "pre"; case 1: return "post"; default: return "sfx"; }
}
juce::String slotOnId (int rack, int slot)            { return juce::String (rackKey (rack)) + "." + juce::String (slot + 1) + ".on"; }
juce::String slotParamId (int rack, int slot, int p)  { return juce::String (rackKey (rack)) + "." + juce::String (slot + 1) + ".p" + juce::String (p + 1); }

namespace
{
struct Builder
{
    std::vector<ParamDef> v;
    juce::String group;

    ParamDef& f (const char* id, const char* name, float mn, float mx, float df, const char* unit = "", float skew = 1.0f)
    {
        ParamDef d; d.id = id; d.name = name; d.type = ParamType::Float; d.min = mn; d.max = mx; d.def = df; d.unit = unit; d.skew = skew; d.group = group;
        v.push_back (d); return v.back();
    }
    ParamDef& lg (const char* id, const char* name, float mn, float mx, float df, const char* unit = "Hz")
    {
        auto& d = f (id, name, mn, mx, df, unit); d.log = true; return d;
    }
    ParamDef& b (const char* id, const char* name, bool df)
    {
        ParamDef d; d.id = id; d.name = name; d.type = ParamType::Bool; d.min = 0; d.max = 1; d.def = df ? 1.f : 0.f; d.group = group;
        v.push_back (d); return v.back();
    }
    ParamDef& i (const char* id, const char* name, int mn, int mx, int df, const char* unit = "")
    {
        ParamDef d; d.id = id; d.name = name; d.type = ParamType::Int; d.min = (float) mn; d.max = (float) mx; d.def = (float) df; d.unit = unit; d.group = group;
        v.push_back (d); return v.back();
    }
    ParamDef& c (const char* id, const char* name, juce::StringArray choices, int df)
    {
        ParamDef d; d.id = id; d.name = name; d.type = ParamType::Choice; d.choices = choices; d.min = 0; d.max = (float) (choices.size() - 1); d.def = (float) df; d.group = group;
        v.push_back (d); return v.back();
    }
};

std::vector<ParamDef> build()
{
    Builder p;

    // ---- Header + footer --------------------------------------------------------------------
    p.group = "global";
    p.f  ("in.gain",      "Input",          -12.f, 12.f, 0.f, "dB");
    p.b  ("gate.on",      "Gate On",        true);
    p.f  ("gate.thresh",  "Gate Threshold", -80.f, 0.f, -51.5f, "dB", 1.0f / 0.858f);
    p.i  ("transpose",    "Transpose",      -12, 12, 0, "st");
    p.b  ("transpose.hq", "Transpose HQ",   false);
    p.b  ("doubler.on",   "Doubler On",     true);
    p.f  ("doubler.time", "Doubler Time",   0.f, 24.f, 6.f, "ms");
    p.f  ("width",        "Width",          0.f, 200.f, 100.f, "%");
    p.f  ("out.gain",     "Output",         -12.f, 12.f, 0.f, "dB");
    // Section bypasses (the tabs).
    p.b  ("pre.on",   "Pedals On",     true);
    p.b  ("synth.on", "Synth On",      false);   // off by default: presets turn it on
    p.b  ("amp.on",   "Amp On",        true);
    p.b  ("cab.on",   "Cab On",        true);
    p.b  ("eq.on",    "EQ On",         true);
    p.b  ("post.on",  "FX On",         true);
    p.b  ("sfx.on",   "Special FX On", true);

    // ---- Synth (guitar-tracking) -------------------------------------------------------------
    p.group = "synth";
    p.f  ("syn.gate",    "Synth Gate",        -90.f, -30.f, -66.f, "dB");
    p.f  ("syn.sens",    "Synth Sensitivity", 0.f, 100.f, 55.f, "%");
    p.f  ("syn.glide",   "Synth Glide",       0.f, 1000.f, 62.5f, "ms", 0.5f);
    p.f  ("syn.mix",     "Synth Mix",         0.f, 100.f, 60.f, "%");
    p.f  ("syn.out",     "Synth Output",      -24.f, 12.f, 1.2f, "dB");
    p.f  ("syn.decay",   "Synth Decay",       10.f, 5000.f, 1378.f, "ms", 1.0f / 2.17f);
    p.f  ("syn.sustain", "Synth Sustain",     0.f, 100.f, 90.f, "%");
    const juce::StringArray waves { "Sine", "Triangle", "Saw", "Square", "Pulse", "Noise" };
    for (int o = 1; o <= 2; ++o)
    {
        const auto n = juce::String (o);
        auto id = [&] (const char* s) { return ("syn.o" + n + "." + s); };
        auto nm = [&] (const char* s) { return ("Osc " + n + " " + s); };
        auto add = [&] (ParamDef d) { p.v.push_back (d); };
        ParamDef d;
        d = {}; d.id = id ("on");    d.name = nm ("On");        d.type = ParamType::Bool;   d.min = 0; d.max = 1; d.def = 1; d.group = "synth"; add (d);
        d = {}; d.id = id ("wave");  d.name = nm ("Wave");      d.type = ParamType::Choice; d.choices = waves; d.min = 0; d.max = 5; d.def = (o == 1 ? 2.f : 3.f); d.group = "synth"; add (d);
        d = {}; d.id = id ("semi");  d.name = nm ("Semitones"); d.type = ParamType::Int;    d.min = -24; d.max = 24; d.def = 0; d.unit = "st"; d.group = "synth"; add (d);
        d = {}; d.id = id ("det");   d.name = nm ("Detune");    d.type = ParamType::Float;  d.min = -100; d.max = 100; d.def = 0; d.unit = "%"; d.group = "synth"; add (d);
        d = {}; d.id = id ("level"); d.name = nm ("Level");     d.type = ParamType::Float;  d.min = 0; d.max = 1; d.def = (o == 1 ? 0.649f : 0.62f); d.group = "synth"; add (d);
        d = {}; d.id = ("syn.u" + n + ".width"); d.name = "Unison " + n + " Width";  d.type = ParamType::Float; d.min = 0; d.max = 100; d.def = (o == 1 ? 100.f : 60.f); d.unit = "%"; d.group = "synth"; add (d);
        d = {}; d.id = ("syn.u" + n + ".det");   d.name = "Unison " + n + " Detune"; d.type = ParamType::Float; d.min = 0; d.max = 100; d.def = (o == 1 ? 10.f : 18.f); d.unit = "%"; d.group = "synth"; add (d);
    }
    p.b  ("syn.flt.on",     "Filter On",        true);
    p.lg ("syn.flt.cutoff", "Filter Cutoff",    20.f, 20000.f, 4480.f);
    p.f  ("syn.flt.res",    "Filter Resonance", 0.f, 100.f, 0.f, "%");
    p.f  ("syn.flt.attack", "Filter Attack",    1.f, 2001.f, 1.25f, "ms", 1.0f / 3.0f);
    p.f  ("syn.flt.drive",  "Filter Drive",     0.f, 100.f, 0.f, "%");
    p.c  ("syn.flt.slope",  "Filter Slope",     { "6 dB", "12 dB", "18 dB", "24 dB" }, 1);
    p.f  ("syn.flt.env",    "Filter Env Amount", -100.f, 100.f, 0.f, "%");
    p.b  ("syn.retrig",     "Env Retrigger",    true);
    p.b  ("syn.post",       "Synth Post-Amp",   true);
    p.b  ("syn.arp.on",     "Arp On",           true);
    const char* stepNames[] = { "Root", "2", "3", "4", "5", "6", "7", "8" };
    for (int s = 0; s < 8; ++s)
    {
        ParamDef d; d.id = "syn.arp.s" + juce::String (s + 1); d.name = juce::String ("Arp Step ") + stepNames[s];
        d.type = ParamType::Bool; d.min = 0; d.max = 1; d.def = (s == 0 ? 1.f : 0.f); d.group = "synth"; p.v.push_back (d);
    }
    p.c  ("syn.arp.key",     "Arp Key",     { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0);
    p.c  ("syn.arp.scale",   "Arp Scale",   { "Major", "Minor" }, 0);
    p.c  ("syn.arp.oct",     "Arp Octave",  { "-2", "-1", "0", "+1", "+2" }, 2);
    p.c  ("syn.arp.pattern", "Arp Pattern", { "Up", "Down", "Up and down", "Down and up", "Random" }, 0);
    p.f  ("syn.arp.rate",    "Arp Rate",    0.f, 1.f, 0.5654f);   // free: 0.1 * 200^x Hz, synced: note value index
    p.b  ("syn.arp.sync",    "Arp Sync",    false);
    p.f  ("syn.tune",        "Synth Tuning", 415.f, 466.f, 440.f, "Hz");

    // ---- Amp ---------------------------------------------------------------------------------
    p.group = "amp";
    p.f  ("amp.gain",     "Amp Gain",     0.f, 10.f, 5.f);
    p.f  ("amp.bass",     "Amp Bass",     0.f, 10.f, 5.f);
    p.f  ("amp.mid",      "Amp Mid",      0.f, 10.f, 5.f);
    p.f  ("amp.treble",   "Amp Treble",   0.f, 10.f, 5.f);
    p.f  ("amp.presence", "Amp Presence", 0.f, 10.f, 5.f);
    p.f  ("amp.depth",    "Amp Depth",    0.f, 10.f, 5.f);
    p.f  ("amp.master",   "Amp Master",   0.f, 10.f, 5.f);
    p.i  ("amp.channel",  "Amp Channel",  0, 3, 0);

    // ---- Cab (two mics) ----------------------------------------------------------------------
    p.group = "cab";
    const juce::StringArray mics { "Dynamic 57", "Ribbon 121", "Condenser 414", "Dynamic 421", "Dynamic 906", "Flat" };
    for (int m = 0; m < 2; ++m)
    {
        const juce::String k = m == 0 ? "a" : "b", N = m == 0 ? "Mic A " : "Mic B ";
        auto add = [&] (ParamDef d) { d.group = "cab"; p.v.push_back (d); };
        ParamDef d;
        d = {}; d.id = "cab." + k + ".mic";   d.name = N + "Model";    d.type = ParamType::Choice; d.choices = mics; d.min = 0; d.max = (float) mics.size() - 1; d.def = (m == 0 ? 0.f : 1.f); add (d);
        d = {}; d.id = "cab." + k + ".pos";   d.name = N + "Position"; d.min = 0; d.max = 1; d.def = (m == 0 ? 0.38f : 0.62f); add (d);
        d = {}; d.id = "cab." + k + ".dist";  d.name = N + "Distance"; d.min = 0; d.max = 30; d.def = (m == 0 ? 2.f : 6.f); d.unit = "cm"; d.skew = 0.5f; add (d);
        d = {}; d.id = "cab." + k + ".level"; d.name = N + "Level";    d.min = -36; d.max = 6; d.def = (m == 0 ? 0.f : -4.5f); d.unit = "dB"; add (d);
        d = {}; d.id = "cab." + k + ".pan";   d.name = N + "Pan";      d.min = -100; d.max = 100; d.def = 0; add (d);
        d = {}; d.id = "cab." + k + ".solo";  d.name = N + "Solo";     d.type = ParamType::Bool; d.min = 0; d.max = 1; d.def = 0; add (d);
        d = {}; d.id = "cab." + k + ".mute";  d.name = N + "Mute";     d.type = ParamType::Bool; d.min = 0; d.max = 1; d.def = 0; add (d);
        d = {}; d.id = "cab." + k + ".phase"; d.name = N + "Phase";    d.type = ParamType::Bool; d.min = 0; d.max = 1; d.def = 0; add (d);
    }
    p.lg ("cab.lc", "Cab Low Cut",  20.f, 500.f, 80.f);
    p.lg ("cab.hc", "Cab High Cut", 2000.f, 20000.f, 9500.f);

    // ---- EQ (9-band graphic + cuts) ----------------------------------------------------------
    p.group = "eq";
    const char* bandNames[] = { "63 Hz", "125 Hz", "250 Hz", "500 Hz", "1 kHz", "2 kHz", "4 kHz", "8 kHz", "16 kHz" };
    for (int b = 0; b < kEqBands; ++b)
    {
        ParamDef d; d.id = "eq.b" + juce::String (b + 1); d.name = juce::String ("EQ ") + bandNames[b];
        d.min = -12; d.max = 12; d.def = 0; d.unit = "dB"; d.group = "eq"; p.v.push_back (d);
    }
    p.lg ("eq.lc", "EQ Low Cut",  20.f, 20000.f, 20.f);       // 20 Hz = off
    p.lg ("eq.hc", "EQ High Cut", 20.f, 20000.f, 20000.f);    // 20 kHz = off

    // ---- Racks: pedals, FX, special FX -------------------------------------------------------
    p.group = "rack";
    const char* rackNames[] = { "Pedal", "FX", "Special FX" };
    for (int r = 0; r < kNumRacks; ++r)
        for (int s = 0; s < kSlotsPerRack; ++s)
        {
            const auto base = juce::String (rackNames[r]) + " " + juce::String (s + 1);
            ParamDef on; on.id = slotOnId (r, s); on.name = base + " On"; on.type = ParamType::Bool; on.min = 0; on.max = 1; on.def = 1; on.group = "rack";
            p.v.push_back (on);
            for (int k = 0; k < kSlotParams; ++k)
            {
                ParamDef d; d.id = slotParamId (r, s, k); d.name = base + " Knob " + juce::String (k + 1);
                d.min = 0; d.max = 1; d.def = 0.5f; d.group = "rack"; p.v.push_back (d);
            }
        }
    return p.v;
}
} // namespace

const std::vector<ParamDef>& paramDefs()
{
    static const std::vector<ParamDef> defs = build();
    return defs;
}

static juce::NormalisableRange<float> rangeFor (const ParamDef& d)
{
    if (d.log)
    {
        const float mn = d.min, mx = d.max;
        return juce::NormalisableRange<float> (mn, mx,
            [] (float a, float b, float t) { return a * std::pow (b / a, t); },
            [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
            [] (float a, float b, float v) { return juce::jlimit (a, b, v); });
    }
    juce::NormalisableRange<float> r (d.min, d.max);
    r.skew = d.skew;
    return r;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    int version = 1;
    for (auto& d : paramDefs())
    {
        const juce::ParameterID pid (d.id, version);
        switch (d.type)
        {
            case ParamType::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, d.name, d.def > 0.5f));
                break;
            case ParamType::Choice:
                layout.add (std::make_unique<juce::AudioParameterChoice> (pid, d.name, d.choices, (int) d.def));
                break;
            case ParamType::Int:
                layout.add (std::make_unique<juce::AudioParameterInt> (pid, d.name, (int) d.min, (int) d.max, (int) d.def,
                    juce::AudioParameterIntAttributes().withLabel (d.unit)));
                break;
            case ParamType::Float:
            default:
            {
                auto unit = d.unit;
                auto attrs = juce::AudioParameterFloatAttributes().withLabel (unit)
                    .withStringFromValueFunction ([unit] (float v, int)
                    {
                        if (unit == "Hz") return v >= 1000.f ? juce::String (v / 1000.f, v >= 10000.f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
                        if (unit == "dB") return (v > 0.04f ? "+" : "") + juce::String (v, 1) + " dB";
                        if (unit == "ms") return v >= 1000.f ? juce::String (v / 1000.f, 2) + " s" : juce::String (v, v < 10.f ? 1 : 0) + " ms";
                        if (unit == "%")  return juce::String (juce::roundToInt (v)) + "%";
                        if (unit == "cm") return juce::String (v, 1) + " cm";
                        return juce::String (v, 2);
                    });
                layout.add (std::make_unique<juce::AudioParameterFloat> (pid, d.name, rangeFor (d), d.def, attrs));
                break;
            }
        }
    }
    return layout;
}

void ParamCache::attach (juce::AudioProcessorValueTreeState& s)
{
    map.clear();
    for (auto& d : paramDefs())
        if (auto* raw = s.getRawParameterValue (d.id))
            map[d.id] = raw;
}

std::atomic<float>* ParamCache::ptr (const juce::String& id) const
{
    auto it = map.find (id);
    jassert (it != map.end());   // typo in a param id?
    if (it == map.end())
    {
        static std::atomic<float> dummy { 0.0f };
        return &dummy;
    }
    return it->second;
}

float ParamCache::get (const juce::String& id) const { return ptr (id)->load (std::memory_order_relaxed); }
} // namespace obs
