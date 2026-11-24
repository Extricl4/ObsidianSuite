#pragma once
// =============================================================================
//  Obsidian Synth: a paraphonic synth played by the guitar (pitch tracking)
//  or by MIDI notes (keyboard, DAW, or the UI's AUDITION button).
//
//   PLAY      threshold, sensitivity, glide, dynamics, legato, HOLD (drone),
//             CHORD (one note -> octaves / power / triad / sus / 7th / 9th in key)
//   SOUND     2 oscillators (saw, pulse, triangle, sine, super) with shape,
//             1-7 voice unison, detune, stereo width; B can hard-sync to A;
//             sub oscillator; coloured noise
//   FILTER    ladder 24 / 12, lowpass, bandpass, highpass, notch, vowel
//             (formants A-E-I-O-U), drive, envelope, key tracking
//   ENVELOPES amp + filter ADSR
//   LFO       6 shapes, free or synced, fade in, 5 targets
//   ARP       16 steps with accent, tie, octave, ratchet, chance, note;
//             up / down / up-down / random / steps; tempo synced, swing
//   OUTPUT    character (clean / analog / 8-bit / VHS), space (stereo echo)
//
//  Everything runs at audio rate except modulation, which is updated every
//  16 samples. No allocation on the audio thread.
// =============================================================================
#include "DspCore.h"
#include "PitchTracker.h"
#include "SynthDefs.h"
#include "AmpModels.h"     // Over2: the halfband used to run the oscillators at 2x

namespace obs::dsp
{
namespace sd = obs::synthdefs;

struct SynthParams
{
    float v[sd::count] {};                  // real values, by SynthDefs index
    unsigned steps[16] {};                  // packed arpeggiator steps
    double bpm = 120; bool hostPlaying = false; double ppq = 0;
    float transposeSemis = 0;               // the plugin's transpose (the guitar notes follow it)
    float bend = 0;                         // pitch wheel, semitones (the KEYS' wheel, or MIDI)
    float mod = 0;                          // mod wheel 0..1: vibrato
    float get (int i) const                 { return v[i]; }
    int   choice (int i) const              { return (int) std::lround (v[i]); }
    bool  on (int i) const                  { return v[i] >= 0.5f; }
};

/** A note from MIDI or the UI: vel > 0 = note on, 0 = note off. offset = sample in the block. */
struct SynthNote { int offset = 0; int note = 60; float vel = 0.f; };

class GuitarSynth
{
public:
    static constexpr int kCtl = 16, kMaxNotes = 4, kMaxUni = 7;

    void prepare (double s)
    {
        sr = s; osSr = 2.0 * s;
        envFast.set (s, 1.f, 25.f); envSlow.set (s, 20.f, 160.f);
        dcL.prepare (s); dcR.prepare (s);
        const int maxDelay = (int) (s * 2.1);
        echoL.prepare (maxDelay); echoR.prepare (maxDelay);
        for (auto& a : diffL) a.prepare ((int) (s * 0.05)); for (auto& a : diffR) a.prepare ((int) (s * 0.05));
        reset();
    }

    void reset()
    {
        envFast.reset(); envSlow.reset();
        gateOpen = false; sounding = false; held = false; pendingStart = false;
        kbCount = 0; onsetHold = 0; detectedMidi = -1; lastFollow = -1; rootNote = -1;
        ampEnv.reset(); fltEnv.reset();
        dynGain = 0; dynTarget = 0; vel = 0.8f;
        for (auto& n : notes) n = {};
        for (auto& o : osc) for (auto& slot : o) for (auto& v : slot) { v.phase = rng.nextFloat(); v.y1 = v.y2 = 0; }
        for (auto& s : sub) s = 0;
        for (auto& d : drift) d = 0;
        for (auto& f : flt) f.reset();
        lfoPhase = 0; lfoHeld = 0; lfoSmooth = 0; lfoTarget = 0; lfoFade = 1;
        arpActive = false; arpPhase = 0; arpStep = -1; arpIdx = 0; arpDir = 1; arpGate = false; arpGateOff = 0; ratchetLeft = 0; arpNote = 0;
        crushHold = 0; crushL = crushR = 0; crushCount = 0;
        vhsLp1 = vhsLp2 = vhsLp3 = vhsLp4 = 0; wow = 0; flutter = 0; noiseLp = 0; bendNow = 0; modNow = 0; vibPh = 0;
        echoL.clear(); echoR.clear(); for (auto& a : diffL) a.clear(); for (auto& a : diffR) a.clear();
        echoLpL = echoLpR = 0;
        dcL.reset(); dcR.reset(); overL.reset(); overR.reset(); wasActive = false;
        levelSm = 1; liveNote.store (-1.f); liveStep.store (-1); activity.store (0.f);
        ctlLeft = 0;
    }

