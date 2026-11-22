#pragma once
// =============================================================================
//  STUDIO: the standalone app's little recorder (the STUDIO page).
//
//  It records what you hear: the finished sound at the end of the chain, before
//  the click and before the studio's own playback. Takes live in memory for
//  playback and are also saved as WAV files in Documents\Obsidian Amp Suite\
//  Recordings. The page owns the arrangement (tracks, takes on the timeline, loop,
//  click) and sends it whenever it changes; this class plays it back with each
//  track's volume / pan / mute / solo, clicks on the beat, and mixes it down to a
//  WAV (EXPORT).
//
//  Threads: process() runs on the audio thread and never allocates or waits.
//  Recording goes through a lock-free FIFO that a message-thread timer empties
//  into the take (and turns into waveform peaks for the page). The arrangement is
//  swapped under a spin lock the audio thread only ever tries.
// =============================================================================
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <map>
#include <memory>
#include <vector>

namespace obs
{
class Studio : private juce::Timer
{
public:
    struct Take { int id = 0; std::vector<float> L, R; double rate = 48000; double bpm = 120; juce::String name; juce::File file; };

    Studio();
    ~Studio() override;

    // ---- audio thread --------------------------------------------------------------------
    void prepare (double sampleRate);
    /** The finished stereo sound: recorded (when recording), then the takes and the click are
        mixed in. n is any block size. */
    void process (float* L, float* R, int n) noexcept;

    // ---- message thread ------------------------------------------------------------------
    /** From the page, in beats: { bpm, tracks: [{vol, pan, on}], clips: [{src, track, start, len, off}],
        loop: {on, s, e}, click }. vol 0..1 (0.8 = 0 dB), pan -1..1, on = heard (mute / solo). */
    void setArrangement (const juce::var& a);
    void play (double fromBeat);
    /** Records onto a track from a beat, after countIn beats of click. */
    void record (int track, double fromBeat, double countInBeats);
    void stop();
    /** For the page, every UI tick: { active, playing, recording, pos (beats), peaks: [new 0..1],
        take: {id, track, start, len, peaks, name, file} once a take is done }. */
    juce::var status();
    /** Mixes the arrangement down to a stereo 24-bit WAV. */
    bool exportMix (const juce::File& dest, juce::String& error);
    /** Where takes are saved. */
    static juce::File recordingsFolder();
    int takeCount() const                       { return (int) takes.size(); }
    bool saveTakes = true;                      // write each take to Recordings (off for tests)
    /** Round-trip latency (interface in + out + the plugin's own), in samples: a take is moved that
        much earlier, so what you played lines up with what you heard. Set before record(). */
    void setRoundTrip (int samples)             { roundTrip = juce::jmax (0, samples); }
    std::shared_ptr<const Take> take (int id) const;
    /** For tests: put audio in as a take (as if it was recorded). */
    int addTake (std::vector<float> L, std::vector<float> R, const juce::String& name);
    /** Collects what's recorded now and finishes a stopped take (the timer does this; for tests). */
    void flush()                                { drain(); if (finishing) finishTake(); }
    /** Display curve for peaks: -48 dB..0 dB -> 0..1. */
    static float peakShape (float peak);

private:
    struct Clip { std::shared_ptr<const Take> src; int track = 0; juce::int64 start = 0, len = 0, off = 0; };
    struct Arr
    {
        double bpm = 120;
        std::vector<float> gL, gR;            // per track (0 when muted / not soloed)
        std::vector<char> mono;               // per track: mono tracks play L+R in the middle
        std::vector<Clip> clips;
        bool loop = false; juce::int64 loopS = 0, loopE = 0;
        bool click = false;
    };
    void timerCallback() override;
    void drain();
    void finishTake();
    std::unique_ptr<Arr> build (const juce::var& a) const;
    double spb() const                          { return sr * 60.0 / juce::jmax (20.0, bpm.load()); }
    void writeWav (std::shared_ptr<const Take> t);

    double sr = 48000;
    std::atomic<double> bpm { 120.0 };
    juce::var lastArr;
    juce::SpinLock arrLock;
    std::unique_ptr<Arr> arr;

    std::atomic<bool> playing { false }, recording { false };
    std::atomic<juce::int64> pos { 0 }, recStart { 0 };
    std::atomic<int> recTrack { 0 };
    std::atomic<int> dropped { 0 };
    std::atomic<int> seekGen { 0 };             // bumped on every jump: the click starts over
    int seenGen = -1;
    double clickPhase = 0; int clickLeft = 0; float clickHz = 1000.f; juce::int64 lastBeatIdx = -1;

    // recording: audio thread -> FIFO -> timer -> take
    juce::AbstractFifo fifo { 1 };
    std::vector<float> fifoL, fifoR;
    std::unique_ptr<Take> current;              // the take being recorded
    int currentTrack = 0; double currentStartBeat = 0;
    juce::int64 peakEvery = 3000, peakCount = 0; float peakMax = 0;
    std::vector<float> currentPeaks, newPeaks;
    bool finishing = false; int finishTicks = 0;
    int roundTrip = 0;
    juce::var doneTake;                         // the last finished take, for status() once

    std::map<int, std::shared_ptr<const Take>> takes;
    int nextId = 1;
    juce::CriticalSection takesLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Studio)
};
} // namespace obs
