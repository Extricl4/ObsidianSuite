#pragma once
// =============================================================================
//  PitchTracker: the guitar's note for the tuner, the synth (SIREN), KERAUNOS
//  and AETHER's FOLLOW. Runs HYDRA's FastTracker (NSDF, a new estimate every
//  millisecond, the window sized to the note and ending at the newest sample),
//  then the same note rules as HYDRA:
//   * after a pick attack, the new note is only reported once a window holds
//     nothing but the new note (no blip of the old one, no wrong first note);
//     until then confidence() is 0;
//   * slides, hammer-ons and pull-offs need 3 agreeing estimates (3 ms), an
//     octave jump without a pick 8 (the tracker, not the guitar, usually);
//   * within 0.7 semitones of the note, the pitch follows continuously, so
//     bends and vibrato come through.
//  A high note is known ~2 periods after the pick, a low E in ~25 ms.
//  Range 38 Hz (8-strings, drop E) to 1.7 kHz.
// =============================================================================
#include "FastTracker.h"
#include <array>
#include <atomic>
#include <cmath>

namespace obs::dsp
{
class PitchTracker
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        ft.prepare (sr, 38.0);           // down to 8-strings and drop E
        ft.octaveRatio = octRatio;
        lvlAtt = (float) std::exp (-1.0 / (0.001 * sr)); lvlRel = (float) std::exp (-1.0 / (0.120 * sr));
        reset();
    }

    void reset()
    {
        ft.reset();
        t = 0; cur = -1; cand = -1; candN = 0; lastVoiced = -(1 << 30); lastOnset = -1; resolved = -1;
        hist = {}; nh = 0; env = 0;
        freq.store (0.f); conf.store (0.f); rms.store (0.f);
        serial.store (0); attackNote.store (false); pending.store (false); sudden.store (false);
    }

    /** Feed host-rate mono samples. */
    void process (const float* x, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float a = std::abs (x[i]);
            env = a > env ? lvlAtt * env + (1.f - lvlAtt) * a : lvlRel * env + (1.f - lvlRel) * a;
            bool onset = false;
            const bool est = ft.push (x[i], t, onset);
            if (onset) { lastOnset = t; pending.store (true, std::memory_order_relaxed); conf.store (0.f, std::memory_order_relaxed); }
            if (est) follow (ft.est());
            ++t;
        }
        rms.store (env);
    }

    float frequency() const     { return freq.load (std::memory_order_relaxed); }   // 0 = none
    float confidence() const    { return conf.load (std::memory_order_relaxed); }
    float inputLevel() const    { return rms.load (std::memory_order_relaxed); }
    /** Counts the notes found: +1 for every new note (a pick attack's note, also the same note picked
        again, a hammer-on, a slide or bend reaching the next note). */
    int noteSerial() const      { return serial.load (std::memory_order_relaxed); }
    /** The latest note came from a pick attack (not a hammer-on, slide or bend). */
    bool noteWasPicked() const  { return attackNote.load (std::memory_order_relaxed); }
    /** The latest change was a jump (hammer-on / pull-off), not a bend or slide passing through. */
    bool noteWasSudden() const  { return sudden.load (std::memory_order_relaxed); }
    /** A pick attack whose note isn't known yet. */
    bool resolving() const      { return pending.load (std::memory_order_relaxed); }

    /** Midi note number (fractional) for a frequency, given the A4 reference. */
    static float toMidi (float hz, float a4 = 440.f)  { return hz > 0 ? 69.f + 12.f * std::log2 (hz / a4) : -1.f; }
    static float toHz (float midi, float a4 = 440.f)  { return a4 * std::pow (2.f, (midi - 69.f) / 12.f); }

