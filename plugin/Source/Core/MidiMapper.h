#pragma once
// =============================================================================
//  MidiMapper: MIDI control of any knob or switch (CC), MIDI learn, and program
//  change -> preset. The audio thread only reads an immutable mapping table and
//  pushes events into a lock-free FIFO; parameters are changed on the message
//  thread (so hosts are never notified from the audio thread).
// =============================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "../DSP/NamEngine.h"   // RtSlot

namespace obs
{
class MidiMapper
{
public:
    struct Mapping
    {
        int cc = 0;               // controller number
        int channel = 0;          // 0 = any
        juce::String paramId;
        int paramIndex = -1;
        float lo = 0.f, hi = 1.f; // normalised range the CC sweeps
        bool toggle = false;      // CC >= 64 on, < 64 off
    };

    MidiMapper()                                   { install (defaultMappings()); }

    // ---- message thread --------------------------------------------------------------
    void fromJson (const juce::var& v);
    juce::var toJson() const;
    void startLearn (const juce::String& paramId)  { learnTarget = paramId; learning.store (true); }
    void cancelLearn()                             { learning.store (false); learnTarget = {}; }
    bool isLearning() const                        { return learning.load(); }
    void remove (int index);
    void clearAll()                                { install ({}); }
    std::vector<Mapping> current() const           { const juce::ScopedLock sl (lock); return mappings; }

    /** Drains events from the audio thread. Returns a description when a learn completed. */
    juce::String dispatch (juce::AudioProcessorValueTreeState& state, const std::function<void (int)>& onProgramChange);

    // ---- audio thread ------------------------------------------------------------------
    void process (const juce::MidiBuffer& midi, int channelFilter, bool programChangeEnabled);
    void blockDone()                               { table.blockDone(); }
    void collectGarbage()                          { table.collectGarbage (false); }

    static std::vector<Mapping> defaultMappings();

private:
    void install (std::vector<Mapping> m);
    struct Event { int type; int a; float b; int c; };   // type 0 = param (a = index, b = norm), 1 = program change (a), 2 = learn (a = cc, c = channel)
    void push (const Event& e);

    std::vector<Mapping> mappings;
    juce::CriticalSection lock;
    dsp::RtSlot<std::vector<Mapping>> table;
    juce::AbstractFifo fifo { 512 };
    Event events[512] {};
    std::atomic<bool> learning { false };
    juce::String learnTarget;
};
} // namespace obs
