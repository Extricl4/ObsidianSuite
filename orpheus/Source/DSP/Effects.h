#pragma once
/*  ORPHEUS · SIREN's EFFECTS and output, the C++ copy of the preview (ui/src/sirenfx.js fxChain + synth.js audio()):
      DRIVE -> CRUSH -> RING -> CHORUS -> FLANGER -> PHASER -> DOUBLER -> DELAY -> REVERB -> SHIMMER (beside the REVERB)
      -> WIDTH -> SPACE (SIREN's stereo echo) + glue compressor -> safety limiter -> soft clip
    Every effect blends with the dry sound as in the preview; one that is off passes the sound untouched and costs nothing.
    DELAY, REVERB and SHIMMER ring out when switched off (not when ALL is switched off, or while a preset loads).       */
#include "Prims.h"
#include "Shimmer.h"
#include "ReverbIR.h"
#include "Dynamics.h"
#include "../Params.h"
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace orph
{
class FxChain
{
public:
    FxChain();
    ~FxChain();
    void prepare (double sampleRate, int maxBlock);
    void reset();
    /** while a preset loads: effects switched off by it stop at once (no old tails over the new sound) */
    void setNoTrails (bool b) noexcept { noTrails = b; }
    /** processes L / R in place (SIREN's voice in, the finished sound out) */
    void process (const Snapshot& S, double bpm, float* L, float* R, int n);
    /** for the tests: wait until the reverb's impulse is ready */
    bool irReady() const noexcept { return irDone.load() == irWanted.load(); }
    /** loads an impulse that is ready (the audio thread does this at the start of every block) */
    void pumpIR();
    /** for the tests: stop after WIDTH (no SPACE, compressor, limiter), like the preview's fx output */
    void setTestRaw (bool b) noexcept { testRaw = b; }

    struct Blk
    {
        Target dry, wet, pi; bool plugged = false, hasState = false; int wetOffIn = -1, unplugIn = -1;
        bool lastOn = false, lastMaster = false; float lastM = -1;
        double lead = 0, tail = .3; bool trails = false;
        void init() { dry.setNow (1); wet.setNow (0); pi.setNow (1); plugged = false; wetOffIn = unplugIn = -1; lastM = -1; }
        bool idle() const noexcept { return ! plugged && wet.v == 0.f && wet.target == 0.f && dry.v == 1.f && dry.target == 1.f && ! wet.pending; }
    };

private:
    enum { DRV, CRU, RING, CHO, FLA, PHA, DBL, DLY, REV, SHM, NB };
    void updateBlocks (const Snapshot& S, double bpm);
    void mixBlock (int id, bool isOn, float m, bool master);
    void blockTimers (int n);
    void clearState (int id);
    void wantIR (int ti, int sz, int dk, bool now);
    void irThread();

    double sr = 48000; int maxN = 512;
    Blk B[NB];
    bool noTrails = false, testRaw = false;
    std::vector<float> wl, wr, rl, rr, sl, sr2;        // scratch: a block's wet signal, the REVERB's input, the SHIMMER's output

    // DRIVE
    struct DriveCh { WABiquad tHp, tEm, fHp, fEm, dc, lp1, lp2, pres; Os2 o1, o2, f1, f2; } drv[2];
    Target dTubeG, dFuzzG, dPreT, dPreF, dOut, dLpF; bool dFuzz = false; float dLp2Mul = 1.1f;
    // CRUSH
    int cruK = 1, cruCnt = 0; float cruHold[2] {}; float cruQ = 32;
    // RING
    Target rFL, rFR; double rPhL = 0, rPhR = .25;
    // CHORUS
    struct ChoV { double ph; double rm; Target g; } cho[6];
    Target cRate; Delay cDl[2]; WABiquad cHp[2], cLp[2];
    // FLANGER
    Target flRate, flDep, flG, flComp; double flPh[2] { 0, .25 }; Delay flDl[2]; WABiquad flLp[2];
    // PHASER
    Target phRate, phDep, phFb; double phPh[2] { 0, .25 }; WABiquad phAp[2][4], phPk[2]; int phCnt = 0;
    // DOUBLER
    Delay dbDl[2]; WABiquad dbHp[2], dbLp[2]; double dbPh[4] {};
    // DELAY
    Delay dlD[2]; WABiquad dlHp[2], dlLp[2]; Os2 dlOs[2]; float dlOut[2] {};
    Target dlT0, dlT1, dlInj1, dlSelf, dlCross, dlPre, dlPost, dlWow, dlFlut, dlLpF, dlHpF; double dlWowPh[2] { 0, 110.0 / 360 }, dlFlPh = 0; bool dlTape = false;
    // REVERB
    WABiquad rvHp[2]; Delay rvPre; Target rvPreT, rvSum, rvMod; Delay rvWob[2]; double rvWobPh[2] { 0, .25 };
    juce::dsp::Convolution conv { juce::dsp::Convolution::NonUniform { 512 } };
    juce::AudioBuffer<float> convBuf;
    // SHIMMER
    Shimmer shm; WABiquad shHp[2]; int lastShmMode = -1;
    // WIDTH
    Target wSide, wSideHi; WABiquad wHp;
    // SPACE + output
    WABiquad spLp; Delay spL, spR; Target spT, spWet; float spOutL = 0, spOutR = 0;
    WACompressor comp, lim;

    // the impulse maker (a thread of its own: an impulse takes a few tens of ms)
    std::thread worker; std::mutex mx; std::condition_variable cv; bool quit = false;
    struct IRJob { int ti, sz, dk; double sec, dark, s01; int64_t due; int gen; } job {}; bool jobPending = false;
    std::string irKey; std::atomic<bool> irLoaded { false }; int64_t samplesDone = 0; bool revHasIR = false;
    juce::SpinLock irLock; juce::AudioBuffer<float> irPending; bool irPendingReady = false; int irPendingGen = 0;
    std::atomic<int> irWanted { 0 }, irDone { 0 };   // made by the thread, loaded by the audio thread
};
} // namespace orph
