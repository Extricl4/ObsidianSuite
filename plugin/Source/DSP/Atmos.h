#pragma once
// =============================================================================
//  AETHER: SIREN's second page. Noise, ambience and atmospheres that play on
//  their own (no notes needed), under and around the guitar.
//
//   BED      a bed of noise: hiss, wind (gusts and a howl), rain (drops on a
//            hiss), surf (waves rolling in), rumble (deep earth and a sub),
//            static (an old radio, crackle and tuning whistles), breath
//            (slow, rasping breathing), brook (a gurgling stream and bubbles),
//            night (crickets and soft air), forest (leaves in a breeze), soft
//            rain (on a window), underwater, air (a soft, high sheen)
//   DRONE    tones that hang in the air, in SIREN's key or following the
//            guitar: organ (tonewheels, drawbars and a Leslie), strings (a
//            section with bows and a wooden body), choir (four vowel resonances),
//            glass (bell-like FM), abyss (a growling sub), descent / ascent (a
//            Shepard tone that falls or rises forever), pipe organ (ranks and the
//            pipes' chiff), pad, angels (an airy "ooh" with an octave above),
//            tanpura (four strings plucked round and round, buzzing on the
//            jawari), singing bowls, harmonium, flute. Chords from one note to a
//            dread cluster, major sevenths, ninths and wide open voicings.
//            SHINE adds the chord an octave and two up; AIR adds breath.
//   TEXTURE  things that happen now and then: vinyl crackle, fire, drips in a
//            cave, metal creaks and scrapes, shimmering glass, whispers, a
//            heartbeat, knocks on a door, distant moans of pain, flesh (wet squelches,
//            bones cracking, tearing), wind chimes, birds, a music box, a distant
//            church bell, a harp, stars, a kalimba, rain on leaves. The melodic ones
//            play SIREN's key and scale (its pentatonic, so nothing clashes).
//   SPACE    an 8-line feedback delay network, from a room to an endless void,
//            with FREEZE (holds what's in it forever) and SHIMMER (the space
//            rises an octave as it rings)
//   OUTPUT   FOLLOW (duck under the guitar or swell with it), EVOLVE (slow
//            random drift of every layer), GRIT, TONE (dark .. bright), WIDTH
//
//  The engine adds it at the very end of the chain, after the guitar's EQ, effects, doubler and width.
//  Settings are read every 32 samples; no allocation on the audio thread.
// =============================================================================
#include <array>
#include <atomic>
#include "DspCore.h"
#include "AtmosDefs.h"

namespace obs::dsp
{
namespace ad = obs::atmosdefs;

struct AtmosParams
{
    float v[ad::count] {};
    int   key = 4;                 // SIREN's key (C = 0): the drone and the shimmer stay in it
    int   scale = 1;               // SIREN's scale (0 Major, 1 Minor, 2 Dorian, 3 Phrygian, 4 Harmonic minor, 5 Pentatonic, 6 Blues)
    float tuneHz = 440.f;          // SIREN's tuning reference
    float guitarHz = 0.f, guitarConf = 0.f;   // the pitch tracker (the drone can follow the guitar)
    float get (int i) const       { return v[i]; }
    int   choice (int i) const    { return (int) std::lround (v[i]); }
    bool  on (int i) const        { return v[i] >= 0.5f; }
};

class Atmos
{
public:
    static constexpr int kCtl = 32, kEvents = 32, kDrops = 12, kNotes = 4, kUni = 3, kShep = 9, kLines = 8, kBubbles = 24, kCrickets = 6;

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        sr = (float) sampleRate; ctlRate = sr / (float) kCtl;
        for (auto* s : { &bpL, &bpR, &whL, &whR, &f1L, &f1R, &f2L, &f2R, &rumL, &rumR, &dLpL, &dLpR, &crkBp })
            s->setSampleRate (sampleRate);
        for (auto& f : formL) f.setSampleRate (sampleRate);
        for (auto& f : formR) f.setSampleRate (sampleRate);
        for (auto& d : drops) d.f.setSampleRate (sampleRate);
        for (int i = 0; i < kNotes * kUni; ++i) { voice[i].bn.setSampleRate (sampleRate); voice[i].u = i % kUni; voice[i].vrate = 4.6f + 1.5f * rng.uni(); }
        for (auto* s : { &airL, &airR, &bowL, &bowR }) s->setSampleRate (sampleRate);
        for (auto& f : chiffF) f.setSampleRate (sampleRate);
        bowL.set (3400.f, 0.8f); bowR.set (3650.f, 0.8f);
        // a violin family's body: the air resonance, a nasal dip, the bridge's brightness; a reed organ's box
        bodyL[0].set (Biquad::Peak, sampleRate, 280.f, 1.2f, 4.f); bodyL[1].set (Biquad::Peak, sampleRate, 1500.f, 1.f, -3.f); bodyL[2].set (Biquad::Peak, sampleRate, 3000.f, 1.5f, 4.f);
        reedL[0].set (Biquad::Peak, sampleRate, 1150.f, 1.3f, 5.f); reedL[1].set (Biquad::Peak, sampleRate, 2700.f, 2.f, 3.f);
        for (int k = 0; k < 3; ++k) coefs (bodyR[k], bodyL[k]);
        for (int k = 0; k < 2; ++k) coefs (reedR[k], reedL[k]);
        windHp.setLowpass (sampleRate, 1800.f); lesLp.setLowpass (sampleRate, 800.f);
        shineLpL.setLowpass (sampleRate, 9000.f); shineLpR.a = shineLpL.a;
        scanLine.prepare ((int) (0.003 * sampleRate) + 8); hornLine.prepare ((int) (0.003 * sampleRate) + 8);
        for (auto& t : tan) { t.line.prepare ((int) (sampleRate / 40.0) + 8); t.jw.setSampleRate (sampleRate); }
        shimLine.prepare ((int) (0.06 * sampleRate) + 8); shimW = 0.05f * (float) sampleRate;
        shimHp.setLowpass (sampleRate, 350.f); shimLp.setLowpass (sampleRate, 7000.f);
        for (int k = 0; k < kCrickets; ++k)
        {
            const float pan = 0.5f + 0.42f * std::sin (1.7f * (float) k + 0.4f);
            cricket[k].pl = std::sqrt (1.f - pan); cricket[k].pr = std::sqrt (pan);
        }
        for (auto& e : ev) { e.a.setSampleRate (sampleRate); e.b.setSampleRate (sampleRate); e.c.setSampleRate (sampleRate); }
        const float maxLen = kBaseMs[kLines - 1] * 0.001f * kMaxScale * sr + 64.f;
        for (auto& l : line) l.prepare ((int) maxLen);
        for (auto& a : diffL) a.prepare ((int) (0.03f * sr));
        for (auto& a : diffR) a.prepare ((int) (0.03f * sr));
        follow.set (sampleRate, 6.f, 450.f);
        crkBp.set (2600.f, 0.8f);
        dcL.prepare (sampleRate); dcR.prepare (sampleRate);
        reset();
    }

    void reset()
    {
        pinkL = pinkR = {}; brL = brR = 0;
        for (auto* s : { &bpL, &bpR, &whL, &whR, &f1L, &f1R, &f2L, &f2R, &rumL, &rumR, &dLpL, &dLpR, &crkBp }) s->reset();
        for (auto& f : formL) f.reset();
        for (auto& f : formR) f.reset();
        for (auto* o : { &lpL, &lpR, &hpL, &hpR, &texLp, &roarLp, &roarLp2, &vinHp, &gurgL, &gurgR, &glotL, &glotR, &windHp, &lesLp, &shineLpL, &shineLpR, &shimHp, &shimLp }) o->reset();
        for (auto* s : { &airL, &airR, &bowL, &bowR }) s->reset();
        for (auto& f : chiffF) f.reset();
        for (int k = 0; k < 3; ++k) { bodyL[k].reset(); bodyR[k].reset(); }
        for (int k = 0; k < 2; ++k) { reedL[k].reset(); reedR[k].reset(); }
        scanLine.clear(); hornLine.clear(); shimLine.clear(); shimPh = 0; shimIn = 0;
        for (auto& t : tan) { t.line.clear(); t.z = 0; t.exN = 0; t.jEnv = 0; t.jw.reset(); }
        for (auto& b : bowl) { for (auto& pp : b.ph) pp[0] = pp[1] = 0.f; b.strike = b.strikeHi = 0.f; b.wait = 0.f; }
        for (auto& b : bubbles) b.on = false;
        for (int k = 0; k < kCrickets; ++k) { cricket[k].pulses = 0; cricket[k].gap = (int) ((0.05f + 0.6f * (float) k / kCrickets) * sr); }
        for (auto& c : chiffEnv) c = 0.f;
        voiceWas = -1; drnOnWas = false; hornHz = drumHz = 0.f;
        for (auto& d : drops) d.on = false;
        for (auto& e : ev) e.kind = -1;
        // the drone's voices start in step: with random phases, unison voices on one pitch (DETUNE at 0) met at
        // random and the same scene came out up to 10 dB louder or softer each time it started (and Abyss's
        // two oscillators changed timbre)
        for (auto& v : voice) { v.ph = 0.f; v.ph2 = 0.f; rng.uni(); rng.uni(); }   // (still draws its two numbers: every other random thing stays as it was)
        for (auto& sh : shep) for (auto& ph : sh.ph) ph = rng.uni();
        shepPos = 0.5f;
        for (auto& l : line) l.clear();
        for (auto& a : diffL) a.clear();
        for (auto& a : diffR) a.clear();
        for (auto& d : damp) d.reset();
        follow.reset(); dcL.reset(); dcR.reset();
        lowShL.reset(); lowShR.reset(); hiShL.reset(); hiShR.reset();
        crush = { 0, 0 }; crushN = 0; gritOn = 0;
        followG = 1; masterSm = 0; bedSm = drnSm = texSm = 0; inSm = 1;
        drnMidi = -1;      // (the event count keeps counting: the page compares it)
        nextEvent = 0; vinylT = 0; ctlLeft = 0;
        for (auto& a : act) a.store (0.f);
        for (auto& a : actAcc) a = 0;
    }

    /** in: mono guitar (FOLLOW listens to it), may be null. outL / outR: the atmosphere (overwritten). */
    void process (const float* in, float* outL, float* outR, int n, const AtmosParams& p)
    {
        prm = &p;
        for (int i = 0; i < n; ++i)
        {
            if (--ctlLeft <= 0) { ctlLeft = kCtl; control (p); }

            // ---- layers ---------------------------------------------------------------------
            float bl = 0, br = 0, dl = 0, dr = 0, tl = 0, tr = 0, sendL = 0, sendR = 0;
            bedSm += (bedTarget - bedSm) * 0.002f;
            drnSm += (drnTarget - drnSm) * 0.002f;
            texSm += (texTarget - texSm) * 0.002f;
            if (bedSm > 1.0e-5f) { bedSample (bl, br); bl *= bedSm; br *= bedSm; }
            if (drnSm > 1.0e-5f) { droneSample (dl, dr); dl *= drnSm; dr *= drnSm; }
            if (texSm > 1.0e-5f || anyEvents) { float xl = 0, xr = 0; textureSample (xl, xr, sendL, sendR); tl = xl * texSm; tr = xr * texSm; sendL *= texSm; sendR *= texSm; }
            actAcc[0] += bl * bl + br * br; actAcc[1] += dl * dl + dr * dr; actAcc[2] += tl * tl + tr * tr;

            float l = bl + dl + tl, r = br + dr + tr;

            // ---- GRIT (before the space: rotten sources in a clean space) ----------------------
            if (gritAmt > 0.002f)
            {
                const float d = 1.f + gritAmt * gritAmt * 22.f, comp = 1.f / fastTanh (d * 0.5f) * 0.5f;
                float gl = fastTanh (l * d) * comp, gr = fastTanh (r * d) * comp;
                if (gritAmt > 0.55f)
                {
                    // above halfway it also rots: a lower rate and fewer bits
                    const int hold = 1 + (int) ((gritAmt - 0.55f) * 9.f);
                    if (++crushN >= hold) { crushN = 0; const float q = 2.f + (1.f - gritAmt) * 60.f; crush = { std::round (gl * q) / q, std::round (gr * q) / q }; }
                    const float m = (gritAmt - 0.55f) * 2.2f;
                    gl += (crush.first - gl) * m; gr += (crush.second - gr) * m;
                }
                l = l + (gl - l) * juce::jmin (1.f, gritAmt * 3.f); r = r + (gr - r) * juce::jmin (1.f, gritAmt * 3.f);
            }

            // ---- SPACE -----------------------------------------------------------------------------
            float wl = 0, wr = 0;
            spaceSample (l + sendL, r + sendR, wl, wr);
            l = l * dryGain + wl * wetGain; r = r * dryGain + wr * wetGain;

            // ---- TONE, WIDTH, FOLLOW, LEVEL -------------------------------------------------------
            l = hiShL.process (lowShL.process (l)); r = hiShR.process (lowShR.process (r));
            const float m = 0.5f * (l + r), s = 0.5f * (l - r) * widthAmt;
            l = m + s; r = m - s;
            const float g = in != nullptr ? follow.process (in[i]) : 0.f;
            followTick (g);
            masterSm += (masterTarget - masterSm) * 0.002f;
            const float og = masterSm * followG;
            l = dcL.process (l * og); r = dcR.process (r * og);
            if (isBad (l) || isBad (r)) { l = r = 0; panic(); }
            outL[i] = softLimit (l); outR[i] = softLimit (r);
        }
        // UI meters
        const float k = 1.f / juce::jmax (1, n);
        for (int j = 0; j < 3; ++j) { act[j].store (std::sqrt (actAcc[j] * k) * 4.f); actAcc[j] = 0; }
    }

    /** How busy each layer is right now (0 bed, 1 drone, 2 texture), roughly 0..1, for the page's meters. */
    float activity (int layer) const  { return act[juce::jlimit (0, 2, layer)].load (std::memory_order_relaxed); }
    /** Counts texture events (the page lights a spark on each new one). */
    int   events() const              { return shownEvents.load (std::memory_order_relaxed); }
    /** The drone's root note now (midi, -1: no drone), so the page can show where FOLLOW took it. */
    float droneRoot() const           { return rootShown.load (std::memory_order_relaxed); }

private:
    // ======================================================================== small parts
    struct Rng
    {
        uint32_t s = 0x2545F491u;
        inline uint32_t u()      { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
        inline float uni()       { return (float) (u() >> 8) * (1.0f / 16777216.0f); }   // [0, 1)
        inline float bi()        { return uni() * 2.f - 1.f; }                         // [-1, 1)
        inline float expo()      { return -std::log (juce::jmax (1.0e-6f, uni())); }   // mean 1
    };
    /** A smooth random walk: a new target now and then, eased between (control rate). */
    struct Drift
    {
        float from = 0, to = 0, t = 1, inc = 0.01f, base = 0.01f, cur = 0;
        void setRate (float hz, float ctl)   { base = clampf (hz / ctl, 1.0e-6f, 0.5f); }
        float tick (Rng& r)
        {
            t += inc;
            if (t >= 1.f) { t -= 1.f; from = to; to = r.bi(); inc = base * (0.6f + 0.8f * r.uni()); }
            const float s = t * t * (3.f - 2.f * t);
            cur = from + (to - from) * s; return cur;
        }
    };
    struct Pink   // Paul Kellet's economy filter
    {
        float b0 = 0, b1 = 0, b2 = 0;
        inline float next (float w) { b0 = 0.99765f * b0 + w * 0.0990460f; b1 = 0.96300f * b1 + w * 0.2965164f; b2 = 0.57000f * b2 + w * 1.0526913f;
                                      return (b0 + b1 + b2 + w * 0.1848f) * 0.25f; }
    };
    static const float* sineTab()
    {
        static const std::array<float, 4097> t = [] { std::array<float, 4097> a {}; for (int i = 0; i <= 4096; ++i) a[(size_t) i] = (float) std::sin (juce::MathConstants<double>::twoPi * i / 4096.0); return a; }();
        return t.data();
    }
    static inline float wrap01 (float x)  { return x - std::floor (x); }
    static inline float sin01 (float ph)  // ph in [0, 1)
    {
        const float x = ph * 4096.f; const int i = juce::jlimit (0, 4095, (int) x); const float f = x - (float) i;
        const float* t = sineTab(); return t[i] + (t[i + 1] - t[i]) * f;
    }
    static inline float blep (float t, float dt)
    {
        if (t < dt) { const float x = t / dt; return x + x - x * x - 1.f; }
        if (t > 1.f - dt) { const float x = (t - 1.f) / dt; return x * x + x + x + 1.f; }
        return 0.f;
    }
    static inline float saw (float ph, float inc) { return 2.f * ph - 1.f - blep (ph, inc); }
    static inline float softLimit (float x) { return std::abs (x) < 0.9f ? x : (x > 0 ? 0.9f + 0.1f * fastTanh ((x - 0.9f) * 10.f) : -0.9f - 0.1f * fastTanh ((-x - 0.9f) * 10.f)); }
    inline float midiHz (float m) const   { return prm->tuneHz * std::exp2 ((m - 69.f) / 12.f); }
    static inline float lvl (float pct)   { const float x = clampf (pct * 0.01f, 0.f, 1.f); return x <= 0.f ? 0.f : std::pow (x, 1.7f); }
    static void coefs (Biquad& d, const Biquad& s) { d.c0 = s.c0; d.c1 = s.c1; d.c2 = s.c2; d.d1 = s.d1; d.d2 = s.d2; }
    void droneFilter (float fc, float q)  { dLpL.set (fc, q); dLpR.set (fc, q); }

