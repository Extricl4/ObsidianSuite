#pragma once
// =============================================================================
//  Params: the ONE list of every automatable control in Obsidian.
//
//  To add a knob:     add one line in Params.cpp (id, name, range, default),
//                     read it in the DSP with  params.get("my.id"),
//                     and bind it in WebUI/obsidian-bridge.js (BINDINGS table).
//  Ranges are chosen to match the UI's own knob maths exactly, so a knob at
//  12 o'clock in the UI is the same value the host sees.
//
//  IDs are stable strings. Never rename one (old presets and DAW projects use it);
//  add a new one instead. Presets store real values (dB, Hz, ...), not 0..1, so
//  widening a range later does not break saved sounds.
// =============================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace obs
{
enum class ParamType { Float, Bool, Choice, Int };

struct ParamDef
{
    juce::String id;
    juce::String name;
    ParamType type = ParamType::Float;
    float min = 0.0f, max = 1.0f, def = 0.0f;
    float skew = 1.0f;              // NormalisableRange skew (value = min + range * p^(1/skew))
    bool log = false;               // logarithmic (frequency-style) mapping
    juce::String unit;              // shown by the host: "dB", "ms", "Hz", "%", "st"
    juce::StringArray choices;      // for Choice
    juce::String group;             // "global", "synth", "eq", "cab", "amp", "rack"
    bool automatable = true;
};

const std::vector<ParamDef>& paramDefs();
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Rack layout: three racks of slots, each slot = on/off + 8 generic module knobs.
constexpr int kNumRacks = 3;         // 0 = pedals (pre-amp), 1 = FX (post), 2 = special FX
constexpr int kSlotsPerRack = 5;
constexpr int kSlotParams = 8;
const char* rackKey (int rack);      // "pre", "post", "sfx"
juce::String slotOnId (int rack, int slot);
juce::String slotParamId (int rack, int slot, int p);

constexpr int kEqBands = 9;
extern const float kEqFreqs[kEqBands];

/** Fast, lock-free access to the raw (real-unit) parameter values from the audio thread. */
class ParamCache
{
public:
    void attach (juce::AudioProcessorValueTreeState&);
    float get (const juce::String& id) const;                 // slow path (map lookup) for setup code
    std::atomic<float>* ptr (const juce::String& id) const;   // cache this pointer, read it per block
private:
    std::unordered_map<juce::String, std::atomic<float>*> map;
};
} // namespace obs
