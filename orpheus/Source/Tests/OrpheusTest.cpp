/*  ORPHEUS · the offline test (no window): plays the default song through the whole engine,
      1. as it starts (every effect off)          4. every preset (presets.json from tools/dump_presets): their levels
      2. every effect on its own                  5. MIDI keys, auditioning, the state saved and loaded back
      3. random settings (NaN, runaway levels)    6. speed (how much faster than real time)
    and writes WAVs to listen to. Fails (exit code 1) on silence, NaN / Inf, clipping or runaway levels.
      OrpheusTest [outFolder] [presets.json]                                                                     */
#include "../PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>

using namespace orph;

static int failures = 0;
static void fail (const juce::String& s) { ++failures; std::printf ("  FAIL  %s\n", s.toRawUTF8()); }
static double db (double x) { return 20 * std::log10 (std::max (1e-12, x)); }

struct Render { juce::AudioBuffer<float> buf; double rms = 0, peak = 0; bool bad = false; double secs = 0; };

static void setP (OrpheusProcessor& p, const char* id, float v)
{
    if (auto* q = p.apvts.getParameter (id)) q->setValueNotifyingHost (q->convertTo0to1 (v));
    else fail (juce::String ("no parameter ") + id);
}
static void resetAll (OrpheusProcessor& p)
{
    for (int i = 0; i < P_COUNT; ++i) if (auto* q = p.apvts.getParameter (paramDefs()[i].id)) q->setValueNotifyingHost (q->getDefaultValue());
}
static Render render (OrpheusProcessor& p, double seconds, double sr = 48000, int block = 512, std::function<void (int, juce::MidiBuffer&)> midiAt = {}, double measureFrom = 1.0)
{
    Render r; const int total = (int) (seconds * sr);
    r.buf.setSize (2, total); r.buf.clear();
    juce::AudioBuffer<float> b (2, block); juce::MidiBuffer mb;
    double acc = 0; int64_t n = 0;
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    for (int off = 0, k = 0; off < total; off += block, ++k)
    {
        const int len = std::min (block, total - off);
        b.setSize (2, len, false, false, true); mb.clear();
        if (midiAt) midiAt (k, mb);
        p.processBlock (b, mb);
        for (int c = 0; c < 2; ++c)
        {
            const float* x = b.getReadPointer (c);
            for (int s = 0; s < len; ++s)
            {
                if (! std::isfinite (x[s])) { r.bad = true; continue; }
                r.peak = std::max (r.peak, (double) std::abs (x[s]));
                if (off + s >= measureFrom * sr) { acc += (double) x[s] * x[s]; ++n; }
            }
            r.buf.copyFrom (c, off, x, len);
        }
    }
    r.secs = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
    r.rms = n ? std::sqrt (acc / n) : 0;
    return r;
}
static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr = 48000)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    if (auto os = std::unique_ptr<juce::OutputStream> (f.createOutputStream().release()))
        if (auto* w = wav.createWriterFor (os.get(), sr, 2, 24, {}, 0)) { os.release(); std::unique_ptr<juce::AudioFormatWriter> ww (w); ww->writeFromAudioSampleBuffer (b, 0, b.getNumSamples()); }
}
static bool check (const juce::String& what, const Render& r, double minDb = -45, double maxPeak = 1.0)
{
    const bool ok = ! r.bad && r.peak <= maxPeak && db (r.rms) > minDb;
    std::printf ("  %-34s rms %6.1f dB  peak %5.2f  %s\n", what.toRawUTF8(), db (r.rms), r.peak, ok ? "ok" : "");
    if (r.bad) fail (what + ": NaN / Inf");
    if (r.peak > maxPeak) fail (what + ": peak " + juce::String (r.peak, 3));
    if (db (r.rms) <= minDb) fail (what + ": too quiet (" + juce::String (db (r.rms), 1) + " dB)");
    return ok;
}

/* --cases cases.json outFolder: the same cases as the preview's livecap.py (a held chord, then let go), from the effects'
   output (before SPACE and the compressor), so the two can be compared */
