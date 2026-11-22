#include "Updater.h"
#include <juce_cryptography/juce_cryptography.h>
#include <juce_events/juce_events.h>

namespace obs
{
namespace
{
const char* stateName (Updater::State s)
{
    switch (s)
    {
        case Updater::State::idle:        return "idle";
        case Updater::State::checking:    return "checking";
        case Updater::State::upToDate:    return "uptodate";
        case Updater::State::available:   return "available";
        case Updater::State::downloading: return "downloading";
        case Updater::State::ready:       return "ready";
        case Updater::State::installing:  return "installing";
        case Updater::State::failed:      return "failed";
    }
    return "idle";
}
// the list, not ".../releases/latest": GitHub's "latest" can be a tagged release (v0.2.0) that isn't a build
juce::String apiUrl()       { return juce::String ("https://api.github.com/repos/") + Updater::repo + "/releases?per_page=20"; }
} // namespace

Updater::Updater() : juce::Thread ("Obsidian updater") {}

Updater::~Updater()
{
    stopDownload = true;
    stopThread (6000);
}

bool Updater::canInstall()
{
   #if JUCE_WINDOWS
    return true;
   #else
    return false;
   #endif
}

bool Updater::trustedUrl (const juce::String& url)
{
    return url.startsWith (juce::String ("https://github.com/") + repo + "/releases/download/") && ! url.containsAnyOf ("\\\"' <>");
}

Updater::Release Updater::parseRelease (const juce::var& j)
{
    Release r;
    if (! j.isObject()) return r;
    r.tag = j["tag_name"].toString();
    r.title = j["name"].toString();
    r.date = j["published_at"].toString();
    const auto body = j["body"].toString().replace ("\r\n", "\n");
    const int at = body.indexOf ("<!-- obsidian");
    r.notes = (at >= 0 ? body.substring (0, at) : body).trim();
    juce::StringPairArray sha;
    if (at >= 0)
    {
        auto block = body.substring (at + 13);
        if (block.contains ("-->")) block = block.upToFirstOccurrenceOf ("-->", false, false);
        for (auto line : juce::StringArray::fromLines (block))
        {
            line = line.trim();
            const auto key = line.upToFirstOccurrenceOf (":", false, false).trim().toLowerCase();
            const auto val = line.fromFirstOccurrenceOf (":", false, false).trim();
            if (key == "build") r.build = val.getIntValue();
            else if (key == "version") r.version = val;
            else if (key == "pack") r.pack = val;
            else if (key == "sha256")
            {
                const auto name = val.upToLastOccurrenceOf (" ", false, false).trim();
                const auto hex = val.fromLastOccurrenceOf (" ", false, false).trim().toLowerCase();
                if (name.isNotEmpty() && hex.length() == 64 && hex.containsOnly ("0123456789abcdef")) sha.set (name, hex);
            }
        }
    }
    if (r.build <= 0) r.build = r.tag.retainCharacters ("0123456789").getIntValue();
    if (auto* assets = j["assets"].getArray())
        for (auto& a : *assets)
        {
            Release::Asset x;
            x.name = a["name"].toString(); x.url = a["browser_download_url"].toString(); x.size = (juce::int64) a["size"];
            x.sha = sha[x.name];
            if (! trustedUrl (x.url) || ! x.name.endsWithIgnoreCase (".exe") || x.sha.isEmpty() || x.size <= 0) continue;
            if (x.name.endsWithIgnoreCase ("-Setup.exe")) r.full = x;
            else if (x.name.endsWithIgnoreCase ("-Update.exe")) r.update = x;
        }
    r.ok = r.build > 0 && r.full.url.isNotEmpty();
    return r;
}

Updater::Release Updater::newestBuild (const juce::var& j)
{
    auto* a = j.getArray();                       // (a JUCE array is an "object" too: test for the list first)
    if (a == nullptr) return parseRelease (j);
    Release best;
    for (auto& e : *a)
    {
        if ((bool) e["prerelease"] || (bool) e["draft"]) continue;
        auto r = parseRelease (e);
        if (r.ok && r.build > best.build) best = r;
    }
    return best;
}

const Updater::Release::Asset& Updater::chosen() const
{
    // the small installer leaves the capture / IR pack alone: only when it's the same pack
    if (latest.update.url.isNotEmpty() && ownPack().isNotEmpty() && latest.pack == ownPack()) return latest.update;
    return latest.full;
}

void Updater::set (State s, const juce::String& err)
{
    { const juce::ScopedLock sl (lock); st = s; error = err; }
    ++changeCount;
}

void Updater::check()
{
    if (isThreadRunning()) return;
    job = Job::check;
    startThread();
}

void Updater::download()
{
    if (isThreadRunning()) return;
    { const juce::ScopedLock sl (lock); if (! latest.ok) return; }
    stopDownload = false;
    job = Job::download;
    startThread();
}

void Updater::cancel()                        { stopDownload = true; }

void Updater::run()
{
    const auto j = job.exchange (Job::none);
    if (j == Job::check) doCheck();
    else if (j == Job::download) doDownload();
}

void Updater::doCheck()
{
    set (State::checking);
    int status = 0;
    auto in = juce::URL (apiUrl()).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                          .withExtraHeaders ("User-Agent: ObsidianAmpSuite\r\nAccept: application/vnd.github+json")
                                                          .withConnectionTimeoutMs (12000).withNumRedirectsToFollow (5).withStatusCode (&status));
    checkedAt = juce::Time::getMillisecondCounterHiRes();
    if (in == nullptr) { set (State::failed, "Couldn't reach GitHub. Are you online?"); return; }
    if (status == 404) { set (State::failed, "No builds are published yet."); return; }
    if (status == 403 || status == 429) { set (State::failed, "GitHub says to wait a little (too many checks). Try again later."); return; }
    if (status != 0 && status != 200) { set (State::failed, "GitHub answered " + juce::String (status) + "."); return; }
    const auto r = newestBuild (juce::JSON::parse (in->readEntireStreamAsString()));
    if (! r.ok) { set (State::failed, "No build with an installer is published yet (the newest may still be building)."); return; }
    { const juce::ScopedLock sl (lock); latest = r; }
    set (ownBuild() > 0 && r.build <= ownBuild() ? State::upToDate : State::available);
}

