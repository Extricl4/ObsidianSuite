#pragma once
// =============================================================================
//  GuitarSynth: a monophonic synth played by the guitar.
//   * pitch from the shared PitchTracker, dynamics from an envelope follower
//   * 2 oscillators (sine/tri/saw/square/pulse/noise), each with 3-voice
//     unison (detune + stereo width), semitone and fine detune
//   * filter 6/12/18/24 dB with resonance, drive and an envelope
//   * amp envelope (decay -> sustain), retrigger on new notes or legato
//   * arpeggiator: scale degrees of the key, 5 patterns, free or tempo synced
// =============================================================================
#include "DspCore.h"
#include "PitchTracker.h"

namespace obs::dsp
{
struct SynthParams
{
    float gateDb = -66, sens = 55, glideMs = 60, mix = 60, outDb = 0, decayMs = 1300, sustain = 90;
    struct Osc { bool on = true; int wave = 2; int semi = 0; float det = 0, level = 0.6f, uWidth = 100, uDet = 10; } osc[2];
    bool fltOn = true; float cutoff = 4480, res = 0, attackMs = 1, drive = 0, envAmt = 0; int slope = 1;
    bool retrig = true;
    bool arpOn = false; bool steps[8] { true, false, false, false, false, false, false, false };
    int key = 0, scale = 0, octave = 0, pattern = 0; float rate = 0.56f; bool sync = false;
    float tuneHz = 440;
    double bpm = 120; bool hostPlaying = false; double ppq = 0;
};

/** Zero-delay-feedback 4-pole ladder (Moog style) with input saturation.
    Stays stable at any cutoff/resonance; resonance near max self-oscillates gently. */
class Ladder
{
public:
    void setSampleRate (double s)       { sr = (float) s; }
    void set (float fc, float res01)
    {
        const float g = std::tan (kPi * clampf (fc, 20.f, sr * 0.45f) / sr);
        G = g / (1.f + g);
        k = clampf (res01, 0.f, 1.f) * 3.95f;
    }
    void reset()                        { for (auto& v : s) v = 0; y3 = y4 = 0; }
    inline float process (float x, bool fourPole, float drive)
    {
        const float b1 = (1 - G) * s[0], b2 = (1 - G) * s[1], b3 = (1 - G) * s[2], b4 = (1 - G) * s[3];
        const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
        const float S = G3 * b1 + G2 * b2 + G * b3 + b4;
        const float est = (G4 * x + S) / (1.f + k * G4);
        float u = x - k * est;
        u = fastTanh (u * drive) / drive;                       // the ladder's input transistor pair
        float in = u;
        for (int i = 0; i < 4; ++i)
        {
            const float v = (in - s[i]) * G;
            const float y = v + s[i];
            s[i] = y + v;
            in = y;
            if (i == 2) y3 = y;
        }
        y4 = in;
        const float comp = 1.f + k * 0.45f;                     // keep the level when resonance rises
        return (fourPole ? y4 : y3) * comp;
    }
private:
    float sr = 48000, G = 0.5f, k = 0; float s[4] {}; float y3 = 0, y4 = 0;
};

class GuitarSynth
{
public:
    void prepare (double s)
    {
        sr = s;
        envFast.set (s, 1.f, 25.f); envSlow.set (s, 20.f, 160.f);
        for (auto& v : voices) for (auto& u : v) { u.phase = rng.nextFloat(); u.tri = 0; }
        for (auto& f : svfL) f.setSampleRate (s);
        for (auto& f : svfR) f.setSampleRate (s);
        ladL.setSampleRate (s); ladR.setSampleRate (s);
        dcL.prepare (s); dcR.prepare (s);
        reset();
    }
    void reset()
    {
        envFast.reset(); envSlow.reset();
        ampEnv = 0; fltEnv = 0; noteGain = 0; curMidi = -1; targetMidi = -1; gateOpen = false;
        arpPos = 0; arpCounter = 0; arpIndex = 0; arpDir = 1; onsetHold = 0;
        for (auto& f : svfL) f.reset(); for (auto& f : svfR) f.reset();
        ladL.reset(); ladR.reset();
        for (auto& d : drift) d = 0;
        opL.reset(); opR.reset(); dcL.reset(); dcR.reset();
    }

