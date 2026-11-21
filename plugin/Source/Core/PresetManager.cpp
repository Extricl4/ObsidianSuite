#include "PresetManager.h"
#include "ObsidianAssets.h"

namespace obs
{
namespace
{
juce::var treeToVar (const juce::ValueTree& t)
{
    auto o = new juce::DynamicObject();
    o->setProperty ("_type", t.getType().toString());
    for (int i = 0; i < t.getNumProperties(); ++i) o->setProperty (t.getPropertyName (i), t[t.getPropertyName (i)]);
    juce::Array<juce::var> kids;
    for (const auto& c : t) kids.add (treeToVar (c));
    if (! kids.isEmpty()) o->setProperty ("_children", kids);
    return juce::var (o);
}

juce::ValueTree varToTree (const juce::var& v)
{
    if (! v.isObject()) return {};
    juce::ValueTree t (v["_type"].toString().isNotEmpty() ? juce::Identifier (v["_type"].toString()) : juce::Identifier ("NODE"));
    for (auto& p : v.getDynamicObject()->getProperties())
    {
        if (p.name.toString().startsWithChar ('_')) continue;
        t.setProperty (p.name, p.value, nullptr);
    }
    if (auto* kids = v["_children"].getArray())
        for (auto& k : *kids) { auto c = varToTree (k); if (c.isValid()) t.appendChild (c, nullptr); }
    return t;
}

juce::String sanitise (juce::String name)
{
    name = name.trim().removeCharacters ("\\/:*?\"<>|").substring (0, 60).trim();
    return name.isEmpty() ? juce::String ("Untitled") : name;
}
} // namespace

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s, Rig& r, AppSettings& st)
    : state (s), rig (r), settings (st)
{
    for (auto& d : paramDefs()) state.addParameterListener (d.id, this);
    current.name = "Default"; current.id = "factory:Default"; current.factory = true;
    rescan();
}

PresetManager::~PresetManager()
{
    for (auto& d : paramDefs()) state.removeParameterListener (d.id, this);
}

juce::File PresetManager::folder() const
{
    juce::File f (settings.get ("presetFolder").toString());
    if (f.getFullPathName().isEmpty() || ! juce::File::isAbsolutePath (f.getFullPathName())) f = AppSettings::defaultPresetFolder();
    f.createDirectory();
    return f;
}

void PresetManager::setFolder (const juce::File& f)
{
    if (! f.isDirectory() && ! f.createDirectory()) { error = "Can't use that folder"; return; }
    settings.set ("presetFolder", f.getFullPathName());
    rescan();
}

void PresetManager::rescan()
{
    presets.clear();
    // Factory presets (compiled in from plugin/Presets/*.json).
    for (int i = 0; i < ObsidianAssets::namedResourceListSize; ++i)
    {
        const juce::String orig (ObsidianAssets::originalFilenames[i]);
        if (! orig.endsWithIgnoreCase (".json")) continue;
        int size = 0;
        const char* data = ObsidianAssets::getNamedResource (ObsidianAssets::namedResourceList[i], size);
        juce::var v;
        if (data == nullptr || ! juce::JSON::parse (juce::String::fromUTF8 (data, size), v).wasOk()) continue;
        if (v["format"].toString() != "obsidian-preset") continue;
        Info inf; inf.name = v["name"].toString(); inf.category = v["category"].toString(); inf.factory = true;
        inf.id = "factory:" + inf.name;
        presets.add (inf);
    }
    std::sort (presets.begin(), presets.end(), [] (const Info& a, const Info& b) { return a.category + a.name < b.category + b.name; });

    // Your presets.
    juce::Array<Info> user;
    for (const auto& f : folder().findChildFiles (juce::File::findFiles, true, "*.obsidian"))
    {
        Info inf; inf.file = f; inf.factory = false;
        juce::var v;
        if (juce::JSON::parse (f.loadFileAsString(), v).wasOk() && v.isObject())
        {
            inf.name = v["name"].toString().isNotEmpty() ? v["name"].toString() : f.getFileNameWithoutExtension();
            inf.category = v["category"].toString();
        }
        else inf.name = f.getFileNameWithoutExtension();
        inf.id = "user:" + f.getFullPathName();
        user.add (inf);
    }
    std::sort (user.begin(), user.end(), [] (const Info& a, const Info& b) { return a.name.compareNatural (b.name) < 0; });
    presets.addArray (user);
    notifyList();
}

juce::File PresetManager::fileFor (const juce::String& name) const
{
    return folder().getChildFile (sanitise (name) + ".obsidian");
}

