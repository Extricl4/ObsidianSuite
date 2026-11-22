#include <map>
#include "Rig.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include "../DSP/SynthDefs.h"

namespace obs
{
using namespace dsp;

namespace
{
const char* builtInCabKeys[] = { "builtin-1x12", "builtin-2x12", "builtin-4x12" };

juce::ValueTree makeDefaultRig()
{
    juce::ValueTree rig ("RIG");
    juce::ValueTree amp ("AMP");
    amp.setProperty ("key", "core", nullptr).setProperty ("type", "model", nullptr).setProperty ("name", "OBSIDIAN CORE", nullptr);
    for (int c = 0; c < AmpSection::kChannels; ++c) amp.setProperty ("ch" + juce::String (c), "", nullptr);
    rig.appendChild (amp, nullptr);
    juce::ValueTree cab ("CAB");
    cab.setProperty ("key", "builtin-4x12", nullptr).setProperty ("name", "Obsidian 4x12", nullptr).setProperty ("irA", "", nullptr).setProperty ("irB", "", nullptr);
    rig.appendChild (cab, nullptr);
    for (int r = 0; r < kNumRacks; ++r)
    {
        juce::ValueTree rack ("RACK"); rack.setProperty ("index", r, nullptr);
        for (int s = 0; s < kSlotsPerRack; ++s)
        {
            juce::ValueTree slot ("SLOT"); slot.setProperty ("index", s, nullptr).setProperty ("key", "", nullptr).setProperty ("file", "", nullptr);
            rack.appendChild (slot, nullptr);
        }
        rig.appendChild (rack, nullptr);
    }
    juce::ValueTree misc ("MISC");
    misc.setProperty ("bpm", 120.0, nullptr).setProperty ("beats", 4, nullptr).setProperty ("accent", true, nullptr).setProperty ("eqPreset", "Flat", nullptr);
    rig.appendChild (misc, nullptr);
    return rig;
}

/** Makes sure every expected child exists (old presets / DAW projects keep loading as the format grows). */
void repairRig (juce::ValueTree& rig)
{
    const auto def = makeDefaultRig();
    for (const auto& child : def)
    {
        auto existing = rig.getChildWithName (child.getType());
        if (child.hasType ("RACK"))
        {
            const int idx = child["index"];
            juce::ValueTree found;
            for (const auto& c : rig) if (c.hasType ("RACK") && (int) c["index"] == idx) found = c;
            if (! found.isValid()) { rig.appendChild (child.createCopy(), nullptr); continue; }
            for (const auto& slot : child)
            {
                bool has = false;
                for (const auto& s : found) if ((int) s["index"] == (int) slot["index"]) has = true;
                if (! has) found.appendChild (slot.createCopy(), nullptr);
            }
            continue;
        }
        if (! existing.isValid()) { rig.appendChild (child.createCopy(), nullptr); continue; }
        for (int i = 0; i < child.getNumProperties(); ++i)
        {
            const auto name = child.getPropertyName (i);
            if (! existing.hasProperty (name)) existing.setProperty (name, child[name], nullptr);
        }
    }
}

juce::var readJsonFile (const juce::File& f)
{
    juce::var v;
    // amp files use "" as the key of a single-setting amp; JUCE can't hold an empty property name, so it becomes "_"
    if (juce::JSON::parse (f.loadFileAsString().replace ("\"\":", "\"_\":"), v).wasOk()) return v;
    return {};
}
} // namespace

Rig::Rig (Engine& e, juce::AudioProcessorValueTreeState& s) : engine (e), state (s)
{
    rig = makeDefaultRig();
    startTimer (200);
}

Rig::~Rig()
{
    stopTimer();
    alive->store (false);
    loader.removeAllJobs (true, 10000);
}

juce::File Rig::contentFolder()
{
    const auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    auto f = docs.getChildFile (OBSIDIAN_PRODUCT_NAME);
    static bool migrated = false;      // the first builds used "Documents\Obsidian Suite": bring its files along once
    if (! migrated)
    {
        migrated = true;
        const auto old = docs.getChildFile ("Obsidian Suite");
        if (! f.exists() && old.isDirectory()) old.copyDirectoryTo (f);
    }
    for (auto sub : { "Presets", "Amps", "Cabs", "Captures", "IRs" }) f.getChildFile (sub).createDirectory();
    return f;
}

void Rig::restore (const juce::ValueTree& saved)
{
    if (saved.isValid() && saved.hasType ("RIG")) rig.copyPropertiesAndChildrenFrom (saved, nullptr);
    else rig.copyPropertiesAndChildrenFrom (makeDefaultRig(), nullptr);
    repairRig (rig);
    applyAll();
}

void Rig::resetToDefault()
{
    rig.copyPropertiesAndChildrenFrom (makeDefaultRig(), nullptr);
    applyAll();
}

void Rig::applyAll()
{
    applyAmp();
    applyCab();
    for (int r = 0; r < kNumRacks; ++r) for (int s = 0; s < kSlotsPerRack; ++s) applySlot (r, s);
    applySynthSteps();
    notifyChanged();
}

// ---------------------------------------------------------------------------------- amp
void Rig::applyAmp()
{
    auto amp = rig.getChildWithName ("AMP");
    const auto type = amp["type"].toString();
    engine.amp().setType (type == "nam" ? AmpSection::Nam : type == "none" ? AmpSection::None : type == "capture" ? AmpSection::Capture : AmpSection::Model);
    if (type == "capture")
    {
        const auto key = amp["key"].toString();
        if (ampMapKey != key || ampMap == nullptr)
        {
            juce::String err;
            ampMap = CaptureAmpMap::compile (readAmpFile (key)["map"], readFaceFile (key), err);
            ampMapKey = ampMap != nullptr ? key : juce::String();
            if (ampMap == nullptr) message (amp["name"].toString() + ": " + (err.isEmpty() ? "its amp file isn't installed (Documents\Obsidian Amp Suite\Amps)" : err), true);
        }
        lastAmpChannel = -1;
        pushFaceState();
        auto* cur = engine.amp().captureSlot().get();
        if (ampMap != nullptr && (cur == nullptr || cur->key != key) && loadingAmpKey != key) startCaptureAmpLoad (key);
    }
    else
    {
        ampMap = nullptr; ampMapKey = {}; loadingAmpKey = {};
        engine.amp().captureSlot().set (nullptr);                 // frees the pack amp's captures
    }
    if (type != "nam" && type != "none" && type != "capture")
    {
        // in-house amp ("core" was the only one in the first builds)
        const int idx = obs::amp::findModel (amp["key"].toString().toRawUTF8());
        engine.amp().setModel (idx >= 0 ? idx : obs::amp::findModel ("core"));
    }
    for (int c = 0; c < AmpSection::kChannels; ++c)
    {
        const juce::String path = amp["ch" + juce::String (c)].toString();
        auto& slot = engine.amp().channelSlot (c);
        if (type != "nam" || path.isEmpty()) { ++generation[c]; slot.set (nullptr); continue; }
        const juce::File f (path);
        if (auto* cur = slot.get()) if (cur->file() == f) continue;   // already loaded
        if (! f.existsAsFile())
        {
            message ("Capture not found: " + f.getFileName() + ". Load it again from the AMP tab.", true);
            slot.set (nullptr);
            continue;
        }
        startCaptureLoad (c, f, false);
    }
}

void Rig::setAmp (const juce::String& key)
{
    auto amp = rig.getChildWithName ("AMP");
    if (key.isEmpty() || key == "none")
    {
        amp.setProperty ("key", "none", nullptr).setProperty ("type", "none", nullptr).setProperty ("name", "", nullptr);
        applyAmp(); notifyChanged(); return;
    }
    const auto entry = findInLibrary (ampLibrary(), key);
    if (entry.isVoid()) { message ("Unknown amp: " + key, true); return; }
    amp.setProperty ("key", key, nullptr).setProperty ("type", entry["type"], nullptr).setProperty ("name", entry["name"], nullptr);
    // capture amps start like the real amp: its default knobs, buttons and channel
    for (int i = amp.getNumProperties(); --i >= 0;)
    {
        const auto n = amp.getPropertyName (i).toString();
        if (n.startsWith ("fd") || n.startsWith ("fx") || n.startsWith ("fs") || n.startsWith ("fk")) amp.removeProperty (n, nullptr);
    }
    if (entry["type"].toString() == "capture")
    {
        juce::String err;
        auto m = CaptureAmpMap::compile (readAmpFile (key)["map"], readFaceFile (key), err);
        if (m != nullptr)
        {
            const int d0start = m->dims.empty() ? 0 : m->dims[0].start;
            for (size_t i = 0; i < m->dims.size(); ++i)
            {
                juce::StringArray per;
                for (int c = 0; c < AmpSection::kChannels; ++c) per.add (juce::String (! m->dims[i].per || c == d0start ? m->dims[i].start : 0));
                amp.setProperty ("fd" + juce::String ((int) i), per.joinIntoString (","), nullptr);
            }
            for (int j = 0; j < (int) m->roles.size(); ++j)
            {
                const juce::String pid = CaptureAmpMap::paramFor (m->roles[(size_t) j]);
                const float def = m->defaults[(size_t) j];
                if (pid.isNotEmpty()) { if (auto* p = state.getParameter (pid)) p->setValueNotifyingHost (p->convertTo0to1 (def * 10.f)); }
                else amp.setProperty ("fx" + juce::String (j), def, nullptr);
            }
            if (auto* p = state.getParameter ("amp.channel")) p->setValueNotifyingHost (p->convertTo0to1 ((float) (m->firstDimIsChannel ? d0start : 0)));
            ampMap = m; ampMapKey = key;
        }
        for (int c = 0; c < AmpSection::kChannels; ++c) amp.setProperty ("ch" + juce::String (c), "", nullptr);
    }
    // Library amps bring their own captures (one per channel).
    else if (auto* caps = entry["captures"].getArray())
        for (int c = 0; c < AmpSection::kChannels; ++c)
            amp.setProperty ("ch" + juce::String (c), c < caps->size() ? (*caps)[c].toString() : juce::String(), nullptr);
    else if (entry["type"].toString() != "nam")
        for (int c = 0; c < AmpSection::kChannels; ++c) amp.setProperty ("ch" + juce::String (c), "", nullptr);
    applyAmp(); notifyChanged();
}

void Rig::loadCapture (int channel, const juce::File& file)
{
    channel = juce::jlimit (0, AmpSection::kChannels - 1, channel);
    auto amp = rig.getChildWithName ("AMP");
    if (amp["type"].toString() != "nam")
        amp.setProperty ("key", "nam-loader", nullptr).setProperty ("type", "nam", nullptr).setProperty ("name", "Eidolon", nullptr);
    amp.setProperty ("ch" + juce::String (channel), file.getFullPathName(), nullptr);
    engine.amp().setType (AmpSection::Nam);
    startCaptureLoad (channel, file, true);
    notifyChanged();
}

void Rig::clearCapture (int channel)
{
    channel = juce::jlimit (0, AmpSection::kChannels - 1, channel);
    rig.getChildWithName ("AMP").setProperty ("ch" + juce::String (channel), "", nullptr);
    ++generation[channel];
    engine.amp().channelSlot (channel).set (nullptr);
    notifyChanged();
}

void Rig::startCaptureLoad (int channel, const juce::File& f, bool notifyOk)
{
    const int gen = ++generation[channel];
    const double rate = engine.currentSampleRate();
    const int block = engine.currentBlockSize();
    ++pending;
    auto token = alive;
    loader.addJob ([this, token, channel, gen, f, rate, block, notifyOk]
    {
        juce::String err;
        auto box = std::make_shared<std::unique_ptr<NamEngine>> (NamEngine::load (f, rate, block, err));
        juce::MessageManager::callAsync ([this, token, channel, gen, box, err, f, rate, notifyOk]
        {
            if (! token->load()) return;
            --pending;
            if (gen != generation[channel]) return;              // a newer load replaced this one
            if (*box == nullptr) { message (err, true); notifyChanged(); return; }
            if (engine.currentSampleRate() != rate) (*box)->prepare (engine.currentSampleRate(), engine.currentBlockSize());
            const auto name = (*box)->name();
            const double trained = (*box)->trainedRate();
            engine.amp().channelSlot (channel).set (std::move (*box));
            if (notifyOk)
                message ("Loaded " + name + (std::abs (trained - engine.currentSampleRate()) > 1 ? "  (trained at " + juce::String ((int) trained) + " Hz, resampled)" : juce::String()), false);
            notifyChanged();
        });
    });
}

// ------------------------------------------------------------------------- capture amps
juce::var Rig::readJson (const juce::File& f) { return readJsonFile (f); }

/** A .nam file's own description (its "metadata": name, gear, amp only or full rig), read once per file. */
juce::var Rig::namInfo (const juce::String& path)
{
    static juce::CriticalSection lock;
    static std::map<juce::String, juce::var> cache;
    const juce::ScopedLock sl (lock);
    if (auto it = cache.find (path); it != cache.end()) return it->second;
    auto o = new juce::DynamicObject();
    const juce::File f (path);
    juce::var v;
    if (f.existsAsFile() && f.getSize() < 64 * 1024 * 1024 && juce::JSON::parse (f.loadFileAsString(), v).wasOk() && v["metadata"].isObject())
    {
        const auto m = v["metadata"];
        o->setProperty ("title", m["name"].toString().trim());
        o->setProperty ("gear", (m["gear_make"].toString().trim() + " " + m["gear_model"].toString().trim()).trim());
        const auto t = m["gear_type"].toString().toLowerCase();
        o->setProperty ("kind", t.isEmpty() ? juce::String() : t.contains ("cab") ? juce::String ("Full rig") : t == "pedal" ? juce::String ("Pedal") : juce::String ("Amp only"));
        o->setProperty ("by", m["modeled_by"].toString().trim());
    }
    return cache[path] = juce::var (o);
}

juce::var Rig::readAmpFile (const juce::String& key)
{
    if (key.isEmpty() || key.containsAnyOf ("/\\:.")) return {};
    return readJsonFile (contentFolder().getChildFile ("Amps").getChildFile (key + ".json"));
}
juce::var Rig::readFaceFile (const juce::String& key)
{
    if (key.isEmpty() || key.containsAnyOf ("/\\:.")) return {};
    return readJsonFile (contentFolder().getChildFile ("Faces").getChildFile (key + ".json"));
}

/** Rig tree -> the amp section's face state (buttons per channel, extra knobs, switches). */
void Rig::pushFaceState()
{
    if (ampMap == nullptr) return;
    auto amp = rig.getChildWithName ("AMP");
    auto& a = engine.amp();
    for (size_t i = 0; i < ampMap->dims.size(); ++i)
    {
        auto vals = juce::StringArray::fromTokens (amp["fd" + juce::String ((int) i)].toString(), ",", "");
        for (int c = 0; c < AmpSection::kChannels; ++c)
            a.setFaceDim ((int) i, c, juce::jlimit (0, ampMap->dims[i].count - 1, c < vals.size() ? vals[c].getIntValue() : ampMap->dims[i].start));
    }
    for (int j = 0; j < (int) ampMap->roles.size(); ++j)
        a.setFaceKnob (j, (float) (double) amp.getProperty ("fx" + juce::String (j), ampMap->defaults[(size_t) j]));
    for (int i = 0; i < ampMap->switchIds.size(); ++i)
        a.setFaceSwitch (i, (bool) amp.getProperty ("fs" + juce::String (i), false));
}

void Rig::setAmpDim (int dim, int value)
{
    if (ampMap == nullptr || ! juce::isPositiveAndBelow (dim, (int) ampMap->dims.size())) return;
    value = juce::jlimit (0, ampMap->dims[(size_t) dim].count - 1, value);
    if (dim == 0 && ampMap->firstDimIsChannel)
    {
        if (auto* p = state.getParameter ("amp.channel")) p->setValueNotifyingHost (p->convertTo0to1 ((float) value));
        captureAmpChannelTick();                 // swap the per-channel knobs now, not on the next timer tick
        notifyChanged();
        return;
    }
    auto amp = rig.getChildWithName ("AMP");
    const auto id = "fd" + juce::String (dim);
    auto vals = juce::StringArray::fromTokens (amp[id].toString(), ",", "");
    while (vals.size() < AmpSection::kChannels) vals.add ("0");
    const int ch = juce::jlimit (0, AmpSection::kChannels - 1, (int) std::round (state.getRawParameterValue ("amp.channel")->load()));
    for (int c = 0; c < AmpSection::kChannels; ++c)
        if (! ampMap->dims[(size_t) dim].per || c == ch) vals.set (c, juce::String (value));
    amp.setProperty (id, vals.joinIntoString (","), nullptr);
    pushFaceState();
    notifyChanged();
}

void Rig::setAmpKnob (int knob, float value)
{
    if (ampMap == nullptr || ! juce::isPositiveAndBelow (knob, (int) ampMap->roles.size())) return;
    rig.getChildWithName ("AMP").setProperty ("fx" + juce::String (knob), juce::jlimit (0.f, 1.f, value), nullptr);
    engine.amp().setFaceKnob (knob, value);
}

void Rig::setAmpSwitch (int index, bool on)
{
    if (ampMap == nullptr || ! juce::isPositiveAndBelow (index, ampMap->switchIds.size())) return;
    rig.getChildWithName ("AMP").setProperty ("fs" + juce::String (index), on, nullptr);
    engine.amp().setFaceSwitch (index, on);
    notifyChanged();
}

/** Amps that keep knobs per channel (Moirai, Umbra...): when the channel changes, put the old channel's
    knobs away and bring the new channel's back. Runs on the timer too, so DAW automation of the channel works. */
void Rig::captureAmpChannelTick()
{
    if (ampMap == nullptr || ampMap->dims.empty() || ampMap->dims[0].perKnobs.empty()) return;
    const int ch = juce::jlimit (0, AmpSection::kChannels - 1, (int) std::round (state.getRawParameterValue ("amp.channel")->load()));
    if (ch == lastAmpChannel) return;
    auto amp = rig.getChildWithName ("AMP");
    const auto& knobs = ampMap->dims[0].perKnobs;
    auto knobValue = [&] (int j) -> float
    {
        const juce::String pid = CaptureAmpMap::paramFor (ampMap->roles[(size_t) j]);
        if (pid.isNotEmpty()) return state.getRawParameterValue (pid)->load() * 0.1f;
        return (float) (double) amp.getProperty ("fx" + juce::String (j), ampMap->defaults[(size_t) j]);
    };
    if (lastAmpChannel >= 0)
    {
        juce::StringArray v;
        for (int j : knobs) v.add (juce::String (knobValue (j), 4));
        amp.setProperty ("fk" + juce::String (lastAmpChannel), v.joinIntoString (","), nullptr);
        const auto saved = juce::StringArray::fromTokens (amp["fk" + juce::String (ch)].toString(), ",", "");
        for (size_t i = 0; i < knobs.size(); ++i)
        {
            const int j = knobs[i];
            const float val = (int) i < saved.size() ? saved[(int) i].getFloatValue() : ampMap->defaults[(size_t) j];
            const juce::String pid = CaptureAmpMap::paramFor (ampMap->roles[(size_t) j]);
            if (pid.isNotEmpty()) { if (auto* p = state.getParameter (pid)) p->setValueNotifyingHost (p->convertTo0to1 (val * 10.f)); }
            else { amp.setProperty ("fx" + juce::String (j), val, nullptr); engine.amp().setFaceKnob (j, val); }
        }
    }
    lastAmpChannel = ch;
}

Rig::CaptureAmpFiles Rig::loadCaptureAmp (std::shared_ptr<const CaptureAmpMap> map, const juce::String& key, double rate, int block)
{
    CaptureAmpFiles res;
    res.amp = std::make_unique<CaptureAmp>();
    res.amp->map = map; res.amp->key = key;
    const auto dir = contentFolder().getChildFile ("Captures");
    for (auto& rel : map->bank)
    {
        const auto f = dir.getChildFile (rel);
        if (! f.existsAsFile()) { res.missing.add (f.getFileName()); res.amp->engines.push_back (nullptr); continue; }
        juce::String err;
        auto e = NamEngine::load (f, rate, block, err);
        if (e == nullptr) res.broken.add (f.getFileName() + ": " + err);
        res.amp->engines.push_back (std::move (e));
    }
    res.amp->levelMatch (rate, block);
    return res;
}

void Rig::startCaptureAmpLoad (const juce::String& key)
{
    auto map = ampMap;
    if (map == nullptr || ! asyncFileLoads) return;
    loadingAmpKey = key;
    const double rate = engine.currentSampleRate();
    const int block = engine.currentBlockSize();
    const auto name = rig.getChildWithName ("AMP")["name"].toString();
    ++pending;
    auto token = alive;
    loader.addJob ([this, token, key, map, rate, block, name]
    {
        auto files = loadCaptureAmp (map, key, rate, block);
        auto ca = std::make_shared<std::unique_ptr<CaptureAmp>> (std::move (files.amp));
        const auto missing = files.missing, broken = files.broken;
        juce::MessageManager::callAsync ([this, token, key, ca, rate, missing, broken, name, total = map->bank.size()]
        {
            if (! token->load()) return;
            --pending;
            if (loadingAmpKey == key) loadingAmpKey = {};
            auto amp = rig.getChildWithName ("AMP");
            if (amp["type"].toString() != "capture" || amp["key"].toString() != key) return;   // the amp changed meanwhile
            if (engine.currentSampleRate() != rate)
                for (auto& e : (*ca)->engines) if (e) e->prepare (engine.currentSampleRate(), engine.currentBlockSize());
            engine.amp().captureSlot().set (std::move (*ca));
            if (missing.size() == total)
                message (name + ": its captures aren't installed. Put the Obsidian amp pack in " + contentFolder().getChildFile ("Captures").getFullPathName(), true);
            else if (! missing.isEmpty())
                message (name + ": " + juce::String (missing.size()) + " of " + juce::String (total) + " captures missing", true);
            else message ("Loaded " + name + " (" + juce::String (total) + " captures)", false);
            for (auto& b : broken) message (name + ": " + b, true);
            notifyChanged();
        });
    });
}

// ---------------------------------------------------------------------------------- cab
void Rig::applyCab()
{
    auto cab = rig.getChildWithName ("CAB");
    const auto key = cab["key"].toString();
    int builtIn = 2;
    for (int i = 0; i < 3; ++i) if (key == builtInCabKeys[i]) builtIn = i;
    const juce::String irs[2] = { cab["irA"].toString(), cab["irB"].toString() };
    for (int m = 0; m < 2; ++m)
    {
        if (irs[m].isNotEmpty())
        {
            const juce::File f (irs[m]);
            const auto err = engine.cab().loadIr (m, f);
            if (err.isEmpty()) continue;
            message (err + " (" + f.getFileName() + ")", true);
        }
        engine.cab().loadBuiltIn (m, builtIn);
    }
}

void Rig::setCab (const juce::String& key)
{
    auto cab = rig.getChildWithName ("CAB");
    const auto entry = findInLibrary (cabLibrary(), key);
    if (entry.isVoid()) { message ("Unknown cab: " + key, true); return; }
    cab.setProperty ("key", key, nullptr).setProperty ("name", entry["name"], nullptr)
       .setProperty ("irA", entry["irA"].toString(), nullptr).setProperty ("irB", entry["irB"].toString(), nullptr);
    applyCab(); notifyChanged();
}

void Rig::loadIr (int mic, const juce::File& file)
{
    auto cab = rig.getChildWithName ("CAB");
    for (int m = 0; m < 2; ++m)
    {
        if (mic != 2 && mic != m) continue;
        const auto err = engine.cab().loadIr (m, file);
        if (err.isNotEmpty()) { message (err, true); return; }
        cab.setProperty (m == 0 ? "irA" : "irB", file.getFullPathName(), nullptr);
    }
    if (! cab["key"].toString().startsWith ("custom")) cab.setProperty ("key", "custom", nullptr).setProperty ("name", file.getFileNameWithoutExtension(), nullptr);
    message ("Loaded IR " + file.getFileNameWithoutExtension(), false);
    notifyChanged();
}

// --------------------------------------------------------------------------------- racks
juce::ValueTree Rig::slotTree (int rack, int slot)
{
    for (auto r : rig)
        if (r.hasType ("RACK") && (int) r["index"] == rack)
            for (auto s : r)
                if ((int) s["index"] == slot) return s;
    return {};
}

void Rig::applySlot (int rack, int slot)
{
    auto s = slotTree (rack, slot);
    if (! s.isValid()) return;
    const auto key = s["key"].toString();
    auto& r = engine.rack (rack);
    if (r.moduleKey (slot) != key || key.isEmpty()) r.setModule (slot, key, engine.moduleContext());
    const auto file = s["file"].toString();
    if (key.isNotEmpty() && file.isNotEmpty())
    {
        if (auto* m = r.moduleAt (slot); m != nullptr && m->fileName().isEmpty())
            startModuleFileLoad (rack, slot, juce::File (file));
    }
    if (const auto* type = key.isEmpty() ? nullptr : findModule (key); type != nullptr && (! type->captures.empty() || ! type->irs.empty()))
        if (auto* m = r.moduleAt (slot); m != nullptr && ! m->packRequested)
            startPackLoad (rack, slot);
}

void Rig::setModule (int rack, int slot, const juce::String& key, bool applyDefaults)
{
    auto s = slotTree (rack, slot);
    if (! s.isValid()) return;
    const auto* type = key.isEmpty() ? nullptr : findModule (key);
    if (key.isNotEmpty() && type == nullptr) { message ("Unknown effect: " + key, true); return; }
    s.setProperty ("key", key, nullptr).setProperty ("file", "", nullptr);
    engine.rack (rack).setModule (slot, key, engine.moduleContext());
    if (type != nullptr && (! type->captures.empty() || ! type->irs.empty())) startPackLoad (rack, slot);

    if (applyDefaults)
    {
        auto setNorm = [this] (const juce::String& id, float norm)
        {
            if (auto* prm = state.getParameter (id)) prm->setValueNotifyingHost (juce::jlimit (0.f, 1.f, norm));
        };
        setNorm (slotOnId (rack, slot), 1.f);
        for (int k = 0; k < kSlotParams; ++k)
        {
            float norm = 0.5f;
            if (type != nullptr && k < (int) type->params.size()) norm = type->params[(size_t) k].toNorm (type->params[(size_t) k].def);
            setNorm (slotParamId (rack, slot, k), norm);
        }
    }
    notifyChanged();
}

void Rig::moveModule (int rack, int from, int to)
{
    if (from == to || from < 0 || to < 0 || from >= kSlotsPerRack || to >= kSlotsPerRack) return;
    struct Item { juce::String key, file; float on; float knobs[kSlotParams]; };
    std::vector<Item> items;
    for (int s = 0; s < kSlotsPerRack; ++s)
    {
        Item it; auto t = slotTree (rack, s);
        it.key = t["key"].toString(); it.file = t["file"].toString();
        it.on = state.getParameter (slotOnId (rack, s))->getValue();
        for (int k = 0; k < kSlotParams; ++k) it.knobs[k] = state.getParameter (slotParamId (rack, s, k))->getValue();
        items.push_back (it);
    }
    auto moved = items[(size_t) from];
    items.erase (items.begin() + from);
    items.insert (items.begin() + to, moved);
    for (int s = 0; s < kSlotsPerRack; ++s)
    {
        const auto& it = items[(size_t) s];
        auto t = slotTree (rack, s);
        t.setProperty ("key", it.key, nullptr).setProperty ("file", it.file, nullptr);
        state.getParameter (slotOnId (rack, s))->setValueNotifyingHost (it.on);
        for (int k = 0; k < kSlotParams; ++k) state.getParameter (slotParamId (rack, s, k))->setValueNotifyingHost (it.knobs[k]);
        engine.rack (rack).setModule (s, {}, engine.moduleContext());
        applySlot (rack, s);
    }
    notifyChanged();
}

void Rig::loadModuleFile (int rack, int slot, const juce::File& file)
{
    auto s = slotTree (rack, slot);
    if (! s.isValid() || s["key"].toString().isEmpty()) return;
    s.setProperty ("file", file.getFullPathName(), nullptr);
    startModuleFileLoad (rack, slot, file);
    notifyChanged();
}

void Rig::startModuleFileLoad (int rack, int slot, const juce::File& f)
{
    if (! asyncFileLoads) return;
    const double rate = engine.currentSampleRate();
    const int block = engine.currentBlockSize();
    const auto key = slotTree (rack, slot)["key"].toString();
    ++pending;
    auto token = alive;
    loader.addJob ([this, token, rack, slot, key, f, rate, block]
    {
        juce::String err;
        auto box = std::make_shared<std::unique_ptr<NamEngine>> (NamEngine::load (f, rate, block, err));
        juce::MessageManager::callAsync ([this, token, rack, slot, key, box, err]
        {
            if (! token->load()) return;
            --pending;
            if (slotTree (rack, slot)["key"].toString() != key) return;      // slot changed meanwhile
            if (*box == nullptr) { message (err, true); notifyChanged(); return; }
            if (auto* m = engine.rack (rack).moduleAt (slot))
            {
                const auto name = (*box)->name();
                m->setNamEngine (std::move (*box), name);
                message ("Loaded " + name, false);
            }
            notifyChanged();
        });
    });
}

std::shared_ptr<Rig::PackFiles> Rig::loadPackFiles (const ModuleType& type, double rate, int block)
{
    auto res = std::make_shared<PackFiles>();
    const auto capDir = contentFolder().getChildFile ("Captures"), irDir = contentFolder().getChildFile ("IRs");
    res->total = (int) (type.captures.size() + type.irs.size());
    res->folder = (type.captures.empty() ? irDir.getChildFile (type.irs.front()) : capDir.getChildFile (type.captures.front())).getParentDirectory();
    for (auto& c : type.captures)
    {
        const auto f = capDir.getChildFile (c);
        res->names.add (f.getFileNameWithoutExtension());
        if (! f.existsAsFile()) { res->missing.add (f.getFileName()); res->engines.push_back (nullptr); continue; }
        juce::String err;
        auto e = NamEngine::load (f, rate, block, err);
        if (e == nullptr) res->broken.add (f.getFileName() + ": " + err);
        res->engines.push_back (std::move (e));
    }
    if (! type.irs.empty())
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        for (auto& rel : type.irs)
        {
            const auto f = irDir.getChildFile (rel);
            IrFile ir; ir.name = f.getFileNameWithoutExtension();
            if (! f.existsAsFile()) res->missing.add (f.getFileName());
            else if (std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f)); rd != nullptr && rd->lengthInSamples > 0)
            {
                const int len = (int) std::min<juce::int64> (rd->lengthInSamples, (juce::int64) (rd->sampleRate * 30.0));   // 30 s is plenty
                ir.buffer.setSize ((int) juce::jlimit (1u, 2u, rd->numChannels), len);
                rd->read (&ir.buffer, 0, len, 0, true, ir.buffer.getNumChannels() > 1);
                ir.sampleRate = rd->sampleRate;
            }
            else res->broken.add (f.getFileName() + ": not a readable audio file");
            res->irs.push_back (std::move (ir));
        }
    }
    return res;
}

