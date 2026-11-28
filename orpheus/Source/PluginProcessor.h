#pragma once
/*  ORPHEUS · the instrument: SIREN (the synth) with its EFFECTS, played by ORPHEUS (the song you draw) and by MIDI.
    - ORPHEUS follows the DAW: when the DAW plays, the song plays in time with it (looped or once). Stopped, or in the
      standalone app, ORPHEUS's own PLAY button plays it at the song's tempo.
    - MIDI notes (a keyboard, a DAW clip) play SIREN too, as chords, on a voice of their own.
    - Every SIREN setting is a host parameter (automatable); the song is saved with the project.                     */
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "DSP/SirenSynth.h"
#include "DSP/Effects.h"
#include "Seq/Song.h"

namespace orph
{
class OrpheusProcessor : public juce::AudioProcessor
{
public:
    OrpheusProcessor();
    ~OrpheusProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override { seqSyn.allOff(); keySyn.allOff(); fx.reset(); sndN = 0; held.fill (0); nHeld = 0; auditioning = false; }
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "ORPHEUS"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---------------------------------------------------------------- from the UI (message thread)
    void setParamFromUi (int index, float realValue, bool noTrails);
    void setSong (const juce::String& songJson, const juce::var& marks);
    juce::String songJson() const;
    void transport (const juce::String& what, double ph);          // "play" (from ph), "stop", "seek"
    void audition (const float* notes, int n, float vel, bool hold);
    void auditionPitches (const float* notes, int n);
    void auditionOff();
    void uiNote (int note, bool on, float vel);                     // SIREN's on-screen KEYS
    void setBend (float semis) noexcept { uiBend = semis; }
    void setMod (float v) noexcept { uiMod = v; }
    void levelRoll (float targetDb) noexcept { rollTarget = targetDb; rollWant = true; }
    juce::var uiExtras;                                             // the UI's own small things (preset name), kept with the state

    // ---------------------------------------------------------------- for the UI (any thread)
    struct Pos { double ph; bool playing, host; double bpm; };
    Pos position() const noexcept { return { posPh.load(), posPlaying.load(), posHost.load(), posBpm.load() }; }
    /** a dice roll's level, measured: the trim to set (NaN: none yet) */
    float takeRollTrim() noexcept { return rollResult.exchange (std::numeric_limits<float>::quiet_NaN()); }
    std::atomic<int> stateGen { 0 };                                // bumped when a saved state is loaded (the UI reloads it)

    // the offline test drives these directly
    void testRaw (bool b) { fx.setTestRaw (b); }
    void testWaitForReverb (int ms) { for (int i = 0; i < ms / 10 && ! fx.irReady(); ++i) { fx.pumpIR(); if (! fx.irReady()) juce::Thread::sleep (10); } juce::Thread::sleep (60); }   // (and JUCE's own swap)

private:
    struct Cmd { enum Type { Audition, AudPitch, AudOff, KeyOn, KeyOff, Play, Stop, Seek } type; float notes[4]; int n; float vel; bool hold; double ph; };
    void push (const Cmd& c);
    void runCommands();
    void handleMidi (const juce::MidiMessage& m);
    void keysChanged (float vel, bool pressed);
    void stepSequencer (int n, double hostPh, bool hostPlay);
    void seqSoundOff();
    void takeSnapshot (Snapshot& s) const noexcept;

    std::array<std::atomic<float>*, P_COUNT> raw {};
    Snapshot S, prevS; bool first = true;
    SirenSynth seqSyn, keySyn;
    FxChain fx;
    double sr = 48000;

    // the song (swapped in by the message thread; read by the audio thread under a try-lock)
    juce::SpinLock songLock; std::unique_ptr<Song> song, songOld;

    // the player
    bool playing = false, followingHost = false; double ph = 0; int sndN = 0, kickMark = -2;
    std::atomic<double> posPh { 0 }, posBpm { 80 }; std::atomic<bool> posPlaying { false }, posHost { false };

    // keys (MIDI, SIREN's on-screen KEYS) and auditioning (clicking ORPHEUS's dots and keys)
    std::array<float, 128> held {}; int nHeld = 0; float lastVel = .8f;
    bool auditioning = false;
    float midiBend = 0, midiMod = 0; std::atomic<float> uiBend { 0 }, uiMod { 0 };

    // commands from the UI
    juce::AbstractFifo fifo { 256 }; std::array<Cmd, 256> cmds {};
    juce::CriticalSection pushLock;
    std::atomic<int64_t> noTrailsUntil { 0 };

    // a dice roll's level (synth.js levelRoll)
    std::atomic<bool> rollWant { false }; std::atomic<float> rollTarget { -23.5f }, rollResult { std::numeric_limits<float>::quiet_NaN() };
    bool rolling = false; double rollAcc = 0; int64_t rollN = 0, rollSkip = 0;

    std::vector<float> bl, br;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrpheusProcessor)
};
} // namespace orph
