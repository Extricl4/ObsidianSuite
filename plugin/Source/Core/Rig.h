#pragma once
// =============================================================================
//  Rig: everything about a sound that isn't a knob: which amp and capture
//  files, which cab / IRs, which pedal sits in which slot, tempo, EQ preset
//  name. Stored in the plugin state (DAW projects) and in presets.
//
//  All file loading (captures, IRs) happens on a background thread; results are
//  installed lock-free, so loading never interrupts the audio.
//
//  Libraries (what ADD AMP / ADD CAB list):
//    built-in : Obsidian Core amp, NAM Loader amp, three generated cabs
//    yours    : JSON files in  Documents\Obsidian Suite\Amps  and  ...\Cabs
//               (see Content/README.md for the format)
// =============================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include "../DSP/Engine.h"

namespace obs
{
class Rig : private juce::Timer
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void rigChanged() {}
        virtual void rigMessage (const juce::String& text, bool isError) { juce::ignoreUnused (text, isError); }
    };

    Rig (Engine& e, juce::AudioProcessorValueTreeState& s);
    ~Rig() override;

    juce::ValueTree& tree()                         { return rig; }
    void restore (const juce::ValueTree& saved);    // from DAW state / preset
    juce::ValueTree snapshot() const                { return rig.createCopy(); }
    void resetToDefault();
    void applyAll();                                // push the whole rig into the engine (after prepare / restore)

    // ---- amp -------------------------------------------------------------------------
    void setAmp (const juce::String& libraryKey);
    void loadCapture (int channel, const juce::File& file);
    void clearCapture (int channel);
    // ---- cab -------------------------------------------------------------------------
    void setCab (const juce::String& libraryKey);
    void loadIr (int mic, const juce::File& file);   // mic: 0 = A, 1 = B, 2 = both
    // ---- racks -----------------------------------------------------------------------
    void setModule (int rack, int slot, const juce::String& key, bool applyDefaults = true);
    void moveModule (int rack, int from, int to);
    void loadModuleFile (int rack, int slot, const juce::File& file);
    // ---- misc ------------------------------------------------------------------------
    void setMisc (const juce::String& key, const juce::var& value);
    juce::var getMisc (const juce::String& key, const juce::var& def = {}) const;

    juce::var toJson() const;                       // for the UI
    static juce::var ampLibrary();
    static juce::var cabLibrary();
    static juce::File contentFolder();              // Documents\Obsidian Suite

    void addListener (Listener* l)                  { listeners.add (l); }
    void removeListener (Listener* l)               { listeners.remove (l); }

    bool isBusy() const                             { return pending.load() > 0; }

private:
    void timerCallback() override;
    void notifyChanged();
    void message (const juce::String& t, bool err);
    void applyAmp();
    void applyCab();
    void applySlot (int rack, int slot);
    void startCaptureLoad (int channel, const juce::File& f, bool notifyOk);
    void startModuleFileLoad (int rack, int slot, const juce::File& f);
    juce::ValueTree slotTree (int rack, int slot);
    static juce::var findInLibrary (const juce::var& lib, const juce::String& key);

    Engine& engine;
    juce::AudioProcessorValueTreeState& state;
    juce::ValueTree rig { "RIG" };
    juce::ThreadPool loader { juce::ThreadPoolOptions{}.withThreadName ("Obsidian loader").withNumberOfThreads (1) };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    std::atomic<int> pending { 0 };
    int generation[4] {};
    juce::ListenerList<Listener> listeners;
};
} // namespace obs
