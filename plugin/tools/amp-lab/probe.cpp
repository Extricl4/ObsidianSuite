#include "AmpModels.h"
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace obs::amp;
static const double SR = 48000;
static double thd (const std::vector<float>& v, size_t from, double f0)
{ auto power = [&] (double f) { const double w = 2 * kPi * f / SR, c = 2 * std::cos (w); double s1 = 0, s2 = 0; for (size_t i = from; i < v.size(); ++i) { const double s0 = v[i] + c * s1 - s2; s2 = s1; s1 = s0; } return s1 * s1 + s2 * s2 - c * s1 * s2; };
  const double p1 = power (f0); double ph = 0; for (int h = 2; h <= 10; ++h) ph += power (f0 * h); return 100 * std::sqrt (ph / (p1 + 1e-30)); }
static double rmsDb (const std::vector<float>& v, size_t from) { double s = 0; for (size_t i = from; i < v.size(); ++i) s += (double) v[i] * v[i]; return 10 * std::log10 (s / (double) (v.size() - from) + 1e-20); }
int main (int argc, char** argv)
{
    const float inDb = (float) atof (argv[2]);
    for (auto d : models())
    {
        if (std::string (d.key) != argv[1]) continue;
        d.inputDb = inDb; const float extra = argc > 3 ? (float) atof (argv[3]) : 0.f; for (int k = 1; k < d.nStages; ++k) d.st[k].gainDb += extra;
        for (float amp : { 0.25f })
        for (float g : { 0.f, 2.5f, 5.f, 7.5f, 10.f })
        {
            std::vector<float> s ((size_t) SR); for (size_t i = 0; i < s.size(); ++i) s[i] = amp * (float) std::sin (2 * kPi * 110.0 * i / SR);
            AmpModel a; a.prepare (SR, 256); a.setModel (&d);
            for (size_t i = 0; i < s.size(); i += 256) a.process (&s[i], (int) std::min<size_t> (256, s.size() - i), g, 5, 5, 5, 5, 5, 5);
            std::printf ("in %.2f gain %4.1f  THD %5.1f  out %6.1f dB  peaks", amp, g, thd (s, (size_t) (SR * .5), 110), rmsDb (s, (size_t) (SR * .5)));
            for (int i = 0; i < d.nStages; ++i) std::printf (" %.2f", a.probe[i]); std::printf (" | %.2f\n", a.probe[6]);
        }
    }
}
