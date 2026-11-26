#pragma once
#include "../Core/Updater.h"
// =============================================================================
//  UiBridge: the ONE door between the web UI and the plugin.
//
//  UI -> plugin:  window.obsidian.call("command", ...args)  -> handle()
//  plugin -> UI:  events "obs:params", "obs:meters", "obs:rig", "obs:preset",
//                 "obs:presets", "obs:toast", "obs:audio", "obs:settings", "obs:cabs" (the cab list, after INSTALL PACK)
//
//  ADDING A COMMAND: add one entry in UiBridge::buildCommands() (UiBridge.cpp)
//  and call it from WebUI/obsidian-bridge.js. Nothing else to touch.
// =============================================================================
#include <juce_gui_extra/juce_gui_extra.h>
#include "../PluginProcessor.h"

namespace obs
{
class UiBridge : private juce::Timer,
                 private juce::AudioProcessorValueTreeState::Listener,
                 private Rig::Listener,
                 private PresetManager::Listener,
                 private juce::ChangeListener
{
public:
    using Emit = std::function<void (const juce::String& event, const juce::var& payload)>;
    using Completion = std::function<void (juce::var)>;

    UiBridge (ObsidianProcessor& p, Emit emitter, std::function<void (float)> setScale);
    ~UiBridge() override;

    /** Called for every window.obsidian.call(...) from JavaScript. */
    void handle (const juce::String& command, const juce::Array<juce::var>& args, Completion done);
    juce::var initialState();
    void uiReady();

private:
    using Cmd = std::function<void (const juce::Array<juce::var>&, Completion)>;
    void buildCommands();
    void timerCallback() override;
    void parameterChanged (const juce::String& id, float) override;
    void rigChanged() override;
    void rigMessage (const juce::String&, bool) override;
    void presetStateChanged() override;
    void presetListChanged() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void toast (const juce::String& t, bool err = false);
    void chooseFile (const juce::String& title, const juce::String& patterns, bool save, std::function<void (juce::File)> then, const juce::String& defaultName = {});

    // Standalone audio device control (no-ops inside a DAW).
    juce::var audioSetup();
    juce::String audioCommand (const juce::String& what, const juce::Array<juce::var>& args);

    juce::var paramsJson();
    juce::var modulesJson();

    ObsidianProcessor& proc;
    juce::SharedResourcePointer<Updater> updater;   // one per process, shared by every window
    int updaterSeen = 0;
    bool studioWasActive = false;
    Emit emit;
    std::function<void (float)> applyScale;
    std::unordered_map<juce::String, Cmd> commands;
    std::vector<juce::String> ids;
    std::unordered_map<juce::String, int> indexOf;
    std::unique_ptr<std::atomic<bool>[]> dirtyParams;
    std::atomic<bool> anyDirty { false };
    std::unique_ptr<juce::FileChooser> chooser;
    bool ready = false, rigDirty = false;
    int meterTick = 0;
    juce::ChangeBroadcaster* deviceManagerBroadcaster = nullptr;
};
} // namespace obs