    // ======================================================================== control rate
    void control (const AtmosParams& p)
    {
        const float evo = p.get (ad::evolve) * 0.01f;
        evB = evDrB.tick (rng) * evo; evD = evDrD.tick (rng) * evo; evT = evDrT.tick (rng) * evo; evP = evDrP.tick (rng) * evo;
        evDrB.setRate (0.045f, ctlRate); evDrD.setRate (0.032f, ctlRate); evDrT.setRate (0.06f, ctlRate); evDrP.setRate (0.05f, ctlRate);

        // layer levels (EVOLVE swings them a few dB)
        bedType = p.choice (ad::bedType); drnVoice = p.choice (ad::drnVoice); texType = p.choice (ad::texType);
        bedTarget = p.on (ad::bedOn) ? lvl (p.get (ad::bedLevel)) * kBedNorm[juce::jlimit (0, ad::kBedTypes - 1, bedType)] * dbToGain (evB * 4.f) : 0.f;
        drnTarget = p.on (ad::drnOn) ? lvl (p.get (ad::drnLevel)) * kDrnNorm[juce::jlimit (0, ad::kVoices - 1, drnVoice)] * dbToGain (evD * 4.f) : 0.f;
        texTarget = p.on (ad::texOn) ? lvl (p.get (ad::texLevel)) * kTexNorm[juce::jlimit (0, ad::kTextures - 1, texType)] : 0.f;
        masterTarget = dbToGain (p.get (ad::level) + kTrimDb);
        gritAmt = p.get (ad::grit) * 0.01f;
        widthAmt = p.get (ad::width) * 0.01f * 1.25f;
        const float tilt = p.get (ad::tone) * 0.01f;
        lowShL.set (Biquad::LowShelf, sr, 260.f, 0.7f, -tilt * 7.f); coefs (lowShR, lowShL);
        hiShL.set (Biquad::HighShelf, sr, 2600.f, 0.7f, tilt * 7.f); coefs (hiShR, hiShL);
        followAmt = p.get (ad::follow) * 0.01f;

        controlBed (p); controlDrone (p); controlTexture (p); controlSpace (p);
    }

    // ======================================================================== BED
    void controlBed (const AtmosParams& p)
    {
        const float t = clampf (p.get (ad::bedTone) * 0.01f + evB * 0.25f, 0.f, 1.f), m = p.get (ad::bedMotion) * 0.01f, d = p.get (ad::bedDepth) * 0.01f;
        bedT = t; bedM = m; bedD = d;
        const float rate = 0.04f + m * m * 1.6f;
        bd1.setRate (rate, ctlRate); bd2.setRate (rate * 1.13f, ctlRate); bd3.setRate (rate * 0.7f, ctlRate); bd4.setRate (rate * 2.9f, ctlRate);
        const float a = bd1.tick (rng), b = bd2.tick (rng), c = bd3.tick (rng), e = bd4.tick (rng);
        switch (bedType)
        {
            case ad::Hiss:
            {
                const float fc = 1800.f * std::exp2 (t * 3.3f + d * 0.8f * a);
                lpL.setLowpass (sr, fc); lpR.a = lpL.a; hpL.setLowpass (sr, 30.f); hpR.a = hpL.a;
                bedAmp = 1.f + d * 0.35f * c;
                break;
            }
            case ad::Wind:
            {
                const float base = 220.f * std::exp2 (t * 3.4f);
                const float q = 1.4f + d * 4.5f;
                bpL.set (base * std::exp2 (d * 1.6f * a), q); bpR.set (base * std::exp2 (d * 1.6f * b), q);
                whL.set (base * 2.3f * std::exp2 (d * 1.2f * b), 9.f + d * 10.f); whR.set (base * 2.1f * std::exp2 (d * 1.2f * a), 9.f + d * 10.f);
                gust = 0.25f + 0.75f * clampf (0.5f + 0.6f * c + 0.25f * e * m, 0.f, 1.f);
                howl = d * d * 0.45f;
                bedQComp = 0.6f + q * 0.18f;
                break;
            }
            case ad::Rain:
            {
                lpL.setLowpass (sr, 1800.f + t * 9000.f); lpR.a = lpL.a; hpL.setLowpass (sr, 420.f); hpR.a = hpL.a;
                rainRate = 25.f + m * m * 520.f;
                bedAmp = 1.f + 0.2f * d * c;
                break;
            }
            case ad::Surf:
            {
                // two waves (left, right), each with its own period; MOTION brings them faster
                for (int k = 0; k < 2; ++k)
                {
                    wavePh[k] += waveInc[k];
                    if (wavePh[k] >= 1.f) { wavePh[k] -= 1.f; waveInc[k] = 1.f / ((11.f - 7.f * m) * (0.75f + 0.5f * rng.uni()) * ctlRate); }
                    const float ph = wavePh[k];
                    const float env = ph < 0.32f ? std::pow (ph / 0.32f, 1.8f) : std::pow ((1.f - ph) / 0.68f, 1.4f);
                    wave[k] = 0.12f + 0.88f * std::pow (env, 1.f + d);
                }
                rumL.set (260.f + wave[0] * (900.f + t * 7000.f), 0.6f); rumR.set (260.f + wave[1] * (900.f + t * 7000.f), 0.6f);
                break;
            }
            case ad::Rumble:
            {
                const float fc = 32.f + t * t * 260.f;
                rumL.set (fc * std::exp2 (0.3f * a * d), 0.9f); rumR.set (fc * std::exp2 (0.3f * b * d), 0.9f);
                subInc = (27.f + t * 20.f) / sr * std::exp2 (0.15f * c * d);
                subAmp = 0.15f + 0.55f * d * clampf (0.5f + 0.7f * c, 0.f, 1.f);
                bedAmp = 1.f + 0.5f * d * a;
                break;
            }
            case ad::Static:
            {
                bpL.set (1300.f * std::exp2 (t * 2.2f), 0.9f); bpR.set (1300.f * std::exp2 (t * 2.2f + 0.15f), 0.9f);
                crkRate = 6.f + m * m * 120.f;
                // a tuning whistle that wanders (and sometimes slides, like a dial being turned)
                whF = 700.f * std::exp2 (1.6f + 1.4f * a + 0.6f * e * m);
                whAmp = d * d * 0.22f * clampf (0.6f + 0.6f * b, 0.f, 1.f);
                bedAmp = 0.6f + 0.4f * clampf (0.5f + 0.8f * c * d, 0.f, 1.f);
                break;
            }
            case ad::Breath:
            {
                // one breath: in (rising, brighter) - a hold - out (falling, darker) - a pause
                breathPh += breathInc;
                if (breathPh >= 1.f) { breathPh -= 1.f; breathInc = 1.f / ((6.5f - 4.f * m) * (1.f - 0.35f * d + 0.7f * d * rng.uni()) * ctlRate); }
                const float ph = breathPh;
                float env, bright;
                if (ph < 0.36f)      { env = std::sin (kPi * 0.5f * ph / 0.36f); bright = 1.f; }
                else if (ph < 0.42f) { env = 1.f - (ph - 0.36f) * 4.f; bright = 0.9f; }
                else if (ph < 0.85f) { const float x = (ph - 0.42f) / 0.43f; env = 0.85f * std::pow (1.f - x, 1.3f) * (0.6f + 0.4f * std::sin (kPi * juce::jmin (1.f, x * 3.f))); bright = 0.55f; }
                else                 { env = 0.f; bright = 0.5f; }
                breathEnv = env;
                const float shift = std::exp2 ((t - 0.5f) * 1.2f) * (0.8f + 0.2f * bright);
                f1L.set (620.f * shift, 3.5f); f1R.set (640.f * shift, 3.5f);
                f2L.set (1750.f * shift * (0.9f + 0.2f * bright), 4.f); f2R.set (1820.f * shift * (0.9f + 0.2f * bright), 4.f);
                lpL.setLowpass (sr, 1500.f + 5500.f * bright * (0.5f + t)); lpR.a = lpL.a;
                fryAmt = clampf ((d - 0.4f) * 1.7f, 0.f, 1.f);
                fryInc = (38.f + 26.f * rng.uni()) / sr;
                break;
            }
            case ad::Brook:
            {
                // a stream: a gurgling hiss and bubbles of every size. MOTION = flow, DEPTH = splash, TONE = bubble size (pitch)
                const float fc = 650.f * std::exp2 (t * 1.8f + 0.4f * a * m);
                bpL.set (fc, 0.8f); bpR.set (fc * 1.09f, 0.8f);
                gurgL.setLowpass (sr, 9.f + 14.f * m); gurgR.a = gurgL.a;
                bubbleRate = 30.f + m * m * 260.f;
                bedAmp = 0.35f + 0.65f * d;
                break;
            }
            case ad::Night:
            {
                // crickets in the dark grass, and the night's soft air. MOTION = how fast they chirp, DEPTH = how many
                const float base = 3600.f * std::exp2 (t * 1.0f);
                for (int k = 0; k < kCrickets; ++k)
                {
                    auto& cr = cricket[k];
                    cr.on = (float) k < 1.f + d * (float) (kCrickets - 1) + 0.01f;
                    cr.inc = base * (1.f + 0.06f * (float) k * (k % 2 ? -1.f : 1.f)) / sr;
                    cr.gapMean = (1.4f - 1.1f * m) * (0.85f + 0.12f * (float) k);
                }
                lpL.setLowpass (sr, 450.f); lpR.a = lpL.a;
                bedAmp = 1.f + 0.15f * c;
                break;
            }
            case ad::Forest:
            {
                // leaves in a breeze: a high rustle that comes in gusts, and the low wind under it. MOTION = breeze, DEPTH = rustle
                gust = 0.45f + 0.55f * clampf (0.5f + 0.7f * a + 0.2f * e * m, 0.f, 1.f);    // a lull, never silence
                hpL.setLowpass (sr, 1300.f + 700.f * t); hpR.a = hpL.a;
                lpL.setLowpass (sr, 4500.f * std::exp2 (t * 1.4f)); lpR.a = lpL.a;
                gurgL.setLowpass (sr, 14.f + 18.f * d); gurgR.a = gurgL.a;
                rumL.set (240.f + 160.f * t, 0.7f); rumR.set (260.f + 160.f * t, 0.7f);
                bedAmp = 0.35f + 0.65f * d;
                break;
            }
            case ad::SoftRain:
            {
                // gentle rain on a window: a soft hiss and small taps on the glass. MOTION = amount, DEPTH = drops
                lpL.setLowpass (sr, 900.f + t * 3600.f); lpR.a = lpL.a; hpL.setLowpass (sr, 300.f); hpR.a = hpL.a;
                rainRate = 14.f + m * m * 200.f;
                bedAmp = 1.f + 0.15f * d * c;
                break;
            }
            case ad::Underwater:
            {
                // under the surface: a deep, muffled swell, slow bubbles rising and the pressure hum. MOTION = current, DEPTH = bubbles
                for (int k = 0; k < 2; ++k)
                {
                    wavePh[k] += waveInc[k];
                    if (wavePh[k] >= 1.f) { wavePh[k] -= 1.f; waveInc[k] = 1.f / ((14.f - 9.f * m) * (0.75f + 0.5f * rng.uni()) * ctlRate); }
                    wave[k] = 0.55f + 0.45f * std::sin (kPi * wavePh[k]);
                }
                rumL.set (110.f + t * 520.f, 0.75f); rumR.set (118.f + t * 520.f, 0.75f);
                bubbleRate = 0.5f + d * d * 22.f;
                subInc = (38.f + 14.f * c) / sr; subAmp = 0.07f;
                break;
            }
            case ad::Air:
            {
                // air: a soft, high sheen of breath that swells and drifts, like the air of a huge, quiet hall. TONE = height
                const float fc = 3800.f * std::exp2 ((t - 0.5f) * 1.8f + 0.25f * a * m);
                bpL.set (fc, 0.55f); bpR.set (fc * 1.1f, 0.55f);
                bedAmp = 0.55f + 0.45f * clampf (0.5f + 0.8f * b * d, 0.f, 1.f);
                break;
            }
            default: break;
        }
    }

