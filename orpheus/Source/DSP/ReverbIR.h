#pragma once
/*  ORPHEUS · the REVERB's impulse responses, grown like the preview's makeIR() (ui/src/sirenfx.js): noise split into six
    bands by a Linkwitz-Riley tree, every band dying in its own time (highs first), a bloom, early reflections, a faded end
    and a normalised energy. ti: 0 ROOM, 1 HALL, 2 PLATE, 3 CATHEDRAL, 4 SHIMMER (the old fallback's space).          */
#include "Prims.h"
#include <vector>

namespace orph
{
struct ReverbIR
{
    std::vector<float> L, R;
    static double secOf (double v) { return .6 * std::pow (20.0, v / 100.0); }
    static double typeMod (int ti) { static const double m[5] = { .00018, .00042, .00032, .0006, .0006 }; return m[std::max (0, std::min (4, ti))]; }

    void make (double sr, double sec, double dark, int ti, double s01, uint32_t seed = 0x1234567u)
    {
        struct Ty { int erN; double er0, er1, er1x, erE, st0, st1, ri0, ri1, mult[6], gain[6]; };
        static const Ty T[5] = {
            { 16, .0025, .02, .016, .45, .002, .005, .003, .01, { .9, 1, 1, .85, .68, .5 }, { 1, 1, .95, .85, .68, .48 } },
            { 12, .007, .04, .045, .2, .008, .022, .012, .05, { 1.25, 1.1, 1, .84, .64, .44 }, { 1, 1, .95, .8, .6, .4 } },
            { 0, 0, 0, 0, 0, 0, 0, .0012, .002, { .72, .95, 1, .96, .86, .72 }, { .6, .92, 1, 1, .95, .85 } },
            { 10, .016, .08, .08, .15, .02, .045, .04, .15, { 1.4, 1.2, 1, .76, .52, .32 }, { 1, 1, .9, .74, .52, .32 } },
            { 0, 0, 0, 0, 0, .02, .03, .18, .3, { 1, 1.05, 1, .92, .8, .62 }, { .6, .9, 1, .95, .85, .66 } },
        };
        static const double DK_M[6] = { 0, 0, .1, .35, .55, .7 }, DK_G[6] = { 0, 0, .1, .35, .6, .85 };
        const Ty& ty = T[std::max (0, std::min (4, ti))];
        const double start = ty.st0 + (ty.st1 - ty.st0) * s01, tau = ty.ri0 + (ty.ri1 - ty.ri0) * s01;
        const int len = (int) std::lround (std::min (9.5, start + sec * 1.15 + .06) * sr);
        L.assign ((size_t) len, 0.f); R.assign ((size_t) len, 0.f);
        double xo[5] = { 250, 800, 2500, 6000, 12000 }; int nx = 0; for (double f : xo) if (f < sr * .42) xo[nx++] = f;
        const int nb = nx + 1;
        double gk[6], rt[6];
        for (int k = 0; k < nb; ++k)
        {
            const double mk = ty.mult[k] * (1 - dark * DK_M[k]);
            gk[k] = std::max (1e-4, ty.gain[k] * (1 - dark * DK_G[k])); rt[k] = std::max (.06, sec * mk);
        }
        Rng rng; rng.s = seed | 1u;
        for (int c = 0; c < 2; ++c)
        {
            auto& ch = c ? R : L;
            WABiquad lp[5][2], hp[5][2];
            for (int k = 0; k < nx; ++k) for (int j = 0; j < 2; ++j)
            {
                lp[k][j].sr = hp[k][j].sr = sr;
                lp[k][j].set (WABiquad::LP, xo[k], -3.0103); hp[k][j].set (WABiquad::HP, xo[k], -3.0103);
            }
            for (int i = 0; i < len; ++i)
            {
                const double t = i / sr;
                float rest = rng.white(); double sum = 0;
                for (int k = 0; k < nb; ++k)
                {
                    float band;
                    if (k < nx) { band = lp[k][1].process (lp[k][0].process (rest)); rest = hp[k][1].process (hp[k][0].process (rest)); }
                    else band = rest;
                    double e;                                        // setValueAtTime + two exponential ramps
                    if (t < start) e = gk[k];
                    else if (t < start + rt[k]) e = gk[k] * std::pow (1e-3, (t - start) / rt[k]);
                    else if (t < start + rt[k] * 5 / 3) e = gk[k] * 1e-3 * std::pow (1e-2, (t - start - rt[k]) / (rt[k] * 2 / 3));
                    else e = gk[k] * 1e-5;
                    sum += band * e;
                }
                const double rise = t < start ? 0.0 : 1.0 - std::exp (-(t - start) / std::max (1e-6, tau));
                ch[(size_t) i] = (float) (sum * rise);
            }
        }
        auto en = [] (const float* x, int n) { double e = 0; for (int i = 0; i < n; ++i) e += (double) x[i] * x[i]; return e; };
        if (ty.erN > 0)
        {
            const double t1 = ty.er1 + ty.er1x * s01, a = 1 - std::exp (-kTau * (9000 - 6000 * dark) / sr);
            for (int c = 0; c < 2; ++c)
            {
                auto& ch = c ? R : L;
                const int n = std::min (len, (int) std::ceil ((t1 + .01) * sr));
                std::vector<float> e ((size_t) std::max (1, n), 0.f);
                for (int j = 0; j < ty.erN; ++j)
                {
                    const double u = (j + rng.uni() * .8) / ty.erN, t = ty.er0 + (t1 - ty.er0) * u;
                    const int i = std::min (n - 1, (int) std::lround (t * sr));
                    e[(size_t) i] += (float) ((1 - .65 * u) * (.55 + .45 * rng.uni()) * (rng.uni() < .5f ? -1 : 1));
                }
                double y1 = 0, y2 = 0; for (int i = 0; i < n; ++i) { y1 += (e[(size_t) i] - y1) * a; y2 += (y1 - y2) * a; e[(size_t) i] = (float) y2; }
                const double s = std::sqrt (ty.erE * en (ch.data(), len) / (en (e.data(), n) + 1e-12));
                for (int i = 0; i < n; ++i) ch[(size_t) i] += (float) (e[(size_t) i] * s);
            }
        }
        const int fl = (int) std::lround (len * .22);
        for (auto* ch : { &L, &R }) for (int i = 0; i < fl; ++i) (*ch)[(size_t) (len - 1 - i)] *= (float) (.5 - .5 * std::cos (kPi * i / fl));
        const double s = 1.0 / std::sqrt ((en (L.data(), len) + en (R.data(), len)) / 2 + 1e-12);
        for (auto* ch : { &L, &R }) for (auto& x : *ch) x = (float) (x * s);
    }
};
} // namespace orph
