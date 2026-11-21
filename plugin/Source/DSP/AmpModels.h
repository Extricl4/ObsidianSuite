#pragma once
// =============================================================================
//  In-house amp models.
//
//  Each amp is modelled stage by stage from its circuit, not from a capture:
//    input (coupling cap, bright cap)  ->  triode stages (each with its own gain,
//    bias point, cathode bypass cap, coupling cap and Miller roll-off, and the
//    phase flip every real tube stage has)  ->  cathode follower  ->  the amp's
//    real tone stack (Fender/Marshall/Mesa "FMV" network solved exactly from its
//    resistor and capacitor values)  ->  phase inverter  ->  push-pull power
//    tubes with supply sag, presence/resonance (the negative-feedback controls)
//    ->  output transformer.
//  All non-linear parts run 4x oversampled (linear-phase halfband FIR), so high
//  gain stays clean of aliasing.
//
//  The table at the bottom lists every amp, what it is based on, and the values.
//  To add an amp: copy an entry, change the numbers, rebuild. Self-contained on
//  purpose (no JUCE), so tools/amp-lab can measure it outside the plugin.
//
//  Names of real amps are trademarks of their owners; "based on" describes what
//  each model was voiced after. No affiliation.
// =============================================================================
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>

namespace obs::amp
{
constexpr double kPi = 3.14159265358979323846;
inline float dbToLin (float db) { return std::pow (10.f, db * 0.05f); }

/* ---------------------------------------------------------------- basics */
struct OnePole        // 1st-order low- or high-pass (bilinear, prewarped)
{
    float b0 = 1, b1 = 0, a1 = 0, z = 0;
    void lp (double fs, double f) { const double k = std::tan (kPi * std::min (f, fs * 0.45) / fs); const double n = 1 / (1 + k); b0 = float (k * n); b1 = b0; a1 = float ((k - 1) * n); }
    void hp (double fs, double f) { const double k = std::tan (kPi * std::min (f, fs * 0.45) / fs); const double n = 1 / (1 + k); b0 = float (n); b1 = -b0; a1 = float ((k - 1) * n); }
    void off() { b0 = 1; b1 = 0; a1 = 0; }
    inline float operator() (float x) { const float y = b0 * x + z; z = b1 * x - a1 * y; return y; }
    void reset() { z = 0; }
};
struct Bq             // RBJ biquad, transposed direct form II
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    enum Kind { LowShelf, HighShelf, Peak, LP, HP };
    void set (Kind k, double fs, double f, double q, double db)
    {
        f = std::min (f, fs * 0.45);
        const double A = std::pow (10.0, db / 40.0), w = 2 * kPi * f / fs, c = std::cos (w), s = std::sin (w), al = s / (2 * q);
        double B0, B1, B2, A0, A1, A2;
        switch (k)
        {
            case Peak:      B0 = 1 + al * A; B1 = -2 * c; B2 = 1 - al * A; A0 = 1 + al / A; A1 = -2 * c; A2 = 1 - al / A; break;
            case LowShelf:  { const double r = 2 * std::sqrt (A) * al; B0 = A * ((A + 1) - (A - 1) * c + r); B1 = 2 * A * ((A - 1) - (A + 1) * c); B2 = A * ((A + 1) - (A - 1) * c - r);
                              A0 = (A + 1) + (A - 1) * c + r; A1 = -2 * ((A - 1) + (A + 1) * c); A2 = (A + 1) + (A - 1) * c - r; break; }
            case HighShelf: { const double r = 2 * std::sqrt (A) * al; B0 = A * ((A + 1) + (A - 1) * c + r); B1 = -2 * A * ((A - 1) + (A + 1) * c); B2 = A * ((A + 1) + (A - 1) * c - r);
                              A0 = (A + 1) - (A - 1) * c + r; A1 = 2 * ((A - 1) - (A + 1) * c); A2 = (A + 1) - (A - 1) * c - r; break; }
            case LP:        B0 = (1 - c) / 2; B1 = 1 - c; B2 = (1 - c) / 2; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; break;
            default:        B0 = (1 + c) / 2; B1 = -(1 + c); B2 = (1 + c) / 2; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; break;
        }
        b0 = float (B0 / A0); b1 = float (B1 / A0); b2 = float (B2 / A0); a1 = float (A1 / A0); a2 = float (A2 / A0);
    }
    inline float operator() (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
};

/* ------------------------------------------------ FMV tone stack (exact) */
// Yeh & Smith, "Discretization of the '59 Fender Bassman Tone Stack" (DAFx-06).
// Treble pot R1, bass pot R2, mid pot R3, slope resistor R4, caps C1..C3.
struct ToneStackParts { double R1, R2, R3, R4, C1, C2, C3; };
struct FmvToneStack
{
    ToneStackParts p {};
    double b[4] {}, a[4] {}, z[3] {};
    void design (double fs, double t, double m, double l)
    {
        t = std::clamp (t, 0.002, 0.998); m = std::clamp (m, 0.002, 0.998); l = std::clamp (l, 0.002, 0.998);
        const double R1 = p.R1, R2 = p.R2, R3 = p.R3, R4 = p.R4, C1 = p.C1, C2 = p.C2, C3 = p.C3;
        const double b1 = t*C1*R1 + m*C3*R3 + l*(C1*R2 + C2*R2) + (C1*R3 + C2*R3);
        const double b2 = t*(C1*C2*R1*R4 + C1*C3*R1*R4) - m*m*(C1*C3*R3*R3 + C2*C3*R3*R3) + m*(C1*C3*R1*R3 + C1*C3*R3*R3 + C2*C3*R3*R3)
                        + l*(C1*C2*R1*R2 + C1*C2*R2*R4 + C1*C3*R2*R4) + l*m*(C1*C3*R2*R3 + C2*C3*R2*R3) + (C1*C2*R1*R3 + C1*C2*R3*R4 + C1*C3*R3*R4);
        const double b3 = l*m*(C1*C2*C3*R1*R2*R3 + C1*C2*C3*R2*R3*R4) - m*m*(C1*C2*C3*R1*R3*R3 + C1*C2*C3*R3*R3*R4) + m*(C1*C2*C3*R1*R3*R3 + C1*C2*C3*R3*R3*R4)
                        + t*C1*C2*C3*R1*R3*R4 - t*m*C1*C2*C3*R1*R3*R4 + t*l*C1*C2*C3*R1*R2*R4;
        const double a0 = 1;
        const double a1 = (C1*R1 + C1*R3 + C2*R3 + C2*R4 + C3*R4) + m*C3*R3 + l*(C1*R2 + C2*R2);
        const double a2 = m*(C1*C3*R1*R3 - C2*C3*R3*R4 + C1*C3*R3*R3 + C2*C3*R3*R3) + l*m*(C1*C3*R2*R3 + C2*C3*R2*R3) - m*m*(C1*C3*R3*R3 + C2*C3*R3*R3)
                        + l*(C1*C2*R2*R4 + C1*C2*R1*R2 + C1*C3*R2*R4 + C2*C3*R2*R4) + (C1*C2*R1*R4 + C1*C3*R1*R4 + C1*C2*R3*R4 + C1*C2*R1*R3 + C1*C3*R3*R4 + C2*C3*R3*R4);
        const double a3 = l*m*(C1*C2*C3*R1*R2*R3 + C1*C2*C3*R2*R3*R4) - m*m*(C1*C2*C3*R1*R3*R3 + C1*C2*C3*R3*R3*R4)
                        + m*(C1*C2*C3*R3*R3*R4 + C1*C2*C3*R1*R3*R3 - C1*C2*C3*R1*R3*R4) + l*C1*C2*C3*R1*R2*R4 + C1*C2*C3*R1*R3*R4;
        // bilinear transform: s = c (1 - z^-1) / (1 + z^-1), prewarped at 1 kHz
        const double c = 2 * kPi * 1000.0 / std::tan (kPi * 1000.0 / fs);
        const double bs[4] = { 0, b1 * c, b2 * c * c, b3 * c * c * c }, as[4] = { a0, a1 * c, a2 * c * c, a3 * c * c * c };
        // (1 - z)^k (1 + z)^(3 - k) expanded, k = 0..3
        static const double P[4][4] = { { 1, 3, 3, 1 }, { 1, 1, -1, -1 }, { 1, -1, -1, 1 }, { 1, -3, 3, -1 } };
        double B[4] = {}, A[4] = {};
        for (int k = 0; k < 4; ++k) for (int i = 0; i < 4; ++i) { B[i] += bs[k] * P[k][i]; A[i] += as[k] * P[k][i]; }
        for (int i = 0; i < 4; ++i) { b[i] = B[i] / A[0]; a[i] = A[i] / A[0]; }
    }
    inline float operator() (float xf)
    {
        const double x = xf, y = b[0] * x + z[0];
        z[0] = b[1] * x - a[1] * y + z[1];
        z[1] = b[2] * x - a[2] * y + z[2];
        z[2] = b[3] * x - a[3] * y;
        return (float) y;
    }
    void reset() { z[0] = z[1] = z[2] = 0; }
};

/* ------------------------------------------- 4x oversampling (halfband FIR) */
/** Straightforward polyphase 2x up/down built on the halfband above. */
class Over2
{
public:
    void reset() { std::fill (std::begin (ub), std::end (ub), 0.f); std::fill (std::begin (db), std::end (db), 0.f); ui = di = 0; }
    Over2()
    {
        auto I0 = [] (double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
        const int M = (N - 1) / 2; const double beta = 8.0;
        for (int n = 0; n < N; ++n)
        {
            const int k = n - M;
            const double sinc = k == 0 ? 0.5 : std::sin (kPi * k / 2.0) / (kPi * k);
            h[n] = (float) (sinc * I0 (beta * std::sqrt (1.0 - (double) (k * k) / (M * M))) / I0 (beta));
        }
        reset();
    }
    /** x -> (y0, y1) at twice the rate. Zero-stuffing + halfband, split into its two phases. */
    inline void up (float x, float& y0, float& y1)
    {
        ui = (ui + 1) & (L - 1); ub[ui] = x;
        // phase A uses even taps h[0], h[2] ... ; phase B uses odd taps (only the centre one is non-zero)
        float a = 0; for (int j = 0; j <= (N - 1) / 2; ++j) a += h[2 * j] * ub[(ui - j) & (L - 1)];
        y0 = 2.f * a;
        y1 = 2.f * h[(N - 1) / 2] * ub[(ui - (N - 1) / 4) & (L - 1)];
    }
    inline float down (float x0, float x1)
    {
        di = (di + 1) & (L - 1); db[di] = x0; di = (di + 1) & (L - 1); db[di] = x1;
        float s = 0;
        for (int n = 0; n < N; n += 2) s += h[n] * db[(di - n) & (L - 1)];
        s += h[(N - 1) / 2] * db[(di - (N - 1) / 2) & (L - 1)];
        return s;
    }
    static constexpr int N = 47;            // (N - 1) / 2 must be odd so the centre tap is the only odd one
private:
    static constexpr int L = 64;
    float h[N] {}, ub[L] {}, db[L] {};
    int ui = 0, di = 0;
};

/* --------------------------------------------------------- the model data */
struct Stage
{
    float gainDb;        // small-signal gain into this stage's tube
    float bias;          // operating point: >0 clips earlier on the top half (more even harmonics)
    float cathodeHz;     // partially bypassed cathode: bass below this has less gain
    float cathodeDb;     // ... by this much (negative)
    float couplingHz;    // coupling cap to the next stage (bass cut)
    float millerHz;      // Miller capacitance / grid stopper (treble roll-off)
};
struct AmpDef
{
    const char* key;  const char* name;  const char* basedOn;  const char* role;  const char* description;
    uint32_t tint;                   // panel colour 0xRRGGBB (nearly black, a hint of colour)
    float inputDb;                   // guitar level into the first tube
    float tightHz;                   // input coupling (high = tighter bass before the gain)
    float brightDb, brightHz;        // bright cap across the gain pot (fades as gain goes up)
    float gainRangeDb;               // gain pot range
    int   nStages;  Stage st[5];
    int   gainAfter;                 // the gain pot sits after this stage (0 = before the first)
    int   toneAfter;                 // tone stack after this stage
    float cfDrive;                   // cathode follower softness (0 = none)
    ToneStackParts ts;  float tsMakeupDb;
    float piDrive;                   // phase inverter
    float powerDb, masterRangeDb;    // power stage drive at master 0 .. +range at master 10
    float hardness;                  // power tube knee (1 soft .. 4 hard)
    float sag, sagMs;                // supply sag amount and recovery time (tube rectifier = more)
    float presenceHz, presenceDb;    // NFB presence range (+/-)
    float depthHz, depthDb;          // NFB resonance / depth range (+/-)
    float xfmrLowHz, xfmrHighHz;     // output transformer bandwidth
    float levelDb;                   // output trim so every amp lands at a similar loudness
};

/* ------------------------------------------------------------ the engine */
class AmpModel
{
public:
    static constexpr int OS = 4;
    void prepare (double sampleRate, int maxBlock)
    {
        fs = sampleRate; fso = fs * OS;
        buf.assign ((size_t) maxBlock * OS, 0.f);
        if (def) configure();
        reset();
    }
    void setModel (const AmpDef* d) { def = d; if (fs > 0) { configure(); reset(); } }
    const AmpDef* model() const { return def; }
    void reset()
    {
        o1.reset(); o2.reset(); d1.reset(); d2.reset();
        tight.reset(); bright.reset(); for (auto& s : sv) s.reset(); ts.reset();
        pres.reset(); depth.reset(); xlo.reset(); xhi.reset(); dcIn.reset(); dcOut.reset();
        sagEnv = 0; for (auto& v : last) v = -1;
    }
    /** Latency in samples at the base rate. */
    int latency() const { return 35; }      // 2 x (23 samples at 2x) + 2 x (23 at 4x) = 34.5 samples

