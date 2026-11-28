#include "SirenSynth.h"

namespace orph
{
static const double kDivBeats[] = { 4, 2, 1, .5, 1.0 / 3, .25, 1.0 / 6, .125 };
static const float kVowels[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 2300, 3000 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
static float vowelF (double cut, int k)
{
    const double x = std::max (0.0, std::min (1.0, std::log (std::max (1.0, cut) / 80.0) / std::log (8000.0 / 80.0))) * 4.0;
    const int i = std::min (3, (int) std::floor (x)); const double t = x - i;
    return (float) (kVowels[i][k] + (kVowels[i + 1][k] - kVowels[i][k]) * t);
}

void SirenSynth::prepare (double sampleRate)
{
    sr = sampleRate;
    Wavetables::build();
    cur.reset(); dying.clear(); gateOpen = false; rebuildIn = -1;
    outGain.setNow (.5f);
    bufL.assign (4096, 0.f); bufR.assign (4096, 0.f);
}

double SirenSynth::lfoHz (const Snapshot& S) const noexcept
{
    return S.on (P_lfo_sync) ? bpm / 60.0 / kDivBeats[std::max (0, std::min (7, S.ch (P_lfo_div)))] : (double) S[P_lfo_rate];
}

/* ------------------------------------------------------------ the filter's ramps */
void SirenSynth::FilterEnv::startRamps (const std::vector<std::pair<float, double>>& p, double sr)
{
    pts.clear(); t = 0; next = 0;
    for (auto& q : p) pts.push_back ({ q.first, std::max (1, (int) std::lround (q.second * sr)) });
    if (! pts.empty()) { f.expOver (pts[0].first, pts[0].second); next = 1; }
}
float SirenSynth::FilterEnv::tick() noexcept
{
    const float v = f.next(); ++t;
    if (next > 0 && next < pts.size() && ! f.busy())         // the next point: a ramp from the last one (exponentialRampToValueAtTime)
    {
        f.expOver (pts[next].first, std::max (1, pts[next].second - pts[next - 1].second));
        ++next;
    }
    return v;
}

/* ------------------------------------------------------------ building a voice (synth.js makeVoice) */
std::unique_ptr<SirenSynth::Voice> SirenSynth::makeVoice (const std::vector<float>& notes, const Snapshot& S)
{
    auto v = std::make_unique<Voice>();
    v->ch = S.ch (P_char);
    v->nNotes = (int) notes.size(); v->notes = notes;
    v->drive = 1 + S[P_flt_drive] / 100.0 * 5 + (v->ch == 1 ? .6 : 0); v->driveNorm = std::tanh (v->drive);
    v->amp.setNow (0); v->trem.setNow (1); v->killed.setNow (1); v->fMul.setNow (1);
    v->os[0].init (sr); v->os[1].init (sr);
    // filter
    v->fOn = S.on (P_flt_on); v->ft = S.ch (P_flt_type);
    const double r = S[P_flt_res] / 100.0;
    if (v->fOn)
    {
        v->nf = v->ft == 6 ? 3 : (v->ft == 0 ? 2 : 1);
        for (int k = 0; k < v->nf; ++k)
        {
            v->q[k] = v->ft == 6 ? 5 + r * 14 : v->ft <= 1 ? (k ? .7 : .7 + r * (v->ft ? 16 : 12)) : .7 + r * 14;
            for (auto& b : v->fl[k]) { b.sr = sr; b.reset(); }
            v->fe[k].f.setNow (350.f);                    // a new BiquadFilterNode starts at 350 Hz
        }
        if (v->ft == 6) { v->vowelG[0] = 2.2f; v->vowelG[1] = .7f * 2.2f; v->vowelG[2] = .35f * 2.2f; }
    }
    if (v->ch == 3) for (auto& b : v->vhs) { b.sr = sr; b.set (WABiquad::LP, 5200, 1); b.reset(); }
    if (v->ch == 3 || v->ch == 1) { v->charRate = v->ch == 3 ? .55 : .23; v->charCents = v->ch == 3 ? 14 : 3; }

    const int nN = v->nNotes;
    struct Lay { float semi; int max; };
    auto addOsc = [&] (bool b, int ni, const Lay* lay)
    {
        const int on = b ? P_b_on : P_a_on, lev = b ? P_b_level : P_a_level, wv = b ? P_b_wave : P_a_wave, vo = b ? P_b_voices : P_a_voices,
                  de = b ? P_b_detune : P_a_detune, sh = b ? P_b_shape : P_a_shape, fi = b ? P_b_fine : P_a_fine, wi = b ? P_b_width : P_a_width, pi = b ? P_b_pitch : P_a_pitch;
        if (! S.on (on) || S[lev] <= 0) return;
        const int w = S.ch (wv);
        const int cnt = std::min (lay ? lay->max : 7, w == 4 ? 7 : std::max (1, std::min (7, S.ch (vo))));
        const double lvl = S[lev] / 100.0, det = w == 4 ? 8 + S[de] * .55 : S[de] * .5;
        const auto& tb = Wavetables::get (w == 4 ? 0 : w, w == 4 ? 0.f : S[sh]);
        for (int i = 0; i < cnt; ++i)
        {
            Osc o; o.tb = &tb; o.ni = ni; o.layer = lay != nullptr;
            const double sp = cnt > 1 ? (i - (cnt - 1) / 2.0) / ((cnt - 1) / 2.0) : 0.0;
            o.cents = (float) (S[fi] + sp * det);
            panGains ((float) (sp * S[wi] / 100.0), o.gl, o.gr);
            const double side = (w == 4 && i != 3) ? (.3 + S[sh] / 100.0 * .7) : 1.0;
            o.gain = (float) (lvl * lvl * side / std::sqrt ((double) cnt) * .32 / std::sqrt ((double) nN));
            o.semi = (float) (S[pi] + (lay ? lay->semi : 0));
            v->oscs.push_back (o);
        }
    };
    for (int ni = 0; ni < nN; ++ni) { addOsc (false, ni, nullptr); addOsc (true, ni, nullptr); }
    // OCTAVE: real oscillators an octave or a fifth away
    v->octG.setNow (0);
    if (S.on (P_fx_on) && S.on (P_fx_oct_on))
    {
        const bool srcA = S.on (P_a_on) && S[P_a_level] > 0; const int mode = S.ch (P_fx_oct_mode);
        v->octG.setNow ((float) (S[P_fx_oct_mix] / 100.0 * (mode >= 2 ? .8 : 1)));
        const Lay lay { (float) (mode == 0 ? -12 : mode == 1 ? -24 : mode == 2 ? 12 : 7), mode < 2 ? 3 : 7 };
        for (int ni = 0; ni < nN; ++ni) addOsc (! srcA, ni, &lay);
    }
    // SUB (into the mix as it is: both sides)
    if (S.on (P_sn_on) && S[P_sub_level] > 0)
    {
        Osc o; const bool sq = S.ch (P_sub_wave) == 1;
        o.tb = &Wavetables::get (sq ? 1 : 3, 0); o.square = sq; o.sine = ! sq; o.ni = 0; o.semi = S.ch (P_sub_oct) ? -24.f : -12.f;
        o.gain = (float) (S[P_sub_level] / 100.0 * (sq ? .18 : .4)); o.gl = o.gr = 1.f;
        v->oscs.push_back (o);
    }
    for (size_t i = 0; i < v->oscs.size(); ++i) v->oscs[i].odd = (i % 2) == 1;
    // NOISE: a second of coloured noise, looped
    if (S.on (P_sn_on) && S[P_noise_level] > 0)
    {
        if (! noiseBuf || noiseColor != S[P_noise_color])
        {
            noiseColor = S[P_noise_color];
            auto nb = std::make_shared<std::vector<float>> ((size_t) sr); double l = 0; const double c = noiseColor / 100.0;
            for (auto& x : *nb) { const double wn = rng.white(); l += (wn - l) * (.04 + c * .96); x = (float) (l * (1.5 - c * .6)); }
            noiseBuf = nb;
        }
        v->noise = noiseBuf; v->noiseG = (float) (S[P_noise_level] / 100.0 * .3);
    }
    v->lvl.assign (v->oscs.size(), 0); v->ratio.assign (v->oscs.size(), 1.0);
    for (size_t i = 0; i < v->oscs.size(); ++i) v->ratio[i] = std::pow (2.0, v->oscs[i].semi / 12.0 + v->oscs[i].cents / 1200.0);
    // LFO (fades in)
    const double depth = S.on (P_lfo_on) ? S[P_lfo_depth] / 100.0 : 0.0;
    if (depth > 0)
    {
        v->lfoOn = true; v->lfoShape = S.ch (P_lfo_shape); v->lfoTarget = S.ch (P_lfo_target); v->lfoDepth = depth;
        const int tg = v->lfoTarget;
        double amt = tg == 0 ? depth * 120 : tg == 1 ? depth * S[P_flt_cutoff] * .9 : tg == 2 ? depth * 18 : tg == 3 ? depth * .5 : depth;
        if (tg == 3) v->trem.setNow ((float) (1 - depth * .5));
        v->lfoAmtFull = (float) amt;
        const double fade = S[P_lfo_fade] / 1000.0;
        v->lfoAmt.setNow (fade > 0 ? 0.f : (float) amt); if (fade > 0) v->lfoAmt.linTo ((float) amt, fade, sr);
    }
    return v;
}

/* ------------------------------------------------------------ envelopes */
void SirenSynth::applyFilter (Voice& v, float root, float vel, bool steady, const Snapshot& S)
{
    if (! v.fOn || v.nf == 0) return;
    const double key = std::pow (2.0, (root - 52.0) / 12.0 * S[P_flt_key] / 100.0);
    const double base = std::max (20.0, std::min (18000.0, S[P_flt_cutoff] * key));
    for (int k = 0; k < v.nf; ++k) v.fe[k].f.v *= v.fMul.v;   // (a live cutoff change so far: kept, from here on in the ramps)
    v.base0 = base; v.fMul.setNow (1);
    const double env = S.on (P_env_f_on) ? S[P_flt_env] / 100.0 * (.6 + .4 * vel) : 0.0;
    auto hzAt = [&] (double l) { return std::max (20.0, std::min (18000.0, base * std::pow (2.0, env * 6 * l))); };
    const int n = std::max (1, std::min (8, S.ch (P_fenv_n)));
    const double sp = 100.0 / std::max (25.0f, S[P_fenv_speed]);
    const double last = S[P_fenv_l1 + 2 * (n - 1)] / 100.0;
    for (int k = 0; k < v.nf; ++k)
    {
        auto map = [&] (double x) { return (float) std::max (20.0, v.ft == 6 ? (double) vowelF (x, k) : x); };
        auto& fe = v.fe[k];
        fe.pts.clear(); fe.next = 0;
        if (steady) { fe.f.approach (map (gateOpen ? hzAt (last) : base), .02, sr); continue; }
        std::vector<std::pair<float, double>> p; double tt = 0;
        for (int i = 0; i < n; ++i)
        {
            const double at = std::max (std::max (.003, S[P_fenv_t1 + 2 * i] / 1000.0 * sp), tt + .001);
            p.push_back ({ map (hzAt (S[P_fenv_l1 + 2 * i] / 100.0)), at }); tt = at;
        }
        fe.startRamps (p, sr);
    }
}

void SirenSynth::startAmp (Voice& v, float vel, const Snapshot& S, bool retrig)
{
    if (! retrig) return;
    const double dyn = S[P_dyn] / 100.0, peak = .9 * (1 - dyn + dyn * vel);
    const double att = S[P_env_a_att] / 1000.0, sus = (fullSus ? 100.0 : (double) S[P_env_a_sus]) / 100.0;
    v.amp.linTo ((float) peak, att, sr);
    v.amp.approach ((float) (peak * sus), S[P_env_a_dec] / 3000.0, sr, att);
    if (v.lfoOn && S[P_lfo_fade] > 0) { v.lfoAmt.setNow (0); v.lfoAmt.linTo (v.lfoAmtFull, S[P_lfo_fade] / 1000.0, sr); }
}

/* ------------------------------------------------------------ playing */
void SirenSynth::setPitches (const float* notes, int n, double glideSec, const Snapshot& S)
{
    if (! cur || n <= 0) return;
    auto& noteHz = cur->hz;
    if ((int) noteHz.size() < cur->nNotes) noteHz.resize ((size_t) cur->nNotes);
    for (int ni = 0; ni < cur->nNotes; ++ni)
    {
        const float f = (float) hz (notes[std::min (ni, n - 1)], S);
        if (glideSec > 0) noteHz[(size_t) ni].approach (f, glideSec / 3.0, sr); else noteHz[(size_t) ni].setNow (f);
    }
}

void SirenSynth::noteOn (const float* notes, int n, float vel, bool legato, bool hold, const Snapshot& S)
{
    if (n <= 0) return;
    fullSus = hold;
    std::vector<float> ns (notes, notes + n);
    bool fresh = false;
    if (! cur || cur->nNotes != n)
    {
        if (cur) { cur->dying = true; cur->killed.approach (0, .015, sr); dying.push_back (std::move (cur)); }
        cur = makeVoice (ns, S); legato = false; fresh = true;
    }
    // a new voice starts on its notes (the preview's swooped up from 440 Hz when the count changed mid-glide)
    const double glide = gateOpen && ! fresh ? S[P_glide] / 1000.0 : 0.0;
    setPitches (notes, n, glide, S);
    const bool retrig = ! (legato && gateOpen);
    gateOpen = true; curNotes = ns; velNow = vel;
    if (retrig) { startAmp (*cur, vel, S, true); applyFilter (*cur, notes[0], vel, false, S); }
}

void SirenSynth::noteOff (const Snapshot& S)
{
    if (! cur) return;
    gateOpen = false;
    cur->amp.approach (0, S[P_env_a_rel] / 3000.0, sr);
    if (cur->fOn)
    {
        const double key = std::pow (2.0, ((curNotes.empty() ? 40.0 : curNotes[0]) - 52.0) / 12.0 * S[P_flt_key] / 100.0);
        const double base = std::max (20.0, std::min (18000.0, S[P_flt_cutoff] * key));
        for (int k = 0; k < cur->nf; ++k)
        {
            cur->fe[k].pts.clear(); cur->fe[k].next = 0;
            cur->fe[k].f.approach ((float) (cur->ft == 6 ? vowelF (base, k) : base / std::max (1e-3f, cur->fMul.target)), S[P_env_f_rel] / 3000.0, sr);
        }
    }
}

void SirenSynth::filterKick (float root, const Snapshot& S)
{
    if (cur && gateOpen) applyFilter (*cur, root, velNow, false, S);
}

/* Which settings rebuild the voice (50 ms after the last change, as in the preview), which move it live, which don't
   touch it. Live (the plugin's own, so DAW automation sweeps smoothly): the filter's cutoff, key tracking, resonance and
   drive (except for VOWEL, which is rebuilt). */
enum : uint8_t { K_REBUILD, K_NONE, K_LIVE };
static const std::array<uint8_t, P_COUNT>& paramKinds()
{
    static const auto k = []
    {
        std::array<uint8_t, P_COUNT> a {};
        for (int i = 0; i < P_COUNT; ++i)
        {
            const juce::String id (paramDefs()[i].id);
            a[(size_t) i] = K_REBUILD;
            if (id.startsWith ("syn.fx.") || id.startsWith ("syn.fenv.") || i == P_env_f_rel || i == P_glide || i == P_dyn || i == P_legato || i == P_level
                || i == P_trim || i == P_space || i == P_env_a_att || i == P_env_a_dec || i == P_env_a_rel) a[(size_t) i] = K_NONE;
            if (i == P_flt_cutoff || i == P_flt_key || i == P_flt_res || i == P_flt_drive) a[(size_t) i] = K_LIVE;
        }
        return a;
    }();
    return k;
}

void SirenSynth::paramsChanged (const Snapshot& now, const Snapshot& before)
{
    const auto& kinds = paramKinds();
    bool rebuild = false, live = false;
    for (int i = 0; i < P_COUNT; ++i)
    {
        if (now.v[(size_t) i] == before.v[(size_t) i]) continue;
        if (i == P_fx_oct_mix) { if (cur) cur->octG.approach ((float) (now[P_fx_oct_mix] / 100.0 * (now.ch (P_fx_oct_mode) >= 2 ? .8 : 1)), .03, sr); continue; }
        if (i == P_fx_oct_on || i == P_fx_oct_mode || (i == P_fx_on && now.on (P_fx_oct_on))) { rebuild = true; continue; }
        const auto k = kinds[(size_t) i];
        if (k == K_LIVE && cur && cur->ft != 6) live = true;
        else if (k != K_NONE) rebuild = true;
    }
    if (live && cur)
    {
        Voice& v = *cur;
        const double r = now[P_flt_res] / 100.0;
        for (int k = 0; k < v.nf; ++k) v.q[k] = v.ft <= 1 ? (k ? .7 : .7 + r * (v.ft ? 16 : 12)) : .7 + r * 14;
        v.drive = 1 + now[P_flt_drive] / 100.0 * 5 + (v.ch == 1 ? .6 : 0); v.driveNorm = std::tanh (v.drive);
        const double root = curNotes.empty() ? 40.0 : curNotes[0];
        const double base = std::max (20.0, std::min (18000.0, now[P_flt_cutoff] * std::pow (2.0, (root - 52.0) / 12.0 * now[P_flt_key] / 100.0)));
        v.fMul.approach ((float) (base / std::max (20.0, v.base0)), .02, sr);
    }
    if (rebuild && cur && rebuildIn < 0) rebuildIn = (int) (sr * .05);
    else if (rebuild && cur) rebuildIn = (int) (sr * .05);
}

/* ------------------------------------------------------------ sound */
float SirenSynth::renderVoice (Voice& v, const Snapshot& S, float* L, float* R, int n)
{
    const double lfoF = v.lfoOn ? lfoHz (S) : 0, lfoInc = lfoF / sr, rndP = lfoF * 4096.0 / sr, rndK = 1 - std::pow (.95, rndP);
    const int rndStep = lfoF > 0 ? std::max (1, (int) std::lround (sr / (64.0 * lfoF))) : 1 << 30;
    float peak = 0;
    // per block: each oscillator's wavetable level (from where its note is now)
    auto& lvl = v.lvl; const auto& ratio = v.ratio;
    for (size_t i = 0; i < v.oscs.size(); ++i)
    {
        const auto& o = v.oscs[i];
        const double f = (o.ni < (int) v.hz.size() ? v.hz[(size_t) o.ni].v : 440.f) * ratio[i];
        lvl[i] = Wavetables::levelFor (f * 1.06, sr);
    }
    for (int s = 0; s < n; ++s)
    {
        // modulation
        double lv = 0;
        if (v.lfoOn)
        {
            const double p = v.lfoPh;
            switch (v.lfoShape)
            {
                case 0: lv = std::sin (kTau * p); break;
                case 1: lv = p < .25 ? 4 * p : p < .75 ? 2 - 4 * p : 4 * p - 4; break;
                case 2: { const double q = p + .5; lv = 2 * (q - std::floor (q)) - 1; break; }
                case 3: lv = p < .5 ? 1 : -1; break;
                default:
                    if (--v.rndLeft <= 0) { v.rndLeft = rndStep; v.rndT = rng.white(); if (v.lfoShape == 4) v.rndA = v.rndT; }
                    if (v.lfoShape == 5) v.rndA += (v.rndT - v.rndA) * rndK;
                    lv = v.rndA; break;
            }
            v.lfoPh += lfoInc; if (v.lfoPh >= 1) v.lfoPh -= 1;
        }
        const double amt = v.lfoOn ? v.lfoAmt.next() : 0;
        double cAll = wheel, cOdd = 0;
        if (v.lfoOn && v.lfoTarget == 0) cAll += lv * amt;
        if (v.lfoOn && v.lfoTarget == 2) cOdd = lv * amt;
        if (v.charRate > 0) { cAll += std::sin (kTau * v.charPh) * v.charCents; v.charPh += v.charRate / sr; if (v.charPh >= 1) v.charPh -= 1; }
        const double mAll = cAll != 0 ? std::pow (2.0, cAll / 1200.0) : 1.0, mOdd = cOdd != 0 ? std::pow (2.0, cOdd / 1200.0) : 1.0;
        // oscillators
        double l = 0, r = 0, ll = 0, lr = 0;
        for (size_t i = 0; i < v.oscs.size(); ++i)
        {
            auto& o = v.oscs[i];
            const double f = (o.ni < (int) v.hz.size() ? (double) v.hz[(size_t) o.ni].v : 440.0) * ratio[i] * mAll * (o.odd ? mOdd : 1.0);
            const float x = Wavetables::read (o.tb->level[lvl[i]], o.ph) * o.gain;
            o.ph += f / sr; o.ph -= std::floor (o.ph);
            if (o.layer) { ll += x * o.gl; lr += x * o.gr; } else { l += x * o.gl; r += x * o.gr; }
        }
        const float og = v.octG.next();
        l += ll * og; r += lr * og;
        if (v.noise) { const float nz = (*v.noise)[v.noiseI] * v.noiseG; l += nz; r += nz; if (++v.noiseI >= v.noise->size()) v.noiseI = 0; }
        // drive (the shaper's input stops at +-1)
        const float dr = (float) v.drive, dn = (float) v.driveNorm;
        auto shape = [dr, dn] (float x) { return std::tanh (clampf (x, -1.f, 1.f) * dr) / dn; };
        l = v.os[0].run ((float) l, shape); r = v.os[1].run ((float) r, shape);
        // filter
        if (v.fOn && v.nf > 0)
        {
            const double addHz = (v.lfoOn && v.lfoTarget == 1) ? lv * amt : 0.0, fm = v.fMul.next();
            double yl = 0, yr = 0;
            for (int k = 0; k < v.nf; ++k)
            {
                const float fk = v.fe[k].tick();
                if ((s & 3) == 0)
                {
                    const auto type = v.ft == 6 ? WABiquad::BP : v.ft <= 2 ? WABiquad::LP : v.ft == 3 ? WABiquad::BP : v.ft == 4 ? WABiquad::HP : WABiquad::NOTCH;
                    const double fq = std::max (0.0, std::min (18000.0, std::max (20.0, (double) fk * fm)) + addHz);
                    v.fl[k][0].set (type, fq, v.q[k]); v.fl[k][1].b0 = v.fl[k][0].b0; v.fl[k][1].b1 = v.fl[k][0].b1; v.fl[k][1].b2 = v.fl[k][0].b2; v.fl[k][1].a1 = v.fl[k][0].a1; v.fl[k][1].a2 = v.fl[k][0].a2;
                }
                if (v.ft == 6) { yl += v.fl[k][0].process ((float) l) * v.vowelG[k]; yr += v.fl[k][1].process ((float) r) * v.vowelG[k]; }
                else { l = v.fl[k][0].process ((float) l); r = v.fl[k][1].process ((float) r); }
            }
            if (v.ft == 6) { l = yl; r = yr; }
        }
        // amp, character, LFO volume / pan
        const double a = v.amp.next() * v.killed.next();
        l *= a; r *= a;
        if (v.ch == 2) { l = std::round (std::max (-1.0, std::min (1.0, l)) * 16) / 16; r = std::round (std::max (-1.0, std::min (1.0, r)) * 16) / 16; }
        if (v.ch == 3) { l = v.vhs[0].process ((float) l); r = v.vhs[1].process ((float) r); }
        if (v.lfoOn && v.lfoTarget == 3) { const double g = (1 - v.lfoDepth * .5) + lv * amt; l *= g; r *= g; }
        if (v.lfoOn && v.lfoTarget == 4)
        {
            const double pan = std::max (-1.0, std::min (1.0, lv * amt));
            if (pan <= 0) { const double x = (pan + 1) * kPi / 2, gl = std::cos (x), gr = std::sin (x); const double nl = l + r * gl, nr = r * gr; l = nl; r = nr; }
            else { const double x = pan * kPi / 2, gl = std::cos (x), gr = std::sin (x); const double nl = l * gl, nr = r + l * gr; l = nl; r = nr; }
        }
        L[s] += (float) l; R[s] += (float) r;
        peak = std::max (peak, (float) std::max (std::abs (l), std::abs (r)));
    }
    return peak;
}

void SirenSynth::render (const Snapshot& S, float* L, float* R, int n)
{
    if ((int) bufL.size() < n) { bufL.resize ((size_t) n); bufR.resize ((size_t) n); }
    std::fill (bufL.begin(), bufL.begin() + n, 0.f); std::fill (bufR.begin(), bufR.begin() + n, 0.f);
    // a rebuild that has waited 50 ms (synth.js onChange)
    if (rebuildIn >= 0 && (rebuildIn -= n) < 0 && cur)
    {
        rebuildIn = -1;
        auto old = std::move (cur);
        cur = makeVoice (curNotes.empty() ? old->notes : curNotes, S);
        const auto& ns = curNotes.empty() ? old->notes : curNotes;
        cur->hz = old->hz; cur->hz.resize ((size_t) cur->nNotes);
        setPitches (ns.data(), (int) ns.size(), 0, S);
        if (gateOpen)
        {
            const float gv = (float) (.9 * (fullSus ? 100.0 : (double) S[P_env_a_sus]) / 100.0);
            cur->amp.setNow (0); cur->amp.linTo (gv, .03, sr);
            applyFilter (*cur, ns.empty() ? 40.f : ns[0], velNow, true, S);
        }
        old->dying = true; old->killed.approach (0, .03, sr); dying.push_back (std::move (old));
    }
    // the wheels (once a block: the KEYS' bend glides in ~8 ms, the vibrato's depth in ~20 ms)
    bendC += (bendT - bendC) * (1 - std::exp (-n / (.008 * sr))); vibC += (vibT - vibC) * (1 - std::exp (-n / (.02 * sr)));
    wheel = bendC + vibC * std::sin (kTau * vibPh); vibPh += 5.5 * n / sr; vibPh -= std::floor (vibPh);
    // glides, a block at a time (the processor renders 32 samples at a time)
    auto glide = [n] (Voice& v) { for (auto& h : v.hz) for (int s = 0; s < n; ++s) h.next(); };
    if (cur) glide (*cur);
    for (auto& d : dying) glide (*d);
    if (cur) renderVoice (*cur, S, bufL.data(), bufR.data(), n);
    for (auto& d : dying) { renderVoice (*d, S, bufL.data(), bufR.data(), n); d->life += n; }
    dying.erase (std::remove_if (dying.begin(), dying.end(), [this] (const std::unique_ptr<Voice>& d) { return d->life > (int) (sr * .3); }), dying.end());
    outGain.approach ((float) (.5 * dbToGain (S[P_level] + S[P_trim])), .05, sr);
    for (int s = 0; s < n; ++s) { const float g = outGain.next(); L[s] += bufL[(size_t) s] * g; R[s] += bufR[(size_t) s] * g; }
}
} // namespace orph
