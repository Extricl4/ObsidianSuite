// HYDRA test bench: plays synthetic guitar parts through the harmonizer and measures
//   - pitch: is every voice on the right note, how long after each attack (ms)
//   - clicks: energy far above anything in the signal (a splice that doesn't line up shows here)
//   - smoothness: level ripple on held notes (badly aligned splices make it pump)
//   - lag: how far the voices trail the guitar
//   - CPU
// Build: see run.sh. Writes WAVs to out/ for listening.
#include "Harmonizer.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>
#include <random>
#include <string>
#include <complex>

using namespace obs::dsp::hydra;

static double SR = 48000;

// ------------------------------------------------------------------ synthetic guitar
struct Note { double t, dur; double midi; bool picked = true; double bendTo = 0, bendAt = 0, bendLen = 0; double vib = 0; double vel = 1; };

struct Track { std::vector<float> x; std::vector<double> f0; std::vector<char> attack; };   // f0 = 0: silent

static double midiHz (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }

static Track render (const std::vector<Note>& notes, double len, unsigned seed = 1, int harmonics = 28, double inharm = 0.00008)
{
    const int N = (int) (len * SR);
    Track tr; tr.x.assign ((size_t) N, 0.f); tr.f0.assign ((size_t) N, 0.0); tr.attack.assign ((size_t) N, 0);
    std::mt19937 rng (seed); std::normal_distribution<double> nd (0, 1);
    // legato notes continue the previous note's phases and level
    std::vector<double> ph ((size_t) harmonics + 1, 0.0), amp ((size_t) harmonics + 1, 0.0);
    for (size_t k = 0; k < notes.size(); ++k)
    {
        const auto& n = notes[k];
        const int s0 = (int) (n.t * SR), s1 = std::min (N, (int) ((n.t + n.dur) * SR));
        if (n.picked)
        {
            const double pickPos = 0.13 + 0.05 * (k % 3);
            for (int h = 1; h <= harmonics; ++h) amp[(size_t) h] = n.vel * std::abs (std::sin (kPiD * h * pickPos)) / std::pow (h, 1.05) * 0.35;
            Biquad nl1, nl2; nl1.set (Biquad::LP, SR, 1800, 0.54); nl2.set (Biquad::LP, SR, 1800, 1.31); Biquad nl3, nl4; nl3.set (Biquad::LP, SR, 1800, 0.54); nl4.set (Biquad::LP, SR, 1800, 1.31);
            for (int i = 0; i < (int) (0.010 * SR) && s0 + i < N; ++i)              // pick noise (a pick, not a hiss)
                tr.x[(size_t) (s0 + i)] += nl4.process (nl3.process (nl2.process (nl1.process ((float) (nd (rng) * 0.3 * n.vel * std::exp (-i / (0.0012 * SR)))))));
            tr.attack[(size_t) s0] = 1;
        }
        for (int i = s0; i < s1; ++i)
        {
            const double t = (i - s0) / SR;
            double m = n.midi;
            if (n.picked) m += 0.15 * std::exp (-t / 0.03);                         // string tension settles
            if (n.bendLen > 0 && t > n.bendAt) m += (n.bendTo - n.midi) * std::min (1.0, (t - n.bendAt) / n.bendLen);
            if (n.vib > 0) m += n.vib * std::sin (2 * kPiD * 5.5 * t) * std::min (1.0, t / 0.2);
            const double f = midiHz (m);
            double y = 0;
            for (int h = 1; h <= harmonics; ++h)
            {
                const double fh = f * h * std::sqrt (1.0 + inharm * h * h);
                if (fh > std::min (SR * 0.45, 3000.0)) break;
                ph[(size_t) h] += 2 * kPiD * fh / SR; if (ph[(size_t) h] > 2 * kPiD) ph[(size_t) h] -= 2 * kPiD;
                amp[(size_t) h] *= std::exp (-(1.2 + 0.35 * h) / SR);
                y += amp[(size_t) h] * std::sin (ph[(size_t) h]);
            }
            const bool lastOfPhrase = k + 1 >= notes.size() || notes[k + 1].picked;
            double rel = lastOfPhrase && s1 - i < (int) (0.01 * SR) ? (s1 - i) / (0.01 * SR) : 1.0;  // damp at the end
            if (n.picked && t < 0.0015) rel *= 0.5 - 0.5 * std::cos (kPiD * t / 0.0015);   // no step at the start
            tr.x[(size_t) i] += (float) (y * rel);
            tr.f0[(size_t) i] = midiHz (m);
        }
        // a following non-legato note: old phases restart
        if (k + 1 < notes.size() && notes[k + 1].picked) for (auto& a : amp) a = 0;
    }
    return tr;
}

static std::vector<float> distort (const std::vector<float>& x)
{
    std::vector<float> y (x.size());
    Biquad pre, post1, post2; pre.set (Biquad::HP, SR, 400, 0.7); post1.set (Biquad::LP, SR, 4500, 0.7); post2.set (Biquad::LP, SR, 5500, 0.6);
    for (size_t i = 0; i < x.size(); ++i) { const float a = pre.process (x[i]) * 30.f; y[i] = post2.process (post1.process (std::tanh (a))) * 0.3f; }
    return y;
}

