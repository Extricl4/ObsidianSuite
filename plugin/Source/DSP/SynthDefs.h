#pragma once
// =============================================================================
//  The synth's parameters, in one table: Params.cpp builds the plugin
//  parameters from it and the engine reads them by index (SynthP::...).
//  ui/src/synth.js has the same list (ids, ranges, defaults); the UI test
//  checks that the two match.
// =============================================================================

namespace obs::synthdefs
{
enum Type { F, LOG, INT, BOOL, CHOICE };

struct Def
{
    const char* id; const char* name; Type type;
    float min, max, def;
    const char* unit;   // or the choices, separated by '|', for CHOICE
    float skew;         // JUCE skew (1 = linear); for F only
};

// Order matters: the engine reads values by this index.
enum P
{
    // play
    sens, gate, glide, dyn, legato, hold, chord, key, scale, tune,
    // oscillator A
    aOn, aWave, aPitch, aFine, aShape, aLevel, aVoices, aDetune, aWidth,
    // oscillator B
    bOn, bWave, bPitch, bFine, bShape, bLevel, bVoices, bDetune, bWidth, bSync,
    // sub + noise
    subLevel, subWave, subOct, noiseLevel, noiseColor,
    // filter
    fOn, fType, fCutoff, fRes, fDrive, fEnv, fKey,
    // envelopes
    aAtt, aDec, aSus, aRel, fAtt, fDec, fSus, fRel,
    // lfo
    lShape, lRate, lSync, lDiv, lDepth, lFade, lTarget,
    // arpeggiator
    arpOn, arpPattern, arpRate, arpLen, arpOct, arpGate, arpSwing,
    // output
    mix, level, space, character, post,
    count
};

inline const Def* table()
{
    static const Def d[count] = {
        { "syn.sens",        "Synth Sensitivity",     F,      0,   100,   55,   "%",  1 },
        { "syn.gate",        "Synth Threshold",       F,    -90,   -30,  -66,   "dB", 1 },
        { "syn.glide",       "Synth Glide",           F,      0,  1000,   40,   "ms", 0.5f },
        { "syn.dyn",         "Synth Dynamics",        F,      0,   100,   60,   "%",  1 },
        { "syn.legato",      "Synth Legato",          BOOL,   0,     1,    0,   "",   1 },
        { "syn.hold",        "Synth Hold",            BOOL,   0,     1,    0,   "",   1 },
        { "syn.chord",       "Synth Chord",           CHOICE, 0,     6,    0,   "Off|Octaves|Power|Triad|Sus|Seventh|Ninth", 1 },
        { "syn.key",         "Synth Key",             CHOICE, 0,    11,    4,   "C|C#|D|D#|E|F|F#|G|G#|A|A#|B", 1 },
        { "syn.scale",       "Synth Scale",           CHOICE, 0,     6,    1,   "Major|Minor|Dorian|Phrygian|Harmonic minor|Pentatonic|Blues", 1 },
        { "syn.tune",        "Synth Tuning",          F,    415,   466,  440,   "Hz", 1 },

        { "syn.a.on",        "Osc A On",              BOOL,   0,     1,    1,   "",   1 },
        { "syn.a.wave",      "Osc A Wave",            CHOICE, 0,     4,    0,   "Saw|Pulse|Triangle|Sine|Super", 1 },
        { "syn.a.pitch",     "Osc A Pitch",           INT,  -24,    24,    0,   "st", 1 },
        { "syn.a.fine",      "Osc A Fine",            F,   -100,   100,    0,   "ct", 1 },
        { "syn.a.shape",     "Osc A Shape",           F,      0,   100,    0,   "%",  1 },
        { "syn.a.level",     "Osc A Level",           F,      0,   100,   80,   "%",  1 },
        { "syn.a.voices",    "Osc A Unison",          CHOICE, 0,     3,    1,   "1|3|5|7", 1 },
        { "syn.a.detune",    "Osc A Detune",          F,      0,   100,   22,   "%",  1 },
        { "syn.a.width",     "Osc A Width",           F,      0,   100,   70,   "%",  1 },

        { "syn.b.on",        "Osc B On",              BOOL,   0,     1,    1,   "",   1 },
        { "syn.b.wave",      "Osc B Wave",            CHOICE, 0,     4,    1,   "Saw|Pulse|Triangle|Sine|Super", 1 },
        { "syn.b.pitch",     "Osc B Pitch",           INT,  -24,    24,  -12,   "st", 1 },
        { "syn.b.fine",      "Osc B Fine",            F,   -100,   100,    7,   "ct", 1 },
        { "syn.b.shape",     "Osc B Shape",           F,      0,   100,   35,   "%",  1 },
        { "syn.b.level",     "Osc B Level",           F,      0,   100,   45,   "%",  1 },
        { "syn.b.voices",    "Osc B Unison",          CHOICE, 0,     3,    0,   "1|3|5|7", 1 },
        { "syn.b.detune",    "Osc B Detune",          F,      0,   100,   12,   "%",  1 },
        { "syn.b.width",     "Osc B Width",           F,      0,   100,   40,   "%",  1 },
        { "syn.b.sync",      "Osc B Sync",            BOOL,   0,     1,    0,   "",   1 },

        { "syn.sub.level",   "Sub Level",             F,      0,   100,   20,   "%",  1 },
        { "syn.sub.wave",    "Sub Wave",              CHOICE, 0,     1,    0,   "Sine|Square", 1 },
        { "syn.sub.oct",     "Sub Octave",            CHOICE, 0,     1,    0,   "-1|-2", 1 },
        { "syn.noise.level", "Noise Level",           F,      0,   100,    0,   "%",  1 },
        { "syn.noise.color", "Noise Color",           F,      0,   100,   50,   "%",  1 },

        { "syn.flt.on",      "Filter On",             BOOL,   0,     1,    1,   "",   1 },
        { "syn.flt.type",    "Filter Type",           CHOICE, 0,     6,    0,   "Ladder 24|Ladder 12|Lowpass|Bandpass|Highpass|Notch|Vowel", 1 },
        { "syn.flt.cutoff",  "Filter Cutoff",         LOG,   20, 20000, 1800,   "Hz", 1 },
        { "syn.flt.res",     "Filter Resonance",      F,      0,   100,   30,   "%",  1 },
        { "syn.flt.drive",   "Filter Drive",          F,      0,   100,   20,   "%",  1 },
        { "syn.flt.env",     "Filter Env Amount",     F,   -100,   100,   40,   "%",  1 },
        { "syn.flt.key",     "Filter Key Track",      F,      0,   100,   50,   "%",  1 },

        { "syn.env.a.att",   "Amp Attack",            F,      1,  5000,    3,   "ms", 0.25f },
        { "syn.env.a.dec",   "Amp Decay",             F,      5,  5000,  700,   "ms", 0.3f },
        { "syn.env.a.sus",   "Amp Sustain",           F,      0,   100,   80,   "%",  1 },
        { "syn.env.a.rel",   "Amp Release",           F,      5,  5000,  260,   "ms", 0.3f },
        { "syn.env.f.att",   "Filter Attack",         F,      1,  5000,    2,   "ms", 0.25f },
        { "syn.env.f.dec",   "Filter Decay",          F,      5,  5000,  420,   "ms", 0.3f },
        { "syn.env.f.sus",   "Filter Sustain",        F,      0,   100,   30,   "%",  1 },
        { "syn.env.f.rel",   "Filter Release",        F,      5,  5000,  300,   "ms", 0.3f },

        { "syn.lfo.shape",   "LFO Shape",             CHOICE, 0,     5,    0,   "Sine|Triangle|Saw|Square|Random|Drift", 1 },
        { "syn.lfo.rate",    "LFO Rate",              LOG,  0.05f,  20,    5,   "Hz", 1 },
        { "syn.lfo.sync",    "LFO Sync",              BOOL,   0,     1,    0,   "",   1 },
        { "syn.lfo.div",     "LFO Sync Rate",         CHOICE, 0,     7,    3,   "1/1|1/2|1/4|1/8|1/8T|1/16|1/16T|1/32", 1 },
        { "syn.lfo.depth",   "LFO Depth",             F,      0,   100,    0,   "%",  1 },
        { "syn.lfo.fade",    "LFO Fade In",           F,      0,  3000,    0,   "ms", 0.5f },
        { "syn.lfo.target",  "LFO Target",            CHOICE, 0,     4,    0,   "Pitch|Cutoff|Shape|Volume|Pan", 1 },

        { "syn.arp.on",      "Arp On",                BOOL,   0,     1,    0,   "",   1 },
        { "syn.arp.pattern", "Arp Pattern",           CHOICE, 0,     4,    0,   "Up|Down|Up and down|Random|Steps", 1 },
        { "syn.arp.rate",    "Arp Rate",              CHOICE, 0,     5,    3,   "1/4|1/8|1/8T|1/16|1/16T|1/32", 1 },
        { "syn.arp.len",     "Arp Length",            INT,    1,    16,   16,   "steps", 1 },
        { "syn.arp.oct",     "Arp Octaves",           CHOICE, 0,     3,    1,   "1|2|3|4", 1 },
        { "syn.arp.gate",    "Arp Gate",              F,      5,   100,   60,   "%",  1 },
        { "syn.arp.swing",   "Arp Swing",             F,      0,    75,    0,   "%",  1 },

        { "syn.mix",         "Synth Mix",             F,      0,   100,   50,   "%",  1 },
        { "syn.level",       "Synth Level",           F,    -24,    12,    0,   "dB", 1 },
        { "syn.space",       "Synth Space",           F,      0,   100,   20,   "%",  1 },
        { "syn.char",        "Synth Character",       CHOICE, 0,     3,    1,   "Clean|Analog|8-bit|VHS", 1 },
        { "syn.post",        "Synth Post-Amp",        BOOL,   0,     1,    1,   "",   1 },
    };
    return d;
}

// ---- arpeggiator steps (stored with the rig as "synSteps", 16 numbers) ----------------------
// bit 0 on, 1 accent, 2 tie, 3-5 octave+1 (-1..+2), 6-7 ratchet-1 (1..4), 8-14 chance 0..100,
// 15-18 note (scale degree 0..14, used by the "Steps" pattern)
struct Step { bool on = false, acc = false, tie = false; int oct = 0, ratchet = 1, chance = 100, note = 0; };

inline Step unpackStep (unsigned v)
{
    Step s;
    s.on = (v & 1u) != 0; s.acc = (v & 2u) != 0; s.tie = (v & 4u) != 0;
    s.oct = (int) ((v >> 3) & 7u) - 1; if (s.oct > 2) s.oct = 2;
    s.ratchet = (int) ((v >> 6) & 3u) + 1;
    s.chance = (int) ((v >> 8) & 127u); if (s.chance > 100) s.chance = 100;
    s.note = (int) ((v >> 15) & 15u); if (s.note > 14) s.note = 14;
    return s;
}
inline unsigned packStep (const Step& s)
{
    return (s.on ? 1u : 0u) | (s.acc ? 2u : 0u) | (s.tie ? 4u : 0u)
         | ((unsigned) (s.oct + 1) & 7u) << 3 | ((unsigned) (s.ratchet - 1) & 3u) << 6
         | ((unsigned) s.chance & 127u) << 8 | ((unsigned) s.note & 15u) << 15;
}
// Default pattern: a driving 16th figure with accents on the beats.
inline unsigned defaultStep (int i)
{
    static const int on[16]  = { 1,0,1,1, 1,0,1,0, 1,0,1,1, 1,0,1,1 };
    static const int oct[16] = { 0,0,0,0, 0,0,1,0, 0,0,0,0, 0,0,1,0 };
    static const int note[16]= { 0,0,2,4, 7,0,4,0, 0,0,2,4, 7,0,9,4 };
    Step s; s.on = on[i] != 0; s.acc = (i % 4) == 0; s.oct = oct[i]; s.note = note[i] > 14 ? 14 : note[i];
    return packStep (s);
}
} // namespace obs::synthdefs
