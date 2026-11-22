#include "Studio.h"
#include "Rig.h"
#include <algorithm>
#include <climits>
#include <cmath>

namespace obs
{
namespace
{
/** Writes stereo float audio as a 24-bit WAV. */
bool writeStereoWav (const juce::File& f, const float* L, const float* R, juce::int64 n, double rate, juce::String& error)
{
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    if (os == nullptr) { error = "Couldn't write " + f.getFullPathName(); return false; }
    juce::WavAudioFormat wav;
    auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (rate).withNumChannels (2).withBitsPerSample (24));
    if (w == nullptr) { error = "Couldn't make a WAV file"; return false; }
    const int chunk = 65536;
    std::vector<float> a ((size_t) chunk), b ((size_t) chunk);
    for (juce::int64 at = 0; at < n; at += chunk)
    {
        const int k = (int) std::min<juce::int64> (chunk, n - at);
        for (int i = 0; i < k; ++i) { a[(size_t) i] = juce::jlimit (-1.f, 1.f, L[at + i]); b[(size_t) i] = juce::jlimit (-1.f, 1.f, R[at + i]); }
        const float* ch[2] = { a.data(), b.data() };
        if (! w->writeFromFloatArrays (ch, 2, k)) { error = "Couldn't write the WAV file"; return false; }
    }
    return true;
}
} // namespace

Studio::Studio()            { startTimer (25); }
Studio::~Studio()           { stopTimer(); }

juce::File Studio::recordingsFolder() { return Rig::contentFolder().getChildFile ("Recordings"); }

float Studio::peakShape (float p)
{
    if (p <= 1.0e-5f) return 0.f;
    return juce::jlimit (0.f, 1.f, (20.f * std::log10 (p) + 48.f) / 48.f);
}

void Studio::prepare (double s)
{
    // before audio runs (prepareToPlay): the FIFO can change size here
    playing = false; recording = false;
    if (current != nullptr) finishTake();
    sr = s > 0 ? s : 48000;
    const int cap = (int) (sr * 10) + 1;        // 10 s: the timer empties it every 25 ms
    fifoL.assign ((size_t) cap, 0.f); fifoR.assign ((size_t) cap, 0.f);
    fifo.setTotalSize (cap); fifo.reset();
    if (lastArr.isObject()) setArrangement (lastArr);
}

std::shared_ptr<const Studio::Take> Studio::take (int id) const
{
    const juce::ScopedLock sl (takesLock);
    auto it = takes.find (id);
    return it == takes.end() ? nullptr : it->second;
}

int Studio::addTake (std::vector<float> L, std::vector<float> R, const juce::String& name)
{
    auto t = std::make_shared<Take>();
    t->id = nextId++; t->L = std::move (L); t->R = std::move (R); t->rate = sr; t->bpm = bpm.load(); t->name = name;
    const juce::ScopedLock sl (takesLock);
    takes[t->id] = t;
    return t->id;
}

std::unique_ptr<Studio::Arr> Studio::build (const juce::var& a) const
{
    auto r = std::make_unique<Arr>();
    r->bpm = juce::jlimit (20.0, 400.0, a.hasProperty ("bpm") ? (double) a["bpm"] : 120.0);
    const double s = sr * 60.0 / r->bpm;
    if (auto* tr = a["tracks"].getArray())
        for (auto& t : *tr)
        {
            const float g = juce::jlimit (0.f, 1.f, (float) t["vol"]) / 0.8f;      // 0.8 on the fader = 0 dB
            const float pan = juce::jlimit (-1.f, 1.f, (float) t["pan"]);
            const bool on = t.hasProperty ("on") ? (bool) t["on"] : true;
            r->gL.push_back (on ? g * juce::jmin (1.f, 1.f - pan) : 0.f);
            r->gR.push_back (on ? g * juce::jmin (1.f, 1.f + pan) : 0.f);
            r->mono.push_back (t.hasProperty ("st") && ! (bool) t["st"] ? 1 : 0);
        }
    if (auto* cl = a["clips"].getArray())
        for (auto& c : *cl)
        {
            auto src = take ((int) c["src"]);
            const int track = (int) c["track"];
            if (src == nullptr || track < 0 || track >= (int) r->gL.size()) continue;
            Clip x; x.src = src; x.track = track;
            x.start = (juce::int64) std::llround ((double) c["start"] * s);
            x.off = std::max<juce::int64> (0, (juce::int64) std::llround ((double) c["off"] * s));
            x.len = std::min<juce::int64> ((juce::int64) std::llround ((double) c["len"] * s), (juce::int64) src->L.size() - x.off);
            if (x.len > 0) r->clips.push_back (x);
        }
    const auto lp = a["loop"];
    r->loop = lp.isObject() && (bool) lp["on"];
    r->loopS = (juce::int64) std::llround ((double) lp["s"] * s);
    r->loopE = (juce::int64) std::llround ((double) lp["e"] * s);
    if (r->loopE <= r->loopS) r->loop = false;
    r->click = (bool) a["click"];
    return r;
}

