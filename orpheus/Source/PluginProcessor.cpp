#include "PluginProcessor.h"
#if ! ORPHEUS_NO_EDITOR
 #include "PluginEditor.h"
#endif

namespace orph
{
#include "Seq/DefaultSong.inc"

static int64_t msNow() { return (int64_t) juce::Time::getMillisecondCounterHiRes(); }

OrpheusProcessor::OrpheusProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ORPHEUS", makeLayout())
{
    for (int i = 0; i < P_COUNT; ++i) raw[(size_t) i] = apvts.getRawParameterValue (paramDefs()[i].id);
    song = Song::fromJson (kDefaultSong, juce::JSON::parse (kDefaultMarks));
    if (song == nullptr) song = std::make_unique<Song>();
    uiExtras = juce::var (new juce::DynamicObject());
}

bool OrpheusProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void OrpheusProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    seqSyn.prepare (sr); keySyn.prepare (sr);
    fx.prepare (sr, std::max (64, std::min (2048, samplesPerBlock)));
    bl.assign ((size_t) std::max (64, samplesPerBlock), 0.f); br.assign ((size_t) std::max (64, samplesPerBlock), 0.f);
    first = true; sndN = 0; kickMark = -2;
}

void OrpheusProcessor::takeSnapshot (Snapshot& s) const noexcept
{
    for (int i = 0; i < P_COUNT; ++i) s.v[(size_t) i] = raw[(size_t) i]->load (std::memory_order_relaxed);
}

/* ------------------------------------------------------------------ commands from the UI (lock-free into the audio thread) */
void OrpheusProcessor::push (const Cmd& c)
{
    const juce::ScopedLock l (pushLock);
    int s1, n1, s2, n2; fifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0) cmds[(size_t) s1] = c; else if (n2 > 0) cmds[(size_t) s2] = c; else return;
    fifo.finishedWrite (1);
}
void OrpheusProcessor::transport (const juce::String& what, double at)
{
    Cmd c {}; c.type = what == "play" ? Cmd::Play : what == "stop" ? Cmd::Stop : Cmd::Seek; c.ph = at; push (c);
}
void OrpheusProcessor::audition (const float* notes, int n, float vel, bool hold)
{
    Cmd c {}; c.type = Cmd::Audition; c.n = juce::jlimit (0, 4, n); for (int i = 0; i < c.n; ++i) c.notes[i] = notes[i]; c.vel = vel; c.hold = hold; push (c);
}
void OrpheusProcessor::auditionPitches (const float* notes, int n)
{
    Cmd c {}; c.type = Cmd::AudPitch; c.n = juce::jlimit (0, 4, n); for (int i = 0; i < c.n; ++i) c.notes[i] = notes[i]; push (c);
}
void OrpheusProcessor::auditionOff() { Cmd c {}; c.type = Cmd::AudOff; push (c); }
void OrpheusProcessor::uiNote (int note, bool on, float vel)
{
    Cmd c {}; c.type = on ? Cmd::KeyOn : Cmd::KeyOff; c.n = 1; c.notes[0] = (float) juce::jlimit (0, 127, note); c.vel = vel; push (c);
}

void OrpheusProcessor::setParamFromUi (int index, float realValue, bool noTrails)
{
    if (index < 0 || index >= P_COUNT) return;
    if (noTrails) noTrailsUntil = msNow() + 250;
    if (auto* p = apvts.getParameter (paramDefs()[index].id))
    {
        const float n = p->convertTo0to1 (realValue);
        if (std::abs (p->getValue() - n) > 1e-7f) { p->beginChangeGesture(); p->setValueNotifyingHost (n); p->endChangeGesture(); }
    }
}

void OrpheusProcessor::setSong (const juce::String& json, const juce::var& marks)
{
    auto s = Song::fromJson (json, marks);
    if (s == nullptr) return;
    { const juce::SpinLock::ScopedLockType l (songLock); std::swap (song, s); }
    // (s now holds the old song: freed here, on the message thread)
}
juce::String OrpheusProcessor::songJson() const
{
    const juce::SpinLock::ScopedLockType l (songLock);
    return song ? song->json : juce::String();
}

