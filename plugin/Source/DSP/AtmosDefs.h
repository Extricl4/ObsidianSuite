#pragma once
// =============================================================================
//  AETHER (SIREN's second page): noise, ambience and atmospheres. Its
//  parameters, in one table like SynthDefs.h: Params.cpp builds the plugin
//  parameters from it, the engine reads them by index (atmosdefs::P), and
//  ui/src/synth.js has the same list (the UI test checks that they match).
// =============================================================================
#include "SynthDefs.h"

namespace obs::atmosdefs
{
using Def = obs::synthdefs::Def;
using obs::synthdefs::F; using obs::synthdefs::LOG; using obs::synthdefs::INT; using obs::synthdefs::BOOL; using obs::synthdefs::CHOICE;

// Order matters: the engine reads values by this index.
enum P
{
    // the whole page
    on, level, follow, evolve, grit, tone, width, space, size, freeze,
    // BED: a bed of noise (wind, rain, surf...)
    bedOn, bedType, bedTone, bedMotion, bedDepth, bedLevel,
    // DRONE: tones that hang in the air
    drnOn, drnVoice, drnChord, drnOct, drnFollow, drnDetune, drnTone, drnMotion, drnLevel,
    // TEXTURE: things that happen now and then (drips, creaks, whispers, a heartbeat...)
    texOn, texType, texDensity, texTone, texChaos, texLevel,
    // added 2026-11-26 (at the end, so saved settings keep their places): the space's shimmer, the drone's shine and air
    shimmer, drnShine, drnAir,
    count
};

// The choices, so the engine and tests can name them.
// New ones go at the end of each list, so a saved choice keeps meaning the same thing.
enum Bed     { Hiss, Wind, Rain, Surf, Rumble, Static, Breath, Brook, Night, Forest, SoftRain, Underwater, Air, kBedTypes };
enum Voice   { Organ, Strings, Choir, Glass, Abyss, Descent, Ascent, PipeOrgan, Pad, Angels, Tanpura, Bowls, Harmonium, Flute, kVoices };
enum Chord   { Root, Fifth, Octaves, Minor, Sus, Tritone, Cluster, Dread, Major, Maj7, Add9, Sus2, Min7, Min9, Heaven, Open, kChords };
enum Texture { Vinyl, Fire, Drips, Metal, Shimmer, Whispers, Heartbeat, Knocks, Moans, Flesh, Chimes, Birds, MusicBox, Bells, Harp, Stars, Kalimba, Leaves, kTextures };

inline const Def* table()
{
    static const Def d[count] = {
        { "atm.on",          "Aether On",             BOOL,   0,     1,    0,   "",   1 },
        { "atm.level",       "Aether Level",          F,    -36,     6,   -6,   "dB", 1 },
        { "atm.follow",      "Aether Follow",         F,   -100,   100,    0,   "%",  1 },
        { "atm.evolve",      "Aether Evolve",         F,      0,   100,   35,   "%",  1 },
        { "atm.grit",        "Aether Grit",           F,      0,   100,    0,   "%",  1 },
        { "atm.tone",        "Aether Tone",           F,   -100,   100,    0,   "%",  1 },
        { "atm.width",       "Aether Width",          F,      0,   100,   80,   "%",  1 },
        { "atm.space",       "Aether Space",          F,      0,   100,   45,   "%",  1 },
        { "atm.size",        "Aether Size",           F,      0,   100,   60,   "%",  1 },
        { "atm.freeze",      "Aether Freeze",         BOOL,   0,     1,    0,   "",   1 },

        { "atm.bed.on",      "Bed On",                BOOL,   0,     1,    1,   "",   1 },
        { "atm.bed.type",    "Bed Type",              CHOICE, 0,    12,    1,   "Hiss|Wind|Rain|Surf|Rumble|Static|Breath|Brook|Night|Forest|Soft Rain|Underwater|Air", 1 },
        { "atm.bed.tone",    "Bed Tone",              F,      0,   100,   50,   "%",  1 },
        { "atm.bed.motion",  "Bed Motion",            F,      0,   100,   35,   "%",  1 },
        { "atm.bed.depth",   "Bed Depth",             F,      0,   100,   50,   "%",  1 },
        { "atm.bed.level",   "Bed Level",             F,      0,   100,   60,   "%",  1 },

        { "atm.drn.on",      "Drone On",              BOOL,   0,     1,    1,   "",   1 },
        { "atm.drn.voice",   "Drone Voice",           CHOICE, 0,    13,    1,   "Organ|Strings|Choir|Glass|Abyss|Descent|Ascent|Pipe Organ|Pad|Angels|Tanpura|Singing Bowls|Harmonium|Flute", 1 },
        { "atm.drn.chord",   "Drone Chord",           CHOICE, 0,    15,    1,   "Root|Fifth|Octaves|Minor|Sus|Tritone|Cluster|Dread|Major|Maj7|Add9|Sus2|Min7|Min9|Heaven|Open", 1 },
        { "atm.drn.oct",     "Drone Octave",          CHOICE, 0,     3,    1,   "Sub|Low|Mid|High", 1 },
        { "atm.drn.follow",  "Drone Follows Guitar",  BOOL,   0,     1,    0,   "",   1 },
        { "atm.drn.detune",  "Drone Detune",          F,      0,   100,   25,   "%",  1 },
        { "atm.drn.tone",    "Drone Tone",            F,      0,   100,   45,   "%",  1 },
        { "atm.drn.motion",  "Drone Motion",          F,      0,   100,   30,   "%",  1 },
        { "atm.drn.level",   "Drone Level",           F,      0,   100,   35,   "%",  1 },

        { "atm.tex.on",      "Texture On",            BOOL,   0,     1,    0,   "",   1 },
        { "atm.tex.type",    "Texture Type",          CHOICE, 0,    17,    1,   "Vinyl|Fire|Drips|Metal|Shimmer|Whispers|Heartbeat|Knocks|Moans|Flesh|Wind Chimes|Birds|Music Box|Bells|Harp|Stars|Kalimba|Leaves", 1 },
        { "atm.tex.density", "Texture Density",       F,      0,   100,   40,   "%",  1 },
        { "atm.tex.tone",    "Texture Tone",          F,      0,   100,   50,   "%",  1 },
        { "atm.tex.chaos",   "Texture Chaos",         F,      0,   100,   40,   "%",  1 },
        { "atm.tex.level",   "Texture Level",         F,      0,   100,   50,   "%",  1 },

        { "atm.shimmer",     "Aether Shimmer",        F,      0,   100,    0,   "%",  1 },
        { "atm.drn.shine",   "Drone Shine",           F,      0,   100,    0,   "%",  1 },
        { "atm.drn.air",     "Drone Air",             F,      0,   100,    0,   "%",  1 },
    };
    return d;
}
} // namespace obs::atmosdefs
