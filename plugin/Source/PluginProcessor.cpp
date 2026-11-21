#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace obs
{
ObsidianProcessor::ObsidianProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "OBSIDIAN_PARAMS", createLayout())
{
    dsp.attach (apvts);
    rigPtr = std::make_unique<Rig> (dsp, apvts);
    presets = std::make_unique<PresetManager> (apvts, *rigPtr, *appSettings);
    appSettings->addListener (this);
    applySettings();
    rigPtr->applyAll();

    // If the host doesn't restore a state (new instance / standalone), start with your last sound.
    juce::MessageManager::callAsync ([token = std::weak_ptr<bool> (aliveToken), this]
    {
        if (! token.expired() && ! stateWasRestored) loadStartupSound();
    });
    startTimerHz (30);
}

ObsidianProcessor::~ObsidianProcessor()
{
    stopTimer();
    appSettings->removeListener (this);
    appSettings->flush();
    presets.reset();
    rigPtr.reset();
    dsp.release();
}

void ObsidianProcessor::loadStartupSound()
{
    const auto last = appSettings->get ("lastPreset").toString();
    if ((bool) appSettings->get ("openLastPreset") && last.isNotEmpty() && presets->load (last)) return;
    // otherwise the clean default sound (Default preset)
}

void ObsidianProcessor::applySettings()
{
    dsp.inputMode.store ((int) appSettings->get ("inputMode"));
    dsp.inputCalibrationDb.store ((float) (double) appSettings->get ("inputCalibration"));
    dsp.muteWhileTuning.store ((bool) appSettings->get ("muteWhileTuning"));
    dsp.metronomeVolume.store ((float) (double) appSettings->get ("metronomeVolume"));
    midiChannel.store ((int) appSettings->get ("midiChannel"));
    pcLoadsPresets.store ((bool) appSettings->get ("pcLoadsPresets"));
    juce::var maps;
    if (juce::JSON::parse (appSettings->get ("midiMappings").toString(), maps).wasOk() && maps.isArray() && maps.size() > 0)
        midiMapper.fromJson (maps);
    const int os = juce::jlimit (1, 4, (int) appSettings->get ("oversampling"));
    if (os != dsp.oversampling.load())
    {
        dsp.oversampling.store (os);
        // Modules read the factor in prepare(): re-prepare if we are running.
        if (getSampleRate() > 0)
        {
            suspendProcessing (true);
            dsp.prepare (getSampleRate(), getBlockSize());
            suspendProcessing (false);
        }
    }
}

void ObsidianProcessor::settingChanged (const juce::String& key, const juce::var&)
{
    if (key == "midiMappings") return;   // set by the mapper itself
    juce::MessageManager::callAsync ([token = std::weak_ptr<bool> (aliveToken), this] { if (! token.expired()) applySettings(); });
}

void ObsidianProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    dsp.prepare (sampleRate, juce::jmax (16, samplesPerBlock));
    doubleScratch.setSize (2, juce::jmax (16, samplesPerBlock));
    reportedLatency = -1;
    timerCallback();
}

void ObsidianProcessor::releaseResources()
{
    dsp.collectGarbage (false);
}

bool ObsidianProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo() || in.isDisabled();
}

void ObsidianProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    midiMapper.process (midi, midiChannel.load(), pcLoadsPresets.load());
    midiMapper.blockDone();

    // MIDI notes play the synth (a keyboard, or notes from the DAW).
    {
        const int chan = midiChannel.load();
        for (const auto meta : midi)
        {
            const auto msg = meta.getMessage();
            if (chan > 0 && msg.getChannel() != chan) continue;
            if (msg.isNoteOn())                              dsp.addSynthNote (meta.samplePosition, msg.getNoteNumber(), msg.getFloatVelocity());
            else if (msg.isNoteOff())                        dsp.addSynthNote (meta.samplePosition, msg.getNoteNumber(), 0.f);
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
                for (int nn = 0; nn < 128; ++nn) dsp.addSynthNote (meta.samplePosition, nn, 0.f);
        }
    }

    HostInfo host;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) { host.bpm = *bpm; host.hasHostTempo = ! isStandalone(); }
            host.playing = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition()) host.ppq = *ppq;
        }

    const int numIn = getTotalNumInputChannels();
    dsp.process (buffer, numIn, host);

    // Test tone (Settings > Device > TEST).
    if (int left = testTone.load(); left > 0)
    {
        const int n = juce::jmin (left, buffer.getNumSamples());
        const float inc = juce::MathConstants<float>::twoPi * 440.f / (float) getSampleRate();
        for (int i = 0; i < n; ++i)
        {
            const float env = juce::jmin (1.f, (float) (left - i) / 2000.f);
            const float s = std::sin (testPhase) * 0.12f * env;
            testPhase += inc; if (testPhase > juce::MathConstants<float>::twoPi) testPhase -= juce::MathConstants<float>::twoPi;
            for (int c = 0; c < buffer.getNumChannels(); ++c) buffer.addSample (c, i, s);
        }
        testTone.store (left - n);
    }
    midi.clear();
}

void ObsidianProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi)
{
    // Rare (some hosts render in double): convert through a float buffer.
    const int n = buffer.getNumSamples(), ch = buffer.getNumChannels();
    if (doubleScratch.getNumChannels() < ch || doubleScratch.getNumSamples() < n) doubleScratch.setSize (ch, n, false, false, true);
    for (int c = 0; c < ch; ++c) for (int i = 0; i < n; ++i) doubleScratch.setSample (c, i, (float) buffer.getSample (c, i));
    juce::AudioBuffer<float> view (doubleScratch.getArrayOfWritePointers(), ch, n);
    processBlock (view, midi);
    for (int c = 0; c < ch; ++c) for (int i = 0; i < n; ++i) buffer.setSample (c, i, (double) view.getSample (c, i));
}

void ObsidianProcessor::timerCallback()
{
    // MIDI events (CC -> params, program change -> preset, learn).
    const auto learned = midiMapper.dispatch (apvts, [this] (int pc)
    {
        const auto& list = presets->list();
        if (pc >= 0 && pc < list.size()) presets->load (list[pc].id);
    });
    if (learned.isNotEmpty())
    {
        appSettings->set ("midiMappings", juce::JSON::toString (midiMapper.toJson(), true));
        const juce::ScopedLock sl (msgLock); learnMessage = learned;
    }
    midiMapper.collectGarbage();
    presets->pollDirty();

    // Latency reporting (so the DAW can compensate).
    const int lat = (bool) appSettings->get ("latencyCompensation") ? dsp.latencySamples() : 0;
    if (lat != reportedLatency) { reportedLatency = lat; setLatencySamples (lat); }
}

// ------------------------------------------------------------------------------------- state
void ObsidianProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree root ("OBSIDIAN");
    root.setProperty ("version", OBSIDIAN_VERSION_STRING, nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    root.appendChild (rigPtr->snapshot(), nullptr);
    juce::ValueTree pre ("PRESET");
    pre.setProperty ("id", presets->currentId(), nullptr).setProperty ("name", presets->currentName(), nullptr).setProperty ("dirty", presets->isDirty(), nullptr);
    root.appendChild (pre, nullptr);
    if (auto xml = root.createXml()) copyXmlToBinary (*xml, dest);
}

void ObsidianProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("OBSIDIAN")) return;
    stateWasRestored = true;

    auto apply = [this, root]
    {
        const auto params = root.getChildWithName ("OBSIDIAN_PARAMS");
        if (params.isValid()) apvts.replaceState (params);
        rigPtr->restore (root.getChildWithName ("RIG"));
        const auto pre = root.getChildWithName ("PRESET");
        presets->setCurrentFromState (pre["id"].toString(), pre["name"].toString(), (bool) pre["dirty"]);
    };
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr && juce::MessageManager::getInstance()->isThisTheMessageThread()) apply();
    else juce::MessageManager::callAsync ([token = std::weak_ptr<bool> (aliveToken), apply] { if (! token.expired()) apply(); });
}

juce::AudioProcessorEditor* ObsidianProcessor::createEditor()
{
   #if OBSIDIAN_HEADLESS
    return nullptr;
   #else
    return new ObsidianEditor (*this);
   #endif
}
} // namespace obs

// The plugin entry point JUCE's wrappers call.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new obs::ObsidianProcessor();
}