void Studio::setArrangement (const juce::var& a)
{
    lastArr = a;
    auto r = build (a);
    bpm = r->bpm;
    {
        const juce::SpinLock::ScopedLockType sl (arrLock);    // the audio thread only uses arr while holding it
        std::swap (arr, r);
    }
    r.reset();                                                // the old one goes here, not on the audio thread
}

void Studio::play (double fromBeat)
{
    if (recording) stop();
    pos = (juce::int64) std::llround (juce::jmax (0.0, fromBeat) * spb());
    ++seekGen;
    playing = true;
}

void Studio::record (int track, double fromBeat, double countInBeats)
{
    if (recording || finishing) { recording = false; finishTake(); }
    playing = false;
    current = std::make_unique<Take>();
    current->rate = sr; current->bpm = bpm.load();
    current->L.reserve ((size_t) (sr * 60)); current->R.reserve ((size_t) (sr * 60));
    currentTrack = track; currentStartBeat = juce::jmax (0.0, fromBeat);
    peakEvery = std::max<juce::int64> (1, (juce::int64) std::llround (spb() / 8.0));      // the page draws 8 peaks per beat
    peakCount = 0; peakMax = 0; currentPeaks.clear(); newPeaks.clear();
    fifo.reset(); dropped = 0;
    recStart = (juce::int64) std::llround (currentStartBeat * spb());
    recTrack = track;
    pos = recStart.load() - (juce::int64) std::llround (juce::jmax (0.0, countInBeats) * spb());
    ++seekGen;
    recording = true;
    playing = true;
}

void Studio::stop()
{
    playing = false;
    if (recording) { recording = false; finishing = true; finishTicks = 0; }   // the timer collects the last samples, then the take is done
}

void Studio::timerCallback()
{
    drain();
    if (finishing && ++finishTicks >= 2) finishTake();
}

void Studio::drain()
{
    const int ready = fifo.getNumReady();
    if (ready <= 0) return;
    int s1, n1, s2, n2;
    fifo.prepareToRead (ready, s1, n1, s2, n2);
    auto add = [this] (int start, int count)
    {
        if (current == nullptr) return;
        for (int i = 0; i < count; ++i)
        {
            const float l = fifoL[(size_t) (start + i)], r = fifoR[(size_t) (start + i)];
            current->L.push_back (l); current->R.push_back (r);
            peakMax = juce::jmax (peakMax, std::abs (l), std::abs (r));
            if (++peakCount >= peakEvery) { const float v = peakShape (peakMax); currentPeaks.push_back (v); newPeaks.push_back (v); peakCount = 0; peakMax = 0; }
        }
    };
    add (s1, n1); add (s2, n2);
    fifo.finishedRead (n1 + n2);
}

