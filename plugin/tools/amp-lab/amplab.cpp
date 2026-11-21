// Amp lab: measures every in-house amp outside the plugin.
//   g++ -O2 -std=c++17 -I../../Source/DSP amplab.cpp -o amplab && ./amplab
// Prints distortion (THD) and loudness at several gain settings, the clean tone
// stack response, and an oversampling/aliasing check. Use it when voicing amps.
#include "AmpModels.h"
#include <cstdio>
#include <random>
using namespace obs::amp;

static const double SR = 48000;

static double rmsDb (const std::vector<float>& v, size_t from)
{
    double s = 0; for (size_t i = from; i < v.size(); ++i) s += (double) v[i] * v[i];
    return 10 * std::log10 (s / (double) (v.size() - from) + 1e-20);
}
// THD via Goertzel at the fundamental and harmonics 2..10
static double thd (const std::vector<float>& v, size_t from, double f0)
{
    auto power = [&] (double f)
    {
        const double w = 2 * kPi * f / SR, c = 2 * std::cos (w); double s1 = 0, s2 = 0;
        for (size_t i = from; i < v.size(); ++i) { const double s0 = v[i] + c * s1 - s2; s2 = s1; s1 = s0; }
        return s1 * s1 + s2 * s2 - c * s1 * s2;
    };
    const double p1 = power (f0); double ph = 0;
    for (int h = 2; h <= 10; ++h) ph += power (f0 * h);
    return 100 * std::sqrt (ph / (p1 + 1e-30));
}
static std::vector<float> run (const AmpDef& d, std::vector<float> in, float gain, float b = 5, float m = 5, float t = 5, float p = 5, float dp = 5, float ms = 5)
{
    AmpModel a; a.prepare (SR, 256); a.setModel (&d);
    for (size_t i = 0; i < in.size(); i += 256) a.process (&in[i], (int) std::min<size_t> (256, in.size() - i), gain, b, m, t, p, dp, ms);
    return in;
}
// plucked-string riff (Karplus-Strong), peaks around -10 dBFS like a DI guitar
static std::vector<float> riff (double secs)
{
    std::vector<float> out ((size_t) (SR * secs)); std::mt19937 rng (7); std::uniform_real_distribution<float> u (-1, 1);
    const float notes[] = { 82.4f, 82.4f, 98.f, 110.f, 82.4f, 146.8f, 123.5f, 196.f };
    std::vector<float> buf; size_t pos = 0; const size_t step = (size_t) (SR * 0.25);
    for (size_t i = 0; i < out.size(); ++i)
    {
        if (i % step == 0) { const int len = (int) (SR / notes[(i / step) % 8]); buf.assign ((size_t) len, 0); for (auto& x : buf) x = u (rng) * 0.33f; pos = 0; }
        const size_t n = buf.size(), j = (pos + 1) % n; const float y = buf[pos]; buf[pos] = 0.4985f * (buf[pos] + buf[j]); pos = j; out[i] = y;
    }
    return out;
}
int main (int argc, char** argv)
{
    const bool resp = argc > 1 && std::string (argv[1]) == "--response";
    std::vector<float> sine ((size_t) SR);
    for (size_t i = 0; i < sine.size(); ++i) sine[i] = 0.25f * (float) std::sin (2 * kPi * 110.0 * i / SR);   // -12 dBFS, A string
    const auto r = riff (4.0);
    std::printf ("%-14s  THD%% @gain 0 / 2.5 / 5 / 7.5 / 10            riff dBFS @gain 2.5/5/10   alias dB\n", "amp");
    for (const auto& d : models())
    {
        std::printf ("%-14s ", d.name);
        for (float g : { 0.f, 2.5f, 5.f, 7.5f, 10.f }) std::printf (" %6.1f", thd (run (d, sine, g), (size_t) (SR * 0.5), 110.0));
        std::printf ("     ");
        for (float g : { 2.5f, 5.f, 10.f }) std::printf (" %6.1f", rmsDb (run (d, r, g), (size_t) (SR * 0.5)));
        // aliasing check: 3.7 kHz sine at full gain; energy that isn't a harmonic of 3.7k (below 20k) vs total
        std::vector<float> hs ((size_t) SR); for (size_t i = 0; i < hs.size(); ++i) hs[i] = 0.25f * (float) std::sin (2 * kPi * 3700.0 * i / SR);
        auto o = run (d, hs, 10.f);
        // Goertzel scan of non-harmonic bins
        double tot = 0, bad = 0;
        for (double f = 100; f < 20000; f += 50)
        {
            const double w = 2 * kPi * f / SR, c = 2 * std::cos (w); double s1 = 0, s2 = 0;
            for (size_t i = (size_t) (SR * 0.5); i < o.size(); ++i) { const double s0 = o[i] + c * s1 - s2; s2 = s1; s1 = s0; }
            const double pw = s1 * s1 + s2 * s2 - c * s1 * s2; tot += pw;
            const double hn = f / 3700.0; if (std::abs (hn - std::round (hn)) * 3700.0 > 120) bad += pw;
        }
        std::printf ("   %6.1f\n", 10 * std::log10 (bad / tot + 1e-20));
    }
    if (resp)
    {
        std::printf ("\nSmall-signal response (gain 0, all tone knobs 5), dB relative to 1 kHz\n%-14s", "Hz");
        const double fr[] = { 50, 100, 200, 400, 800, 1600, 3200, 6400, 10000 };
        for (double f : fr) std::printf (" %6.0f", f); std::printf ("\n");
        for (const auto& d : models())
        {
            std::printf ("%-14s", d.name);
            double ref = 0; std::vector<double> lv;
            for (double f : fr)
            {
                std::vector<float> s ((size_t) (SR * 0.6)); for (size_t i = 0; i < s.size(); ++i) s[i] = 0.002f * (float) std::sin (2 * kPi * f * i / SR);
                lv.push_back (rmsDb (run (d, s, 0.f), (size_t) (SR * 0.3)));
                if (f == 800) ref = lv.back();
            }
            for (double v : lv) std::printf (" %6.1f", v - ref); std::printf ("\n");
        }
    }
    return 0;
}