    inline void bedSample (float& l, float& r)
    {
        const float wl = rng.bi(), wr = rng.bi();
        switch (bedType)
        {
            case ad::Hiss:
            {
                const float pl = pinkL.next (wl), pr = pinkR.next (wr);
                brL = brL * 0.996f + wl * 0.06f; brR = brR * 0.996f + wr * 0.06f;
                float nl, nr;
                if (bedT < 0.5f) { const float k = bedT * 2.f; nl = brL * (1.f - k) + pl * k; nr = brR * (1.f - k) + pr * k; }
                else { const float k = (bedT - 0.5f) * 2.f; nl = pl * (1.f - k) + wl * 0.5f * k; nr = pr * (1.f - k) + wr * 0.5f * k; }
                nl = lpL.lp (nl); nr = lpR.lp (nr); nl -= hpL.lp (nl); nr -= hpR.lp (nr);
                l = nl * bedAmp; r = nr * bedAmp; break;
            }
            case ad::Wind:
            {
                const float pl = pinkL.next (wl), pr = pinkR.next (wr);
                bpL.tick (pl); bpR.tick (pr); whL.tick (pl); whR.tick (pr);
                l = (bpL.band * bedQComp + whL.band * howl * 2.2f) * gust;
                r = (bpR.band * bedQComp + whR.band * howl * 2.2f) * gust; break;
            }
            case ad::Rain:
            {
                const float pl = pinkL.next (wl), pr = pinkR.next (wr);
                float hl = lpL.lp (pl), hr = lpR.lp (pr); hl -= hpL.lp (hl); hr -= hpR.lp (hr);
                l = hl * 0.8f * bedAmp; r = hr * 0.8f * bedAmp;
                if (rng.uni() < rainRate / sr) spawnDrop (false);
                dropsSample (l, r);
                break;
            }
            case ad::Surf:
            {
                const float pl = pinkL.next (wl), pr = pinkR.next (wr);
                rumL.tick (pl); rumR.tick (pr);
                l = rumL.low * wave[0] * 1.6f; r = rumR.low * wave[1] * 1.6f; break;
            }
            case ad::Rumble:
            {
                brL = brL * 0.997f + wl * 0.05f; brR = brR * 0.997f + wr * 0.05f;
                rumL.tick (brL); rumR.tick (brR);
                subPh = wrap01 (subPh + subInc);
                const float sub = sin01 (subPh) * subAmp;
                l = (rumL.low * 2.2f) * bedAmp + sub; r = (rumR.low * 2.2f) * bedAmp + sub; break;
            }
            case ad::Static:
            {
                bpL.tick (wl); bpR.tick (wr);
                // crackle: bursts of clicks that come and go
                if (rng.uni() < crkRate / sr) crkBurst = (int) ((0.004f + 0.05f * rng.uni()) * sr);
                float crk = 0;
                if (crkBurst > 0) { --crkBurst; if (rng.uni() < 0.08f) crk = rng.bi() * (0.4f + rng.uni()); }
                crkBp.tick (crk);
                whPh = wrap01 (whPh + whF / sr); whPh2 = wrap01 (whPh2 + whF * 1.0031f / sr);
                const float wh = (sin01 (whPh) + 0.6f * sin01 (whPh2)) * whAmp;
                l = bpL.band * 0.9f * bedAmp + crkBp.band * 1.5f + wh;
                r = bpR.band * 0.9f * bedAmp + crkBp.band * 1.3f + wh * 0.9f; break;
            }
            case ad::Breath:
            {
                const float pl = pinkL.next (wl) * 0.7f + wl * 0.12f, pr = pinkR.next (wr) * 0.7f + wr * 0.12f;
                f1L.tick (pl); f1R.tick (pr); f2L.tick (pl); f2R.tick (pr);
                float sl = f1L.band * 1.1f + f2L.band * 0.9f + lpL.lp (pl) * 0.25f;
                float sr_ = f1R.band * 1.1f + f2R.band * 0.9f + lpR.lp (pr) * 0.25f;
                // vocal fry: the rasp of a sore throat
                fryPh = wrap01 (fryPh + fryInc);
                const float fry = 1.f - fryAmt + fryAmt * (fryPh < 0.18f ? 1.6f : 0.25f);
                l = sl * breathEnv * fry * 1.5f; r = sr_ * breathEnv * fry * 1.5f; break;
            }
            case ad::Brook:
            {
                const float pl = pinkL.next (wl), pr = pinkR.next (wr);
                bpL.tick (pl); bpR.tick (pr);
                // the gurgle: the hiss pulled up and down many times a second, differently left and right
                const float gl = gurgL.lp (std::abs (rng.bi())), gr = gurgR.lp (std::abs (rng.bi()));
                l = bpL.band * (0.25f + 2.6f * gl) * bedAmp * 0.55f; r = bpR.band * (0.25f + 2.6f * gr) * bedAmp * 0.55f;
                if (rng.uni() < bubbleRate / sr) spawnBubble (false);
                bubblesSample (l, r);
                break;
            }
            case ad::Night:
            {
                brL = brL * 0.996f + wl * 0.05f; brR = brR * 0.996f + wr * 0.05f;
                l = lpL.lp (brL) * 0.28f * bedAmp; r = lpR.lp (brR) * 0.28f * bedAmp;
                for (auto& c : cricket)
                {
                    if (! c.on) continue;
                    if (c.pulses <= 0)
                    {
                        if (--c.gap > 0) continue;
                        c.pulses = 3 + (int) (rng.uni() * 2.99f); c.pph = 0.f;     // a chirp: 3 to 5 pulses
                        c.prate = (26.f + 10.f * rng.uni()) / sr;
                        c.amp = 0.5f + 0.5f * rng.uni();
                    }
                    c.ph = wrap01 (c.ph + c.inc); c.pph += c.prate;
                    if (c.pph >= 1.f) { c.pph -= 1.f; if (--c.pulses <= 0) c.gap = (int) (c.gapMean * (0.7f + 0.6f * rng.uni()) * sr); }
                    const float u = c.pph / 0.6f;                                     // each pulse sounds for 60 % of its period
                    const float env = u < 1.f ? sin01 (u * 0.5f) : 0.f;
                    const float s = sin01 (c.ph) * env * env * c.amp * 0.34f;
                    l += s * c.pl; r += s * c.pr;
                }
                break;
            }
            case ad::Forest:
            {
                const float pl = pinkL.next (wl), pr = pinkR.next (wr);
                // leaves: the highs, fluttering (rectified, smoothed noise) under the gusts
                float hl = lpL.lp (pl); hl -= hpL.lp (hl); float hr = lpR.lp (pr); hr -= hpR.lp (hr);
                const float fl = gurgL.lp (std::abs (rng.bi())), fr = gurgR.lp (std::abs (rng.bi()));
                const float gg = gust * std::sqrt (gust) * 1.3f;
                brL = brL * 0.997f + wl * 0.05f; brR = brR * 0.997f + wr * 0.05f;
                rumL.tick (brL); rumR.tick (brR);
                l = hl * gg * (0.3f + 3.f * fl * bedAmp) * 1.6f + rumL.band * gust * 0.9f;
                r = hr * gg * (0.3f + 3.f * fr * bedAmp) * 1.6f + rumR.band * gust * 0.9f;
                break;
            }
            case ad::SoftRain:
            {
                const float pl = pinkL.next (wl), pr = pinkR.next (wr);
                float hl = lpL.lp (pl), hr = lpR.lp (pr); hl -= hpL.lp (hl); hr -= hpR.lp (hr);
                l = hl * 0.45f * bedAmp; r = hr * 0.45f * bedAmp;
                if (rng.uni() < rainRate / sr) spawnDrop (true);
                dropsSample (l, r);
                break;
            }
            case ad::Underwater:
            {
                brL = brL * 0.998f + wl * 0.045f; brR = brR * 0.998f + wr * 0.045f;
                rumL.tick (brL); rumR.tick (brR);
                subPh = wrap01 (subPh + subInc);
                const float hum = sin01 (subPh) * subAmp;
                l = rumL.low * 2.4f * wave[0] + hum; r = rumR.low * 2.4f * wave[1] + hum;
                if (rng.uni() < bubbleRate / sr) spawnBubble (true);
                bubblesSample (l, r);
                break;
            }
            case ad::Air:
            {
                const float pl = pinkL.next (wl), pr = pinkR.next (wr);
                bpL.tick (pl); bpR.tick (pr);
                l = (bpL.band + wl * 0.03f * bedT) * bedAmp * 0.9f; r = (bpR.band + wr * 0.03f * bedT) * bedAmp * 0.9f;
                break;
            }
            default: l = r = 0; break;
        }
    }

    void spawnDrop (bool soft)
    {
        for (auto& d : drops)
            if (! d.on)
            {
                d.on = true; d.f.reset();
                const float size = clampf (bedD * rng.uni() + (1.f - bedD) * 0.5f, 0.f, 1.f);
                if (soft)
                {
                    // a small tap on the glass: higher, shorter, gentler
                    d.f.set (1800.f * std::exp2 ((bedT - 0.3f) * 1.2f + (1.f - size) * 1.1f + 0.4f * rng.bi()), 5.f + 5.f * rng.uni());
                    d.env = 1.f; d.dec = std::exp (-1.f / ((0.002f + 0.006f * size) * sr)); d.exN = 1 + (int) (0.0008f * sr);
                    d.amp = (0.15f + 0.35f * rng.uni()) * (0.5f + size) * 0.8f;
                }
                else
                {
                    d.f.set (1300.f * std::exp2 ((bedT - 0.3f) * 2.f + (1.f - size) * 1.6f + 0.5f * rng.bi()), 9.f + 8.f * rng.uni());
                    d.env = 1.f; d.dec = std::exp (-1.f / ((0.004f + 0.02f * size) * sr)); d.exN = 1 + (int) (0.0012f * sr);
                    d.amp = (0.25f + 0.75f * rng.uni()) * (0.6f + size) * 0.6f;
                }
                const float pan = rng.uni(); d.pl = std::sqrt (1.f - pan); d.pr = std::sqrt (pan);
                return;
            }
    }
    inline void dropsSample (float& l, float& r)
    {
        for (auto& d : drops)
            if (d.on)
            {
                const float x = d.exN > 0 ? (--d.exN, rng.bi()) : 0.f;
                d.f.tick (x); const float s = d.f.band * d.env * d.amp;
                d.env *= d.dec; if (d.env < 1.0e-4f) d.on = false;
                l += s * d.pl; r += s * d.pr;
            }
    }
    /** A bubble rings at its size and rises in pitch as it nears the surface (brook: small and many; underwater: big and slow). */
    void spawnBubble (bool deep)
    {
        for (auto& b : bubbles)
            if (! b.on)
            {
                const float size = rng.uni();
                b.on = true; b.ph = 0.f; b.age = 0;
                b.f = deep ? 130.f * std::exp2 (bedT * 1.4f + 1.4f * size) : 420.f * std::exp2 (bedT * 2.2f + 1.6f * size);
                b.rise = (deep ? 2.f + 4.f * rng.uni() : 6.f + 18.f * rng.uni()) / sr;
                b.env = 1.f; b.dec = std::exp (-1.f / ((deep ? 0.03f + 0.06f * rng.uni() : 0.006f + 0.02f * rng.uni()) * sr));
                b.amp = (0.15f + 0.85f * rng.uni() * rng.uni()) * (deep ? 0.55f : 0.7f) * (0.6f + 0.6f * bedD);
                const float pan = 0.5f + 0.45f * rng.bi(); b.pl = std::sqrt (1.f - pan); b.pr = std::sqrt (pan);
                return;
            }
    }
    inline void bubblesSample (float& l, float& r)
    {
        const int att = juce::jmax (1, (int) (0.0008f * sr));
        for (auto& b : bubbles)
            if (b.on)
            {
                b.ph = wrap01 (b.ph + juce::jmin (0.45f, b.f / sr));
                const float s = sin01 (b.ph) * b.env * b.amp * (b.age < att ? (float) b.age / (float) att : 1.f);
                b.f *= 1.f + b.rise; b.env *= b.dec; ++b.age;
                if (b.env < 1.0e-4f) b.on = false;
                l += s * b.pl; r += s * b.pr;
            }
    }

    // ======================================================================== DRONE
    //  Each note of the chord has kUni voices. For most sounds they are detuned copies (a section, an ensemble); some
    //  instruments use them as their own parts: the pipe organ's ranks (8', 4', 2'), the harmonium's reeds (two 8' and
    //  a 4'), the tonewheel organ's one set of wheels. Tanpura, singing bowls and the Shepard tones play their own way.
    static float smooth01 (float x, float a, float b) { const float t = clampf ((x - a) / (b - a), 0.f, 1.f); return t * t * (3.f - 2.f * t); }