    /** in: mono guitar (read only). outL/outR: synth signal (overwritten). */
    void process (const float* in, float* outL, float* outR, int n, const SynthParams& p, const PitchTracker& pt,
                  const SynthNote* events = nullptr, int numEvents = 0)
    {
        prm = &p;
        // ---- block-rate settings ----------------------------------------------------------
        const float openLin = dbToGain (p.get (sd::gate)), closeLin = dbToGain (p.get (sd::gate) - 4.f);
        const float sensGain = dbToGain (p.get (sd::sens) * 0.36f);
        const float tuneHz = p.get (sd::tune);
        glideCoef = p.get (sd::glide) < 1.f ? 0.f : std::exp (-(float) kCtl / (p.get (sd::glide) * 0.001f * (float) sr * 0.33f));
        ampEnv.set (sr, p.get (sd::aAtt), p.get (sd::aDec), p.get (sd::aSus) * 0.01f, p.get (sd::aRel));
        fltEnv.set (sr, p.get (sd::fAtt), p.get (sd::fDec), p.get (sd::fSus) * 0.01f, p.get (sd::fRel));
        const float outGain = dbToGain (p.get (sd::level));
        character = p.choice (sd::character);
        const int lfoTarget_ = p.choice (sd::lTarget);
        const float lfoDepth = p.get (sd::lDepth) * 0.01f;

        // Detected guitar note (continuous, so bends come through), relative to the tuning reference.
        const float f0 = pt.frequency(), conf = pt.confidence();
        if (f0 > 0 && conf > 0.6f)
            detectedMidi = 69.f + 12.f * std::log2 (f0 / tuneHz) + p.transposeSemis;

        // Space (echo) settings.
        const double beat = 60.0 / juce::jlimit (30.0, 300.0, p.bpm);
        echoSamples = (float) juce::jlimit (64.0, sr * 2.0, beat * 0.75 * sr);
        const float space = p.get (sd::space) * 0.01f;
        echoFb = 0.28f + space * 0.3f; echoWet = space * 0.75f;
        echoDamp.setLowpass (sr, 4200.f);

        int ev = 0;
        for (int i = 0; i < n; ++i)
        {
            // ---- note events (MIDI / UI) at their sample -------------------------------------
            while (ev < numEvents && events[ev].offset <= i) { handleNote (events[ev]); ++ev; }

            // ---- guitar gate + onsets ------------------------------------------------------
            const float x = in != nullptr ? in[i] : 0.f;
            const float ef = envFast.process (x), es = envSlow.process (x);
            const bool wasOpen = gateOpen;
            if (! gateOpen && ef > openLin) gateOpen = true;
            else if (gateOpen && ef < closeLin) gateOpen = false;
            if (kbCount == 0)
            {
                // a new pick: wait for a fresh, confident pitch before the note starts (no wrong-note blips)
                if (gateOpen && ! wasOpen) { pendingStart = true; if (sounding || held) detectedMidi = -1; }
                if (! gateOpen) pendingStart = false;
                if (pendingStart && detectedMidi > 0) { pendingStart = false; startNote (detectedMidi, 0.8f, Src::Guitar, false); }
                if (onsetHold > 0) --onsetHold;
                else if (gateOpen && ef > es * 1.9f && ef > openLin * 1.5f)
                {
                    onsetHold = (int) (0.06 * sr);
                    if (sounding && ! held && src == Src::Guitar && ! p.on (sd::legato) && ! arpActive) retrigger (0.8f);
                }
                if (gateOpen && detectedMidi > 0 && sounding && src == Src::Guitar)
                {
                    // follow the guitar; a clear new note (not a bend) restarts the envelopes
                    if (std::abs (detectedMidi - lastFollow) > 0.8f && ! p.on (sd::legato) && ! arpActive) retrigger (0.8f);
                    lastFollow = detectedMidi; rootNote = detectedMidi;
                }
                if (! gateOpen && wasOpen && src == Src::Guitar && sounding && ! held) { stopNote(); detectedMidi = -1; }
                dynTarget = src == Src::Guitar && gateOpen ? juce::jmin (1.f, std::sqrt (ef * sensGain)) : dynTarget;
            }

            // ---- modulation, every 16 samples -------------------------------------------
            if (--ctlLeft <= 0) { ctlLeft = kCtl; controlTick (p, lfoTarget_, lfoDepth, tuneHz); }

            // ---- envelopes + dynamics -------------------------------------------------------
            const float a = ampEnv.next();
            dynGain += (dynTarget - dynGain) * 0.002f;
            const float dyn = p.get (sd::dyn) * 0.01f;
            const float dynMul = src == Src::Guitar && ! held ? (1.f - dyn) + dyn * dynGain : (1.f - dyn) + dyn * vel;
            float g = a * dynMul * tremolo * outGain;

            float l = 0, r = 0;
            if (a > 1.0e-6f || ampEnv.active())
            {
                if (! wasActive) { overL.reset(); overR.reset(); wasActive = true; }
                // oscillators, drive and filter run at twice the sample rate: far less aliasing
                float l2[2], r2[2];
                const bool fOn = p.on (sd::fOn);
                for (int k = 0; k < 2; ++k)
                {
                    float xl = 0, xr = 0;
                    renderVoices (xl, xr);
                    if (character == 1) { xl = fastTanh (xl * 1.35f) * 0.78f; xr = fastTanh (xr * 1.35f) * 0.78f; }
                    if (fOn) { xl = flt[0].process (xl); xr = flt[1].process (xr); }
                    l2[k] = xl; r2[k] = xr;
                }
                l = overL.down (l2[0], l2[1]); r = overR.down (r2[0], r2[1]);
                l *= g; r *= g;
                // auto-pan
                l *= panL; r *= panR;
            }
            else wasActive = false;
            // character after the filter
            if (character == 2) crush (l, r);
            else if (character == 3) vhs (l, r, a);

            // space: ping-pong echo, smeared
            if (echoWet > 0.0005f)
            {
                const float dl = echoL.readLinear (echoSamples), dr = echoR.readLinear (echoSamples);
                echoLpL += (dl - echoLpL) * (1.f - echoDamp.a); echoLpR += (dr - echoLpR) * (1.f - echoDamp.a);
                echoL.push (0.5f * (l + r) * 0.8f + echoLpR * echoFb);
                echoR.push (echoLpL * echoFb);
                float wl = dl, wr = dr;
                wl = diffL[0].process (wl, (float) sr * 0.0131f, 0.55f); wl = diffL[1].process (wl, (float) sr * 0.0217f, 0.5f);
                wr = diffR[0].process (wr, (float) sr * 0.0157f, 0.55f); wr = diffR[1].process (wr, (float) sr * 0.0239f, 0.5f);
                l += wl * echoWet; r += wr * echoWet;
            }
            else { echoL.push (0.f); echoR.push (0.f); }

            l = dcL.process (l); r = dcR.process (r);
            if (isBad (l) || isBad (r)) { l = r = 0.f; panicReset(); }
            outL[i] = softLimit (l); outR[i] = softLimit (r);
        }
        while (ev < numEvents) handleNote (events[ev++]);

        activity.store (ampEnv.value() * (src == Src::Guitar && ! held ? juce::jmax (0.2f, dynGain) : vel));
        liveNote.store (ampEnv.active() && rootNote > 0 ? (arpActive ? (float) arpNote : rootNote) : -1.f);
        liveStep.store (arpActive ? arpStep : -1);
        liveSrc.store ((int) (held ? Src::Held : src));
    }

