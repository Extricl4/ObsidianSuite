#pragma once
// =============================================================================
//  Engine: the whole Obsidian signal chain, independent of the plugin wrapper
//  (so the self-test can run it without a host).
//
//   input -> gain -> gate -> transpose -> [synth pre] -> PEDALS (mono)
//         -> AMP (mono) -> CAB (mono -> stereo) -> [synth post] -> [AETHER] -> EQ
//         -> FX -> SPECIAL FX -> doubler -> width -> output -> metronome
// =============================================================================
#include "../Core/Params.h"
#include "Sections.h"
#include "PitchTracker.h"
#include "GuitarSynth.h"
#include "Atmos.h"
#include "Harmonizer.h"
#include <array>
#include "AmpSection.h"
#include "CabSection.h"
#include "Modules/ModuleRegistry.h"
#include "Spectrum.h"

namespace obs
{
struct HostInfo
{
    double bpm = 120; bool playing = false; double ppq = 0; bool hasHostTempo = false;
};

class Studio;          // Core/Studio.h: the standalone app's recorder, fed from the end of the chain

class Engine
{
public:
    Engine();
    void attach (juce::AudioProcessorValueTreeState& state);
    void prepare (double sampleRate, int maxBlock);
    void release();
    /** buffer: in/out, numIn = how many channels hold input. Output is always stereo (ch 0/1). */
    /** numOut: the output bus's channels (a stereo-in / mono-out layout still has a 2-channel buffer). */
    void process (juce::AudioBuffer<float>& buffer, int numIn, const HostInfo& host, int numOut = 2);

    // ---- message-thread controls (non-automatable state) ---------------------------
    dsp::AmpSection& amp()                        { return ampSection; }
    dsp::CabSection& cab()                        { return cabSection; }
    dsp::SlotRack& rack (int r)                   { return racks[juce::jlimit (0, kNumRacks - 1, r)]; }
    const dsp::PitchTracker& tracker() const      { return pitch; }
    dsp::ModuleContext moduleContext() const      { return ctxProto; }
    void collectGarbage (bool force);
    int latencySamples();

    // Settings that aren't presets (set from Settings / bridge).
    std::atomic<int>   inputMode { 0 };           // 0 = sum of inputs, 1 = input 1 only, 2 = input 2 only
    std::atomic<float> inputCalibrationDb { 0.f };
    std::atomic<bool>  tunerOpen { false };
    std::atomic<bool>  muteWhileTuning { true };
    std::atomic<int>   oversampling { 2 };
    std::atomic<double> internalBpm { 120.0 };
    std::atomic<int>   beatsPerBar { 4 };
    std::atomic<bool>  accentFirst { true };
    std::atomic<float> metronomeVolume { 0.7f };
    void setMetronomeRunning (bool r)             { metroRunning.store (r); }
    /** STUDIO (standalone only): records the finished sound and plays its takes back in. */
    void setStudio (Studio* s)                    { studio.store (s); }
    bool metronomeRunning() const                 { return metroRunning.load(); }
    int  metronomeBeat() const                    { return metronome.currentBeat(); }
    double effectiveBpm() const                   { return lastBpm.load(); }

    // Meters / analysis for the UI.
    float inputPeak() const                       { return inMeter.read(); }
    float outputPeakL() const                     { return outMeterL.read(); }
    /** The EQ page's analyzer (what comes out of the EQ). */
    dsp::SpectrumTap& eqSpectrum()                { return spectrum; }
    float outputPeakR() const                     { return outMeterR.read(); }
    float gateGain() const                        { return gate.currentGain(); }
    float synthActivity() const                   { return synth.currentActivity(); }
    float synthNote() const                       { return synth.currentNote(); }
    int   synthStep() const                       { return synth.currentStep(); }
    int   synthSource() const                     { return synth.currentSource(); }
    /** AETHER, for its page: how busy each layer is (0 bed, 1 drone, 2 texture) and a count of texture events. */
    float atmosActivity (int layer) const          { return atmosFade.active() ? atmos.activity (layer) : 0.f; }
    int   atmosEvents() const                     { return atmos.events(); }
    float atmosRoot() const                       { return atmosFade.active() ? atmos.droneRoot() : -1.f; }
    /** HYDRA, for the HARMONY page: the note it hears (-1: none), in CHORDS the chord's notes. */
    int   harmonyNote() const                     { return harmonizer.uiNote.load(); }
    juce::Array<juce::var> harmonyChord() const
    {
        juce::Array<juce::var> a; const int n = harmonizer.uiChordCount.load();
        for (int i = 0; i < n && i < dsp::hydra::ChordDetector::kMaxNotes; ++i) a.add (harmonizer.uiChord[i].load());
        return a;
    }