    /** Knobs in 0..10. Audio thread, no allocation. */
    void process (float* x, int n, float gain, float bass, float mid, float treble, float presence, float dep, float master)
    {
        if (def == nullptr) return;
        updateControls (gain, bass, mid, treble, presence, dep, master);
        const AmpDef& D = *def;
        const float inG = dbToLin (D.inputDb);
        // 1) up to 4x
        for (int i = 0; i < n; ++i)
        {
            float a, b, c0, c1, c2, c3;
            o1.up (dcIn (tight (x[i] * inG)), a, b);
            o2.up (a, c0, c1); o2.up (b, c2, c3);               // same filter, consecutive samples
            float* u = &buf[(size_t) i * OS]; u[0] = c0; u[1] = c1; u[2] = c2; u[3] = c3;
        }
        // 2) preamp, tone stack, power amp at 4x
        const int m = n * OS;
        for (int i = 0; i < m; ++i) buf[(size_t) i] = runAmp (buf[(size_t) i]);
        // 3) back down
        for (int i = 0; i < n; ++i)
        {
            const float* u = &buf[(size_t) i * OS];
            const float a = d2.down (u[0], u[1]), b = d2.down (u[2], u[3]);
            x[i] = dcOut (xhi (xlo (d1.down (a, b)))) * outGain;
        }
    }

private:
    const AmpDef* def = nullptr;
    double fs = 0, fso = 0;
    Over2 o1, o2, d1, d2;           // 2x then 2x again (up), and back down
    OnePole tight, dcIn, dcOut;
    Bq bright, xlo, xhi, pres, depth;
    struct StageState { Bq cath; OnePole coup, mill; void reset() { cath.reset(); coup.reset(); mill.reset(); } };
    StageState sv[5];
    float stGain[5] {}, stOff[5] {};
    FmvToneStack ts;
    float gainPot = 1, tsMakeup = 1, piG = 1, powerG = 1, outGain = 1, sagEnv = 0, sagA = 0, sagR = 0, hardInv = 1;
    float last[7] { -1, -1, -1, -1, -1, -1, -1 };
    std::vector<float> buf;
public:
   #ifdef AMP_LAB_PROBE
    float probe[8] {};
   #endif
private:

