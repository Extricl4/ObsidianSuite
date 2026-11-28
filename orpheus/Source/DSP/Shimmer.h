#pragma once
/*  ORPHEUS · SIREN's SHIMMER, the C++ copy of ui/src/fx-worklet.js (the preview's AudioWorklet), sample for sample:
    pre-delay -> four allpass diffusers -> an 8-line feedback delay network (moving lengths, Householder mix, damping),
    and the shimmer: the network's own tail pitched up an octave (and a fifth in + FIFTHS) by a granular shifter with
    four jittered grains, fed back in, so every pass climbs. Stereo out; the input is summed to mono.               */
#include "Prims.h"
#include <array>

namespace orph
{
class Shimmer
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        auto ms = [this] (double x) { return x * sr / 1000.0; };
        preD = (int) std::lround (ms (28));
        const double apMs[4] = { 4.77, 3.59, 12.73, 9.31 };
        for (int k = 0; k < 4; ++k) { ap[k].b.assign ((size_t) std::max (1, (int) std::lround (ms (apMs[k]))), 0.f); ap[k].i = 0; ap[k].g = k < 2 ? .72f : .62f; }
        const double L[8] = { 37.1, 41.9, 48.7, 53.3, 61.1, 67.3, 73.9, 83.3 }, R[8] = { .071, .093, .117, .131, .157, .183, .211, .239 };
        int need = 1; while (need < (int) ms (83.3 * 1.35 + 4) + 8) need <<= 1;
        M = need - 1;
        for (int i = 0; i < 8; ++i)
        {
            len[i] = ms (L[i] * 1.35); dep[i] = ms (.28 + .09 * i); inc[i] = kTau * R[i] / sr; ph[i] = i * 1.3;
            buf[i].assign ((size_t) need, 0.f);
        }
        int sn = 1; while (sn < (int) (sr * 1.3)) sn <<= 1;          // 65536 at 48 kHz
        sb.assign ((size_t) sn, 0.f); SM = sn - 1;
        pre.assign (8192, 0.f);
        up12.init (sr, 2.0, .095, rng); up7.init (sr, 1.5, .13, rng);
        ah = 1 - std::exp (-kTau * 22 / sr); a = 1 - std::exp (-kTau * 7000 / sr);
        aSh = 1 - std::exp (-kTau * 5500 / sr); ahSh = 1 - std::exp (-kTau * 380 / sr);
        decay = -1; reset();
    }
    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.f);
        std::fill (sb.begin(), sb.end(), 0.f); std::fill (pre.begin(), pre.end(), 0.f);
        for (auto& x : ap) std::fill (x.b.begin(), x.b.end(), 0.f);
        lp.fill (0); hp.fill (0); shLp = shHp = 0; quiet = 0; idle = true;
    }
    /** decay: seconds to -60 dB; rise 0..1; fifth 0 / 1 */
    void setParams (double decaySec, double rise01, double fifth01) noexcept { decayT = decaySec; riseT = rise01; f5T = fifth01; }

    /** in: stereo input (summed to mono inside); adds nothing: writes the stereo output */
    void process (const float* iL, const float* iR, float* oL, float* oR, int n) noexcept
    {
        bool live = false;
        for (int t = 0; t < n; t += 4) if (iL[t] != 0 || iR[t] != 0) { live = true; break; }
        if (! live && idle) { std::fill (oL, oL + n, 0.f); std::fill (oR, oR + n, 0.f); return; }
        if (live) { idle = false; quiet = 0; }
        if (decayT != decay)
        {
            decay = decayT; double gm = 0;
            for (int i = 0; i < 8; ++i) { g[i] = std::pow (10.0, -3 * len[i] / (sr * decay)); gm += g[i] / 8; }
            const double gr = std::pow (10.0, -3 * .0787 / 4);
            norm = std::sqrt (std::max (.03, 1 - gm * gm) / (1 - gr * gr));
        }
        const int PM = (int) pre.size() - 1;
        for (int t = 0; t < n; ++t)
        {
            rise += (riseT - rise) * .0005; f5 += (f5T - f5) * .0005;
            double x = (iL[t] + iR[t]) * .5;
            pre[(size_t) preW] = (float) x; x = pre[(size_t) ((preW - preD) & PM)]; preW = (preW + 1) & PM;
            for (auto& A : ap)
            {
                const double d = A.b[(size_t) A.i], y = d - A.g * x;
                A.b[(size_t) A.i] = (float) (x + A.g * y); if (++A.i == (int) A.b.size()) A.i = 0; x = y;
            }
            double s = 0;
            for (int i = 0; i < 8; ++i)
            {
                ph[i] += inc[i]; if (ph[i] > kTau) ph[i] -= kTau;
                const double pos = w - len[i] - dep[i] * std::sin (ph[i]); const int j = (int) std::floor (pos); const double f = pos - j;
                const auto& b = buf[i];
                const double xm = b[(size_t) ((j - 1) & M)], x0 = b[(size_t) (j & M)], x1 = b[(size_t) ((j + 1) & M)], x2 = b[(size_t) ((j + 2) & M)];
                double y = ((((.5 * (x2 - xm) + 1.5 * (x0 - x1)) * f + (xm - 2.5 * x0 + 2 * x1 - .5 * x2)) * f + .5 * (x1 - xm)) * f + x0);
                lp[i] += (y - lp[i]) * a; y = lp[i];
                hp[i] += (y - hp[i]) * ah; y -= hp[i];
                v[i] = y; s += y;
            }
            double sh = 0;
            if (rise > 1e-4)
            {
                sb[(size_t) sw] = (float) (s * .35);
                const double u = up12.read (sb, SM, sw, rng), q = f5 > 1e-3 ? up7.read (sb, SM, sw, rng) : 0.0;
                sh = u * (1 - .35 * f5) + q * .7 * f5;
                shLp += (sh - shLp) * aSh; shHp += (shLp - shHp) * ahSh; sh = shLp - shHp;
                sh *= rise * 1.5; sh = sh / (1 + std::abs (sh) * .8);
            }
            else sb[(size_t) sw] = 0;
            sw = (sw + 1) & SM;
            const double hs = s * .25;
            double l = 0, r = 0;
            for (int i = 0; i < 8; ++i)
            {
                buf[i][(size_t) w] = (float) ((v[i] - hs) * g[i] + x * sgIn[i] * .42 + sh * sgSh[i] * .5);
                l += v[i] * oLg[i]; r += v[i] * oRg[i];
            }
            w = (w + 1) & M;
            l *= .42 * norm; r *= .42 * norm;
            if (! (std::abs (l) < 8 && std::abs (r) < 8)) { reset(); idle = false; l = r = 0; }
            oL[t] = (float) l; oR[t] = (float) r;
        }
        if (! live)
        {
            float m = 0; for (int t = 0; t < n; t += 8) m = std::max (m, std::max (std::abs (oL[t]), std::abs (oR[t])));
            if (m < 2e-6f) { quiet += n; if (quiet > (int) (sr * .32)) reset(); } else quiet = 0;
        }
    }
    bool sleeping() const noexcept { return idle; }

