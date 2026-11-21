// Finds input level and output trim per amp for target distortion/loudness.
//   g++ -O2 -std=c++17 -DAMP_LAB_PROBE -I../../Source/DSP calibrate.cpp -o calibrate && ./calibrate
#include "AmpModels.h"
#include <cstdio>
#include <map>
#include <string>
using namespace obs::amp;
static const double SR = 48000;
static double thd (const std::vector<float>& v, size_t from, double f0)
{
    auto power = [&] (double f) { const double w = 2 * kPi * f / SR, c = 2 * std::cos (w); double s1 = 0, s2 = 0;
        for (size_t i = from; i < v.size(); ++i) { const double s0 = v[i] + c * s1 - s2; s2 = s1; s1 = s0; } return s1 * s1 + s2 * s2 - c * s1 * s2; };
    const double p1 = power (f0); double ph = 0; for (int h = 2; h <= 10; ++h) ph += power (f0 * h); return 100 * std::sqrt (ph / (p1 + 1e-30));
}
static double rmsDb (const std::vector<float>& v, size_t from) { double s = 0; for (size_t i = from; i < v.size(); ++i) s += (double) v[i] * v[i]; return 10 * std::log10 (s / (double) (v.size() - from) + 1e-20); }
static std::vector<float> run (const AmpDef& d, std::vector<float> in, float gain, float* probe = nullptr)
{
    AmpModel a; a.prepare (SR, 256); a.setModel (&d);
    for (size_t i = 0; i < in.size(); i += 256) a.process (&in[i], (int) std::min<size_t> (256, in.size() - i), gain, 5, 5, 5, 5, 5, 5);
    if (probe) for (int i = 0; i < 8; ++i) probe[i] = a.probe[i];
    return in;
}
int main()
{
    std::map<std::string, double> target { { "glass65", 3 }, { "top30", 8 }, { "plexi68", 16 }, { "brit800", 30 }, { "slo100", 40 }, { "styx51", 44 }, { "recto", 40 }, { "vh4", 40 }, { "core", 42 } };
    std::vector<float> sine ((size_t) SR); for (size_t i = 0; i < sine.size(); ++i) sine[i] = 0.25f * (float) std::sin (2 * kPi * 110.0 * i / SR);
    for (auto d : models())     // copy, so we can change it
    {
        double lo = -30, hi = 30;
        for (int it = 0; it < 30; ++it) { d.inputDb = (float) ((lo + hi) / 2); const double t = thd (run (d, sine, 5), (size_t) (SR * .5), 110); if (t < target[d.key]) lo = d.inputDb; else hi = d.inputDb; }
        d.inputDb = (float) ((lo + hi) / 2);
        float pr[8]; run (d, sine, 5, pr);
        d.levelDb = 0; const double lv = rmsDb (run (d, sine, 5), (size_t) (SR * .5));
        const double trim = -18 - lv;
        std::printf ("%-9s inputDb %6.1f  levelDb %6.1f   THD g0 %5.1f g5 %5.1f g10 %5.1f   stage peaks:", d.key, d.inputDb, trim,
                     thd (run (d, sine, 0), (size_t) (SR * .5), 110), thd (run (d, sine, 5), (size_t) (SR * .5), 110), thd (run (d, sine, 10), (size_t) (SR * .5), 110));
        for (int i = 0; i < d.nStages; ++i) std::printf (" %.2f", pr[i]); std::printf (" | pre-PI %.2f\n", pr[6]);
    }
}
