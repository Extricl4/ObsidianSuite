#pragma once
// =============================================================================
//  AETHER: SIREN's second page. Noise, ambience and atmospheres that play on
//  their own (no notes needed), under and around the guitar.
//
//   BED      a bed of noise: hiss, wind (gusts and a howl), rain (drops on a
//            hiss), surf (waves rolling in), rumble (deep earth and a sub),
//            static (an old radio, crackle and tuning whistles), breath
//            (slow, rasping breathing)
//   DRONE    tones that hang in the air, in SIREN's key or following the
//            guitar: organ, strings, choir (sung vowels), glass (bell-like FM),
//            abyss (a growling sub), descent / ascent (a Shepard tone that
//            falls or rises forever). Chords from a fifth to a dread cluster.
//   TEXTURE  things that happen now and then: vinyl crackle, fire, drips in a
//            cave, metal creaks and scrapes, shimmering glass, whispers, a
//            heartbeat, knocks on a door, distant moans of pain, flesh (wet squelches,
//            bones cracking, tearing)
//   SPACE    an 8-line feedback delay network, from a room to an endless void,
//            with FREEZE (holds what's in it forever)
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
    float tuneHz = 440.f;          // SIREN's tuning reference
    float guitarHz = 0.f, guitarConf = 0.f;   // the pitch tracker (the drone can follow the guitar)
    float get (int i) const       { return v[i]; }
    int   choice (int i) const    { return (int) std::lround (v[i]); }
    bool  on (int i) const        { return v[i] >= 0.5f; }
};