    float currentActivity() const   { return activity.load (std::memory_order_relaxed); }
    float currentNote() const       { return liveNote.load (std::memory_order_relaxed); }   // MIDI note, -1 = silent
    int   currentStep() const       { return liveStep.load (std::memory_order_relaxed); }   // arp step, -1 = arp idle
    int   currentSource() const     { return liveSrc.load (std::memory_order_relaxed); }    // 0 guitar, 1 midi, 2 hold

    // ---- helpers shared with the UI (and tested) ------------------------------------------------
    static int scaleSize (int scale)  { return scale >= 6 ? 6 : scale == 5 ? 5 : 7; }
    static int scaleNote (int scale, int i)
    {
        static const int S[7][7] = { { 0,2,4,5,7,9,11 }, { 0,2,3,5,7,8,10 }, { 0,2,3,5,7,9,10 }, { 0,1,3,5,7,8,10 }, { 0,2,3,5,7,8,11 }, { 0,3,5,7,10,0,0 }, { 0,3,5,6,7,10,0 } };
        return S[juce::jlimit (0, 6, scale)][i];
    }
    /** The scale note 'degree' steps above 'root' (root snapped into key; an off-scale root keeps its offset). */
    static float scaleTone (float root, int degree, int key, int scale)
    {
        const int sz = scaleSize (scale);
        const int r = (int) std::lround (root);
        const int rel = ((r - key) % 12 + 12) % 12;
        int d0 = 0; for (int i = 0; i < sz; ++i) if (scaleNote (scale, i) <= rel) d0 = i;
        const int off = rel - scaleNote (scale, d0);
        const int i = d0 + degree;
        const int o = (int) std::floor ((double) i / sz);
        const int idx = ((i % sz) + sz) % sz;
        return root + (float) (- rel + scaleNote (scale, idx) + 12 * o + off);
    }
    /** Notes of the CHORD setting for a root (returns how many). */
    static int chordNotes (float root, int chord, int key, int scale, float* out)
    {
        const int sc = scaleSize (scale) < 7 ? 1 : scale;          // pentatonic / blues chords use natural minor
        auto t = [&] (int d) { return scaleTone (root, d, key, sc); };
        switch (chord)
        {
            case 1: out[0] = root; out[1] = root + 12; return 2;
            case 2: out[0] = root; out[1] = root + 7; out[2] = root + 12; return 3;
            case 3: out[0] = t (0); out[1] = t (2); out[2] = t (4); return 3;
            case 4: out[0] = root; out[1] = root + 5; out[2] = root + 7; return 3;
            case 5: out[0] = t (0); out[1] = t (2); out[2] = t (4); out[3] = t (6); return 4;
            case 6: out[0] = t (0); out[1] = t (2); out[2] = t (4); out[3] = t (8); return 4;
            default: out[0] = root; return 1;
        }
    }

private:
    enum class Src { Guitar = 0, Midi = 1, Held = 2 };

    // ------------------------------------------------------------------ building blocks
    struct Adsr
    {
        void set (double sr, float aMs, float dMs, float s, float rMs)
        {
            atk = 1.f / juce::jmax (1.f, aMs * 0.001f * (float) sr);
            dec = std::exp (-4.6f / juce::jmax (1.f, dMs * 0.001f * (float) sr));
            rel = std::exp (-4.6f / juce::jmax (1.f, rMs * 0.001f * (float) sr));
            sus = clampf (s, 0.f, 1.f);
        }
        void gate (bool on, float peakLevel = 1.f) { if (on) { stage = 1; peak = peakLevel; } else if (stage != 0) stage = 4; }
        void reset() { stage = 0; v = 0; }
        bool active() const { return stage != 0; }
        float value() const { return v; }
        inline float next()
        {
            switch (stage)
            {
                case 1: v += atk * peak; if (v >= peak) { v = peak; stage = 2; } break;
                case 2: v = sus * peak + (v - sus * peak) * dec; break;
                case 4: v *= rel; if (v < 1.0e-5f) { v = 0; stage = 0; } break;
                default: break;
            }
            return v;
        }
        /** Advance n samples at once (for the control-rate filter envelope). */
        inline float advance (int n) { for (int i = 0; i < n; ++i) next(); return v; }
        int stage = 0; float v = 0, atk = 0.01f, dec = 0.999f, rel = 0.999f, sus = 1, peak = 1;
    };

