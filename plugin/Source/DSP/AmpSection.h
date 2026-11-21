#pragma once
// =============================================================================
//  AmpSection: the AMP tab.
//   * "model": one of the in-house amps in AmpModels.h (modelled circuit by
//              circuit; no files needed)
//   * "nam"  : NAM captures, one per channel (up to 4), switched instantly
//   * "capture": a capture amp from the amp pack (CaptureAmp.h): many captures,
//              picked and blended by the amp's own knobs and buttons
//   * "none" : no amp (signal passes straight to the cab)
//  For models, the knobs are the amp's own controls (its real tone stack,
//  presence/depth in the power amp, master into the power tubes). For captures,
//  Gain drives the capture's input and the other knobs are an EQ after it.
//  Every switch crossfades, so there are no clicks.
// =============================================================================
#include "DspCore.h"
#include "NamEngine.h"
#include "AmpModels.h"
#include "CaptureAmp.h"
#include <juce_dsp/juce_dsp.h>

namespace obs::dsp
{
/** Passive-style tone stack + presence/depth + master, applied after the amp. */
class ToneStack
{
public:
    void prepare (double s)  { sr = s; for (auto& v : cache) v = -99.f; reset(); }
    void reset()             { bass.reset(); mid.reset(); treble.reset(); presence.reset(); depth.reset(); }
    void process (float* x, int n, float b, float m, float t, float p, float d, float master)
    {
        auto upd = [this] (int idx, float v) { if (std::abs (cache[idx] - v) < 1e-4f) return false; cache[idx] = v; return true; };
        if (upd (0, b)) bass.set (Biquad::LowShelf, sr, 120.f, 0.7f, (b - 5.f) * 2.4f);
        if (upd (1, m)) mid.set (Biquad::Peak, sr, 650.f, 0.7f, (m - 5.f) * 2.0f);
        if (upd (2, t)) treble.set (Biquad::HighShelf, sr, 3000.f, 0.7f, (t - 5.f) * 2.4f);
        if (upd (3, p)) presence.set (Biquad::HighShelf, sr, 5500.f, 0.8f, (p - 5.f) * 1.8f);
        if (upd (4, d)) depth.set (Biquad::Peak, sr, 90.f, 1.1f, (d - 5.f) * 1.8f);
        masterSm.set (dbToGain ((master - 5.f) * 3.f));
        for (int i = 0; i < n; ++i)
            x[i] = depth.process (presence.process (treble.process (mid.process (bass.process (x[i]))))) * masterSm.next();
    }
    void initSmoother (double s) { masterSm.reset (s, 30.f, 1.f); }
private:
    double sr = 48000;
    Biquad bass, mid, treble, presence, depth;
    float cache[5] {};
    Smoother masterSm;
};

class AmpSection
{
public:
    enum Type { None = 0, Model = 1, Nam = 2, Capture = 3 };
    static constexpr int kChannels = 4;

    void prepare (double s, int maxBlock)
    {
        sr = s; block = maxBlock;
        model.prepare (s, maxBlock);
        if (model.model() == nullptr) model.setModel (&amp::models()[(size_t) juce::jlimit (0, (int) amp::models().size() - 1, modelIndex.load())]);
        tone.prepare (s); tone.initSmoother (s);
        inGain.reset (s, 30.f, 1.f);
        fade.reset (s, 12.f, 1.f);
        scratch.assign ((size_t) maxBlock, 0.f);
        for (auto& v : vbuf) v.assign ((size_t) maxBlock, 0.f);
        driveSm.reset (s, 30.f, 1.f); levelSm.reset (s, 30.f, 1.f);
        rampLen = (float) (0.03 * s);
        reverb.prepare (s); chorusLine.prepare ((int) (0.05 * s)); chorusLine.clear();
        tremLfo.setSampleRate (s); chorusLfo.setSampleRate (s);
        for (auto& c : capCache) c = -99.f;
        nv = 0;
    }
    void reset()             { model.reset(); tone.reset(); nv = 0; }

