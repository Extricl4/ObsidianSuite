#include "Effects.h"
#include <chrono>

namespace orph
{
static inline double v01 (const Snapshot& S, int i) { return S[i] / 100.0; }
static inline double sinT (double ph) { return std::sin (kTau * ph); }
/* the preview's triangle LFO: 15 harmonics (8 odd ones), peak 1 */
static inline double triLfo (double ph)
{
    double s = 0;
    for (int n = 1, k = 0; n <= 15; n += 2, ++k) s += (k & 1 ? -1.0 : 1.0) / (n * n) * std::sin (kTau * n * ph);
    return s * 8.0 / (kPi * kPi);
}
static inline float sat1 (float x) { return clampf (x, -1.f, 1.f); }

FxChain::FxChain() { worker = std::thread ([this] { irThread(); }); }
FxChain::~FxChain()
{
    { std::lock_guard<std::mutex> l (mx); quit = true; }
    cv.notify_all(); if (worker.joinable()) worker.join();
}

void FxChain::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate; maxN = std::max (32, maxBlock);
    for (auto* v : { &wl, &wr, &rl, &rr, &sl, &sr2 }) v->assign ((size_t) maxN, 0.f);
    for (auto& b : B) b.init();
    B[CHO].lead = .022; B[FLA].lead = .08; B[DBL].lead = .035;
    for (int i : { DLY, REV }) { B[i].trails = true; B[i].tail = 12; }
    B[SHM].trails = true; B[SHM].tail = 20;
    auto init = [this] (WABiquad& b) { b.sr = sr; b.reset(); };
    for (auto& c : drv)
    {
        for (auto* b : { &c.tHp, &c.tEm, &c.fHp, &c.fEm, &c.dc, &c.lp1, &c.lp2, &c.pres }) init (*b);
        c.tHp.setPlain (WABiquad::HP, 70, .6); c.tEm.setPlain (WABiquad::PEAK, 900, .7, 2);
        c.fHp.setPlain (WABiquad::HP, 150, .6); c.fEm.setPlain (WABiquad::PEAK, 1100, .8, 6);
        c.dc.setPlain (WABiquad::HP, 22, .6); c.pres.setPlain (WABiquad::PEAK, 3200, .9, 1.5);
        c.o1.init (sr); c.o2.init (sr * 2); c.f1.init (sr); c.f2.init (sr * 2);
    }
    dTubeG.setNow (1); dFuzzG.setNow (0); dPreT.setNow (1); dPreF.setNow (4); dOut.setNow (.3f); dLpF.setNow (4000);
    rFL.setNow (220); rFR.setNow (220);
    const double cdt[3] = { .0072, .0114, .0157 }, crm[3] = { 1, 1.09, .92 };
    for (int c = 0; c < 2; ++c)
    {
        cDl[c].init (.05, sr); init (cHp[c]); init (cLp[c]); cHp[c].setPlain (WABiquad::HP, 160, .55); cLp[c].setPlain (WABiquad::LP, 9500, .6);
        for (int j = 0; j < 3; ++j) { auto& v = cho[c * 3 + j]; v.ph = (j * 120 + c * 60) / 360.0; v.rm = crm[j]; v.g.setNow (0); (void) cdt; }
        flDl[c].init (.1, sr); init (flLp[c]); flLp[c].setPlain (WABiquad::LP, 9000, .6);
        for (auto& a : phAp[c]) init (a);
        init (phPk[c]);
        dbDl[c].init (.1, sr); init (dbHp[c]); init (dbLp[c]);
        dlD[c].init (4.6, sr); init (dlHp[c]); init (dlLp[c]); dlOs[c].init (sr);
        init (rvHp[c]); rvHp[c].setPlain (WABiquad::HP, 120, .6); rvWob[c].init (.05, sr);
        init (shHp[c]); shHp[c].setPlain (WABiquad::HP, 160, .6);
    }
    dbHp[0].setPlain (WABiquad::HP, 165, .6); dbLp[0].setPlain (WABiquad::LP, 9800, .6);
    dbHp[1].setPlain (WABiquad::HP, 150, .6); dbLp[1].setPlain (WABiquad::LP, 8800, .6);
    dbPh[0] = 0; dbPh[1] = 0; dbPh[2] = 70.0 / 360; dbPh[3] = 140.0 / 360;
    cRate.setNow (.8f); flRate.setNow (.15f); flDep.setNow (0); flG.setNow (0); flComp.setNow (.78f);
    phRate.setNow (.3f); phDep.setNow (0); phFb.setNow (0);
    dlT0.setNow (.38f); dlT1.setNow (.38f); dlInj1.setNow (0); dlSelf.setNow (0); dlCross.setNow (0); dlPre.setNow (.55f / 4); dlPost.setNow (1 / (1.3f * .55f));
    dlWow.setNow (.00012f); dlFlut.setNow (.000015f); dlLpF.setNow (5000); dlHpF.setNow (110);
    rvPre.init (.5, sr); rvPreT.setNow (0); rvSum.setNow (1); rvMod.setNow ((float) ReverbIR::typeMod (1));
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxN, 2 };
    conv.prepare (spec); convBuf.setSize (2, maxN);
    shm.prepare (sr);
    wSide.setNow (1); wSideHi.setNow (0); init (wHp); wHp.setPlain (WABiquad::HP, 150, .6);
    init (spLp); spLp.set (WABiquad::LP, 3500, 1); spL.init (2, sr); spR.init (2, sr); spT.setNow (.375f); spWet.setNow (0);
    comp.set (sr, -12, 30, 3, .003, .25);
    lim.set (sr, -2, 0, 20, .002, .12);
    irKey.clear(); revHasIR = false; samplesDone = 0;
}