void Rig::installPack (Module& m, PackFiles& files)
{
    if (! files.engines.empty()) m.setCaptureBank (std::move (files.engines), files.names);
    if (! files.irs.empty()) m.setIrBank (std::move (files.irs));
}

void Rig::startPackLoad (int rack, int slot)
{
    auto* m = engine.rack (rack).moduleAt (slot);
    const auto key = slotTree (rack, slot)["key"].toString();
    const auto* type = findModule (key);
    if (m == nullptr || type == nullptr || ! asyncFileLoads) return;
    m->packRequested = true;
    const double rate = engine.currentSampleRate();
    const int block = engine.currentBlockSize();
    ++pending;
    auto token = alive;
    loader.addJob ([this, token, rack, slot, key, m, type, rate, block]
    {
        auto res = loadPackFiles (*type, rate, block);
        juce::MessageManager::callAsync ([this, token, rack, slot, key, m, type, res, rate]
        {
            if (! token->load()) return;
            --pending;
            if (slotTree (rack, slot)["key"].toString() != key || engine.rack (rack).moduleAt (slot) != m) return;   // slot changed meanwhile
            if (engine.currentSampleRate() != rate)
                for (auto& e : res->engines) if (e) e->prepare (engine.currentSampleRate(), engine.currentBlockSize());
            installPack (*m, *res);
            if (res->missing.size() == res->total)
                message (type->name + ": its files aren't installed. Put the Obsidian pedal pack in " + res->folder.getFullPathName(), true);
            else if (! res->missing.isEmpty())
                message (type->name + ": " + juce::String (res->missing.size()) + " of " + juce::String (res->total) + " files missing (" + res->missing.joinIntoString (", ") + ")", true);
            for (auto& b : res->broken) message (type->name + ": " + b, true);
            notifyChanged();
        });
    });
}

