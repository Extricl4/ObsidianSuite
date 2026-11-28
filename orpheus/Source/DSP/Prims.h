#pragma once
/*  ORPHEUS · small DSP parts, made to behave like the Web Audio nodes the UI's preview (ui/src) was tuned on, so the
    plugin sounds like what Emil heard in the browser:
    - WABiquad: the Web Audio BiquadFilterNode formulas, including its quirk that a lowpass's or highpass's Q is in dB
    - Target: AudioParam.setTargetAtTime / linear and exponential ramps (one value moving toward another)
    - Delay: a delay line read with linear interpolation (like DelayNode)
    - Shaper: a WaveShaperNode curve (linear interpolation, the ends hold)                                           */
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace orph
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTau = 2.0 * kPi;

inline float clampf (float v, float a, float b) noexcept { return v < a ? a : (v > b ? b : v); }
inline double dbToGain (double db) noexcept { return std::pow (10.0, db / 20.0); }
inline float tcCoef (double tcSec, double sr) noexcept { return tcSec <= 0 ? 1.f : (float) (1.0 - std::exp (-1.0 / (tcSec * sr))); }

/* ------------------------------------------------------------ Web Audio biquad */
struct WABiquad
{
    enum Type { LP, HP, BP, NOTCH, AP, PEAK };
    Type type = LP;
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    double sr = 48000;

    void reset() noexcept { x1 = x2 = y1 = y2 = 0; }
    /** q: the value Web Audio's Q would hold (dB for LP/HP, plain otherwise); gainDb for PEAK */
    void set (Type t, double f, double q, double gainDb = 0, double detuneCents = 0) noexcept
    {
        type = t;
        if (detuneCents != 0) f *= std::pow (2.0, detuneCents / 1200.0);
        const double nyq = sr * 0.5;
        f = std::max (1e-3, std::min (f, nyq * 0.9999));
        const double w0 = kTau * f / sr, c = std::cos (w0), s = std::sin (w0);
        double B0, B1, B2, A0, A1, A2;
        switch (t)
        {
            case LP: { const double al = s / (2.0 * std::pow (10.0, q / 20.0)); B0 = (1 - c) / 2; B1 = 1 - c; B2 = (1 - c) / 2; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; break; }
            case HP: { const double al = s / (2.0 * std::pow (10.0, q / 20.0)); B0 = (1 + c) / 2; B1 = -(1 + c); B2 = (1 + c) / 2; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; break; }
            case BP: { const double al = s / (2.0 * std::max (1e-4, q)); B0 = al; B1 = 0; B2 = -al; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; break; }
            case NOTCH: { const double al = s / (2.0 * std::max (1e-4, q)); B0 = 1; B1 = -2 * c; B2 = 1; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; break; }
            case AP: { const double al = s / (2.0 * std::max (1e-4, q)); B0 = 1 - al; B1 = -2 * c; B2 = 1 + al; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; break; }
            default: { const double A = std::pow (10.0, gainDb / 40.0), al = s / (2.0 * std::max (1e-4, q));
                       B0 = 1 + al * A; B1 = -2 * c; B2 = 1 - al * A; A0 = 1 + al / A; A1 = -2 * c; A2 = 1 - al / A; break; }
        }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    /** a filter set up with a "plain" Q for every type (sirenfx.js's filt(): plain Q, converted to dB for LP/HP) */
    void setPlain (Type t, double f, double qPlain, double gainDb = 0, double detune = 0) noexcept
    {
        set (t, f, (t == LP || t == HP) ? 20.0 * std::log10 (std::max (1e-4, qPlain)) : qPlain, gainDb, detune);
    }
    inline float process (float x) noexcept
    {
        const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = (std::abs (y) < 1e-30 ? 0.0 : y);
        return (float) y;
    }
};

/* ------------------------------------------------------------ a value that moves like an AudioParam */
struct Target
{
    // modes: hold, approach (setTargetAtTime), linear ramp, exponential ramp
    enum Mode { HOLD, APPROACH, LIN, EXP };
    float v = 0, target = 0, coef = 1;
    double step = 0, ratio = 1; int left = 0; Mode mode = HOLD;
    float pendingTarget = 0, pendingCoef = 1; int delay = 0; bool pending = false;   // an approach that starts later