    /** Message thread. */
    void setType (Type t)    { type.store ((int) t); }
    Type getType() const     { return (Type) type.load(); }
    /** Message thread: which in-house amp (index into amp::models()). */
    void setModel (int index) { modelIndex.store (juce::jlimit (0, (int) amp::models().size() - 1, index)); }
    RtSlot<NamEngine>& channelSlot (int ch) { return captures[juce::jlimit (0, kChannels - 1, ch)]; }
    /** Capture amps: the loaded amp, and its face state (knobs that aren't plugin parameters, buttons, switches). */
    RtSlot<CaptureAmp>& captureSlot()       { return capture; }
    void setFaceKnob (int j, float v)       { if (juce::isPositiveAndBelow (j, CaptureAmpMap::kMaxKnobs)) faceKnob[j].store (juce::jlimit (0.f, 1.f, v)); }
    void setFaceDim (int i, int ch, int v)  { if (juce::isPositiveAndBelow (i, CaptureAmpMap::kMaxDims) && juce::isPositiveAndBelow (ch, kChannels)) faceDim[i][ch].store (v); }
    void setFaceSwitch (int i, bool on)     { if (juce::isPositiveAndBelow (i, CaptureAmpMap::kMaxSwitches)) faceSwitch[i].store (on); }
    /** True while the playing capture already contains a cab (the cab section is bypassed). */
    bool cabInside() const                  { return type.load() == Capture && cabIn.load(); }
    /** What a capture amp plays right now (bank indices, blend), for the AMP page. */
    int playingA() const                    { return playA.load(); }
    int playingB() const                    { return playB.load(); }
    float playingBlend() const              { return playF.load(); }

    /** Audio thread. gain..master in 0..10 knob units. */
    void process (float* x, int n, bool enabled, int channel, float gain, float bass, float mid, float treble, float presence, float depth, float master)
    {
        const int t = enabled ? type.load() : (int) None;
        channel = juce::jlimit (0, kChannels - 1, channel);
        NamEngine* eng = t == Nam ? captures[channel].get() : nullptr;

        // The in-house amp changed? Switch it here, on the audio thread (no allocation).
        const auto* wanted = &amp::models()[(size_t) modelIndex.load()];
        if (t == Model && model.model() != wanted) model.setModel (wanted);
        // Crossfade (dip) whenever the processing path changes: type, model, channel or capture.
        CaptureAmp* cap = t == Capture ? capture.get() : nullptr;
        const void* path = t == Nam ? (const void*) eng : t == Model ? (const void*) wanted : t == Capture ? (const void*) cap : (const void*) (intptr_t) (t + 1);
        if (path != lastPath) { lastPath = path; fade.snap (0.f); fade.set (1.f); }

        if (t == None)
        {
            if (fade.isSmoothing()) for (int i = 0; i < n; ++i) x[i] *= fade.next();
            for (auto& c : captures) c.blockDone();
            capture.blockDone();
            return;
        }

        if (t == Nam)
        {
            if (eng == nullptr)
            {
                // Channel selected but no capture loaded yet: play the in-house amp so you still hear an amp.
                if (model.model() != wanted) model.setModel (wanted);
                model.process (x, n, gain, bass, mid, treble, presence, depth, master);
            }
            else
            {
                inGain.set (dbToGain ((gain - 5.f) * 4.f));      // 0..10 -> -20..+20 dB into the capture
                for (int i = 0; i < n; ++i) x[i] *= inGain.next();
                eng->process (x, n);
                tone.process (x, n, bass, mid, treble, presence, depth, master);
            }
        }
        else if (t == Capture)
        {
            if (cap == nullptr || cap->map == nullptr || n > (int) vbuf[0].size())
            {
                if (model.model() != wanted) model.setModel (wanted);      // still loading: play the in-house amp
                model.process (x, n, gain, bass, mid, treble, presence, depth, master);
                playA.store (-1); playB.store (-1);
            }
            else processCapture (*cap, x, n, channel, gain, bass, mid, treble, presence, depth, master);
        }
        else model.process (x, n, gain, bass, mid, treble, presence, depth, master);

        if (fade.isSmoothing()) for (int i = 0; i < n; ++i) x[i] *= fade.next();
        for (auto& c : captures) c.blockDone();
        capture.blockDone();
    }

