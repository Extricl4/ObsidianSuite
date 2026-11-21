#pragma once
// =============================================================================
//  AppSettings: things that belong to YOU, not to a preset (theme, preset
//  folder, MIDI mappings, audio options...). Stored in
//    Windows: %APPDATA%\Obsidian Amp Suite\settings.xml
//  and shared by every Obsidian instance (VST3 and standalone).
//  Adding a setting: give it a default in defaults() and read/write it with
//  get()/set(). The UI sees every key automatically.
// =============================================================================
#include <juce_data_structures/juce_data_structures.h>

namespace obs
{
class AppSettings
{
public:
    AppSettings()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "settings";
        o.filenameSuffix = ".xml";
        o.folderName = OBSIDIAN_PRODUCT_NAME;
        o.osxLibrarySubFolder = "Application Support";
        o.storageFormat = juce::PropertiesFile::storeAsXML;
        o.millisecondsBeforeSaving = 500;
        o.commonToAllUsers = false;
        o.processLock = &interProcess;
        file = std::make_unique<juce::PropertiesFile> (o);
    }

    static juce::File dataFolder()
    {
        auto f = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile (OBSIDIAN_PRODUCT_NAME);
        f.createDirectory();
        return f;
    }
    static juce::File defaultPresetFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (OBSIDIAN_PRODUCT_NAME).getChildFile ("Presets");
    }

    static const juce::NamedValueSet& defaults()
    {
        static const juce::NamedValueSet d = []
        {
            juce::NamedValueSet s;
            s.set ("theme", "obsidian");
            s.set ("accent", "");
            s.set ("themeFx", true);           // moving theme details (particles, flicker)
            s.set ("tooltips", true);
            s.set ("uiScale", 1.0);
            s.set ("presetFolder", defaultPresetFolder().getFullPathName());
            s.set ("openLastPreset", true);
            s.set ("lastPreset", "");
            s.set ("markUnsaved", true);
            s.set ("askBeforeSwitching", false);
            s.set ("oversampling", 2);
            s.set ("renderQuality", "Highest");
            s.set ("inputCalibration", 0.0);
            s.set ("muteWhileTuning", true);
            s.set ("liveTuner", false);
            s.set ("latencyCompensation", true);
            s.set ("inputMode", 0);
            s.set ("midiChannel", 0);
            s.set ("midiInput", "All devices");
            s.set ("pcLoadsPresets", true);
            s.set ("midiMappings", "[]");
            s.set ("metronomeVolume", 0.7);
            s.set ("uiDevFolder", "");
            return s;
        }();
        return d;
    }

    juce::var get (const juce::String& key) const
    {
        const juce::ScopedLock sl (lock);
        if (file->containsKey (key))
        {
            const auto& d = defaults()[juce::Identifier (key)];
            const auto raw = file->getValue (key);
            if (d.isBool())   return raw == "1" || raw.equalsIgnoreCase ("true");
            if (d.isInt())    return raw.getIntValue();
            if (d.isDouble()) return raw.getDoubleValue();
            return raw;
        }
        return defaults()[juce::Identifier (key)];
    }
    void set (const juce::String& key, const juce::var& v)
    {
        {
            const juce::ScopedLock sl (lock);
            file->setValue (key, v.isBool() ? juce::var ((bool) v ? "1" : "0") : v);
        }
        listeners.call ([&] (Listener& l) { l.settingChanged (key, v); });
    }
    juce::DynamicObject::Ptr toObject() const
    {
        auto o = juce::DynamicObject::Ptr (new juce::DynamicObject());
        for (auto& nv : defaults()) o->setProperty (nv.name, get (nv.name.toString()));
        return o;
    }
    void flush()                    { const juce::ScopedLock sl (lock); file->saveIfNeeded(); }

    struct Listener { virtual ~Listener() = default; virtual void settingChanged (const juce::String& key, const juce::var& value) = 0; };
    void addListener (Listener* l)    { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

private:
    juce::InterProcessLock interProcess { "ObsidianSuiteSettings" };
    std::unique_ptr<juce::PropertiesFile> file;
    juce::CriticalSection lock;
    juce::ListenerList<Listener> listeners;
};
} // namespace obs