bool Rig::captureAmpInstalled (const juce::String& key)
{
    const auto amp = readAmpFile (key);
    if (! amp.isObject()) return false;
    if (auto* sets = amp["map"]["settings"].getDynamicObject())
        for (auto& st : sets->getProperties())
            if (auto* files = st.value["files"].getArray())
                for (auto& f : *files)
                {
                    const auto first = f.isArray() ? (f.size() > 0 ? f[0].toString() : juce::String()) : f.toString();
                    if (first.isNotEmpty()) return contentFolder().getChildFile ("Captures").getChildFile (first).existsAsFile();
                }
    return false;
}

void Rig::loadFilesNow()
{
    const double rate = engine.currentSampleRate();
    const int block = engine.currentBlockSize();
    auto amp = rig.getChildWithName ("AMP");
    if (amp["type"].toString() == "capture" && ampMap != nullptr)
    {
        auto* cur = engine.amp().captureSlot().get();
        if (cur == nullptr || cur->key != ampMapKey)
            engine.amp().captureSlot().set (std::move (loadCaptureAmp (ampMap, ampMapKey, rate, block).amp));
    }
    for (int r = 0; r < kNumRacks; ++r)
        for (int sl = 0; sl < kSlotsPerRack; ++sl)
        {
            const auto* type = findModule (slotTree (r, sl)["key"].toString());
            auto* m = engine.rack (r).moduleAt (sl);
            if (type == nullptr || m == nullptr || (type->captures.empty() && type->irs.empty()) || m->packRequested) continue;
            m->packRequested = true;
            auto files = loadPackFiles (*type, rate, block);
            installPack (*m, *files);
        }
}