    /** in: mono guitar (read only). outL/outR: synth signal (overwritten). */
    void process (const float* in, float* outL, float* outR, int n, const SynthParams& p, const PitchTracker& pt)
    {
        const float openLin = dbToGain (p.gateDb), closeLin = dbToGain (p.gateDb - 4.f);
        const float sensGain = dbToGain (p.sens * 0.36f);
        const float glideCoef = p.glideMs < 1.f ? 0.f : std::exp (-1.f / (float) (p.glideMs * 0.001 * sr * 0.33));
        const float decayTau = juce::jmax (1.f, p.decayMs / 3.f) * 0.001f * (float) sr;
        const float decayCoef = std::exp (-1.f / decayTau);
        const float sus = p.sustain * 0.01f;
        const float attackStep = 1.f / juce::jmax (1.f, p.attackMs * 0.001f * (float) sr);
        const float outGain = dbToGain (p.outDb);
        const float tuneRatio = p.tuneHz / 440.f;

        // Detected note (refreshed per block).
        const float f0 = pt.frequency(), conf = pt.confidence();
        if (f0 > 0 && conf > 0.6f)
            detectedMidi = PitchTracker::toMidi (f0 * tuneRatio);

        // Arp step length in samples.
        double stepSamples;
        if (p.sync) stepSamples = NoteDivisions::beats ((int) std::round (p.rate * 12.f)) * 60.0 / juce::jlimit (20.0, 400.0, p.bpm) * sr;
        else        stepSamples = sr / (0.1 * std::pow (200.0, (double) p.rate));
        stepSamples = juce::jmax (64.0, stepSamples);

        // Filter settings (once per block, cutoff modulated per 32 samples below).
        const float q = 0.707f + p.res * 0.01f * 11.f;
        const float driveGain = 1.f + p.drive * 0.01f * 14.f;
        const float driveNorm = 1.f / fastTanh (driveGain);

        for (int i = 0; i < n; ++i)
        {
            const float x = in[i];
            const float ef = envFast.process (x), es = envSlow.process (x);

            // Gate with hysteresis.
            if (! gateOpen && ef > openLin) { gateOpen = true; triggerNote (p, true); }
            else if (gateOpen && ef < closeLin) gateOpen = false;

            // Onset: a fresh pick attack while already sounding.
            if (onsetHold > 0) --onsetHold;
            else if (gateOpen && ef > es * 1.9f && ef > openLin * 1.5f) { onsetHold = (int) (0.06 * sr); if (p.retrig && ! p.arpOn) triggerNote (p, false); }

            // Pitch target.
            if (p.arpOn && gateOpen && detectedMidi > 0)
            {
                arpCounter += 1.0;
                if (arpCounter >= stepSamples) { arpCounter -= stepSamples; advanceArp (p); if (p.retrig) { ampEnv = 1.f; fltEnv = 0.f; } }
            }
            else if (detectedMidi > 0)
            {
                // Follow the guitar; retrigger on a clear note change.
                if (p.retrig && curMidi > 0 && std::abs (detectedMidi - lastFollowMidi) > 0.8f && gateOpen)
                { ampEnv = 1.f; fltEnv = 0.f; }
                lastFollowMidi = detectedMidi;
                targetMidi = detectedMidi;
            }
            if (targetMidi > 0)
            {
                if (curMidi < 0) curMidi = targetMidi;
                curMidi = glideCoef > 0 ? targetMidi + (curMidi - targetMidi) * glideCoef : targetMidi;
            }

            // Envelopes.
            ampEnv = sus + (ampEnv - sus) * decayCoef;
            fltEnv = juce::jmin (1.f, fltEnv + attackStep);
            const float dyn = juce::jmin (1.f, std::sqrt (ef * sensGain));
            const float target = gateOpen && curMidi > 0 ? dyn * ampEnv : 0.f;
            noteGain += (target - noteGain) * (target > noteGain ? 0.02f : 0.0015f);

            float sl = 0, sr_ = 0;
            if (curMidi > 0 && noteGain > 1.0e-5f)
            {
                const float baseHz = PitchTracker::toHz (curMidi);
                renderOscs (p, baseHz, sl, sr_);
                // Filter.
                if (p.fltOn)
                {
                    if ((i & 31) == 0)
                    {
                        const float envOct = p.envAmt * 0.01f * 4.f * fltEnv;
                        const float fc = clampf (p.cutoff * std::pow (2.f, envOct), 20.f, 20000.f);
                        svfL[1].set (fc, q); svfR[1].set (fc, q);
                        opL.setLowpass (sr, fc); opR.setLowpass (sr, fc);
                        ladL.set (fc, p.res * 0.01f); ladR.set (fc, p.res * 0.01f);
                    }
                    if (p.slope >= 2)
                    {   // 18 / 24 dB: ladder, its own saturation does the drive
                        const float dr = 1.f + p.drive * 0.01f * 6.f;
                        sl = ladL.process (sl, p.slope == 3, dr); sr_ = ladR.process (sr_, p.slope == 3, dr);
                    }
                    else
                    {
                        if (p.drive > 0.5f) { sl = fastTanh (sl * driveGain) * driveNorm; sr_ = fastTanh (sr_ * driveGain) * driveNorm; }
                        sl = filt (sl, svfL, opL, p.slope); sr_ = filt (sr_, svfR, opR, p.slope);
                    }
                }
                sl *= noteGain * outGain; sr_ *= noteGain * outGain;
            }
            outL[i] = dcL.process (sl); outR[i] = dcR.process (sr_);
        }
        activity.store (noteGain);
    }

