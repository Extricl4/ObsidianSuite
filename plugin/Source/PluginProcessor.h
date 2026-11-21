#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Core/Params.h"
#include "Core/AppSettings.h"
#include "Core/Rig.h"
#include "Core/PresetManager.h"
#include "Core/MidiMapper.h"
#include "DSP/Engine.h"

namespace obs
{
class ObsidianProcessor : public juce::AudioProcessor,
                          private juce::Timer,
                          private AppSettings::Listener
{
public:
    ObsidianProcessor();
    ~ObsidianProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                 { return true; }

    const juce::String getName() const override     { return OBSIDIAN_PRODUCT_NAME; }
    bool acceptsMidi() const override               { return true; }
    bool producesMidi() const override              { return false; }
    bool isMidiEffect() const override              { return false; }
    double getTailLengthSeconds() const override    { return 4.0; }

    int getNumPrograms() override                   { return 1; }
    int getCurrentProgram() override                { return 0; }
    void setCurrentProgram (int) override           {}
    const juce::String getProgramName (int) override { return presets->currentName(); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- access for the editor / bridge -----------------------------------------------
    juce::AudioProcessorValueTreeState& state()     { return apvts; }
    Engine& engine()                                { return dsp; }
    Rig& rig()                                      { return *rigPtr; }
    PresetManager& presetManager()                  { return *presets; }
    MidiMapper& midi()                              { return midiMapper; }
    AppSettings& settings()                         { return *appSettings; }
    bool isStandalone() const                       { return wrapperType == wrapperType_Standalone; }

    /** Plays a short test tone through the output (Settings > Device > TEST). */
    void playTestTone()                             { testTone.store ((int) (getSampleRate() * 0.6)); }
    juce::String takeLearnMessage()                 { const juce::ScopedLock sl (msgLock); auto m = learnMessage; learnMessage = {}; return m; }

private:
    void timerCallback() override;
    void settingChanged (const juce::String& key, const juce::var& value) override;
    void applySettings();
    void loadStartupSound();

    juce::SharedResourcePointer<AppSettings> appSettings;
    Engine dsp;
    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<Rig> rigPtr;
    std::unique_ptr<PresetManager> presets;
    MidiMapper midiMapper;
    std::atomic<int> testTone { 0 };
    float testPhase = 0;
    std::atomic<int> midiChannel { 0 };
    std::atomic<bool> pcLoadsPresets { true };
    bool stateWasRestored = false;
    int reportedLatency = -1;
    juce::CriticalSection msgLock;
    juce::String learnMessage;
    juce::AudioBuffer<float> doubleScratch;
    std::shared_ptr<bool> aliveToken = std::make_shared<bool> (true);   // expires with the processor

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ObsidianProcessor)
};
} // namespace obs