// ------------------------------------------------------------------ analysis
/** Accurate offline pitch (YIN at full rate, 30 ms window) around sample c. 0 if unclear. */
static double yinAt (const std::vector<float>& x, int c, double fmin = 60, double fmax = 3000, double* clar = nullptr, double expect = 0)
{
    int W = (int) (0.025 * SR), tmax = (int) (SR / fmin), tmin = (int) (SR / fmax);
    if (expect > 0) { const double P = SR / expect; W = std::max ((int) (0.002 * SR), (int) (1.5 * P)); tmax = (int) (P * 1.7); tmin = std::max (2, (int) (P / 1.7)); }
    const int s = c - W / 2 - tmax / 2;
    if (s < 0 || s + W + tmax >= (int) x.size()) return 0;
    std::vector<double> d ((size_t) tmax + 2, 0.0);
    double run = 0; d[0] = 1;
    for (int tau = 1; tau <= tmax; ++tau)
    {
        double acc = 0;
        for (int j = 0; j < W; ++j) { const double df = x[(size_t) (s + j)] - x[(size_t) (s + j + tau)]; acc += df * df; }
        run += acc; d[(size_t) tau] = run > 0 ? acc * tau / run : 1;
    }
    int best = -1;
    for (int tau = tmin; tau < tmax; ++tau) if (d[(size_t) tau] < 0.12) { while (tau + 1 < tmax && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau; best = tau; break; }
    if (best < 0) { if (clar) *clar = 0; return 0; }
    double t = best;
    const double a = d[(size_t) best - 1], b = d[(size_t) best], cc = d[(size_t) best + 1], den = a - 2 * b + cc;
    if (std::abs (den) > 1e-12) t += 0.5 * (a - cc) / den;
    if (clar) *clar = 1 - b;
    return SR / t;
}

struct Result
{
    std::string name;
    int notes = 0, wrong = 0;
    double meanSettle = 0, maxSettle = 0, centsErr = 0, centsMax = 0;
    double clickDb = -200, rippleDb = 0, lagMs = 0, cpuPct = 0;
    double meanDelay = 0, maxDelay = 0; int jumps = 0, forced = 0, postponed = 0;
};

/** Energy above `hz` in 1 ms frames relative to the signal's overall level: the worst frame (dB). */
static double clickMeasure (const std::vector<float>& y, double hz, int from, int to)
{
    Biquad a, b, c, d; a.set (Biquad::HP, SR, hz, 0.5412); b.set (Biquad::HP, SR, hz, 1.3066); c.set (Biquad::HP, SR, hz, 0.5412); d.set (Biquad::HP, SR, hz, 1.3066);
    double tot = 0; for (int i = from; i < to; ++i) tot += (double) y[(size_t) i] * y[(size_t) i];
    const double rms = std::sqrt (tot / std::max (1, to - from));
    const int F = (int) (0.001 * SR);
    double worst = 0, acc = 0; int cnt = 0;
    for (int i = from; i < to; ++i)
    {
        const float h = d.process (c.process (b.process (a.process (y[(size_t) i]))));
        acc += (double) h * h;
        if (++cnt == F) { worst = std::max (worst, std::sqrt (acc / F)); acc = 0; cnt = 0; }
    }
    return 20 * std::log10 (worst / std::max (rms, 1e-12) + 1e-12);
}

/** Level ripple on held parts: 2 ms RMS vs its 40 ms average, the worst dip/peak in dB. */
struct Span { int a, b; double period; };
static double ripple (const std::vector<float>& y, const std::vector<Span>& spans)
{
    double worst = 0;
    for (auto sp : spans)
    {
        const int a = sp.a, b = sp.b;
        const int F = std::max ((int) (0.002 * SR), (int) (2.0 * sp.period));
        std::vector<double> r;
        const int stp = (int) (0.001 * SR);
        for (int i = a; i + F < b; i += stp) { double s = 0; for (int j = 0; j < F; ++j) s += (double) y[(size_t) (i + j)] * y[(size_t) (i + j)]; r.push_back (std::sqrt (s / F)); }
        const size_t hw = std::max<size_t> ((size_t) (F / stp) + 2, 20);
        for (size_t k = hw; k + hw < r.size(); ++k)
        {
            double m = 0; for (size_t j = k - hw; j < k + hw; ++j) m += r[j]; m /= (double) (2 * hw);
            if (m < 1e-5) continue;
            worst = std::max (worst, std::abs (20 * std::log10 ((r[k] + 1e-9) / m)));
        }
    }
    return worst;
}

// ------------------------------------------------------------------ running
struct Run { std::vector<float> L, R; double seconds = 0; };

static Run process (Harmonizer& hz, const std::vector<float>& di, const std::vector<float>& sig, Params p, int block = 128, bool stereo = true)
{
    Run o; const int N = (int) sig.size(); o.L.assign ((size_t) N, 0.f); o.R.assign ((size_t) N, 0.f);
    hz.prepare (SR, block);
    const auto t0 = std::chrono::steady_clock::now();
    for (int s = 0; s < N; s += block)
    {
        const int n = std::min (block, N - s);
        hz.process (di.data() + s, sig.data() + s, stereo ? sig.data() + s : nullptr, o.L.data() + s, stereo ? o.R.data() + s : nullptr, n, p);
    }
    o.seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    return o;
}

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R)
{
    FILE* f = std::fopen (path.c_str(), "wb"); if (! f) return;
    const int n = (int) L.size(), ch = 2, bytes = n * ch * 2;
    auto w32 = [&] (int v) { std::fwrite (&v, 4, 1, f); }; auto w16 = [&] (short v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + bytes); std::fwrite ("WAVEfmt ", 1, 8, f); w32 (16); w16 (1); w16 ((short) ch); w32 ((int) SR); w32 ((int) SR * ch * 2); w16 ((short) (ch * 2)); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (bytes);
    for (int i = 0; i < n; ++i) for (int c = 0; c < 2; ++c) { const float v = std::max (-1.f, std::min (1.f, (c ? R : L)[(size_t) i])); w16 ((short) std::lrint (v * 32000)); }
    std::fclose (f);
}

