#pragma once
// =============================================================================
//  Modules = pedals, FX and special FX. Each module:
//   * declares its knobs (name, unit, range, default) in its ModuleType entry
//   * gets those knob values in real units every block
//   * processes 1 (pedal board, mono) or 2 (FX / special FX, stereo) channels
//
//  ADDING A NEW PEDAL OR EFFECT
//   1. Write a class deriving from Module (see any file in this folder).
//   2. Register it in ModuleRegistry.cpp with a unique key, e.g. "ember".
//      The key must never change: presets store it.
//   3. That's it. The UI lists it automatically in the right library
//      (by category), and its knobs appear on the card. If a pedal design
//      from visuals/pedals/<key>.json exists, the UI uses that art.
//
//  PACK PEDALS (a pedal that ships with its own captures or reverb IRs)
//   List the files in ModuleType::captures / ::irs, relative to
//   Documents\Obsidian Amp Suite\Captures and ...\IRs (e.g. "Pedals/Chiron/x.nam").
//   ModuleType::packSelectors names the knobs that pick the file: the file
//   index is built from them like digits (first selector = most significant).
//   The Rig loads the whole set in the background when the pedal is added and
//   hands it over with setCaptureBank / setIrBank. See PackModules.cpp.
// =============================================================================
#include "../DspCore.h"
#include "../NamEngine.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <functional>
#include <memory>

namespace obs::dsp { class PitchTracker; }

namespace obs::dsp
{
/** Everything a module may want to know about the host / the rest of the rig. */
struct ModuleContext
{
    double sampleRate = 48000;
    int maxBlock = 512;
    double bpm = 120;
    bool hostPlaying = false;
    double ppq = 0;
    int oversampling = 2;                   // 1, 2 or 4 (from Settings > Audio)
    const PitchTracker* pitch = nullptr;    // shared guitar pitch tracker (may be null)
};

/** A reverb impulse response read from disk (any channel count, any rate). */
struct IrFile
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 48000;
    juce::String name;
};

class Module
{
public:
    virtual ~Module() = default;
    virtual void prepare (const ModuleContext& ctx) = 0;
    virtual void reset() = 0;
    /** p = knob values in real units (same order as the ModuleType's params). */
    virtual void process (float* const* ch, int numCh, int n, const float* p, const ModuleContext& ctx) = 0;
    virtual int latency() const { return 0; }
    /** For modules that load a file (NAM pedal captures). Called on the message thread. */
    virtual juce::String loadFile (const juce::File&) { return "This module doesn't load files"; }
    virtual juce::String fileName() const { return {}; }
    /** Hand over an already-loaded capture (loaded on a background thread). */
    virtual void setNamEngine (std::unique_ptr<NamEngine> engine, const juce::String& name) { juce::ignoreUnused (engine, name); }

    // ---- pack pedals (message thread) ---------------------------------------------------
    /** All captures of a pack pedal, in ModuleType::captures order (missing files = nullptr). */
    virtual void setCaptureBank (std::vector<std::unique_ptr<NamEngine>> engines, juce::StringArray names) { juce::ignoreUnused (engines, names); }
    /** All IRs of a pack pedal, in ModuleType::irs order (missing files = empty buffer). */
    virtual void setIrBank (std::vector<IrFile> irs) { juce::ignoreUnused (irs); }
    /** Called on the message thread every few hundred ms (IR pedals rebuild their IR here). */
    virtual void messageTick() {}
    bool packRequested = false;          // set by the Rig once it started loading this pedal's files
};

struct ModuleParam
{
    juce::String name;
    juce::String unit;                   // "dB", "ms", "Hz", "%", "st", "x", ""
    float min = 0, max = 1, def = 0.5f;
    float skew = 1.f;                    // >1 = more resolution at the bottom
    juce::StringArray choices;           // non-empty = stepped selector
    bool momentary = false;              // a button you hold (freeze, stutter, tap)
    bool toggle = false;                 // on/off switch

    float fromNorm (float t) const
    {
        t = clampf (t, 0.f, 1.f);
        if (choices.size() > 0) return (float) juce::roundToInt (t * (float) (choices.size() - 1));
        if (toggle || momentary) return t >= 0.5f ? 1.f : 0.f;
        if (skew != 1.f) t = std::pow (t, skew);
        return min + (max - min) * t;
    }
    float toNorm (float v) const
    {
        if (choices.size() > 1) return clampf (v / (float) (choices.size() - 1), 0.f, 1.f);
        if (toggle || momentary) return v >= 0.5f ? 1.f : 0.f;
        float t = max > min ? (v - min) / (max - min) : 0.f;
        t = clampf (t, 0.f, 1.f);
        if (skew != 1.f) t = std::pow (t, 1.f / skew);
        return t;
    }
};

enum RackMask : unsigned { RackPre = 1, RackPost = 2, RackSfx = 4, RackAll = 7 };

struct ModuleType
{
    juce::String key;                    // stable id, stored in presets
    juce::String name;                   // shown in the UI
    juce::String category;               // "drive","boost","compressor","wah","pitch","delay","reverb","modulation","dynamics","freeze","glitch","looper","filter"
    unsigned racks = RackAll;            // where it may be placed
    juce::String description;
    std::vector<ModuleParam> params;     // max 8
    std::function<std::unique_ptr<Module>()> create;
    juce::String fileKind;               // "nam" for capture pedals, "pack" for pack pedals, empty otherwise
    std::vector<juce::String> captures;  // pack pedals: .nam files (relative to Documents\Obsidian Amp Suite\Captures)
    std::vector<juce::String> irs;       // pack pedals: .wav IRs   (relative to Documents\Obsidian Amp Suite\IRs)
    std::vector<int> packSelectors;      // param indices that pick the file (stepped choices or on/off switches)
    int packFirstChoice = 0;             // selector value that maps to file 0 (Crypt: 0 = built-in algorithm, 1.. = IRs)
};

/** Number of positions of a selector knob (choices) or switch (2). */
inline int selectorSize (const ModuleParam& p)  { return p.choices.size() > 0 ? p.choices.size() : 2; }
} // namespace obs::dsp
