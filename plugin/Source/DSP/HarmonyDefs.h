#pragma once
// =============================================================================
//  HYDRA (the HARMONY page): its parameters and its 45 modes, in one place.
//  Params.cpp builds the plugin parameters from these tables and the engine
//  reads them by index. ui/src/harmony.js has the same modes in the same order
//  and the same smart-harmony rule (the UI test checks that they match).
//  No JUCE in here: the harmonizer's offline tests build without it.
// =============================================================================

namespace obs::harmdefs
{
enum Type { F, INT, BOOL, CHOICE };

struct Def
{
    const char* id; const char* name; Type type;
    float min, max, def;
    const char* unit;       // or the choices, separated by '|', for CHOICE (nullptr: modeChoices())
};

constexpr int kVoices = 6;

// Order matters: the engine reads values by these indexes.
enum G { on, key, mode, track, place, offkey, human, spread, mix, out, gCount };
enum V { vOn, vSmart, vStep, vSemi, vLevel, vPan, vDetune, vDelay, vCount };

struct Mode { const char* group; const char* name; int n; int steps[12]; };
constexpr int kModes = 45;

inline const Mode* modes()
{
    static const Mode m[kModes] = {
        { "Common", "Major (Ionian)", 7, { 0, 2, 4, 5, 7, 9, 11 } },
        { "Common", "Natural minor (Aeolian)", 7, { 0, 2, 3, 5, 7, 8, 10 } },
        { "Common", "Harmonic minor", 7, { 0, 2, 3, 5, 7, 8, 11 } },
        { "Common", "Melodic minor", 7, { 0, 2, 3, 5, 7, 9, 11 } },
        { "Church modes", "Dorian", 7, { 0, 2, 3, 5, 7, 9, 10 } },
        { "Church modes", "Phrygian", 7, { 0, 1, 3, 5, 7, 8, 10 } },
        { "Church modes", "Lydian", 7, { 0, 2, 4, 6, 7, 9, 11 } },
        { "Church modes", "Mixolydian", 7, { 0, 2, 4, 5, 7, 9, 10 } },
        { "Church modes", "Locrian", 7, { 0, 1, 3, 5, 6, 8, 10 } },
        { "Pentatonic and blues", "Major pentatonic", 5, { 0, 2, 4, 7, 9 } },
        { "Pentatonic and blues", "Minor pentatonic", 5, { 0, 3, 5, 7, 10 } },
        { "Pentatonic and blues", "Blues", 6, { 0, 3, 5, 6, 7, 10 } },
        { "Pentatonic and blues", "Major blues", 6, { 0, 2, 3, 4, 7, 9 } },
        { "Pentatonic and blues", "Dominant pentatonic", 5, { 0, 2, 4, 7, 10 } },
        { "Metal and dark", "Phrygian dominant", 7, { 0, 1, 4, 5, 7, 8, 10 } },
        { "Metal and dark", "Hungarian minor", 7, { 0, 2, 3, 6, 7, 8, 11 } },
        { "Metal and dark", "Double harmonic (Byzantine)", 7, { 0, 1, 4, 5, 7, 8, 11 } },
        { "Metal and dark", "Neapolitan minor", 7, { 0, 1, 3, 5, 7, 8, 11 } },
        { "Metal and dark", "Neapolitan major", 7, { 0, 1, 3, 5, 7, 9, 11 } },
        { "Metal and dark", "Locrian nat6", 7, { 0, 1, 3, 5, 6, 9, 10 } },
        { "Metal and dark", "Super Locrian (altered)", 7, { 0, 1, 3, 4, 6, 8, 10 } },
        { "Metal and dark", "Enigmatic", 7, { 0, 1, 4, 6, 8, 10, 11 } },
        { "Symmetric", "Diminished (whole-half)", 8, { 0, 2, 3, 5, 6, 8, 9, 11 } },
        { "Symmetric", "Diminished (half-whole)", 8, { 0, 1, 3, 4, 6, 7, 9, 10 } },
        { "Symmetric", "Whole tone", 6, { 0, 2, 4, 6, 8, 10 } },
        { "Symmetric", "Augmented", 6, { 0, 3, 4, 7, 8, 11 } },
        { "Symmetric", "Chromatic", 12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
        { "Melodic minor family", "Dorian b2", 7, { 0, 1, 3, 5, 7, 9, 10 } },
        { "Melodic minor family", "Lydian augmented", 7, { 0, 2, 4, 6, 8, 9, 11 } },
        { "Melodic minor family", "Lydian dominant", 7, { 0, 2, 4, 6, 7, 9, 10 } },
        { "Melodic minor family", "Mixolydian b6", 7, { 0, 2, 4, 5, 7, 8, 10 } },
        { "Melodic minor family", "Locrian nat2", 7, { 0, 2, 3, 5, 6, 8, 10 } },
        { "Harmonic minor family", "Ionian #5", 7, { 0, 2, 4, 5, 8, 9, 11 } },
        { "Harmonic minor family", "Ukrainian Dorian", 7, { 0, 2, 3, 6, 7, 9, 10 } },
        { "Harmonic minor family", "Lydian #2", 7, { 0, 3, 4, 6, 7, 9, 11 } },
        { "World", "Hirajoshi", 5, { 0, 2, 3, 7, 8 } },
        { "World", "In Sen", 5, { 0, 1, 5, 7, 10 } },
        { "World", "Iwato", 5, { 0, 1, 5, 6, 10 } },
        { "World", "Kumoi", 5, { 0, 2, 3, 7, 9 } },
        { "World", "Persian", 7, { 0, 1, 4, 5, 6, 8, 11 } },
        { "World", "Egyptian", 5, { 0, 2, 5, 7, 10 } },
        { "World", "Gypsy", 7, { 0, 2, 3, 6, 7, 8, 10 } },
        { "World", "Prometheus", 6, { 0, 2, 4, 6, 9, 10 } },
        { "Jazz", "Bebop dominant", 8, { 0, 2, 4, 5, 7, 9, 10, 11 } },
        { "Jazz", "Bebop major", 8, { 0, 2, 4, 5, 7, 8, 9, 11 } },
    };
    return m;
}

/** "Major (Ionian)|Natural minor (Aeolian)|..." for the mode parameter (plain ASCII). */
inline const char* modeChoices()
{
    static char buf[2048] = {};
    if (buf[0] == 0)
    {
        int p = 0;
        for (int i = 0; i < kModes; ++i)
        {
            if (i) buf[p++] = '|';
            for (const char* c = modes()[i].name; *c && p < 2040; ++c) buf[p++] = *c;
        }
        buf[p] = 0;
    }
    return buf;
}

inline const Def* globals()
{
    static const Def d[gCount] = {
        { "harm.on",     "Harmony On",       BOOL,     0,   1,  0, "" },
        { "harm.key",    "Harmony Key",      CHOICE,   0,  11,  4, "C|C#|D|D#|E|F|F#|G|G#|A|A#|B" },
        { "harm.mode",   "Harmony Mode",     CHOICE,   0, kModes - 1, 1, nullptr },
        { "harm.track",  "Harmony Tracking", CHOICE,   0,   1,  0, "Lead|Chords" },
        { "harm.place",  "Harmony Place",    CHOICE,   0,   1,  0, "After amp|Before amp" },
        { "harm.offkey", "Harmony Off-Key",  CHOICE,   0,   1,  0, "Follow|Hold" },
        { "harm.human",  "Harmony Humanize", F,        0, 100, 15, "%" },
        { "harm.spread", "Harmony Spread",   F,        0, 100, 50, "%" },
        { "harm.mix",    "Harmony Mix",      F,        0, 100, 50, "%" },
        { "harm.out",    "Harmony Output",   F,      -24,  12,  0, "dB" },
    };
    return d;
}

/** Per-voice parameters: ids "harm.v1.on" ... "harm.v6.delay", names "Harmony Voice 1 On" ... */
inline const Def* voiceDefs()
{
    static const Def d[vCount] = {
        { "on",     "On",        BOOL,    0,   1,  0, "" },
        { "smart",  "Smart",     BOOL,    0,   1,  1, "" },
        { "step",   "Step",      INT,   -14,  14,  2, "" },
        { "semi",   "Semitones", INT,   -24,  24,  4, "st" },
        { "level",  "Level",     F,       0, 100, 70, "%" },
        { "pan",    "Pan",       F,    -100, 100,  0, "" },
        { "detune", "Detune",    F,     -50,  50,  0, "ct" },
        { "delay",  "Delay",     F,       0, 120,  0, "ms" },
    };
    return d;
}

/** The starting setup of each voice (the HARMONY page's defaults: a third and a fifth above). */
inline float voiceDefault (int voice, int p)
{
    static const float d[kVoices][vCount] = {
        //  on smart step semi level  pan  detune delay
        {   1,  1,    2,   4,   80,  -35,    0,    0 },
        {   1,  1,    4,   7,   65,   35,    0,    0 },
        {   0,  0,   -7, -12,   60,    0,    0,    0 },
        {   0,  1,    5,   9,   60,  -60,    6,   12 },
        {   0,  1,    7,  12,   50,   60,   -6,   18 },
        {   0,  1,   -2,  -3,   60,    0,    0,    0 },
    };
    return d[voice < 0 ? 0 : (voice >= kVoices ? kVoices - 1 : voice)][p];
}

inline int floorDiv (int a, int b)  { return a >= 0 ? a / b : -((-a + b - 1) / b); }

/** The smart harmony (the same rule as harmony() in ui/src/harmony.js): how many semitones a voice
    `steps` scale steps away moves from MIDI note m in the key and mode. A note outside the mode
    snaps to the nearest scale note for the count (FOLLOW) or reuses `lastShift` (HOLD). */
inline int smartShift (int m, int steps, int key, int mode, bool hold, int lastShift)
{
    const auto& md = modes()[mode < 0 ? 0 : (mode >= kModes ? kModes - 1 : mode)];
    const int n = md.n, rel = ((m - key) % 12 + 12) % 12;
    int i = -1;
    for (int k = 0; k < n; ++k) if (md.steps[k] == rel) { i = k; break; }
    if (i < 0)
    {
        if (hold) return lastShift;
        int bd = 99;
        for (int k = 0; k < n; ++k)
        {
            const int a = md.steps[k] > rel ? md.steps[k] - rel : rel - md.steps[k];
            const int dd = a < 12 - a ? a : 12 - a;
            if (dd < bd) { bd = dd; i = k; }
        }
    }
    const int t = i + steps, o = floorDiv (t, n), idx = ((t % n) + n) % n;
    return md.steps[idx] + 12 * o - md.steps[i];
}
} // namespace obs::harmdefs