static Params onlyVoice (bool smart, int stepOrSemi, int key = 4, int mode = 1)
{
    Params p; p.on = true; p.key = key; p.mode = mode; p.mix = 1.f; p.human = 0; p.spread = 0;
    for (auto& v : p.v) v.on = false;
    p.v[0].on = true; p.v[0].smart = smart; p.v[0].level = 1.f; p.v[0].pan = 0;
    if (smart) p.v[0].step = stepOrSemi; else p.v[0].semi = stepOrSemi;
    return p;
}

/** Pitch-check one voice against the notes: for each note, when (ms after its start) does the
    voice settle within 25 cents of the right pitch, and how far off is it after that. */
static Result checkPitch (const std::string& name, const Track& tr, const std::vector<Note>& notes, const Run& o, const Params& p, const Harmonizer& hz, bool verbose = false)
{
    Result r; r.name = name;
    const auto& y = o.L;
    double sumSettle = 0; int nSettle = 0; double sumC = 0; int nC = 0;
    for (size_t k = 0; k < notes.size(); ++k)
    {
        const auto& n = notes[k];
        if (n.bendLen > 0 || n.vib > 0 || n.dur < 0.05) continue;
        const int s0 = (int) (n.t * SR);
        const int s1 = (int) ((n.t + n.dur) * SR) - (int) (0.015 * SR);
        const auto& v = p.v[0];
        const int note = (int) std::lround (n.midi);
        const int shift = v.smart ? obs::harmdefs::smartShift (note, v.step, p.key, p.mode, p.hold, 0) : v.semi;
        double settle = -1, worst = 0, sum = 0; int cnt = 0, okRun = 0; bool bad = false;
        for (int c = s0 + (int) (0.002 * SR); c < s1; c += (int) (0.001 * SR))
        {
            const double want = tr.f0[(size_t) std::max (s0 + 1, c - (int) (0.004 * SR))] * std::pow (2.0, (shift + v.detune * 0.01) / 12.0);
            if (want <= 0) continue;
            double cl = 0;
            const double got = yinAt (y, c, 50, 4000, &cl, want);
            const double cents = got > 0 ? 1200 * std::log2 (got / want) : 9999;
            if (settle < 0) { if (std::abs (cents) < 25 && ++okRun >= 3) settle = (c - s0) / SR * 1000 - 2; else { if (std::abs (cents) >= 25) okRun = 0; if (getenv ("DUMPNOTE") && atoi (getenv ("DUMPNOTE")) == (int) k) std::printf ("      t+%5.1f ms want %7.1f got %7.1f (pre)\n", (c - s0) / SR * 1000, want, got); continue; } }
            if (std::abs (cents) > 50) bad = true;
            if (getenv ("DUMPNOTE") && atoi (getenv ("DUMPNOTE")) == (int) k) std::printf ("      t+%5.1f ms want %7.1f got %7.1f (%6.0f ct)\n", (c - s0) / SR * 1000, want, got, cents);
            worst = std::max (worst, std::abs (cents)); sum += std::abs (cents); ++cnt;
        }
        ++r.notes;
        if (settle < 0 || bad) { ++r.wrong; if (verbose) std::printf ("    note %zu (midi %.0f, shift %d): %s\n", k, n.midi, shift, settle < 0 ? "never right" : "wanders off"); }
        if (settle >= 0) { sumSettle += settle; ++nSettle; r.maxSettle = std::max (r.maxSettle, settle); }
        if (cnt) { sumC += sum / cnt; ++nC; r.centsMax = std::max (r.centsMax, worst); }
    }
    r.meanSettle = nSettle ? sumSettle / nSettle : -1;
    r.centsErr = nC ? sumC / nC : -1;
    const auto& st = hz.leadVoice (0).getStats();
    r.meanDelay = st.n ? st.sumDelay / st.n / SR * 1000 : 0; r.maxDelay = st.maxDelay / SR * 1000;
    r.jumps = st.jumps; r.forced = st.forced; r.postponed = st.postponed;
    r.cpuPct = o.seconds / (tr.x.size() / SR) * 100;
    // clicks: above 13 kHz (the parts stay under ~7 kHz even shifted up)
    r.clickDb = clickMeasure (y, 13000, (int) (0.05 * SR), (int) y.size() - (int) (0.05 * SR));
    std::vector<Span> spans;
    for (auto& n : notes) if (n.dur > 0.3 && n.bendLen == 0 && n.vib == 0)
    {
        const int shift = p.v[0].smart ? obs::harmdefs::smartShift ((int) std::lround (n.midi), p.v[0].step, p.key, p.mode, p.hold, 0) : p.v[0].semi;
        spans.push_back ({ (int) ((n.t + 0.06) * SR), (int) ((n.t + n.dur - 0.03) * SR), SR / (midiHz (n.midi + shift)) });
    }
    r.rippleDb = spans.empty() ? 0 : ripple (y, spans);
    return r;
}

