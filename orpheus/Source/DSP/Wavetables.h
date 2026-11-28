#pragma once
/*  ORPHEUS · SIREN's waveforms as band-limited wavetables.
    The same shapes as the UI's waveAt() (saw with an offset saw, pulse width, folding triangle, sine with feedback FM),
    taken apart into 63 harmonics like the preview's PeriodicWave, normalised to a peak of 1, and stored in octave steps
    with fewer harmonics for higher notes, so nothing aliases. SHAPE is in 5% steps (21 tables per waveform).       */
#include <vector>
#include <array>
#include <atomic>

namespace orph
{
class Wavetables
{
public:
    static constexpr int N = 2048, LEVELS = 8, SHAPES = 21, WAVES = 4;   // saw, pulse, triangle, sine (SUPER = 7 saws)
    /** call once (not on the audio thread): about 20 ms */
    static void build();
    struct Table { const float* level[LEVELS]; };
    /** the table for a waveform (0..3) and shape (0..100) */
    static const Table& get (int wave, float shape) noexcept;
    /** which level to use for a fundamental of f Hz at sample rate sr */
    static int levelFor (double f, double sr) noexcept;
    /** read at phase 0..1 (linear interpolation) */
    static inline float read (const float* t, double ph) noexcept
    {
        const double p = ph * N; const int i = (int) p; const float fr = (float) (p - i);
        const float a = t[i & (N - 1)], b = t[(i + 1) & (N - 1)];
        return a + (b - a) * fr;
    }
    static float waveAt (int w, double ph, double shape);              // the naive shape (as in the UI)
private:
    static std::vector<float> data;
    static std::array<Table, WAVES * SHAPES> tables;
    static std::atomic<bool> ready;
};
} // namespace orph
