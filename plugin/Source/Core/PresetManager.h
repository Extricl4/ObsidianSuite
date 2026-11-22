#pragma once
// =============================================================================
//  PresetManager: factory presets (built in) + your presets (.obsidian files in
//  Documents\Obsidian Amp Suite\Presets, or the folder chosen in Settings).
//
//  A preset file is readable JSON:
//    { "format": "obsidian-preset", "version": 1, "name": "...", "category": "...",
//      "params": { "in.gain": 0.0, "amp.gain": 6.5, ... },   // real values (dB, Hz...)
//      "rig": { ...amp/cab/pedal slots... } }
//  Unknown params are ignored and missing ones get defaults, so presets keep
//  working when parameters are added or ranges change.
//  Factory presets also carry "type", "note" and "fallbackAmp": an in-house amp used
//  when the preset's capture amp isn't installed (the amp pack is optional).
//  Your own folders are real sub-folders of the presets folder (one level shown);
//  factory and artist presets are built in and locked: they can't be deleted or renamed.
// =============================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include "Rig.h"
#include "AppSettings.h"

namespace obs
{
class PresetManager : private juce::AudioProcessorValueTreeState::Listener
{
public:
    /** category: the folder ("Rhythm", or "Artists/Metallica" for the artist presets);
        type: what kind of sound (Rhythm, Lead, Clean...); note: one line about it;
        dir: your own folder the preset sits in ("" = loose in My presets). */
    struct Info { juce::String id, name, category, type, note, dir; bool factory = false; juce::File file; int resource = -1; };
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
    bool saveAs (const juce::String& name, const juce::String& category = {}, const juce::String& dir = {});
    bool remove (const juce::String& id);
    bool rename (const juce::String& id, const juce::String& newName);
    bool importFile (const juce::File& f);

    // your own folders (sub-folders of folder())
    const juce::StringArray& userFolders() const { return dirs; }
    bool createFolder (const juce::String& name);
    bool renameFolder (const juce::String& name, const juce::String& newName);
    bool removeFolder (const juce::String& name);       // to the recycle bin, presets and all
    bool move (const juce::String& id, const juce::String& dir);   // dir "" = loose in My presets
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
    juce::var foldersJson() const;
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
    juce::File fileFor (const juce::String& name, const juce::String& dir = {}) const;
    juce::File dirFile (const juce::String& dir) const;     // an existing folder of yours, or {}
    void followCurrent (const juce::File& newFile);          // after the current preset's file moved

    juce::AudioProcessorValueTreeState& state;
    Rig& rig;
    AppSettings& settings;
    juce::Array<Info> presets;
    juce::StringArray dirs;
    Info current;
    bool dirty = false;
    std::atomic<bool> dirtyFlagFromAudio { false };
    std::atomic<int> suppress { 0 };
    juce::String error;
    juce::ListenerList<Listener> listeners;
};
} // namespace obs
