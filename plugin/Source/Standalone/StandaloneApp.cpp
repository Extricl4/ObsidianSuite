// =============================================================================
//  The standalone "Obsidian Amp Suite.exe": our own app window around the plugin.
//   * no JUCE "Options" bar: audio/MIDI settings live in Obsidian's Settings
//   * input is never muted (it's a guitar amp)
//   * first launch: picks ASIO if a driver is installed (e.g. Focusrite USB ASIO)
//   * "Obsidian Amp Suite.exe --selftest report.txt" runs the offline test suite
// =============================================================================
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>

#if JucePlugin_Build_Standalone

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "../PluginProcessor.h"
#include "../Editor/UiBridge.h"

namespace obs
{
int runSelfTest (const juce::StringArray& args);
int writeFactoryPresets (const juce::File& dir, const juce::File& spec, float targetDb);
int renderSynthDemos (const juce::File& dir);
int renderPedalDemos (const juce::File& dir);
int renderSounds (const juce::File& dir, const juce::File& spec);

/** Opens MIDI inputs as they appear, like JUCE's own auto-open, but following Settings > MIDI > "MIDI input".
    (JUCE's turns every device on half a second after launch, which undid picking one device.)
    A device that shows up is turned on when the setting is "All devices", when it is the chosen one,
    or when the chosen one isn't connected (the menu then shows "All devices" too). */
class MidiWatch : private juce::Timer
{
public:
    explicit MidiWatch (juce::AudioDeviceManager& d) : dm (d) { timerCallback(); startTimer (700); }
private:
    void timerCallback() override
    {
        const auto now = juce::MidiInput::getAvailableDevices();
        if (now == last) return;
        const auto want = settings->get ("midiInput").toString();
        bool present = false;
        for (auto& d : now) present = present || d.name == want;
        for (auto& d : last)
            if (! now.contains (d)) dm.setMidiInputDeviceEnabled (d.identifier, false);
        for (auto& d : now)
            if (! last.contains (d))
                dm.setMidiInputDeviceEnabled (d.identifier, want.isEmpty() || want == "All devices" || ! present || d.name == want);
        last = now;
    }
    juce::AudioDeviceManager& dm;
    juce::Array<juce::MidiDeviceInfo> last;
    juce::SharedResourcePointer<AppSettings> settings;
};

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow (juce::StandalonePluginHolder& h, juce::PropertySet* props)
        : DocumentWindow (OBSIDIAN_PRODUCT_NAME, juce::Colour (0xff0c0c0d), DocumentWindow::allButtons), holder (h), settings (props)
    {
        setUsingNativeTitleBar (true);
        if (auto* p = holder.processor.get())
        {
            editor.reset (p->createEditorIfNeeded());
            if (editor != nullptr)
            {
                setContentNonOwned (editor.get(), true);
                getConstrainer()->setFixedAspectRatio ((double) editor->getWidth() / juce::jmax (1, editor->getHeight()));
            }
        }
        setResizable (true, false);
        setResizeLimits (680, 418, 2720, 1672);

        if (settings != nullptr && settings->containsKey ("windowPos"))
            restoreWindowStateFromString (settings->getValue ("windowPos"));
        else
            centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }
    ~MainWindow() override
    {
        if (settings != nullptr) settings->setValue ("windowPos", getWindowStateAsString());
        clearContentComponent();
        editor.reset();
    }
    void closeButtonPressed() override  { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
private:
    juce::StandalonePluginHolder& holder;
    juce::PropertySet* settings;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
};

class StandaloneApp : public juce::JUCEApplication
{
public:
    StandaloneApp()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "standalone";
        o.filenameSuffix = ".settings";
        o.folderName = OBSIDIAN_PRODUCT_NAME;
        o.osxLibrarySubFolder = "Application Support";
        props.setStorageParameters (o);
    }