static int runCases (const juce::File& cases, const juce::File& out)
{
    const double SR = 48000; out.createDirectory();
    auto list = juce::JSON::parse (cases);
    for (auto& c : *list.getArray())
    {
        OrpheusProcessor p; p.setPlayConfigDetails (0, 2, SR, 128); p.prepareToPlay (SR, 128); p.testRaw (true);
        if (auto* o = c["params"].getDynamicObject())
            for (auto& kv : o->getProperties())
                if (auto* q = p.apvts.getParameter (kv.name.toString())) q->setValueNotifyingHost (q->convertTo0to1 ((float) (double) kv.value));
        render (p, .1, SR, 128); p.testWaitForReverb (4000); render (p, .3, SR, 128);
        float ns[4] = { 57, 64, 69, 73 }; int n = 4;
        const juce::var nv = c.getProperty ("notes", juce::var());
        if (auto* arr = nv.getArray()) { n = 0; for (auto& x : *arr) if (n < 4) ns[n++] = (float) (double) x; }
        const double hold = (double) c.getProperty ("hold", 3000) / 1000.0, total = (double) c.getProperty ("total", 9000) / 1000.0;
        p.audition (ns, n, .9f, true); render (p, .003, SR, 128); p.auditionOff(); render (p, 1.9, SR, 128);   // as livecap.js: the voice is made first
        p.audition (ns, n, .9f, true);
        auto a = render (p, hold, SR, 128, {}, 0);
        p.auditionOff();
        auto b = render (p, std::max (.1, total - hold), SR, 128, {}, 0);
        juce::AudioBuffer<float> all (2, a.buf.getNumSamples() + b.buf.getNumSamples());
        for (int ch = 0; ch < 2; ++ch) { all.copyFrom (ch, 0, a.buf, ch, 0, a.buf.getNumSamples()); all.copyFrom (ch, a.buf.getNumSamples(), b.buf, ch, 0, b.buf.getNumSamples()); }
        writeWav (out.getChildFile (c["name"].toString() + ".wav"), all, SR);
        std::printf ("%s peak %.2f%s\n", c["name"].toString().toRawUTF8(), std::max (a.peak, b.peak), a.bad || b.bad ? " NaN!" : "");
    }
    juce::DeletedAtShutdown::deleteAll(); juce::MessageManager::deleteInstance();
    return 0;
}