static void print (const Result& r)
{
    std::printf ("  %-34s notes %3d wrong %2d | settle %5.1f ms (max %5.1f) | err %4.1f ct (max %4.1f) | HF %6.1f dB | ripple %4.1f dB | delay %4.1f ms (max %4.1f) | splices %4d forced %d | cpu %.2f%%\n",
                 r.name.c_str(), r.notes, r.wrong, r.meanSettle, r.maxSettle, r.centsErr, r.centsMax, r.clickDb, r.rippleDb, r.meanDelay, r.maxDelay, r.jumps, r.forced, r.cpuPct);
}

// ------------------------------------------------------------------ parts
static std::vector<Note> sustained (const std::vector<int>& midis, double dur = 0.9)
{
    std::vector<Note> v; double t = 0.1;
    for (int m : midis) { v.push_back ({ t, dur, (double) m }); t += dur + 0.1; }
    return v;
}
static std::vector<Note> phrase (const std::vector<int>& midis, double noteLen, double gap = 0.0)
{
    std::vector<Note> v; double t = 0.1;
    for (int m : midis) { v.push_back ({ t, noteLen - gap, (double) m }); t += noteLen; }
    return v;
}
static double endOf (const std::vector<Note>& v) { double e = 0; for (auto& n : v) e = std::max (e, n.t + n.dur); return e + 0.3; }