// ---------------------------------------------------------------------------------- misc
void Rig::setMisc (const juce::String& key, const juce::var& value)
{
    rig.getChildWithName ("MISC").setProperty (key, value, nullptr);
}
void Rig::setSynthSteps (const juce::var& v)
{
    juce::StringArray parts;
    if (auto* a = v.getArray()) for (auto& x : *a) parts.add (juce::String ((juce::int64) x));
    else parts = juce::StringArray::fromTokens (v.toString(), ",", "");
    if (parts.size() != 16) return;
    setMisc ("synSteps", parts.joinIntoString (","));
    applySynthSteps();
}

void Rig::applySynthSteps()
{
    unsigned s[16];
    const auto parts = juce::StringArray::fromTokens (getMisc ("synSteps", {}).toString(), ",", "");
    for (int i = 0; i < 16; ++i) s[i] = parts.size() == 16 ? (unsigned) parts[i].getLargeIntValue() : obs::synthdefs::defaultStep (i);
    engine.setSynthSteps (s);
}

juce::var Rig::getMisc (const juce::String& key, const juce::var& def) const
{
    auto m = rig.getChildWithName ("MISC");
    return m.hasProperty (key) ? m[key] : def;
}

// ------------------------------------------------------------------------------- libraries
juce::var Rig::findInLibrary (const juce::var& lib, const juce::String& key)
{
    if (auto* a = lib.getArray()) for (auto& e : *a) if (e["key"].toString() == key) return e;
    return {};
}