    void controlDrone (const AtmosParams& p)
    {
        const int chord = juce::jlimit (0, ad::kChords - 1, p.choice (ad::drnChord));
        static const int I[ad::kChords][4] = {
            { 0 }, { 0, 7 }, { 0, 12 }, { 0, 3, 7 }, { 0, 5, 7 }, { 0, 6 }, { 0, 1, 2 }, { 0, 1, 6, 11 },
            { 0, 4, 7 }, { 0, 4, 7, 11 }, { 0, 4, 7, 14 }, { 0, 2, 7 }, { 0, 3, 7, 10 }, { 0, 3, 10, 14 },
            { 0, 7, 14, 16 },       // Heaven: the root, its fifth, and the ninth and third an octave up: wide open
            { 0, 7, 12, 19 } };     // Open: fifths and octaves stacked
        static const int N[ad::kChords] = { 1, 2, 2, 3, 3, 2, 3, 4, 3, 4, 4, 3, 4, 4, 4, 4 };
        numNotes = N[chord]; for (int k = 0; k < 4; ++k) interval[k] = I[chord][k];
        const int oct = juce::jlimit (0, 3, p.choice (ad::drnOct));
        drnOct = oct;
        int pc = ((p.key % 12) + 12) % 12;
        if (p.on (ad::drnFollow) && p.guitarHz > 0 && p.guitarConf > 0.7f)
        {
            const float gm = 69.f + 12.f * std::log2 (p.guitarHz / p.tuneHz);
            followPc = (((int) std::lround (gm)) % 12 + 12) % 12;
        }
        if (p.on (ad::drnFollow) && followPc >= 0) pc = followPc;
        const float target = (float) (24 + 12 * oct + pc);
        if (drnMidi < 0) drnMidi = target;
        drnMidi += (target - drnMidi) * (1.f - std::exp (-1.f / (0.6f * ctlRate)));    // slides to a new root
        rootShown.store (p.on (ad::drnOn) ? target : -1.f, std::memory_order_relaxed);

        // a new sound, root or chord, or the drone coming on: pipes speak with a chiff, bowls are struck, the tanpura starts its round
        const bool isOn = p.on (ad::drnOn);
        const bool fresh = drnVoice != voiceWas || std::abs (target - targetWas) > 0.01f || chord != chordWas || (isOn && ! drnOnWas);
        if (fresh)
        {
            for (auto& c : chiffEnv) c = 1.f;
            if (drnVoice != voiceWas)
            {
                for (int k = 0; k < kNotes; ++k) bowl[k].wait = 0.05f + 0.45f * (float) k + 0.2f * rng.uni();
                tanNext = 0; tanClock = 1.0e3f;
            }
        }
        voiceWas = drnVoice; targetWas = target; chordWas = chord; drnOnWas = isOn;

        const float det = p.get (ad::drnDetune) * 0.01f, tone = clampf (p.get (ad::drnTone) * 0.01f + evD * 0.2f, 0.f, 1.f), mot = p.get (ad::drnMotion) * 0.01f;
        drnTone = tone; drnMot = mot;
        shineAmt = p.get (ad::drnShine) * 0.01f; airAmt = p.get (ad::drnAir) * 0.01f;
        swellDr.setRate (0.05f + mot * 0.25f, ctlRate);
        // MOTION is a swell for most sounds; for these it moves something of their own (Leslie, tremulant, bellows, pace, strikes)
        const bool ownMotion = drnVoice == ad::Organ || drnVoice == ad::PipeOrgan || drnVoice == ad::Harmonium || drnVoice == ad::Tanpura || drnVoice == ad::Bowls;
        swell = ownMotion ? 1.f : 1.f + 0.35f * mot * swellDr.tick (rng);
        const float wid = clampf (p.get (ad::width) * 0.01f, 0.f, 1.f);
        for (int k = 0; k < kNotes; ++k) noteHz[k] = midiHz (drnMidi + (float) interval[juce::jmin (k, numNotes - 1)]);

        // AIR: breath around the drone, in its upper range
        airDr.setRate (0.12f + 0.3f * mot, ctlRate);
        airGl = 0.6f + 0.4f * airDr.tick (rng);
        const float airF = 1400.f * std::exp2 (tone * 2.3f);
        airL.set (airF, 0.8f); airR.set (airF * 1.07f, 0.8f);
        // SHINE: the chord an octave and two octaves up, each note glinting by itself
        for (int k = 0; k < numNotes; ++k)
        {
            shineInc[k][0] = juce::jmin (0.45f, 2.f * noteHz[k] / sr); shineInc[k][1] = juce::jmin (0.45f, 4.f * noteHz[k] / sr);
            shineDr[k].setRate (0.15f + 0.25f * (float) k * 0.3f, ctlRate);
            shineGl[k] = 0.55f + 0.45f * shineDr[k].tick (rng);
        }

        if (drnVoice == ad::Descent || drnVoice == ad::Ascent)
        {
            // Shepard tone: octave-spaced partials that glide forever under a fixed bell-shaped window
            const float speed = (0.02f + mot * 0.2f) / ctlRate;                 // octaves per control tick (12 s an octave at 30 %)
            shepPos += drnVoice == ad::Descent ? -speed : speed;
            if (shepPos < 0.f) { shepPos += 1.f; for (auto& sh : shep) { for (int k = 0; k < kShep - 1; ++k) sh.ph[k] = sh.ph[k + 1]; sh.ph[kShep - 1] = rng.uni(); } }
            if (shepPos >= 1.f) { shepPos -= 1.f; for (auto& sh : shep) { for (int k = kShep - 1; k > 0; --k) sh.ph[k] = sh.ph[k - 1]; sh.ph[0] = rng.uni(); } }
            for (int nI = 0; nI < numNotes; ++nI)
            {
                auto& sh = shep[nI];
                const float c0 = midiHz (12.f + (drnMidi - target));                 // C0, plus the slide to a new root
                const float pcOff = (float) ((pc + interval[nI]) % 12) / 12.f;
                for (int k = 0; k < kShep; ++k)
                {
                    const float y = (float) k + shepPos + pcOff;                       // octaves above C0
                    const float f = c0 * std::exp2 (y);
                    sh.inc[k] = f < sr * 0.45f ? f / sr : 0.f;
                    const float w = y / (float) kShep;
                    sh.amp[k] = w <= 0.f || w >= 1.f ? 0.f : 0.5f - 0.5f * std::cos (kTwoPi * w);
                    sh.pan[k] = 0.5f + 0.5f * wid * (k % 2 == 0 ? -0.6f : 0.6f) * (nI % 2 == 0 ? 1.f : -1.f);
                }
            }
            droneFilter (500.f * std::exp2 (tone * 4.5f), 0.6f);
            return;
        }
        if (drnVoice == ad::Tanpura) { controlTanpura (tone, det, mot, wid); return; }
        if (drnVoice == ad::Bowls)   { controlBowls (tone, det, mot, wid); droneFilter (9000.f + tone * 7000.f, 0.5f); return; }

        // the parts of the sounds that move by themselves
        if (drnVoice == ad::PipeOrgan)
        {
            // TONE draws the stops: a soft stopped flute, then principals (8' + 4'), then the 2' and a 16' bourdon (full organ)
            principal = smooth01 (tone, 0.12f, 0.5f); rank4 = 0.8f * smooth01 (tone, 0.25f, 0.6f); rank2 = 0.5f * smooth01 (tone, 0.6f, 0.95f);
            bourdon = 0.55f * smooth01 (tone, 0.7f, 1.f);
            tremPh = wrap01 (tremPh + (5.6f + mot * 0.9f) / ctlRate);           // MOTION = tremulant
            const float tw = sin01 (tremPh);
            tremAm = 1.f - mot * 0.2f * (0.5f + 0.5f * tw); tremPitch = tw * mot * 0.03f;
            for (auto& c : chiffEnv) c *= std::exp (-(float) kCtl / (0.06f * sr));
            for (int k = 0; k < numNotes; ++k) chiffF[k].set (juce::jmin (sr * 0.4f, noteHz[k] * 4.f), 2.5f);
            chiffAmt = 0.08f + 0.22f * tone; windAmt = 0.004f + 0.008f * tone;
            droneFilter (1700.f * std::exp2 (tone * 3.2f), 0.55f);
        }
        else if (drnVoice == ad::Harmonium)
        {
            bellowsDr.setRate (0.08f, ctlRate);
            bellowsPh = wrap01 (bellowsPh + (0.1f + mot * 0.28f) * (0.85f + 0.3f * bellowsDr.tick (rng)) / ctlRate);   // MOTION = the bellows
            const float pr = 0.5f + 0.5f * sin01 (bellowsPh);
            bellowsAm = 1.f - mot * 0.32f * (1.f - pr); bellowsPitch = -(1.f - pr) * mot * 0.05f;
            droneFilter (1250.f * std::exp2 (tone * 2.6f), 0.7f);
        }

        int vi = 0;
        for (int nI = 0; nI < numNotes; ++nI)
            for (int u = 0; u < kUni; ++u, ++vi)
            {
                auto& v = voice[vi];
                v.drift.setRate (0.07f + 0.1f * mot, ctlRate);
                v.vph = wrap01 (v.vph + v.vrate / ctlRate);
                const float dr = v.drift.tick (rng);
                float spread = (float) (u - 1) * det * 0.22f + dr * det * 0.07f;   // semitones
                float mul = 1.f, amp = 1.f, vib = 0.f, d2 = 0.f;
                switch (drnVoice)
                {
                    // one set of tonewheels, no detune: the scanner chorus (DETUNE) and the Leslie (MOTION) move it
                    case ad::Organ:     spread = 0.f; amp = u == 1 ? 1.f : 0.f; break;
                    case ad::PipeOrgan: mul = (float) (1 << u); amp = u == 0 ? 1.f : u == 1 ? rank4 : rank2;
                                        spread = (float) (u - 1) * det * 0.06f + dr * det * 0.02f + tremPitch; break;   // DETUNE = a celeste
                    case ad::Harmonium: mul = u == 2 ? 2.f : 1.f; amp = u == 2 ? 0.2f + 0.45f * drnTone : 1.f;
                                        spread = (u == 0 ? -(0.04f + det * 0.2f) : u == 1 ? 0.f : det * 0.05f) + bellowsPitch; break;   // DETUNE = musette
                    case ad::Strings:   vib = sin01 (v.vph) * (0.05f + mot * 0.16f); d2 = (u == 1 ? 0.07f : u == 0 ? -0.11f : 0.13f) * (0.5f + det); break;
                    case ad::Choir:     vib = sin01 (v.vph) * (0.035f + mot * 0.12f); break;
                    case ad::Angels:    vib = sin01 (v.vph) * (0.03f + mot * 0.08f); spread *= 0.6f; break;
                    case ad::Flute:
                    {
                        // flutes play from about A3 up: a low drone goes up by octaves (three near-unison flutes down there just beat slowly)
                        vib = sin01 (v.vph) * (0.03f + mot * 0.12f); spread = (float) (u - 1) * (0.05f + det * 0.12f) + dr * det * 0.05f;
                        const float base = midiHz (drnMidi + (float) interval[nI]);
                        while (base * mul < 220.f && mul < 8.f) mul *= 2.f;
                        break;
                    }
                    case ad::Pad:       spread = (float) (u - 1) * det * 0.16f + dr * det * 0.06f; d2 = 0.06f + det * 0.12f; break;
                    default: break;
                }
                const float f = midiHz (drnMidi + (float) interval[nI] + spread + vib) * mul;
                v.inc = juce::jmin (f / sr, 0.45f);
                v.inc2 = v.inc * std::exp2 (d2 / 12.f);
                v.amp = f < sr * 0.45f ? amp : 0.f;
                if (drnVoice == ad::Flute) v.bn.set (juce::jmin (sr * 0.4f, f * 2.1f), 2.5f);
                const float pos = numNotes * kUni > 1 ? ((float) vi / (float) (numNotes * kUni - 1)) * 2.f - 1.f : 0.f;
                const float pan = 0.5f + 0.5f * pos * wid * (u == 1 ? 0.4f : 1.f);
                v.pl = std::sqrt (1.f - pan); v.pr = std::sqrt (pan);
            }
        numVoices = vi;
        voiceNorm = 1.f / std::sqrt ((float) juce::jmax (1, numVoices));

        switch (drnVoice)
        {
            case ad::Organ:
            {
                // drawbars 16', 5 1/3', 8', 4', 2 2/3', 2', 1 3/5', 1 1/3', 1': a soft flute (00 8400 000), jazz (88 8000 000),
                // gospel (88 8866 000), then the full organ (88 8888 668) as TONE goes up
                static const float R[4][9] = { { 0, 0, 8, 4, 0, 0, 0, 0, 0 }, { 8, 8, 8, 0, 0, 0, 0, 0, 0 }, { 8, 8, 8, 8, 6, 6, 0, 0, 0 }, { 8, 8, 8, 8, 8, 8, 6, 6, 8 } };
                const float x = tone * 3.f; const int a = juce::jmin (2, (int) x); const float fr = x - (float) a;
                for (int k = 0; k < 9; ++k)
                {
                    const float d = R[a][k] + (R[a + 1][k] - R[a][k]) * fr;                     // drawbar position 0..8, 3 dB a step
                    drawbar[k] = d < 0.05f ? 0.f : std::pow (10.f, (d - 8.f) * 0.15f) * juce::jmin (1.f, d);
                }
                droneFilter (11000.f, 0.5f);
                // the Leslie: stopped at MOTION 0, slow (chorale) up to 60 %, fast (tremolo) above; the horn speeds up in about
                // a second, the heavy drum takes three or four
                const float hornT = mot < 0.06f ? 0.f : mot < 0.6f ? 0.6f + 0.45f * (mot - 0.06f) / 0.54f : 6.7f;
                const float drumT = mot < 0.06f ? 0.f : mot < 0.6f ? 0.5f + 0.3f * (mot - 0.06f) / 0.54f : 5.8f;
                hornHz += (hornT - hornHz) * (1.f - std::exp (-1.f / ((hornT > hornHz ? 0.9f : 0.7f) * ctlRate)));
                drumHz += (drumT - drumHz) * (1.f - std::exp (-1.f / ((drumT > drumHz ? 3.6f : 2.6f) * ctlRate)));
                scanDepth = det;
                break;
            }
            case ad::Strings:
                droneFilter (350.f * std::exp2 (tone * 4.6f + 0.25f * swellDr.cur * mot), 0.8f);
                bowAmt = 0.03f * (0.7f + 0.3f * swell) * (0.5f + tone);
                break;
            case ad::Choir: case ad::Angels:
            {
                // vowels (F1..F4) of an ensemble: a, o, u, e, i. Choir drifts between a, o, u and e; Angels between o and u (a soft "ooh")
                static const float V[5][4] = { { 800, 1150, 2900, 3900 }, { 450, 800, 2830, 3800 }, { 325, 700, 2530, 3500 }, { 400, 1600, 2700, 3300 }, { 290, 1900, 2600, 3600 } };
                const bool angels = drnVoice == ad::Angels;
                vowelDr.setRate (0.04f + mot * 0.3f, ctlRate);
                const float d = vowelDr.tick (rng) * 0.5f + 0.5f;
                const float x = angels ? 1.f + d : d * 3.f;
                const int a = juce::jlimit (0, 3, (int) x); const float fr = x - (float) a;
                const float shift = std::exp2 ((tone - 0.5f) * (angels ? 0.5f : 0.8f));
                for (int k = 0; k < 4; ++k)
                {
                    const float fk = (V[a][k] + (V[a + 1][k] - V[a][k]) * fr) * shift, q = 6.f + 2.5f * (float) k;
                    formL[k].set (fk, q); formR[k].set (fk * 1.012f, q);
                }
                glotL.setLowpass (sr, (angels ? 650.f : 1500.f) * (1.f + tone)); glotR.a = glotL.a;
                choirBreath = (angels ? 0.1f : 0.04f) + airAmt * 0.2f;
                droneFilter (angels ? 3500.f + tone * 5000.f : 5000.f + tone * 6000.f, 0.6f);
                break;
            }
            case ad::Pad:
                padDr.setRate (0.04f + mot * 0.12f, ctlRate);
                droneFilter (220.f * std::exp2 (tone * 5.2f + mot * 1.1f * padDr.tick (rng)), 0.95f);
                break;
            case ad::Flute:
                fluteH2 = 0.12f + 0.25f * tone; fluteH3 = 0.07f * tone;
                fluteBreath = 0.35f + 0.5f * tone + airAmt * 1.2f;
                droneFilter (2600.f * std::exp2 (tone * 2.2f), 0.6f);
                break;
            case ad::Glass:   droneFilter (2500.f * std::exp2 (tone * 2.f), 0.6f); fmIndex = 0.04f + tone * tone * 0.45f; break;
            case ad::Abyss:
                droneFilter (90.f * std::exp2 (tone * 3.6f), 1.1f);
                abyssDrive = 1.5f + tone * 5.f;
                growlDr.setRate (6.f + mot * 30.f, ctlRate);
                growl = 1.f - 0.5f * mot * (0.5f + 0.5f * growlDr.tick (rng));
                break;
            default: break;
        }
    }

    // ---- tanpura: four strings plucked round and round (Pa, Sa, Sa, low Sa), each buzzing on its curved bridge (the jawari)
    void controlTanpura (float tone, float det, float mot, float wid)
    {
        // where a tanpura sits: the root between A2 and G#3 (Sub, Low) or E3 and D#4 (Mid, High)
        float root = drnMidi; const float lo = drnOct <= 1 ? 45.f : 52.f;
        while (root < lo) root += 12.f;
        while (root >= lo + 12.f) root -= 12.f;
        // the first string: the chord's second note an octave down (a fifth by default: Pa), then Sa, Sa and Sa an octave down
        const int second = numNotes > 1 ? interval[1] % 12 : 7;
        const float notes[4] = { root + (float) (second == 0 ? 7 : second) - 12.f, root, root, root - 12.f };
        for (int k = 0; k < 4; ++k)
        {
            auto& t = tan[k];
            const float f = midiHz (notes[k]);
            t.a = 0.3f - 0.24f * tone;                                              // the bridge: brighter as TONE goes up
            t.D = juce::jlimit (2.f, (float) t.line.size() - 4.f, sr / f - 1.f - t.a / (1.f - t.a));
            t.g = std::pow (10.f, -3.f / (12.f * f));                               // a string rings about 12 s
            t.jEnv *= std::exp (-(float) kCtl / (1.7f * sr));                       // the buzz sweeps down as the note settles
            t.jw.set (650.f + (2400.f + 2800.f * tone) * t.jEnv, 2.2f);
            const float pan = 0.5f + 0.5f * wid * (k == 0 ? -0.55f : k == 1 ? -0.18f : k == 2 ? 0.18f : 0.5f);
            t.pl = std::sqrt (1.f - pan); t.pr = std::sqrt (pan);
        }
        jawari = 0.25f + 0.75f * det;                                               // DETUNE = jawari (the buzz)
        // the round: four plucks evenly spaced, a little human (MOTION = pace: about 6 s to 2.8 s a round)
        tanClock += 1.f / ctlRate;
        if (tanClock >= tanGap)
        {
            auto& t = tan[tanNext];
            t.exN = (int) (t.D + 2.f); t.ex = 0.5f * (0.85f + 0.15f * rng.uni()) * (tanNext == 3 ? 1.15f : 1.f);
            t.exLp.setLowpass (sr, 1300.f * std::exp2 (tone * 3.f)); t.jEnv = 1.f;
            tanNext = (tanNext + 1) % 4; tanClock = 0.f;
            tanGap = (6.f - 3.2f * mot) * 0.25f * (1.f + 0.05f * rng.bi());
        }
        droneFilter (3000.f * std::exp2 (tone * 2.2f), 0.5f);
    }
    inline void tanpuraSample (float& l, float& r)
    {
        for (auto& t : tan)
        {
            float ex = 0;
            if (t.exN > 0) { --t.exN; ex = t.exLp.lp (rng.bi()) * t.ex; }
            const float x = t.line.readLinear (t.D);
            t.z = (1.f - t.a) * x + t.a * t.z;                                      // the string's loss, brighter or darker
            const float y = t.g * t.z + ex;
            t.line.push (y);
            t.jw.tick (y);
            const float s = (y + fastTanh (t.jw.band * 5.f) * 0.2f * jawari * (0.3f + 0.7f * t.jEnv)) * 3.f;
            l += s * t.pl; r += s * t.pr;
        }
    }

    // ---- singing bowls: one bowl per note of the chord, rubbed (a slow swell) and struck now and then (MOTION)
    void controlBowls (float tone, float det, float mot, float wid)
    {
        static const float R[4] = { 1.f, 2.71f, 5.15f, 8.36f };                     // a bowl's modes, not harmonics
        for (int k = 0; k < numNotes; ++k)
        {
            auto& b = bowl[k];
            float m = drnMidi + (float) interval[k] + 12.f;
            while (m < 50.f) m += 12.f;                                             // where bowls sing
            while (m > 79.f) m -= 12.f;
            const float f = midiHz (m);
            for (int j = 0; j < 4; ++j)
            {
                // each mode is two, a hair apart (the bowl isn't perfectly round): the slow "wah-wah" beating. DETUNE = beat
                const float beat = (0.35f + det * 2.4f) * (1.f + 0.8f * (float) j);
                const float fj = f * R[j];
                b.inc[j][0] = juce::jmin (0.45f, (fj - beat * 0.5f) / sr); b.inc[j][1] = juce::jmin (0.45f, (fj + beat * 0.5f) / sr);
            }
            b.rubDr.setRate (0.05f + 0.2f * mot, ctlRate);
            b.rub = 0.55f + 0.45f * b.rubDr.tick (rng);
            b.strike *= std::exp (-(float) kCtl / (2.4f * sr));
            b.strikeHi *= std::exp (-(float) kCtl / (0.6f * sr));
            b.wait -= 1.f / ctlRate;
            if (b.wait <= 0.f)
            {
                b.strike = juce::jmin (1.4f, b.strike + 0.9f); b.strikeHi = 1.f;
                b.wait = (3.f + 9.f * (1.f - mot)) * (0.6f + 0.8f * rng.uni());
            }
            const float pan = numNotes > 1 ? 0.5f + 0.5f * wid * (((float) k / (float) (numNotes - 1)) * 1.4f - 0.7f) : 0.5f;
            b.pl = std::sqrt (1.f - pan); b.pr = std::sqrt (pan);
        }
        bowlUpper = 0.25f + 0.9f * tone;                                            // TONE = how much the upper modes ring
    }
    inline void bowlsSample (float& l, float& r)
    {
        static const float A[4] = { 1.f, 0.5f, 0.28f, 0.15f };
        const float sm = 1.f - std::exp (-1.f / (0.004f * sr));                    // a soft mallet: the strike comes in over a few ms
        for (int k = 0; k < numNotes; ++k)
        {
            auto& b = bowl[k];
            float s = 0;
            for (int j = 0; j < 4; ++j)
            {
                b.ph[j][0] = wrap01 (b.ph[j][0] + b.inc[j][0]); b.ph[j][1] = wrap01 (b.ph[j][1] + b.inc[j][1]);
                const float a = A[j] * (j == 0 ? b.rub * 0.8f + b.strike * 0.5f : bowlUpper * (b.rub * 0.35f + b.strike * 0.4f + b.strikeHi * 0.9f));
                b.amp[j] += (a - b.amp[j]) * sm;
                s += (sin01 (b.ph[j][0]) + sin01 (b.ph[j][1])) * b.amp[j];
            }
            l += s * b.pl * 0.3f; r += s * b.pr * 0.3f;
        }
    }