void FxChain::reset()
{
    for (int i = 0; i < NB; ++i) clearState (i);
    spL.clear(); spR.clear(); spLp.reset(); spOutL = spOutR = 0; comp.reset(); lim.reset();
}

void FxChain::clearState (int id)
{
    switch (id)
    {
        case DRV: for (auto& c : drv) for (auto* b : { &c.tHp, &c.tEm, &c.fHp, &c.fEm, &c.dc, &c.lp1, &c.lp2, &c.pres }) b->reset(); break;
        case CRU: cruHold[0] = cruHold[1] = 0; cruCnt = 0; break;
        case CHO: for (int c = 0; c < 2; ++c) { cDl[c].clear(); cHp[c].reset(); cLp[c].reset(); } break;
        case FLA: for (int c = 0; c < 2; ++c) { flDl[c].clear(); flLp[c].reset(); } break;
        case PHA: for (int c = 0; c < 2; ++c) { for (auto& a : phAp[c]) a.reset(); phPk[c].reset(); } break;
        case DBL: for (int c = 0; c < 2; ++c) { dbDl[c].clear(); dbHp[c].reset(); dbLp[c].reset(); } break;
        case DLY: for (int c = 0; c < 2; ++c) { dlD[c].clear(); dlHp[c].reset(); dlLp[c].reset(); dlOut[c] = 0; } break;
        case REV: for (int c = 0; c < 2; ++c) { rvHp[c].reset(); rvWob[c].clear(); } rvPre.clear(); conv.reset(); break;
        case SHM: shm.reset(); for (auto& h : shHp) h.reset(); break;
        default: break;
    }
}