juce::var Rig::ampLibrary()
{
    juce::Array<juce::var> list;
    auto add = [&] (const juce::String& key, const juce::String& name, const juce::String& type, const juce::String& role, const juce::String& desc, int channels)
    {
        auto o = new juce::DynamicObject();
        o->setProperty ("key", key); o->setProperty ("name", name); o->setProperty ("type", type);
        o->setProperty ("role", role); o->setProperty ("description", desc); o->setProperty ("channels", channels); o->setProperty ("builtIn", true);
        list.add (juce::var (o));
    };
    for (const auto& m : obs::amp::models())
    {
        add (m.key, juce::String::fromUTF8 (m.name), "model", juce::String::fromUTF8 (m.role), juce::String::fromUTF8 (m.description), 1);
        auto* o = list.getReference (list.size() - 1).getDynamicObject();
        o->setProperty ("basedOn", juce::String::fromUTF8 (m.basedOn));
        o->setProperty ("tint", juce::String::toHexString ((int) m.tint).paddedLeft ('0', 6));
        o->setProperty ("folder", "OBSIDIAN AMPS");      // the amp chooser groups these in one folder
    }
    // Eidolon, the NAM loader: your own captures (up to four), with the rack face from the finished-amps page
    add ("nam-loader", "Eidolon", "nam", "NAM loader", "Load your own .nam captures (up to four) and step through them with PREV / NEXT or BROWSE.", 4);
    {
        auto* o = list.getReference (list.size() - 1).getDynamicObject();
        o->setProperty ("basedOn", "Your own NAM captures");
        const auto face = readFaceFile ("eidolon");
        if (face.isObject())
        {
            o->setProperty ("face", face);
            static const char* params[] = { "amp.gain", "amp.bass", "amp.mid", "amp.treble", "amp.presence", "amp.master" };
            juce::Array<juce::var> knobs;
            if (auto* fk = face["knobs"].getArray())
                for (int j = 0; j < fk->size(); ++j)
                {
                    const bool tone = j < 6;        // the last knob is BROWSE: it picks the capture
                    auto k = new juce::DynamicObject();
                    k->setProperty ("label", (*fk)[j]["label"]);
                    k->setProperty ("role", tone ? "tone" : "browse");
                    k->setProperty ("param", tone ? juce::String (params[j]) : juce::String());
                    k->setProperty ("def", 0.5);
                    k->setProperty ("steps", tone ? 0 : AmpSection::kChannels);
                    knobs.add (juce::var (k));
                }
            o->setProperty ("knobs", knobs);
        }
    }

    // Your amps: Documents\Obsidian Suite\Amps\*.json
    for (const auto& f : contentFolder().getChildFile ("Amps").findChildFiles (juce::File::findFiles, false, "*.json"))
    {
        auto v = readJsonFile (f);
        if (! v.isObject() || v["key"].toString().isEmpty()) continue;
        auto o = v.getDynamicObject();
        if (! o->hasProperty ("type")) o->setProperty ("type", "nam");
        // capture paths may be relative to Documents\Obsidian Suite\Captures
        if (auto* caps = v["captures"].getArray())
            for (auto& c : *caps)
            {
                juce::File cf = juce::File::isAbsolutePath (c.toString()) ? juce::File (c.toString()) : contentFolder().getChildFile ("Captures").getChildFile (c.toString());
                c = cf.getFullPathName();
            }
        o->setProperty ("builtIn", false);
        if (v["folder"].toString().isEmpty()) o->setProperty ("folder", "CAPTURE AMPS");   // the amp pack and amps you add
        if (v["map"].isObject())
        {
            // a capture amp: its knobs and buttons pick the captures (CaptureAmp.h). The UI gets the face and
            // what each knob does, not the wiring itself.
            const auto face = readJsonFile (contentFolder().getChildFile ("Faces").getChildFile (f.getFileNameWithoutExtension() + ".json"));
            juce::String err;
            if (auto m = CaptureAmpMap::compile (v["map"], face, err))
            {
                o->setProperty ("type", "capture");
                juce::Array<juce::var> knobs;
                for (int j = 0; j < (int) m->roles.size(); ++j)
                {
                    auto k = new juce::DynamicObject();
                    k->setProperty ("label", m->knobLabels[j]);
                    k->setProperty ("role", CaptureAmpMap::roleName (m->roles[(size_t) j]));
                    k->setProperty ("param", CaptureAmpMap::paramFor (m->roles[(size_t) j]));
                    k->setProperty ("def", m->defaults[(size_t) j]);
                    k->setProperty ("steps", m->steps[(size_t) j]);
                    knobs.add (juce::var (k));
                }
                o->setProperty ("knobs", knobs);
                juce::Array<juce::var> bank;
                for (auto& b : m->bank) bank.add (b.fromLastOccurrenceOf ("/", false, false).upToLastOccurrenceOf (".", false, false));
                o->setProperty ("bank", bank);
                o->setProperty ("channels", m->dims.empty() ? 1 : m->dims[0].count);
                if (face.isObject()) o->setProperty ("face", face);
            }
            else o->setProperty ("error", err);
            o->removeProperty ("map");
            o->removeProperty ("captures");
        }
        list.add (v);
    }
    return list;
}

