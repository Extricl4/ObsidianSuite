#include "UiBridge.h"

#if JucePlugin_Build_Standalone
 #include <juce_audio_utils/juce_audio_utils.h>
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

namespace obs
{
namespace
{
juce::String noteName (float midi)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int n = (int) std::round (midi);
    return juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
}
juce::var obj (std::initializer_list<std::pair<const char*, juce::var>> kv)
{
    auto o = new juce::DynamicObject();
    for (auto& p : kv) o->setProperty (p.first, p.second);
    return juce::var (o);
}
juce::String argStr (const juce::Array<juce::var>& a, int i)  { return i < a.size() ? a[i].toString() : juce::String(); }
int   argInt (const juce::Array<juce::var>& a, int i, int d = 0)   { return i < a.size() ? (int) a[i] : d; }
float argF (const juce::Array<juce::var>& a, int i, float d = 0)   { return i < a.size() ? (float) (double) a[i] : d; }
} // namespace

UiBridge::UiBridge (ObsidianProcessor& p, Emit e, std::function<void (float)> scale)
    : proc (p), emit (std::move (e)), applyScale (std::move (scale))
{
    for (auto& d : paramDefs()) { indexOf[d.id] = (int) ids.size(); ids.push_back (d.id); }
    dirtyParams.reset (new std::atomic<bool>[ids.size()]);
    for (size_t i = 0; i < ids.size(); ++i) dirtyParams[i].store (false);
    for (auto& id : ids) proc.state().addParameterListener (id, this);
    proc.rig().addListener (this);
    proc.presetManager().addListener (this);
   #if JucePlugin_Build_Standalone
    if (auto* h = juce::StandalonePluginHolder::getInstance())
    {
        h->getMuteInputValue().setValue (false);      // an amp sim must never mute its input
        deviceManagerBroadcaster = &h->deviceManager;
        h->deviceManager.addChangeListener (this);
    }
   #endif
    buildCommands();
    startTimerHz (30);
}

UiBridge::~UiBridge()
{
    stopTimer();
    if (deviceManagerBroadcaster != nullptr) deviceManagerBroadcaster->removeChangeListener (this);
    for (auto& id : ids) proc.state().removeParameterListener (id, this);
    proc.rig().removeListener (this);
    proc.presetManager().removeListener (this);
}

// --------------------------------------------------------------------------- JSON helpers
juce::var UiBridge::paramsJson()
{
    juce::Array<juce::var> a;
    for (auto& d : paramDefs())
    {
        auto* prm = dynamic_cast<juce::RangedAudioParameter*> (proc.state().getParameter (d.id));
        if (prm == nullptr) continue;
        auto o = new juce::DynamicObject();
        o->setProperty ("id", d.id); o->setProperty ("name", d.name); o->setProperty ("group", d.group);
        o->setProperty ("type", d.type == ParamType::Bool ? "bool" : d.type == ParamType::Choice ? "choice" : d.type == ParamType::Int ? "int" : "float");
        o->setProperty ("min", d.min); o->setProperty ("max", d.max); o->setProperty ("def", d.def); o->setProperty ("skew", d.skew);
        o->setProperty ("log", d.log); o->setProperty ("unit", d.unit);
        if (d.choices.size() > 0) { juce::Array<juce::var> c; for (auto& s : d.choices) c.add (s); o->setProperty ("choices", c); }
        o->setProperty ("norm", prm->getValue());
        o->setProperty ("value", prm->convertFrom0to1 (prm->getValue()));
        o->setProperty ("text", prm->getCurrentValueAsText());
        a.add (juce::var (o));
    }
    return a;
}

juce::var UiBridge::modulesJson()
{
    juce::Array<juce::var> a;
    for (auto& m : dsp::moduleTypes())
    {
        auto o = new juce::DynamicObject();
        o->setProperty ("key", m.key); o->setProperty ("name", m.name); o->setProperty ("category", m.category);
        o->setProperty ("description", m.description); o->setProperty ("fileKind", m.fileKind);
        juce::Array<juce::var> racks;
        if (m.racks & dsp::RackPre) racks.add ("pre");
        if (m.racks & dsp::RackPost) racks.add ("post");
        if (m.racks & dsp::RackSfx) racks.add ("sfx");
        o->setProperty ("racks", racks);
        juce::Array<juce::var> ps;
        for (auto& p : m.params)
        {
            auto po = new juce::DynamicObject();
            po->setProperty ("name", p.name); po->setProperty ("unit", p.unit); po->setProperty ("min", p.min); po->setProperty ("max", p.max);
            po->setProperty ("def", p.def); po->setProperty ("defNorm", p.toNorm (p.def)); po->setProperty ("skew", p.skew);
            po->setProperty ("momentary", p.momentary); po->setProperty ("toggle", p.toggle);
            if (p.choices.size() > 0) { juce::Array<juce::var> c; for (auto& s : p.choices) c.add (s); po->setProperty ("choices", c); }
            ps.add (juce::var (po));
        }
        o->setProperty ("params", ps);
        a.add (juce::var (o));
    }
    return a;
}

