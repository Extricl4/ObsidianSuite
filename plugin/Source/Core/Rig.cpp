#include "Rig.h"

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
    if (juce::JSON::parse (f.loadFileAsString(), v).wasOk()) return v;
    return {};
}
} // namespace

Rig::Rig (Engine& e, juce::AudioProcessorValueTreeState& s) : engine (e), state (s)
{
    rig = makeDefaultRig();
    startTimer (400);
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
    notifyChanged();
}

// ---------------------------------------------------------------------------------- amp
void Rig::applyAmp()
{
    auto amp = rig.getChildWithName ("AMP");
    const auto type = amp["type"].toString();
    engine.amp().setType (type == "nam" ? AmpSection::Nam : type == "none" ? AmpSection::None : AmpSection::Model);
    if (type != "nam" && type != "none")
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
    // Library amps bring their own captures (one per channel).
    if (auto* caps = entry["captures"].getArray())
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
        amp.setProperty ("key", "nam-loader", nullptr).setProperty ("type", "nam", nullptr).setProperty ("name", "NAM Loader", nullptr);
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
}

void Rig::setModule (int rack, int slot, const juce::String& key, bool applyDefaults)
{
    auto s = slotTree (rack, slot);
    if (! s.isValid()) return;
    const auto* type = key.isEmpty() ? nullptr : findModule (key);
    if (key.isNotEmpty() && type == nullptr) { message ("Unknown effect: " + key, true); return; }
    s.setProperty ("key", key, nullptr).setProperty ("file", "", nullptr);
    engine.rack (rack).setModule (slot, key, engine.moduleContext());

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

// ---------------------------------------------------------------------------------- misc
void Rig::setMisc (const juce::String& key, const juce::var& value)
{
    rig.getChildWithName ("MISC").setProperty (key, value, nullptr);
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
    }
    add ("nam-loader", "NAM Loader", "nam", "capture", "Load your own .nam captures, one per channel.", 4);

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
        auto* e = const_cast<Engine&> (engine).amp().channelSlot (c).get();
        ch->setProperty ("loaded", e != nullptr);
        if (e != nullptr) ch->setProperty ("trainedRate", e->trainedRate());
        chans.add (juce::var (ch));
    }
    a->setProperty ("channels", chans);
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

void Rig::timerCallback()                     { engine.collectGarbage (false); }
void Rig::notifyChanged()                     { listeners.call ([] (Listener& l) { l.rigChanged(); }); }
void Rig::message (const juce::String& t, bool err)
{
    if (err) DBG ("Obsidian: " << t);
    listeners.call ([&] (Listener& l) { l.rigMessage (t, err); });
}
} // namespace obs