    float currentActivity() const { return activity.load (std::memory_order_relaxed); }

private:
    struct Voice { float phase = 0, tri = 0; };

    static float filt (float x, Svf* s, OnePole& op, int slope)
    {
        switch (slope)
        {
            case 0: return op.lp (x);
            case 1: s[1].tick (x); return s[1].low;
            case 2: s[1].tick (op.lp (x)); return s[1].low;
            default: s[0].tick (x); s[1].tick (s[0].low); return s[1].low;
        }
    }

    void triggerNote (const SynthParams& p, bool fromSilence)
    {
        ampEnv = 1.f; fltEnv = 0.f;
        if (fromSilence) { arpCounter = 0; arpIndex = 0; arpDir = 1; if (p.arpOn) advanceArp (p, true); }
    }

    // Scale degree -> semitone offset.
    static int scaleStep (int scale, int degree)
    {
        static const int maj[7] = { 0, 2, 4, 5, 7, 9, 11 }, mnr[7] = { 0, 2, 3, 5, 7, 8, 10 };
        const int oct = (int) std::floor (degree / 7.0);
        const int d = degree - oct * 7;
        return (scale == 0 ? maj[d] : mnr[d]) + 12 * oct;
    }

    void advanceArp (const SynthParams& p, bool restart = false)
    {
        int sel[8]; int count = 0;
        for (int s = 0; s < 8; ++s) if (p.steps[s]) sel[count++] = s;
        if (count == 0) { sel[0] = 0; count = 1; }

        int idx;
        if (restart) { arpIndex = (p.pattern == 1 || p.pattern == 3) ? count - 1 : 0; arpDir = (p.pattern == 1 || p.pattern == 3) ? -1 : 1; idx = arpIndex; }
        else
        {
            switch (p.pattern)
            {
                case 0: arpIndex = (arpIndex + 1) % count; break;
                case 1: arpIndex = (arpIndex - 1 + count) % count; break;
                case 2: case 3:
                    if (count == 1) { arpIndex = 0; break; }
                    arpIndex += arpDir;
                    if (arpIndex >= count) { arpIndex = count - 2; arpDir = -1; }
                    if (arpIndex < 0)      { arpIndex = 1; arpDir = 1; }
                    break;
                default: arpIndex = rng.nextInt (count); break;
            }
            idx = arpIndex;
        }
        // Snap the played note into the key, then walk scale degrees from it.
        const int played = (int) std::round (detectedMidi);
        const int rel = ((played - p.key) % 12 + 12) % 12;
        int deg = 0, best = 99;
        for (int d = 0; d < 7; ++d) { const int diff = std::abs (scaleStep (p.scale, d) - rel); if (diff < best) { best = diff; deg = d; } }
        const int base = played - rel + scaleStep (p.scale, deg);
        const int degreeOffset = sel[idx];                 // 0 = root ... 7 = octave
        const int note = base - scaleStep (p.scale, deg) + scaleStep (p.scale, deg + degreeOffset) + 12 * (p.octave - 2);
        targetMidi = (float) note;
    }