void Updater::doDownload()
{
    Release::Asset a;
    { const juce::ScopedLock sl (lock); a = chosen(); }
    if (! trustedUrl (a.url) || a.sha.length() != 64) { set (State::failed, "That download isn't one of ours."); return; }

    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ObsidianAmpSuite-update");
    folder.createDirectory();
    const auto target = folder.getChildFile (juce::File::createLegalFileName (a.name));
    auto sha256 = [] (const juce::File& f) { return juce::SHA256 (f).toHexString().removeCharacters (" ").toLowerCase(); };

    // already downloaded earlier?
    if (target.existsAsFile() && target.getSize() == a.size && sha256 (target) == a.sha)
    {
        { const juce::ScopedLock sl (lock); file = target; }
        progress = 1.f; set (State::ready); return;
    }

    progress = 0.f; set (State::downloading);
    int status = 0;
    auto in = juce::URL (a.url).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                       .withExtraHeaders ("User-Agent: ObsidianAmpSuite\r\nAccept: application/octet-stream")
                                                       .withConnectionTimeoutMs (15000).withNumRedirectsToFollow (8).withStatusCode (&status));
    if (in == nullptr || (status != 0 && status != 200)) { set (State::failed, "The download didn't start (GitHub " + juce::String (status) + ")."); return; }

    const auto part = target.withFileExtension ("part");
    part.deleteFile();
    {
        juce::FileOutputStream out (part);
        if (! out.openedOk()) { set (State::failed, "Couldn't write to " + folder.getFullPathName()); return; }
        juce::HeapBlock<char> buf (1 << 16);
        juce::int64 got = 0; int lastPct = -1;
        while (! in->isExhausted())
        {
            if (threadShouldExit() || stopDownload) { out.flush(); part.deleteFile(); progress = 0.f; set (State::available, "Download stopped."); return; }
            const int n = in->read (buf.getData(), 1 << 16);
            if (n < 0) break;
            if (n == 0) { if (in->isExhausted()) break; wait (5); continue; }
            if (! out.write (buf.getData(), (size_t) n)) { part.deleteFile(); set (State::failed, "The disk is full?"); return; }
            got += n;
            if (got > a.size + (1 << 20)) break;                      // more than promised: something's wrong
            progress = (float) juce::jlimit (0.0, 1.0, (double) got / (double) juce::jmax ((juce::int64) 1, a.size));
            const int pct = (int) (progress * 100.f);
            if (pct != lastPct) { lastPct = pct; ++changeCount; }
        }
        out.flush();
    }
    if (part.getSize() != a.size) { part.deleteFile(); progress = 0.f; set (State::failed, "The download was cut off. Try again."); return; }
    if (sha256 (part) != a.sha) { part.deleteFile(); progress = 0.f; set (State::failed, "The download was damaged (checksum). Try again."); return; }
    target.deleteFile();
    if (! part.moveFileTo (target)) { set (State::failed, "Couldn't save the installer."); return; }
    { const juce::ScopedLock sl (lock); file = target; }
    progress = 1.f;
    set (State::ready);
}

bool Updater::install (bool standalone, juce::String& message)
{
    juce::File f;
    { const juce::ScopedLock sl (lock); f = file; if (st != State::ready) { message = "Download the update first."; return false; } }
    if (! f.existsAsFile()) { set (State::available); message = "The downloaded installer is gone. Download it again."; return false; }
    if (! canInstall())
    {
        f.revealToUser();
        message = "Run the installer yourself (it's shown in its folder).";
        return false;
    }
    // Standalone: a quiet install (just a progress bar), the app closes and the installer starts it again.
    // In a DAW: the installer's own window; it offers to close the DAW, which holds the plugin file.
    const juce::String args = standalone ? "/SP- /SILENT /NOCANCEL /NORESTART /CLOSEAPPLICATIONS" : "/SP- /NORESTART";
    if (! f.startAsProcess (args))
    {
        message = "The installer didn't start (did Windows ask, and was it allowed?).";
        return false;
    }
    set (State::installing);
    message = standalone ? "Installing: the app closes now and opens again when it's done."
                         : "The installer is open. Save your project first: it will ask to close your DAW (the plugin file is in use).";
    if (standalone)
        juce::Timer::callAfterDelay (900, [] { if (auto* app = juce::JUCEApplicationBase::getInstance()) app->systemRequestedQuit(); });
    return true;
}

juce::var Updater::toJson() const
{
    const juce::ScopedLock sl (lock);
    auto o = new juce::DynamicObject();
    o->setProperty ("state", stateName (st));
    o->setProperty ("error", error);
    o->setProperty ("own", ownBuild());
    o->setProperty ("latest", latest.build);
    o->setProperty ("version", latest.version);
    o->setProperty ("title", latest.title);
    o->setProperty ("notes", latest.notes.substring (0, 1200));
    o->setProperty ("date", latest.date);
    const auto& a = chosen();
    o->setProperty ("size", (double) a.size);
    o->setProperty ("full", latest.ok && &a == &latest.full);
    o->setProperty ("progress", progress.load());
    o->setProperty ("canInstall", canInstall());
    return juce::var (o);
}
} // namespace obs
