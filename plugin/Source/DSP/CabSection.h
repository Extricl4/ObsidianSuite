#pragma once
// =============================================================================
//  CabSection: the CAB tab. Two mics (A and B), each with its own impulse
//  response, mic model, position, distance, level, pan, solo/mute/phase.
//   * IRs come from the cab library or "Load IR..." (any .wav)
//   * with no IR loaded, a built-in speaker-cab response is generated, so
//     there's always a cab sound
//   * distance also adds the real time-of-flight delay between the mics, which
//     gives you the natural phase interaction of a two-mic setup
// =============================================================================
#include "DspCore.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>

namespace obs::dsp
{
class CabSection
{
public:
    enum BuiltIn { Cab1x12 = 0, Cab2x12 = 1, Cab4x12 = 2 };

    void prepare (double s, int maxBlock)
    {
        sr = s;
        juce::dsp::ProcessSpec spec { s, (juce::uint32) maxBlock, 1 };
        for (auto& m : mics)
        {
            m.conv.prepare (spec);
            m.delay.prepare ((int) (0.004 * s) + 8);
            m.levelSm.reset (s, 30.f, 1.f);
            m.panSm.reset (s, 30.f, 0.f);
            m.buf.assign ((size_t) maxBlock, 0.f);
            for (auto& c : m.cache) c = -999.f;
        }
        lcCache = hcCache = -1.f;
        for (int i = 0; i < 2; ++i) if (! mics[i].hasIr) loadBuiltIn (i, builtIn[i]);
        reset();
    }
    void reset()
    {
        for (auto& m : mics) { m.conv.reset(); m.delay.clear(); m.posF.reset(); m.prox.reset(); m.air.reset(); for (auto& f : m.micF) f.reset(); }
        for (auto& f : lc) f.reset(); for (auto& f : hc) f.reset();
    }
    int latency() const    { return mics[0].conv.getLatency(); }

    /** Message thread: load an IR file for a mic. Returns an error message or empty. */
    juce::String loadIr (int mic, const juce::File& file)
    {
        if (! file.existsAsFile()) return "IR file not found";
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (file));
        if (r == nullptr) return "Not a readable audio file (use .wav or .aiff)";
        if (r->lengthInSamples < 16) return "The IR file is empty";
        if (r->lengthInSamples > (juce::int64) (r->sampleRate * 10)) return "That file is too long to be an impulse response";
        auto& m = mics[juce::jlimit (0, 1, mic)];
        m.conv.loadImpulseResponse (file, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::yes, 0,
                                    juce::dsp::Convolution::Normalise::yes);
        m.hasIr = true; m.irName = file.getFileNameWithoutExtension();
        return {};
    }
    /** Message thread: use a generated cab response. */
    void loadBuiltIn (int mic, int which)
    {
        auto& m = mics[juce::jlimit (0, 1, mic)];
        builtIn[mic] = which;
        juce::AudioBuffer<float> ir (1, 4096);
        generateCabIr (ir, sr, which, mic);
        m.conv.loadImpulseResponse (std::move (ir), sr, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::no,
                                    juce::dsp::Convolution::Normalise::yes);
        m.hasIr = false; m.irName = {};
    }
    juce::String irName (int mic) const   { return mics[juce::jlimit (0, 1, mic)].irName; }

    /** Message thread: where a real IR's mic was when it was recorded (0 = dust cap .. 1 = edge, and cm from the grille).
        A real IR already has its mic and position in it, so there the mic's POSITION and DISTANCE work relative to
        this spot: at the spot the IR plays exactly as recorded, moving away shades it. pos < 0: the generated
        (built-in) cabs, where they set the sound outright. */
    void setRecordedSpot (int mic, float pos, float distCm)
    {
        auto& m = mics[juce::jlimit (0, 1, mic)];
        m.distRef.store (juce::jmax (0.f, distCm)); m.posRef.store (pos);
    }
    float recordedPos (int mic) const     { return mics[juce::jlimit (0, 1, mic)].posRef.load(); }

    struct MicParams { int model = 0; float pos = 0.4f, distCm = 2.f, levelDb = 0.f, pan = 0.f; bool solo = false, mute = false, phase = false; };

