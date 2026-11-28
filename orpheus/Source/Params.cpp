#include "Params.h"

namespace orph
{
static const ParamDef kDefs[] = {
#include "ParamDefs.inc"
};
static_assert (sizeof (kDefs) / sizeof (kDefs[0]) == P_COUNT, "ParamDefs.inc and ParamIds.inc disagree: run tools/gen_params.py");

const ParamDef* paramDefs() { return kDefs; }

int paramIndex (const juce::String& id)
{
    static const auto map = []
    {
        std::unordered_map<std::string, int> m;
        for (int i = 0; i < P_COUNT; ++i) m[kDefs[i].id] = i;
        return m;
    }();
    auto it = map.find (id.toStdString());
    return it == map.end() ? -1 : it->second;
}

/* A readable name for hosts: "syn.fx.rev.size" -> "Reverb Size" */
static juce::String niceName (const juce::String& id)
{
    static const std::map<juce::String, juce::String> part {
        { "a", "Osc A" }, { "b", "Osc B" }, { "sub", "Sub" }, { "noise", "Noise" }, { "sn", "Sub/Noise" }, { "flt", "Filter" },
        { "env", "Env" }, { "fenv", "Filter Env" }, { "lfo", "LFO" }, { "fx", "FX" }, { "drv", "Drive" }, { "cru", "Crush" },
        { "ring", "Ring" }, { "oct", "Octave" }, { "cho", "Chorus" }, { "fla", "Flanger" }, { "pha", "Phaser" }, { "dly", "Delay" },
        { "rev", "Reverb" }, { "shm", "Shimmer" }, { "dbl", "Doubler" }, { "char", "Character" }, { "on", "On" }, { "amt", "Amount" },
        { "fb", "Feedback" }, { "res", "Resonance" }, { "att", "Attack" }, { "dec", "Decay" }, { "sus", "Sustain" }, { "rel", "Release" },
        { "dyn", "Dynamics" }, { "pre", "Pre-delay" }, { "damp", "Dark" }, { "div", "Note" }, { "f", "Filter" } };
    juce::StringArray bits; bits.addTokens (id.fromFirstOccurrenceOf ("syn.", false, false), ".", "");
    juce::StringArray out;
    for (auto& b : bits)
    {
        auto it = part.find (b);
        out.add (it != part.end() ? it->second : b.substring (0, 1).toUpperCase() + b.substring (1));
    }
    return out.joinIntoString (" ");
}

juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < P_COUNT; ++i)
    {
        const auto& d = kDefs[i];
        const juce::ParameterID pid { d.id, 1 };
        const auto name = niceName (d.id);
        switch (d.type)
        {
            case PT_BOOL:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, name, d.def > 0.5));
                break;
            case PT_CHOICE:
            {
                juce::StringArray c; c.addTokens (d.choices, "|", "");
                layout.add (std::make_unique<juce::AudioParameterChoice> (pid, name, c, (int) d.def));
                break;
            }
            case PT_INT:
                layout.add (std::make_unique<juce::AudioParameterInt> (pid, name, (int) d.lo, (int) d.hi, (int) d.def));
                break;
            case PT_LOG:
            {
                // log: the UI's knob is linear in log(frequency)
                const double lo = d.lo, hi = d.hi;
                juce::NormalisableRange<float> r ((float) lo, (float) hi,
                    [lo, hi] (float, float, float n) { return (float) (lo * std::pow (hi / lo, (double) n)); },
                    [lo, hi] (float, float, float v) { return (float) (std::log ((double) v / lo) / std::log (hi / lo)); },
                    [] (float s, float e, float v) { return juce::jlimit (s, e, v); });
                layout.add (std::make_unique<juce::AudioParameterFloat> (pid, name, r, (float) d.def));
                break;
            }
            default:
            {
                // f: the UI's knob maps n -> lo + (hi - lo) * n ^ (1 / skew)  (skew < 1: finer at the low end)
                const double lo = d.lo, hi = d.hi, sk = d.skew > 0 ? d.skew : 1.0;
                juce::NormalisableRange<float> r ((float) lo, (float) hi,
                    [lo, hi, sk] (float, float, float n) { return (float) (lo + (hi - lo) * std::pow ((double) n, 1.0 / sk)); },
                    [lo, hi, sk] (float, float, float v) { return (float) std::pow (juce::jlimit (0.0, 1.0, ((double) v - lo) / (hi - lo)), sk); },
                    [] (float s, float e, float v) { return juce::jlimit (s, e, v); });
                layout.add (std::make_unique<juce::AudioParameterFloat> (pid, name, r, (float) d.def));
                break;
            }
        }
    }
    return layout;
}
} // namespace orph