    // ---- the tonewheel organ's scanner chorus and Leslie speaker (after the drone's filter)
    inline void leslie (float& l, float& r)
    {
        const float x = 0.5f * (l + r);
        // scanner chorus (DETUNE): a delay swept at 7 Hz, mixed with the dry organ
        scanLine.push (x);
        scanPh = wrap01 (scanPh + 7.f / sr);
        const float ch = scanLine.readLinear ((0.25f + 0.55f * (0.5f + 0.5f * sin01 (scanPh))) * 0.001f * sr);
        const float y = x * (1.f - 0.5f * scanDepth) + ch * 0.5f * scanDepth;
        // crossover at 800 Hz: the drum rotor takes the lows, the horn the highs
        const float lo = lesLp.lp (y), hi = y - lo;
        hornTh = wrap01 (hornTh + hornHz / sr); drumTh = wrap01 (drumTh + drumHz / sr);
        hornLine.push (hi);
        // two mics on either side: the horn comes nearer (louder, a hair earlier) and goes away; the pitch bends with it
        const float exc = 0.00026f * sr;
        const float cL = sin01 (wrap01 (hornTh + 0.25f)), cR = sin01 (wrap01 (hornTh + 0.25f + 0.42f));
        const float hl = hornLine.readLinear (2.f + exc * (1.f - cL)) * (0.68f + 0.32f * cL);
        const float hr = hornLine.readLinear (2.f + exc * (1.f - cR)) * (0.68f + 0.32f * cR);
        const float dL = 0.82f + 0.18f * sin01 (wrap01 (drumTh + 0.25f)), dR = 0.82f + 0.18f * sin01 (wrap01 (drumTh + 0.75f));
        l = hl + lo * dL; r = hr + lo * dR;
    }

    inline void droneSample (float& l, float& r)
    {
        float xl = 0, xr = 0;
        if (drnVoice == ad::Descent || drnVoice == ad::Ascent)
        {
            for (int nI = 0; nI < numNotes; ++nI)
            {
                auto& sh = shep[nI];
                for (int k = 0; k < kShep; ++k)
                {
                    if (sh.inc[k] <= 0.f || sh.amp[k] <= 0.f) continue;
                    sh.ph[k] = wrap01 (sh.ph[k] + sh.inc[k]);
                    const float ph = sh.ph[k];
                    const float s = (sin01 (ph) + drnTone * 0.35f * sin01 (wrap01 (ph * 2.f))) * sh.amp[k];
                    xl += s * (1.f - sh.pan[k]); xr += s * sh.pan[k];
                }
            }
            dLpL.tick (xl); dLpR.tick (xr);
            const float g = 0.42f / std::sqrt ((float) numNotes) * swell;
            l = dLpL.low * g; r = dLpR.low * g;
            airSample (l, r);
            return;
        }
        float sheenL = 0, sheenR = 0;
        if (drnVoice == ad::Tanpura) tanpuraSample (xl, xr);
        else if (drnVoice == ad::Bowls) bowlsSample (xl, xr);
        else
        {
            for (int i = 0; i < numVoices; ++i)
            {
                auto& v = voice[i];
                if (v.amp <= 0.f) continue;
                v.ph += v.inc; if (v.ph >= 1.f) v.ph -= 1.f;
                const auto h = [&v] (float k) { return v.inc * k < 0.45f ? sin01 (wrap01 (v.ph * k)) : 0.f; };   // a harmonic, if it fits
                float s = 0;
                switch (drnVoice)
                {
                    case ad::Organ:
                    {
                        // every drawbar is a whole multiple of the 16' wheel
                        static const float H[9] = { 1, 3, 2, 4, 6, 8, 10, 12, 16 };
                        v.ph2 = wrap01 (v.ph2 + v.inc * 0.5f);
                        const float lim = 0.45f / juce::jmax (1.0e-6f, v.inc * 0.5f);
                        for (int k = 0; k < 9; ++k) if (drawbar[k] > 0.f && H[k] < lim) s += drawbar[k] * sin01 (wrap01 (v.ph2 * H[k]));
                        s *= 0.28f; break;
                    }
                    case ad::PipeOrgan:
                    {
                        // a stopped flute (the fundamental and a little twelfth) opening into a principal (a full row of harmonics)
                        s = sin01 (v.ph) + (1.f - principal) * 0.1f * h (3.f)
                          + principal * (0.42f * h (2.f) + 0.26f * h (3.f) + 0.15f * h (4.f) + 0.08f * h (5.f));
                        if (v.u == 0 && bourdon > 0.f) { v.ph2 = wrap01 (v.ph2 + v.inc * 0.5f); s += bourdon * sin01 (v.ph2); }
                        s *= 0.45f * v.amp; break;
                    }
                    case ad::Strings:
                        v.ph2 = wrap01 (v.ph2 + v.inc2);
                        s = (saw (v.ph, v.inc) + 0.8f * saw (v.ph2, v.inc2)) * 0.38f; break;
                    case ad::Pad:
                        v.ph2 = wrap01 (v.ph2 + v.inc2);
                        s = (saw (v.ph, v.inc) + 0.75f * saw (v.ph2, v.inc2)) * 0.4f;
                        if (v.u == 1) { v.ph3 = wrap01 (v.ph3 + v.inc * 0.5f); s += 0.35f * sin01 (v.ph3); }
                        break;
                    case ad::Choir:
                        s = saw (v.ph, v.inc) * 0.55f; break;
                    case ad::Angels:
                    {
                        s = (sin01 (v.ph) * 0.7f + saw (v.ph, v.inc) * 0.3f) * 0.6f;
                        const float o = h (2.f) * 0.16f;                                 // a soft octave above, outside the vowels
                        sheenL += o * v.pl; sheenR += o * v.pr; break;
                    }
                    case ad::Harmonium:
                        s = (saw (v.ph, v.inc) - saw (wrap01 (v.ph + 0.34f), v.inc)) * 0.42f * v.amp; break;   // a free reed: a narrow pulse
                    case ad::Flute:
                        v.bn.tick (rng.bi());
                        s = (sin01 (v.ph) + fluteH2 * h (2.f) + fluteH3 * h (3.f) + v.bn.band * fluteBreath) * 0.5f; break;
                    case ad::Glass:
                    {
                        v.ph2 = wrap01 (v.ph2 + v.inc * 3.51f);
                        s = sin01 (wrap01 (v.ph + fmIndex * sin01 (v.ph2))) * 0.8f; break;
                    }
                    case ad::Abyss:
                    {
                        v.ph2 = wrap01 (v.ph2 + v.inc * 0.5f);
                        s = sin01 (v.ph2) * 0.9f + sin01 (v.ph) * 0.5f + saw (v.ph, v.inc) * 0.3f; break;
                    }
                    default: break;
                }
                xl += s * v.pl; xr += s * v.pr;
            }
            xl *= voiceNorm; xr *= voiceNorm; sheenL *= voiceNorm; sheenR *= voiceNorm;
        }
        switch (drnVoice)
        {
            case ad::Choir: case ad::Angels:
            {
                // the voices through a throat (a gentle lowpass) and four vowel resonances, with some breath
                static const float W[4] = { 1.f, 0.65f, 0.4f, 0.28f };
                const float il = glotL.lp (xl) + rng.bi() * choirBreath * 0.3f, ir = glotR.lp (xr) + rng.bi() * choirBreath * 0.3f;
                float yl = 0, yr = 0;
                for (int k = 0; k < 4; ++k) { formL[k].tick (il); formR[k].tick (ir); yl += formL[k].band * W[k]; yr += formR[k].band * W[k]; }
                xl = yl * 2.6f; xr = yr * 2.6f; break;
            }
            case ad::Strings:
            {
                // bow noise, and the wooden body: an air resonance, a nasal dip and the bridge's brightness
                bowL.tick (rng.bi()); bowR.tick (rng.bi());
                xl += bowL.band * bowAmt; xr += bowR.band * bowAmt;
                for (int k = 0; k < 3; ++k) { xl = bodyL[k].process (xl); xr = bodyR[k].process (xr); }
                break;
            }
            case ad::PipeOrgan:
            {
                // the chiff of the pipes speaking, and the wind in the chest
                const float n = rng.bi();
                float c = 0;
                for (int k = 0; k < numNotes; ++k) if (chiffEnv[k] > 1.0e-4f) { chiffF[k].tick (n); c += chiffF[k].band * chiffEnv[k]; }
                const float w = windHp.hp (rng.bi()) * windAmt;
                xl = (xl + c * chiffAmt + w) * tremAm; xr = (xr + c * chiffAmt * 0.9f + w * 0.9f) * tremAm; break;
            }
            case ad::Harmonium:
                for (int k = 0; k < 2; ++k) { xl = reedL[k].process (xl); xr = reedR[k].process (xr); }
                xl *= bellowsAm; xr *= bellowsAm; break;
            case ad::Abyss: xl = fastTanh (xl * abyssDrive) * growl; xr = fastTanh (xr * abyssDrive) * growl; break;
            default: break;
        }
        dLpL.tick (xl); dLpR.tick (xr);
        l = dLpL.low * swell; r = dLpR.low * swell;
        if (drnVoice == ad::Organ) leslie (l, r);
        l += sheenL; r += sheenR;
        shineSample (l, r);
        airSample (l, r);
    }
    // SHINE: the notes an octave and two up, softly glinting (not the Shepard tones: they have no notes to shine)
    inline void shineSample (float& l, float& r)
    {
        if (shineAmt <= 0.001f) return;
        float sl = 0, sr_ = 0;
        for (int k = 0; k < numNotes; ++k)
        {
            shinePh[k][0] = wrap01 (shinePh[k][0] + shineInc[k][0]); shinePh[k][1] = wrap01 (shinePh[k][1] + shineInc[k][1]);
            const float s = (sin01 (shinePh[k][0]) + 0.4f * sin01 (shinePh[k][1])) * shineGl[k];
            if (k % 2 == 0) { sl += s * 0.8f; sr_ += s * 0.45f; } else { sl += s * 0.45f; sr_ += s * 0.8f; }
        }
        const float g = shineAmt * shineAmt * 0.11f / std::sqrt ((float) numNotes);
        l += shineLpL.lp (sl) * g; r += shineLpR.lp (sr_) * g;
    }
    inline void airSample (float& l, float& r)
    {
        if (airAmt <= 0.001f) return;
        airL.tick (airPL.next (rng.bi())); airR.tick (airPR.next (rng.bi()));
        const float g = airAmt * airAmt * 0.5f * airGl * swell;
        l += airL.band * g; r += airR.band * g;
    }

    // ======================================================================== TEXTURE
    void controlTexture (const AtmosParams& p)
    {
        texD = clampf (p.get (ad::texDensity) * 0.01f + evT * 0.25f, 0.f, 1.f);
        texTn = p.get (ad::texTone) * 0.01f; texC = p.get (ad::texChaos) * 0.01f;
        // how often things happen (per second) at this density
        //                                         Vinyl  Fire  Drips Metal Shimmer Whispers Heart Knocks Moans Flesh Chimes Birds MusicBox Bells Harp  Stars Kalimba Leaves
        static const float lo[ad::kTextures] = { 0.15f, 0.8f, 0.25f, 0.06f, 0.6f,  0.15f,   0.f,  0.06f, 0.05f, 0.12f, 0.05f, 0.05f, 0.04f, 0.02f, 0.05f, 0.25f, 0.06f, 1.2f };
        static const float hi[ad::kTextures] = { 2.0f, 14.f,  7.f,   1.4f,  22.f,  2.8f,    0.f,  1.2f,  0.9f,  3.5f,  0.7f,  1.0f,  0.45f, 0.25f, 0.6f,  6.0f,  1.0f,  22.f };
        const int tt = juce::jlimit (0, ad::kTextures - 1, texType);
        if (tt != texTypeWas) { texTypeWas = tt; nextEvent = juce::jmin (nextEvent, (int) (0.05f * sr)); }
        texRate = lo[tt] + (hi[tt] - lo[tt]) * texD * texD;
        if (tt == ad::Heartbeat) texRate = (42.f + texD * 118.f) / 60.f;      // beats per second
        texLp.setLowpass (sr, 600.f * std::exp2 (texTn * 5.f));
        roarLp.setLowpass (sr, 250.f + texTn * 900.f); roarLp2.a = roarLp.a;
        vinHp.setLowpass (sr, 900.f);
        roarDr.setRate (0.3f + texD * 1.5f, ctlRate);
        roarAmp = texType == ad::Fire ? (0.35f + 0.45f * texD) * (0.6f + 0.4f * roarDr.tick (rng)) : 0.f;
        vinylRate = texType == ad::Vinyl ? 8.f + texD * texD * 160.f : 0.f;
    }

    inline void textureSample (float& l, float& r, float& sendL, float& sendR)
    {
        // schedule: CHAOS 0 = like clockwork, 100 = fully random
        if (texSm > 1.0e-5f && texRate > 0.f)
        {
            if (--nextEvent <= 0)
            {
                spawnTexture();
                const float mean = sr / texRate;
                if (texType == ad::Heartbeat)
                {
                    // a heart keeps time: CHAOS adds a stagger, and above half it skips and stumbles
                    nextEvent = juce::jmax (1, (int) (mean * (1.f + 0.18f * texC * rng.bi())));
                    if (texC > 0.5f && rng.uni() < (texC - 0.5f) * 0.35f) nextEvent += (int) (mean * (0.4f + 0.5f * rng.uni()));
                }
                else nextEvent = juce::jmax (1, (int) (mean * ((1.f - texC) + texC * rng.expo())));
            }
        }
        // continuous parts
        float cl = 0, cr = 0;
        if (vinylRate > 0.f)
        {
            if (rng.uni() < vinylRate / sr) vinylClick = rng.bi() * (0.3f + 0.7f * std::pow (rng.uni(), 3.f)) * 2.5f;
            const float c = vinylClick; vinylClick *= 0.35f;
            const float hiss = rng.bi() * 0.012f;
            const float x = texLp.lp (c + hiss); const float y = x - vinHp.lp (x);
            cl += y; cr += y * 0.95f;
        }
        if (roarAmp > 0.f)
        {
            brT = brT * 0.996f + rng.bi() * 0.06f; brT2 = brT2 * 0.996f + rng.bi() * 0.06f;
            cl += roarLp.lp (brT) * roarAmp * 1.3f; cr += roarLp2.lp (brT2) * roarAmp * 1.3f;
        }
        float el = 0, er = 0, sl = 0, sr2 = 0;
        anyEvents = false;
        for (auto& e : ev)
        {
            if (e.kind < 0) continue;
            anyEvents = true;
            if (e.wait > 0) { --e.wait; continue; }
            const float s = renderEvent (e);
            el += s * e.pl; er += s * e.pr;
            sl += s * e.pl * e.send; sr2 += s * e.pr * e.send;
        }
        l = cl + el; r = cr + er; sendL = sl; sendR = sr2;
    }

    struct Ev
    {
        int kind = -1, wait = 0, age = 0, len = 0, sub = 0;
        float amp = 0, pl = 0.7f, pr = 0.7f, send = 0;
        float ph = 0, ph2 = 0, inc = 0, inc2 = 0, f0 = 0, f1 = 0, env = 0, dec = 0.999f, x1 = 0, x2 = 0;
        Svf a, b, c;
        Biquad m[5]; float mg[5] {};
        float pp[6] {}, pi[6] {}, pa[6] {}, pe[6] {}, pd[6] {}; int np = 0;     // partials: phase, increment, amplitude, envelope, decay
    };
    /** The scale's pentatonic (SIREN's KEY and SCALE): the major one for Major, a minor one otherwise. */
    const int* pentatonic() const
    {
        static const int maj[5] = { 0, 2, 4, 7, 9 }, mnr[5] = { 0, 3, 5, 7, 10 }, hm[5] = { 0, 2, 3, 7, 8 };
        return prm->scale == 0 ? maj : prm->scale == 4 ? hm : mnr;
    }
    /** A note of that pentatonic: degree 0 is the key's root at or below 'nearMidi', counting up from there. */
    float keyNote (int nearMidi, int degree) const
    {
        const int key = ((prm->key % 12) + 12) % 12;
        const int root = nearMidi - (((nearMidi - key) % 12) + 12) % 12;
        const int d = juce::jmax (0, degree);
        return (float) (root + pentatonic()[d % 5] + 12 * (d / 5));
    }
    /** A struck or plucked thing: partials at f * R, amplitudes A, ringing T seconds (to -60 dB), after an attack (s). */
    void partials (Ev& e, float f, const float* R, const float* A, const float* T, int n, float amp, float attack)
    {
        e.np = juce::jmin (n, 6);
        for (int k = 0; k < e.np; ++k)
        {
            const float inc = f * R[k] / sr;
            e.pi[k] = juce::jmin (inc, 0.45f); e.pa[k] = inc < 0.45f ? A[k] : 0.f; e.pe[k] = 1.f; e.pp[k] = 0.f;
            e.pd[k] = std::exp (-6.9f / (T[k] * sr));
        }
        e.amp = amp; e.x1 = attack;
    }