    /** Arpeggiator steps (16 packed numbers, see SynthDefs.h). Any thread. */
    void setSynthSteps (const unsigned* s)         { for (int i = 0; i < 16; ++i) synthSteps[i].store (s[i]); }
    /** Audio thread, before process(): a MIDI note for the synth (vel 0 = off). */
    void addSynthNote (int offset, int note, float vel)
    {
        if (numSynthNotes < kMaxSynthNotes) synthNotes[numSynthNotes++] = { offset, note, vel };
    }
    /** Message thread: the UI's AUDITION button / computer keys. */
    void uiSynthNote (int note, bool on)
    {
        int s1, n1, s2, n2; uiNotes.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 > 0) { uiNoteBuf[(size_t) s1] = on ? note : -1 - note; uiNotes.finishedWrite (1); }
    }
    float cpuLoad() const                         { return cpu.load(); }
    bool  takeClip()                              { return outMeterL.takeClip() | outMeterR.takeClip(); }
    double currentSampleRate() const              { return sr; }
    int    currentBlockSize() const               { return maxBlock; }

private:
    void readSynthParams (dsp::SynthParams& sp, const HostInfo& h);
    void readAtmosParams (dsp::AtmosParams& ap);

    double sr = 48000; int maxBlock = 512; bool prepared = false;
    ParamCache params;
    dsp::NoiseGate gate;
    dsp::Transposer transposer;
    dsp::PitchTracker pitch;
    dsp::GuitarSynth synth;
    dsp::Atmos atmos;
    dsp::AmpSection ampSection;
    dsp::CabSection cabSection;
    dsp::GraphicEq eq;
    dsp::SlotRack racks[kNumRacks];
    dsp::Doubler doubler;
    dsp::StereoWidth width;
    dsp::Metronome metronome;
    std::atomic<Studio*> studio { nullptr };
    dsp::PeakMeter inMeter, outMeterL, outMeterR;
    dsp::SpectrumTap spectrum;
    dsp::Smoother inGainSm, outGainSm, tunerMute;
    dsp::SectionFade cabFade, eqFade, synthFade, atmosFade;   // click-free on / off of those sections
    bool cabWasActive = true, eqWasActive = true, atmosWasActive = false;
    dsp::ModuleContext ctxProto;
    std::atomic<bool> metroRunning { false };
    std::atomic<double> lastBpm { 120.0 };
    std::atomic<float> cpu { 0.f };

    std::vector<float> mono, synthL, synthR, scratchR, dryL, dryR, synthG, atmL, atmR, atmG;
    std::atomic<unsigned> synthSteps[16];
    static constexpr int kMaxSynthNotes = 256;
    dsp::SynthNote synthNotes[kMaxSynthNotes]; int numSynthNotes = 0;
    juce::AbstractFifo uiNotes { 64 }; std::array<int, 64> uiNoteBuf {};

    // Cached parameter pointers (read every block, lock-free).
    struct P
    {
        std::atomic<float>* transposeHq, *inGain, *gateOn, *gateThresh, *transpose, *doublerOn, *doublerTime, *width, *outGain;
        std::atomic<float>* preOn, *synthOn, *ampOn, *cabOn, *eqOn, *postOn, *sfxOn;
        std::atomic<float>* ampGain, *ampBass, *ampMid, *ampTreble, *ampPresence, *ampDepth, *ampMaster, *ampChannel;
        std::atomic<float>* eqBand[kEqBands], *eqLc, *eqHc, *eqLcOn, *eqHcOn;
        struct Mic { std::atomic<float>* model, *pos, *dist, *level, *pan, *solo, *mute, *phase; } mic[2];
        std::atomic<float>* cabLc, *cabHc;
        // synth: every parameter, by SynthDefs index
        std::atomic<float>* syn[obs::synthdefs::count];
        std::atomic<float>* atm[obs::atmosdefs::count];   // AETHER, by AtmosDefs index
        // HYDRA: by HarmonyDefs index
        std::atomic<float>* harm[obs::harmdefs::gCount];
        std::atomic<float>* harmV[obs::harmdefs::kVoices][obs::harmdefs::vCount];
    } p {};
    void readHarmonyParams (dsp::hydra::Params& hp);
    dsp::hydra::Harmonizer harmonizer;
    std::vector<float> di;
    int harmPlace = 0; bool harmMoving = false;
};
} // namespace obs