juce::var UiBridge::initialState()
{
    auto o = new juce::DynamicObject();
    o->setProperty ("version", OBSIDIAN_VERSION_STRING);
    o->setProperty ("build", Updater::ownBuild());
    o->setProperty ("update", updater->toJson());
    o->setProperty ("standalone", proc.isStandalone());
    o->setProperty ("platform", juce::SystemStats::getOperatingSystemName());
    o->setProperty ("cpu", juce::SystemStats::getCpuModel());
    o->setProperty ("params", paramsJson());
    o->setProperty ("modules", modulesJson());
    o->setProperty ("amps", Rig::ampLibrary());
    o->setProperty ("cabs", Rig::cabLibrary());
    o->setProperty ("rig", proc.rig().toJson());
    o->setProperty ("preset", proc.presetManager().toJson());
    o->setProperty ("presets", proc.presetManager().listJson());
    o->setProperty ("presetFolders", proc.presetManager().foldersJson());
    o->setProperty ("settings", juce::var (proc.settings().toObject().get()));
    o->setProperty ("midi", proc.midi().toJson());
    o->setProperty ("audio", audioSetup());
    o->setProperty ("contentFolder", Rig::contentFolder().getFullPathName());
    return juce::var (o);
}

void UiBridge::uiReady()
{
    ready = true;
    updaterSeen = updater->changes();
    // look for a newer build when the window opens (at most every 6 hours; Settings > About can turn it off)
    auto& st = proc.settings();
    const double now = juce::Time::currentTimeMillis() / 1000.0;
    if ((bool) st.get ("checkUpdates") && now - (double) st.get ("lastUpdateCheck") > 6 * 3600.0 && updater->state() != Updater::State::downloading)
    {
        st.set ("lastUpdateCheck", now);
        updater->check();
    }
}

// ---------------------------------------------------------------------------- commands
void UiBridge::handle (const juce::String& command, const juce::Array<juce::var>& args, Completion done)
{
    auto it = commands.find (command);
    if (it == commands.end()) { DBG ("Obsidian UI: unknown command " << command); done (obj ({ { "error", "unknown command " + command } })); return; }
    try { it->second (args, done); }
    catch (const std::exception& e) { toast (juce::String ("Error: ") + e.what(), true); done (juce::var()); }
}