    Ev* takeEvent()
    {
        for (auto& e : ev) if (e.kind < 0) return &e;
        // all busy: steal the oldest
        Ev* o = &ev[0]; for (auto& e : ev) if (e.age > o->age) o = &e;
        return o;
    }
    void setPan (Ev& e, float pan) { pan = clampf (pan, 0.f, 1.f); e.pl = std::sqrt (1.f - pan); e.pr = std::sqrt (pan); }

    void spawnTexture()
    {
        shownEvents.store (shownEvents.load (std::memory_order_relaxed) + 1, std::memory_order_relaxed);
        const float wide = clampf (prm->get (ad::width) * 0.01f, 0.f, 1.f);
        const float pan = 0.5f + 0.5f * wide * rng.bi();
        switch (texType)
        {
            case ad::Vinyl:
            {
                // a pop: a low thump with a click
                auto& e = *takeEvent(); e = {}; e.kind = ad::Vinyl; e.sub = 0;
                e.amp = 0.35f + 0.4f * rng.uni(); e.env = 1; e.dec = std::exp (-1.f / (0.012f * sr)); e.len = (int) (0.08f * sr);
                e.a.setSampleRate (sr); e.a.set (90.f + 120.f * rng.uni(), 1.2f); e.ph2 = 1.f;
                setPan (e, 0.5f + 0.3f * rng.bi()); e.send = 0.1f; break;
            }
            case ad::Fire:
            {
                // a crackle: a quick cluster of tiny snaps
                auto& e = *takeEvent(); e = {}; e.kind = ad::Fire;
                e.len = (int) ((0.03f + 0.12f * rng.uni()) * sr); e.amp = 0.4f + 0.6f * rng.uni();
                e.a.setSampleRate (sr); e.a.set (1400.f * std::exp2 (texTn * 1.6f + 0.6f * rng.bi()), 1.4f);
                e.f0 = 0.02f + 0.08f * texD;   // chance of a snap per sample
                setPan (e, pan); e.send = 0.15f; break;
            }
            case ad::Drips:
            {
                // a water drop: a sine that bends up fast and dies, deep in a cave
                auto& e = *takeEvent(); e = {}; e.kind = ad::Drips;
                e.f0 = 450.f * std::exp2 (texTn * 2.2f + (texC * 0.8f + 0.2f) * rng.bi());
                e.env = 1; e.dec = std::exp (-1.f / ((0.025f + 0.05f * rng.uni()) * sr)); e.len = (int) (0.4f * sr);
                e.amp = 0.35f + 0.65f * rng.uni(); setPan (e, pan); e.send = 1.4f; break;
            }
            case ad::Metal:
            {
                // a creak, a groan or a hit: a struck or scraped metal plate (5 inharmonic modes)
                auto& e = *takeEvent(); e = {}; e.kind = ad::Metal;
                const bool scrape = rng.uni() < 0.35f + 0.5f * (1.f - texC);
                e.sub = scrape ? 1 : 0;
                const float base = 55.f * std::exp2 (texTn * 3.f + 0.8f * rng.bi());
                static const float R[5] = { 1.f, 2.32f, 4.25f, 6.63f, 9.38f };
                for (int k = 0; k < 5; ++k)
                {
                    const float f = base * R[k] * (1.f + 0.03f * rng.bi());
                    const float q = 120.f + 280.f * rng.uni(), fk = juce::jmin (f, sr * 0.45f);      // plates ring for seconds
                    e.m[k].set (Biquad::BP, sr, fk, q);
                    // a band-pass with a unity peak barely rings from a short hit: scale each mode to ring at about the hit's size
                    e.mg[k] = (k == 0 ? 1.f : 0.75f / (float) k) * (0.5f + rng.uni()) * 0.02f * 2.f * q / juce::jmax (0.002f, std::sin (kTwoPi * fk / sr));
                }
                e.len = (int) ((scrape ? 0.6f + 2.2f * rng.uni() : 0.02f) * sr);
                e.f0 = scrape ? (0.15f + 0.35f * rng.uni()) : 0.f;      // attack part of a scrape
                e.amp = (scrape ? 0.5f : 0.9f) * (0.5f + 0.5f * rng.uni());
                e.a.setSampleRate (sr); e.a.set (600.f + 2400.f * rng.uni(), 0.7f);
                setPan (e, pan); e.send = 0.8f; e.x1 = 0; e.x2 = rng.uni(); break;
            }
            case ad::Shimmer:
            {
                // a grain of glass: a high, soft tone from the scale's pentatonic (it used to take the major one in every key)
                auto& e = *takeEvent(); e = {}; e.kind = ad::Shimmer;
                const float note = keyNote (72 + 12 * (int) (texTn * 2.f + 0.5f * rng.uni()), (int) (rng.uni() * 4.99f));
                e.inc = midiHz (note + (texC > 0.6f ? 0.5f * rng.bi() * (texC - 0.6f) * 2.f : 0.f)) / sr;
                e.len = (int) ((0.15f + 0.9f * rng.uni()) * sr); e.f0 = 0.08f + 0.35f * rng.uni();   // attack share
                e.amp = 0.25f + 0.45f * rng.uni(); setPan (e, pan); e.send = 1.1f; break;
            }
            case ad::Whispers:
            {
                // a whispered word: a hiss into changing vowels, with syllables
                auto& e = *takeEvent(); e = {}; e.kind = ad::Whispers;
                e.len = (int) ((0.3f + 0.8f * rng.uni()) * sr);
                e.f0 = 300.f + 500.f * rng.uni(); e.f1 = 900.f + 1500.f * rng.uni();     // first vowel
                e.x1 = 300.f + 500.f * rng.uni(); e.x2 = 900.f + 1500.f * rng.uni();     // last vowel
                e.inc2 = (3.f + 4.f * rng.uni()) / sr;                                    // syllable rate
                e.sub = (int) ((0.03f + 0.08f * rng.uni()) * sr);                         // the opening "s"
                e.a.setSampleRate (sr); e.b.setSampleRate (sr); e.c.setSampleRate (sr);
                e.c.set (6500.f, 1.2f);
                e.amp = 0.5f + 0.5f * rng.uni(); setPan (e, 0.5f + 0.5f * wide * rng.bi() * 1.1f); e.send = 0.9f;
                e.ph2 = rng.uni() < 0.25f ? 1.f : 0.f;                                    // sometimes it swells up instead (reversed)
                break;
            }
            case ad::Heartbeat:
            {
                // lub ... dub
                const float beatGap = 1.f / juce::jmax (0.1f, texRate);
                for (int k = 0; k < 2; ++k)
                {
                    auto& e = *takeEvent(); e = {}; e.kind = ad::Heartbeat; e.sub = k;
                    e.wait = k == 0 ? 0 : (int) (beatGap * (0.26f + 0.06f * rng.uni()) * sr * juce::jmin (1.f, 0.6f + 0.6f * beatGap));
                    e.f0 = k == 0 ? 58.f : 74.f; e.f1 = k == 0 ? 36.f : 46.f;
                    e.env = 1; e.dec = std::exp (-1.f / ((k == 0 ? 0.11f : 0.08f) * sr)); e.len = (int) (0.35f * sr);
                    e.amp = (k == 0 ? 1.f : 0.7f) * (1.f - 0.25f * texC * rng.uni());
                    e.a.setSampleRate (sr); e.a.set (90.f + texTn * 900.f, 0.7f);
                    setPan (e, 0.5f); e.send = 0.15f;
                }
                break;
            }
            case ad::Knocks:
            {
                // knock knock (knock): on a wooden door somewhere in the house
                const int count = 2 + (int) (rng.uni() * (1.5f + texC * 1.5f));
                const float gap = 0.13f + 0.1f * rng.uni();
                const float where = pan;
                int wait = 0;
                for (int k = 0; k < count; ++k)
                {
                    auto& e = *takeEvent(); e = {}; e.kind = ad::Knocks; e.wait = wait;
                    e.a.setSampleRate (sr); e.a.set (170.f * std::exp2 (texTn * 1.3f + 0.15f * rng.bi()), 5.f);
                    e.b.setSampleRate (sr); e.b.set (280.f + texTn * 2800.f, 0.7f);
                    e.env = 1; e.dec = std::exp (-1.f / (0.05f * sr)); e.len = (int) (0.25f * sr);
                    e.f0 = 95.f + 30.f * rng.uni(); e.amp = (0.6f + 0.4f * rng.uni()) * (k == count - 1 ? 0.8f : 1.f);
                    setPan (e, where); e.send = 1.2f;
                    wait += (int) (gap * (1.f + texC * 0.5f * rng.bi()) * sr);
                }
                break;
            }
            case ad::Moans:
            {
                // a moan of pain somewhere in the dark: a voice that swells, cracks and sags
                auto& e = *takeEvent(); e = {}; e.kind = ad::Moans;
                e.len = (int) ((1.3f + 2.6f * rng.uni()) * sr);
                e.f0 = 82.f * std::exp2 (texTn * 2.4f + 0.45f * rng.bi());          // low groans .. high wails
                e.f1 = 1.5f + 4.f * rng.uni();                                        // semitones up into the peak
                e.mg[0] = 0.3f + 0.7f * rng.uni() * (0.5f + texC);                   // how far it sags (octaves)
                e.mg[1] = 0.15f + 0.5f * rng.uni();                                   // breathiness
                e.mg[2] = 0.1f + 0.25f * rng.uni();                                   // attack share
                e.mg[3] = (float) (int) (rng.uni() * 2.99f); e.mg[4] = (float) (int) (rng.uni() * 2.99f);   // vowel from / to
                e.inc2 = (4.5f + 2.f * rng.uni()) / sr;                               // vibrato
                e.a.setSampleRate (sr); e.b.setSampleRate (sr); e.c.setSampleRate (sr);
                e.amp = 0.55f + 0.45f * rng.uni();
                setPan (e, 0.5f + 0.5f * wide * rng.bi()); e.send = 1.6f;
                break;
            }
            case ad::Flesh:
            {
                // the wet, the cracking and the tearing
                const float r0 = rng.uni();
                const int sub = r0 < 0.5f ? 0 : r0 < 0.5f + 0.3f * (0.4f + texC) ? 1 : 2;
                auto& e = *takeEvent(); e = {}; e.kind = ad::Flesh; e.sub = sub;
                e.a.setSampleRate (sr); e.b.setSampleRate (sr); e.c.setSampleRate (sr);
                setPan (e, 0.5f + 0.4f * wide * rng.bi());
                if (sub == 0)        // a squelch: a wet, sliding gurgle
                {
                    e.len = (int) ((0.12f + 0.5f * rng.uni()) * sr);
                    e.f0 = 220.f * std::exp2 (texTn * 1.5f + 0.5f * rng.bi());      // start of the slide
                    e.f1 = e.f0 * std::exp2 (rng.bi() * 1.4f);                       // end of it
                    e.x2 = 0.002f + 0.01f * rng.uni();                                // blob rate per sample
                    e.amp = 0.6f + 0.4f * rng.uni(); e.send = 0.35f;
                }
                else if (sub == 1)   // a crack: bone giving way
                {
                    e.len = (int) (0.09f * sr);
                    e.c.set (1600.f * std::exp2 (texTn * 1.3f + 0.4f * rng.bi()), 4.f);
                    e.b.set (95.f + 50.f * rng.uni(), 1.5f);
                    e.x2 = (float) (2 + (int) (rng.uni() * 3.f));                     // how many snaps
                    e.amp = 0.8f + 0.2f * rng.uni(); e.send = 0.5f;
                }
                else                 // a tear: something ripping, slow, then fast
                {
                    e.len = (int) ((0.35f + 0.7f * rng.uni()) * sr);
                    e.c.set (1300.f * std::exp2 (texTn * 1.6f + 0.3f * rng.bi()), 1.1f);
                    e.amp = 0.6f + 0.4f * rng.uni(); e.send = 0.4f;
                }
                break;
            }
            case ad::Chimes:
            {
                // a breeze through wind chimes: two to six tubes, a moment apart, from the scale's pentatonic (high)
                const int n = 2 + (int) (rng.uni() * (2.f + 3.f * texD)), base = 74 + (int) (texTn * 10.f);
                float when = 0.f;
                for (int k = 0; k < n; ++k)
                {
                    auto& e = *takeEvent(); e = {}; e.kind = ad::Chimes;
                    e.wait = (int) (when * sr); when += (0.06f + 0.4f * rng.uni()) * (0.4f + 0.8f * texC + 0.4f);
                    static const float R[4] = { 1.f, 2.756f, 5.404f, 8.933f }, A[4] = { 1.f, 0.55f, 0.3f, 0.15f }, T[4] = { 3.6f, 2.2f, 1.2f, 0.7f };
                    const float f = midiHz (keyNote (base, (int) (rng.uni() * 7.99f)));
                    partials (e, f, R, A, T, 4, 0.6f + 0.4f * rng.uni(), 0.0015f);
                    e.a.setSampleRate (sr); e.a.set (juce::jmin (sr * 0.4f, 6500.f), 1.5f); e.sub = (int) (0.002f * sr);   // the strike
                    setPan (e, 0.5f + 0.45f * wide * rng.bi()); e.send = 0.9f;
                }
                break;
            }
            case ad::Birds:
            {
                // a bird's phrase: two to six notes, each a quick sweep (a whistle, a chirp, sometimes a trill)
                auto& e = *takeEvent(); e = {}; e.kind = ad::Birds;
                const int n = 2 + (int) (rng.uni() * 4.99f);
                const float hi = 2300.f * std::exp2 (texTn * 1.3f + 0.5f * rng.bi());
                const float dur = 0.04f + 0.14f * rng.uni(), gap = 0.02f + 0.12f * rng.uni() * (0.5f + texC);
                const bool rising = rng.uni() < 0.5f;
                float t0 = 0.f;
                e.np = juce::jmin (6, n);
                for (int k = 0; k < e.np; ++k)
                {
                    const float d = dur * (0.7f + 0.6f * rng.uni());
                    const float fa = hi * std::exp2 (0.6f * rng.bi() * (0.3f + texC)), fb = fa * std::exp2 ((rising ? 1.f : -1.f) * (0.2f + 0.8f * rng.uni()));
                    e.pp[k] = t0; e.pi[k] = d; e.pa[k] = fa; e.pd[k] = fb;
                    t0 += d + gap * (0.6f + 0.8f * rng.uni());
                }
                e.len = (int) (t0 * sr) + 8; e.sub = rng.uni() < 0.3f ? 1 : 0;           // a trill
                e.f0 = 28.f + 30.f * rng.uni();
                e.amp = 0.35f + 0.45f * rng.uni(); setPan (e, 0.5f + 0.5f * wide * rng.bi()); e.send = 0.55f;
                break;
            }
            case ad::MusicBox:
            {
                // a music box: a little melody stepping through the scale's pentatonic, high and bright
                const int n = 3 + (int) (rng.uni() * 5.f);
                int deg = (int) (rng.uni() * 8.f);
                const float step = 0.16f + 0.14f * rng.uni();
                const int base = 79 + (int) (texTn * 8.f);
                for (int k = 0; k < n; ++k)
                {
                    auto& e = *takeEvent(); e = {}; e.kind = ad::MusicBox;
                    e.wait = (int) ((float) k * step * (1.f + 0.25f * texC * rng.bi()) * sr);
                    static const float R[3] = { 1.f, 2.f, 6.27f }, A[3] = { 1.f, 0.12f, 0.22f }, T[3] = { 2.2f, 1.2f, 0.3f };
                    partials (e, midiHz (keyNote (base, deg)), R, A, T, 3, 0.55f + 0.3f * rng.uni(), 0.0008f);
                    e.a.setSampleRate (sr); e.a.set (juce::jmin (sr * 0.4f, 5000.f), 2.f); e.sub = (int) (0.0015f * sr);
                    setPan (e, 0.5f + 0.25f * wide * rng.bi()); e.send = 0.5f;
                    deg = juce::jlimit (0, 11, deg + (rng.uni() < 0.5f ? -1 : 1) * (1 + (int) (rng.uni() * (1.f + 2.f * texC))));
                }
                break;
            }
            case ad::Bells:
            {
                // a church bell far away, in key (the root or the fifth): hum, prime, the minor third, the fifth and the nominal
                const int strikes = texC < 0.5f && rng.uni() < 0.35f ? 2 + (int) (rng.uni() * 1.99f) : 1;    // sometimes it tolls
                const float note = (float) (texTn < 0.4f ? 36 : texTn < 0.8f ? 48 : 60) + (float) (((prm->key % 12) + 12) % 12) + (rng.uni() < 0.3f ? 7.f : 0.f);
                for (int k = 0; k < strikes; ++k)
                {
                    auto& e = *takeEvent(); e = {}; e.kind = ad::Bells;
                    e.wait = (int) ((float) k * (1.9f + 0.5f * rng.uni()) * sr);
                    static const float R[6] = { 0.5f, 1.f, 1.189f, 1.5f, 2.f, 2.66f }, A[6] = { 0.45f, 0.6f, 0.42f, 0.2f, 0.9f, 0.28f }, T[6] = { 11.f, 7.f, 5.f, 4.f, 5.5f, 2.4f };
                    partials (e, midiHz (note), R, A, T, 6, 0.5f + 0.25f * rng.uni(), 0.002f);
                    e.a.setSampleRate (sr); e.a.set (juce::jmin (sr * 0.4f, midiHz (note) * 3.f), 3.f); e.sub = (int) (0.012f * sr);
                    setPan (e, 0.5f + 0.3f * wide * rng.bi()); e.send = 1.5f;
                }
                break;
            }
            case ad::Harp:
            {
                // a harp: a few notes rolled up (now and then down) the scale's pentatonic
                const int n = 2 + (int) (rng.uni() * 3.99f);
                const bool down = rng.uni() < 0.25f;
                int deg = (int) (rng.uni() * 5.f);
                const float step = 0.08f + 0.1f * rng.uni();
                const int base = 55 + (int) (texTn * 12.f);
                for (int k = 0; k < n; ++k)
                {
                    auto& e = *takeEvent(); e = {}; e.kind = ad::Harp;
                    e.wait = (int) ((float) k * step * (1.f + 0.3f * texC * rng.bi()) * sr);
                    static const float R[6] = { 1, 2, 3, 4, 5, 6 }, A[6] = { 1.f, 0.42f, 0.25f, 0.15f, 0.09f, 0.05f }, T[6] = { 3.4f, 2.2f, 1.6f, 1.2f, 0.9f, 0.7f };
                    partials (e, midiHz (keyNote (base, deg + (down ? n - 1 - k : k) * 2)), R, A, T, 6, 0.55f + 0.3f * rng.uni(), 0.003f);
                    setPan (e, 0.5f + 0.35f * wide * rng.bi()); e.send = 0.8f;
                }
                break;
            }
            case ad::Stars:
            {
                // a star: a soft, high tone that fades in and twinkles away, from the scale's pentatonic
                auto& e = *takeEvent(); e = {}; e.kind = ad::Stars;
                static const float R[2] = { 1.f, 2.f }, A[2] = { 1.f, 0.08f }, T[2] = { 4.f, 2.f };
                partials (e, midiHz (keyNote (86 + (int) (texTn * 8.f), (int) (rng.uni() * 8.99f))), R, A, T, 2, 0.3f + 0.35f * rng.uni(), 0.25f + 0.7f * rng.uni());
                for (int k = 0; k < 2; ++k) e.pd[k] = std::exp (-6.9f / ((1.8f + 3.f * rng.uni()) * sr) * (k == 0 ? 1.f : 1.6f));
                e.f0 = 3.f + 4.f * rng.uni();                                            // how fast it twinkles
                setPan (e, 0.5f + 0.5f * wide * rng.bi()); e.send = 1.6f;
                break;
            }
            case ad::Kalimba:
            {
                // a kalimba: thumb-plucked tines, a short figure in the scale's pentatonic, sometimes played twice
                const int n = 1 + (int) (rng.uni() * 3.99f);
                const int base = 60 + (int) (texTn * 12.f);
                int deg = (int) (rng.uni() * 6.f);
                const float step = 0.15f + 0.2f * rng.uni();
                const int reps = texC < 0.6f && rng.uni() < 0.3f ? 2 : 1;
                int degs[4];
                for (int k = 0; k < n; ++k) { degs[k] = deg; deg = juce::jlimit (0, 10, deg + (int) (rng.bi() * 3.f)); }
                for (int rp = 0; rp < reps; ++rp)
                    for (int k = 0; k < n; ++k)
                    {
                        auto& e = *takeEvent(); e = {}; e.kind = ad::Kalimba;
                        e.wait = (int) (((float) (rp * n + k)) * step * sr);
                        static const float R[3] = { 1.f, 5.92f, 2.f }, A[3] = { 1.f, 0.25f, 0.06f }, T[3] = { 1.5f, 0.12f, 0.6f };
                        partials (e, midiHz (keyNote (base, degs[k])), R, A, T, 3, 0.6f + 0.3f * rng.uni(), 0.0006f);
                        e.a.setSampleRate (sr); e.a.set (2500.f, 1.2f); e.sub = (int) (0.003f * sr);
                        setPan (e, 0.5f + 0.3f * wide * rng.bi()); e.send = 0.6f;
                    }
                break;
            }
            case ad::Leaves:
            {
                // rain on leaves: a short patter of tiny taps
                auto& e = *takeEvent(); e = {}; e.kind = ad::Leaves;
                e.len = (int) ((0.05f + 0.3f * rng.uni()) * sr);
                e.a.setSampleRate (sr); e.a.set (1500.f * std::exp2 (texTn * 1.6f + 0.4f * rng.bi()), 2.5f);
                e.f0 = (0.002f + 0.008f * texD) * (0.5f + rng.uni());                    // chance of a tap per sample
                e.x1 = std::exp (-1.f / (0.0007f * sr)); e.x2 = 0.f;
                e.amp = 0.4f + 0.6f * rng.uni(); setPan (e, pan); e.send = 0.35f;
                break;
            }
            default: break;
        }
    }

