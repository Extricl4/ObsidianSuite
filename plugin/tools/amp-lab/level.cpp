#include "AmpModels.h"
#include <cstdio>
#include <random>
using namespace obs::amp;
static const double SR = 48000;
static double rmsDb (const std::vector<float>& v, size_t from) { double s = 0; for (size_t i = from; i < v.size(); ++i) s += (double) v[i] * v[i]; return 10 * std::log10 (s / (double) (v.size() - from) + 1e-20); }
static std::vector<float> riff (double secs)
{ std::vector<float> out ((size_t) (SR * secs)); std::mt19937 rng (7); std::uniform_real_distribution<float> u (-1, 1);
  const float notes[] = { 82.4f, 82.4f, 98.f, 110.f, 82.4f, 146.8f, 123.5f, 196.f }; std::vector<float> buf; size_t pos = 0; const size_t step = (size_t) (SR * 0.25);
  for (size_t i = 0; i < out.size(); ++i) { if (i % step == 0) { const int len = (int) (SR / notes[(i / step) % 8]); buf.assign ((size_t) len, 0); for (auto& x : buf) x = u (rng) * 0.33f; pos = 0; }
    const size_t n = buf.size(), j = (pos + 1) % n; const float y = buf[pos]; buf[pos] = 0.4985f * (buf[pos] + buf[j]); pos = j; out[i] = y; } return out; }
int main()
{
    auto r = riff (4); std::printf ("riff in %.1f dB RMS\n", rmsDb (r, 0));
    for (const auto& d : models())
    {
        std::printf ("%-9s", d.key);
        for (float g : { 0.f, 5.f, 10.f })
        { auto x = r; AmpModel a; a.prepare (SR, 256); a.setModel (&d);
          for (size_t i = 0; i < x.size(); i += 256) a.process (&x[i], (int) std::min<size_t> (256, x.size() - i), g, 5, 5, 5, 5, 5, 5);
          std::printf ("  g%-4.0f %6.1f", g, rmsDb (x, (size_t) SR)); }
        std::printf ("\n");
    }
}