void UiBridge::buildCommands()
{
    auto& c = commands;
    auto param = [this] (const juce::String& id) { return proc.state().getParameter (id); };

    // ---- params -------------------------------------------------------------------------
    c["setParam"] = [param] (const juce::Array<juce::var>& a, Completion d)
    {
        if (auto* p = param (argStr (a, 0))) p->setValueNotifyingHost (juce::jlimit (0.f, 1.f, argF (a, 1)));
        d (true);
    };
    c["setParamValue"] = [param] (const juce::Array<juce::var>& a, Completion d)   // real units
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param (argStr (a, 0)))) p->setValueNotifyingHost (p->convertTo0to1 (argF (a, 1)));
        d (true);
    };
    c["beginGesture"] = [param] (const juce::Array<juce::var>& a, Completion d) { if (auto* p = param (argStr (a, 0))) p->beginChangeGesture(); d (true); };
    c["endGesture"]   = [param] (const juce::Array<juce::var>& a, Completion d) { if (auto* p = param (argStr (a, 0))) p->endChangeGesture(); d (true); };
    c["resetParam"]   = [param] (const juce::Array<juce::var>& a, Completion d) { if (auto* p = param (argStr (a, 0))) p->setValueNotifyingHost (p->getDefaultValue()); d (true); };
    c["getParams"]    = [this] (const juce::Array<juce::var>&, Completion d) { d (paramsJson()); };
    c["paramText"]    = [param] (const juce::Array<juce::var>& a, Completion d)
    {
        auto* p = param (argStr (a, 0)); d (p != nullptr ? p->getText (juce::jlimit (0.f, 1.f, argF (a, 1)), 32) : juce::String());
    };

    // ---- rig ------------------------------------------------------------------------------
    c["getRig"]       = [this] (const juce::Array<juce::var>&, Completion d) { d (proc.rig().toJson()); };
    c["libraries"]    = [this] (const juce::Array<juce::var>&, Completion d) { d (obj ({ { "amps", Rig::ampLibrary() }, { "cabs", Rig::cabLibrary() }, { "modules", modulesJson() } })); };
    c["setAmp"]       = [this] (const juce::Array<juce::var>& a, Completion d) { proc.rig().setAmp (argStr (a, 0)); d (proc.rig().toJson()); };
    // capture amps: the amp's own buttons, knobs that aren't plugin parameters, switches
    c["ampDim"]       = [this] (const juce::Array<juce::var>& a, Completion d) { proc.rig().setAmpDim (argInt (a, 0), argInt (a, 1)); proc.presetManager().markDirty(); d (proc.rig().toJson()); };
    c["ampKnob"]      = [this] (const juce::Array<juce::var>& a, Completion d) { proc.rig().setAmpKnob (argInt (a, 0), argF (a, 1)); proc.presetManager().markDirty(); d (true); };
    c["ampSwitch"]    = [this] (const juce::Array<juce::var>& a, Completion d) { proc.rig().setAmpSwitch (argInt (a, 0), a.size() > 1 && (bool) a[1]); proc.presetManager().markDirty(); d (proc.rig().toJson()); };
    c["clearCapture"] = [this] (const juce::Array<juce::var>& a, Completion d) { proc.rig().clearCapture (argInt (a, 0)); d (true); };
    c["loadCapture"]  = [this] (const juce::Array<juce::var>& a, Completion d)
    {
        const int ch = argInt (a, 0);
        chooseFile ("Load a NAM capture", "*.nam", false, [this, ch] (juce::File f) { proc.rig().loadCapture (ch, f); });
        d (true);
    };
    c["loadCaptureFile"] = [this] (const juce::Array<juce::var>& a, Completion d) { proc.rig().loadCapture (argInt (a, 0), juce::File (argStr (a, 1))); d (true); };
    c["setCab"]       = [this] (const juce::Array<juce::var>& a, Completion d) { proc.rig().setCab (argStr (a, 0)); d (proc.rig().toJson()); };
    c["loadIr"]       = [this] (const juce::Array<juce::var>& a, Completion d)
    {
        const int mic = argInt (a, 0, 2);
        chooseFile ("Load an impulse response", "*.wav;*.aif;*.aiff;*.flac", false, [this, mic] (juce::File f) { proc.rig().loadIr (mic, f); });
        d (true);
    };
    c["setModule"]    = [this] (const juce::Array<juce::var>& a, Completion d) { proc.rig().setModule (argInt (a, 0), argInt (a, 1), argStr (a, 2)); d (proc.rig().toJson()); };
    c["moveModule"]   = [this] (const juce::Array<juce::var>& a, Completion d) { proc.rig().moveModule (argInt (a, 0), argInt (a, 1), argInt (a, 2)); d (proc.rig().toJson()); };
    c["loadModuleFile"] = [this] (const juce::Array<juce::var>& a, Completion d)
    {
        const int r = argInt (a, 0), s = argInt (a, 1);
        chooseFile ("Load a pedal capture", "*.nam", false, [this, r, s] (juce::File f) { proc.rig().loadModuleFile (r, s, f); });
        d (true);
    };
    // synth: a note from the UI (AUDITION, computer keys) and the arpeggiator steps
    c["synthNote"]    = [this] (const juce::Array<juce::var>& a, Completion d) { proc.engine().uiSynthNote (juce::jlimit (0, 127, argInt (a, 0, 40)), a.size() > 1 && (bool) a[1],
                                                                                         a.size() > 2 ? juce::jlimit (0.05f, 1.f, (float) (double) a[2]) : 0.85f); d (true); };
    c["setSynthSteps"]= [this] (const juce::Array<juce::var>& a, Completion d)
    {
        proc.rig().setSynthSteps (a.size() > 0 ? a[0] : juce::var());
        proc.presetManager().markDirty(); d (true);
    };
    c["setMisc"]      = [this] (const juce::Array<juce::var>& a, Completion d) { proc.rig().setMisc (argStr (a, 0), a.size() > 1 ? a[1] : juce::var()); proc.presetManager().markDirty(); d (true); };

    // ---- presets --------------------------------------------------------------------------
    auto& pm = proc.presetManager();
    auto result = [this] (bool ok) { if (! ok) toast (proc.presetManager().lastError(), true); return ok; };
    c["presetList"]   = [&pm] (const juce::Array<juce::var>&, Completion d) { d (pm.listJson()); };
    c["presetLoad"]   = [&pm, result] (const juce::Array<juce::var>& a, Completion d) { d (result (pm.load (argStr (a, 0)))); };
    c["presetNext"]   = [&pm, result] (const juce::Array<juce::var>& a, Completion d) { d (result (pm.loadNext (argInt (a, 0, 1)))); };
    c["presetInit"]   = [&pm] (const juce::Array<juce::var>&, Completion d) { d (pm.loadInit()); };
    c["presetSave"]   = [this, &pm, result] (const juce::Array<juce::var>&, Completion d) { const bool ok = result (pm.save()); if (ok) toast ("Saved " + pm.currentName()); d (ok); };
    c["presetSaveAs"] = [this, &pm, result] (const juce::Array<juce::var>& a, Completion d) { const bool ok = result (pm.saveAs (argStr (a, 0), argStr (a, 1), argStr (a, 2))); if (ok) toast ("Saved " + pm.currentName()); d (ok); };
    c["presetDelete"] = [this, &pm, result] (const juce::Array<juce::var>& a, Completion d) { const bool ok = result (pm.remove (argStr (a, 0).isEmpty() ? pm.currentId() : argStr (a, 0))); if (ok) toast ("Preset moved to the recycle bin"); d (ok); };
    c["presetRename"] = [&pm, result] (const juce::Array<juce::var>& a, Completion d) { d (result (pm.rename (argStr (a, 0), argStr (a, 1)))); };
    // your own folders in My presets
    c["presetFolders"]      = [&pm] (const juce::Array<juce::var>&, Completion d) { d (pm.foldersJson()); };
    c["presetNewFolder"]    = [this, &pm, result] (const juce::Array<juce::var>& a, Completion d) { const bool ok = result (pm.createFolder (argStr (a, 0))); if (ok) toast ("Made the folder"); d (ok); };
    c["presetRenameFolder"] = [&pm, result] (const juce::Array<juce::var>& a, Completion d) { d (result (pm.renameFolder (argStr (a, 0), argStr (a, 1)))); };
    c["presetDeleteFolder"] = [this, &pm, result] (const juce::Array<juce::var>& a, Completion d) { const bool ok = result (pm.removeFolder (argStr (a, 0))); if (ok) toast ("Folder moved to the recycle bin"); d (ok); };
    c["presetMove"]         = [this, &pm, result] (const juce::Array<juce::var>& a, Completion d)
    {
        const auto dir = argStr (a, 1);
        const bool ok = result (pm.move (argStr (a, 0), dir));
        if (ok) toast ("Moved to " + (dir.isEmpty() ? juce::String ("My presets") : dir));
        d (ok);
    };
    c["presetRescan"] = [&pm] (const juce::Array<juce::var>&, Completion d) { pm.rescan(); d (pm.listJson()); };
    c["presetImport"] = [this, &pm, result] (const juce::Array<juce::var>&, Completion d)
    {
        chooseFile ("Import a preset", "*.obsidian;*.json", false, [this, &pm, result] (juce::File f) { if (result (pm.importFile (f))) toast ("Imported " + f.getFileNameWithoutExtension()); });
        d (true);
    };
    c["presetExport"] = [this, &pm, result] (const juce::Array<juce::var>& a, Completion d)
    {
        const auto id = argStr (a, 0);
        chooseFile ("Export preset", "*.obsidian", true, [this, &pm, id, result] (juce::File f)
        {
            if (! f.hasFileExtension ("obsidian")) f = f.withFileExtension ("obsidian");
            if (result (pm.exportTo (id, f))) toast ("Exported to " + f.getFileName());
        }, pm.currentName() + ".obsidian");
        d (true);
    };
    c["presetReveal"] = [&pm] (const juce::Array<juce::var>&, Completion d) { pm.folder().revealToUser(); d (true); };
    c["presetChooseFolder"] = [this, &pm] (const juce::Array<juce::var>&, Completion d)
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a preset folder", pm.folder());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this, &pm] (const juce::FileChooser& fc)
        {
            if (fc.getResult() != juce::File()) { pm.setFolder (fc.getResult()); emit ("obs:settings", juce::var (proc.settings().toObject().get())); }
        });
        d (true);
    };

    // ---- settings -------------------------------------------------------------------------
    c["getSettings"] = [this] (const juce::Array<juce::var>&, Completion d) { d (juce::var (proc.settings().toObject().get())); };
    c["setSetting"]  = [this] (const juce::Array<juce::var>& a, Completion d)
    {
        const auto key = argStr (a, 0);
        const auto value = a.size() > 1 ? a[1] : juce::var();
        if (key == "presetFolder") proc.presetManager().setFolder (juce::File (value.toString()));
        else proc.settings().set (key, value);
        if (key == "uiScale" && applyScale) applyScale ((float) (double) value);
        d (true);
    };

    // ---- STUDIO (standalone): the page owns the arrangement, the engine records and plays ---
    c["studioSync"]   = [this] (const juce::Array<juce::var>& a, Completion d) { proc.studio().setArrangement (a.size() > 0 ? a[0] : juce::var()); d (true); };
    c["studioPlay"]   = [this] (const juce::Array<juce::var>& a, Completion d) { proc.studio().play (a.size() > 0 ? (double) a[0] : 0.0); d (true); };
    c["studioRecord"] = [this] (const juce::Array<juce::var>& a, Completion d)
    {
        // round trip: the interface's input + output latency and the plugin's own (when compensation is on)
        int rt = 0;
        if (auto* h = juce::StandalonePluginHolder::getInstance())
            if (auto* dev = h->deviceManager.getCurrentAudioDevice())
                rt = dev->getInputLatencyInSamples() + dev->getOutputLatencyInSamples();
        if ((bool) proc.settings().get ("latencyCompensation")) rt += proc.getLatencySamples();
        proc.studio().setRoundTrip (rt);
        proc.studio().record (argInt (a, 0), a.size() > 1 ? (double) a[1] : 0.0, a.size() > 2 ? (double) a[2] : 0.0);
        d (true);
    };
    c["studioStop"]   = [this] (const juce::Array<juce::var>&, Completion d) { proc.studio().stop(); d (true); };
    c["studioExport"] = [this] (const juce::Array<juce::var>&, Completion d)
    {
        chooseFile ("Export the STUDIO mix", "*.wav", true, [this] (juce::File f)
        {
            if (! f.hasFileExtension ("wav")) f = f.withFileExtension ("wav");
            juce::String err;
            if (proc.studio().exportMix (f, err)) toast ("Exported " + f.getFileName());
            else toast (err, true);
        }, "Obsidian Studio mix.wav");
        d (true);
    };
    c["studioReveal"] = [] (const juce::Array<juce::var>&, Completion d) { auto f = Studio::recordingsFolder(); f.createDirectory(); f.revealToUser(); d (true); };

    // ---- tempo / metronome / tuner -------------------------------------------------------
    c["setBpm"]    = [this] (const juce::Array<juce::var>& a, Completion d) { const double b = juce::jlimit (20.0, 400.0, (double) argF (a, 0, 120.f)); proc.engine().internalBpm.store (b); proc.rig().setMisc ("bpm", b); d (true); };
    c["setBeats"]  = [this] (const juce::Array<juce::var>& a, Completion d) { const int b = juce::jlimit (1, 16, argInt (a, 0, 4)); proc.engine().beatsPerBar.store (b); proc.rig().setMisc ("beats", b); d (true); };
    c["setAccent"] = [this] (const juce::Array<juce::var>& a, Completion d) { proc.engine().accentFirst.store (a.size() > 0 && (bool) a[0]); proc.rig().setMisc ("accent", a.size() > 0 && (bool) a[0]); d (true); };
    c["metronome"] = [this] (const juce::Array<juce::var>& a, Completion d) { proc.engine().setMetronomeRunning (a.size() > 0 && (bool) a[0]); d (true); };
    // the EQ page is open (its analyzer runs only then)
    c["eqSpectrum"] = [this] (const juce::Array<juce::var>& a, Completion d) { proc.engine().eqSpectrum().setWanted (a.size() > 0 && (bool) a[0]); d (true); };
    c["tunerOpen"] = [this] (const juce::Array<juce::var>& a, Completion d) { proc.engine().tunerOpen.store (a.size() > 0 && (bool) a[0]); d (true); };

    // ---- MIDI -------------------------------------------------------------------------------
    c["midiLearn"]       = [this] (const juce::Array<juce::var>& a, Completion d) { proc.midi().startLearn (argStr (a, 0)); toast ("Move a knob or press a switch on your MIDI controller"); d (true); };
    c["midiCancelLearn"] = [this] (const juce::Array<juce::var>&, Completion d) { proc.midi().cancelLearn(); d (true); };
    c["midiMappings"]    = [this] (const juce::Array<juce::var>&, Completion d) { d (proc.midi().toJson()); };
    c["midiRemove"]      = [this] (const juce::Array<juce::var>& a, Completion d)
    {
        proc.midi().remove (argInt (a, 0));
        proc.settings().set ("midiMappings", juce::JSON::toString (proc.midi().toJson(), true));
        d (proc.midi().toJson());
    };
    c["midiDefaults"]    = [this] (const juce::Array<juce::var>&, Completion d)
    {
        proc.midi().fromJson (juce::var());
        juce::Array<juce::var> a;
        for (auto& m : MidiMapper::defaultMappings()) a.add (obj ({ { "cc", m.cc }, { "channel", m.channel }, { "param", m.paramId }, { "toggle", m.toggle } }));
        proc.midi().fromJson (a);
        proc.settings().set ("midiMappings", juce::JSON::toString (proc.midi().toJson(), true));
        d (proc.midi().toJson());
    };

    // ---- audio device (standalone) ---------------------------------------------------------
    for (auto name : { "audioSetType", "audioSetDevice", "audioSetSampleRate", "audioSetBufferSize", "audioSetInputs", "audioSetOutputs",
                       "audioControlPanel", "audioReset", "midiInputEnable" })
    {
        const juce::String n (name);
        c[n] = [this, n] (const juce::Array<juce::var>& a, Completion d)
        {
            const auto err = audioCommand (n, a);
            if (err.isNotEmpty()) toast (err, true);
            d (audioSetup());
        };
    }
    c["audioSetup"] = [this] (const juce::Array<juce::var>&, Completion d) { d (audioSetup()); };
    c["audioTest"]  = [this] (const juce::Array<juce::var>&, Completion d) { proc.playTestTone(); d (true); };

    // ---- misc -------------------------------------------------------------------------------
    c["openUrl"]      = [] (const juce::Array<juce::var>& a, Completion d) { juce::URL (argStr (a, 0)).launchInDefaultBrowser(); d (true); };

    // ---- in-app updates (Settings > About > UPDATES) --------------------------------------
    c["updateCheck"]    = [this] (const juce::Array<juce::var>&, Completion d)
    {
        updater->check();
        proc.settings().set ("lastUpdateCheck", juce::Time::currentTimeMillis() / 1000.0);
        d (updater->toJson());
    };
    c["updateDownload"] = [this] (const juce::Array<juce::var>&, Completion d) { updater->download(); d (updater->toJson()); };
    c["updateCancel"]   = [this] (const juce::Array<juce::var>&, Completion d) { updater->cancel(); d (true); };
    c["updateInstall"]  = [this] (const juce::Array<juce::var>&, Completion d)
    {
        juce::String msg;
        const bool ok = updater->install (proc.isStandalone(), msg);
        auto o = new juce::DynamicObject(); o->setProperty ("ok", ok); o->setProperty ("message", msg);
        d (juce::var (o));
    };
    c["getUpdate"]      = [this] (const juce::Array<juce::var>&, Completion d) { d (updater->toJson()); };
    c["revealContent"] = [] (const juce::Array<juce::var>& a, Completion d)
    {
        auto f = Rig::contentFolder(); const auto sub = argStr (a, 0);
        (sub.isNotEmpty() ? f.getChildFile (sub) : f).revealToUser(); d (true);
    };
    c["log"]   = [] (const juce::Array<juce::var>& a, Completion d) { juce::ignoreUnused (a); DBG ("UI: " << argStr (a, 0)); d (true); };
    c["ready"] = [this] (const juce::Array<juce::var>&, Completion d) { uiReady(); d (initialState()); };
    c["init"]  = [this] (const juce::Array<juce::var>&, Completion d) { d (initialState()); };
}

