#include <algorithm>
#include "Engine.h"
#include "../Core/Studio.h"

namespace obs
{
using namespace dsp;

Engine::Engine() = default;

void Engine::attach (juce::AudioProcessorValueTreeState& state)
{
    params.attach (state);
    auto g = [this] (const juce::String& id) { return params.ptr (id); };
    p.transposeHq = g ("transpose.hq"); p.inGain = g ("in.gain"); p.gateOn = g ("gate.on"); p.gateThresh = g ("gate.thresh"); p.transpose = g ("transpose");
    p.doublerOn = g ("doubler.on"); p.doublerTime = g ("doubler.time"); p.width = g ("width"); p.outGain = g ("out.gain");
    p.preOn = g ("pre.on"); p.synthOn = g ("synth.on"); p.ampOn = g ("amp.on"); p.cabOn = g ("cab.on"); p.eqOn = g ("eq.on"); p.postOn = g ("post.on"); p.sfxOn = g ("sfx.on");
    p.ampGain = g ("amp.gain"); p.ampBass = g ("amp.bass"); p.ampMid = g ("amp.mid"); p.ampTreble = g ("amp.treble");
    p.ampPresence = g ("amp.presence"); p.ampDepth = g ("amp.depth"); p.ampMaster = g ("amp.master"); p.ampChannel = g ("amp.channel");
    for (int b = 0; b < kEqBands; ++b) p.eqBand[b] = g ("eq.b" + juce::String (b + 1));
    p.eqLc = g ("eq.lc"); p.eqHc = g ("eq.hc"); p.eqLcOn = g ("eq.lc.on"); p.eqHcOn = g ("eq.hc.on");
    for (int m = 0; m < 2; ++m)
    {
        const juce::String k = "cab." + juce::String (m == 0 ? "a" : "b") + ".";
        p.mic[m] = { g (k + "mic"), g (k + "pos"), g (k + "dist"), g (k + "level"), g (k + "pan"), g (k + "solo"), g (k + "mute"), g (k + "phase") };
    }
    p.cabLc = g ("cab.lc"); p.cabHc = g ("cab.hc");
    for (int i = 0; i < obs::synthdefs::count; ++i) p.syn[i] = g (obs::synthdefs::table()[i].id);
    for (int i = 0; i < obs::harmdefs::gCount; ++i) p.harm[i] = g (obs::harmdefs::globals()[i].id);
    for (int v = 0; v < obs::harmdefs::kVoices; ++v)
        for (int i = 0; i < obs::harmdefs::vCount; ++i)
            p.harmV[v][i] = g ("harm.v" + juce::String (v + 1) + "." + obs::harmdefs::voiceDefs()[i].id);
    for (int i = 0; i < 16; ++i) synthSteps[i].store (obs::synthdefs::defaultStep (i));
    for (int r = 0; r < kNumRacks; ++r) racks[r].attach (params, r);
}

void Engine::prepare (double sampleRate, int block)
{
    sr = sampleRate; maxBlock = juce::jmax (16, block);
    ctxProto.sampleRate = sr; ctxProto.maxBlock = maxBlock; ctxProto.oversampling = oversampling.load(); ctxProto.pitch = &pitch;
    gate.prepare (sr); transposer.prepare (sr, maxBlock); pitch.prepare (sr); synth.prepare (sr);
    ampSection.prepare (sr, maxBlock); cabSection.prepare (sr, maxBlock); eq.prepare (sr);
    for (auto& r : racks) r.prepare (ctxProto);
    doubler.prepare (sr); width.prepare (sr); metronome.prepare (sr);
    harmonizer.prepare (sr, maxBlock); di.assign ((size_t) maxBlock, 0.f);
    // Re-prepare loaded captures for this rate.
    for (int c = 0; c < AmpSection::kChannels; ++c)
        if (auto* e = ampSection.channelSlot (c).get()) e->prepare (sr, maxBlock);
    if (auto* ca = ampSection.captureSlot().get()) for (auto& e : ca->engines) if (e) e->prepare (sr, maxBlock);
    inGainSm.reset (sr, 30.f, 1.f); outGainSm.reset (sr, 30.f, 1.f); tunerMute.reset (sr, 40.f, 1.f);
    mono.assign ((size_t) maxBlock, 0.f); scratchR.assign ((size_t) maxBlock, 0.f); synthL.assign ((size_t) maxBlock, 0.f); synthR.assign ((size_t) maxBlock, 0.f);
    dryL.assign ((size_t) maxBlock, 0.f); dryR.assign ((size_t) maxBlock, 0.f); synthG.assign ((size_t) maxBlock, 0.f);
    cabFade.reset (sr, p.cabOn == nullptr || p.cabOn->load() > 0.5f); eqFade.reset (sr, p.eqOn == nullptr || p.eqOn->load() > 0.5f);
    synthFade.reset (sr, p.synthOn != nullptr && p.synthOn->load() > 0.5f);
    cabWasActive = cabFade.active(); eqWasActive = eqFade.active();
    prepared = true;
}

void Engine::release()
{
    collectGarbage (true);
}

void Engine::collectGarbage (bool force)
{
    ampSection.collectGarbage (force);
    for (auto& r : racks) r.collectGarbage (force);
}

int Engine::latencySamples()
{
    int l = ampSection.latency() + cabSection.latency() + transposer.latency (p.transposeHq->load() > 0.5f);
    l += ampSection.namLatency ((int) p.ampChannel->load());
    for (auto& r : racks) l += r.latency();
    return l;
}

void Engine::readSynthParams (SynthParams& sp, const HostInfo& h)
{
    for (int i = 0; i < obs::synthdefs::count; ++i) sp.v[i] = p.syn[i]->load (std::memory_order_relaxed);
    for (int i = 0; i < 16; ++i) sp.steps[i] = synthSteps[i].load (std::memory_order_relaxed);
    // The guitar notes follow the plugin's transpose.
    sp.transposeSemis = std::round (p.transpose->load (std::memory_order_relaxed));
    sp.bpm = h.bpm; sp.hostPlaying = h.playing; sp.ppq = h.ppq;
}

void Engine::readHarmonyParams (dsp::hydra::Params& hp)
{
    namespace hd = obs::harmdefs;
    auto v = [] (std::atomic<float>* a) { return a->load (std::memory_order_relaxed); };
    hp.on = v (p.harm[hd::on]) > 0.5f;
    hp.key = juce::jlimit (0, 11, (int) std::lround (v (p.harm[hd::key])));
    hp.mode = juce::jlimit (0, hd::kModes - 1, (int) std::lround (v (p.harm[hd::mode])));
    hp.track = (int) std::lround (v (p.harm[hd::track]));
    hp.place = (int) std::lround (v (p.harm[hd::place]));
    hp.hold = v (p.harm[hd::offkey]) > 0.5f;
    hp.human = v (p.harm[hd::human]) * 0.01f;
    hp.spread = v (p.harm[hd::spread]) * 0.01f;
    hp.mix = v (p.harm[hd::mix]) * 0.01f;
    hp.outDb = v (p.harm[hd::out]);
    for (int k = 0; k < hd::kVoices; ++k)
    {
        auto& o = hp.v[k]; auto* q = p.harmV[k];
        o.on = v (q[hd::vOn]) > 0.5f; o.smart = v (q[hd::vSmart]) > 0.5f;
        o.step = (int) std::lround (v (q[hd::vStep])); o.semi = (int) std::lround (v (q[hd::vSemi]));
        o.level = v (q[hd::vLevel]) * 0.01f; o.pan = v (q[hd::vPan]); o.detune = v (q[hd::vDetune]); o.delayMs = v (q[hd::vDelay]);
    }
    hp.transpose = (float) std::lround (v (p.transpose));
}

void Engine::process (juce::AudioBuffer<float>& buffer, int numIn, const HostInfo& hostIn, int numOut)
{
    const auto t0 = juce::Time::getHighResolutionTicks();
    const int total = buffer.getNumSamples();
    if (! prepared || buffer.getNumChannels() < 1) { buffer.clear(); return; }

    HostInfo host = hostIn;
    if (! host.hasHostTempo) host.bpm = internalBpm.load();
    lastBpm.store (host.bpm);
    ctxProto.bpm = host.bpm; ctxProto.hostPlaying = host.playing; ctxProto.ppq = host.ppq;

    auto v = [] (std::atomic<float>* a) { return a->load (std::memory_order_relaxed); };
    const bool stereoOut = buffer.getNumChannels() >= 2 && numOut >= 2;
    const bool stereoIn = buffer.getNumChannels() >= 2 && numIn >= 2;

    // Notes from the UI (AUDITION / computer keys) play at the start of this block, before any MIDI.
    {
        int s1, n1, s2, n2; uiNotes.prepareToRead (uiNotes.getNumReady(), s1, n1, s2, n2);
        const int ui = juce::jmin (n1 + n2, kMaxSynthNotes - numSynthNotes);
        if (ui > 0)
        {
            std::memmove (synthNotes + ui, synthNotes, sizeof (SynthNote) * (size_t) numSynthNotes);
            for (int k = 0; k < ui; ++k)
            {
                const int c = uiNoteBuf[(size_t) (k < n1 ? s1 + k : s2 + (k - n1))];
                synthNotes[k] = c >= 0 ? SynthNote { 0, c, 0.85f } : SynthNote { 0, -1 - c, 0.f };
            }
            numSynthNotes += ui;
        }
        uiNotes.finishedRead (n1 + n2);
    }

    // Process in chunks no larger than the prepared block size (hosts sometimes send more).
    for (int start = 0; start < total; start += maxBlock)
    {
        const int n = juce::jmin (maxBlock, total - start);
        float* L = buffer.getWritePointer (0, start);
        float* R = stereoOut ? buffer.getWritePointer (1, start) : nullptr;
        const float* inR = stereoIn ? buffer.getReadPointer (1, start) : nullptr;
        float* x = mono.data();

        // ---- input select ------------------------------------------------------------
        const int mode = inputMode.load();
        // sum of both inputs, unless they're the same signal (a mono track in a DAW arrives on both
        // channels: adding them would make it 6 dB hotter than the same guitar in the app)
        if (inR != nullptr && mode == 0 && std::equal (L, L + n, inR)) std::copy (L, L + n, x);
        else if (inR != nullptr && mode == 0) for (int i = 0; i < n; ++i) x[i] = L[i] + inR[i];
        else if (inR != nullptr && mode == 2) std::copy (inR, inR + n, x);
        else if (numIn >= 1)              std::copy (L, L + n, x);
        else                              std::fill (x, x + n, 0.f);

        inGainSm.set (dbToGain (v (p.inGain) + inputCalibrationDb.load()));
        for (int i = 0; i < n; ++i) { float s = x[i] * inGainSm.next(); x[i] = std::isfinite (s) ? s : 0.f; }
        inMeter.process (x, n);
        pitch.process (x, n);
        std::copy (x, x + n, di.data());                // the clean guitar: HYDRA listens to this

        // HYDRA: where it sits (a move fades it out, switches, fades it back in)
        dsp::hydra::Params hp; readHarmonyParams (hp);
        if (hp.place != harmPlace && ! harmMoving) harmMoving = true;
        if (harmMoving)
        {
            if (harmonizer.isQuiet()) { harmPlace = hp.place; harmMoving = false; harmonizer.reset(); }
            else hp.on = false;
        }

        // ---- gate + transpose ------------------------------------------------------------
        gate.process (x, n, v (p.gateThresh), v (p.gateOn) > 0.5f);
        transposer.process (x, n, (int) std::round (v (p.transpose)), v (p.transposeHq) > 0.5f);

        // ---- synth ------------------------------------------------------------------------
        synthFade.set (v (p.synthOn) > 0.5f);
        const bool synthOn = synthFade.active();             // on, or fading in / out (no click)
        const bool synthPost = v (p.syn[obs::synthdefs::post]) > 0.5f;
        float synthMix = 0.f;
        // this chunk's notes, with offsets relative to the chunk
        SynthNote chunkNotes[kMaxSynthNotes]; int numChunk = 0;
        for (int e = 0; e < numSynthNotes; ++e)
            if (synthNotes[e].offset >= start && synthNotes[e].offset < start + n)
                chunkNotes[numChunk++] = { synthNotes[e].offset - start, synthNotes[e].note, synthNotes[e].vel };
        if (! synthOn) synth.keysWhileOff (chunkNotes, numChunk);
        if (synthOn)
        {
            SynthParams sp; readSynthParams (sp, host);
            synth.process (x, synthL.data(), synthR.data(), n, sp, pitch, chunkNotes, numChunk);
            synthMix = sp.v[obs::synthdefs::mix] * 0.01f;
            synthFade.gains (synthG.data(), n);
            for (int i = 0; i < n; ++i) synthG[(size_t) i] *= synthMix;
            if (! synthPost)
                for (int i = 0; i < n; ++i) { const float m = synthG[(size_t) i]; x[i] = x[i] * (1.f - m) + 0.5f * (synthL[(size_t) i] + synthR[(size_t) i]) * m; }
        }

        // ---- pedals (mono) ----------------------------------------------------------------
        {
            float* chans[1] = { x };
            racks[0].process (chans, 1, n, ctxProto, v (p.preOn) > 0.5f);
        }

        // ---- HYDRA before the amp (the voices go through it) ---------------------------------
        if (harmPlace == 1) harmonizer.process (di.data(), x, nullptr, x, nullptr, n, hp);

        // ---- amp ----------------------------------------------------------------------------
        ampSection.process (x, n, v (p.ampOn) > 0.5f, (int) std::round (v (p.ampChannel)), v (p.ampGain), v (p.ampBass), v (p.ampMid),
                            v (p.ampTreble), v (p.ampPresence), v (p.ampDepth), v (p.ampMaster));

        // ---- cab (mono -> stereo) ------------------------------------------------------------
        CabSection::MicParams mp[2];
        for (int m = 0; m < 2; ++m)
        {
            auto& s = p.mic[m];
            mp[m] = { (int) v (s.model), v (s.pos), v (s.dist), v (s.level), v (s.pan), v (s.solo) > 0.5f, v (s.mute) > 0.5f, v (s.phase) > 0.5f };
        }
        float* outR = R != nullptr ? R : scratchR.data();   // mono output: render R into scratch and fold at the end
        // capture amps whose captures already contain a cab bypass the cab section (crossfaded, no click)
        cabFade.set (v (p.cabOn) > 0.5f && ! ampSection.cabInside());
        if (cabFade.active())
        {
            if (! cabWasActive) cabSection.reset();           // coming back on: no old tail from before
            cabSection.process (x, L, outR, n, mp[0], mp[1], v (p.cabLc), v (p.cabHc), true);
            if (! cabFade.full()) cabFade.mix (x, x, L, outR, n);
        }
        else { std::copy (x, x + n, L); std::copy (x, x + n, outR); }
        cabWasActive = cabFade.active();

        // ---- synth post-amp ----------------------------------------------------------------
        if (synthOn && synthPost)
            for (int i = 0; i < n; ++i)
            {
                const float m = synthG[(size_t) i];
                L[i] = L[i] * (1.f - m) + synthL[(size_t) i] * m;
                outR[i] = outR[i] * (1.f - m) + synthR[(size_t) i] * m;
            }

        // ---- HYDRA after the amp: harmonies of the amp's sound, like a second guitarist -------
        if (harmPlace == 0) harmonizer.process (di.data(), L, outR, L, outR, n, hp);

        float* st[2] = { L, outR };

        // ---- EQ ---------------------------------------------------------------------------
        eqFade.set (v (p.eqOn) > 0.5f);
        if (eqFade.active())
        {
            const bool full = eqFade.full();
            if (! eqWasActive) eq.reset();
            if (! full) { std::copy (L, L + n, dryL.data()); std::copy (outR, outR + n, dryR.data()); }
            for (int b = 0; b < kEqBands; ++b) eq.setBand (b, kEqFreqs[b], v (p.eqBand[b]));
            eq.setCuts (v (p.eqLc), v (p.eqHc), v (p.eqLcOn) > 0.5f, v (p.eqHcOn) > 0.5f);
            eq.process (st, 2, n);
            if (! full) eqFade.mix (dryL.data(), dryR.data(), L, outR, n);
        }
        eqWasActive = eqFade.active();

        // ---- FX, special FX ------------------------------------------------------------------
        racks[1].process (st, 2, n, ctxProto, v (p.postOn) > 0.5f);
        racks[2].process (st, 2, n, ctxProto, v (p.sfxOn) > 0.5f);

        // ---- doubler, width, output ------------------------------------------------------------
        doubler.process (L, outR, n, v (p.doublerTime), v (p.doublerOn) > 0.5f);
        width.process (L, outR, n, v (p.width));
        outGainSm.set (dbToGain (v (p.outGain)));
        tunerMute.set (tunerOpen.load() && muteWhileTuning.load() ? 0.f : 1.f);
        for (int i = 0; i < n; ++i)
        {
            const float g = outGainSm.next() * tunerMute.next();
            float a = L[i] * g, b = outR[i] * g;
            if (! std::isfinite (a)) a = 0.f;
            if (! std::isfinite (b)) b = 0.f;
            // Last line of defence for your ears and speakers: soft ceiling just above 0 dBFS.
            L[i] = juce::jlimit (-2.f, 2.f, a); outR[i] = juce::jlimit (-2.f, 2.f, b);
        }

        // ---- STUDIO: records what you hear, then its takes and its click go in ------------------
        if (auto* rec = studio.load (std::memory_order_acquire)) rec->process (L, outR, n);

        // ---- metronome ------------------------------------------------------------------------
        metronome.setRunning (metroRunning.load());
        metronome.process (L, outR, n, host.bpm, beatsPerBar.load(), accentFirst.load(), metronomeVolume.load());

        if (R == nullptr) for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + outR[i]);
        outMeterL.process (L, n); outMeterR.process (outR, n);
    }
    // Any extra output channels: silence (with a mono output, channel 1 only held the right input).
    for (int c = stereoOut ? 2 : 1; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, total);
    numSynthNotes = 0;          // this block's synth notes are used up

    // CPU meter: share of the real-time budget this block used.
    const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
    const double budget = total / sr;
    const float load = (float) (budget > 0 ? secs / budget : 0.0);
    cpu.store (cpu.load() * 0.9f + load * 0.1f);
}
} // namespace obs