private:
    struct Grains
    {
        double r = 2; int L = 1, jit = 1; struct G { int age = 0; double d0 = 0; } g[4];
        void init (double sr, double ratio, double grain, Rng& rng)
        {
            r = ratio; L = (int) std::lround (grain * sr); jit = (int) std::lround (.018 * sr);
            for (int k = 0; k < 4; ++k) { g[k].age = (int) std::lround (k * L / 4.0); restart (g[k], true, rng); }
        }
        void restart (G& x, bool keepAge, Rng& rng) { x.d0 = (r - 1) * L + 6 + rng.uni() * jit; if (! keepAge) x.age = 0; }
        double read (const std::vector<float>& b, int Mk, int wr, Rng& rng)
        {
            double out = 0; const double s = r - 1;
            for (auto& x : g)
            {
                if (x.age >= L) restart (x, false, rng);
                const double d = x.d0 - s * x.age, pos = wr - d; const int i = (int) std::floor (pos); const double f = pos - i;
                const double A = b[(size_t) (i & Mk)], B = b[(size_t) ((i + 1) & Mk)];
                out += (A + (B - A) * f) * (.5 - .5 * std::cos (kTau * x.age / L));
                ++x.age;
            }
            return out * .5;
        }
    };
    struct AP { std::vector<float> b; int i = 0; float g = .7f; };

    double sr = 48000;
    std::vector<float> pre; int preW = 0, preD = 0;
    std::array<AP, 4> ap;
    std::array<std::vector<float>, 8> buf; int M = 16383, w = 0;
    double len[8] {}, dep[8] {}, inc[8] {}, ph[8] {}, g[8] {}, v[8] {};
    std::array<double, 8> lp {}, hp {};
    const double sgIn[8] = { 1, -1, 1, -1, -1, 1, -1, 1 }, sgSh[8] = { 1, 1, -1, -1, 1, 1, -1, -1 };
    const double oLg[8] = { 1, .35, -1, -.35, 1, .35, -1, -.35 };       // (x ? 1 : .35) * (i % 4 < 2 ? 1 : -1), x = even
    const double oRg[8] = { -.35, 1, .35, -1, -.35, 1, .35, -1 };     // (x ? 1 : .35) * (i % 4 == 1 || 2 ? 1 : -1), x = odd
    std::vector<float> sb; int SM = 65535, sw = 0;
    Grains up12, up7; Rng rng;
    double rise = 0, f5 = 0, riseT = 0, f5T = 0, decay = -1, decayT = 5, norm = 1, a = .5, ah = 0, aSh = 0, ahSh = 0, shLp = 0, shHp = 0;
    int quiet = 0; bool idle = true;
};
} // namespace orph
