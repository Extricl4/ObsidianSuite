#pragma once
// =============================================================================
//  Updater: in-app updates (Settings > About > UPDATES).
//
//  Every Windows build on GitHub publishes itself as a release "build-<n>" with
//  two installers: the full one, and a small update one without the big
//  capture / IR pack. The release notes end with a hidden block:
//      <!-- obsidian
//      build: 145
//      version: 0.2.0
//      pack: 3f2a...              (which pedal and amp pack it carries)
//      sha256: ObsidianAmpSuite-0.2.0-Setup.exe <hex>
//      sha256: ObsidianAmpSuite-0.2.0-Update.exe <hex>
//      -->
//  The app compares the build with its own (OBSIDIAN_BUILD_NUMBER), downloads
//  the small installer (the full one when the pack changed), checks its size
//  and SHA-256, and runs it. Windows asks you to allow the install.
//
//  One Updater per process (shared by every plugin window). All network work
//  runs on its own thread.
// =============================================================================
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <atomic>

#ifndef OBSIDIAN_BUILD_NUMBER
 #define OBSIDIAN_BUILD_NUMBER 0
#endif
#ifndef OBSIDIAN_PACK_HASH
 #define OBSIDIAN_PACK_HASH ""
#endif

namespace obs
{
class Updater : private juce::Thread
{
public:
    enum class State { idle, checking, upToDate, available, downloading, ready, installing, failed };

    Updater();
    ~Updater() override;

    static constexpr const char* repo = "Extricl4/ObsidianSuite";
    static int ownBuild()                     { return OBSIDIAN_BUILD_NUMBER; }
    static juce::String ownPack()             { return OBSIDIAN_PACK_HASH; }
    static bool canInstall();                 // Windows: runs the installer; elsewhere: shows the file

    /** Ask GitHub for the newest build (in the background). */
    void check();
    /** Download the newest build (in the background); when ready, the UI calls install(). */
    void download();
    void cancel();
    /** Run the downloaded installer. standalone: a quiet install, then the app closes and the
        installer starts it again. In a DAW: the installer's own window (it offers to close the DAW,
        which holds the plugin file). Returns false with a reason if it couldn't start. */
    bool install (bool standalone, juce::String& message);

    juce::var toJson() const;                 // for the UI (obs:update)
    int changes() const                       { return changeCount.load(); }
    State state() const                       { const juce::ScopedLock sl (lock); return st; }

    // ---- pieces the self-test checks ----------------------------------------------------
    struct Release
    {
        int build = 0; juce::String version, tag, title, notes, date, pack;
        struct Asset { juce::String name, url, sha; juce::int64 size = 0; } full, update;
        bool ok = false;
    };
    static Release parseRelease (const juce::var& json);
    /** The newest published build in GitHub's release list (a list, or one release). Releases without
        our hidden block (a "v0.2.0" tag release), pre-releases and drafts are skipped. */
    static Release newestBuild (const juce::var& json);
    /** True when this download URL really is one of our release files. */
    static bool trustedUrl (const juce::String& url);

private:
    enum class Job { none, check, download };
    void run() override;
    void doCheck();
    void doDownload();
    void set (State s, const juce::String& err = {});
    const Release::Asset& chosen() const;     // the small installer unless the pack changed

    mutable juce::CriticalSection lock;
    State st = State::idle;
    juce::String error;
    Release latest;
    juce::File file;                           // the downloaded installer
    std::atomic<float> progress { 0.f };
    std::atomic<int> changeCount { 1 };
    std::atomic<Job> job { Job::none };
    std::atomic<bool> stopDownload { false };
    double checkedAt = 0;
};
} // namespace obs