    /** in: mono amp signal. out: stereo. */
    void process (const float* in, float* outL, float* outR, int n, const MicParams& a, const MicParams& b, float lcHz, float hcHz, bool enabled)
    {
        if (! enabled)
        {
            for (int i = 0; i < n; ++i) outL[i] = outR[i] = in[i];
            return;
        }
        std::fill (outL, outL + n, 0.f); std::fill (outR, outR + n, 0.f);
        const MicParams* mp[2] = { &a, &b };
        const bool anySolo = a.solo || b.solo;
        for (int k = 0; k < 2; ++k)
        {
            auto& m = mics[k]; const auto& p = *mp[k];
            const bool audible = ! p.mute && (! anySolo || p.solo);
            updateMicFilters (m, p);
            float* x = m.buf.data();
            std::copy (in, in + n, x);
            float* chans[1] = { x };
            juce::dsp::AudioBlock<float> blk (chans, 1, (size_t) n);
            m.conv.process (juce::dsp::ProcessContextReplacing<float> (blk));
            // time-of-flight: 1 cm ~ 29 us. A real IR has its own distance in it: only what's beyond that counts.
            const bool real = m.posRef.load() >= 0.f;
            const float dRef = real ? m.distRef.load() : 0.f;
            const float delaySamples = juce::jmax (0.f, p.distCm - dRef) * 0.01f / 343.f * (float) sr;
            const float distGain = (1.f + dRef / 25.f) / (1.f + p.distCm / 25.f);
            m.levelSm.set (audible ? dbToGain (p.levelDb) * distGain * (p.phase ? -1.f : 1.f) : 0.f);
            m.panSm.set (clampf (p.pan / 100.f, -1.f, 1.f));
            for (int i = 0; i < n; ++i)
            {
                m.delay.push (x[i]);
                float v = m.delay.readLinear (delaySamples);
                if (m.colour) for (auto& f : m.micF) v = f.process (v);
                v = m.prox.process (m.posF.process (v));
                if (m.airOn) v = m.air.process (v);
                const float g = m.levelSm.next(), pn = m.panSm.next();
                const float gl = std::cos ((pn + 1.f) * kPi * 0.25f) * 1.41421356f, gr = std::sin ((pn + 1.f) * kPi * 0.25f) * 1.41421356f;
                outL[i] += v * g * juce::jmin (1.f, gl); outR[i] += v * g * juce::jmin (1.f, gr);
            }
        }
        if (lcHz != lcCache) { lcCache = lcHz; for (auto& f : lc) f.set (Biquad::HP, sr, lcHz, 0.707f); }
        if (hcHz != hcCache) { hcCache = hcHz; for (auto& f : hc) f.set (Biquad::LP, sr, hcHz, 0.707f); }
        // at their end stops the cuts are off (a 20 kHz low-pass still shades the top of a real IR)
        const bool lcOn = lcHz > 20.5f, hcOn = hcHz < 19990.f;
        if (lcOn || hcOn)
            for (int i = 0; i < n; ++i)
            {
                if (lcOn) { outL[i] = lc[0].process (outL[i]); outR[i] = lc[1].process (outR[i]); }
                if (hcOn) { outL[i] = hc[0].process (outL[i]); outR[i] = hc[1].process (outR[i]); }
            }
    }

    static juce::StringArray builtInNames()  { return { "Obsidian 1x12", "Obsidian 2x12", "Obsidian 4x12" }; }

private:
    struct Mic
    {
        juce::dsp::Convolution conv { juce::dsp::Convolution::NonUniform { 512 } };
        DelayLine delay;
        Biquad micF[3], posF, prox, air;
        Smoother levelSm, panSm;
        std::vector<float> buf;
        float cache[5] { -999.f, -999.f, -999.f, -999.f, -999.f };
        bool hasIr = false, colour = true, airOn = true; juce::String irName;
        std::atomic<float> posRef { -1.f }, distRef { 0.f };      // the recorded spot of a real IR (setRecordedSpot)
    };

