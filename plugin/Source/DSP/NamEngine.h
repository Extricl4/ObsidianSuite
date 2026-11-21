#pragma once
// =============================================================================
//  NamEngine: one loaded .nam capture, ready for the audio thread.
//   * loads on a background thread (never on the audio thread)
//   * resamples automatically when the host rate differs from the capture's
//     training rate (usually 48 kHz)
//   * normalises loudness so switching captures doesn't blast you
//  Ownership: built off-thread, handed to the audio thread through a lock-free
//  slot (see RtSlot below), deleted later on the message thread.
// =============================================================================
#include <juce_core/juce_core.h>
#include <memory>
#include <atomic>
#include <vector>

namespace nam { class DSP; }

namespace obs::dsp
{
class NamEngine
{
public:
    ~NamEngine();

    /** Loads a capture. Returns nullptr and fills `error` on failure (never throws). */
    static std::unique_ptr<NamEngine> load (const juce::File& file, double hostRate, int maxBlock, juce::String& error);

    /** Re-prepare for a new host sample rate / block size (call off the audio thread). */
    bool prepare (double hostRate, int maxBlock);

    /** Mono in-place processing. */
    void process (float* io, int n);

    juce::String name() const             { return displayName; }
    juce::File file() const               { return source; }
    double trainedRate() const            { return modelRate; }
    int latencySamples() const;
    float loudnessCompensationDb() const  { return loudnessDb; }

private:
    NamEngine() = default;
    struct Impl;
    std::unique_ptr<Impl> impl;
    juce::File source;
    juce::String displayName;
    double modelRate = 48000.0, hostRate = 48000.0;
    float loudnessDb = 0.f;
};

/** Lock-free hand-over of heap objects to the audio thread with deferred deletion.
    The audio thread calls `acquire()` at the start of a block and `release()` at the end;
    retired objects are deleted by `collectGarbage()` on the message thread once the audio
    thread has finished at least one more block. */
template <typename T>
class RtSlot
{
public:
    ~RtSlot()                              { delete current.load(); for (auto& r : retired) delete r.obj; }

    /** Message thread: install a new object (may be nullptr). The old one is retired. */
    void set (std::unique_ptr<T> obj)
    {
        T* old = current.exchange (obj.release(), std::memory_order_acq_rel);
        if (old != nullptr) { const juce::ScopedLock sl (lock); retired.push_back ({ old, epoch.load() }); }
    }
    /** Audio thread. */
    T* get() const                         { return current.load (std::memory_order_acquire); }
    void blockDone()                       { epoch.fetch_add (1, std::memory_order_acq_rel); }
    /** Message thread, call periodically (or with force=true when audio is stopped). */
    void collectGarbage (bool force = false)
    {
        const juce::ScopedLock sl (lock);
        const auto now = epoch.load();
        for (auto it = retired.begin(); it != retired.end();)
        {
            if (force || now >= it->epoch + 2) { delete it->obj; it = retired.erase (it); }
            else ++it;
        }
    }
private:
    struct Retired { T* obj; uint64_t epoch; };
    std::atomic<T*> current { nullptr };
    std::atomic<uint64_t> epoch { 0 };
    std::vector<Retired> retired;
    juce::CriticalSection lock;
};
} // namespace obs::dsp