void UiBridge::chooseFile (const juce::String& title, const juce::String& patterns, bool save, std::function<void (juce::File)> then, const juce::String& defaultName)
{
    auto start = Rig::contentFolder();
    if (defaultName.isNotEmpty()) start = start.getChildFile (defaultName);
    chooser = std::make_unique<juce::FileChooser> (title, start, patterns);
    const int flags = (save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
                            : juce::FileBrowserComponent::openMode) | juce::FileBrowserComponent::canSelectFiles;
    chooser->launchAsync (flags, [then] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f != juce::File() && then) then (f);
    });
}

// ------------------------------------------------------------------------------ events
void UiBridge::toast (const juce::String& t, bool err)
{
    if (t.isNotEmpty()) emit ("obs:toast", obj ({ { "text", t }, { "error", err } }));
}

void UiBridge::parameterChanged (const juce::String& id, float)
{
    // any thread (automation runs on the audio thread): just flag it; the timer sends batches to the UI
    const auto it = indexOf.find (id);
    if (it != indexOf.end()) { dirtyParams[(size_t) it->second].store (true); anyDirty.store (true); }
}

void UiBridge::rigChanged()                         { rigDirty = true; }
void UiBridge::rigMessage (const juce::String& t, bool err) { toast (t, err); }
void UiBridge::presetStateChanged()                 { emit ("obs:preset", proc.presetManager().toJson()); }
void UiBridge::presetListChanged()
{
    auto o = new juce::DynamicObject();
    o->setProperty ("list", proc.presetManager().listJson());
    o->setProperty ("folders", proc.presetManager().foldersJson());
    emit ("obs:presets", juce::var (o));
}
void UiBridge::changeListenerCallback (juce::ChangeBroadcaster*) { emit ("obs:audio", audioSetup()); }