void Studio::finishTake()
{
    finishing = false;
    drain();
    if (current == nullptr) return;
    if (peakCount > 0) { const float v = peakShape (peakMax); currentPeaks.push_back (v); newPeaks.push_back (v); peakCount = 0; peakMax = 0; }
    auto o = new juce::DynamicObject();
    o->setProperty ("track", currentTrack);
    o->setProperty ("start", currentStartBeat);
    if ((double) current->L.size() < sr * 0.1)                // under 0.1 s: nothing worth keeping
    {
        o->setProperty ("discarded", true);
        current.reset();
        doneTake = juce::var (o);
        return;
    }
    // latency: what you played reached us roundTrip samples after you heard the beat
    const auto cut = (size_t) std::min<juce::int64> (roundTrip, (juce::int64) current->L.size() / 2);
    if (cut > 0)
    {
        current->L.erase (current->L.begin(), current->L.begin() + (std::ptrdiff_t) cut);
        current->R.erase (current->R.begin(), current->R.begin() + (std::ptrdiff_t) cut);
        currentPeaks.clear();
        for (size_t at = 0; at < current->L.size(); at += (size_t) peakEvery)
        {
            float m = 0;
            for (size_t k = at; k < juce::jmin (current->L.size(), at + (size_t) peakEvery); ++k) m = juce::jmax (m, std::abs (current->L[k]), std::abs (current->R[k]));
            currentPeaks.push_back (peakShape (m));
        }
    }
    current->id = nextId++;
    current->name = "TAKE " + juce::String (current->id);
    current->file = recordingsFolder().getChildFile ("Take " + juce::String (current->id) + " " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S") + ".wav");
    std::shared_ptr<const Take> t (current.release());
    { const juce::ScopedLock sl (takesLock); takes[t->id] = t; }
    if (saveTakes) writeWav (t);
    juce::Array<juce::var> pk; for (float v : currentPeaks) pk.add (v);
    o->setProperty ("id", t->id); o->setProperty ("name", t->name);
    o->setProperty ("len", (double) t->L.size() / spb());
    o->setProperty ("peaks", pk);
    o->setProperty ("file", t->file.getFullPathName());
    if (dropped.load() > 0) o->setProperty ("dropped", dropped.load());
    doneTake = juce::var (o);
}

void Studio::writeWav (std::shared_ptr<const Take> t)
{
    // in the background: a long take is a big file
    juce::Thread::launch ([t]
    {
        juce::String err;
        writeStereoWav (t->file, t->L.data(), t->R.data(), (juce::int64) t->L.size(), t->rate, err);
    });
}

juce::var Studio::status()
{
    auto o = new juce::DynamicObject();
    const bool pl = playing.load(), rc = recording.load();
    o->setProperty ("playing", pl);
    o->setProperty ("recording", rc);
    o->setProperty ("pos", (double) pos.load() / spb());
    bool extra = false;
    if (! newPeaks.empty()) { juce::Array<juce::var> pk; for (float v : newPeaks) pk.add (v); newPeaks.clear(); o->setProperty ("peaks", pk); extra = true; }
    if (doneTake.isObject()) { o->setProperty ("take", doneTake); doneTake = juce::var(); extra = true; }
    o->setProperty ("active", pl || rc || finishing || extra);
    return juce::var (o);
}

bool Studio::exportMix (const juce::File& dest, juce::String& error)
{
    auto a = build (lastArr);
    juce::int64 end = 0;
    for (auto& c : a->clips) if (a->gL[(size_t) c.track] != 0.f || a->gR[(size_t) c.track] != 0.f) end = std::max (end, c.start + c.len);
    if (end <= 0) { error = "Nothing to export yet: record a take first"; return false; }
    end += (juce::int64) (sr * 0.5);                       // a little tail
    std::vector<float> L ((size_t) end, 0.f), R ((size_t) end, 0.f);
    for (auto& c : a->clips)
    {
        const float gl = a->gL[(size_t) c.track], gr = a->gR[(size_t) c.track];
        const bool mono = a->mono[(size_t) c.track] != 0;
        const auto& t = *c.src;
        for (juce::int64 k = 0; k < c.len; ++k)
        {
            const auto si = (size_t) (c.off + k), di = (size_t) (c.start + k);
            if (si >= t.L.size() || di >= L.size()) break;
            float l = t.L[si], r = t.R[si];
            if (mono) l = r = 0.5f * (l + r);
            L[di] += l * gl; R[di] += r * gr;
        }
    }
    return writeStereoWav (dest, L.data(), R.data(), end, sr, error);
}

void Studio::process (float* L, float* R, int n) noexcept
{
    if (! playing.load (std::memory_order_acquire) && clickLeft <= 0) return;
    const int gen = seekGen.load();
    if (gen != seenGen) { seenGen = gen; lastBeatIdx = LLONG_MIN; }
    const juce::SpinLock::ScopedTryLockType sl (arrLock);
    const Arr* a = sl.isLocked() ? arr.get() : nullptr;
    const bool pl = playing.load(), rec = recording.load();
    const juce::int64 rs = recStart.load();
    const double s = sr * 60.0 / juce::jmax (20.0, bpm.load());
    const int clickLen = (int) (0.03 * sr);
    juce::int64 p = pos.load();
    int i = 0;
    while (i < n)
    {
        int seg = n - i;
        const bool looping = pl && a != nullptr && a->loop && ! rec;
        if (looping && p < a->loopE && p + seg > a->loopE) seg = (int) (a->loopE - p);

        // 1. record what you hear (before the takes and the click go in)
        if (pl && rec)
        {
            const juce::int64 from = std::max (p, rs);
            const int k0 = (int) std::min<juce::int64> (seg, from - p);
            if (k0 < seg)
            {
                const int cnt = seg - k0;
                int s1, n1, s2, n2;
                fifo.prepareToWrite (cnt, s1, n1, s2, n2);
                for (int k = 0; k < n1; ++k) { fifoL[(size_t) (s1 + k)] = L[i + k0 + k]; fifoR[(size_t) (s1 + k)] = R[i + k0 + k]; }
                for (int k = 0; k < n2; ++k) { fifoL[(size_t) (s2 + k)] = L[i + k0 + n1 + k]; fifoR[(size_t) (s2 + k)] = R[i + k0 + n1 + k]; }
                fifo.finishedWrite (n1 + n2);
                if (n1 + n2 < cnt) dropped += cnt - (n1 + n2);
            }
        }
        // 2. the takes on the timeline
        if (pl && a != nullptr)
            for (auto& c : a->clips)
            {
                const juce::int64 cs = c.start, ce = c.start + c.len;
                if (ce <= p || cs >= p + seg) continue;
                const float gl = a->gL[(size_t) c.track], gr = a->gR[(size_t) c.track];
                if (gl == 0.f && gr == 0.f) continue;
                const bool mono = a->mono[(size_t) c.track] != 0;
                const juce::int64 from = std::max (cs, p), to = std::min (ce, p + seg);
                const auto& t = *c.src;
                const juce::int64 src = c.off + (from - cs);
                const int dst = i + (int) (from - p), cnt = (int) (to - from);
                for (int k = 0; k < cnt; ++k)
                {
                    const auto si = (size_t) (src + k);
                    if (si >= t.L.size()) break;
                    float l = t.L[si], r = t.R[si];
                    if (mono) l = r = 0.5f * (l + r);
                    L[dst + k] += l * gl; R[dst + k] += r * gr;
                }
            }
        // 3. the click: on every beat when it's on, and always during the count-in
        const bool clickOn = pl && ((a != nullptr && a->click) || (rec && p < rs));
        for (int k = 0; k < seg; ++k)
        {
            if (clickOn)
            {
                const juce::int64 q = p + k;
                const auto b = (juce::int64) std::floor ((double) q / s);
                if (b != lastBeatIdx)
                {
                    const bool atStart = (double) q - (double) b * s < 1.0;      // not mid-beat after a jump
                    lastBeatIdx = b;
                    if (atStart) { clickLeft = clickLen; clickPhase = 0; clickHz = ((b % 4) + 4) % 4 == 0 ? 1600.f : 1100.f; }
                }
            }
            if (clickLeft > 0)
            {
                const float e = (float) clickLeft / (float) clickLen, v = 0.22f * e * e * (float) std::sin (clickPhase);
                clickPhase += juce::MathConstants<double>::twoPi * clickHz / sr;
                --clickLeft;
                L[i + k] += v; R[i + k] += v;
            }
        }
        if (pl) p += seg;
        i += seg;
        if (looping && p == a->loopE) p = a->loopS;
    }
    if (pl) pos.store (p);
}
} // namespace obs