    inline float renderEvent (Ev& e)
    {
        float s = 0;
        const float t = (float) e.age / sr;
        switch (e.kind)
        {
            case ad::Vinyl:
            {
                const float click = e.ph2 > 0.f ? (e.ph2 = 0.f, 1.f) : 0.f;
                e.a.tick (click * 3.f + rng.bi() * 0.02f * e.env);
                s = (e.a.low * 4.f + click * 0.3f) * e.env * e.amp; e.env *= e.dec; break;
            }
            case ad::Fire:
            {
                const float w = juce::jmax (0.f, 1.f - (float) e.age / (float) juce::jmax (1, e.len));
                float x = 0; if (w > 0.f && rng.uni() < e.f0) x = rng.bi() * (0.4f + 0.6f * rng.uni());
                e.a.tick (x);
                s = e.a.band * 2.2f * e.amp * (0.3f + 0.7f * w); break;
            }
            case ad::Drips:
            {
                const float f = e.f0 * (1.f + 1.1f * (1.f - std::exp (-t / 0.006f)));
                e.ph = wrap01 (e.ph + f / sr);
                s = sin01 (e.ph) * e.env * e.amp * 0.8f; e.env *= e.dec; break;
            }
            case ad::Metal:
            {
                float x;
                if (e.sub == 1)
                {
                    // a scrape: bowed, stick-slip noise that swells, wavers and fades
                    const float u = juce::jmin (1.f, (float) e.age / (float) juce::jmax (1, e.len));
                    const float env = u < e.f0 ? u / e.f0 : std::pow ((1.f - u) / (1.f - e.f0), 1.5f);
                    e.x1 += (rng.bi() - e.x1) * 0.003f;                        // slow waver
                    const float slip = 0.55f + 0.45f * std::sin (kTwoPi * (t * (7.f + 9.f * e.x2)) + e.x1 * 6.f);
                    e.a.tick (rng.bi());
                    x = e.a.band * env * slip * 0.35f;
                }
                else x = e.age < (int) (0.004f * sr) ? rng.bi() * 0.9f : 0.f;    // a hit
                for (int k = 0; k < 5; ++k) s += e.m[k].process (x) * e.mg[k];
                s *= e.amp;
                e.env = juce::jmax (std::abs (s), e.env * 0.9995f);
                if (e.age > e.len + (int) (0.05f * sr) && e.env < 2.0e-5f) e.kind = -1;
                break;
            }
            case ad::Shimmer:
            {
                e.ph = wrap01 (e.ph + e.inc);
                const float u = (float) e.age / (float) juce::jmax (1, e.len);
                if (u >= 1.f) break;
                const float env = u < e.f0 ? std::sin (kPi * 0.5f * u / e.f0) : std::pow ((1.f - u) / (1.f - e.f0), 2.f);
                s = (sin01 (e.ph) + 0.15f * sin01 (wrap01 (e.ph * 2.01f))) * env * e.amp * 0.5f; break;
            }
            case ad::Whispers:
            {
                const float u = (float) e.age / (float) juce::jmax (1, e.len);
                if (u >= 1.f) break;
                const float uu = e.ph2 > 0.5f ? 1.f - u : u;
                const float f1 = (e.f0 + (e.x1 - e.f0) * u) * std::exp2 ((texTn - 0.5f) * 0.8f), f2 = (e.f1 + (e.x2 - e.f1) * u) * std::exp2 ((texTn - 0.5f) * 0.8f);
                if ((e.age & 31) == 0) { e.a.set (f1, 6.f); e.b.set (f2, 7.f); }
                e.ph = wrap01 (e.ph + e.inc2);
                const float syl = 0.35f + 0.65f * std::pow (0.5f + 0.5f * sin01 (e.ph), 1.5f);
                const float env = std::sin (kPi * juce::jmin (1.f, uu * 1.4f)) * (e.ph2 > 0.5f ? uu : 1.f);
                const float w = rng.bi();
                e.a.tick (w); e.b.tick (w); e.c.tick (w);
                const float sib = e.age < e.sub ? 1.f - (float) e.age / (float) e.sub : 0.f;
                s = ((e.a.band * 1.2f + e.b.band) * syl * env + e.c.band * sib * 0.8f) * e.amp * 1.3f; break;
            }
            case ad::Heartbeat:
            {
                const float f = e.f1 + (e.f0 - e.f1) * std::exp (-t / 0.03f);
                e.ph = wrap01 (e.ph + f / sr);
                const float click = e.age < (int) (0.006f * sr) ? rng.bi() * 0.25f : 0.f;
                e.a.tick (sin01 (e.ph) * e.env + click);
                s = (e.a.low * 1.6f) * e.amp; e.env *= e.dec; break;
            }
            case ad::Knocks:
            {
                const float x = e.age < (int) (0.003f * sr) ? rng.bi() : 0.f;
                e.a.tick (x * 2.f);
                e.ph = wrap01 (e.ph + e.f0 / sr);
                const float thud = sin01 (e.ph) * e.env * 0.5f;
                e.b.tick (e.a.band * 2.5f + thud);
                s = e.b.low * e.amp * 1.4f; e.env *= e.dec; break;
            }
            case ad::Moans:
            {
                const float u = (float) e.age / (float) juce::jmax (1, e.len);
                if (u >= 1.f) break;
                // pitch: up into the peak, then sagging away (a voice running out of breath)
                float semis;
                if (u < 0.22f) { const float x = u / 0.22f; semis = e.f1 * x * x * (3.f - 2.f * x); }
                else { const float x = (u - 0.22f) / 0.78f; semis = e.f1 - (e.f1 + e.mg[0] * 12.f) * std::pow (x, 1.6f); }
                e.ph2 = wrap01 (e.ph2 + e.inc2);
                e.x1 += (rng.bi() - e.x1) * 0.0025f;                                  // an unsteady, shaking voice
                const float vib = sin01 (e.ph2) * (0.12f + 0.45f * u) + e.x1 * (0.6f + 1.2f * texC);
                const float f = e.f0 * std::exp2 ((semis + vib) / 12.f);
                const float inc = f / sr;
                e.ph += inc; if (e.ph >= 1.f) e.ph -= 1.f;
                const float glot = saw (e.ph, inc) * (1.f + 0.25f * rng.bi() * texC);        // rough at the edges
                const float src = glot * (1.f - e.mg[1]) + rng.bi() * e.mg[1] * 0.8f;
                if ((e.age & 31) == 0)
                {
                    static const float V[3][3] = { { 450, 800, 2830 }, { 325, 700, 2530 }, { 730, 1090, 2440 } };    // o, u, a
                    const int a = juce::jlimit (0, 2, (int) e.mg[3]), b = juce::jlimit (0, 2, (int) e.mg[4]);
                    const float sh = std::exp2 ((texTn - 0.4f) * 0.5f), m = u * u;
                    e.a.set ((V[a][0] + (V[b][0] - V[a][0]) * m) * sh, 6.f);
                    e.b.set ((V[a][1] + (V[b][1] - V[a][1]) * m) * sh, 8.f);
                    e.c.set ((V[a][2] + (V[b][2] - V[a][2]) * m) * sh, 10.f);
                }
                e.a.tick (src); e.b.tick (src); e.c.tick (src);
                const float att = e.mg[2];
                float env = u < att ? std::sin (kPi * 0.5f * u / att) : u > 0.65f ? std::pow ((1.f - u) / 0.35f, 1.4f) : 1.f;
                env *= 0.85f + 0.15f * sin01 (wrap01 (e.ph2 * 0.7f + 0.3f));
                s = (e.a.band * 0.5f + e.b.band * 0.3f + e.c.band * 0.12f) * env * e.amp;
                break;
            }
            case ad::Flesh:
            {
                const float u = (float) e.age / (float) juce::jmax (1, e.len);
                if (u >= 1.f && e.sub != 1) break;
                if (e.sub == 0)
                {
                    // blobs of noise through a sliding resonance: wet, thick, gurgling
                    if (rng.uni() < e.x2) e.x1 = 0.4f + 0.6f * rng.uni();
                    e.x1 *= 0.9985f;
                    if ((e.age & 31) == 0) { e.a.set (e.f0 + (e.f1 - e.f0) * u, 7.f); e.b.set ((e.f0 + (e.f1 - e.f0) * u) * 2.7f, 5.f); }
                    const float x = rng.bi() * e.x1;
                    e.a.tick (x); e.b.tick (x);
                    const float env = std::sin (kPi * u);
                    s = (e.a.band * 0.45f + e.b.band * 0.15f) * env * e.amp;
                }
                else if (e.sub == 1)
                {
                    // a few snaps a few milliseconds apart, a body that rings and a low crunch
                    const int gap = (int) (0.007f * sr);
                    const int k = e.age / juce::jmax (1, gap);
                    const float x = (k < (int) e.x2 && e.age % gap < 24) ? rng.bi() * (1.f - 0.2f * (float) k) : 0.f;
                    e.c.tick (x); e.b.tick (x * 2.f);
                    const float crunch = e.age < (int) (0.05f * sr) ? rng.bi() * std::exp (-(float) e.age / (0.012f * sr)) * 0.35f : 0.f;
                    s = (e.c.band * 1.1f + e.b.low * 0.9f + crunch) * e.amp;
                }
                else
                {
                    // ripping: snaps that come faster and faster, then stop
                    const float rate = 0.004f + 0.06f * u * u;
                    float x = 0; if (rng.uni() < rate) x = rng.bi() * (0.5f + 0.5f * rng.uni());
                    e.c.tick (x + rng.bi() * 0.02f * u);
                    const float env = u < 0.85f ? 1.f : (1.f - u) / 0.15f;
                    s = e.c.band * 1.6f * env * e.amp;
                }
                break;
            }
            case ad::Chimes: case ad::MusicBox: case ad::Bells: case ad::Harp: case ad::Stars: case ad::Kalimba:
            {
                // struck or plucked things: sine partials, each dying away at its own pace, after a short (Stars: slow) attack
                const float at = e.x1 > 0.f ? clampf (t / e.x1, 0.f, 1.f) : 1.f;
                float sum = 0, live = 0;
                for (int k = 0; k < e.np; ++k)
                {
                    e.pp[k] = wrap01 (e.pp[k] + e.pi[k]);
                    sum += sin01 (e.pp[k]) * e.pa[k] * e.pe[k];
                    e.pe[k] *= e.pd[k]; live = juce::jmax (live, e.pe[k]);
                }
                s = sum * (at < 1.f ? std::sin (kPi * 0.5f * at) : 1.f) * e.amp;
                if (e.kind == ad::Stars) s *= 1.f - 0.18f * (0.5f + 0.5f * std::sin (kTwoPi * e.f0 * t));    // twinkling
                if (e.sub > 0) { --e.sub; e.a.tick (rng.bi()); s += e.a.band * 0.3f * e.amp; }             // the strike, the pluck
                if (live < 1.0e-4f && e.sub <= 0) e.kind = -1;
                break;
            }
            case ad::Birds:
            {
                float y = 0;
                for (int k = 0; k < e.np; ++k)
                {
                    const float u = (t - e.pp[k]) / e.pi[k];
                    if (u < 0.f || u >= 1.f) continue;
                    const float f = e.pa[k] * std::pow (e.pd[k] / e.pa[k], u);          // a sweep, as birds sing it
                    e.ph = wrap01 (e.ph + f / sr);
                    const float env = std::pow (std::sin (kPi * u), 1.5f);
                    const float trill = e.sub == 1 ? 0.55f + 0.45f * std::sin (kTwoPi * e.f0 * t) : 1.f;
                    y = (sin01 (e.ph) + 0.1f * sin01 (wrap01 (e.ph * 2.f))) * env * trill;
                    break;
                }
                s = y * e.amp * 0.45f;
                if (e.age >= e.len) e.kind = -1;
                break;
            }
            case ad::Leaves:
            {
                // each tap a tiny burst (0.7 ms), not a single-sample click
                if (e.age < e.len && rng.uni() < e.f0) e.x2 = 0.3f + 0.7f * rng.uni();
                const float x = rng.bi() * e.x2; e.x2 *= e.x1;
                e.a.tick (x);
                s = e.a.band * 1.6f * e.amp;
                if (e.age > e.len + (int) (0.02f * sr)) e.kind = -1;
                break;
            }
            default: break;
        }
        // struck and plucked things end when they have died away (above), the rest when their time is up
        const bool rings = e.kind == ad::Chimes || e.kind == ad::MusicBox || e.kind == ad::Bells || e.kind == ad::Harp || e.kind == ad::Stars || e.kind == ad::Kalimba;
        ++e.age;
        if (rings) { if (e.age > (int) (25.f * sr)) e.kind = -1; }
        else if (e.kind == ad::Metal) { if (e.age > e.len + (int) (4.f * sr)) e.kind = -1; }
        else if (e.age > e.len + (int) (0.2f * sr)) e.kind = -1;
        return s;
    }