juce::var Rig::cabLibrary()
{
    juce::Array<juce::var> list;
    const char* names[] = { "Obsidian 1x12", "Obsidian 2x12", "Obsidian 4x12" };
    const char* sizes[] = { "1x12", "2x12", "4x12" };
    for (int i = 0; i < 3; ++i)
    {
        auto o = new juce::DynamicObject();
        o->setProperty ("key", builtInCabKeys[i]); o->setProperty ("name", names[i]); o->setProperty ("size", sizes[i]);
        o->setProperty ("irA", ""); o->setProperty ("irB", ""); o->setProperty ("builtIn", true);
        list.add (juce::var (o));
    }
    for (const auto& f : contentFolder().getChildFile ("Cabs").findChildFiles (juce::File::findFiles, false, "*.json"))
    {
        auto v = readJsonFile (f);
        if (! v.isObject() || v["key"].toString().isEmpty()) continue;
        auto o = v.getDynamicObject();
        for (auto k : { "irA", "irB" })
        {
            const auto p = v[k].toString();
            if (p.isNotEmpty() && ! juce::File::isAbsolutePath (p)) o->setProperty (k, contentFolder().getChildFile ("IRs").getChildFile (p).getFullPathName());
        }
        o->setProperty ("builtIn", false);
        list.add (v);
    }
    return list;
}

