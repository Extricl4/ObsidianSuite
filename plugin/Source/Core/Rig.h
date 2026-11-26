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
//    built-in : Obsidian Core amp, Eidolon (the NAM loader amp), three generated cabs
//    yours    : JSON files in  Documents\Obsidian Amp Suite\Amps  and  ...\Cabs
//               (see Content/README.md for the format)
//
//  Pack pedals (Chiron, Aeon, Ossuary, Crypt's RV modes ...) bring their own
//  files from  Documents\Obsidian Amp Suite\Captures\Pedals\<Pedal>  and
//  ...\IRs\Pedals\<Pedal>; the Rig loads them when the pedal is added.
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
    /** The rig as a copy (presets, DAW state). Safe from any thread: off the message thread (some hosts save
        projects from a background thread) it's the copy the message thread keeps up to date. */
    juce::ValueTree snapshot() const;
    /** Message thread: refresh the copy other threads get (on every change, and from a timer). */
    void refreshSnapshot();
    void resetToDefault();
    void applyAll();                                // push the whole rig into the engine (after prepare / restore)

    // ---- amp -------------------------------------------------------------------------
    void setAmp (const juce::String& libraryKey);
    void loadCapture (int channel, const juce::File& file);
    void clearCapture (int channel);
    /** Capture amps (the amp pack): a button on the amp's face, a face knob that isn't a plugin
        parameter (0..1), a switch. Buttons the amp keeps per channel are stored per channel. */
    void setAmpDim (int dim, int value);
    void setAmpKnob (int knob, float value);
    void setAmpSwitch (int index, bool on);
    /** The amp file of a capture amp (Documents\Obsidian Amp Suite\Amps\<key>.json), or void. */
    static juce::var readAmpFile (const juce::String& key);
    static juce::var readFaceFile (const juce::String& key);
    /** A .nam file's own description ({title, gear, kind, by}), read once per file and kept. */
    static juce::var namInfo (const juce::String& path);
    static juce::var readJson (const juce::File& f);          // JSON file, tolerant of "" keys
    /** Loads every capture of a capture amp (call off the audio thread; the Rig's loader and the self-test use it). */
    struct CaptureAmpFiles { std::unique_ptr<dsp::CaptureAmp> amp; juce::StringArray missing, broken; };
    static CaptureAmpFiles loadCaptureAmp (std::shared_ptr<const dsp::CaptureAmpMap> map, const juce::String& key, double rate, int block);
    // ---- cab -------------------------------------------------------------------------
    void setCab (const juce::String& libraryKey);
    /** A cab whose file lists "voices" (album tones, mic positions, speakers): switch to voice n (its irA / irB). */
    void setCabVoice (int voice);
    void loadIr (int mic, const juce::File& file);   // mic: 0 = A, 1 = B, 2 = both
    /** Mic A/B dragged on the CAB page: onto speaker spk (index in the cab file's "speakers"), at angle ang round it.
        pos is the mic's POSITION (0 = dust cap .. 1 = edge): a cab recorded per speaker and position switches the mic
        to the real IR nearest to that spot. */
    void setCabMic (int mic, int spk, double ang, float pos);
    // ---- racks -----------------------------------------------------------------------
    void setModule (int rack, int slot, const juce::String& key, bool applyDefaults = true);
    void moveModule (int rack, int from, int to);
    void loadModuleFile (int rack, int slot, const juce::File& file);
    // ---- misc ------------------------------------------------------------------------
    void setMisc (const juce::String& key, const juce::var& value);
    /** Arpeggiator steps: an array (or comma list) of 16 packed numbers. Stored with the rig. */
    void setSynthSteps (const juce::var& steps);
    void applySynthSteps();
    juce::var getMisc (const juce::String& key, const juce::var& def = {}) const;

    juce::var toJson() const;                       // for the UI
    /** A pack pedal's files, loaded (call off the audio thread; used by the Rig's loader and the self-test). */
    struct PackFiles
    {
        std::vector<std::unique_ptr<dsp::NamEngine>> engines; juce::StringArray names;
        std::vector<dsp::IrFile> irs;
        juce::StringArray missing, broken;
        int total = 0; juce::File folder;
    };
    static std::shared_ptr<PackFiles> loadPackFiles (const dsp::ModuleType& type, double rate, int block);
    static void installPack (dsp::Module& m, PackFiles& files);

    static juce::var ampLibrary();
    /** Every cab. A cab whose IRs come from a maker's free pack the plugin may not ship ("pack": {id, name, url})
        has "packReady": false until that pack is installed into IRs/Packs/<id>. */
    static juce::var cabLibrary();
    /** Installs an IR pack zip the user downloaded from its maker: finds the folder holding the files the cabs ask
        for (the 48 kHz one when the pack comes in several sample rates) and keeps its IRs as IRs/Packs/<id>.
        Returns what happened; an error starts with "!". irsRoot: where IRs/ is (the self-test uses a temp folder). */
    static juce::String installIrPack (const juce::File& zip, juce::File irsRoot = {});
    static juce::var findInLibrary (const juce::var& lib, const juce::String& key);
    /** True when a capture amp's amp file and its first capture are installed (the amp pack is optional). */
    static bool captureAmpInstalled (const juce::String& key);
    /** Tools (the factory preset writer) load files right away instead of on the loader thread. */
    static inline bool asyncFileLoads = true;
    void loadFilesNow();
    static juce::File contentFolder();              // Documents\Obsidian Amp Suite

    void addListener (Listener* l)                  { listeners.add (l); }
    void removeListener (Listener* l)               { listeners.remove (l); }

    bool isBusy() const                             { return pending.load() > 0; }

private:
    void timerCallback() override;
    void notifyChanged();
    void message (const juce::String& t, bool err);
    void applyAmp();
    void applyCab();
    void applyCabMic (int mic, const juce::var& libraryEntry);
    void placeCabMics (const juce::var& libraryEntry, bool wasRealIrCab);
    juce::var cabMics() const;
    void applySlot (int rack, int slot);
    void startCaptureLoad (int channel, const juce::File& f, bool notifyOk);
    void startCaptureAmpLoad (const juce::String& key);
    void pushFaceState();
    void captureAmpChannelTick();
    void startModuleFileLoad (int rack, int slot, const juce::File& f);
    void startPackLoad (int rack, int slot);
    juce::ValueTree slotTree (int rack, int slot);

    Engine& engine;
    juce::AudioProcessorValueTreeState& state;
    juce::ValueTree rig { "RIG" };
    juce::ThreadPool loader { juce::ThreadPoolOptions{}.withThreadName ("Obsidian loader").withNumberOfThreads (1) };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    std::atomic<int> pending { 0 };
    int generation[4] {};
    std::shared_ptr<const dsp::CaptureAmpMap> ampMap;    // the capture amp in the AMP slot (message thread)
    juce::String ampMapKey, loadingAmpKey;
    int lastAmpChannel = -1;
    juce::String shownFile[kNumRacks][kSlotsPerRack];   // what each pack pedal showed last (UI refresh when it changes)
    juce::ListenerList<Listener> listeners;
    mutable juce::CriticalSection snapLock;
    juce::ValueTree snapCache;
    int snapTicks = 0;
};
} // namespace obs