    void collectGarbage (bool force)  { for (auto& c : captures) c.collectGarbage (force); capture.collectGarbage (force); }
    int latency() const
    {
        const int t = type.load();
        if (t == Model) return model.latency();
        return 0;   // NAM resampler latency is small and varies per capture; reported separately
    }
    int namLatency (int channel)
    {
        if (type.load() == Capture)
        {
            if (auto* c = capture.get()) for (auto& e : c->engines) if (e) return e->latencySamples();
            return 0;
        }
        auto* e = captures[juce::jlimit (0, kChannels - 1, channel)].get(); return e ? e->latencySamples() : 0;
    }
    double sampleRate() const         { return sr; }
    int maxBlock() const              { return block; }

private:
    // ------------------------------------------------------------------ capture amps
    void processCapture (CaptureAmp& ca, float* x, int n, int channel, float gain, float bass, float mid, float treble, float presence, float depth, float master)
    {
        const auto& M = *ca.map;
        const int nk = juce::jmin ((int) M.roles.size(), CaptureAmpMap::kMaxKnobs);
        if (&ca != lastCapture) { lastCapture = &ca; nv = 0; for (auto& c : capCache) c = -99.f; }

        // knob positions 0..1: the plugin's AMP knobs where the face has them, the face's own knobs otherwise
        const float pv[7] = { gain, bass, mid, treble, presence, depth, master };
        float k[CaptureAmpMap::kMaxKnobs] {};
        float paramOf[7] = { 5.f, 5.f, 5.f, 5.f, 5.f, 5.f, 5.f };     // the AMP knob values the tone stack gets (5 = flat)
        int paramKnob[7] = { -1, -1, -1, -1, -1, -1, -1 };
        for (int j = 0; j < nk; ++j)
        {
            const auto r = M.roles[(size_t) j];
            const int ri = (int) r - (int) KnobRole::Gain;
            if (ri >= 0 && ri < 7) { k[j] = clampf (pv[ri] * 0.1f, 0.f, 1.f); paramKnob[ri] = j; }
            else k[j] = faceKnob[j].load (std::memory_order_relaxed);
            if (const int st = M.steps[(size_t) j]; st > 1) k[j] = std::round (k[j] * (float) (st - 1)) / (float) (st - 1);
        }
        int dv[CaptureAmpMap::kMaxDims] {};
        for (size_t i = 0; i < M.dims.size(); ++i)
            dv[i] = (i == 0 && M.firstDimIsChannel) ? channel : faceDim[i][channel].load (std::memory_order_relaxed);
        const auto pk = M.resolve (k, dv);
        if (pk.rig != lastRig) { lastRig = pk.rig; cabIn.store (pk.rig); fade.snap (0.f); fade.set (1.f); }
        playA.store (pk.a, std::memory_order_relaxed); playB.store (pk.b, std::memory_order_relaxed); playF.store (pk.f, std::memory_order_relaxed);

        // ---- into the captures: drive, switches, tight
        float driveDb = pk.driveFromGain ? (k[pk.driveKnob] - M.defaults[(size_t) pk.driveKnob]) * 24.f : 0.f;
        float levelDb = 0.f, tightV = -1.f, airV = -1.f, bodyV = -1.f, revV = 0.f, speedV = 0.5f, intenV = 0.f, chSpeedV = 0.5f, chDepthV = 0.5f;
        for (int j = 0; j < nk; ++j)
        {
            const float d = k[j] - M.defaults[(size_t) j];
            switch (M.roles[(size_t) j])
            {
                case KnobRole::Drive:    driveDb += d * 24.f; break;
                case KnobRole::Level:    levelDb += d * 24.f; break;
                case KnobRole::LevelCh0: if (channel == 0) levelDb += d * 24.f; break;
                case KnobRole::LevelCh1: if (channel == 1) levelDb += d * 24.f; break;
                case KnobRole::Tight:    tightV = k[j]; break;
                case KnobRole::Air:      airV = k[j]; break;
                case KnobRole::Body:     bodyV = k[j]; break;
                case KnobRole::Reverb:   revV = k[j]; break;
                case KnobRole::Speed:    speedV = k[j]; break;
                case KnobRole::Intensity: intenV = k[j]; break;
                case KnobRole::ChorusSpeed: chSpeedV = k[j]; break;
                case KnobRole::ChorusDepth: chDepthV = k[j]; break;
                default: break;
            }
        }
        bool bright = false, chorus = false;
        for (int i = 0; i < M.switchIds.size(); ++i)
            if (faceSwitch[i].load (std::memory_order_relaxed))
            {
                const auto& id = M.switchIds[i];
                if (id == "crunch" || id == "boost") driveDb += 6.f;
                else if (id == "bright") bright = true;
                else if (id == "chorus") chorus = true;
            }
        driveSm.set (dbToGain (clampf (driveDb, -24.f, 24.f)));
        auto upd = [this] (int i, float v) { if (std::abs (capCache[i] - v) < 1e-4f) return false; capCache[i] = v; return true; };
        if (upd (0, tightV) && tightV >= 0.f) tightF.set (Biquad::HP, sr, 20.f * std::pow (12.5f, tightV), 0.7f);
        float* in = vbuf[kVoices].data();
        for (int i = 0; i < n; ++i) { float v = x[i] * driveSm.next(); if (tightV >= 0.f) v = tightF.process (v); in[i] = v; }

        // ---- the captures: up to two blend, older ones fade out (no clicks when a knob crosses a step)
        const float ta = pk.b >= 0 ? 1.f - pk.f : 1.f, tb = pk.f;
        for (int v = 0; v < nv; ++v) voices[v].target = (voices[v].e == pk.a ? ta : 0.f) + (voices[v].e == pk.b ? tb : 0.f);
        const bool fresh = nv == 0;
        auto ensure = [this, fresh] (int e, float target)
        {
            if (e < 0) return;
            for (int v = 0; v < nv; ++v) if (voices[v].e == e) return;
            if (nv == kVoices)
            {
                int drop = -1; float lo = 2.f;
                for (int v = 0; v < nv; ++v) if (voices[v].target == 0.f && voices[v].w < lo) { lo = voices[v].w; drop = v; }
                if (drop < 0) return;
                voices[drop] = voices[--nv];
            }
            voices[nv++] = { e, fresh ? target : 0.f, target };
        };
        ensure (pk.a, ta); ensure (pk.b, tb);
        std::fill (x, x + n, 0.f);
        const float step = (float) n / juce::jmax (1.f, rampLen);
        for (int v = 0; v < nv; ++v)
        {
            auto& vo = voices[v];
            const float w0 = vo.w, w1 = vo.w < vo.target ? juce::jmin (vo.target, vo.w + step) : juce::jmax (vo.target, vo.w - step);
            vo.w = w1;
            auto* eng = juce::isPositiveAndBelow (vo.e, (int) ca.engines.size()) ? ca.engines[(size_t) vo.e].get() : nullptr;
            if (eng == nullptr) continue;                                    // a missing file: nothing from this one
            float* b = vbuf[v].data();
            std::copy (in, in + n, b);
            eng->process (b, n);
            const float inc = (w1 - w0) / (float) n;
            float w = w0;
            for (int i = 0; i < n; ++i) { w += inc; x[i] += b[i] * w; }
        }
        for (int v = 0; v < nv;) { if (voices[v].w <= 0.f && voices[v].target <= 0.f) voices[v] = voices[--nv]; else ++v; }

        // ---- after the captures: the amp's tone knobs (flat at 5; knobs that picked the captures stay flat), extras
        for (int ri = 0; ri < 6; ++ri)
        {
            const int j = paramKnob[ri];
            if (j >= 0 && ri > 0 && ! (pk.selects & (1u << j))) paramOf[ri] = pv[ri];
        }
        const int mj = paramKnob[6];
        const float masterAdj = mj >= 0 ? 5.f + (master - M.defaults[(size_t) mj] * 10.f) : 5.f;   // the face's default Master = unity
        tone.process (x, n, paramOf[1], paramOf[2], paramOf[3], paramOf[4], paramOf[5], masterAdj);
        if (upd (1, bright ? 1.f : 0.f)) brightF.set (Biquad::HighShelf, sr, 2500.f, 0.7f, bright ? 5.f : 0.f);
        if (upd (2, airV) && airV >= 0.f) airF.set (Biquad::HighShelf, sr, 7000.f, 0.7f, (airV - 0.5f) * 12.f);
        if (upd (3, bodyV) && bodyV >= 0.f) bodyF.set (Biquad::Peak, sr, 180.f, 0.8f, (bodyV - 0.5f) * 10.f);
        levelDb += ca.trimDb + (pk.rig ? CaptureAmp::kRigDb : 0.f);
        levelSm.set (dbToGain (clampf (levelDb, -36.f, 36.f)));
        tremLfo.setRate (1.5f + speedV * 8.5f);
        chorusLfo.setRate (0.2f + chSpeedV * 4.f);
        const float revMix = std::pow (revV, 1.5f) * 0.6f;
        for (int i = 0; i < n; ++i)
        {
            float v = x[i];
            if (bright) v = brightF.process (v);
            if (airV >= 0.f) v = airF.process (v);
            if (bodyV >= 0.f) v = bodyF.process (v);
            v *= levelSm.next();
            if (intenV > 0.001f) v *= 1.f - intenV * 0.9f * (0.5f - 0.5f * tremLfo.next());
            if (chorus)
            {
                chorusLine.push (v);
                const float d = (0.007f + 0.004f * chDepthV * (1.f + chorusLfo.next())) * (float) sr;
                v = 0.7f * v + 0.7f * chorusLine.readLinear (d);
            }
            if (revMix > 0.001f) v += reverb.process (v) * revMix;
            x[i] = v;
        }
    }

