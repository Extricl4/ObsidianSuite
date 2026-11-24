#pragma once
// =============================================================================
//  The SFX tab's GLITCH page: two units, one parameter table each.
//   KERAUNOS  (ker.*)  a multi-effect expression pedal: whammy, lasers, ring mod...
//                      moved by the treadle, an envelope, an LFO or each pick attack
//   KHAOS     (glt.*)  a tempo-synced glitch engine with 22 glitch types,
//                      random or a 16-step sequence
//  The page (ui/src/fxlab.js) mirrors these tables.
// =============================================================================
#include "SynthDefs.h"

namespace obs::fxdefs
{
using Def = obs::synthdefs::Def;
using obs::synthdefs::F; using obs::synthdefs::LOG; using obs::synthdefs::INT; using obs::synthdefs::BOOL; using obs::synthdefs::CHOICE;

// ---- KERAUNOS ------------------------------------------------------------------------------
namespace ker
{
enum P { on, algo, pos, mode, speed, sens, shape, color, mix, level, place, trig, count };
enum Algo { WhammyUp, WhammyDown, DiveBomb, FifthUp, OctFifth, Squeal, DetuneDrop, Laser, ZapComb, FreqShift, RingMod, RingTrack,
            Wah, TalkBox, Filter, BitCrush, kAlgos };
enum Mode { Pedal, Envelope, Auto, Trigger };
inline const Def* table()
{
    static const Def d[count] = {
        { "ker.on",    "Keraunos On",          BOOL,   0,   1,   0, "", 1 },
        { "ker.algo",  "Keraunos Effect",      CHOICE, 0,  15,   0, "Whammy Up|Whammy Down|Dive Bomb|Fifth Up|Oct + 5th|Squeal|Detune Drop|Laser|Zap Comb|Freq Shift|Ring Mod|Ring Tracking|Wah|Talk Box|Filter|Bit Crush", 1 },
        { "ker.pos",   "Keraunos Pedal",       F,      0, 100,   0, "%", 1 },
        { "ker.mode",  "Keraunos Mode",        CHOICE, 0,   3,   0, "Pedal|Envelope|Auto|Trigger", 1 },
        { "ker.speed", "Keraunos Speed",       F,      0, 100,  50, "%", 1 },
        { "ker.sens",  "Keraunos Sensitivity", F,      0, 100,  50, "%", 1 },
        { "ker.shape", "Keraunos Sweep",       CHOICE, 0,   3,   0, "Up|Down|Up and down|Random", 1 },
        { "ker.color", "Keraunos Color",       F,      0, 100,  50, "%", 1 },
        { "ker.mix",   "Keraunos Mix",         F,      0, 100, 100, "%", 1 },
        { "ker.level", "Keraunos Level",       F,    -12,  12,   0, "dB", 1 },
        { "ker.place", "Keraunos Place",       CHOICE, 0,   1,   0, "Before amp|After amp", 1 },
        { "ker.trig",  "Keraunos Trigger",     BOOL,   0,   1,   0, "", 1 },
    };
    return d;
}
}

// ---- KHAOS -------------------------------------------------------------------------------------
namespace glt
{
// 22 glitch types, in five groups (the page lays them out the same way)
enum Type { Stutter, Roll, Reverse, Scatter, Skip, Freeze,             // TIME
            Pitch, TapeStop, SpinUp, HalfSpeed, Warble,                  // PITCH
            Crush, Chop, Dropout, Data,                                  // DIGITAL
            Gsm, Radio, Hum, Static,                                     // INTERFERENCE
            Grains, Echo, Vinyl,                                         // TEXTURE
            kTypes };
inline const char* typeNames() { return "Stutter|Roll|Reverse|Scatter|Skip|Freeze|Pitch|Tape Stop|Spin Up|Half Speed|Warble|Crush|Chop|Dropout|Data|GSM|Radio|Hum|Static|Grains|Echo|Vinyl"; }
enum P { on, mode, grid, loop, chance, len, chaos, pitch, dir, filter, mix, level, hold,
         t0, s0 = t0 + kTypes, count = s0 + 16 };
enum Mode { Random, Sequence };
inline const Def* table()
{
    static Def d[count] = {
        { "glt.on",     "Khaos On",        BOOL,   0,   1,   0, "", 1 },
        { "glt.mode",   "Khaos Mode",      CHOICE, 0,   1,   0, "Random|Sequence", 1 },
        { "glt.grid",   "Khaos Grid",      CHOICE, 0,   5,   3, "1/4|1/8|1/8T|1/16|1/16T|1/32", 1 },
        { "glt.loop",   "Khaos Loop",      CHOICE, 0,   3,   1, "1/2 bar|1 bar|2 bars|4 bars", 1 },
        { "glt.chance", "Khaos Chance",    F,      0, 100,  35, "%", 1 },
        { "glt.len",    "Khaos Length",    CHOICE, 0,   3,   1, "1 step|2 steps|4 steps|8 steps", 1 },
        { "glt.chaos",  "Khaos Chaos",     F,      0, 100,  40, "%", 1 },
        { "glt.pitch",  "Khaos Pitch",     F,      0,  24,  12, "st", 1 },
        { "glt.dir",    "Khaos Direction", CHOICE, 0,   2,   2, "Up|Down|Both", 1 },
        { "glt.filter", "Khaos Filter",    F,   -100, 100,   0, "%", 1 },
        { "glt.mix",    "Khaos Mix",       F,      0, 100, 100, "%", 1 },
        { "glt.level",  "Khaos Level",     F,    -12,  12,   0, "dB", 1 },
        { "glt.hold",   "Khaos Hold",      BOOL,   0,   1,   0, "", 1 },
    };
    static bool filled = false;
    if (! filled)
    {
        filled = true;
        // which types RANDOM may pick (on by default: a tasteful handful)
        static const char* tid[kTypes]   = { "glt.t.stutter", "glt.t.roll", "glt.t.reverse", "glt.t.scatter", "glt.t.skip", "glt.t.freeze",
                                             "glt.t.pitch", "glt.t.tapestop", "glt.t.spinup", "glt.t.halfspeed", "glt.t.warble",
                                             "glt.t.crush", "glt.t.chop", "glt.t.dropout", "glt.t.data",
                                             "glt.t.gsm", "glt.t.radio", "glt.t.hum", "glt.t.static",
                                             "glt.t.grains", "glt.t.echo", "glt.t.vinyl" };
        static const char* tname[kTypes] = { "Khaos Stutter", "Khaos Roll", "Khaos Reverse", "Khaos Scatter", "Khaos Skip", "Khaos Freeze",
                                             "Khaos Pitch", "Khaos Tape Stop", "Khaos Spin Up", "Khaos Half Speed", "Khaos Warble",
                                             "Khaos Crush", "Khaos Chop", "Khaos Dropout", "Khaos Data",
                                             "Khaos GSM", "Khaos Radio", "Khaos Hum", "Khaos Static",
                                             "Khaos Grains", "Khaos Echo", "Khaos Vinyl" };
        static const bool defOn[kTypes]  = { true, true, true, false, false, false,  true, true, false, false, false,
                                             false, true, false, false,  false, false, false, false,  false, false, false };
        for (int i = 0; i < kTypes; ++i) d[t0 + i] = { tid[i], tname[i], BOOL, 0, 1, defOn[i] ? 1.f : 0.f, "", 1 };
        // the 16-step sequence: 0 = nothing, 1.. = a type
        static const char* sid[16] = { "glt.s1", "glt.s2", "glt.s3", "glt.s4", "glt.s5", "glt.s6", "glt.s7", "glt.s8",
                                       "glt.s9", "glt.s10", "glt.s11", "glt.s12", "glt.s13", "glt.s14", "glt.s15", "glt.s16" };
        static const char* sname[16] = { "Khaos Step 1", "Khaos Step 2", "Khaos Step 3", "Khaos Step 4", "Khaos Step 5", "Khaos Step 6", "Khaos Step 7", "Khaos Step 8",
                                         "Khaos Step 9", "Khaos Step 10", "Khaos Step 11", "Khaos Step 12", "Khaos Step 13", "Khaos Step 14", "Khaos Step 15", "Khaos Step 16" };
        static const char* stepChoices = "-|Stutter|Roll|Reverse|Scatter|Skip|Freeze|Pitch|Tape Stop|Spin Up|Half Speed|Warble|Crush|Chop|Dropout|Data|GSM|Radio|Hum|Static|Grains|Echo|Vinyl";
        static const int seqDef[16] = { 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 2, 0, 8, 0 };     // a stutter, a roll, a tape stop
        for (int i = 0; i < 16; ++i) d[s0 + i] = { sid[i], sname[i], CHOICE, 0, (float) kTypes, (float) seqDef[i], stepChoices, 1 };
    }
    return d;
}
}
} // namespace obs::fxdefs
