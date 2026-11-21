#include "Engine.h"

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
    p.eqLc = g ("eq.lc"); p.eqHc = g ("eq.hc");
    for (int m = 0; m < 2; ++m)
    {
        const juce::String k = "cab." + juce::String (m == 0 ? "a" : "b") + ".";
        p.mic[m] = { g (k + "mic"), g (k + "pos"), g (k + "dist"), g (k + "level"), g (k + "pan"), g (k + "solo"), g (k + "mute"), g (k + "phase") };
    }
    p.cabLc = g ("cab.lc"); p.cabHc = g ("cab.hc");
    for (int i = 0; i < obs::synthdefs::count; ++i) p.syn[i] = g (obs::synthdefs::table()[i].id);
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
    // Re-prepare loaded captures for this rate.
    for (int c = 0; c < AmpSection::kChannels; ++c)
        if (auto* e = ampSection.channelSlot (c).get()) e->prepare (sr, maxBlock);
    inGainSm.reset (sr, 30.f, 1.f); outGainSm.reset (sr, 30.f, 1.f); tunerMute.reset (sr, 40.f, 1.f);
    mono.assign ((size_t) maxBlock, 0.f); scratchR.assign ((size_t) maxBlock, 0.f); synthL.assign ((size_t) maxBlock, 0.f); synthR.assign ((size_t) maxBlock, 0.f);
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

void Engine::process (juce::AudioBuffer<float>& buffer, int numIn, const HostInfo& hostIn)
{
    const auto t0 = juce::Time::getHighResolutionTicks();
    const int total = buffer.getNumSamples();
    if (! prepared || buffer.getNumChannels() < 1) { buffer.clear(); return; }

    HostInfo host = hostIn;
    if (! host.hasHostTempo) host.bpm = internalBpm.load();
    lastBpm.store (host.bpm);
    ctxProto.bpm = host.bpm; ctxProto.hostPlaying = host.playing; ctxProto.ppq = host.ppq;

    auto v = [] (std::atomic<float>* a) { return a->load (std::memory_order_relaxed); };
    const bool stereoOut = buffer.getNumChannels() >= 2;

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
        float* x = mono.data();

        // ---- input select ------------------------------------------------------------
        const int mode = inputMode.load();
        if (numIn >= 2 && mode == 0)      for (int i = 0; i < n; ++i) x[i] = L[i] + R[i];
        else if (numIn >= 2 && mode == 2) std::copy (R, R + n, x);
        else if (numIn >= 1)              std::copy (L, L + n, x);
        else                              std::fill (x, x + n, 0.f);

        inGainSm.set (dbToGain (v (p.inGain) + inputCalibrationDb.load()));
        for (int i = 0; i < n; ++i) { float s = x[i] * inGainSm.next(); x[i] = std::isfinite (s) ? s : 0.f; }
        inMeter.process (x, n);
        pitch.process (x, n);

        // ---- gate + transpose ------------------------------------------------------------
        gate.process (x, n, v (p.gateThresh), v (p.gateOn) > 0.5f);
        transposer.process (x, n, (int) std::round (v (p.transpose)), v (p.transposeHq) > 0.5f);

        // ---- synth ------------------------------------------------------------------------
        const bool synthOn = v (p.synthOn) > 0.5f;
        const bool synthPost = v (p.syn[obs::synthdefs::post]) > 0.5f;
        float synthMix = 0.f;
        if (synthOn)
        {
            SynthParams sp; readSynthParams (sp, host);
            // this chunk's notes, with offsets relative to the chunk
            SynthNote chunkNotes[64]; int numChunk = 0;
            for (int e = 0; e < numSynthNotes && numChunk < 64; ++e)
                if (synthNotes[e].offset >= start && synthNotes[e].offset < start + n)
                    chunkNotes[numChunk++] = { synthNotes[e].offset - start, synthNotes[e].note, synthNotes[e].vel };
            synth.process (x, synthL.data(), synthR.data(), n, sp, pitch, chunkNotes, numChunk);
            synthMix = sp.v[obs::synthdefs::mix] * 0.01f;
            if (! synthPost)
                for (int i = 0; i < n; ++i) x[i] = x[i] * (1.f - synthMix) + 0.5f * (synthL[(size_t) i] + synthR[(size_t) i]) * synthMix;
        }

        // ---- pedals (mono) ----------------------------------------------------------------
        {
            float* chans[1] = { x };
            racks[0].process (chans, 1, n, ctxProto, v (p.preOn) > 0.5f);
        }

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
        cabSection.process (x, L, outR, n, mp[0], mp[1], v (p.cabLc), v (p.cabHc), v (p.cabOn) > 0.5f);

        // ---- synth post-amp ----------------------------------------------------------------
        if (synthOn && synthPost)
            for (int i = 0; i < n; ++i)
            {
                L[i] = L[i] * (1.f - synthMix) + synthL[(size_t) i] * synthMix;
                outR[i] = outR[i] * (1.f - synthMix) + synthR[(size_t) i] * synthMix;
            }

        float* st[2] = { L, outR };

        // ---- EQ ---------------------------------------------------------------------------
        if (v (p.eqOn) > 0.5f)
        {
            for (int b = 0; b < kEqBands; ++b) eq.setBand (b, kEqFreqs[b], v (p.eqBand[b]));
            eq.setCuts (v (p.eqLc), v (p.eqHc));
            eq.process (st, 2, n);
        }

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

        // ---- metronome ------------------------------------------------------------------------
        metronome.setRunning (metroRunning.load());
        metronome.process (L, outR, n, host.bpm, beatsPerBar.load(), accentFirst.load(), metronomeVolume.load());

        if (R == nullptr) for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + outR[i]);
        outMeterL.process (L, n); outMeterR.process (outR, n);
    }
    // Any extra output channels: silence.
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, total);
    numSynthNotes = 0;          // this block's synth notes are used up

    // CPU meter: share of the real-time budget this block used.
    const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
    const double budget = total / sr;
    const float load = (float) (budget > 0 ? secs / budget : 0.0);
    cpu.store (cpu.load() * 0.9f + load * 0.1f);
}
} // namespace obs
