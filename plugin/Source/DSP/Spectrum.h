#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>

namespace obs::dsp
{
/** The EQ page's analyzer. The audio thread keeps the latest samples after the EQ (only while the page
    is open); the message thread turns them into a level per point across the EQ's frequency axis. */
class SpectrumTap
{
public:
    static constexpr int kOrder = 12, kSize = 1 << kOrder;   // 4096 samples: ~85 ms at 48 kHz
    static constexpr int kPoints = 121;                      // f = 31.25 Hz * 2^(10 k / 120): the EQ graph's x axis
    static constexpr float kFloorDb = -120.f;

    SpectrumTap()
    {
        for (auto& s : ring) s.store (0.f, std::memory_order_relaxed);
        for (int i = 0; i < kSize; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) kSize);
        smooth.fill (kFloorDb);
    }

    /** Message thread: the EQ page opened or closed. */
    void setWanted (bool on) noexcept
    {
        if (on && ! wanted.load()) { for (auto& s : ring) s.store (0.f, std::memory_order_relaxed); smooth.fill (kFloorDb); }
        wanted.store (on);
    }
    bool isWanted() const noexcept { return wanted.load (std::memory_order_relaxed); }

    /** Audio thread: the stereo output of the EQ section. */
    void push (const float* L, const float* R, int n) noexcept
    {
        if (! wanted.load (std::memory_order_relaxed)) return;
        int w = write.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            ring[(size_t) w].store (0.5f * (L[i] + R[i]), std::memory_order_relaxed);
            w = (w + 1) & (kSize - 1);
        }
        write.store (w, std::memory_order_release);
    }

    /** Message thread: dB per point, rising at once and falling 48 dB a second, so it settles when you stop.
        A +3 dB/octave tilt (0 dB at 1 kHz) makes a guitar read level instead of sloping away. */
    const std::array<float, kPoints>& analyse (double sampleRate, double secondsSinceLast)
    {
        if (sampleRate <= 0) return smooth;
        const int w = write.load (std::memory_order_acquire);
        for (int i = 0; i < kSize; ++i)
            fftData[(size_t) i] = ring[(size_t) ((w + i) & (kSize - 1))].load (std::memory_order_relaxed) * window[(size_t) i];
        std::fill (fftData.begin() + kSize, fftData.end(), 0.f);
        fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

        const double binHz = sampleRate / kSize, half = std::pow (2.0, 5.0 / (kPoints - 1));
        const float norm = 4.f / (float) kSize;                 // Hann window: a full-scale sine reads 0 dB
        const float fall = (float) (48.0 * juce::jlimit (0.0, 0.5, secondsSinceLast));
        for (int k = 0; k < kPoints; ++k)
        {
            const double f = 31.25 * std::pow (2.0, 10.0 * k / (kPoints - 1));
            float db = kFloorDb;
            if (f < sampleRate * 0.49)
            {
                const int b0 = juce::jmax (1, (int) std::ceil (f / half / binHz)), b1 = juce::jmin (kSize / 2 - 1, (int) std::floor (f * half / binHz));
                float mag = 0.f;
                if (b1 >= b0) for (int b = b0; b <= b1; ++b) mag = std::max (mag, fftData[(size_t) b]);   // wide: loudest bin
                else                                                                                     // narrow: between two bins
                {
                    const double x = f / binHz; const int b = juce::jlimit (0, kSize / 2 - 2, (int) x); const float t = (float) (x - b);
                    mag = fftData[(size_t) b] * (1.f - t) + fftData[(size_t) b + 1] * t;
                }
                db = juce::Decibels::gainToDecibels (mag * norm, kFloorDb) + 3.f * (float) std::log2 (f / 1000.0);
            }
            auto& s = smooth[(size_t) k];
            s = db > s ? db : std::max (db, s - fall);
        }
        return smooth;
    }

private:
    std::atomic<bool> wanted { false };
    std::atomic<int> write { 0 };
    std::array<std::atomic<float>, kSize> ring;
    std::array<float, kSize> window {};
    std::array<float, kSize * 2> fftData {};
    std::array<float, kPoints> smooth {};
    juce::dsp::FFT fft { kOrder };
};
} // namespace obs::dsp