    static inline float triode (float x)
    {
        // Grid conduction (positive side) limits early and fairly hard; cut-off (negative side) is softer and later.
        return x >= 0 ? x / std::sqrt (1.f + 2.2f * x * x) : x / std::sqrt (1.f + 0.45f * x * x);
    }
    inline float powerTube (float x) const
    {
        // push-pull pair: symmetric, knee set by "hardness"
        const float ax = std::abs (x), h = def->hardness;
        return x / std::pow (1.f + std::pow (ax, h), hardInv);
    }

    void configure()
    {
        const AmpDef& D = *def;
        tight.hp (fs, D.tightHz); dcIn.hp (fs, 8.0); dcOut.hp (fs, 10.0);
        for (int s = 0; s < D.nStages; ++s)
        {
            const Stage& S = D.st[s];
            sv[s].cath.set (Bq::LowShelf, fso, S.cathodeHz, 0.6, S.cathodeDb);
            sv[s].coup.hp (fso, S.couplingHz);
            sv[s].mill.lp (fso, S.millerHz);
            stGain[s] = dbToLin (S.gainDb);
            stOff[s] = triode (S.bias);          // removes the DC the bias point would add
        }
        ts.p = D.ts; tsMakeup = dbToLin (D.tsMakeupDb);
        piG = dbToLin (D.piDrive);
        xlo.set (Bq::HP, fs, D.xfmrLowHz, 0.6, 0); xhi.set (Bq::LP, fs, D.xfmrHighHz, 0.75, 0);
        sagA = (float) std::exp (-1.0 / (fso * 0.004)); sagR = (float) std::exp (-1.0 / (fso * D.sagMs * 0.001));
        hardInv = 1.f / D.hardness;
        for (auto& v : last) v = -1;
    }