void UiBridge::timerCallback()
{
    if (! ready) return;
    if (proc.isStandalone())
    {
        const auto st = proc.studio().status();
        const bool act = (bool) st["active"];
        if (act || studioWasActive) emit ("obs:studio", st);
        studioWasActive = act;
    }
    if (updater->changes() != updaterSeen) { updaterSeen = updater->changes(); emit ("obs:update", updater->toJson()); }
    if (anyDirty.exchange (false))
    {
        auto o = new juce::DynamicObject();
        for (size_t i = 0; i < ids.size(); ++i)
            if (dirtyParams[i].exchange (false))
                if (auto* p = proc.state().getParameter (ids[i]))
                    o->setProperty (ids[i], p->getValue());
        emit ("obs:params", juce::var (o));
    }
    if (rigDirty) { rigDirty = false; emit ("obs:rig", proc.rig().toJson()); }

    const auto learned = proc.takeLearnMessage();
    if (learned.isNotEmpty()) { toast (learned); emit ("obs:midi", proc.midi().toJson()); }

    auto& e = proc.engine();
    const auto& t = e.tracker();
    const float hz = t.frequency(), conf = t.confidence();
    const float ref = 440.f;
    const float midi = dsp::PitchTracker::toMidi (hz, ref);
    const float cents = hz > 0 ? (midi - std::round (midi)) * 100.f : 0.f;
    auto tuner = obj ({ { "hz", hz }, { "conf", conf }, { "note", hz > 0 && conf > 0.5f ? noteName (midi) : juce::String() }, { "cents", cents }, { "level", t.inputLevel() } });
    auto meters = obj ({
        { "in", e.inputPeak() }, { "outL", e.outputPeakL() }, { "outR", e.outputPeakR() }, { "gate", e.gateGain() },
        { "cpu", e.cpuLoad() }, { "clip", e.takeClip() }, { "tuner", tuner }, { "beat", e.metronomeBeat() },
        { "synth", e.synthActivity() }, { "bpm", e.effectiveBpm() }, { "busy", proc.rig().isBusy() },
        { "synNote", e.synthNote() }, { "synStep", e.synthStep() }, { "synSrc", e.synthSource() },
        { "atm", juce::Array<juce::var> { e.atmosActivity (0), e.atmosActivity (1), e.atmosActivity (2) } }, { "atmEv", e.atmosEvents() }, { "atmRoot", e.atmosRoot() },
        { "kerPos", e.keraunosPos() }, { "gltStep", e.khaosStep() }, { "gltType", e.khaosGlitch() }, { "gltEv", e.khaosEvents() },
        { "hmNote", e.harmonyNote() }, { "hmChord", juce::var (e.harmonyChord()) },
        { "ampA", e.amp().playingA() }, { "ampB", e.amp().playingB() }, { "ampF", e.amp().playingBlend() }, { "cabIn", e.amp().cabInside() },
        { "latencyMs", proc.getSampleRate() > 0 ? proc.getLatencySamples() * 1000.0 / proc.getSampleRate() : 0.0 } });
    if (auto& sp = e.eqSpectrum(); sp.isWanted())
    {
        juce::Array<juce::var> spec;
        for (float db : sp.analyse (proc.getSampleRate(), 1.0 / 30.0)) spec.add (juce::roundToInt (db));
        meters.getDynamicObject()->setProperty ("spec", spec);
    }
    emit ("obs:meters", meters);

    // Device info changes slowly; refresh it now and then for the settings page.
    if (++meterTick % 60 == 0 && proc.isStandalone()) emit ("obs:audioStats", audioSetup());
}