/* ------------------------------------------------------------ a block's dry / wet / listening (sirenfx.js block().mix) */
void FxChain::mixBlock (int id, bool isOn, float m, bool master)
{
    auto& b = B[id];
    float d = 1, w = 0;
    if (isOn)
        switch (id)
        {
            case RING: d = (float) std::cos (m * kPi / 2); w = (float) std::sin (m * kPi / 2); break;
            case CHO: d = 1 - m * .4f; w = m * .9f; break;
            case DBL: d = .74f; w = .76f; break;
            case DLY: d = 1; w = m; break;
            case REV: d = 1 - m * .3f; w = m * .82f; break;
            case SHM: d = 1 - m * .2f; w = m * .95f; break;
            default: d = 1 - m; w = m; break;                  // DRIVE, CRUSH, FLANGER, PHASER: a crossfade
        }
    b.dry.approach (d, .03, sr);
    b.wetOffIn = b.unplugIn = -1;
    if (! isOn && b.trails && master && ! noTrails)
    {
        b.pi.approach (0, .015, sr);                           // stops listening; what's in it rings out
        if (b.plugged) b.wetOffIn = (int) (b.tail * sr);
        return;
    }
    if (isOn) b.pi.approach (1, .01, sr);
    if (isOn && ! b.plugged && b.lead > 0) b.wet.approach (w, .03, sr, b.lead);   // its delay lines fill first
    else b.wet.approach (w, .03, sr);
    if (isOn && ! b.plugged)
    {
        if (b.hasState) clearState (id);
        b.plugged = true; b.hasState = true;
    }
    if (! isOn && b.plugged) b.unplugIn = (int) (.3 * sr);
}

void FxChain::blockTimers (int n)
{
    for (auto& b : B)
    {
        if (b.wetOffIn >= 0 && (b.wetOffIn -= n) < 0) { b.wetOffIn = -1; b.wet.approach (0, .05, sr); b.unplugIn = (int) (.3 * sr); }
        if (b.unplugIn >= 0 && (b.unplugIn -= n) < 0) { b.unplugIn = -1; b.plugged = false; b.wet.setNow (0); }
    }
}