    // Topology-preserving state-variable filter with all outputs.
    struct Svf2
    {
        void set (double sr, float fc, float q)
        {
            g = std::tan (kPi * clampf (fc, 16.f, (float) sr * 0.45f) / (float) sr); k = 1.f / juce::jmax (0.3f, q);
            a1 = 1.f / (1.f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        void reset() { ic1 = ic2 = 0; }
        inline void tick (float x)
        {
            const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
            lp = v2; bp = v1; hp = x - k * v1 - v2;
        }
        float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0, lp = 0, bp = 0, hp = 0;
    };

    // One channel of the filter section: ladder, SVF modes and the vowel formants.
    struct Filter
    {
        void reset() { for (auto& s : st) s = 0; svf.reset(); for (auto& f : form) f.reset(); }
        void setLadder (double sr, float fc, float res01, bool four, float drive)
        {
            const float gg = std::tan (kPi * clampf (fc, 16.f, (float) sr * 0.45f) / (float) sr);
            G = gg / (1.f + gg); kk = clampf (res01, 0.f, 1.f) * (four ? 3.9f : 3.4f); fourPole = four; drv = drive; mode = 0;
        }
        void setSvf (double sr, float fc, float res01, int m, float drive) { svf.set (sr, fc, 0.707f + res01 * 11.f); mode = m; drv = drive; }
        void setVowel (double sr, const float* f, float res01, float drive)
        {
            for (int i = 0; i < 3; ++i) form[i].set (sr, f[i], 5.f + res01 * 14.f);
            mode = 6; drv = drive;
        }
        inline float process (float x)
        {
            const float dn = 1.f / fastTanh (drv);
            switch (mode)
            {
                case 0:
                {
                    const float b0 = (1 - G) * st[0], b1 = (1 - G) * st[1], b2 = (1 - G) * st[2], b3 = (1 - G) * st[3];
                    const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
                    const float S = G3 * b0 + G2 * b1 + G * b2 + b3;
                    const float est = (G4 * x + S) / (1.f + kk * G4);
                    float in = fastTanh ((x - kk * est) * drv) / drv;
                    float y2 = 0;
                    for (int i = 0; i < 4; ++i)
                    {
                        const float v = (in - st[i]) * G, y = v + st[i];
                        st[i] = y + v; in = y;
                        if (i == 1) y2 = y;
                    }
                    return (fourPole ? in : y2) * (1.f + kk * 0.42f);
                }
                case 6:
                {
                    const float u = fastTanh (x * drv) * dn;
                    form[0].tick (u); form[1].tick (u); form[2].tick (u);
                    return (form[0].bp * form[0].k * 1.0f + form[1].bp * form[1].k * 0.7f + form[2].bp * form[2].k * 0.35f) * 1.6f;
                }
                default:
                {
                    svf.tick (fastTanh (x * drv) * dn);
                    switch (mode)
                    {
                        case 2: return svf.lp;
                        case 3: return svf.bp * svf.k;
                        case 4: return svf.hp;
                        default: return svf.lp + svf.hp;   // notch
                    }
                }
            }
        }
        float st[4] {}, G = 0.5f, kk = 0, drv = 1; bool fourPole = true; int mode = 0;
        Svf2 svf; Svf2 form[3];
    };

    struct OscV { float phase = 0, inc = 0, gain = 0, panL = 0.7f, panR = 0.7f, y1 = 0, y2 = 0; bool wrapped = false; float wrapFrac = 0; };
    struct NoteSlot { float cur = 0, target = 0; bool on = false; };

    static inline float blep (float t, float dt)
    {
        if (t < dt) { const float x = t / dt; return x + x - x * x - 1.f; }
        if (t > 1.f - dt) { const float x = (t - 1.f) / dt; return x * x + x + x + 1.f; }
        return 0.f;
    }
    static inline float wrap01 (float x) { return x - std::floor (x); }

    // ------------------------------------------------------------------ notes
    void handleNote (const SynthNote& e)
    {
        if (e.note < 0) { kbCount = 0; if (src == Src::Midi) stopNote(); return; }   // all notes off (the DAW stopped)
        if (e.vel > 0)
        {
            for (int i = 0; i < kbCount; ++i) if (kb[i].note == e.note) { removeKb (i); break; }
            if (kbCount < 8) kb[kbCount++] = { e.note, e.vel };
            startNote ((float) e.note, e.vel, Src::Midi, kbCount > 1);
        }
        else
        {
            for (int i = 0; i < kbCount; ++i) if (kb[i].note == e.note) { removeKb (i); break; }
            if (kbCount > 0) startNote ((float) kb[kbCount - 1].note, kb[kbCount - 1].vel, Src::Midi, true);   // back to the held key
            else if (src == Src::Midi) stopNote();
        }
    }
public:
    /** While the synth is switched off: keys let go meanwhile are let go here too
        (else they'd hang when it's switched back on). Note-ons are ignored. */
    void keysWhileOff (const SynthNote* events, int numEvents)
    {
        for (int k = 0; k < numEvents; ++k)
        {
            const auto& e = events[k];
            if (e.note < 0) kbCount = 0;
            else if (e.vel <= 0) { for (int i = 0; i < kbCount; ++i) if (kb[i].note == e.note) { removeKb (i); break; } }
        }
        if (kbCount == 0 && src == Src::Midi && sounding)
        {
            sounding = held = arpActive = arpGate = false;
            ampEnv.gate (false); fltEnv.gate (false);
        }
    }
private:
    void removeKb (int i) { for (int k = i; k < kbCount - 1; ++k) kb[k] = kb[k + 1]; --kbCount; }

    void startNote (float note, float velocity, Src s, bool legatoMove)
    {
        if (note <= 0) return;
        const auto& p = *prm;
        src = s; vel = clampf (velocity, 0.05f, 1.f);
        const bool wasSounding = sounding && ! held;
        held = false; sounding = true; rootNote = note; lastFollow = note;
        if (p.on (sd::arpOn)) { if (! arpActive) startArp(); return; }
        arpActive = false;
        setChordTargets (note, ! wasSounding);
        if (! (wasSounding && (p.on (sd::legato) || legatoMove))) retrigger (vel);
    }
    void stopNote()
    {
        const auto& p = *prm;
        if (p.on (sd::hold) && sounding) { held = true; return; }   // keep droning
        sounding = false; held = false;
        if (! arpActive) { ampEnv.gate (false); fltEnv.gate (false); }
        else { arpActive = false; arpGate = false; ampEnv.gate (false); fltEnv.gate (false); }
    }
    void retrigger (float v, float accent = 0.f)
    {
        ampEnv.gate (true, juce::jmin (1.f, 0.75f + 0.25f * v + accent * 0.2f)); fltEnv.gate (true, 1.f + accent * 0.35f);
        if (prm->get (sd::lFade) >= 1.f) lfoFade = 0;
    }
    void setChordTargets (float root, bool snap)
    {
        const auto& p = *prm;
        float ch[4]; int cnt = 1;
        if (p.choice (sd::chord) > 0)
        {
            // the chord shape comes from the rounded note, so bends move the whole chord
            const float rr = std::round (root);
            cnt = chordNotes (rr, p.choice (sd::chord), p.choice (sd::key), p.choice (sd::scale), ch);
            for (int i = 0; i < cnt; ++i) ch[i] += root - rr;
        }
        else ch[0] = root;
        numNotes = cnt;
        for (int i = 0; i < kMaxNotes; ++i)
        {
            auto& s = notes[i];
            if (i < cnt) { s.target = ch[i]; if (snap || ! s.on) s.cur = ch[i]; s.on = true; }
            else s.on = false;
        }
    }

    // ------------------------------------------------------------------ arpeggiator
    static double rateBeats (int r) { static const double b[6] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 }; return b[juce::jlimit (0, 5, r)]; }
    void startArp()
    {
        const auto& p = *prm;
        arpActive = true; arpIdx = 0; arpDir = 1; ratchetLeft = 0; arpGate = false;
        const double stepBeats = rateBeats (p.choice (sd::arpRate));
        // in a playing DAW, lock to its grid; otherwise start right now
        arpPhase = p.hostPlaying ? p.ppq / stepBeats : 0.0;
        arpStepK = std::numeric_limits<int>::min(); arpStep = -1;
        numNotes = 1;
    }
    int arpList (float root, float* out) const
    {
        const auto& p = *prm;
        float ch[4]; int cnt = 1; ch[0] = root;
        if (p.choice (sd::chord) > 0) { const float rr = std::round (root); cnt = chordNotes (rr, p.choice (sd::chord), p.choice (sd::key), p.choice (sd::scale), ch); for (int i = 0; i < cnt; ++i) ch[i] += root - rr; }
        const int oct = p.choice (sd::arpOct) + 1;
        int n = 0;
        for (int o = 0; o < oct; ++o) for (int i = 0; i < cnt; ++i) out[n++] = ch[i] + 12.f * o;
        return n;
    }
    void arpTick (const SynthParams& p, int samples)
    {
        if (! arpActive) return;
        if (! sounding && ! held) { arpActive = false; return; }
        const double stepBeats = rateBeats (p.choice (sd::arpRate));
        const double stepLen = stepBeats * 60.0 / juce::jlimit (20.0, 400.0, p.bpm) * sr;
        const int len = juce::jlimit (1, 16, (int) std::lround (p.get (sd::arpLen)));
        const double swing = p.get (sd::arpSwing) * 0.01 * 0.5;
        arpPhase += samples / stepLen;
        // gate off / ratchets inside the current step
        const double inStep = arpPhase - std::floor (arpPhase);
        if (arpGate && arpPhase >= arpGateOff) { arpGate = false; ampEnv.gate (false); fltEnv.gate (false); }
        if (ratchetLeft > 0 && arpPhase >= nextRatchet)
        {
            --ratchetLeft; nextRatchet += ratchetLen;
            retrigger (lastStepVel, lastStepAcc); arpGate = true;
            arpGateOff = nextRatchet - ratchetLen + ratchetLen * p.get (sd::arpGate) * 0.01 * 0.9;
        }
        // next step (odd steps are swung)
        const int k = (int) std::floor (arpPhase);
        const double startAt = std::floor (arpPhase) + ((k & 1) ? swing : 0.0);
        if ((k != arpStepK || arpStep < 0) && arpPhase >= startAt)
        {
            arpStepK = k;
            arpStep = ((k % len) + len) % len;
            const auto s = sd::unpackStep (p.steps[arpStep]);
            const float root = rootNote > 0 ? rootNote : arpNote;
            if (s.on && (s.chance >= 100 || rng.nextInt (100) < s.chance) && root > 0)
            {
                const int pat = p.choice (sd::arpPattern);
                float note;
                if (pat == 4) note = scaleTone (root, s.note, p.choice (sd::key), p.choice (sd::scale));
                else
                {
                    float list[16]; const int cnt = arpList (root, list);
                    if (pat == 3) note = list[rng.nextInt (cnt)];
                    else if (pat == 2 && cnt > 1)
                    {
                        const int period = 2 * cnt - 2;
                        const int q = arpIdx % period; note = list[q < cnt ? q : period - q]; ++arpIdx;
                    }
                    else { const int q = arpIdx % cnt; note = list[pat == 1 ? cnt - 1 - q : q]; ++arpIdx; }
                }
                note += 12.f * s.oct;
                arpNote = note;
                notes[0].target = note; if (! arpGate || glideCoef <= 0.f) notes[0].cur = (s.tie && arpGate) ? notes[0].cur : note;
                notes[0].on = true; for (int i = 1; i < kMaxNotes; ++i) notes[i].on = false; numNotes = 1;
                const auto next = sd::unpackStep (p.steps[(arpStep + 1) % len]);
                lastStepVel = s.acc ? 1.f : 0.72f; lastStepAcc = s.acc ? 1.f : 0.f;
                if (! (s.tie && arpGate)) retrigger (lastStepVel, lastStepAcc);
                arpGate = true;
                const double base = startAt;
                ratchetLen = (1.0 - ((k & 1) ? swing : 0.0)) / s.ratchet;
                ratchetLeft = s.ratchet - 1; nextRatchet = base + ratchetLen;
                const bool tieNext = next.on && next.tie && s.ratchet == 1;
                arpGateOff = tieNext ? base + 2.0 : base + ratchetLen * p.get (sd::arpGate) * 0.01 * (s.ratchet > 1 ? 0.9 : 1.0);
            }
            juce::ignoreUnused (inStep);
        }
    }