class Atmos
{
public:
    static constexpr int kCtl = 32, kEvents = 16, kDrops = 12, kNotes = 4, kUni = 3, kShep = 9, kLines = 8;

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        sr = (float) sampleRate; ctlRate = sr / (float) kCtl;
        for (auto* s : { &bpL, &bpR, &whL, &whR, &f1L, &f1R, &f2L, &f2R, &rumL, &rumR, &dLpL, &dLpR, &crkBp })
            s->setSampleRate (sampleRate);
        for (auto& f : formL) f.setSampleRate (sampleRate);
        for (auto& f : formR) f.setSampleRate (sampleRate);
        for (auto& d : drops) d.f.setSampleRate (sampleRate);
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
        for (auto* o : { &lpL, &lpR, &hpL, &hpR, &texLp, &roarLp, &roarLp2, &vinHp }) o->reset();
        for (auto& d : drops) d.on = false;
        for (auto& e : ev) e.kind = -1;
        for (auto& v : voice) { v.ph = rng.uni(); v.ph2 = rng.uni(); }
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
                if (rng.uni() < rainRate / sr) spawnDrop();
                for (auto& d : drops)
                    if (d.on)
                    {
                        const float x = d.exN > 0 ? (--d.exN, rng.bi()) : 0.f;
                        d.f.tick (x); const float s = d.f.band * d.env * d.amp;
                        d.env *= d.dec; if (d.env < 1.0e-4f) d.on = false;
                        l += s * d.pl; r += s * d.pr;
                    }
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
            default: l = r = 0; break;
        }
    }

    void spawnDrop()
    {
        for (auto& d : drops)
            if (! d.on)
            {
                d.on = true; d.f.reset();
                const float size = clampf (bedD * rng.uni() + (1.f - bedD) * 0.5f, 0.f, 1.f);
                d.f.set (1300.f * std::exp2 ((bedT - 0.3f) * 2.f + (1.f - size) * 1.6f + 0.5f * rng.bi()), 9.f + 8.f * rng.uni());
                d.env = 1.f; d.dec = std::exp (-1.f / ((0.004f + 0.02f * size) * sr)); d.exN = 1 + (int) (0.0012f * sr);
                d.amp = (0.25f + 0.75f * rng.uni()) * (0.6f + size) * 0.6f;
                const float pan = rng.uni(); d.pl = std::sqrt (1.f - pan); d.pr = std::sqrt (pan);
                return;
            }
    }

    // ======================================================================== DRONE
    void controlDrone (const AtmosParams& p)
    {
        const int chord = juce::jlimit (0, ad::kChords - 1, p.choice (ad::drnChord));
        static const int I[ad::kChords][4] = { { 0 }, { 0, 7 }, { 0, 12 }, { 0, 3, 7 }, { 0, 5, 7 }, { 0, 6 }, { 0, 1, 2 }, { 0, 1, 6, 11 } };
        static const int N[ad::kChords] = { 1, 2, 2, 3, 3, 2, 3, 4 };
        numNotes = N[chord]; for (int k = 0; k < 4; ++k) interval[k] = I[chord][k];
        const int oct = juce::jlimit (0, 3, p.choice (ad::drnOct));
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

        const float det = p.get (ad::drnDetune) * 0.01f, tone = clampf (p.get (ad::drnTone) * 0.01f + evD * 0.2f, 0.f, 1.f), mot = p.get (ad::drnMotion) * 0.01f;
        drnTone = tone; drnMot = mot;
        vibPh = wrap01 (vibPh + (4.6f + mot * 1.2f) / ctlRate);
        const float vib = (drnVoice == ad::Strings || drnVoice == ad::Choir) ? sin01 (vibPh) * mot * 0.22f : 0.f;
        swellDr.setRate (0.05f + mot * 0.25f, ctlRate);
        swell = 1.f + 0.35f * mot * swellDr.tick (rng);
        const float wid = clampf (p.get (ad::width) * 0.01f, 0.f, 1.f);

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

        int vi = 0;
        for (int nI = 0; nI < numNotes; ++nI)
            for (int u = 0; u < kUni; ++u, ++vi)
            {
                auto& v = voice[vi];
                v.drift.setRate (0.07f + 0.1f * mot, ctlRate);
                const float spread = (float) (u - 1) * det * 0.22f + v.drift.tick (rng) * det * 0.07f;   // semitones
                const float m = drnMidi + (float) interval[nI] + spread + vib;
                const float f = midiHz (m);
                v.inc = f / sr;
                const float pos = numNotes * kUni > 1 ? ((float) vi / (float) (numNotes * kUni - 1)) * 2.f - 1.f : 0.f;
                const float pan = 0.5f + 0.5f * pos * wid * (u == 1 ? 0.4f : 1.f);
                v.pl = std::sqrt (1.f - pan); v.pr = std::sqrt (pan);
            }
        numVoices = vi;
        voiceNorm = 1.f / std::sqrt ((float) juce::jmax (1, numVoices));

        switch (drnVoice)
        {
            case ad::Organ:   droneFilter (700.f * std::exp2 (tone * 4.f), 0.6f); break;
            case ad::Strings: droneFilter (350.f * std::exp2 (tone * 4.6f + 0.25f * swellDr.cur * mot), 0.8f); break;
            case ad::Choir:
            {
                // sung vowels drifting between a, o, u and e
                static const float V[4][3] = { { 800, 1150, 2900 }, { 450, 800, 2830 }, { 325, 700, 2530 }, { 400, 1600, 2700 } };
                vowelDr.setRate (0.04f + mot * 0.3f, ctlRate);
                const float x = (vowelDr.tick (rng) * 0.5f + 0.5f) * 3.f;
                const int a = juce::jlimit (0, 2, (int) x); const float fr = x - (float) a;
                const float shift = std::exp2 ((tone - 0.5f) * 0.8f);
                for (int k = 0; k < 3; ++k)
                {
                    const float fk = (V[a][k] + (V[a + 1][k] - V[a][k]) * fr) * shift;
                    formL[k].set (fk, 7.f + 3.f * (float) k); formR[k].set (fk * 1.012f, 7.f + 3.f * (float) k);
                }
                droneFilter (5000.f + tone * 6000.f, 0.6f);
                break;
            }
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
            l = dLpL.low * g; r = dLpR.low * g; return;
        }
        for (int i = 0; i < numVoices; ++i)
        {
            auto& v = voice[i];
            v.ph += v.inc; if (v.ph >= 1.f) v.ph -= 1.f;
            float s = 0;
            switch (drnVoice)
            {
                case ad::Organ:
                    s = sin01 (v.ph) + 0.55f * sin01 (wrap01 (v.ph * 2.f)) + 0.3f * drnTone * sin01 (wrap01 (v.ph * 3.f)) + 0.2f * drnTone * drnTone * sin01 (wrap01 (v.ph * 4.f));
                    s *= 0.6f; break;
                case ad::Strings: case ad::Choir:
                    s = saw (v.ph, v.inc) * 0.55f; break;
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
        xl *= voiceNorm; xr *= voiceNorm;
        if (drnVoice == ad::Choir)
        {
            float yl = 0, yr = 0;
            for (int k = 0; k < 3; ++k) { formL[k].tick (xl); formR[k].tick (xr); const float w = k == 0 ? 1.f : k == 1 ? 0.7f : 0.4f; yl += formL[k].band * w; yr += formR[k].band * w; }
            xl = yl * 2.2f; xr = yr * 2.2f;
        }
        else if (drnVoice == ad::Abyss) { xl = fastTanh (xl * abyssDrive) * growl; xr = fastTanh (xr * abyssDrive) * growl; }
        dLpL.tick (xl); dLpR.tick (xr);
        l = dLpL.low * swell; r = dLpR.low * swell;
    }

    // ======================================================================== TEXTURE
    void controlTexture (const AtmosParams& p)
    {
        texD = clampf (p.get (ad::texDensity) * 0.01f + evT * 0.25f, 0.f, 1.f);
        texTn = p.get (ad::texTone) * 0.01f; texC = p.get (ad::texChaos) * 0.01f;
        // how often things happen (per second) at this density
        static const float lo[ad::kTextures] = { 0.15f, 0.8f, 0.25f, 0.06f, 0.6f, 0.15f, 0.f, 0.06f, 0.05f, 0.12f };
        static const float hi[ad::kTextures] = { 2.0f, 14.f, 7.f, 1.4f, 22.f, 2.8f, 0.f, 1.2f, 0.9f, 3.5f };
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
    };

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
                // a grain of glass: a high, soft tone from the key's pentatonic
                static const int pent[5] = { 0, 2, 4, 7, 9 };
                auto& e = *takeEvent(); e = {}; e.kind = ad::Shimmer;
                const int note = 72 + 12 * (int) (texTn * 2.f + 0.5f * rng.uni()) + ((prm->key % 12) + 12) % 12 + pent[(int) (rng.uni() * 4.99f)];
                e.inc = midiHz ((float) note + (texC > 0.6f ? 0.5f * rng.bi() * (texC - 0.6f) * 2.f : 0.f)) / sr;
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
            default: break;
        }
        if (++e.age > e.len + (int) (0.2f * sr) && e.kind != ad::Metal) e.kind = -1;
        else if (e.kind == ad::Metal && e.age > e.len + (int) (4.f * sr)) e.kind = -1;
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
    }

    inline void spaceSample (float inL, float inR, float& outL_, float& outR_)
    {
        if (wetGain < 0.0005f && ! (inSm < 0.5f)) { outL_ = outR_ = 0; for (auto& l : line) l.push (0.f); return; }
        float a = inL * inSm, b = inR * inSm;
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
        pinkL = pinkR = {}; brL = brR = brT = brT2 = 0;
        for (auto* s : { &bpL, &bpR, &whL, &whR, &f1L, &f1R, &f2L, &f2R, &rumL, &rumR, &dLpL, &dLpR, &crkBp }) s->reset();
        for (auto& f : formL) f.reset();
        for (auto& f : formR) f.reset();
    }

    // Overall trim: a typical scene at LEVEL -6 dB sits around -30 dBFS, well under the guitar (presets aim at -26).
    static constexpr float kTrimDb = -9.f;
    // Per-type gains so every bed / voice / texture sits about as loud as the others at the same level.
    //                                               Hiss  Wind  Rain  Surf  Rumble Static Breath
    static constexpr float kBedNorm[ad::kBedTypes] = { 1.0f, 0.9f, 1.0f, 0.9f, 0.7f,  1.6f,  0.65f };
    //                                               Organ Strings Choir Glass Abyss Descent Ascent
    static constexpr float kDrnNorm[ad::kVoices]   = { 1.1f, 2.0f,   0.55f, 1.1f, 0.6f, 1.4f,  1.4f };
    //                                               Vinyl Fire Drips Metal Shimmer Whispers Heartbeat Knocks Moans Flesh
    static constexpr float kTexNorm[ad::kTextures] = { 4.0f, 0.9f, 1.4f, 3.f,  1.2f,   0.9f,    0.7f,     1.1f,  1.f,  3.f };

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

    // drone
    int drnVoice = 1, numNotes = 2, numVoices = 6, interval[4] { 0, 7, 0, 0 }, followPc = -1;
    float drnMidi = -1, drnTone = 0.45f, drnMot = 0.3f, vibPh = 0, swell = 1, voiceNorm = 0.4f, fmIndex = 0.1f, abyssDrive = 2, growl = 1;
    Drift swellDr, vowelDr, growlDr;
    struct Voice { float ph = 0, ph2 = 0, inc = 0, pl = 0.7f, pr = 0.7f; Drift drift; } voice[kNotes * kUni];
    struct Shep { float ph[kShep] {}, inc[kShep] {}, amp[kShep] {}, pan[kShep] {}; } shep[kNotes];
    float shepPos = 0.5f;
    Svf formL[3], formR[3], dLpL, dLpR;

    // texture
    int texType = 1, texTypeWas = -1; float texD = 0.4f, texTn = 0.5f, texC = 0.4f, texRate = 1; int nextEvent = 0; bool anyEvents = false;
    Ev ev[kEvents];
    OnePole texLp, roarLp, roarLp2, vinHp; Drift roarDr; float brT2 = 0; float roarAmp = 0, vinylRate = 0, vinylClick = 0, vinylT = 0;

    // space
    DelayLine line[kLines]; OnePole damp[kLines]; Allpass diffL[3], diffR[3];
    float lineLen[kLines] {}, lineG[kLines] {}, modPh[2] {}, modA = 0, modB = 0, diffLen = 200, wetGain = 0, dryGain = 1;
};
} // namespace obs::dsp
