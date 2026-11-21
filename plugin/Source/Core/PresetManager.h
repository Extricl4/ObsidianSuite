#pragma once
// =============================================================================
//  PresetManager: factory presets (built in) + your presets (.obsidian files in
//  Documents\Obsidian Suite\Presets, or the folder chosen in Settings).
//
//  A preset file is readable JSON:
//    { "format": "obsidian-preset", "version": 1, "name": "...", "category": "...",
//      "params": { "in.gain": 0.0, "amp.gain": 6.5, ... },   // real values (dB, Hz...)
//      "rig": { ...amp/cab/pedal slots... } }
//  Unknown params are ignored and missing ones get defaults, so presets keep
//  working when parameters are added or ranges change.
// =============================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include "Rig.h"
#include "AppSettings.h"

namespace obs
{
class PresetManager : private juce::AudioProcessorValueTreeState::Listener
{
public:
    struct Info { juce::String id, name, category; bool factory = false; juce::File file; };
    struct Listener { virtual ~Listener() = default; virtual void presetStateChanged() = 0; virtual void presetListChanged() = 0; };

    PresetManager (juce::AudioProcessorValueTreeState& s, Rig& r, AppSettings& settings);
    ~PresetManager() override;

    juce::File folder() const;
    void setFolder (const juce::File& f);
    void rescan();
    const juce::Array<Info>& list() const       { return presets; }

    bool load (const juce::String& id);
    bool loadNext (int direction);
    bool loadInit();
    bool save();                                       // overwrite current (factory presets: becomes Save As)
    bool saveAs (const juce::String& name, const juce::String& category = {});
    bool remove (const juce::String& id);
    bool rename (const juce::String& id, const juce::String& newName);
    bool importFile (const juce::File& f);
    bool exportTo (const juce::String& id, const juce::File& dest);

    juce::String currentName() const            { return current.name; }
    juce::String currentId() const              { return current.id; }
    bool isFactory() const                      { return current.factory; }
    bool isDirty() const                        { return dirty; }
    void markDirty();
    void pollDirty();                           // call from a message-thread timer
    void setCurrentFromState (const juce::String& id, const juce::String& name, bool isDirtyFlag);

    juce::var toJson() const;
    juce::var listJson() const;
    juce::String lastError() const              { return error; }

    void addListener (Listener* l)              { listeners.add (l); }
    void removeListener (Listener* l)           { listeners.remove (l); }

    /** Serialises the current sound (params + rig) as a preset object. */
    juce::var captureCurrent (const juce::String& name, const juce::String& category) const;
    bool applyPresetVar (const juce::var& preset);

private:
    void parameterChanged (const juce::String&, float) override;
    void notifyState();
    void notifyList();
    juce::File fileFor (const juce::String& name) const;

    juce::AudioProcessorValueTreeState& state;
    Rig& rig;
    AppSettings& settings;
    juce::Array<Info> presets;
    Info current;
    bool dirty = false;
    std::atomic<bool> dirtyFlagFromAudio { false };
    std::atomic<int> suppress { 0 };
    juce::String error;
    juce::ListenerList<Listener> listeners;
};
} // namespace obs