// ------------------------------------------------------------------------- audio device
juce::var UiBridge::audioSetup()
{
#if JucePlugin_Build_Standalone
    auto* h = juce::StandalonePluginHolder::getInstance();
    if (h == nullptr || ! proc.isStandalone()) return obj ({ { "standalone", false } });
    auto& dm = h->deviceManager;
    auto o = new juce::DynamicObject();
    o->setProperty ("standalone", true);
    juce::Array<juce::var> types;
    for (auto* t : dm.getAvailableDeviceTypes()) types.add (t->getTypeName());
    o->setProperty ("types", types);
    auto* type = dm.getCurrentDeviceTypeObject();
    o->setProperty ("type", type != nullptr ? type->getTypeName() : juce::String());
    if (type != nullptr)
    {
        type->scanForDevices();
        juce::Array<juce::var> ins, outs;
        for (auto& n : type->getDeviceNames (true)) ins.add (n);
        for (auto& n : type->getDeviceNames (false)) outs.add (n);
        o->setProperty ("inputDevices", ins); o->setProperty ("outputDevices", outs);
        o->setProperty ("separateInOut", type->hasSeparateInputsAndOutputs());
    }
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    dm.getAudioDeviceSetup (setup);
    o->setProperty ("inputDevice", setup.inputDeviceName);
    o->setProperty ("outputDevice", setup.outputDeviceName);
    if (auto* dev = dm.getCurrentAudioDevice())
    {
        juce::Array<juce::var> rates, sizes, inCh, outCh, inOn, outOn;
        for (auto r : dev->getAvailableSampleRates()) rates.add (r);
        for (auto s : dev->getAvailableBufferSizes()) sizes.add (s);
        for (auto& n : dev->getInputChannelNames()) inCh.add (n);
        for (auto& n : dev->getOutputChannelNames()) outCh.add (n);
        const auto ai = dev->getActiveInputChannels(), ao = dev->getActiveOutputChannels();
        for (int i = 0; i < inCh.size(); ++i) inOn.add (ai[i]);
        for (int i = 0; i < outCh.size(); ++i) outOn.add (ao[i]);
        o->setProperty ("sampleRates", rates); o->setProperty ("bufferSizes", sizes);
        o->setProperty ("inputChannels", inCh); o->setProperty ("outputChannels", outCh);
        o->setProperty ("inputsOn", inOn); o->setProperty ("outputsOn", outOn);
        o->setProperty ("sampleRate", dev->getCurrentSampleRate());
        o->setProperty ("bufferSize", dev->getCurrentBufferSizeSamples());
        o->setProperty ("bitDepth", dev->getCurrentBitDepth());
        o->setProperty ("inputLatencyMs", dev->getInputLatencyInSamples() * 1000.0 / juce::jmax (1.0, dev->getCurrentSampleRate()));
        o->setProperty ("outputLatencyMs", dev->getOutputLatencyInSamples() * 1000.0 / juce::jmax (1.0, dev->getCurrentSampleRate()));
        o->setProperty ("hasControlPanel", dev->hasControlPanel());
        o->setProperty ("deviceName", dev->getName());
    }
    else o->setProperty ("error", "No audio device is open. Pick one below.");
    o->setProperty ("cpu", dm.getCpuUsage());
    juce::Array<juce::var> midiIns;
    for (auto& m : juce::MidiInput::getAvailableDevices())
        midiIns.add (obj ({ { "name", m.name }, { "id", m.identifier }, { "enabled", dm.isMidiInputDeviceEnabled (m.identifier) } }));
    o->setProperty ("midiInputs", midiIns);
    return juce::var (o);
#else
    return obj ({ { "standalone", false } });
#endif
}