// --------------------------------------------------------------------------------- json
juce::var Rig::toJson() const
{
    auto o = new juce::DynamicObject();
    auto amp = rig.getChildWithName ("AMP");
    auto a = new juce::DynamicObject();
    a->setProperty ("key", amp["key"]); a->setProperty ("type", amp["type"]); a->setProperty ("name", amp["name"]);
    juce::Array<juce::var> chans;
    for (int c = 0; c < AmpSection::kChannels; ++c)
    {
        auto ch = new juce::DynamicObject();
        const juce::String path = amp["ch" + juce::String (c)].toString();
        ch->setProperty ("file", path);
        ch->setProperty ("name", path.isEmpty() ? juce::String() : juce::File (path).getFileNameWithoutExtension());
        if (path.isNotEmpty())          // what the capture says about itself (Eidolon's screen)
        {
            const auto info = namInfo (path);
            for (auto k : { "title", "gear", "kind", "by" }) if (info[k].toString().isNotEmpty()) ch->setProperty (k, info[k]);
        }
        auto* e = const_cast<Engine&> (engine).amp().channelSlot (c).get();
        ch->setProperty ("loaded", e != nullptr);
        if (e != nullptr) ch->setProperty ("trainedRate", e->trainedRate());
        chans.add (juce::var (ch));
    }
    a->setProperty ("channels", chans);
    if (amp["type"].toString() == "capture")
    {
        auto f = new juce::DynamicObject();
        juce::Array<juce::var> dims, knobs, sws;
        if (ampMap != nullptr)
        {
            for (size_t i = 0; i < ampMap->dims.size(); ++i)
            {
                juce::Array<juce::var> per;
                auto vals = juce::StringArray::fromTokens (amp["fd" + juce::String ((int) i)].toString(), ",", "");
                for (int c = 0; c < AmpSection::kChannels; ++c) per.add (c < vals.size() ? vals[c].getIntValue() : 0);
                dims.add (per);
            }
            for (int j = 0; j < (int) ampMap->roles.size(); ++j) knobs.add ((double) amp.getProperty ("fx" + juce::String (j), ampMap->defaults[(size_t) j]));
            for (int i = 0; i < ampMap->switchIds.size(); ++i) sws.add ((bool) amp.getProperty ("fs" + juce::String (i), false));
        }
        f->setProperty ("dims", dims); f->setProperty ("knobs", knobs); f->setProperty ("switches", sws);
        auto* ca = const_cast<Engine&> (engine).amp().captureSlot().get();
        f->setProperty ("loaded", ca != nullptr && ca->key == amp["key"].toString());
        f->setProperty ("ok", ampMap != nullptr);
        a->setProperty ("face", juce::var (f));
    }
    o->setProperty ("amp", juce::var (a));

    auto cab = rig.getChildWithName ("CAB");
    auto cb = new juce::DynamicObject();
    for (auto k : { "key", "name", "irA", "irB" }) cb->setProperty (k, cab[k]);
    cb->setProperty ("irAName", cab["irA"].toString().isEmpty() ? juce::String() : juce::File (cab["irA"].toString()).getFileNameWithoutExtension());
    cb->setProperty ("irBName", cab["irB"].toString().isEmpty() ? juce::String() : juce::File (cab["irB"].toString()).getFileNameWithoutExtension());
    o->setProperty ("cab", juce::var (cb));

    juce::Array<juce::var> racks;
    for (int r = 0; r < kNumRacks; ++r)
    {
        juce::Array<juce::var> slots;
        for (int s = 0; s < kSlotsPerRack; ++s)
        {
            auto t = const_cast<Rig*> (this)->slotTree (r, s);
            auto so = new juce::DynamicObject();
            const auto key = t["key"].toString();
            so->setProperty ("key", key);
            const auto* type = key.isEmpty() ? nullptr : findModule (key);
            so->setProperty ("name", type != nullptr ? type->name : juce::String());
            so->setProperty ("file", t["file"]);
            auto* m = const_cast<Engine&> (engine).rack (r).moduleAt (s);
            so->setProperty ("fileName", m != nullptr ? m->fileName() : juce::String());
            slots.add (juce::var (so));
        }
        racks.add (slots);
    }
    o->setProperty ("racks", racks);

    auto misc = rig.getChildWithName ("MISC");
    auto mo = new juce::DynamicObject();
    for (int i = 0; i < misc.getNumProperties(); ++i) mo->setProperty (misc.getPropertyName (i), misc[misc.getPropertyName (i)]);
    o->setProperty ("misc", juce::var (mo));
    o->setProperty ("busy", pending.load() > 0);
    return juce::var (o);
}

void Rig::timerCallback()
{
    engine.collectGarbage (false);
    captureAmpChannelTick();
    bool changed = false;
    for (int r = 0; r < kNumRacks; ++r)                         // IR pedals rebuild their IR here (never on the audio thread)
        for (int s = 0; s < kSlotsPerRack; ++s)
            if (auto* m = engine.rack (r).moduleAt (s))
            {
                m->messageTick();
                if (const auto f = m->fileName(); f != shownFile[r][s]) { shownFile[r][s] = f; changed = true; }
            }
    if (changed) notifyChanged();                               // e.g. a pack pedal's switch picked another capture
}
void Rig::notifyChanged()                     { listeners.call ([] (Listener& l) { l.rigChanged(); }); }
void Rig::message (const juce::String& t, bool err)
{
    if (err) DBG ("Obsidian: " << t);
    listeners.call ([&] (Listener& l) { l.rigMessage (t, err); });
}
} // namespace obs