/* ------------------------------------------------------------ the settings (each effect's update()) */
void FxChain::updateBlocks (const Snapshot& S, double bpm)
{
    const bool master = S.on (P_fx_on);
    struct { int id, on, mix; } map[NB] = {
        { DRV, P_fx_drv_on, P_fx_drv_mix }, { CRU, P_fx_cru_on, P_fx_cru_mix }, { RING, P_fx_ring_on, P_fx_ring_mix },
        { CHO, P_fx_cho_on, P_fx_cho_mix }, { FLA, P_fx_fla_on, P_fx_fla_mix }, { PHA, P_fx_pha_on, P_fx_pha_mix },
        { DBL, P_fx_dbl, -1 }, { DLY, P_fx_dly_on, P_fx_dly_mix }, { REV, P_fx_rev_on, P_fx_rev_mix }, { SHM, P_fx_shm_on, P_fx_shm_mix } };
    for (auto& e : map)
    {
        const bool isOn = e.id == DBL ? S.on (P_fx_dbl) : (master && S.on (e.on));
        const float m = e.mix < 0 ? 1.f : (float) v01 (S, e.mix);
        auto& b = B[e.id];
        if (isOn != b.lastOn || m != b.lastM || (e.id != DBL && master != b.lastMaster))
        {
            b.lastOn = isOn; b.lastM = m; b.lastMaster = master;
            mixBlock (e.id, isOn, m, master);
        }
    }
    // DRIVE
    {
        const double a = v01 (S, P_fx_drv_amt); const bool fz = S.ch (P_fx_drv_mode) == 1; const double f = 1600 * std::pow (2.0, v01 (S, P_fx_drv_tone) * 2.9);
        static const double DT[5] = { .96, .355, .168, .128, .116 }, DF[5] = { .098, .09, .089, .089, .089 };
        auto lerpT = [] (const double* tb, double x01) { const double x = x01 * 4; const int i = std::min (3, (int) std::floor (x)); const double u = x - i; return tb[i] * std::pow (tb[i + 1] / tb[i], u); };
        dPreT.approach ((float) (1 + a * a * 30), .03, sr); dPreF.approach ((float) (4 + a * a * 140), .03, sr);
        dTubeG.approach (fz ? 0.f : 1.f, .02, sr); dFuzzG.approach (fz ? 1.f : 0.f, .02, sr);
        dLpF.approach ((float) f, .03, sr); dLp2Mul = fz ? .85f : 1.1f; dFuzz = fz;
        dOut.approach ((float) lerpT (fz ? DF : DT, a), .03, sr);
    }
    // CRUSH
    cruK = std::max (1, (int) std::lround (sr / std::max (1.f, S[P_fx_cru_rate])));
    cruQ = (float) std::pow (2.0, S.ch (P_fx_cru_bits) - 1);
    // RING
    rFL.approach (S[P_fx_ring_freq] * .997f, .05, sr); rFR.approach (S[P_fx_ring_freq] * 1.003f, .05, sr);
    // CHORUS
    {
        const double r = S[P_fx_cho_rate], cents = v01 (S, P_fx_cho_depth) * 24;
        cRate.approach ((float) r, .05, sr);
        for (auto& v : cho) { const double f = r * v.rm; v.g.approach ((float) std::min (.0055, cents * .000578 / (kTau * f)), .03, sr); }
    }
    // FLANGER
    {
        const double g = S[P_fx_fla_fb] / 100.0 * .9;
        flRate.approach (S[P_fx_fla_rate], .05, sr); flDep.approach ((float) (v01 (S, P_fx_fla_depth) * 2.4 / 4), .03, sr);
        flG.approach ((float) g, .03, sr); flComp.approach ((float) (.78 * (1 - g * .45)), .03, sr);
    }
    // PHASER
    phRate.approach (S[P_fx_pha_rate], .05, sr); phDep.approach ((float) (v01 (S, P_fx_pha_depth) * 3000), .03, sr); phFb.approach (S[P_fx_pha_fb] / 100.f, .03, sr);
    // DELAY
    {
        dlTape = S.ch (P_fx_dly_mode) == 1;
        static const double DIV[8] = { .25, 1.0 / 3, .5, .75, 2.0 / 3, 1, 1.5, 2 };
        const double t = S.on (P_fx_dly_sync) ? 60.0 / std::max (20.0, bpm) * DIV[std::max (0, std::min (7, S.ch (P_fx_dly_div)))] : S[P_fx_dly_time] / 1000.0;
        const double fb = S[P_fx_dly_fb] / 100.0, f = 1000 * std::pow (2.0, v01 (S, P_fx_dly_tone) * 3.9) * (dlTape ? .62 : 1), drive = dlTape ? 1.7 : .55;
        dlT0.approach ((float) t, .08, sr); dlT1.approach ((float) (t + (dlTape ? .009 : 0)), .08, sr);
        dlInj1.approach (dlTape ? 1.f : 0.f, .02, sr); dlSelf.approach ((float) (dlTape ? fb : 0), .02, sr); dlCross.approach ((float) (dlTape ? 0 : fb), .02, sr);
        dlLpF.approach ((float) f, .03, sr); dlHpF.approach (dlTape ? 190.f : 110.f, .03, sr);
        dlPre.approach ((float) (drive / 4), .03, sr); dlPost.approach ((float) (1 / (1.3 * drive)), .03, sr);
        dlWow.approach (dlTape ? .00075f : .00012f, .1, sr); dlFlut.approach (dlTape ? .00006f : .000015f, .1, sr);
    }
    // REVERB
    {
        const int ti = std::max (0, std::min (3, S.ch (P_fx_rev_type))); const int sz = (int) std::lround (S[P_fx_rev_size]), dk = (int) std::lround (S[P_fx_rev_damp]);
        static const float SUM[4] = { 1, 1, 1.22f, .94f };
        rvPreT.approach ((float) std::max (0.0, S[P_fx_rev_pre] / 1000.0 - .0067), .05, sr); rvMod.approach ((float) ReverbIR::typeMod (ti), .1, sr);
        rvSum.approach (SUM[ti], .03, sr);
        if (B[REV].plugged || revHasIR) wantIR (ti, sz, dk, ! revHasIR);
    }
    // SHIMMER
    shm.setParams (ReverbIR::secOf (std::lround (S[P_fx_shm_decay])), v01 (S, P_fx_shm_amt), S.ch (P_fx_shm_mode));
    // WIDTH
    { const double w = S[P_fx_width] / 100.0; wSide.approach ((float) std::min (1.0, w), .03, sr); wSideHi.approach ((float) std::max (0.0, w - 1), .03, sr); }
    // SPACE
    spT.approach ((float) (60.0 / std::max (20.0, bpm) * .75), .05, sr); spWet.approach (S[P_space] / 100.f * .75f, .05, sr);
}