    // ------------------------------------------------------------------ control rate
    void controlTick (const SynthParams& p, int lfoTgt, float lfoDepth, float tuneHz)
    {
        // HOLD switched off while droning: let go
        if (held && ! p.on (sd::hold)) { held = false; sounding = false; arpActive = false; arpGate = false; ampEnv.gate (false); fltEnv.gate (false); }
        // arpeggiator
        if (p.on (sd::arpOn) && (sounding || held))
        {
            if (! arpActive) startArp();
            arpTick (p, kCtl);
        }
        else if (arpActive) { arpActive = false; if (sounding || held) { setChordTargets (rootNote, false); if (! ampEnv.active() || ampEnv.stage == 4) retrigger (vel); } }
        else if ((sounding || held) && rootNote > 0 && ! p.on (sd::arpOn)) setChordTargets (rootNote, false);

        // LFO
        const double beat = 60.0 / juce::jlimit (20.0, 400.0, p.bpm);
        static const double divBeats[8] = { 4, 2, 1, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        const double hz = p.on (sd::lSync) ? 1.0 / (divBeats[juce::jlimit (0, 7, p.choice (sd::lDiv))] * beat) : (double) p.get (sd::lRate);
        lfoPhase += hz * kCtl / sr;
        bool wrappedLfo = false;
        if (lfoPhase >= 1.0) { lfoPhase -= std::floor (lfoPhase); wrappedLfo = true; }
        const float ph = (float) lfoPhase;
        float lfo = 0;
        switch (p.choice (sd::lShape))
        {
            case 0: lfo = std::sin (kTwoPi * ph); break;
            case 1: lfo = ph < 0.25f ? ph * 4.f : ph < 0.75f ? 2.f - ph * 4.f : ph * 4.f - 4.f; break;
            case 2: lfo = 1.f - 2.f * ph; break;
            case 3: lfo = ph < 0.5f ? 1.f : -1.f; break;
            case 4: if (wrappedLfo) lfoHeld = noise.next(); lfo = lfoHeld; break;
            default:
                if (wrappedLfo) lfoTarget = noise.next();
                lfoSmooth += (lfoTarget - lfoSmooth) * (float) juce::jmin (1.0, hz * kCtl / sr * 4.0); lfo = lfoSmooth; break;
        }
        const float fadeMs = p.get (sd::lFade);
        if (fadeMs >= 1.f) lfoFade = juce::jmin (1.f, lfoFade + (float) kCtl / (fadeMs * 0.001f * (float) sr)); else lfoFade = 1.f;
        const float m = lfo * lfoDepth * lfoFade;
        const float pitchMod = lfoTgt == 0 ? m * 2.f : 0.f;              // up to +-2 semitones
        const float cutMod = lfoTgt == 1 ? m * 3.f : 0.f;                // up to +-3 octaves
        shapeMod = lfoTgt == 2 ? m * 50.f : 0.f;
        tremolo = lfoTgt == 3 ? 1.f - lfoDepth * lfoFade * 0.5f * (1.f - lfo) : 1.f;
        if (lfoTgt == 4) { const float pan = clampf (m, -1.f, 1.f); panL = std::sqrt (0.5f * (1.f - pan)) * 1.4142f; panR = std::sqrt (0.5f * (1.f + pan)) * 1.4142f; }
        else panL = panR = 1.f;

        // character: tape wow/flutter, analog drift
        float charCents = 0;
        if (character == 3)
        {
            wow += 0.55f * kCtl / (float) sr; if (wow >= 1) wow -= 1;
            flutter += 6.3f * kCtl / (float) sr; if (flutter >= 1) flutter -= 1;
            charCents = std::sin (kTwoPi * wow) * 14.f + std::sin (kTwoPi * flutter) * 3.f;
        }
        const bool drifts = character == 1 || character == 3;

        // glide + pitches
        for (int i = 0; i < kMaxNotes; ++i)
        {
            auto& s = notes[i]; if (! s.on) continue;
            s.cur = glideCoef > 0 ? s.target + (s.cur - s.target) * glideCoef : s.target;
        }
        // the pitch wheel glides (no zipper), the mod wheel adds a vibrato (5.5 Hz, up to +-0.6 st)
        const float wa = 1.f - std::exp (-(float) kCtl / (0.012f * (float) sr));
        bendNow += (clampf (p.bend, -24.f, 24.f) - bendNow) * wa; modNow += (clampf (p.mod, 0.f, 1.f) - modNow) * wa;
        vibPh += 5.5f * kCtl / (float) sr; if (vibPh >= 1) vibPh -= 1;
        const float semisBase = pitchMod + charCents * 0.01f + bendNow + std::sin (kTwoPi * vibPh) * 0.6f * modNow;
        syncOn = p.on (sd::bSync) && p.on (sd::bOn) && p.get (sd::bLevel) > 0.f;
        for (int o = 0; o < 2; ++o)
        {
            const int base = o == 0 ? sd::aOn : sd::bOn;
            oscOn[o] = p.on (base) && p.get (base + 5) > 0.f;
            // with sync on, A keeps running silently so B can follow it
            oscRun[o] = oscOn[o] || (o == 0 && syncOn);
            if (! oscRun[o]) continue;
            const int wave = p.choice (base + 1);
            const bool isSuper = wave == 4;
            const int cnt = isSuper ? 7 : 1 + 2 * juce::jlimit (0, 3, p.choice (base + 6));
            oscWave[o] = wave; oscCount[o] = cnt;
            oscShape[o] = clampf (p.get (base + 4) + shapeMod, 0.f, 100.f) * 0.01f;
            const float level = oscOn[o] ? p.get (base + 5) * 0.01f : 0.f;
            const float spread = isSuper ? (0.06f + p.get (base + 7) * 0.01f * 0.6f) : p.get (base + 7) * 0.01f * 0.5f;   // semitones
            const float width = p.get (base + 8) * 0.01f;
            const float semis = (float) p.get (base + 2) + p.get (base + 3) * 0.01f + semisBase;
            static const float sup[7] = { -1.f, -0.62f, -0.3f, 0.f, 0.29f, 0.64f, 0.98f };
            const float noteNorm = 1.f / std::sqrt ((float) juce::jmax (1, numNotes));
            for (int u = 0; u < cnt; ++u)
            {
                const float off = cnt == 1 ? 0.f : isSuper ? sup[u] : (u - (cnt - 1) * 0.5f) / ((cnt - 1) * 0.5f);
                float side = 1.f;
                if (isSuper && u != 3) side = 0.25f + oscShape[o] * 0.75f;
                float& d = drift[o * kMaxUni + u];
                if (drifts) d = clampf (d + noise.next() * 0.004f, -0.07f, 0.07f) * 0.999f; else d *= 0.99f;
                const float pan = off * width;
                for (int i = 0; i < kMaxNotes; ++i)
                {
                    auto& v = osc[o][i][u];
                    if (! notes[i].on) continue;
                    float note = notes[i].cur + semis + off * spread + d;
                    if (character == 2) note = std::round (note);                       // 8-bit: arcade pitch steps
                    const float f = tuneHz * std::exp2 ((note - 69.f) / 12.f);
                    v.inc = clampf (f / (float) osSr, 0.f, 0.45f);
                    v.gain = level * level * side * 0.36f / std::sqrt ((float) cnt) * noteNorm;
                    v.panL = std::sqrt (0.5f * (1.f - pan)) * 1.4142f; v.panR = std::sqrt (0.5f * (1.f + pan)) * 1.4142f;
                }
            }
        }
        // sub follows the lowest sounding note
        subLevel = p.get (sd::subLevel) * 0.01f; subSquare = p.choice (sd::subWave) == 1;
        if (subLevel > 0 && notes[0].on)
        {
            const float f = tuneHz * std::exp2 ((notes[0].cur - 12.f * (1 + p.choice (sd::subOct)) + semisBase - 69.f) / 12.f);
            subInc = clampf (f / (float) osSr, 0.f, 0.45f);
        }
        noiseLevel = p.get (sd::noiseLevel) * 0.01f;
        noiseCoef = 1.f - std::exp (-kTwoPi * clampf (300.f * std::exp2 (p.get (sd::noiseColor) * 0.06f), 20.f, (float) osSr * 0.45f) / (float) osSr);

        // filter
        const float fe = fltEnv.advance (kCtl);
        const float dyn = p.get (sd::dyn) * 0.01f;
        const float velMod = 1.f - dyn * 0.45f * (1.f - (src == Src::Guitar ? dynGain : vel));
        const float keyOct = ((notes[0].on ? notes[0].cur : 52.f) - 52.f) / 12.f * p.get (sd::fKey) * 0.01f;
        const float envOct = p.get (sd::fEnv) * 0.01f * 6.f * fe * velMod;
        float jitter = 1.f; if (character == 1) { cutDrift = clampf (cutDrift + noise.next() * 0.002f, -0.04f, 0.04f); jitter = 1.f + cutDrift; }
        const float fc = clampf (p.get (sd::fCutoff) * jitter * std::exp2 (envOct + keyOct + cutMod), 20.f, 20000.f);
        const float res = p.get (sd::fRes) * 0.01f, drive = 1.f + p.get (sd::fDrive) * 0.01f * 5.f;
        const int type = p.choice (sd::fType);
        for (auto& f : flt)
        {
            if (type <= 1) f.setLadder (osSr, fc, res, type == 0, drive);
            else if (type == 6)
            {
                float fr[3]; vowelFormants (fc, fr);
                f.setVowel (osSr, fr, res, drive);
            }
            else f.setSvf (osSr, fc, res, type, drive);
        }
        // the amp envelope finished: forget the held note
        if (! ampEnv.active() && ! sounding && ! held) { for (auto& s : notes) s.on = false; }
    }

    static void vowelFormants (float cut, float* out)
    {
        static const float V[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 2300, 3000 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
        const float x = clampf (std::log (cut / 80.f) / std::log (100.f), 0.f, 1.f) * 4.f;
        const int i = juce::jmin (3, (int) x); const float t = x - (float) i;
        for (int k = 0; k < 3; ++k) out[k] = V[i][k] + (V[i + 1][k] - V[i][k]) * t;
    }

    // ------------------------------------------------------------------ audio rate
    inline float oscSample (int wave, float shape, OscV& v)
    {
        const float t = v.phase, dt = v.inc;
        float out;
        switch (wave)
        {
            case 0:
            {
                float a = 2.f * t - 1.f - blep (t, dt);
                if (shape > 0.001f) { const float t2 = wrap01 (t + 0.5f * shape); const float b = 2.f * t2 - 1.f - blep (t2, dt); a = (1.f - 0.5f * shape) * a + 0.5f * shape * b; }
                out = a; break;
            }
            case 1:
            {
                const float pw = 0.5f - 0.45f * shape;
                out = (t < pw ? 1.f : -1.f) + blep (t, dt) - blep (wrap01 (t + 1.f - pw), dt) - (2.f * pw - 1.f);
                out *= 0.85f; break;
            }
            case 2:
            {
                const float tri = t < 0.5f ? 4.f * t - 1.f : 3.f - 4.f * t;
                out = std::sin (0.5f * kPi * tri * (1.f + 3.f * shape)); break;
            }
            case 3:
            {
                const float fb = 0.5f * (v.y1 + v.y2) * shape * 2.2f;
                out = std::sin (kTwoPi * t + fb); v.y2 = v.y1; v.y1 = out; break;
            }
            default: out = 2.f * t - 1.f - blep (t, dt); break;   // super: each voice a saw
        }
        v.phase += dt;
        v.wrapped = false;
        if (v.phase >= 1.f) { v.phase -= 1.f; v.wrapped = true; v.wrapFrac = dt > 0 ? v.phase / dt : 0.f; }
        return out;
    }
    static inline float naiveAt (int wave, float shape, float t)
    {
        if (wave == 1) { const float pw = 0.5f - 0.45f * shape; return (t < pw ? 1.f : -1.f) * 0.85f; }
        if (wave == 0 || wave == 4) return 2.f * t - 1.f;
        return 0.f;
    }
    inline void renderVoices (float& l, float& r)
    {
        for (int i = 0; i < kMaxNotes; ++i)
        {
            if (! notes[i].on) continue;
            if (oscOn[0])
                for (int u = 0; u < oscCount[0]; ++u)
                {
                    auto& v = osc[0][i][u];
                    const float s = oscSample (oscWave[0], oscShape[0], v) * v.gain;
                    l += s * v.panL; r += s * v.panR;
                }
            else if (oscRun[0])
                for (int u = 0; u < oscCount[0]; ++u)
                {
                    auto& v = osc[0][i][u];
                    v.phase += v.inc; v.wrapped = false;
                    if (v.phase >= 1.f) { v.phase -= 1.f; v.wrapped = true; v.wrapFrac = v.inc > 0 ? v.phase / v.inc : 0.f; }
                }
            if (oscOn[1])
                for (int u = 0; u < oscCount[1]; ++u)
                {
                    auto& v = osc[1][i][u];
                    float s = oscSample (oscWave[1], oscShape[1], v);
                    if (syncOn && oscRun[0])
                    {
                        const auto& m = osc[0][i][u % juce::jmax (1, oscCount[0])];
                        if (m.wrapped)
                        {
                            // hard sync: restart B where A wrapped, with a one-sided BLEP for the jump
                            const float before = naiveAt (oscWave[1], oscShape[1], v.phase);
                            v.phase = m.wrapFrac * v.inc;
                            const float after = naiveAt (oscWave[1], oscShape[1], v.phase);
                            const float x = clampf (m.wrapFrac, 0.f, 1.f);
                            s += -(after - before) * 0.5f * (1.f - x) * (1.f - x);
                        }
                    }
                    s *= v.gain;
                    l += s * v.panL; r += s * v.panR;
                }
        }
        if (subLevel > 0 && notes[0].on)
        {
            float s;
            if (subSquare) s = ((sub[0] < 0.5f ? 1.f : -1.f) + blep (sub[0], subInc) - blep (wrap01 (sub[0] + 0.5f), subInc)) * 0.45f;
            else s = std::sin (kTwoPi * sub[0]) * 0.9f;
            sub[0] += subInc; if (sub[0] >= 1.f) sub[0] -= 1.f;
            s *= subLevel * 0.55f; l += s; r += s;
        }
        if (noiseLevel > 0)
        {
            noiseLp += (noise.next() - noiseLp) * noiseCoef;
            const float s = noiseLp * noiseLevel * 0.35f * (1.6f - noiseCoef * 0.6f);
            l += s; r += s * 0.9f;
        }
    }

    inline void crush (float& l, float& r)
    {
        // 8-bit: about 11 kHz sample rate, 5-bit levels
        const int hold = juce::jmax (1, (int) std::lround (sr / 11025.0));
        if (++crushCount >= hold) { crushCount = 0; crushL = std::round (l * 16.f) / 16.f; crushR = std::round (r * 16.f) / 16.f; }
        l = crushL; r = crushR;
    }
    inline void vhs (float& l, float& r, float env)
    {
        const float c = 1.f - std::exp (-kTwoPi * 5200.f / (float) sr);
        vhsLp1 += (l - vhsLp1) * c; vhsLp2 += (vhsLp1 - vhsLp2) * c;
        vhsLp3 += (r - vhsLp3) * c; vhsLp4 += (vhsLp3 - vhsLp4) * c;
        const float hiss = noise.next() * 0.0025f * juce::jmin (1.f, env * 3.f);
        l = fastTanh (vhsLp2 * 1.2f) + hiss; r = fastTanh (vhsLp4 * 1.2f) + hiss * 0.8f;
    }
    static inline float softLimit (float x) { return std::abs (x) < 0.9f ? x : (x > 0 ? 0.9f + 0.1f * fastTanh ((x - 0.9f) * 10.f) : -0.9f - 0.1f * fastTanh ((-x - 0.9f) * 10.f)); }
    void panicReset() { for (auto& f : flt) f.reset(); for (auto& o : osc) for (auto& s : o) for (auto& v : s) { v.y1 = v.y2 = 0; } echoL.clear(); echoR.clear(); }

    // ------------------------------------------------------------------ state
    double sr = 48000, osSr = 96000;
    const SynthParams* prm = nullptr;
    obs::amp::Over2 overL, overR; bool wasActive = false;
    EnvFollower envFast, envSlow;
    bool gateOpen = false, sounding = false, held = false, pendingStart = false;
    Src src = Src::Guitar;
    struct Key { int note; float vel; } kb[8] {}; int kbCount = 0;
    int onsetHold = 0;
    float detectedMidi = -1, lastFollow = -1, rootNote = -1, vel = 0.8f;
    float dynGain = 0, dynTarget = 0;
    Adsr ampEnv, fltEnv;
    NoteSlot notes[kMaxNotes]; int numNotes = 1;
    OscV osc[2][kMaxNotes][kMaxUni];
    bool oscOn[2] {}, oscRun[2] {}; int oscWave[2] {}, oscCount[2] { 1, 1 }; float oscShape[2] {};
    bool syncOn = false;
    float sub[1] {}, subInc = 0, subLevel = 0; bool subSquare = false;
    float noiseLevel = 0, noiseLp = 0, noiseCoef = 1;
    float drift[2 * kMaxUni] {}, cutDrift = 0;
    Filter flt[2];
    float glideCoef = 0;
    // lfo
    double lfoPhase = 0; float lfoHeld = 0, lfoSmooth = 0, lfoTarget = 0, lfoFade = 1, shapeMod = 0, tremolo = 1, panL = 1, panR = 1;
    // arp
    bool arpActive = false, arpGate = false; double arpPhase = 0, arpGateOff = 0, nextRatchet = 0, ratchetLen = 1; int arpStep = -1, arpStepK = -999999, arpIdx = 0, arpDir = 1, ratchetLeft = 0;
    float arpNote = 0, lastStepVel = 0.8f, lastStepAcc = 0;
    // character
    int character = 0, crushCount = 0; float crushHold = 0, crushL = 0, crushR = 0;
    float vhsLp1 = 0, vhsLp2 = 0, vhsLp3 = 0, vhsLp4 = 0, wow = 0, flutter = 0;
    float bendNow = 0, modNow = 0, vibPh = 0;          // pitch wheel (smoothed), mod wheel vibrato
    // space
    DelayLine echoL, echoR; Allpass diffL[2], diffR[2]; OnePole echoDamp; float echoLpL = 0, echoLpR = 0, echoSamples = 1000, echoFb = 0.3f, echoWet = 0;
    DcBlocker dcL, dcR;
    Noise noise; juce::Random rng;
    float levelSm = 1;
    int ctlLeft = 0;
    std::atomic<float> activity { 0.f }, liveNote { -1.f };
    std::atomic<int> liveStep { -1 }, liveSrc { 0 };
};
} // namespace obs::dsp