    const juce::String getApplicationName() override       { return OBSIDIAN_PRODUCT_NAME; }
    const juce::String getApplicationVersion() override    { return OBSIDIAN_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override             { return false; }
    void anotherInstanceStarted (const juce::String&) override { if (window != nullptr) window->toFront (true); }

    void initialise (const juce::String& commandLine) override
    {
        const auto args = juce::StringArray::fromTokens (commandLine, true);
        if (const int i = args.indexOf ("--write-factory-presets"); i >= 0)
        {
            const auto cwd = juce::File::getCurrentWorkingDirectory();
            setApplicationReturnValue (writeFactoryPresets (cwd.getChildFile (args[i + 1].unquoted()), cwd.getChildFile (args[i + 2].unquoted()),
                                                            args.size() > i + 3 && ! args[i + 3].startsWith ("--") ? args[i + 3].getFloatValue() : -20.f));
            quit();
            return;
        }
        if (const int i = args.indexOf ("--render-sounds"); i >= 0)
        {
            const auto cwd = juce::File::getCurrentWorkingDirectory();
            setApplicationReturnValue (renderSounds (cwd.getChildFile (args[i + 1].unquoted()), cwd.getChildFile (args[i + 2].unquoted())));
            quit();
            return;
        }
        if (const int i = args.indexOf ("--render-synth"); i >= 0)
        {
            setApplicationReturnValue (renderSynthDemos (juce::File::getCurrentWorkingDirectory().getChildFile (args[i + 1].unquoted())));
            quit();
            return;
        }
        if (const int i = args.indexOf ("--render-pedal-demos"); i >= 0)
        {
            setApplicationReturnValue (renderPedalDemos (juce::File::getCurrentWorkingDirectory().getChildFile (args[i + 1].unquoted())));
            quit();
            return;
        }
        if (const int i = args.indexOf ("--dump-ui-state"); i >= 0)
        {
            // for UI development: writes what the page receives at start-up (see tools/ui-test)
            ObsidianProcessor p;
            p.setPlayConfigDetails (2, 2, 48000, 256);
            p.prepareToPlay (48000, 256);
            {
                UiBridge bridge (p, [] (const juce::String&, const juce::var&) {}, [] (float) {});
                juce::File::getCurrentWorkingDirectory().getChildFile (args[i + 1].unquoted()).replaceWithText (juce::JSON::toString (bridge.initialState()));
            }
            p.releaseResources();
            quit();
            return;
        }
        if (const int i = args.indexOf ("--dump-presets"); i >= 0)
        {
            // for the web preview: what every built-in preset does to the knobs (normalised, only
            // those that differ from Default) and the rig, exactly as the page would receive them
            ObsidianProcessor p;
            p.setPlayConfigDetails (2, 2, 48000, 256);
            p.prepareToPlay (48000, 256);
            const auto oldLast = p.settings().get ("lastPreset");
            obs::Rig::asyncFileLoads = false;
            auto& pm = p.presetManager();
            auto norms = [&p]
            {
                juce::NamedValueSet v;
                for (auto* prm : p.getParameters())
                    if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm)) v.set (r->getParameterID(), std::round (r->getValue() * 1.0e6) / 1.0e6);
                return v;
            };
            pm.loadInit();
            const auto def = norms();
            auto out = new juce::DynamicObject(), all = new juce::DynamicObject();
            out->setProperty ("defaultRig", p.rig().toJson());
            auto dv = new juce::DynamicObject();
            for (auto& nv : def) dv->setProperty (nv.name, nv.value);
            out->setProperty ("defaults", juce::var (dv));
            int n = 0;
            for (auto& pr : pm.list())
            {
                if (! pr.factory || ! pm.load (pr.id)) continue;
                auto e = new juce::DynamicObject(), pv = new juce::DynamicObject();
                const auto now = norms();
                for (auto& nv : now) if (std::abs ((double) nv.value - (double) def[nv.name]) > 1.0e-6) pv->setProperty (nv.name, nv.value);
                e->setProperty ("p", juce::var (pv));
                e->setProperty ("rig", p.rig().toJson());
                all->setProperty (pr.id, juce::var (e));
                if (++n % 40 == 0) p.engine().collectGarbage (true);
            }
            out->setProperty ("presets", juce::var (all));
            obs::Rig::asyncFileLoads = true;
            pm.loadInit();
            p.settings().set ("lastPreset", oldLast);
            juce::File::getCurrentWorkingDirectory().getChildFile (args[i + 1].unquoted()).replaceWithText (juce::JSON::toString (juce::var (out), true));
            std::printf ("%d presets\n", n);
            p.releaseResources();
            quit();
            return;
        }
        if (args.contains ("--selftest"))
        {
            setApplicationReturnValue (runSelfTest (args));
            quit();
            return;
        }

        auto* settings = props.getUserSettings();
        const bool firstRun = ! settings->containsKey ("audioSetup");
        holder = std::make_unique<juce::StandalonePluginHolder> (settings, false, juce::String(), nullptr,
                                                                 juce::Array<juce::StandalonePluginHolder::PluginInOuts>(), false);
        holder->getMuteInputValue().setValue (false);
        if (firstRun) preferAsio();
        midiWatch = std::make_unique<MidiWatch> (holder->deviceManager);

        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty()) return;   // headless: audio only
        window = std::make_unique<MainWindow> (*holder, settings);
    }

    void shutdown() override
    {
        midiWatch = nullptr;
        if (holder != nullptr) { holder->savePluginState(); holder->saveAudioDeviceState(); }
        window = nullptr;
        holder = nullptr;
        props.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (holder != nullptr) holder->savePluginState();
        quit();
    }

private:
    /** On the very first launch, use ASIO when a driver is present (lowest latency). */
    void preferAsio()
    {
        auto& dm = holder->deviceManager;
        for (auto* type : dm.getAvailableDeviceTypes())
        {
            if (type->getTypeName() != "ASIO") continue;
            type->scanForDevices();
            const auto names = type->getDeviceNames();
            if (names.isEmpty()) return;
            dm.setCurrentAudioDeviceType ("ASIO", true);
            juce::AudioDeviceManager::AudioDeviceSetup s;
            dm.getAudioDeviceSetup (s);
            s.inputDeviceName = s.outputDeviceName = names[0];
            s.useDefaultInputChannels = s.useDefaultOutputChannels = true;
            s.bufferSize = 128;
            if (dm.setAudioDeviceSetup (s, true).isNotEmpty())
            {
                s.bufferSize = 256;
                dm.setAudioDeviceSetup (s, true);
            }
            holder->saveAudioDeviceState();
            return;
        }
    }

    juce::ApplicationProperties props;
    std::unique_ptr<juce::StandalonePluginHolder> holder;
    std::unique_ptr<MidiWatch> midiWatch;
    std::unique_ptr<MainWindow> window;
};
} // namespace obs

juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication() { return new obs::StandaloneApp(); }

#endif