void OrpheusProcessor::runCommands()
{
    int s1, n1, s2, n2; const int ready = fifo.getNumReady();
    if (ready == 0) return;
    fifo.prepareToRead (ready, s1, n1, s2, n2);
    auto run = [this] (const Cmd& c)
    {
        switch (c.type)
        {
            case Cmd::Audition: if (c.n > 0) { keySyn.noteOn (c.notes, c.n, c.vel, false, c.hold, S); auditioning = true; } break;
            case Cmd::AudPitch: if (auditioning && c.n > 0) keySyn.setPitches (c.notes, c.n, .03, S); break;
            case Cmd::AudOff: if (auditioning) { auditioning = false; if (nHeld == 0) keySyn.noteOff (S); } break;
            case Cmd::KeyOn: if (held[(size_t) c.notes[0]] == 0) { held[(size_t) c.notes[0]] = std::max (.01f, c.vel); ++nHeld; } keysChanged (c.vel, true); break;
            case Cmd::KeyOff: if (held[(size_t) c.notes[0]] > 0) { held[(size_t) c.notes[0]] = 0; --nHeld; } keysChanged (lastVel, false); break;
            case Cmd::Play: if (! followingHost) { ph = std::max (0.0, c.ph); playing = true; seqSoundOff(); kickMark = -2; } break;
            case Cmd::Stop: if (! followingHost) { playing = false; seqSoundOff(); } break;
            case Cmd::Seek: if (! followingHost) { ph = std::max (0.0, c.ph); if (playing) seqSoundOff(); } break;
        }
    };
    for (int i = 0; i < n1; ++i) run (cmds[(size_t) (s1 + i)]);
    for (int i = 0; i < n2; ++i) run (cmds[(size_t) (s2 + i)]);
    fifo.finishedRead (n1 + n2);
}

/* ------------------------------------------------------------------ keys: the held notes ring together as one chord */
void OrpheusProcessor::keysChanged (float vel, bool pressed)
{
    float notes[16]; int n = 0;
    for (int m = 0; m < 128 && n < 16; ++m) if (held[(size_t) m] > 0) notes[n++] = (float) m;
    if (n == 0) { if (! auditioning) keySyn.noteOff (S); return; }
    if (pressed) { lastVel = vel; keySyn.noteOn (notes, n, vel, S.on (P_legato), false, S); }
    else keySyn.setPitches (notes, n, 0, S);           // a key let go: the rest keep ringing (no new attack)
}

void OrpheusProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        const int k = m.getNoteNumber();
        if (held[(size_t) k] == 0) ++nHeld;
        held[(size_t) k] = std::max (.01f, m.getFloatVelocity());
        keysChanged (m.getFloatVelocity(), true);
    }
    else if (m.isNoteOff())
    {
        const int k = m.getNoteNumber();
        if (held[(size_t) k] > 0) { held[(size_t) k] = 0; --nHeld; keysChanged (lastVel, false); }
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        held.fill (0); nHeld = 0; keySyn.noteOff (S);
    }
    else if (m.isPitchWheel()) midiBend = (float) ((m.getPitchWheelValue() - 8192) / 8192.0 * 2.0);
    else if (m.isControllerOfType (1)) midiMod = m.getControllerValue() / 127.f;
}

/* ------------------------------------------------------------------ the player (orpheus.js frame()) */
void OrpheusProcessor::seqSoundOff()
{
    if (sndN) { seqSyn.noteOff (S); sndN = 0; }
}

void OrpheusProcessor::stepSequencer (int n, double hostPh, bool hostPlay)
{
    const juce::SpinLock::ScopedTryLockType lock (songLock);
    if (! lock.isLocked() || song == nullptr) return;            // (a new song is being swapped in: next block)
    const Song& sg = *song;
    const double len = sg.length();
    if (hostPlay)
    {
        if (! followingHost) { followingHost = true; playing = true; seqSoundOff(); kickMark = -2; }
        double p = std::max (0.0, hostPh);
        if (sg.loop) p = std::fmod (p, len);
        if (p < ph - 1e-6 && sndN) seqSoundOff();                 // the DAW jumped back (or looped): play it fresh
        ph = p;
        if (! sg.loop && p >= len) { seqSoundOff(); return; }
    }
    else
    {
        if (followingHost) { followingHost = false; playing = false; seqSoundOff(); }
        if (! playing) return;
        double p = ph + n / sr * sg.bpm / 60.0;
        if (p >= len)
        {
            if (! sg.loop) { ph = len; playing = false; seqSoundOff(); return; }
            p = std::fmod (p, len); seqSoundOff();
        }
        ph = p;
    }
    float now[4]; const int cnt = sg.sounding (ph, now);
    if (cnt == 0) seqSoundOff();
    else if (sndN != cnt) { seqSyn.noteOn (now, cnt, .8f, false, true, S); sndN = cnt; }
    else seqSyn.setPitches (now, cnt, .03, S);
    const int km = sg.markAt (ph);
    if (km != kickMark)
    {
        kickMark = km;
        if (km >= 0 && cnt) { float lo = now[0]; for (int i = 1; i < cnt; ++i) lo = std::min (lo, now[i]); seqSyn.filterKick (lo, S); }
    }
}

void OrpheusProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    for (int c = 0; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    if ((int) bl.size() < n) { bl.resize ((size_t) n); br.resize ((size_t) n); }   // (a host that sends more than it said)

    takeSnapshot (S);
    if (first) { prevS = S; first = false; }
    else if (S.v != prevS.v) { seqSyn.paramsChanged (S, prevS); keySyn.paramsChanged (S, prevS); prevS = S; }

    // the DAW's transport
    bool hostPlay = false; double hostPpq = 0, hostBpm = 0;
    if (! juce::JUCEApplicationBase::isStandaloneApp())
        if (auto* phd = getPlayHead())
            if (auto pos = phd->getPosition())
            {
                if (auto b = pos->getBpm()) hostBpm = *b;
                hostPlay = pos->getIsPlaying() && pos->getPpqPosition().hasValue();
                if (hostPlay) hostPpq = *pos->getPpqPosition();
            }
    double songBpm = 80;
    { const juce::SpinLock::ScopedTryLockType l (songLock); if (l.isLocked() && song) songBpm = song->bpm; }
    const double bpm = hostPlay && hostBpm > 0 ? hostBpm : songBpm;
    seqSyn.setBpm (bpm); keySyn.setBpm (bpm);
    const float bend = midiBend + uiBend.load(), mod = juce::jlimit (0.f, 1.f, midiMod + uiMod.load());
    seqSyn.setWheels (bend, mod); keySyn.setWheels (bend, mod);
    fx.setNoTrails (msNow() < noTrailsUntil.load());

    runCommands();
    if (rollWant.exchange (false)) { rolling = true; rollAcc = 0; rollN = 0; rollSkip = (int64_t) (sr * .18); }

    std::fill (bl.begin(), bl.begin() + n, 0.f); std::fill (br.begin(), br.begin() + n, 0.f);
    auto mi = midi.begin();
    constexpr int CH = 32;
    for (int off = 0; off < n; off += CH)
    {
        const int len = std::min (CH, n - off);
        for (; mi != midi.end() && (*mi).samplePosition < off + len; ++mi) handleMidi ((*mi).getMessage());
        stepSequencer (len, hostPpq + off / sr * (hostBpm > 0 ? hostBpm : bpm) / 60.0, hostPlay);
        if (seqSyn.active()) seqSyn.render (S, bl.data() + off, br.data() + off, len);
        if (keySyn.active()) keySyn.render (S, bl.data() + off, br.data() + off, len);
    }
    // a dice roll's level: the voice (before the effects) while it plays, past the first attack
    if (rolling && (seqSyn.gate() || keySyn.gate()))
    {
        for (int s = 0; s < n; ++s)
        {
            if (rollSkip > 0) { --rollSkip; continue; }
            const double m = (bl[(size_t) s] + br[(size_t) s]) * .5; rollAcc += m * m; ++rollN;   // (mono, like the preview's analyser)
        }
        if (rollN > (int64_t) (sr * .62))
        {
            const double g = dbToGain (S[P_level] + S[P_trim]);
            const double db = 20 * std::log10 (std::sqrt (rollAcc / (double) rollN) / g + 1e-9);
            rolling = false;
            if (db > -90) rollResult = (float) juce::jlimit (-12.0, 12.0, std::round (2 * (rollTarget.load() - db)) / 2);
        }
    }
    fx.process (S, bpm, bl.data(), br.data(), n);

    if (buffer.getNumChannels() >= 2) { buffer.copyFrom (0, 0, bl.data(), n); buffer.copyFrom (1, 0, br.data(), n); }
    else if (buffer.getNumChannels() == 1) { for (int s = 0; s < n; ++s) bl[(size_t) s] = (bl[(size_t) s] + br[(size_t) s]) * .5f; buffer.copyFrom (0, 0, bl.data(), n); }

    posPh = ph; posPlaying = playing; posHost = followingHost; posBpm = bpm;
}

/* ------------------------------------------------------------------ state: every setting, the song, the UI's extras */
void OrpheusProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    juce::ValueTree s ("SONG"); s.setProperty ("json", songJson(), nullptr);
    { const juce::SpinLock::ScopedLockType l (songLock);
      juce::Array<juce::var> mk; if (song) for (double t : song->marks) mk.add (t);
      s.setProperty ("marks", juce::JSON::toString (juce::var (mk), true), nullptr); }
    st.removeChild (st.getChildWithName ("SONG"), nullptr); st.appendChild (s, nullptr);
    st.removeChild (st.getChildWithName ("UIX"), nullptr);
    juce::ValueTree u ("UIX"); u.setProperty ("json", juce::JSON::toString (uiExtras, true), nullptr); st.appendChild (u, nullptr);
    st.setProperty ("version", 1, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void OrpheusProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto st = juce::ValueTree::fromXml (*xml);
    if (! st.hasType (apvts.state.getType())) return;
    auto s = st.getChildWithName ("SONG"), u = st.getChildWithName ("UIX");
    if (s.isValid()) { const auto j = s["json"].toString(); if (j.isNotEmpty()) setSong (j, juce::JSON::parse (s["marks"].toString())); }
    if (u.isValid()) { auto v = juce::JSON::parse (u["json"].toString()); if (v.isObject()) uiExtras = v; }
    st.removeChild (s, nullptr); st.removeChild (u, nullptr);
    apvts.replaceState (st);
    ++stateGen;
}

juce::AudioProcessorEditor* OrpheusProcessor::createEditor()
{
   #if ORPHEUS_NO_EDITOR
    return nullptr;
   #else
    return new OrpheusEditor (*this);
   #endif
}
} // namespace orph

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new orph::OrpheusProcessor(); }