    void updateControls (float g, float b, float mi, float t, float p, float d, float ms)
    {
        const AmpDef& D = *def;
        auto changed = [this] (int i, float v) { if (std::abs (last[i] - v) < 1e-3f) return false; last[i] = v; return true; };
        if (changed (0, g))
        {
            const float g01 = std::clamp (g / 10.f, 0.f, 1.f);
            gainPot = dbToLin (-D.gainRangeDb * (1.f - std::pow (g01, 0.55f)));     // audio-taper pot
            bright.set (Bq::HighShelf, fso, D.brightHz, 0.7, D.brightDb * (1.f - g01) * (1.f - g01));
        }
        if (changed (1, b) | changed (2, mi) | changed (3, t))
        {
            const double l = std::pow (std::clamp (b / 10.0, 0.0, 1.0), 2.2);     // bass pot is audio taper
            ts.design (fso, std::clamp (t / 10.0, 0.0, 1.0), std::clamp (mi / 10.0, 0.0, 1.0), l);
        }
        if (changed (4, p)) pres.set (Bq::HighShelf, fso, D.presenceHz, 0.6, (p - 5.f) / 5.f * D.presenceDb);
        if (changed (5, d)) depth.set (Bq::Peak, fso, D.depthHz, 0.9, (d - 5.f) / 5.f * D.depthDb);
        if (changed (6, ms))
        {
            const float m01 = std::clamp (ms / 10.f, 0.f, 1.f);
            powerG = dbToLin (D.powerDb + D.masterRangeDb * (m01 - 0.5f));
            // louder with master, but less than the drive goes up (the power stage compresses)
            outGain = dbToLin (D.levelDb + 14.f * (m01 - 0.5f));
        }
    }