    void updateMicFilters (Mic& m, const MicParams& p)
    {
        const float pr = m.posRef.load(), dr = m.distRef.load();
        const bool real = pr >= 0.f;
        if (m.cache[3] != pr || m.cache[4] != dr) { m.cache[3] = pr; m.cache[4] = dr; m.cache[1] = m.cache[2] = -999.f; }
        if (m.cache[0] != (float) p.model)
        {
            m.cache[0] = (float) p.model;
            m.colour = p.model >= 0 && p.model <= 4;           // Flat: no mic colouring at all
            m.cache[2] = -999.f;   // the proximity bass below depends on the mic type too: redo it (it used to keep the last mic's)
            switch (p.model)
            {
                case 0: // Dynamic 57: presence bump, low-end roll-off
                    m.micF[0].set (Biquad::HP, sr, 90.f, 0.7f); m.micF[1].set (Biquad::Peak, sr, 5200.f, 1.2f, 4.f); m.micF[2].set (Biquad::LP, sr, 13000.f, 0.7f); break;
                case 1: // Ribbon 121: warm, dark top
                    m.micF[0].set (Biquad::LowShelf, sr, 180.f, 0.7f, 2.5f); m.micF[1].set (Biquad::HighShelf, sr, 4500.f, 0.7f, -5.f); m.micF[2].set (Biquad::LP, sr, 11000.f, 0.6f); break;
                case 2: // Condenser 414: open and airy
                    m.micF[0].set (Biquad::HP, sr, 40.f, 0.7f); m.micF[1].set (Biquad::HighShelf, sr, 8000.f, 0.7f, 2.5f); m.micF[2].set (Biquad::Peak, sr, 2500.f, 1.f, 1.f); break;
                case 3: // Dynamic 421: scooped, bright
                    m.micF[0].set (Biquad::Peak, sr, 450.f, 0.9f, -2.5f); m.micF[1].set (Biquad::Peak, sr, 4000.f, 1.f, 3.f); m.micF[2].set (Biquad::HP, sr, 60.f, 0.7f); break;
                case 4: // Dynamic 906: mid-forward
                    m.micF[0].set (Biquad::Peak, sr, 1200.f, 0.8f, 2.5f); m.micF[1].set (Biquad::Peak, sr, 6500.f, 1.5f, 2.f); m.micF[2].set (Biquad::HP, sr, 80.f, 0.7f); break;
                default: for (auto& f : m.micF) f.set (Biquad::Peak, sr, 1000.f, 1.f, 0.f); break;
            }
        }
        if (std::abs (m.cache[1] - p.pos) > 1e-3f)
        {
            m.cache[1] = p.pos;   // centre of cone = bright, edge = dark (a real IR: relative to where it was recorded)
            m.posF.set (Biquad::HighShelf, sr, 2500.f, 0.6f, real ? (pr - p.pos) * 12.f : 3.f - p.pos * 12.f);
        }
        if (std::abs (m.cache[2] - p.distCm) > 1e-3f)
        {
            m.cache[2] = p.distCm;
            const bool directional = p.model != 2;
            auto bass = [] (float d) { return 6.f * (1.f - clampf (d / 20.f, 0.f, 1.f)); };
            m.prox.set (Biquad::LowShelf, sr, 160.f, 0.7f, directional ? bass (p.distCm) - (real ? bass (dr) : 0.f) : 0.f);
            const float far = real ? p.distCm - dr : p.distCm;
            m.airOn = ! real || far > 0.05f;                    // a real IR at (or nearer than) its spot: no air loss
            m.air.set (Biquad::LP, sr, 20000.f - clampf (far / 30.f, 0.f, 1.f) * 9000.f, 0.6f);
        }
    }

    /** Builds a plausible guitar-speaker impulse response (resonance, cone break-up, roll-off). */
    static void generateCabIr (juce::AudioBuffer<float>& ir, double sr, int which, int micIndex)
    {
        ir.clear();
        float* d = ir.getWritePointer (0);
        const int n = ir.getNumSamples();
        // Excite with an impulse plus a little cabinet box reflection.
        d[0] = 1.f;
        const int refl = (int) (sr * (which == 2 ? 0.0011 : which == 1 ? 0.0009 : 0.0007));
        if (refl < n) d[refl] = -0.18f;
        const float lowRes = which == 2 ? 105.f : which == 1 ? 120.f : 135.f;
        const float lp = which == 2 ? 4800.f : which == 1 ? 5300.f : 5800.f;
        Biquad chain[8];
        chain[0].set (Biquad::HP, sr, lowRes * 0.6f, 1.3f);              // resonant low end of a closed back cab
        chain[1].set (Biquad::Peak, sr, lowRes, 1.4f, which == 2 ? 5.f : 3.f);
        chain[2].set (Biquad::Peak, sr, 420.f, 0.9f, -3.f);
        chain[3].set (Biquad::Peak, sr, 2400.f, 1.2f, 4.5f);              // cone break-up
        chain[4].set (Biquad::Peak, sr, 3900.f, 2.5f, micIndex == 0 ? 2.f : -1.f);
        chain[5].set (Biquad::LP, sr, lp, 0.8f);
        chain[6].set (Biquad::LP, sr, lp * 1.25f, 0.6f);
        chain[7].set (Biquad::Notch, sr, 7600.f, 3.f);
        for (int i = 0; i < n; ++i)
        {
            float v = d[i];
            for (auto& f : chain) v = f.process (v);
            d[i] = v;
        }
        // Short fade to avoid a truncation click.
        for (int i = n - 256; i < n; ++i) d[i] *= (float) (n - i) / 256.f;
    }

    double sr = 48000;
    Mic mics[2];
    int builtIn[2] { Cab4x12, Cab4x12 };
    Biquad lc[2], hc[2];
    float lcCache = -1.f, hcCache = -1.f;
};
} // namespace obs::dsp
