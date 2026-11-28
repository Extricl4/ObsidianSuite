#pragma once
/*  ORPHEUS · SIREN's voice (C++), a faithful copy of the UI's preview engine (ui/src/synth.js, "preview engine").
    One voice plays a whole chord (ORPHEUS's four lines, or the keys held on a MIDI keyboard):
      oscillators A and B (wavetables, 1-7 unison voices spread across the stereo field, SUPER = 7 saws),
      SUB, NOISE, OCTAVE's extra layer  ->  drive  ->  filter (Ladder 24/12, Lowpass, Bandpass, Highpass, Notch, Vowel)
      ->  amp  ->  CHARACTER (8-bit, VHS)  ->  LFO volume / pan  ->  out
    A new note count makes a new voice (the old one fades out), like the preview. Changing a sound setting rebuilds the
    voice 50 ms after the last change, crossfaded, also like the preview.                                          */
#include "Prims.h"
#include "Wavetables.h"
#include "../Params.h"
#include <vector>
#include <memory>

namespace orph
{
class SirenSynth
{
public:
    void prepare (double sampleRate);
    void setBpm (double b) noexcept { bpm = b; }
    /** notes: (fractional) MIDI notes; hold = ORPHEUS: the notes ring at full level until the next change */
    void noteOn (const float* notes, int n, float vel, bool legato, bool hold, const Snapshot& S);
    void setPitches (const float* notes, int n, double glideSec, const Snapshot& S);
    void noteOff (const Snapshot& S);
    void filterKick (float root, const Snapshot& S);
    /** after the parameters changed: rebuilds the voice 50 ms after the last change to a sound setting */
    void paramsChanged (const Snapshot& now, const Snapshot& before);
    /** adds n samples of the voice (after SIREN's output level) to L / R */
    void render (const Snapshot& S, float* L, float* R, int n);
    bool gate() const noexcept { return gateOpen; }
    bool active() const noexcept { return cur != nullptr || ! dying.empty(); }
    void allOff() noexcept { cur.reset(); dying.clear(); gateOpen = false; }
    /** the wheels: pitch bend (semitones) and the mod wheel (0..1: a 5.5 Hz vibrato up to 60 cents) */
    void setWheels (float bendSemis, float mod01) noexcept { bendT = bendSemis * 100.f; vibT = mod01 * 60.f; }

private:
    struct Osc
    {
        const Wavetables::Table* tb = nullptr; double ph = 0; int ni = 0; float semi = 0, cents = 0, gain = 0, gl = 0, gr = 0;
        bool sine = false, square = false, layer = false, odd = false;   // sine/square: the SUB (plain shapes)
    };
    struct FilterEnv                                       // the filter's frequency: ramps through the shape's points
    {
        Target f; std::vector<std::pair<float, int>> pts; size_t next = 0; int t = 0;  // (Hz, sample it arrives at)
        void startRamps (const std::vector<std::pair<float, double>>& p, double sr);
        float tick() noexcept;
    };
    struct Voice
    {
        std::vector<Osc> oscs; int nNotes = 1; std::vector<float> notes;
        Target amp, octG, lfoAmt, trem, killed;
        int ft = 0; bool fOn = true; int nf = 0; double q[3] {}; WABiquad fl[3][2]; FilterEnv fe[3]; float vowelG[3] { 1, 1, 1 };
        double drive = 1, driveNorm = 1; Os2 os[2];
        double base0 = 1000; Target fMul;   // the cutoff the filter's ramps were made from; a live change scales them
        int ch = 0; WABiquad vhs[2]; double charPh = 0, charRate = 0, charCents = 0;
        int lfoShape = 0, lfoTarget = 0; bool lfoOn = false; double lfoPh = 0, lfoDepth = 0, rndA = 0, rndT = 0; int rndLeft = 0; float lfoAmtFull = 0;
        std::shared_ptr<const std::vector<float>> noise; size_t noiseI = 0; float noiseG = 0;
        std::vector<int> lvl; std::vector<double> ratio;   // per oscillator: its wavetable level, its pitch ratio
        std::vector<Target> hz;            // each chord note's frequency (glides)
        bool dying = false; int life = 0;
    };
    std::unique_ptr<Voice> makeVoice (const std::vector<float>& notes, const Snapshot& S);
    void applyFilter (Voice& v, float root, float vel, bool steady, const Snapshot& S);
    void startAmp (Voice& v, float vel, const Snapshot& S, bool retrig);
    float renderVoice (Voice& v, const Snapshot& S, float* L, float* R, int n);
    double hz (double midi, const Snapshot& S) const noexcept { return 440.0 * std::pow (2.0, (midi - 69.0) / 12.0) * (S[P_tune] / 440.0); }
    double lfoHz (const Snapshot& S) const noexcept;

    double sr = 48000, bpm = 120;
    std::unique_ptr<Voice> cur;
    std::vector<std::unique_ptr<Voice>> dying;
    bool gateOpen = false, fullSus = false; float velNow = .9f; std::vector<float> curNotes;
    int rebuildIn = -1;                        // samples until the voice is rebuilt (-1: no)
    Target outGain;
    Rng rng;
    std::shared_ptr<const std::vector<float>> noiseBuf; float noiseColor = -1;   // a second of coloured noise (made again when COLOR changes)
    std::vector<float> bufL, bufR;
    float bendT = 0, vibT = 0; double bendC = 0, vibC = 0, vibPh = 0, wheel = 0;
};
} // namespace orph
