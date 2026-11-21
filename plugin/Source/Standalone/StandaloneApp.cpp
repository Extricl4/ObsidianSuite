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
int writeFactoryPresets (const juce::File& dir);

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
            setApplicationReturnValue (writeFactoryPresets (juce::File::getCurrentWorkingDirectory().getChildFile (args[i + 1].unquoted())));
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
        if (args.contains ("--selftest"))
        {
            setApplicationReturnValue (runSelfTest (args));
            quit();
            return;
        }

        auto* settings = props.getUserSettings();
        const bool firstRun = ! settings->containsKey ("audioSetup");
        holder = std::make_unique<juce::StandalonePluginHolder> (settings, false, juce::String(), nullptr,
                                                                 juce::Array<juce::StandalonePluginHolder::PluginInOuts>(), true);
        holder->getMuteInputValue().setValue (false);
        if (firstRun) preferAsio();

        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty()) return;   // headless: audio only
        window = std::make_unique<MainWindow> (*holder, settings);
    }

    void shutdown() override
    {
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
    std::unique_ptr<MainWindow> window;
};
} // namespace obs

juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication() { return new obs::StandaloneApp(); }

#endif
