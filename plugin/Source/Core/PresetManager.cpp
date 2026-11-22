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
/** A name for one of your folders: no path characters, no leading/trailing dots. */
juce::String folderName (juce::String name)
{
    name = name.trim().removeCharacters ("\\/:*?\"<>|").substring (0, 40).trim();
    while (name.startsWithChar ('.')) name = name.substring (1).trimStart();
    while (name.endsWithChar ('.') || name.endsWithChar (' ')) name = name.dropLastCharacters (1);
    return name;
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
        inf.type = v["type"].toString().isNotEmpty() ? v["type"].toString() : inf.category; inf.note = v["note"].toString();
        inf.id = "factory:" + inf.name; inf.resource = i;
        presets.add (inf);
    }
    // factory sounds first, then the artists' folders, each sorted by folder and name
    auto order = [] (const Info& i) { return (i.category.startsWith ("Artists/") ? "1" : "0") + i.category + "\n" + i.name; };
    std::stable_sort (presets.begin(), presets.end(), [&] (const Info& a, const Info& b) { return order (a).compareNatural (order (b)) < 0; });

    // Your presets: loose ones, and the ones in your folders.
    const auto root = folder();
    dirs.clear();
    for (const auto& d : root.findChildFiles (juce::File::findDirectories, false))
        if (! d.getFileName().startsWithChar ('.') && ! d.isHidden()) dirs.add (d.getFileName());
    dirs.sortNatural();
    juce::Array<Info> user;
    for (const auto& f : root.findChildFiles (juce::File::findFiles, true, "*.obsidian"))
    {
        Info inf; inf.file = f; inf.factory = false;
        const auto rel = f.getRelativePathFrom (root).replaceCharacter ('\\', '/');
        if (rel.containsChar ('/')) inf.dir = rel.upToFirstOccurrenceOf ("/", false, false);
        if (inf.dir.isNotEmpty() && ! dirs.contains (inf.dir)) continue;    // a hidden folder
        juce::var v;
        if (juce::JSON::parse (f.loadFileAsString(), v).wasOk() && v.isObject())
        {
            inf.name = v["name"].toString().isNotEmpty() ? v["name"].toString() : f.getFileNameWithoutExtension();
            inf.category = v["category"].toString();
            inf.type = v["type"].toString().isNotEmpty() ? v["type"].toString() : inf.category; inf.note = v["note"].toString();
        }
        else inf.name = f.getFileNameWithoutExtension();
        inf.id = "user:" + f.getFullPathName();
        user.add (inf);
    }
    std::sort (user.begin(), user.end(), [] (const Info& a, const Info& b) { return a.name.compareNatural (b.name) < 0; });
    presets.addArray (user);
    notifyList();
}

juce::File PresetManager::fileFor (const juce::String& name, const juce::String& dir) const
{
    const auto d = dir.isEmpty() ? folder() : folder().getChildFile (folderName (dir));
    return d.getChildFile (sanitise (name) + ".obsidian");
}

juce::File PresetManager::dirFile (const juce::String& dir) const
{
    const auto n = folderName (dir);
    if (n.isEmpty() || n != dir.trim()) return {};
    const auto root = folder(), d = root.getChildFile (n);
    return d.isDirectory() && d.getParentDirectory() == root ? d : juce::File();
}

void PresetManager::followCurrent (const juce::File& newFile)
{
    for (auto& q : presets) if (q.file == newFile) { current = q; settings.set ("lastPreset", current.id); notifyState(); return; }
}