private:
    using i64 = hydra::i64;

    void publish (float f, float c) { freq.store (f, std::memory_order_relaxed); conf.store (c, std::memory_order_relaxed); }
    void newNote (int n, bool picked, bool jump)
    {
        cur = n; attackNote.store (picked, std::memory_order_relaxed); sudden.store (jump, std::memory_order_relaxed);
        serial.fetch_add (1, std::memory_order_relaxed);
    }

    void follow (const hydra::FastTracker::Estimate& e)
    {
        if (! e.voiced || e.f0 <= 0)
        {
            // a gap (the note died, or silence): forget the note; a short dropout keeps it
            if (e.time - lastVoiced > (i64) (0.08 * sr) && cur >= 0) { cur = -1; publish (0.f, 0.f); }
            else if (cur >= 0 && ! pending.load (std::memory_order_relaxed)) conf.store (std::min (conf.load (std::memory_order_relaxed), e.clarity), std::memory_order_relaxed);
            if (e.time - lastVoiced > (i64) (0.08 * sr) && env < 0.0006f) publish (0.f, 0.f);
            return;
        }
        const float m = 69.f + 12.f * std::log2 (e.f0 / 440.f);
        const int n = (int) std::lround (m);
        const float mAgo = hist[(size_t) ((nh + 1) % kH)];
        hist[(size_t) (nh % kH)] = m; ++nh;

        // a pick attack whose note isn't known yet: wait for a window that holds only the new note
        if (lastOnset >= 0 && lastOnset > resolved)
        {
            if (e.time - lastOnset > (i64) (0.1 * sr)) { resolved = lastOnset; pending.store (false, std::memory_order_relaxed); }   // give up: keep following
            else
            {
                const i64 windowStart = e.time - (i64) (e.span * (e.clarity >= 0.95f ? 0.75f : 1.f));
                if (windowStart < lastOnset + (i64) (0.0005 * sr) || e.clarity < 0.86f || std::abs (m - (float) n) > 0.45f) return;
                if (n == cand) ++candN; else { cand = n; candN = 1; }
                if (candN < (e.clarity > 0.95f && std::abs (m - (float) n) < 0.3f ? 1 : 2)) return;
                candN = 0; resolved = lastOnset; lastVoiced = e.time;
                newNote (n, true, true);
                pending.store (false, std::memory_order_relaxed);
                publish (e.f0, e.clarity);
                return;
            }
        }

        // no new attack: follow slides, hammer-ons and bends with hysteresis
        const bool gap = cur < 0 || e.time - lastVoiced > (i64) (0.08 * sr);
        lastVoiced = e.time;
        if (! gap && std::abs (m - (float) cur) <= 0.70f) { candN = 0; publish (e.f0, e.clarity); return; }
        if (std::abs (m - (float) n) > 0.45f || e.clarity < 0.84f) return;          // between notes: keep the last
        if (n == cand) ++candN; else { cand = n; candN = 1; }
        const bool octaveJump = cur >= 0 && std::abs (n - cur) == 12;
        if (candN < (gap ? 2 : (octaveJump ? 8 : 3))) return;
        candN = 0;
        if (n != cur || gap)
        {
            // a hammer-on / pull-off jumps; a bend or slide passes through (it was near the old note just before)
            const bool jump = cur >= 0 && nh > kH && std::abs (mAgo - (float) cur) < 0.4f;
            newNote (n, gap, jump || gap);
        }
        publish (e.f0, e.clarity);
    }

    double sr = 48000;
public:
    float octRatio = 0.8f;      // (HYDRA's value; tests can change it)
private:
    hydra::FastTracker ft;
    i64 t = 0, lastVoiced = 0, lastOnset = -1, resolved = -1;
    int cur = -1, cand = -1, candN = 0;
    static constexpr int kH = 8;
    std::array<float, kH> hist {}; int nh = 0;
    float env = 0, lvlAtt = 0.99f, lvlRel = 0.999f;
    std::atomic<float> freq { 0.f }, conf { 0.f }, rms { 0.f };
    std::atomic<int> serial { 0 };
    std::atomic<bool> attackNote { false }, pending { false }, sudden { false };
};
} // namespace obs::dsp