    void setNow (float x) noexcept { v = target = x; mode = HOLD; pending = false; left = 0; }
    void approach (float to, double tcSec, double sr, double startInSec = 0) noexcept
    {
        if (startInSec > 0) { pending = true; pendingTarget = to; pendingCoef = tcCoef (tcSec, sr); delay = (int) std::lround (startInSec * sr); return; }
        pending = false; mode = APPROACH; target = to; coef = tcCoef (tcSec, sr);
    }
    void linTo (float to, double sec, double sr) noexcept
    {
        pending = false; left = std::max (1, (int) std::lround (sec * sr)); mode = LIN; target = to; step = (to - v) / left;
    }
    void expTo (float to, double sec, double sr) noexcept
    {
        pending = false; left = std::max (1, (int) std::lround (sec * sr)); mode = EXP; target = to;
        const double a = std::max (1e-6, (double) v), b = std::max (1e-6, (double) to);
        v = (float) a; ratio = std::pow (b / a, 1.0 / left);
    }
    /** an exponential ramp over a number of samples (from where it is now) */
    void expOver (float to, int samples) noexcept
    {
        pending = false; left = std::max (1, samples); mode = EXP; target = to;
        const double a = std::max (1e-6, (double) v), b = std::max (1e-6, (double) to);
        v = (float) a; ratio = std::pow (b / a, 1.0 / left);
    }
    inline float next() noexcept
    {
        if (pending && --delay <= 0) { pending = false; mode = APPROACH; target = pendingTarget; coef = pendingCoef; }
        switch (mode)
        {
            case APPROACH: v += (target - v) * coef; break;
            case LIN: v = (float) (v + step); if (--left <= 0) { v = target; mode = HOLD; } break;
            case EXP: v = (float) (v * ratio); if (--left <= 0) { v = target; mode = HOLD; } break;
            default: break;
        }
        return v;
    }
    bool busy() const noexcept { return mode == LIN || mode == EXP; }
};

/* ------------------------------------------------------------ delay line (linear interpolation, like DelayNode) */
struct Delay
{
    std::vector<float> buf; int mask = 0, w = 0;
    void init (double maxSec, double sr)
    {
        int n = 1; const int need = (int) std::ceil (maxSec * sr) + 4;
        while (n < need) n <<= 1;
        buf.assign ((size_t) n, 0.f); mask = n - 1; w = 0;
    }
    void clear() noexcept { std::fill (buf.begin(), buf.end(), 0.f); }
    inline void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }
    /** d in samples (>= 0), read after push: d = 0 is the sample just written */
    inline float read (double d) const noexcept
    {
        d = std::max (0.0, std::min (d, (double) mask - 2));
        const double pos = (double) w - 1 - d; const int i = (int) std::floor (pos); const float f = (float) (pos - i);
        const float a = buf[(size_t) (i & mask)], b = buf[(size_t) ((i + 1) & mask)];
        return a + (b - a) * f;
    }
};

/* ------------------------------------------------------------ waveshaper curve */
struct Shaper
{
    std::vector<float> c;
    template <typename Fn> void build (Fn fn, int n)
    {
        c.resize ((size_t) n);
        for (int i = 0; i < n; ++i) c[(size_t) i] = (float) fn ((double) i / (n - 1) * 2.0 - 1.0);
    }
    inline float operator() (float x) const noexcept
    {
        const int n = (int) c.size();
        const double p = (std::max (-1.0, std::min (1.0, (double) x)) + 1.0) * 0.5 * (n - 1);
        const int i = std::min (n - 2, (int) p); const float f = (float) (p - i);
        return c[(size_t) i] + (c[(size_t) i + 1] - c[(size_t) i]) * f;
    }
};

/* ------------------------------------------------------------ 2x oversampling around a shaper (half-band IIR, light) */
struct Os2
{
    // a 7th-order elliptic-ish polyphase approximation is overkill here: two cascaded biquad lowpasses at 0.45 fs
    WABiquad up1, up2, dn1, dn2;
    void init (double sr)
    {
        for (auto* b : { &up1, &up2, &dn1, &dn2 }) { b->sr = sr * 2; b->set (WABiquad::LP, sr * 0.45, -3.0103); b->reset(); }
    }
    template <typename Fn> inline float run (float x, Fn&& fn) noexcept
    {
        const float a = up2.process (up1.process (x * 2.f)), b = up2.process (up1.process (0.f));
        const float ya = dn2.process (dn1.process (fn (a))); dn2.process (dn1.process (fn (b)));
        return ya;
    }
};

/* ------------------------------------------------------------ white noise */
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    inline float white() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) ((double) s / 2147483648.0 - 1.0); }
    inline float uni() noexcept { return (white() + 1.f) * 0.5f; }
};

/* ------------------------------------------------------------ equal-power pan (StereoPannerNode, mono in) */
inline void panGains (float pan, float& l, float& r) noexcept
{
    const double x = (std::max (-1.0f, std::min (1.0f, pan)) + 1.0) * 0.5 * kPi * 0.5;
    l = (float) std::cos (x); r = (float) std::sin (x);
}
} // namespace orph