juce::var PresetManager::captureCurrent (const juce::String& name, const juce::String& category) const
{
    auto o = new juce::DynamicObject();
    o->setProperty ("format", "obsidian-preset");
    o->setProperty ("version", 1);
    o->setProperty ("name", name);
    o->setProperty ("category", category);
    o->setProperty ("app", OBSIDIAN_PRODUCT_NAME " " OBSIDIAN_VERSION_STRING);
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
    // a capture amp that isn't installed (no amp pack): the preset's in-house stand-in plays instead
    auto tree = varToTree (v["rig"]);
    auto amp = tree.getChildWithName ("AMP");
    const auto fallback = v["fallbackAmp"].toString();
    if (amp.isValid() && amp["type"].toString() == "capture" && fallback.isNotEmpty() && ! Rig::captureAmpInstalled (amp["key"].toString()))
    {
        const auto entry = Rig::findInLibrary (Rig::ampLibrary(), fallback);
        if (! entry.isVoid())
        {
            for (int i = amp.getNumProperties(); --i >= 0;)
            {
                const auto n = amp.getPropertyName (i).toString();
                if (n.startsWith ("fd") || n.startsWith ("fx") || n.startsWith ("fs") || n.startsWith ("fk")) amp.removeProperty (n, nullptr);
            }
            amp.setProperty ("key", fallback, nullptr).setProperty ("type", entry["type"], nullptr).setProperty ("name", entry["name"], nullptr);
        }
    }
    rig.restore (tree);
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

    if (found.factory && found.resource >= 0)
    {
        int size = 0;
        const char* data = ObsidianAssets::getNamedResource (ObsidianAssets::namedResourceList[found.resource], size);
        if (data == nullptr || ! juce::JSON::parse (juce::String::fromUTF8 (data, size), v).wasOk()) { error = "Couldn't read the preset"; return false; }
    }
    else if (found.factory)
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

bool PresetManager::saveAs (const juce::String& nameIn, const juce::String& category, const juce::String& dir)
{
    const auto name = sanitise (nameIn);
    if (dir.isNotEmpty() && folderName (dir).isEmpty()) { error = "That folder name won't work"; return false; }
    if (dir.isNotEmpty()) folder().getChildFile (folderName (dir)).createDirectory();
    const auto f = fileFor (name, dir);
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
            if (p.factory) { error = "Factory and artist presets are locked: they can't be deleted"; return false; }
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
            const auto dest = p.file.getSiblingFile (name + ".obsidian");     // stays in its folder
            if (dest.exists() && dest != p.file && ! dest.getFullPathName().equalsIgnoreCase (p.file.getFullPathName())) { error = "A preset with that name already exists"; return false; }
            if (! dest.replaceWithText (juce::JSON::toString (v, false))) { error = "Couldn't write the file"; return false; }
            if (dest != p.file) p.file.deleteFile();
            const bool wasCurrent = current.id == id;
            rescan();
            if (wasCurrent) for (auto& q : presets) if (q.file == dest) { current = q; notifyState(); }
            return true;
        }
    error = "Factory and artist presets are locked: DUPLICATE one to make your own"; return false;
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

bool PresetManager::createFolder (const juce::String& nameIn)
{
    const auto n = folderName (nameIn);
    if (n.isEmpty()) { error = "Give the folder a name"; return false; }
    const auto d = folder().getChildFile (n);
    if (d.exists()) { error = "There's already a folder called " + n; return false; }
    if (! d.createDirectory()) { error = "Couldn't make the folder"; return false; }
    rescan();
    return true;
}

bool PresetManager::renameFolder (const juce::String& name, const juce::String& newNameIn)
{
    const auto src = dirFile (name);
    if (src == juce::File()) { error = "Folder not found"; return false; }
    const auto n = folderName (newNameIn);
    if (n.isEmpty()) { error = "Give the folder a name"; return false; }
    if (n == src.getFileName()) return true;
    const auto dst = folder().getChildFile (n);
    const bool caseOnly = dst.getFullPathName().equalsIgnoreCase (src.getFullPathName());
    if (dst.exists() && ! caseOnly) { error = "There's already a folder called " + n; return false; }
    const bool inside = current.file.isAChildOf (src);
    const auto rel = inside ? current.file.getRelativePathFrom (src) : juce::String();
    bool ok;
    if (caseOnly) { const auto tmp = src.getNonexistentSibling(); ok = src.moveFileTo (tmp) && tmp.moveFileTo (dst); }
    else ok = src.moveFileTo (dst);
    if (! ok) { error = "Couldn't rename the folder (is a file in it open?)"; rescan(); return false; }
    rescan();
    if (inside) followCurrent (dst.getChildFile (rel));
    return true;
}

bool PresetManager::removeFolder (const juce::String& name)
{
    const auto d = dirFile (name);
    if (d == juce::File()) { error = "Folder not found"; return false; }
    const bool inside = current.file.isAChildOf (d);
    if (! d.moveToTrash())
    {
        if (d.findChildFiles (juce::File::findFilesAndDirectories, true).isEmpty() && d.deleteFile()) {}
        else { error = "Couldn't move the folder to the recycle bin"; return false; }
    }
    rescan();
    if (inside) { current.factory = true; current.file = juce::File(); current.id = "factory:" + current.name; current.dir = {}; dirty = true; notifyState(); }
    return true;
}

bool PresetManager::move (const juce::String& id, const juce::String& dir)
{
    for (auto& p : presets)
        if (p.id == id)
        {
            if (p.factory) { error = "Factory and artist presets are locked: DUPLICATE one to make your own"; return false; }
            const auto d = dir.isEmpty() ? folder() : dirFile (dir);
            if (d == juce::File()) { error = "Folder not found"; return false; }
            const auto dest = d.getChildFile (p.file.getFileName());
            if (dest == p.file) return true;
            if (dest.exists()) { error = "There's already a preset called " + p.name + " in " + (dir.isEmpty() ? juce::String ("My presets") : dir); return false; }
            const bool wasCurrent = current.id == id;
            if (! p.file.moveFileTo (dest)) { error = "Couldn't move the file"; return false; }
            rescan();
            if (wasCurrent) followCurrent (dest);
            return true;
        }
    error = "Preset not found"; return false;
}

juce::var PresetManager::foldersJson() const
{
    juce::Array<juce::var> a;
    for (auto& d : dirs) a.add (d);
    return a;
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
        if (p.dir.isNotEmpty()) o->setProperty ("dir", p.dir);      // your folder ("folder" in toJson() is the presets root)
        if (p.type.isNotEmpty() && p.type != p.category) o->setProperty ("type", p.type);
        if (p.note.isNotEmpty()) o->setProperty ("note", p.note);
        a.add (juce::var (o));
    }
    return a;
}

void PresetManager::notifyState()  { listeners.call ([] (Listener& l) { l.presetStateChanged(); }); }
void PresetManager::notifyList()   { listeners.call ([] (Listener& l) { l.presetListChanged(); }); }
} // namespace obs