/* ------------------------------------------------------------ the REVERB's impulses, made on a thread of their own */
static int64_t nowMs() { return std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::steady_clock::now().time_since_epoch()).count(); }
void FxChain::wantIR (int ti, int sz, int dk, bool now)
{
    const std::string key = std::to_string (ti) + ":" + std::to_string (sz) + ":" + std::to_string (dk);
    if (key == irKey) return;
    irKey = key; revHasIR = true;
    const int gen = ++irWanted;
    { std::lock_guard<std::mutex> l (mx); job = { ti, sz, dk, ReverbIR::secOf (sz), dk / 100.0, sz / 100.0, nowMs() + (now ? 0 : 120), gen }; jobPending = true; }
    cv.notify_all();
}
void FxChain::irThread()
{
    for (;;)
    {
        IRJob j;
        {
            std::unique_lock<std::mutex> l (mx);
            cv.wait (l, [this] { return quit || jobPending; });
            if (quit) return;
            j = job;
            if (nowMs() < j.due) { cv.wait_for (l, std::chrono::milliseconds (j.due - nowMs())); continue; }   // (a newer one may come)
            jobPending = false;
        }
        ReverbIR ir; ir.make (sr, j.sec, j.dark, j.ti, j.s01);
        juce::AudioBuffer<float> buf (2, (int) ir.L.size());
        buf.copyFrom (0, 0, ir.L.data(), (int) ir.L.size()); buf.copyFrom (1, 0, ir.R.data(), (int) ir.R.size());
        { const juce::SpinLock::ScopedLockType l (irLock); irPending = std::move (buf); irPendingReady = true; irPendingGen = j.gen; }   // (JUCE: load from the audio thread)
    }
}