    // ======================================================================== SPACE
    static constexpr float kBaseMs[kLines] = { 37.1f, 41.3f, 47.9f, 53.3f, 59.7f, 67.1f, 73.9f, 79.3f };
    static constexpr float kMaxScale = 2.6f;

    void controlSpace (const AtmosParams& p)
    {
        const float size = p.get (ad::size) * 0.01f, mix = p.get (ad::space) * 0.01f;
        const bool frz = p.on (ad::freeze);
        const float scale = 0.45f * std::pow (kMaxScale / 0.45f, size);
        const float t60 = 1.2f * std::pow (25.f, size);
        for (int j = 0; j < kLines; ++j)
        {
            lineLen[j] = kBaseMs[j] * 0.001f * scale * sr;
            const float g = std::pow (10.f, -3.f * lineLen[j] / (t60 * sr));
            lineG[j] += ((frz ? 1.f : g) - lineG[j]) * 0.05f;
            damp[j].setLowpass (sr, frz ? 18000.f : (9000.f - 4500.f * size) * std::exp2 (p.get (ad::tone) * 0.006f));
        }
        inSm += ((frz ? 0.f : 1.f) - inSm) * 0.08f;
        wetGain = mix * 1.1f; dryGain = 1.f - mix * 0.45f;
        modPh[0] = wrap01 (modPh[0] + 0.23f / ctlRate); modPh[1] = wrap01 (modPh[1] + 0.31f / ctlRate);
        modA = sin01 (modPh[0]) * 10.f; modB = sin01 (modPh[1]) * 10.f;
        diffLen = 0.0045f * sr * (0.6f + size);
        shimAmt = p.get (ad::shimmer) * 0.01f;
    }

    inline void spaceSample (float inL, float inR, float& outL_, float& outR_)
    {
        if (wetGain < 0.0005f && ! (inSm < 0.5f)) { outL_ = outR_ = 0; for (auto& l : line) l.push (0.f); shimIn = 0; return; }
        // SHIMMER: what the space gives back, an octave up (two read heads sweeping through 50 ms, crossfaded), fed into it
        // again: every pass rises another octave until the damping takes it. FREEZE closes the input, so it can't build up.
        float sh = 0;
        if (shimAmt > 0.001f)
        {
            shimLine.push (shimIn);
            shimPh = wrap01 (shimPh + 1.f / shimW);
            const float pa = shimPh, pb = wrap01 (shimPh + 0.5f);
            const float ya = shimLine.readLinear (2.f + shimW * (1.f - pa)), yb = shimLine.readLinear (2.f + shimW * (1.f - pb));
            sh = ya * sin01 (pa * 0.5f) + yb * sin01 (pb * 0.5f);                  // half-sine windows: constant power
            sh = shimLp.lp (sh); sh -= shimHp.lp (sh);
            sh = fastTanh (sh * 1.5f) * (0.62f * shimAmt);
        }
        float a = (inL + sh) * inSm, b = (inR + sh) * inSm;
        for (int k = 0; k < 3; ++k) { a = diffL[k].process (a, diffLen * (1.f + 0.31f * (float) k), 0.6f); b = diffR[k].process (b, diffLen * (1.13f + 0.29f * (float) k), 0.6f); }
        float x[kLines];
        for (int j = 0; j < kLines; ++j)
        {
            const float d = lineLen[j] + (j == 0 ? modA : j == 5 ? modB : 0.f);
            x[j] = damp[j].lp (line[j].readLinear (d));
        }
        float sum = 0; for (float v : x) sum += v;
        const float h = sum * (2.f / (float) kLines);
        for (int j = 0; j < kLines; ++j)
        {
            const float in = j < 4 ? a : b;
            line[j].push (in * 0.5f + (x[j] - h) * lineG[j]);
        }
        outL_ = (x[0] + x[2] + x[4] + x[6]) * 0.5f;
        outR_ = (x[1] + x[3] + x[5] + x[7]) * 0.5f;
        shimIn = 0.5f * (outL_ + outR_);
    }

    // ======================================================================== FOLLOW
    inline void followTick (float env)
    {
        const float e = clampf (env * 9.f, 0.f, 1.f);
        const float target = followAmt < 0.f ? 1.f + followAmt * e : (1.f - followAmt) + followAmt * e;
        followG += (target - followG) * (target < followG ? 0.004f : 0.0008f);
    }

    void panic()
    {
        lowShL.reset(); lowShR.reset(); hiShL.reset(); hiShR.reset(); dcL.reset(); dcR.reset();
        for (auto& d : damp) d.reset();
        for (auto& a : diffL) a.clear();
        for (auto& a : diffR) a.clear();
        crush = { 0, 0 };
        for (auto& l : line) l.clear();
        for (auto& e : ev) e.kind = -1;
        for (auto& d : drops) d.on = false;
        for (auto& b : bubbles) b.on = false;
        shimLine.clear(); shimIn = 0; scanLine.clear(); hornLine.clear();
        for (auto& t : tan) { t.line.clear(); t.z = 0; t.exN = 0; t.jw.reset(); }
        for (int k = 0; k < 3; ++k) { bodyL[k].reset(); bodyR[k].reset(); }
        for (int k = 0; k < 2; ++k) { reedL[k].reset(); reedR[k].reset(); }
        for (auto* s : { &airL, &airR, &bowL, &bowR }) s->reset();
        for (auto& f : chiffF) f.reset();
        pinkL = pinkR = {}; brL = brR = brT = brT2 = 0;
        for (auto* s : { &bpL, &bpR, &whL, &whR, &f1L, &f1R, &f2L, &f2R, &rumL, &rumR, &dLpL, &dLpR, &crkBp }) s->reset();
        for (auto& f : formL) f.reset();
        for (auto& f : formR) f.reset();
    }

    // Overall trim: a typical scene at LEVEL -6 dB sits around -30 dBFS, well under the guitar (presets aim at -26).
    static constexpr float kTrimDb = -9.f;
    // Per-type gains so every bed / voice / texture sits about as loud as the others at the same level.
    //                                               Hiss  Wind  Rain  Surf  Rumble Static Breath Brook Night Forest SoftRain Underwater Air
    static constexpr float kBedNorm[ad::kBedTypes] = { 1.0f, 0.9f, 1.0f, 0.9f, 0.7f,  1.6f,  0.65f, 1.55f, 2.5f, 0.54f, 2.0f,    0.86f,     1.7f };
    //                                               Organ Strings Choir Glass Abyss Descent Ascent PipeOrgan Pad  Angels Tanpura Bowls Harmonium Flute
    static constexpr float kDrnNorm[ad::kVoices]   = { 3.3f, 2.0f,   0.55f, 1.1f, 0.6f, 1.4f,  1.4f,  2.6f,     2.1f, 1.5f,  2.7f,   2.45f, 1.7f,    1.56f };
    //                                               Vinyl Fire Drips Metal Shimmer Whispers Heartbeat Knocks Moans Flesh Chimes Birds MusicBox Bells Harp Stars Kalimba Leaves
    static constexpr float kTexNorm[ad::kTextures] = { 4.0f, 0.9f, 1.4f, 3.f,  1.2f,   0.9f,    0.7f,     1.1f,  1.f,  3.f,  0.52f, 1.66f, 0.66f,   1.0f, 0.58f, 0.86f, 0.82f, 0.88f };

    // ======================================================================== state
    const AtmosParams* prm = nullptr;
    float sr = 48000.f, ctlRate = 1500.f; int ctlLeft = 0;
    Rng rng;

    // levels
    float bedTarget = 0, drnTarget = 0, texTarget = 0, masterTarget = 0, bedSm = 0, drnSm = 0, texSm = 0, masterSm = 0;
    float gritAmt = 0, widthAmt = 1, followAmt = 0, followG = 1, inSm = 1;
    float evB = 0, evD = 0, evT = 0, evP = 0; Drift evDrB, evDrD, evDrT, evDrP;
    Biquad lowShL, lowShR, hiShL, hiShR;
    EnvFollower follow; DcBlocker dcL, dcR;
    std::pair<float, float> crush { 0, 0 }; int crushN = 0; float gritOn = 0;
    std::atomic<float> act[3] {}; float actAcc[3] {};
    std::atomic<int> shownEvents { 0 };
    std::atomic<float> rootShown { -1.f };

    // bed
    int bedType = 1; float bedT = 0.5f, bedM = 0.3f, bedD = 0.5f, bedAmp = 1, bedQComp = 1, gust = 1, howl = 0;
    Pink pinkL, pinkR; float brL = 0, brR = 0, brT = 0;
    Svf bpL, bpR, whL, whR, f1L, f1R, f2L, f2R, rumL, rumR, crkBp;
    OnePole lpL, lpR, hpL, hpR;
    Drift bd1, bd2, bd3, bd4;
    struct Drop { bool on = false; Svf f; float env = 0, dec = 0.99f, amp = 1, pl = 0.7f, pr = 0.7f; int exN = 0; } drops[kDrops];
    float rainRate = 50;
    float wavePh[2] { 0.f, 0.45f }, waveInc[2] { 0.0001f, 0.00012f }, wave[2] { 0.2f, 0.2f };
    float subPh = 0, subInc = 0, subAmp = 0;
    float crkRate = 10; int crkBurst = 0; float whPh = 0, whPh2 = 0, whF = 1200, whAmp = 0;
    float breathPh = 0, breathInc = 0.0002f, breathEnv = 0, fryAmt = 0, fryPh = 0, fryInc = 0.001f;

    // bed (added 2026-11-26): bubbles (brook, underwater), crickets (night), the gurgle and the leaves' flutter
    struct Bubble { bool on = false; float ph = 0, f = 500, rise = 0, env = 0, dec = 0.99f, amp = 0, pl = 0.7f, pr = 0.7f; int age = 0; } bubbles[kBubbles];
    float bubbleRate = 50;
    struct Cricket { bool on = false; float ph = 0, inc = 0.09f, pph = 0, prate = 0.0006f, amp = 1, gapMean = 0.8f, pl = 0.7f, pr = 0.7f; int pulses = 0, gap = 0; } cricket[kCrickets];
    OnePole gurgL, gurgR;

    // drone
    int drnVoice = 1, numNotes = 2, numVoices = 6, interval[4] { 0, 7, 0, 0 }, followPc = -1, drnOct = 1;
    float drnMidi = -1, drnTone = 0.45f, drnMot = 0.3f, swell = 1, voiceNorm = 0.4f, fmIndex = 0.1f, abyssDrive = 2, growl = 1;
    int voiceWas = -1, chordWas = -1; float targetWas = -1; bool drnOnWas = false;
    float noteHz[kNotes] {};
    Drift swellDr, vowelDr, growlDr, padDr, bellowsDr;
    struct Voice { float ph = 0, ph2 = 0, ph3 = 0, inc = 0, inc2 = 0, amp = 1, vph = 0, vrate = 5.f, pl = 0.7f, pr = 0.7f; int u = 1; Drift drift; Svf bn; } voice[kNotes * kUni];
    struct Shep { float ph[kShep] {}, inc[kShep] {}, amp[kShep] {}, pan[kShep] {}; } shep[kNotes];
    float shepPos = 0.5f;
    Svf formL[4], formR[4], dLpL, dLpR;
    OnePole glotL, glotR; float choirBreath = 0.04f;                               // choir, angels
    float drawbar[9] {}, hornHz = 0, drumHz = 0, hornTh = 0, drumTh = 0.5f, scanPh = 0, scanDepth = 0;   // tonewheel organ
    DelayLine scanLine, hornLine; OnePole lesLp;
    float principal = 0, rank4 = 0, rank2 = 0, bourdon = 0, tremPh = 0, tremAm = 1, tremPitch = 0, chiffEnv[kNotes] {}, chiffAmt = 0, windAmt = 0;   // pipe organ
    Svf chiffF[kNotes]; OnePole windHp;
    float bowAmt = 0; Svf bowL, bowR; Biquad bodyL[3], bodyR[3];                  // strings
    float bellowsPh = 0, bellowsAm = 1, bellowsPitch = 0; Biquad reedL[2], reedR[2];   // harmonium
    float fluteH2 = 0.2f, fluteH3 = 0, fluteBreath = 0.4f;                         // flute
    struct TString { DelayLine line; float D = 100, g = 0.999f, a = 0.2f, z = 0, ex = 0, jEnv = 0, pl = 0.7f, pr = 0.7f; int exN = 0; OnePole exLp; Svf jw; } tan[4];
    float tanClock = 0, tanGap = 0, jawari = 0.5f; int tanNext = 0;                // tanpura
    struct Bowl { float ph[4][2] {}, inc[4][2] {}, amp[4] {}, rub = 0, strike = 0, strikeHi = 0, wait = 0, pl = 0.7f, pr = 0.7f; Drift rubDr; } bowl[kNotes];
    float bowlUpper = 0.5f;                                                        // singing bowls
    float shineAmt = 0, shinePh[kNotes][2] {}, shineInc[kNotes][2] {}, shineGl[kNotes] {}; Drift shineDr[kNotes]; OnePole shineLpL, shineLpR;
    float airAmt = 0, airGl = 1; Drift airDr; Svf airL, airR; Pink airPL, airPR;

    // texture
    int texType = 1, texTypeWas = -1; float texD = 0.4f, texTn = 0.5f, texC = 0.4f, texRate = 1; int nextEvent = 0; bool anyEvents = false;
    Ev ev[kEvents];
    OnePole texLp, roarLp, roarLp2, vinHp; Drift roarDr; float brT2 = 0; float roarAmp = 0, vinylRate = 0, vinylClick = 0, vinylT = 0;

    // space
    DelayLine line[kLines]; OnePole damp[kLines]; Allpass diffL[3], diffR[3];
    float lineLen[kLines] {}, lineG[kLines] {}, modPh[2] {}, modA = 0, modB = 0, diffLen = 200, wetGain = 0, dryGain = 1;
    DelayLine shimLine; OnePole shimHp, shimLp; float shimAmt = 0, shimPh = 0, shimW = 2400, shimIn = 0;
};
} // namespace obs::dsp