int main (int argc, char** argv)
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);                  // (live in CI logs)
    juce::MessageManager::getInstance();
    if (argc > 3 && juce::String (argv[1]) == "--cases")
        return runCases (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]), juce::File::getCurrentWorkingDirectory().getChildFile (argv[3]));
    const juce::File out (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory().getChildFile ("orpheus-test"));
    out.createDirectory();
    const double SR = 48000;
    {
        OrpheusProcessor p; p.setPlayConfigDetails (0, 2, SR, 512); p.prepareToPlay (SR, 512);

        std::printf ("1. the default song, every effect off\n");
        p.transport ("play", 0);
        auto r = render (p, 14);
        check ("default", r, -40);
        writeWav (out.getChildFile ("01-default.wav"), r.buf);
        const double base = db (r.rms);
        std::printf ("   speed: %.0fx real time\n", 14.0 / std::max (1e-6, r.secs));

        std::printf ("2. every effect on its own (its own defaults)\n");
        struct E { const char* name; const char* on; std::vector<std::pair<const char*, float>> more; } fx[] = {
            { "drive", "syn.fx.drv.on", {} }, { "drive fuzz", "syn.fx.drv.on", { { "syn.fx.drv.mode", 1 }, { "syn.fx.drv.amt", 90 } } },
            { "crush", "syn.fx.cru.on", {} }, { "ring", "syn.fx.ring.on", {} }, { "octave", "syn.fx.oct.on", {} },
            { "chorus", "syn.fx.cho.on", {} }, { "flanger", "syn.fx.fla.on", { { "syn.fx.fla.fb", 95 } } }, { "phaser", "syn.fx.pha.on", { { "syn.fx.pha.fb", 95 } } },
            { "doubler", "syn.fx.dbl", {} }, { "delay", "syn.fx.dly.on", {} }, { "delay tape 95%", "syn.fx.dly.on", { { "syn.fx.dly.mode", 1 }, { "syn.fx.dly.fb", 95 } } },
            { "reverb hall", "syn.fx.rev.on", {} }, { "reverb cathedral", "syn.fx.rev.on", { { "syn.fx.rev.type", 3 }, { "syn.fx.rev.size", 100 }, { "syn.fx.rev.mix", 100 } } },
            { "shimmer", "syn.fx.shm.on", {} }, { "shimmer fifths max", "syn.fx.shm.on", { { "syn.fx.shm.mode", 1 }, { "syn.fx.shm.amt", 100 }, { "syn.fx.shm.decay", 100 }, { "syn.fx.shm.mix", 100 } } },
            { "width 200%", "syn.fx.dbl", { { "syn.fx.width", 200 } } }, { "space 100", "syn.fx.dbl", { { "syn.space", 100 } } },
        };
        int k = 0;
        for (auto& e : fx)
        {
            resetAll (p); p.reset(); render (p, .3);
            setP (p, e.on, 1); for (auto& m : e.more) setP (p, m.first, m.second);
            render (p, .05); p.testWaitForReverb (3000);
            p.transport ("play", 0);
            auto q = render (p, 10);
            if (check (e.name, q, -42) && std::abs (db (q.rms) - base) > 12) fail (juce::String (e.name) + ": " + juce::String (db (q.rms) - base, 1) + " dB from the dry sound");
            writeWav (out.getChildFile (juce::String::formatted ("02-%02d-", ++k) + juce::String (e.name).replaceCharacter (' ', '-') + ".wav"), q.buf);
        }

        std::printf ("3. tails: the reverb rings out after it is switched off, and stops at once when ALL is off\n");
        {
            resetAll (p); setP (p, "syn.fx.rev.on", 1); setP (p, "syn.fx.rev.mix", 100); render (p, .05); p.testWaitForReverb (3000);
            p.transport ("play", 0); render (p, 4);
            p.transport ("stop", 0); setP (p, "syn.fx.rev.on", 0);
            auto t1 = render (p, 1.5, SR, 512, {}, .5);
            std::printf ("   off, ringing: %.1f dB\n", db (t1.rms));
            if (db (t1.rms) < -70) fail ("the reverb didn't ring out when switched off");
            setP (p, "syn.fx.rev.on", 1); p.transport ("play", 0); render (p, 3); p.transport ("stop", 0); setP (p, "syn.fx.on", 0);
            auto t2 = render (p, 1.5, SR, 512, {}, .5);
            std::printf ("   ALL off: %.1f dB\n", db (t2.rms));
            if (db (t2.rms) > -80) fail ("ALL off left a reverb tail");
        }

        std::printf ("4. random settings (40 x 2 s)\n");
        {
            juce::Random rnd (1234); int bad = 0; double worst = 0;
            for (int it = 0; it < 40; ++it)
            {
                for (int i = 0; i < P_COUNT; ++i)
                {
                    const auto& d = paramDefs()[i]; auto* q = p.apvts.getParameter (d.id);
                    if (juce::String (d.id).startsWith ("syn.level") || juce::String (d.id) == "syn.trim") continue;
                    if (rnd.nextFloat() < .5f) q->setValueNotifyingHost (rnd.nextFloat());
                }
                setP (p, "syn.flt.on", 1); setP (p, "syn.a.on", 1); setP (p, "syn.a.level", 80);
                p.transport ("play", rnd.nextFloat() * 20);
                auto q = render (p, 2, SR, 256 + 64 * (it % 5), {}, .2);
                worst = std::max (worst, q.peak);
                if (q.bad || q.peak > 1.0) { ++bad; fail ("random #" + juce::String (it) + (q.bad ? ": NaN" : ": peak " + juce::String (q.peak, 2))); }
                if (it == 7) writeWav (out.getChildFile ("04-random.wav"), q.buf);
            }
            std::printf ("   %d bad, worst peak %.2f\n", bad, worst);
        }

        std::printf ("5. MIDI keys, auditioning, state\n");
        {
            resetAll (p); p.transport ("stop", 0); render (p, .5);
            auto q = render (p, 3, SR, 512, [] (int k2, juce::MidiBuffer& mb)
            {
                if (k2 == 2) { mb.addEvent (juce::MidiMessage::noteOn (1, 48, .9f), 0); mb.addEvent (juce::MidiMessage::noteOn (1, 52, .9f), 10); mb.addEvent (juce::MidiMessage::noteOn (1, 55, .9f), 20); }
                if (k2 == 150) { mb.addEvent (juce::MidiMessage::noteOff (1, 52), 0); }
                if (k2 == 220) { mb.addEvent (juce::MidiMessage::noteOff (1, 48), 0); mb.addEvent (juce::MidiMessage::noteOff (1, 55), 0); }
            }, .1);
            check ("MIDI chord", q, -45);
            writeWav (out.getChildFile ("05-midi.wav"), q.buf);
            const float ns[3] = { 60, 64, 67 }; p.audition (ns, 3, .8f, true);
            auto a = render (p, .6, SR, 512, {}, .1); p.auditionOff(); render (p, 1);
            check ("audition", a, -45);
            setP (p, "syn.flt.cutoff", 1234); setP (p, "syn.fx.rev.on", 1);
            juce::MemoryBlock mbk; p.getStateInformation (mbk);
            OrpheusProcessor p2; p2.setStateInformation (mbk.getData(), (int) mbk.getSize());
            const float c2 = p2.apvts.getRawParameterValue ("syn.flt.cutoff")->load();
            if (std::abs (c2 - 1234) > .5f || p2.apvts.getRawParameterValue ("syn.fx.rev.on")->load() < .5f) fail ("state didn't load back (cutoff " + juce::String (c2) + ")");
            if (p2.songJson() != p.songJson()) fail ("the song didn't load back");
            else std::printf ("  state saved and loaded back: ok\n");
        }

        std::printf ("6. presets\n");
        const juce::File pj (argc > 2 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]) : juce::File());
        if (pj.existsAsFile())
        {
            auto d = juce::JSON::parse (pj);
            auto* list = d["presets"].getArray();
            struct L { juce::String name; double db; };
            std::vector<L> levels;
            for (auto& pr : *list)
            {
                resetAll (p); p.transport ("stop", 0); p.reset(); render (p, .2);
                if (auto* o = pr["v"].getDynamicObject())
                    for (auto& kv : o->getProperties())
                        if (auto* q = p.apvts.getParameter (kv.name.toString())) q->setValueNotifyingHost (q->convertTo0to1 ((float) (double) kv.value));
                render (p, .1); p.testWaitForReverb (3000);
                p.transport ("play", 0);
                auto q = render (p, 12, SR, 512, {}, 1);
                const juce::String nm = pr["name"].toString();
                if (q.bad || q.peak > 1.0) fail (nm + (q.bad ? ": NaN" : ": peak " + juce::String (q.peak, 2)));
                levels.push_back ({ nm, db (q.rms) });
                writeWav (out.getChildFile ("06-" + nm.replaceCharacter (' ', '-').retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-") + ".wav"), q.buf);
            }
            std::vector<double> v; for (auto& l : levels) v.push_back (l.db);
            std::sort (v.begin(), v.end()); const double med = v[v.size() / 2];
            int in15 = 0, in3 = 0;
            for (auto& l : levels)
            {
                const double dv = l.db - med;
                if (std::abs (dv) <= 1.5) ++in15;
                if (std::abs (dv) <= 3) ++in3;
                std::printf ("   %-22s %6.1f dB  %+5.1f%s\n", l.name.toRawUTF8(), l.db, dv, std::abs (dv) > 3 ? "  <<" : "");
            }
            std::printf ("   median %.1f dB; within 1.5 dB: %d / %d, within 3 dB: %d / %d\n", med, in15, (int) levels.size(), in3, (int) levels.size());
            if (in3 < (int) levels.size() * 8 / 10) fail ("the presets' levels are far apart: the engine doesn't match the preview");
        }
        else std::printf ("   (no presets.json given)\n");
    }
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
    std::printf (failures ? "\n%d FAILED\n" : "\nALL OK\n", failures);
    return failures ? 1 : 0;
}