void FxChain::pumpIR()
{
    const juce::SpinLock::ScopedTryLockType l (irLock);
    if (l.isLocked() && irPendingReady)
    {
        irPendingReady = false;
        conv.loadImpulseResponse (std::move (irPending), sr, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
        irLoaded = true; irDone = irPendingGen;
    }
}

/* ------------------------------------------------------------ sound */
void FxChain::process (const Snapshot& S, double bpm, float* L, float* R, int total)
{
    for (int off = 0; off < total; off += maxN)
    {
        const int n = std::min (maxN, total - off);
        float* l = L + off; float* r = R + off;
        updateBlocks (S, bpm);
        pumpIR();
        auto runBlock = [&] (int id, auto&& fx)          // dry * in + wet * fx(in * pi)
        {
            auto& b = B[id];
            if (b.idle()) return;
            if (b.plugged)
            {
                Target pi = b.pi;
                for (int s = 0; s < n; ++s) { const float g = pi.next(); wl[(size_t) s] = l[s] * g; wr[(size_t) s] = r[s] * g; }
                b.pi = pi;
                fx (wl.data(), wr.data());
            }
            else { std::fill (wl.begin(), wl.begin() + n, 0.f); std::fill (wr.begin(), wr.begin() + n, 0.f); }
            for (int s = 0; s < n; ++s) { const float d = b.dry.next(), w = b.wet.next(); l[s] = l[s] * d + wl[(size_t) s] * w; r[s] = r[s] * d + wr[(size_t) s] * w; }
        };

        // DRIVE
        runBlock (DRV, [&] (float* a, float* c)
        {
            for (int s = 0; s < n; ++s)
            {
                const float gt = dTubeG.next(), gf = dFuzzG.next(), pt = dPreT.next(), pf = dPreF.next(), go = dOut.next(), fq = dLpF.next();
                if ((s & 15) == 0) for (auto& ch : drv) { ch.lp1.setPlain (WABiquad::LP, fq, .62); ch.lp2.setPlain (WABiquad::LP, fq * dLp2Mul, .54); }
                for (int c2 = 0; c2 < 2; ++c2)
                {
                    float* x = c2 ? c : a; auto& ch = drv[c2]; float y = 0;
                    if (gt > 1e-4f)
                    {
                        const float u = ch.tEm.process (ch.tHp.process (x[s])) * pt;
                        y += gt * ch.o1.run (u, [&] (float v) { return ch.o2.run (v, [] (float q) { return (float) (std::tanh (clampf (q, -32.f, 32.f) + .25) - std::tanh (.25)); }); });
                    }
                    if (gf > 1e-4f)
                    {
                        const float u = ch.fEm.process (ch.fHp.process (x[s])) * pf;
                        y += gf * ch.f1.run (u, [&] (float v) { return ch.f2.run (v, [] (float q) {
                            const double xx = clampf (q, -32.f, 32.f); const double z = xx >= 0 ? .82 * std::tanh (2.5 * xx) : std::tanh (4 * xx);
                            return (float) (std::tanh (2.2 * z) / std::tanh (2.2)); }); });
                    }
                    x[s] = ch.pres.process (ch.lp2.process (ch.lp1.process (ch.dc.process (y)))) * go;
                }
            }
        });
        // CRUSH
        runBlock (CRU, [&] (float* a, float* c)
        {
            for (int s = 0; s < n; ++s)
            {
                if (cruCnt <= 0) { cruHold[0] = a[s]; cruHold[1] = c[s]; cruCnt = cruK; }
                --cruCnt;
                a[s] = std::round (sat1 (cruHold[0] * 2.6f) * cruQ) / cruQ / 2.6f;
                c[s] = std::round (sat1 (cruHold[1] * 2.6f) * cruQ) / cruQ / 2.6f;
            }
        });
        // RING
        runBlock (RING, [&] (float* a, float* c)
        {
            for (int s = 0; s < n; ++s)
            {
                a[s] *= (float) (sinT (rPhL) * 1.45); c[s] *= (float) (sinT (rPhR) * 1.45);
                rPhL += rFL.next() / sr; rPhR += rFR.next() / sr; rPhL -= std::floor (rPhL); rPhR -= std::floor (rPhR);
            }
        });
        if (B[RING].idle()) { rFL.v = rFL.target; rFR.v = rFR.target; }
        // CHORUS
        runBlock (CHO, [&] (float* a, float* c)
        {
            static const double DT[3] = { .0072, .0114, .0157 };
            for (int s = 0; s < n; ++s)
            {
                const double rate = cRate.next();
                for (int c2 = 0; c2 < 2; ++c2)
                {
                    float* x = c2 ? c : a; cDl[c2].push (x[s]); double sum = 0;
                    for (int j = 0; j < 3; ++j)
                    {
                        auto& v = cho[c2 * 3 + j];
                        const double d = DT[j] + c2 * .0013 + v.g.next() * sinT (v.ph);
                        v.ph += rate * v.rm / sr; v.ph -= std::floor (v.ph);
                        sum += cDl[c2].read (d * sr);
                    }
                    x[s] = cLp[c2].process (cHp[c2].process ((float) (sum / std::sqrt (3.0))));
                }
            }
        });
        // FLANGER
        runBlock (FLA, [&] (float* a, float* c)
        {
            for (int s = 0; s < n; ++s)
            {
                const double rate = flRate.next(), dep = flDep.next(), g = flG.next(), comp1 = flComp.next();
                for (int c2 = 0; c2 < 2; ++c2)
                {
                    float* x = c2 ? c : a; flDl[c2].push (x[s]);
                    const double u = clampf ((float) (triLfo (flPh[c2]) * dep), -1.f, 1.f), base = .0024 * std::pow (2.0, 4 * u);
                    double taps = 0, gk = 1;
                    for (int k = 1; k <= 6; ++k) { taps += flDl[c2].read (k * base * sr) * gk; gk *= g; }
                    x[s] = (float) ((x[s] + flLp[c2].process ((float) taps)) * comp1);
                    flPh[c2] += rate / sr; flPh[c2] -= std::floor (flPh[c2]);
                }
            }
        });
        // PHASER
        runBlock (PHA, [&] (float* a, float* c)
        {
            for (int s = 0; s < n; ++s)
            {
                const double rate = phRate.next(), dep = phDep.next(), fb = phFb.next();
                for (int c2 = 0; c2 < 2; ++c2)
                {
                    if ((s & 7) == 0)
                    {
                        const double f = 620 * std::pow (2.0, sinT (phPh[c2]) * dep / 1200.0);
                        for (auto& ap : phAp[c2]) ap.set (WABiquad::AP, f, .5 + fb * .25);
                        phPk[c2].set (WABiquad::PEAK, f, .9 + fb * 2.2, fb * 13);
                    }
                    float* x = c2 ? c : a; float y = x[s];
                    for (auto& ap : phAp[c2]) y = ap.process (y);
                    x[s] = phPk[c2].process ((x[s] + y) * .66f);
                    phPh[c2] += rate / sr; phPh[c2] -= std::floor (phPh[c2]);
                }
            }
        });
        // DOUBLER
        runBlock (DBL, [&] (float* a, float* c)
        {
            static const double DT[2] = { .0168, .0287 }, F1[2] = { .21, .33 }, A1[2] = { .0019, .0024 }, F2[2] = { .067, .049 }, A2[2] = { .0007, .0009 };
            for (int s = 0; s < n; ++s)
            {
                const float m = (a[s] + c[s]) * .5f;
                for (int c2 = 0; c2 < 2; ++c2)
                {
                    dbDl[c2].push (m);
                    const double d = DT[c2] + A1[c2] * sinT (dbPh[c2] + (c2 ? 70.0 / 360 : 0)) + A2[c2] * sinT (dbPh[2 + c2] + (c2 ? 140.0 / 360 : 0));
                    dbPh[c2] += F1[c2] / sr; dbPh[2 + c2] += F2[c2] / sr; dbPh[c2] -= std::floor (dbPh[c2]); dbPh[2 + c2] -= std::floor (dbPh[2 + c2]);
                    (c2 ? c : a)[s] = dbLp[c2].process (dbHp[c2].process (dbDl[c2].read (d * sr)));
                }
            }
        });
        // DELAY
        runBlock (DLY, [&] (float* a, float* c)
        {
            for (int s = 0; s < n; ++s)
            {
                const double t0 = dlT0.next(), t1 = dlT1.next(), inj1 = dlInj1.next(), self = dlSelf.next(), cross = dlCross.next(), pre = dlPre.next(), post = dlPost.next();
                const double wow = dlWow.next(), flut = dlFlut.next(), lpF = dlLpF.next(), hpF = dlHpF.next();
                if ((s & 15) == 0) for (int c2 = 0; c2 < 2; ++c2) { dlLp[c2].setPlain (WABiquad::LP, lpF, .55); dlHp[c2].setPlain (WABiquad::HP, hpF, .5); }
                const double fl = flut * sinT (dlFlPh);
                dlFlPh += 6.3 / sr; dlFlPh -= std::floor (dlFlPh);
                const float in = (a[s] + c[s]) * .5f;
                float o[2];
                for (int c2 = 0; c2 < 2; ++c2)
                {
                    const double d = (c2 ? t1 : t0) + wow * sinT (dlWowPh[c2]) + fl;
                    dlWowPh[c2] += (c2 ? .61 : .52) / sr; dlWowPh[c2] -= std::floor (dlWowPh[c2]);
                    const float y = dlD[c2].read (std::max (1.0, d * sr) - 1);       // (read before this sample goes in)
                    const float f = dlLp[c2].process (dlHp[c2].process (y)) * (float) pre;
                    o[c2] = dlOs[c2].run (f, [] (float u) { return (float) std::tanh (5.2 * clampf (u, -1.f, 1.f)); }) * (float) post;
                }
                dlD[0].push ((float) (in + self * o[0] + cross * o[1]));
                dlD[1].push ((float) (in * inj1 + self * o[1] + cross * o[0]));
                a[s] = o[0]; c[s] = o[1];
            }
        });
        // REVERB (its input is kept for the SHIMMER, which listens beside it)
        std::copy (l, l + n, rl.begin()); std::copy (r, r + n, rr.begin());
        runBlock (REV, [&] (float* a, float* c)
        {
            float* m = convBuf.getWritePointer (0); float* m2 = convBuf.getWritePointer (1);
            for (int s = 0; s < n; ++s)
            {
                const float x = (rvHp[0].process (a[s]) + rvHp[1].process (c[s])) * .5f;
                rvPre.push (x);
                const float y = rvPre.read (rvPreT.next() * sr);
                m[s] = y; m2[s] = y;
            }
            juce::dsp::AudioBlock<float> blk (convBuf.getArrayOfWritePointers(), 2, (size_t) n);
            conv.process (juce::dsp::ProcessContextReplacing<float> (blk));
            for (int s = 0; s < n; ++s)
            {
                const float g = rvSum.next(), dep = rvMod.next();
                const float yl = m[s] * g, yr = m2[s] * g;
                rvWob[0].push (yl); rvWob[1].push (yr);
                a[s] = rvWob[0].read ((.0061 + dep * sinT (rvWobPh[0])) * sr);
                c[s] = rvWob[1].read ((.0073 + dep * sinT (rvWobPh[1])) * sr);
                rvWobPh[0] += .17 / sr; rvWobPh[1] += .23 / sr; rvWobPh[0] -= std::floor (rvWobPh[0]); rvWobPh[1] -= std::floor (rvWobPh[1]);
            }
        });
        // SHIMMER: listens to what goes into the REVERB
        {
            auto& b = B[SHM];
            if (! b.idle())
            {
                if (b.plugged)
                {
                    for (int s = 0; s < n; ++s) { const float g = b.pi.next(); wl[(size_t) s] = shHp[0].process (rl[(size_t) s] * g); wr[(size_t) s] = shHp[1].process (rr[(size_t) s] * g); }
                    shm.process (wl.data(), wr.data(), sl.data(), sr2.data(), n);
                }
                else { std::fill (sl.begin(), sl.begin() + n, 0.f); std::fill (sr2.begin(), sr2.begin() + n, 0.f); }
                for (int s = 0; s < n; ++s) { const float d = b.dry.next(), w = b.wet.next() * .83f; l[s] = l[s] * d + sl[(size_t) s] * w; r[s] = r[s] * d + sr2[(size_t) s] * w; }
            }
        }
        // WIDTH (mid / side; the side widens only above 150 Hz)
        for (int s = 0; s < n; ++s)
        {
            const float mid = (l[s] + r[s]) * .5f, side = (l[s] - r[s]) * .5f, hi = wHp.process (side);
            const float sd = side * wSide.next() + hi * wSideHi.next();
            l[s] = mid + sd; r[s] = mid - sd;
        }
        // SPACE, the glue compressor, the limiter, the clip
        for (int s = 0; s < n && ! testRaw; ++s)
        {
            const double t = spT.next() * sr; const float wet = spWet.next();
            const float x = spLp.process ((l[s] + r[s]) * .5f);
            const float yl = spL.read (std::max (1.0, t) - 1), yr = spR.read (std::max (1.0, t) - 1);
            spL.push (x + yr * .42f); spR.push (yl * .42f);
            float a = l[s] + yl * wet, c = r[s] + yr * wet;
            comp.process (a, c);
            lim.process (a, c);
            static const double k = std::tanh (1.15);
            l[s] = (float) (std::tanh (clampf (a, -1.f, 1.f) * 1.15) / k * .98);
            r[s] = (float) (std::tanh (clampf (c, -1.f, 1.f) * 1.15) / k * .98);
        }
        blockTimers (n);
        samplesDone += n;
    }
}
} // namespace orph
