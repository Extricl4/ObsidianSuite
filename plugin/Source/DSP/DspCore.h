#pragma once
// =============================================================================
//  Small, allocation-free DSP building blocks shared by every effect.
//  All process() calls are real-time safe: no locks, no allocation, no I/O.
// =============================================================================
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>
#include <array>
#include <algorithm>
#include <complex>
#include <cstdint>

namespace obs::dsp
{
constexpr float kPi = juce::MathConstants<float>::pi;
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

inline float dbToGain (float db)          { return db <= -120.f ? 0.f : std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)           { return g <= 1.0e-6f ? -120.f : 20.f * std::log10 (g); }
inline float clampf (float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float lerp (float a, float b, float t)   { return a + (b - a) * t; }
inline bool  isBad (float v)              { return ! std::isfinite (v) || std::abs (v) > 1.0e6f; }

// Smooth, cheap saturators.
inline float softClip (float x)           { return x / (1.0f + std::abs (x)); }
inline float fastTanh (float x)
{
    x = clampf (x, -4.97f, 4.97f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/** Linear ramp toward a target over N samples. */
class Smoother
{
public:
    void reset (double sr, float timeMs, float initial)  { steps = juce::jmax (1, (int) (sr * timeMs * 0.001)); cur = target = initial; left = 0; }
    void set (float t)                    { if (t != target) { target = t; left = steps; inc = (target - cur) / (float) steps; } }
    void snap (float t)                   { cur = target = t; left = 0; }
    inline float next()                   { if (left > 0) { cur += inc; if (--left == 0) cur = target; } return cur; }
    float current() const                 { return cur; }
    float targetValue() const             { return target; }
    bool  isSmoothing() const             { return left > 0; }
private:
    float cur = 0, target = 0, inc = 0; int steps = 1, left = 0;
};

/** Click-free on / off for a whole section: while it switches, the processed signal is crossfaded
    with the dry one (15 ms). Fully off = the section isn't processed at all. */
class SectionFade
{
public:
    void reset (double sr, bool on)      { sm.reset (sr, 15.f, on ? 1.f : 0.f); }
    void set (bool on)                    { sm.set (on ? 1.f : 0.f); }
    bool active() const                   { return sm.isSmoothing() || sm.current() > 0.f; }   // process the section
    bool full() const                     { return ! sm.isSmoothing() && sm.current() >= 1.f; } // no dry needed
    /** wet = dry + g (wet - dry), sample by sample (call only when active and not full). */
    void mix (const float* dryL, const float* dryR, float* L, float* R, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float g = sm.next();
            L[i] = dryL[i] + g * (L[i] - dryL[i]);
            if (R != nullptr) R[i] = dryR[i] + g * (R[i] - dryR[i]);
        }
    }
    /** Advance without mixing (fills gains: for sections that mix themselves). */
    void gains (float* g, int n)          { for (int i = 0; i < n; ++i) g[i] = sm.next(); }
private:
    Smoother sm;
};

/** One-pole lowpass / highpass (6 dB/oct). */
class OnePole
{
public:
    void setLowpass (double sr, float hz)  { a = std::exp (-kTwoPi * clampf (hz, 1.f, (float) sr * 0.49f) / (float) sr); }
    void reset (float v = 0.f)             { z = v; }
    inline float lp (float x)              { z = x + a * (z - x); return z; }
    inline float hp (float x)              { return x - lp (x); }
    float a = 0.f, z = 0.f;
};

/** RBJ biquad, transposed direct form II. */
class Biquad
{
public:
    enum Type { LP, HP, BP, Notch, Peak, LowShelf, HighShelf, AllPass };

    void set (Type t, double sr, float f, float q, float gainDb = 0.f)
    {
        f = clampf (f, 10.f, (float) sr * 0.495f);
        q = juce::jmax (0.05f, q);
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = kTwoPi * f / (float) sr, cw = std::cos (w), sw = std::sin (w), al = sw / (2.0f * q);
        float b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
        switch (t)
        {
            case LP:   b0 = (1 - cw) * 0.5f; b1 = 1 - cw; b2 = b0; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; break;
            case HP:   b0 = (1 + cw) * 0.5f; b1 = -(1 + cw); b2 = b0; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; break;
            case BP:   b0 = al; b1 = 0; b2 = -al; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; break;
            case Notch:b0 = 1; b1 = -2 * cw; b2 = 1; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; break;
            case AllPass: b0 = 1 - al; b1 = -2 * cw; b2 = 1 + al; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; break;
            case Peak: b0 = 1 + al * A; b1 = -2 * cw; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * cw; a2 = 1 - al / A; break;
            case LowShelf:
            {
                const float s = 2 * std::sqrt (A) * al;
                b0 = A * ((A + 1) - (A - 1) * cw + s); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - s);
                a0 = (A + 1) + (A - 1) * cw + s; a1 = -2 * ((A - 1) + (A + 1) * cw); a2 = (A + 1) + (A - 1) * cw - s; break;
            }
            case HighShelf:
            {
                const float s = 2 * std::sqrt (A) * al;
                b0 = A * ((A + 1) + (A - 1) * cw + s); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - s);
                a0 = (A + 1) - (A - 1) * cw + s; a1 = 2 * ((A - 1) - (A + 1) * cw); a2 = (A + 1) - (A - 1) * cw - s; break;
            }
        }
        const float inv = 1.0f / a0;
        c0 = b0 * inv; c1 = b1 * inv; c2 = b2 * inv; d1 = a1 * inv; d2 = a2 * inv;
    }
    void reset()                     { z1 = z2 = 0; }
    inline float process (float x)
    {
        const float y = c0 * x + z1;
        z1 = c1 * x - d1 * y + z2;
        z2 = c2 * x - d2 * y;
        return y;
    }
    // Magnitude response (for UI curves / tests).
    float magnitude (double sr, float f) const
    {
        const double w = juce::MathConstants<double>::twoPi * f / sr;
        const std::complex<double> z = std::polar (1.0, -w), z2c = z * z;
        const auto num = (double) c0 + (double) c1 * z + (double) c2 * z2c;
        const auto den = 1.0 + (double) d1 * z + (double) d2 * z2c;
        return (float) std::abs (num / den);
    }
    float c0 = 1, c1 = 0, c2 = 0, d1 = 0, d2 = 0, z1 = 0, z2 = 0;
};

/** Zavalishin TPT state-variable filter: stable under fast modulation. */
class Svf
{
public:
    void setSampleRate (double s)    { sr = (float) s; }
    void set (float f, float q)
    {
        f = clampf (f, 10.f, sr * 0.49f);
        g = std::tan (kPi * f / sr); k = 1.0f / juce::jmax (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void reset()                     { ic1 = ic2 = 0; }
    inline void tick (float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        low = v2; band = v1; high = x - k * v1 - v2;
    }
    float low = 0, band = 0, high = 0;
private:
    float sr = 48000, g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
};

/** Fractional delay line with linear / cubic read. Size is fixed at prepare (power of two). */
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        int n = 1; while (n < maxSamples + 4) n <<= 1;
        buf.assign ((size_t) n, 0.f); mask = n - 1; w = 0;
    }
    void clear()                     { std::fill (buf.begin(), buf.end(), 0.f); }
    int  size() const                { return (int) buf.size(); }
    inline void push (float x)       { buf[(size_t) w] = x; w = (w + 1) & mask; }
    inline float at (int d) const    { return buf[(size_t) ((w - 1 - d) & mask)]; }
    inline float readLinear (float d) const
    {
        d = clampf (d, 0.f, (float) (mask - 2));
        const int i = (int) d; const float fr = d - (float) i;
        return at (i) + (at (i + 1) - at (i)) * fr;
    }
    inline float readCubic (float d) const
    {
        d = clampf (d, 1.f, (float) (mask - 3));
        const int i = (int) d; const float t = d - (float) i;
        const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
        const float c0 = y1, c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + c0;
    }
private:
    std::vector<float> buf { 0.f, 0.f, 0.f, 0.f };
    int mask = 3, w = 0;
};

/** Schroeder allpass for diffusion. */
class Allpass
{
public:
    void prepare (int maxLen)        { line.prepare (maxLen); }
    void clear()                     { line.clear(); }
    inline float process (float x, float d, float g)
    {
        const float del = line.readLinear (d);
        const float v = x + g * del;
        line.push (v);
        return del - g * v;
    }
private:
    DelayLine line;
};

class Lfo
{
public:
    enum Shape { Sine, Triangle, Square, Saw, SampleHold };
    void setSampleRate (double s)    { sr = (float) s; }
    void setRate (float hz)          { inc = hz / sr; }
    void setPhase (float p)          { ph = p - std::floor (p); }
    float phase() const              { return ph; }
    inline float next (Shape s = Sine)
    {
        float out = 0;
        switch (s)
        {
            case Sine:     out = std::sin (kTwoPi * ph); break;
            case Triangle: out = 4.f * std::abs (ph - 0.5f) - 1.f; break;
            case Square:   out = ph < 0.5f ? 1.f : -1.f; break;
            case Saw:      out = 2.f * ph - 1.f; break;
            case SampleHold: out = held; break;
        }
        ph += inc;
        if (ph >= 1.f) { ph -= 1.f; held = rng.nextFloat() * 2.f - 1.f; }
        return out;
    }
private:
    float sr = 48000, ph = 0, inc = 0, held = 0;
    juce::Random rng;
};

/** Peak envelope follower with separate attack / release. */
class EnvFollower
{
public:
    void set (double sr, float attackMs, float releaseMs)
    {
        at = std::exp (-1.0f / (float) (juce::jmax (0.01, attackMs * 0.001 * sr)));
        rt = std::exp (-1.0f / (float) (juce::jmax (0.01, releaseMs * 0.001 * sr)));
    }
    inline float process (float x)
    {
        const float a = std::abs (x);
        env = a > env ? at * (env - a) + a : rt * (env - a) + a;
        return env;
    }
    void reset()                      { env = 0; }
    float value() const               { return env; }
private:
    float at = 0, rt = 0, env = 0;
};

/** Removes DC so feedback loops and asymmetric clippers never drift. */
class DcBlocker
{
public:
    void prepare (double sr)          { r = 1.0f - (kTwoPi * 10.0f / (float) sr); }
    inline float process (float x)    { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
    void reset()                      { x1 = y1 = 0; }
private:
    float r = 0.995f, x1 = 0, y1 = 0;
};

/** Hann-window crossfade helper used by pitch shifters / grain engines. */
inline float hann (float t)           { return 0.5f - 0.5f * std::cos (kTwoPi * t); }

/** Very cheap xorshift noise in [-1, 1]. */
class Noise
{
public:
    inline float next()               { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.0f / 2147483648.0f); }
private:
    uint32_t s = 0x9E3779B9u;
};

/** Tempo-synced note values used by delays, LFOs and the arpeggiator. */
struct NoteDivisions
{
    static const juce::StringArray& names()
    {
        static const juce::StringArray n { "1/1", "1/2", "1/2T", "1/4", "1/4.", "1/4T", "1/8", "1/8.", "1/8T", "1/16", "1/16.", "1/16T", "1/32" };
        return n;
    }
    /** Length of a division in quarter notes. */
    static double beats (int idx)
    {
        static const double b[] = { 4.0, 2.0, 4.0 / 3.0, 1.0, 1.5, 2.0 / 3.0, 0.5, 0.75, 1.0 / 3.0, 0.25, 0.375, 1.0 / 6.0, 0.125 };
        return b[juce::jlimit (0, 12, idx)];
    }
};
} // namespace obs::dsp