    inline float waveSample (int wave, Voice& v, float inc)
    {
        float t = v.phase, out = 0;
        auto blep = [inc] (float ph) -> float
        {
            if (ph < inc) { const float x = ph / inc; return x + x - x * x - 1.f; }
            if (ph > 1.f - inc) { const float x = (ph - 1.f) / inc; return x * x + x + x + 1.f; }
            return 0.f;
        };
        switch (wave)
        {
            case 0: out = std::sin (kTwoPi * t); break;
            case 1:
            {
                float sq = (t < 0.5f ? 1.f : -1.f) + blep (t) - blep (std::fmod (t + 0.5f, 1.f));
                v.tri = v.tri * 0.999f + sq * inc * 4.f; out = v.tri; break;
            }
            case 2: out = (2.f * t - 1.f) - blep (t); break;
            case 3: out = (t < 0.5f ? 1.f : -1.f) + blep (t) - blep (std::fmod (t + 0.5f, 1.f)); out *= 0.8f; break;
            case 4: out = (t < 0.25f ? 1.f : -1.f) + blep (t) - blep (std::fmod (t + 0.75f, 1.f)); out *= 0.8f; break;
            default: out = noise.next() * 0.7f; break;
        }
        v.phase += inc; if (v.phase >= 1.f) v.phase -= 1.f;
        return out;
    }

    void renderOscs (const SynthParams& p, float baseHz, float& l, float& r)
    {
        for (int o = 0; o < 2; ++o)
        {
            const auto& os = p.osc[o];
            if (! os.on || os.level <= 0.0001f) continue;
            const float g = os.level * os.level;
            const float centre = (float) os.semi + os.det * 0.005f;          // +-100% = +-0.5 semitone
            const float spread = os.uDet * 0.005f;                           // 100% = +-0.5 semitone
            const float width = os.uWidth * 0.01f;
            for (int u = 0; u < 3; ++u)
            {
                const float off = (float) (u - 1);
                // slow analog drift (random walk, about +-4 cents) so it never sounds sterile
                float& d = drift[o * 3 + u];
                d = clampf (d + noise.next() * 0.00004f, -0.04f, 0.04f) * 0.99999f;
                const float hz = baseHz * std::pow (2.f, (centre + off * spread + d) / 12.f);
                const float inc = clampf (hz / (float) sr, 0.f, 0.45f);
                const float s = waveSample (os.wave, voices[o][u], inc) * g * 0.45f;
                const float pan = off * width;                               // -1..1
                const float gl = std::sqrt (0.5f * (1.f - pan)), gr = std::sqrt (0.5f * (1.f + pan));
                l += s * gl; r += s * gr;
            }
        }
    }

    double sr = 48000;
    EnvFollower envFast, envSlow;
    Voice voices[2][3];
    Svf svfL[2], svfR[2];
    OnePole opL, opR;
    Ladder ladL, ladR;
    float drift[6] {};
    DcBlocker dcL, dcR;
    Noise noise;
    juce::Random rng;
    float ampEnv = 0, fltEnv = 0, noteGain = 0, curMidi = -1, targetMidi = -1, detectedMidi = -1, lastFollowMidi = -1;
    bool gateOpen = false;
    double arpCounter = 0; int arpPos = 0, arpIndex = 0, arpDir = 1, onsetHold = 0;
    std::atomic<float> activity { 0.f };
};
} // namespace obs::dsp