juce::var PresetManager::captureCurrent (const juce::String& name, const juce::String& category) const
{
    auto o = new juce::DynamicObject();
    o->setProperty ("format", "obsidian-preset");
    o->setProperty ("version", 1);
    o->setProperty ("name", name);
    o->setProperty ("category", category);
    o->setProperty ("app", "Obsidian Suite " OBSIDIAN_VERSION_STRING);
    auto pv = new juce::DynamicObject();
    for (auto& d : paramDefs())
        if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (d.id)))
            pv->setProperty (d.id, std::round ((double) prm->convertFrom0to1 (prm->getValue()) * 10000.0) / 10000.0);   // tidy numbers in the file
    o->setProperty ("params", juce::var (pv));
    o->setProperty ("rig", treeToVar (rig.snapshot()));
    return juce::var (o);
}

bool PresetManager::applyPresetVar (const juce::var& v)
{
    if (! v.isObject() || v["format"].toString() != "obsidian-preset") { error = "That isn't an Obsidian preset"; return false; }
    ++suppress;
    const auto params = v["params"];
    for (auto& d : paramDefs())
    {
        auto* prm = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (d.id));
        if (prm == nullptr) continue;
        float real = d.def;
        if (params.isObject() && params.getDynamicObject()->hasProperty (d.id)) real = (float) (double) params[juce::Identifier (d.id)];
        if (! std::isfinite (real)) real = d.def;
        real = juce::jlimit (d.min, d.max, real);
        prm->setValueNotifyingHost (prm->convertTo0to1 (real));
    }
    rig.restore (varToTree (v["rig"]));
    --suppress;
    return true;
}

bool PresetManager::load (const juce::String& id)
{
    error = {};
    juce::var v;
    Info found;
    for (auto& p : presets) if (p.id == id) found = p;
    if (found.id.isEmpty()) { error = "Preset not found"; return false; }

    if (found.factory)
    {
        for (int i = 0; i < ObsidianAssets::namedResourceListSize; ++i)
        {
            int size = 0;
            const char* data = ObsidianAssets::getNamedResource (ObsidianAssets::namedResourceList[i], size);
            juce::var cand;
            if (data != nullptr && juce::String (ObsidianAssets::originalFilenames[i]).endsWithIgnoreCase (".json")
                && juce::JSON::parse (juce::String::fromUTF8 (data, size), cand).wasOk() && "factory:" + cand["name"].toString() == id)
            { v = cand; break; }
        }
    }
    else if (! juce::JSON::parse (found.file.loadFileAsString(), v).wasOk())
    {
        error = "Couldn't read " + found.file.getFileName(); return false;
    }
    if (! applyPresetVar (v)) return false;
    current = found;
    dirty = false; dirtyFlagFromAudio = false;
    settings.set ("lastPreset", id);
    notifyState();
    return true;
}

bool PresetManager::loadNext (int direction)
{
    if (presets.isEmpty()) return false;
    int idx = -1;
    for (int i = 0; i < presets.size(); ++i) if (presets[i].id == current.id) idx = i;
    idx = idx < 0 ? 0 : (idx + direction + presets.size()) % presets.size();
    return load (presets[idx].id);
}

bool PresetManager::loadInit()
{
    ++suppress;
    for (auto& d : paramDefs())
        if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (d.id)))
            prm->setValueNotifyingHost (prm->getDefaultValue());
    rig.resetToDefault();
    --suppress;
    current = {}; current.name = "Default"; current.id = "factory:Default"; current.factory = true;
    dirty = false; dirtyFlagFromAudio = false;
    notifyState();
    return true;
}

bool PresetManager::save()
{
    if (current.factory || current.file == juce::File()) return saveAs (current.name == "Default" ? "My Preset" : current.name);
    const auto v = captureCurrent (current.name, current.category);
    if (! current.file.replaceWithText (juce::JSON::toString (v, false))) { error = "Couldn't write the preset file"; return false; }
    dirty = false; dirtyFlagFromAudio = false;
    notifyState();
    return true;
}

bool PresetManager::saveAs (const juce::String& nameIn, const juce::String& category)
{
    const auto name = sanitise (nameIn);
    const auto f = fileFor (name);
    const auto v = captureCurrent (name, category);
    if (! f.replaceWithText (juce::JSON::toString (v, false))) { error = "Couldn't write " + f.getFullPathName(); return false; }
    rescan();
    for (auto& p : presets) if (p.file == f) current = p;
    dirty = false; dirtyFlagFromAudio = false;
    settings.set ("lastPreset", current.id);
    notifyState();
    return true;
}