    inline float runAmp (float x)
    {
        const AmpDef& D = *def;
        if (D.gainAfter == 0) x *= gainPot;
        for (int s = 0; s < D.nStages; ++s)
        {
            x = sv[s].cath (x) * stGain[s];
            x = -(triode (x + D.st[s].bias) - stOff[s]);           // every tube stage flips the phase
            x = sv[s].mill (sv[s].coup (x));
           #ifdef AMP_LAB_PROBE
            probe[s] = std::max (probe[s], std::abs (x));
           #endif
            if (s + 1 == D.gainAfter) x = bright (x) * gainPot;
            if (s + 1 == D.toneAfter)
            {
                if (D.cfDrive > 0) x = x > 0 ? x / (1.f + D.cfDrive * x) : x;     // cathode follower
                x = ts (x) * tsMakeup;
            }
        }
        // power amp: NFB tone controls, sag, push-pull tubes
       #ifdef AMP_LAB_PROBE
        probe[6] = std::max (probe[6], std::abs (x));
       #endif
        x = depth (pres (x * piG));
        x = std::tanh (x + 0.04f) - 0.03998f;                       // phase inverter, slight imbalance
        const float level = std::abs (x);
        sagEnv = level > sagEnv ? sagA * sagEnv + (1 - sagA) * level : sagR * sagEnv + (1 - sagR) * level;
        const float supply = 1.f / (1.f + D.sag * sagEnv);
        return powerTube (x * powerG * supply) / std::max (0.35f, supply * 0.7f + 0.3f);
    }
};

/* ============================================================ the amps */
inline const std::vector<AmpDef>& models()
{
    // Tone stack parts: R1 treble, R2 bass, R3 mid, R4 slope, C1, C2, C3 (ohms, farads)
    static const ToneStackParts fender65 { 250e3, 250e3, 10e3, 100e3, 250e-12, 100e-9, 47e-9 };
    static const ToneStackParts marshall { 220e3, 1e6, 25e3, 33e3, 470e-12, 22e-9, 22e-9 };
    static const ToneStackParts vox      { 1e6, 1e6, 10e3, 100e3, 50e-12, 22e-9, 22e-9 };
    static const ToneStackParts soldano  { 250e3, 1e6, 25e3, 47e3, 470e-12, 20e-9, 20e-9 };
    static const ToneStackParts peavey   { 250e3, 1e6, 25e3, 47e3, 330e-12, 22e-9, 22e-9 };
    static const ToneStackParts mesa     { 250e3, 250e3, 25e3, 47e3, 500e-12, 22e-9, 22e-9 };
    static const ToneStackParts modern   { 250e3, 1e6, 25e3, 33e3, 500e-12, 22e-9, 22e-9 };

    static const std::vector<AmpDef> list
    {
        // key, name, based on, role, description,
        // tint, inputDb, tightHz, brightDb, brightHz, gainRangeDb,
        // stages { gainDb, bias, cathodeHz, cathodeDb, couplingHz, millerHz } ...,
        // gainAfter, toneAfter, cfDrive, tone stack, tsMakeupDb,
        // piDrive, powerDb, masterRangeDb, hardness, sag, sagMs, presenceHz, presenceDb, depthHz, depthDb, xfmrLowHz, xfmrHighHz, levelDb
        { "glass65", "GLASS 65", "Fender '65 Twin Reverb", "clean",
          "Huge, glassy clean with lots of headroom. Bright-cap sparkle at low gain, scooped Fender mids.",
          0x15181d, -9.f, 20.f, 6.f, 1500.f, 30.f,
          2, { { 8.f, 0.05f, 300.f, -2.f, 20.f, 18000.f }, { 18.f, 0.03f, 300.f, -2.f, 20.f, 16000.f } },
          1, 1, 0.f, fender65, 6.f,
          6.f, -2.f, 16.f, 1.6f, 0.15f, 120.f, 3500.f, 5.f, 110.f, 3.f, 35.f, 9000.f, -1.1f },

        { "top30", "TOP 30", "Vox AC30 Top Boost", "clean · crunch",
          "Chimey class-A breakup. No negative feedback, so the top end jangles and the power amp compresses early.",
          0x1d1915, -13.f, 30.f, 6.f, 2200.f, 30.f,
          2, { { 10.f, 0.1f, 400.f, -1.5f, 35.f, 15000.f }, { 18.f, 0.12f, 400.f, -1.f, 40.f, 12000.f } },
          1, 2, 0.f, vox, 6.f,
          6.f, 3.f, 14.f, 1.4f, 0.55f, 200.f, 4500.f, 4.f, 120.f, 2.f, 60.f, 8000.f, -1.f },

        { "plexi68", "PLEXI 68", "Marshall 1959 Super Lead (jumped)", "crunch",
          "Classic rock crunch from a non-master amp: open and punchy, with more gain from the power tubes as you turn up.",
          0x1c1812, -16.f, 40.f, 8.f, 1800.f, 32.f,
          3, { { 10.f, 0.1f, 500.f, -3.f, 40.f, 14000.f }, { 14.f, 0.08f, 300.f, -2.f, 50.f, 12000.f }, { 10.f, 0.15f, 200.f, -1.f, 30.f, 10000.f } },
          1, 3, 0.4f, marshall, 4.f,
          8.f, 8.f, 18.f, 1.8f, 0.45f, 160.f, 3800.f, 6.f, 100.f, 4.f, 70.f, 7500.f, -9.7f },

        { "brit800", "BRIT 800", "Marshall JCM800 2203", "crunch · high gain",
          "The 80s rock and metal workhorse: cascaded preamp, tight mids, cutting top. Great with a boost in front.",
          0x1d1712, 0.f, 60.f, 7.f, 2000.f, 48.f,
          3, { { 12.f, 0.12f, 600.f, -4.f, 45.f, 13000.f }, { 22.f, 0.1f, 400.f, -3.f, 70.f, 11000.f }, { 18.f, 0.18f, 250.f, -1.f, 35.f, 9000.f } },
          1, 3, 0.5f, marshall, 4.f,
          6.f, 2.f, 16.f, 2.f, 0.35f, 140.f, 3600.f, 6.f, 95.f, 4.f, 70.f, 7000.f, -12.3f },

        { "slo100", "SLO 100", "Soldano SLO-100 (overdrive channel)", "high gain · lead",
          "Smooth, liquid saturation with a vocal midrange. Sings on leads and still stays tight on rhythm.",
          0x18161c, 0.f, 70.f, 5.f, 2400.f, 58.f,
          4, { { 12.f, 0.1f, 700.f, -4.f, 45.f, 12000.f }, { 26.f, 0.12f, 500.f, -3.f, 80.f, 10000.f }, { 24.f, 0.15f, 300.f, -2.f, 60.f, 9000.f }, { 20.f, 0.2f, 200.f, -1.f, 35.f, 8000.f } },
          1, 4, 0.6f, soldano, 2.f,
          6.f, 0.f, 14.f, 2.2f, 0.25f, 120.f, 3600.f, 6.f, 90.f, 5.f, 70.f, 7000.f, -10.8f },

        { "styx51", "STYX 51", "Peavey 5150 / EVH 5150 (lead channel)", "high gain",
          "Aggressive modern metal: very tight low end, saturated upper mids. Made for down-tuned riffs.",
          0x1a1414, 0.f, 110.f, 4.f, 2500.f, 60.f,
          4, { { 14.f, 0.12f, 900.f, -6.f, 70.f, 11000.f }, { 26.f, 0.14f, 600.f, -4.f, 110.f, 10000.f }, { 26.f, 0.16f, 400.f, -3.f, 80.f, 9000.f }, { 20.f, 0.22f, 250.f, -1.f, 45.f, 7500.f } },
          1, 4, 0.7f, peavey, 2.f,
          6.f, 2.f, 14.f, 2.6f, 0.2f, 100.f, 3800.f, 7.f, 85.f, 6.f, 60.f, 7200.f, -12.1f },

        { "recto", "RECTO", "Mesa/Boogie Dual Rectifier (modern)", "high gain",
          "Thick, saggy, dark modern gain with a big low end. The rectifier sag makes palm mutes bloom.",
          0x141619, 0.f, 70.f, 3.f, 2500.f, 58.f,
          4, { { 14.f, 0.12f, 600.f, -3.f, 40.f, 10000.f }, { 26.f, 0.15f, 400.f, -2.f, 60.f, 9000.f }, { 24.f, 0.18f, 300.f, -2.f, 55.f, 8000.f }, { 20.f, 0.22f, 200.f, -1.f, 30.f, 7000.f } },
          1, 4, 0.7f, mesa, 2.f,
          6.f, 3.f, 16.f, 1.8f, 0.7f, 240.f, 3200.f, 6.f, 80.f, 6.f, 45.f, 6500.f, -12.1f },

        { "vh4", "VH-4", "Diezel VH4 (channel 3)", "high gain",
          "Precise, articulate modern German gain: dense, but every note in a chord stays clear.",
          0x161816, 0.f, 90.f, 4.f, 2600.f, 58.f,
          4, { { 12.f, 0.1f, 800.f, -5.f, 60.f, 12000.f }, { 26.f, 0.12f, 500.f, -4.f, 90.f, 11000.f }, { 24.f, 0.14f, 350.f, -2.f, 70.f, 9500.f }, { 20.f, 0.18f, 220.f, -1.f, 40.f, 8500.f } },
          1, 4, 0.6f, modern, 2.f,
          6.f, 0.f, 14.f, 2.8f, 0.2f, 110.f, 4000.f, 6.f, 90.f, 5.f, 60.f, 7800.f, -11.7f },

        { "core", "OBSIDIAN CORE", "Obsidian original (modern high gain)", "high gain",
          "Obsidian's own amp: tight like a 5150, smooth like an SLO, with a clearer top end.",
          0x121214, 0.f, 95.f, 4.f, 2400.f, 60.f,
          4, { { 12.f, 0.1f, 800.f, -5.f, 65.f, 12000.f }, { 26.f, 0.12f, 500.f, -4.f, 95.f, 10500.f }, { 24.f, 0.15f, 350.f, -2.f, 70.f, 9500.f }, { 20.f, 0.2f, 220.f, -1.f, 40.f, 8000.f } },
          1, 4, 0.6f, modern, 2.f,
          6.f, 1.f, 14.f, 2.4f, 0.3f, 140.f, 3800.f, 6.f, 90.f, 5.f, 55.f, 7600.f, -12.1f },
    };
    return list;
}
inline int findModel (const char* key)
{
    const auto& l = models();
    for (size_t i = 0; i < l.size(); ++i) if (std::strcmp (l[i].key, key) == 0) return (int) i;
    return -1;
}
} // namespace obs::amp