    static constexpr int kVoices = 4;
    struct Voice { int e = -1; float w = 0.f, target = 0.f; };
    RtSlot<CaptureAmp> capture;
    Voice voices[kVoices];
    int nv = 0;
    std::vector<float> vbuf[kVoices + 1];
    const CaptureAmp* lastCapture = nullptr;
    bool lastRig = false;
    float rampLen = 1440.f;
    Smoother driveSm, levelSm;
    Biquad tightF, brightF, airF, bodyF;
    float capCache[4] { -99.f, -99.f, -99.f, -99.f };
    AmpReverb reverb;
    DelayLine chorusLine;
    Lfo tremLfo, chorusLfo;
    std::atomic<float> faceKnob[CaptureAmpMap::kMaxKnobs] {};
    std::atomic<int> faceDim[CaptureAmpMap::kMaxDims][kChannels] {};
    std::atomic<bool> faceSwitch[CaptureAmpMap::kMaxSwitches] {};
    std::atomic<bool> cabIn { false };
    std::atomic<int> playA { -1 }, playB { -1 };
    std::atomic<float> playF { 0.f };

    double sr = 48000; int block = 512;
    std::atomic<int> type { (int) Model };
    std::atomic<int> modelIndex { amp::findModel ("core") >= 0 ? amp::findModel ("core") : 0 };
    RtSlot<NamEngine> captures[kChannels];
    amp::AmpModel model;
    ToneStack tone;
    Smoother inGain, fade;
    const void* lastPath = nullptr;
    std::vector<float> scratch;
};
} // namespace obs::dsp