juce::String UiBridge::audioCommand (const juce::String& what, const juce::Array<juce::var>& a)
{
#if JucePlugin_Build_Standalone
    auto* h = juce::StandalonePluginHolder::getInstance();
    if (h == nullptr || ! proc.isStandalone()) return "Audio settings are managed by your DAW.";
    auto& dm = h->deviceManager;
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    dm.getAudioDeviceSetup (setup);
    juce::String err;

    if (what == "audioSetType")
    {
        dm.setCurrentAudioDeviceType (argStr (a, 0), true);
        // open the first device of the new type so there's always sound
        if (dm.getCurrentAudioDevice() == nullptr)
        {
            dm.getAudioDeviceSetup (setup);
            if (auto* t = dm.getCurrentDeviceTypeObject())
            {
                t->scanForDevices();
                setup.outputDeviceName = t->getDeviceNames (false)[t->getDefaultDeviceIndex (false)];
                setup.inputDeviceName  = t->getDeviceNames (true)[t->getDefaultDeviceIndex (true)];
                setup.useDefaultInputChannels = setup.useDefaultOutputChannels = true;
                err = dm.setAudioDeviceSetup (setup, true);
            }
        }
    }
    else if (what == "audioSetDevice")
    {
        setup.inputDeviceName = argStr (a, 0);
        setup.outputDeviceName = argStr (a, 1).isNotEmpty() ? argStr (a, 1) : argStr (a, 0);
        setup.useDefaultInputChannels = setup.useDefaultOutputChannels = true;
        err = dm.setAudioDeviceSetup (setup, true);
    }
    else if (what == "audioSetSampleRate") { setup.sampleRate = argF (a, 0); err = dm.setAudioDeviceSetup (setup, true); }
    else if (what == "audioSetBufferSize") { setup.bufferSize = argInt (a, 0); err = dm.setAudioDeviceSetup (setup, true); }
    else if (what == "audioSetInputs" || what == "audioSetOutputs")
    {
        juce::BigInteger bits;
        if (auto* arr = a.size() > 0 ? a[0].getArray() : nullptr) for (int i = 0; i < arr->size(); ++i) bits.setBit (i, (bool) (*arr)[i]);
        if (what == "audioSetInputs") { setup.inputChannels = bits; setup.useDefaultInputChannels = false; }
        else { setup.outputChannels = bits; setup.useDefaultOutputChannels = false; }
        err = dm.setAudioDeviceSetup (setup, true);
    }
    else if (what == "audioControlPanel")
    {
        if (auto* dev = dm.getCurrentAudioDevice(); dev != nullptr && dev->hasControlPanel())
        {
            if (dev->showControlPanel()) { dm.closeAudioDevice(); dm.restartLastAudioDevice(); }
        }
        else err = "This device has no control panel";
    }
    else if (what == "audioReset")
    {
        dm.closeAudioDevice();
        dm.restartLastAudioDevice();
        if (dm.getCurrentAudioDevice() == nullptr) err = dm.initialiseWithDefaultDevices (2, 2);
    }
    else if (what == "midiInputEnable")
        dm.setMidiInputDeviceEnabled (argStr (a, 0), a.size() > 1 && (bool) a[1]);

    h->getMuteInputValue().setValue (false);
    h->saveAudioDeviceState();
    return err;
#else
    juce::ignoreUnused (what, a);
    return "Audio settings are managed by your DAW.";
#endif
}
} // namespace obs
