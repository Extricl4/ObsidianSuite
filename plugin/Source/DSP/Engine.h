#pragma once
// =============================================================================
//  Engine: the whole Obsidian signal chain, independent of the plugin wrapper
//  (so the self-test can run it without a host).
//
//   input -> gain -> gate -> transpose -> [synth pre] -> PEDALS (mono)
//         -> AMP (mono) -> CAB (mono -> stereo) -> [synth post] -> EQ
//         -> FX -> SPECIAL FX -> doubler -> width -> output -> metronome
// =============================================================================
#include "../Core/Params.h"
#include "Sections.h"
#include "PitchTracker.h"
#include "GuitarSynth.h"
#include "AmpSection.h"
#include "CabSection.h"
#include "Modules/ModuleRegistry.h"

namespace obs
{
struct HostInfo
{
    double bpm = 120; bool playing = false; double ppq = 0; bool hasHostTempo = false;
};

class Engine
{
public:
    Engine();
    void attach (juce::AudioProcessorValueTreeState& state);
    void prepare (double sampleRate, int maxBlock);
    void release();
    /** buffer: in/out, numIn = how many channels hold input. Output is always stereo (ch 0/1). */
    void process (juce::AudioBuffer<float>& buffer, int numIn, const HostInfo& host);

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
    bool metronomeRunning() const                 { return metroRunning.load(); }
    int  metronomeBeat() const                    { return metronome.currentBeat(); }
    double effectiveBpm() const                   { return lastBpm.load(); }

    // Meters / analysis for the UI.
    float inputPeak() const                       { return inMeter.read(); }
    float outputPeakL() const                     { return outMeterL.read(); }
    float outputPeakR() const                     { return outMeterR.read(); }
    float gateGain() const                        { return gate.currentGain(); }
    float synthActivity() const                   { return synth.currentActivity(); }
    float cpuLoad() const                         { return cpu.load(); }
    bool  takeClip()                              { return outMeterL.takeClip() | outMeterR.takeClip(); }
    double currentSampleRate() const              { return sr; }
    int    currentBlockSize() const               { return maxBlock; }

private:
    void readSynthParams (dsp::SynthParams& sp, const HostInfo& h);

    double sr = 48000; int maxBlock = 512; bool prepared = false;
    ParamCache params;
    dsp::NoiseGate gate;
    dsp::Transposer transposer;
    dsp::PitchTracker pitch;
    dsp::GuitarSynth synth;
    dsp::AmpSection ampSection;
    dsp::CabSection cabSection;
    dsp::GraphicEq eq;
    dsp::SlotRack racks[kNumRacks];
    dsp::Doubler doubler;
    dsp::StereoWidth width;
    dsp::Metronome metronome;
    dsp::PeakMeter inMeter, outMeterL, outMeterR;
    dsp::Smoother inGainSm, outGainSm, tunerMute;
    dsp::ModuleContext ctxProto;
    std::atomic<bool> metroRunning { false };
    std::atomic<double> lastBpm { 120.0 };
    std::atomic<float> cpu { 0.f };

    std::vector<float> mono, synthL, synthR, scratchR;

    // Cached parameter pointers (read every block, lock-free).
    struct P
    {
        std::atomic<float>* transposeHq, *inGain, *gateOn, *gateThresh, *transpose, *doublerOn, *doublerTime, *width, *outGain;
        std::atomic<float>* preOn, *synthOn, *ampOn, *cabOn, *eqOn, *postOn, *sfxOn;
        std::atomic<float>* ampGain, *ampBass, *ampMid, *ampTreble, *ampPresence, *ampDepth, *ampMaster, *ampChannel;
        std::atomic<float>* eqBand[kEqBands], *eqLc, *eqHc;
        struct Mic { std::atomic<float>* model, *pos, *dist, *level, *pan, *solo, *mute, *phase; } mic[2];
        std::atomic<float>* cabLc, *cabHc;
        // synth
        std::atomic<float>* sGate, *sSens, *sGlide, *sMix, *sOut, *sDecay, *sSustain;
        struct Osc { std::atomic<float>* on, *wave, *semi, *det, *level, *uWidth, *uDet; } osc[2];
        std::atomic<float>* fOn, *fCut, *fRes, *fAtt, *fDrive, *fSlope, *fEnv, *retrig, *post;
        std::atomic<float>* arpOn, *arpStep[8], *arpKey, *arpScale, *arpOct, *arpPattern, *arpRate, *arpSync, *tune;
    } p {};
};
} // namespace obs