int main (int argc, char** argv)
{
    std::string only = argc > 1 ? argv[1] : "";
    std::system ("mkdir -p out");
    Harmonizer hz;
    std::vector<Result> all;
    if (only == "dbgclean")
    {   // the self-test's melody: clean tones, no pick noise
        const int mel[] = { 57, 59, 60, 62, 64, 65, 67, 69, 67, 64, 60, 57 };
        const double nl = 0.30; const int N = (int) (SR * nl * 13);
        std::vector<float> x ((size_t) N, 0.f);
        for (int i = 0; i < N; ++i)
        {
            const double t = i / SR; const int k = (int) (t / nl); if (k >= 12) break;
            const double tn = t - k * nl, f = 440.0 * std::pow (2.0, (mel[k] - 69) / 12.0);
            double v = 0; for (int h = 1; h <= 10; ++h) v += std::sin (2 * kPiD * f * h * tn) * std::exp (-tn * (1.5 + 0.6 * h)) / h;
            const double att = std::min (1.0, tn / 0.0015), rel = std::min (1.0, (nl - tn) / 0.008);
            x[(size_t) i] = (float) (0.2 * v * att * rel);
        }
        auto p = onlyVoice (true, 2, 9, 1);
        const auto o = process (hz, x, x, p);
        writeWav ("out/clean.wav", o.L, x);
        return 0;
    }
    if (only == "dbglow")
    {
        const int semi = argc > 2 ? std::atoi (argv[2]) : 7;
        const std::vector<int> mel = { 40, 40, 43, 40, 45, 40, 46, 45, 40, 40, 43, 47, 45, 43, 42, 40 };
        const auto notes = phrase (mel, 0.25, 0.03);
        const auto tr = render (notes, endOf (notes), 6);
        const auto p = onlyVoice (false, semi);
        const auto o = process (hz, tr.x, tr.x, p);
        auto r = checkPitch ("dbg", tr, notes, o, p, hz, true); print (r);
        writeWav ("out/dbglow.wav", o.L, tr.x);
        return 0;
    }
    if (only == "dbgheld")
    {
        const int semi = argc > 2 ? std::atoi (argv[2]) : -12;
        const auto notes = sustained ({ 40, 45, 50, 55, 59, 64, 69, 74, 79, 84, 88 });
        const auto tr = render (notes, endOf (notes));
        const auto p = onlyVoice (false, semi);
        const auto o = process (hz, tr.x, tr.x, p);
        auto r = checkPitch ("dbg", tr, notes, o, p, hz, true); print (r);
        writeWav ("out/dbgheld.wav", o.L, tr.x);
        return 0;
    }
    if (only == "dbgphrase")
    {
        const int st = argc > 2 ? std::atoi (argv[2]) : 7;
        const std::vector<int> mel = { 64, 66, 67, 69, 71, 72, 74, 76 };
        const auto notes = phrase (mel, 60.0 / 140 / 2);
        const auto tr = render (notes, endOf (notes), 3);
        auto p = argc > 3 ? onlyVoice (true, st) : onlyVoice (false, st);
        const double dms = getenv ("DELAYMS") ? atof (getenv ("DELAYMS")) : 0.0;
        p.v[0].delayMs = (float) dms;
        const auto o = process (hz, tr.x, tr.x, p);
        auto n2 = notes; for (auto& n : n2) n.t += dms * 0.001;
        Track t2 = tr; t2.f0.insert (t2.f0.begin(), (size_t) (dms * 0.001 * SR), 0.0); t2.f0.resize (tr.f0.size());
        auto r = checkPitch ("dbg", t2, n2, o, p, hz, true); print (r);
        writeWav ("out/dbg.wav", o.L, tr.x);
        return 0;
    }
    if (only == "debug")
    {
        const int semi = argc > 2 ? std::atoi (argv[2]) : -12;
        const int midi = argc > 3 ? std::atoi (argv[3]) : 57;
        const auto notes = sustained ({ midi }, 1.0);
        const auto tr = render (notes, endOf (notes));
        auto p = onlyVoice (false, semi);
        hz.prepare (SR, 128);
        std::vector<float> L (tr.x.size()), R (tr.x.size());
        int lastJ = 0;
        for (size_t s = 0; s < tr.x.size(); s += 128)
        {
            const int n = (int) std::min<size_t> (128, tr.x.size() - s);
            hz.process (tr.x.data() + s, tr.x.data() + s, nullptr, L.data() + s, nullptr, n, p);
            const auto& st = hz.leadVoice (0).getStats();
            if (s % 1920 == 0 || st.jumps != lastJ)
            {
                const auto& sg = hz.timeline().latest();
                std::printf ("t %6.1f ms  delay %6.2f ms  jumps %d forced %d  seg note %d period %.1f  pitch %.1f\n", s / SR * 1000, hz.leadVoice (0).delay ((i64) (s + n)) / SR * 1000, st.jumps, st.forced, sg.note, sg.period, yinAt (L, (int) s - 1500));
                lastJ = st.jumps;
            }
        }
        writeWav ("out/debug.wav", L, tr.x);
        return 0;
    }
    auto want = [&] (const char* k) { return only.empty() || only == k || only == "all"; };

    // 1. held notes over the whole neck, common fixed intervals
    if (want ("held"))
    {
        std::printf ("\n== Held notes E2..E6, fixed intervals\n");
        const auto notes = sustained ({ 40, 45, 50, 55, 59, 64, 69, 74, 79, 84, 88 });
        const auto tr = render (notes, endOf (notes));
        for (int semi : { 3, 4, 7, 12, -12, -5, 24 })
        {
            const auto p = onlyVoice (false, semi);
            const auto o = process (hz, tr.x, tr.x, p);
            auto r = checkPitch ("held, fixed " + std::to_string (semi) + " st", tr, notes, o, p, hz, true);
            print (r); all.push_back (r);
            if (semi == 4) writeWav ("out/held_plus4.wav", o.L, tr.x);
        }
    }
    // 2. picked E minor phrase, smart third / fifth / sixth below
    if (want ("lead"))
    {
        std::printf ("\n== Picked lead phrase in E minor (8ths at 140 bpm), smart voices\n");
        const std::vector<int> mel = { 64, 66, 67, 69, 71, 72, 74, 76, 74, 72, 71, 69, 67, 66, 64, 71, 76, 79, 78, 76, 74, 71, 72, 69 };
        const auto notes = phrase (mel, 60.0 / 140 / 2);
        const auto tr = render (notes, endOf (notes), 3);
        for (int st : { 2, 4, -2, 5, 7 })
        {
            const auto p = onlyVoice (true, st);
            const auto o = process (hz, tr.x, tr.x, p);
            auto r = checkPitch ("lead 8ths, smart step " + std::to_string (st), tr, notes, o, p, hz, true);
            print (r); all.push_back (r);
            if (st == 2) writeWav ("out/lead_third.wav", o.L, tr.x);
        }
        // fast 16ths at 180
        const auto fast = phrase (mel, 60.0 / 180 / 4);
        const auto tf = render (fast, endOf (fast), 4);
        for (int st : { 2, 4 })
        {
            const auto p = onlyVoice (true, st);
            const auto o = process (hz, tf.x, tf.x, p);
            auto r = checkPitch ("lead 16ths@180, smart step " + std::to_string (st), tf, fast, o, p, hz);
            print (r); all.push_back (r);
            if (st == 2) writeWav ("out/fast_third.wav", o.L, tf.x);
        }
        // with a voice DELAY of 30 ms the interval should be right from the attack
        {
            auto p = onlyVoice (true, 2); p.v[0].delayMs = 30;
            const auto o = process (hz, tr.x, tr.x, p);
            // shift the expected timeline by 30 ms
            auto n2 = notes; for (auto& n : n2) n.t += 0.030;
            Track t2 = tr; t2.f0.insert (t2.f0.begin(), (size_t) (0.03 * SR), 0.0); t2.f0.resize (tr.f0.size());
            auto r = checkPitch ("lead 8ths, smart 3rd, DELAY 30 ms", t2, n2, o, p, hz);
            print (r); all.push_back (r);
        }
    }
    // 3. high register lead (where solos live)
    if (want ("high"))
    {
        std::printf ("\n== High lead (B3..E6), 16ths at 140 bpm\n");
        const std::vector<int> mel = { 76, 79, 81, 83, 84, 86, 88, 86, 84, 83, 81, 79, 78, 76, 74, 71, 83, 88, 91, 88, 86, 83, 81, 79 };
        const auto notes = phrase (mel, 60.0 / 140 / 4);
        const auto tr = render (notes, endOf (notes), 5);
        for (int st : { 2, 4, 7 })
        {
            const auto p = onlyVoice (true, st);
            const auto o = process (hz, tr.x, tr.x, p);
            auto r = checkPitch ("high 16ths, smart step " + std::to_string (st), tr, notes, o, p, hz, true);
            print (r); all.push_back (r);
        }
    }
    // 4. low riffs (palm-mute-ish short notes) with octave down / fifth
    if (want ("low"))
    {
        std::printf ("\n== Low riff (E2-A2), 8ths at 120 bpm\n");
        const std::vector<int> mel = { 40, 40, 43, 40, 45, 40, 46, 45, 40, 40, 43, 47, 45, 43, 42, 40 };
        const auto notes = phrase (mel, 0.25, 0.03);
        const auto tr = render (notes, endOf (notes), 6);
        for (int semi : { -12, 7, 12 })
        {
            const auto p = onlyVoice (false, semi);
            const auto o = process (hz, tr.x, tr.x, p);
            auto r = checkPitch ("low riff, fixed " + std::to_string (semi), tr, notes, o, p, hz, true);
            print (r); all.push_back (r);
        }
    }
    // 5. legato, bends, vibrato (no attacks between notes)
    if (want ("legato"))
    {
        std::printf ("\n== Legato, bend, vibrato\n");
        std::vector<Note> notes;
        double t = 0.1;
        notes.push_back ({ t, 0.25, 69 }); t += 0.25;
        notes.push_back ({ t, 0.25, 71, false }); t += 0.25;            // hammer-on
        notes.push_back ({ t, 0.25, 72, false }); t += 0.25;
        notes.push_back ({ t, 0.25, 71, false }); t += 0.25;            // pull-off
        notes.push_back ({ t, 0.5, 69, false }); t += 0.6;
        const auto tr = render (notes, endOf (notes), 7);
        for (int st : { 2, 4 })
        {
            const auto p = onlyVoice (true, st);
            const auto o = process (hz, tr.x, tr.x, p);
            auto r = checkPitch ("legato, smart step " + std::to_string (st), tr, notes, o, p, hz, true);
            print (r); all.push_back (r);
            if (st == 2) writeWav ("out/legato_third.wav", o.L, tr.x);
        }
        // a whole-step bend and a vibrato note: check for clicks and level only
        std::vector<Note> nb;
        nb.push_back ({ 0.1, 1.2, 74, true, 76, 0.25, 0.25 });
        nb.push_back ({ 1.5, 1.4, 76, true, 0, 0, 0, 0.35 });
        const auto tb = render (nb, endOf (nb), 8);
        const auto p = onlyVoice (true, 2);
        const auto o = process (hz, tb.x, tb.x, p);
        auto r = checkPitch ("bend + vibrato, smart 3rd", tb, nb, o, p, hz);
        print (r); all.push_back (r);
        writeWav ("out/bend_vibrato.wav", o.L, tb.x);
    }
    // 6. after a distorted amp (track the clean DI, shift the distorted signal)
    if (want ("dist"))
    {
        std::printf ("\n== After a high-gain amp\n");
        const std::vector<int> mel = { 64, 66, 67, 69, 71, 72, 74, 76, 74, 72, 71, 69, 67, 66, 64, 71 };
        const auto notes = phrase (mel, 60.0 / 140 / 2);
        const auto tr = render (notes, endOf (notes), 9);
        const auto d = distort (tr.x);
        for (int st : { 2, 4 })
        {
            const auto p = onlyVoice (true, st);
            const auto o = process (hz, tr.x, d, p);
            auto r = checkPitch ("distorted, smart step " + std::to_string (st), tr, notes, o, p, hz, true);
            r.clickDb = clickMeasure (o.L, 13000, (int) (0.05 * SR), (int) o.L.size() - (int) (0.05 * SR)) - clickMeasure (d, 13000, (int) (0.05 * SR), (int) d.size() - (int) (0.05 * SR));
            print (r); all.push_back (r);
            if (st == 2) writeWav ("out/dist_third.wav", o.L, d);
        }
    }
    // 7. sample rates and block sizes
    if (want ("rates"))
    {
        std::printf ("\n== Sample rates / buffer sizes\n");
        for (double rate : { 44100.0, 96000.0 })
            for (int b : { 32, 1024 })
            {
                SR = rate;
                const std::vector<int> mel = { 64, 66, 67, 69, 71, 72, 74, 76 };
                const auto notes = phrase (mel, 0.2);
                const auto tr = render (notes, endOf (notes), 10);
                const auto p = onlyVoice (true, 2);
                const auto o = process (hz, tr.x, tr.x, p, b);
                auto r = checkPitch (std::to_string ((int) rate) + " Hz, block " + std::to_string (b), tr, notes, o, p, hz);
                print (r); all.push_back (r);
            }
        SR = 48000;
    }
    // 8. all six voices, humanize on: CPU and sanity
    if (want ("six"))
    {
        std::printf ("\n== Six voices, humanize 40%%, 10 s of lead\n");
        std::vector<int> mel; for (int i = 0; i < 80; ++i) mel.push_back ((int[]) { 64, 67, 69, 71, 74, 76, 79, 81 }[(i * 5) % 8]);
        const auto notes = phrase (mel, 0.125);
        const auto tr = render (notes, endOf (notes), 11);
        Params p; p.on = true; p.key = 4; p.mode = 1; p.human = 0.4f; p.mix = 0.5f; p.spread = 0.5f;
        const int steps[6] = { 2, 4, 7, -2, -5, 9 };
        for (int k = 0; k < 6; ++k) { p.v[k].on = true; p.v[k].smart = true; p.v[k].step = steps[k]; p.v[k].level = 0.6f; p.v[k].pan = -60.f + 24.f * k; }
        const auto o = process (hz, tr.x, tr.x, p);
        double pk = 0; for (float v : o.L) pk = std::max (pk, (double) std::abs (v));
        std::printf ("  six voices: %.2f%% of one core (48 kHz), peak %.2f, HF %.1f dB\n", o.seconds / (tr.x.size() / SR) * 100, pk, clickMeasure (o.L, 13000, 2400, (int) o.L.size() - 2400));
        writeWav ("out/six_voices.wav", o.L, o.R);
    }
    // 9. CHORDS
    if (want ("chords"))
    {
        std::printf ("\n== CHORDS: triads and power chords\n");
        struct Ch { std::vector<int> notes; };
        const std::vector<Ch> prog = { { { 40, 47, 52 } }, { { 48, 52, 55 } }, { { 43, 47, 50, 55 } }, { { 45, 52, 57, 60 } }, { { 52, 56, 59 } } };
        std::vector<Note> notes; double t = 0.1;
        for (auto& c : prog) { for (int m : c.notes) notes.push_back ({ t, 1.1, (double) m }); t += 1.2; }
        const auto tr = render (notes, endOf (notes), 12);
        // chroma (12 pitch classes, 60 Hz - 2.5 kHz) of a stretch of signal
        auto chroma = [&] (const std::vector<float>& y, int mid) {
            std::array<double, 12> c {}; const int L = 8192;
            std::vector<std::complex<double>> X (L);
            for (int i = 0; i < L; ++i) { const double w = 0.5 - 0.5 * std::cos (2 * kPiD * i / L); X[(size_t) i] = y[(size_t) (mid - L / 2 + i)] * w; }
            // plain DFT on the bins we need (fast enough here)
            for (int k = (int) (60.0 * L / SR); k < (int) (2500.0 * L / SR); ++k)
            {
                std::complex<double> acc = 0; const double wk = -2 * kPiD * k / L;
                for (int i = 0; i < L; i += 1) acc += X[(size_t) i] * std::polar (1.0, wk * i);
                const double f = k * SR / L, m = 12 * std::log2 (f / 440.0) + 69;
                const int pc = ((int) std::lround (m) % 12 + 12) % 12;
                c[(size_t) pc] += std::norm (acc);
            }
            double n = 0; for (double v : c) n += v * v; n = std::sqrt (n); for (double& v : c) v /= std::max (n, 1e-20);
            return c; };
        auto cosine = [] (const std::array<double, 12>& a, const std::array<double, 12>& b) { double s = 0; for (int i = 0; i < 12; ++i) s += a[(size_t) i] * b[(size_t) i]; return s; };
        for (int mode : { 0, 1 })
        {
            Params p = onlyVoice (mode == 1, mode == 1 ? 2 : 7); p.track = 1;
            const auto o = process (hz, tr.x, tr.x, p, 256);
            if (mode == 1)
            {   // what does the chord detector hear? (run again, print the notes in the middle of each chord)
                hz.prepare (SR, 256); std::vector<float> L2 (tr.x.size());
                for (size_t s0 = 0; s0 < tr.x.size(); s0 += 256)
                {
                    const int n = (int) std::min<size_t> (256, tr.x.size() - s0);
                    hz.process (tr.x.data() + s0, tr.x.data() + s0, nullptr, L2.data() + s0, nullptr, n, p);
                    for (size_t c = 0; c < prog.size(); ++c)
                        for (double at : { 0.15, 0.4, 0.8 })
                            if (s0 <= (size_t) ((0.1 + c * 1.2 + at) * SR) && (size_t) ((0.1 + c * 1.2 + at) * SR) < s0 + 256)
                            {
                                std::printf ("    chord %zu at +%.2fs: heard", c, at);
                                for (int i = 0; i < hz.uiChordCount.load(); ++i) std::printf (" %d", hz.uiChord[i].load());
                                std::printf ("  (played"); for (int m : prog[c].notes) std::printf (" %d", m); std::printf (")\n");
                            }
                }
            }
            // the ideal: the same synth playing the harmony notes
            std::vector<Note> ideal;
            for (auto& n : notes) { Note m = n; const int sft = mode == 1 ? obs::harmdefs::smartShift ((int) n.midi, 2, 4, 1, false, 0) : 7; m.midi += sft; ideal.push_back (m); }
            const auto ref = render (ideal, endOf (ideal), 12);
            double worst = 1, sum = 0; std::string row;
            for (size_t c = 0; c < prog.size(); ++c)
            {
                const int mid = (int) ((0.1 + c * 1.2 + 0.7) * SR);
                const double sim = cosine (chroma (o.L, mid), chroma (ref.x, mid - (int) (hz.chordLatency())));
                const double dry = cosine (chroma (tr.x, mid), chroma (ref.x, mid));
                const int early = (int) ((0.1 + c * 1.2 + 0.13) * SR);       // just after the chord change (window spans ~0.04..0.22 s)
                const double simE = cosine (chroma (o.L, early + (int) hz.chordLatency()), chroma (ref.x, early));
                if (getenv ("EARLY")) std::printf ("    chord %zu early %.2f\n", c, simE);
                worst = std::min (worst, sim); sum += sim;
                char b[64]; std::snprintf (b, sizeof b, " %.2f(dry %.2f)", sim, dry); row += b;
            }
            std::printf ("  chords %s: chroma match%s | mean %.2f worst %.2f | latency %.1f ms | cpu %.2f%% | HF %.1f dB\n",
                         mode ? "smart 3rd" : "fixed 5th", row.c_str(), sum / prog.size(), worst, hz.chordLatency() / SR * 1000, o.seconds / (tr.x.size() / SR) * 100,
                         clickMeasure (o.L, 13000, 4800, (int) o.L.size() - 4800));
            writeWav (mode ? "out/chords_third.wav" : "out/chords_fifth.wav", o.L, tr.x);
        }
    }
    // 10. robustness: silence, noise, a chord in LEAD, a huge spike
    if (want ("robust"))
    {
        std::printf ("\n== Robustness\n");
        const int N = (int) (3 * SR);
        std::vector<float> x ((size_t) N, 0.f);
        std::mt19937 rng (5); std::normal_distribution<float> nd (0, 1);
        for (int i = (int) SR; i < 2 * (int) SR; ++i) x[(size_t) i] = nd (rng) * 0.05f;
        x[(size_t) (2.5 * SR)] = 0.95f;
        auto p = onlyVoice (true, 2);
        auto o = process (hz, x, x, p);
        bool finite = true; double pk = 0; for (float v : o.L) { finite &= std::isfinite (v); pk = std::max (pk, (double) std::abs (v)); }
        std::printf ("  silence/noise/spike: finite %s, peak %.2f\n", finite ? "yes" : "NO", pk);
        // a chord played into LEAD: no explosion, level sane
        std::vector<Note> ch = { { 0.1, 1.5, 52 }, { 0.1, 1.5, 56 }, { 0.1, 1.5, 59 } };
        const auto tr = render (ch, 2.0, 13);
        o = process (hz, tr.x, tr.x, onlyVoice (false, 4));
        pk = 0; for (float v : o.L) pk = std::max (pk, (double) std::abs (v));
        double ipk = 0; for (float v : tr.x) ipk = std::max (ipk, (double) std::abs (v));
        std::printf ("  chord into LEAD: out peak %.2f (in %.2f), HF %.1f dB\n", pk, ipk, clickMeasure (o.L, 13000, 4800, (int) o.L.size() - 4800));
        // switching on/off and changing settings while playing
        {
            const auto notes = phrase ({ 64, 67, 69, 71, 74, 76, 79, 76, 74, 71, 69, 67 }, 0.3);
            const auto t2 = render (notes, endOf (notes), 14);
            const int B = 128; const int M = (int) t2.x.size();
            std::vector<float> L ((size_t) M), R ((size_t) M);
            hz.prepare (SR, B);
            auto pp = onlyVoice (true, 2); pp.mix = 0.5f;
            for (int s = 0; s < M; s += B)
            {
                const int n = std::min (B, M - s);
                const double tt = s / SR;
                pp.on = ! (tt > 1.0 && tt < 1.5);
                pp.v[0].step = tt > 2.0 ? 4 : 2;
                pp.v[0].delayMs = tt > 2.7 ? 40.f : 0.f;
                pp.mode = tt > 3.2 ? 0 : 1;
                pp.track = tt > 3.6 ? 1 : 0;
                hz.process (t2.x.data() + s, t2.x.data() + s, t2.x.data() + s, L.data() + s, R.data() + s, n, pp);
            }
            std::printf ("  switching while playing: HF %.1f dB (dry alone %.1f dB)\n", clickMeasure (L, 13000, 4800, M - 4800), clickMeasure (t2.x, 13000, 4800, M - 4800));
            writeWav ("out/switching.wav", L, R);
        }
    }
    return 0;
}
