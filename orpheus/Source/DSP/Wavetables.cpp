#include "Wavetables.h"
#include "Prims.h"
#include <mutex>

namespace orph
{
std::vector<float> Wavetables::data;
std::array<Wavetables::Table, Wavetables::WAVES * Wavetables::SHAPES> Wavetables::tables;
std::atomic<bool> Wavetables::ready { false };

static inline double frac (double x) { return x - std::floor (x); }

float Wavetables::waveAt (int w, double ph, double shape)
{
    const double s = shape / 100.0;
    switch (w)
    {
        case 0: { const double a = 1 - 2 * ph, b = 1 - 2 * frac (ph + .5 * s); return (float) ((1 - .5 * s) * a + .5 * s * b); }
        case 1: { const double pw = .5 - .45 * s; return ph < pw ? 1.f : -1.f; }
        case 2: { const double t = ph < .5 ? 4 * ph - 1 : 3 - 4 * ph; return (float) std::sin (kPi / 2 * t * (1 + 3 * s)); }
        default: { double y = 0; for (int i = 0; i < 3; ++i) y = std::sin (kTau * ph + s * 2.2 * y); return (float) y; }
    }
}

void Wavetables::build()
{
    static std::mutex m; std::lock_guard<std::mutex> lock (m);
    if (ready.load()) return;
    constexpr int H = 64, NS = 1024;
    data.assign ((size_t) WAVES * SHAPES * LEVELS * N, 0.f);
    std::vector<double> x ((size_t) NS), re (H), im (H), ct ((size_t) N), st ((size_t) N);
    for (int i = 0; i < N; ++i) { ct[(size_t) i] = std::cos (kTau * i / N); st[(size_t) i] = std::sin (kTau * i / N); }   // cos(2 pi j / N) by index
    // harmonics allowed per level: 63, 32, 16, 8, 4, 2, 1, 1 (level k is used when the fundamental is in octave k above ~350 Hz / sr)
    const int maxH[LEVELS] = { 63, 32, 16, 8, 4, 2, 1, 1 };
    for (int w = 0; w < WAVES; ++w)
        for (int si = 0; si < SHAPES; ++si)
        {
            const double shape = si * 5.0;
            for (int i = 0; i < NS; ++i) x[(size_t) i] = waveAt (w, (double) i / NS, shape);
            for (int k = 1; k < H; ++k)
            {
                double a = 0, b = 0;
                for (int i = 0; i < NS; ++i) { const size_t j = (size_t) ((2 * k * i) & (N - 1)); a += x[(size_t) i] * ct[j]; b += x[(size_t) i] * st[j]; }
                re[(size_t) k] = a / NS * 2; im[(size_t) k] = b / NS * 2;
            }
            // normalise like a PeriodicWave: the full table's peak is 1 (the same gain for every level, so levels match)
            std::vector<double> full ((size_t) N);
            double peak = 1e-9;
            for (int i = 0; i < N; ++i)
            {
                double y = 0;
                for (int k = 1; k < H; ++k) { const size_t j = (size_t) ((k * i) & (N - 1)); y += re[(size_t) k] * ct[j] + im[(size_t) k] * st[j]; }
                full[(size_t) i] = y; peak = std::max (peak, std::abs (y));
            }
            Table& tb = tables[(size_t) (w * SHAPES + si)];
            for (int L = 0; L < LEVELS; ++L)
            {
                float* dst = data.data() + ((size_t) ((w * SHAPES + si) * LEVELS + L)) * N;
                for (int i = 0; i < N; ++i)
                {
                    double y = 0;
                    for (int k = 1; k <= maxH[L] && k < H; ++k) { const size_t j = (size_t) ((k * i) & (N - 1)); y += re[(size_t) k] * ct[j] + im[(size_t) k] * st[j]; }
                    dst[i] = (float) (y / peak);
                }
                tb.level[L] = dst;
            }
        }
    ready.store (true);
}

const Wavetables::Table& Wavetables::get (int wave, float shape) noexcept
{
    wave = std::max (0, std::min (WAVES - 1, wave));
    const int si = std::max (0, std::min (SHAPES - 1, (int) std::lround (shape / 5.0f)));
    return tables[(size_t) (wave * SHAPES + si)];
}

int Wavetables::levelFor (double f, double sr) noexcept
{
    // the level whose highest harmonic stays under ~0.47 sr
    const int maxH[LEVELS] = { 63, 32, 16, 8, 4, 2, 1, 1 };
    const double lim = 0.47 * sr;
    for (int L = 0; L < LEVELS; ++L) if (maxH[L] * f <= lim) return L;
    return LEVELS - 1;
}
} // namespace orph