bool PresetManager::remove (const juce::String& id)
{
    for (auto& p : presets)
        if (p.id == id)
        {
            if (p.factory) { error = "Factory presets can't be deleted"; return false; }
            if (! p.file.moveToTrash() && ! p.file.deleteFile()) { error = "Couldn't delete the file"; return false; }
            const bool wasCurrent = current.id == id;
            rescan();
            if (wasCurrent) { current.factory = true; current.file = juce::File(); current.id = "factory:" + current.name; dirty = true; notifyState(); }
            return true;
        }
    error = "Preset not found"; return false;
}

bool PresetManager::rename (const juce::String& id, const juce::String& newName)
{
    for (auto& p : presets)
        if (p.id == id && ! p.factory)
        {
            juce::var v;
            if (! juce::JSON::parse (p.file.loadFileAsString(), v).wasOk() || ! v.isObject()) { error = "Couldn't read the preset"; return false; }
            const auto name = sanitise (newName);
            v.getDynamicObject()->setProperty ("name", name);
            const auto dest = fileFor (name);
            if (dest.exists() && dest != p.file) { error = "A preset with that name already exists"; return false; }
            if (! dest.replaceWithText (juce::JSON::toString (v, false))) { error = "Couldn't write the file"; return false; }
            if (dest != p.file) p.file.deleteFile();
            const bool wasCurrent = current.id == id;
            rescan();
            if (wasCurrent) for (auto& q : presets) if (q.file == dest) { current = q; notifyState(); }
            return true;
        }
    error = "Only your own presets can be renamed"; return false;
}

bool PresetManager::importFile (const juce::File& f)
{
    juce::var v;
    if (! juce::JSON::parse (f.loadFileAsString(), v).wasOk() || v["format"].toString() != "obsidian-preset") { error = f.getFileName() + " isn't an Obsidian preset"; return false; }
    auto dest = fileFor (v["name"].toString().isNotEmpty() ? v["name"].toString() : f.getFileNameWithoutExtension());
    if (dest.exists()) dest = dest.getNonexistentSibling();
    if (! f.copyFileTo (dest)) { error = "Couldn't copy the file"; return false; }
    rescan();
    return true;
}

bool PresetManager::exportTo (const juce::String& id, const juce::File& dest)
{
    juce::var v;
    if (id.isEmpty() || id == current.id) v = captureCurrent (current.name, current.category);
    else
    {
        for (auto& p : presets) if (p.id == id && ! p.factory) juce::JSON::parse (p.file.loadFileAsString(), v);
        if (! v.isObject()) v = captureCurrent (current.name, current.category);
    }
    if (! dest.replaceWithText (juce::JSON::toString (v, false))) { error = "Couldn't write " + dest.getFullPathName(); return false; }
    return true;
}

void PresetManager::markDirty()
{
    if (suppress.load() > 0 || dirty) return;
    dirty = true;
    notifyState();
}

void PresetManager::setCurrentFromState (const juce::String& id, const juce::String& name, bool d)
{
    current = {}; current.id = id; current.name = name.isNotEmpty() ? name : juce::String ("Default");
    for (auto& p : presets) if (p.id == id) current = p;
    if (current.id.isEmpty()) { current.id = id; current.factory = ! id.startsWith ("user:"); }
    dirty = d;
    notifyState();
}

void PresetManager::parameterChanged (const juce::String&, float)
{
    // May be called on the audio thread (automation): only set a flag here.
    if (suppress.load() == 0) dirtyFlagFromAudio = true;
}

void PresetManager::pollDirty()
{
    if (dirtyFlagFromAudio.exchange (false)) markDirty();
}

juce::var PresetManager::toJson() const
{
    auto o = new juce::DynamicObject();
    o->setProperty ("id", current.id); o->setProperty ("name", current.name); o->setProperty ("category", current.category);
    o->setProperty ("factory", current.factory); o->setProperty ("dirty", dirty);
    o->setProperty ("folder", folder().getFullPathName());
    return juce::var (o);
}

juce::var PresetManager::listJson() const
{
    juce::Array<juce::var> a;
    for (auto& p : presets)
    {
        auto o = new juce::DynamicObject();
        o->setProperty ("id", p.id); o->setProperty ("name", p.name); o->setProperty ("category", p.category); o->setProperty ("factory", p.factory);
        a.add (juce::var (o));
    }
    return a;
}

void PresetManager::notifyState()  { listeners.call ([] (Listener& l) { l.presetStateChanged(); }); }
void PresetManager::notifyList()   { listeners.call ([] (Listener& l) { l.presetListChanged(); }); }
} // namespace obs
