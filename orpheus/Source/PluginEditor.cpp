#include "PluginEditor.h"
#include "OrpheusAssets.h"

namespace orph
{
static std::vector<std::byte> bytesOf (const void* data, size_t size)
{
    const auto* b = static_cast<const std::byte*> (data);
    return std::vector<std::byte> (b, b + size);
}
static juce::String mimeFor (const juce::String& path)
{
    const auto ext = path.fromLastOccurrenceOf (".", false, false).toLowerCase();
    if (ext == "html") return "text/html";
    if (ext == "js") return "text/javascript";
    if (ext == "css") return "text/css";
    if (ext == "png") return "image/png";
    if (ext == "woff2") return "font/woff2";
    if (ext == "json") return "application/json";
    return "application/octet-stream";
}
static juce::File dataFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("ORPHEUS");
}

OrpheusEditor::OrpheusEditor (OrpheusProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setOpaque (true);
    using WBC = juce::WebBrowserComponent;
    auto options = WBC::Options {}
       #if JUCE_WINDOWS
        .withBackend (WBC::Options::Backend::webview2)
        .withWinWebView2Options (WBC::Options::WinWebView2 {}
                                     .withUserDataFolder (dataFolder().getChildFile ("WebView2"))
                                     .withStatusBarDisabled()
                                     .withBuiltInErrorPageDisabled()
                                     .withBackgroundColour (juce::Colour (0xff111113)))
       #endif
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withNativeFunction ("orpheusCall", [this] (const juce::Array<juce::var>& args, WBC::NativeFunctionCompletion done)
        {
            if (args.isEmpty()) { done (juce::var()); return; }
            juce::Array<juce::var> rest; for (int i = 1; i < args.size(); ++i) rest.add (args[i]);
            done (handle (args[0].toString(), rest));
        })
       #if JUCE_WEB_BROWSER_RESOURCE_PROVIDER_AVAILABLE && ! JUCE_LINUX
        .withResourceProvider ([this] (const juce::String& url) { return provide (url); })
       #endif
        ;
    for (int i = 0; i < P_COUNT; ++i) pageVals[(size_t) i] = std::numeric_limits<float>::quiet_NaN();

    if (WBC::areOptionsSupported (options))
    {
        web = std::make_unique<WBC> (options);
        addAndMakeVisible (*web);
       #if JUCE_LINUX
        // Linux (development only): the page is loaded from a temporary folder
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("orpheus-ui");
        dir.createDirectory(); dir.getChildFile ("fonts").createDirectory();
        for (int i = 0; i < OrpheusAssets::namedResourceListSize; ++i)
        {
            const juce::String name (OrpheusAssets::originalFilenames[i]);
            if (auto r = provide (name))
                dir.getChildFile (name.endsWithIgnoreCase (".woff2") ? "fonts/" + name : name).replaceWithData (r->data.data(), r->data.size());
        }
        web->goToURL (juce::URL (dir.getChildFile ("index.html")).toString (false));
       #elif JUCE_WEB_BROWSER_RESOURCE_PROVIDER_AVAILABLE
        web->goToURL (WBC::getResourceProviderRoot());
       #endif
    }
    else
        fallbackMessage = "ORPHEUS needs the Microsoft Edge WebView2 Runtime to show its window.\n"
                          "It is part of Windows 11; on older systems install it from\n"
                          "https://developer.microsoft.com/microsoft-edge/webview2\n\n"
                          "The sound is running normally.";

    constrainer.setFixedAspectRatio ((double) kWidth / kHeight);
    constrainer.setSizeLimits (kWidth / 2, kHeight / 2, kWidth * 2, kHeight * 2);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (kWidth, kHeight);
    stateGen = proc.stateGen.load();
    if (auto t = juce::SystemStats::getEnvironmentVariable ("ORPHEUS_UITEST", {}); t.isNotEmpty()) uiTestFile = juce::File (t);
    startTimerHz (30);
}

OrpheusEditor::~OrpheusEditor() { stopTimer(); web.reset(); }

void OrpheusEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff111113));
    if (fallbackMessage.isNotEmpty())
    {
        g.setColour (juce::Colour (0xffe9e6df)); g.setFont (juce::FontOptions (16.0f));
        g.drawFittedText (fallbackMessage, getLocalBounds().reduced (40), juce::Justification::centred, 8);
    }
}
void OrpheusEditor::resized() { if (web) web->setBounds (getLocalBounds()); }

void OrpheusEditor::emit (const juce::var& payload)
{
    if (web && pageReady) web->emitEventIfBrowserIsVisible ("orph", payload);
}

/* ------------------------------------------------------------ the page's calls */
juce::var OrpheusEditor::stateForPage()
{
    auto* o = new juce::DynamicObject(); auto* vals = new juce::DynamicObject();
    juce::Array<juce::var> ids;
    for (int i = 0; i < P_COUNT; ++i)
    {
        const auto* d = &paramDefs()[i];
        const float v = proc.apvts.getRawParameterValue (d->id)->load();
        vals->setProperty (d->id, v); ids.add (juce::String (d->id)); pageVals[(size_t) i] = v;
    }
    o->setProperty ("ids", ids); o->setProperty ("values", juce::var (vals));
    const auto song = proc.songJson();
    o->setProperty ("song", song.isNotEmpty() && song.startsWith ("{") ? juce::var (song) : juce::var());
    o->setProperty ("extras", juce::JSON::toString (proc.uiExtras, true));
    return juce::var (o);
}

juce::var OrpheusEditor::handle (const juce::String& cmd, const juce::Array<juce::var>& a)
{
    auto num = [&] (int i, double d = 0) { return i < a.size() ? (double) a[i] : d; };
    auto notes = [&] (int i, float* out) { int n = 0; if (i < a.size()) if (auto* arr = a[i].getArray()) for (auto& x : *arr) if (n < 4) out[n++] = (float) (double) x; return n; };
    if (cmd == "hello") { pageReady = true; return stateForPage(); }
    if (cmd == "set")
    {
        const int i = paramIndex (a[0].toString()); if (i < 0) return {};
        const float v = (float) num (1);
        pageVals[(size_t) i] = v;
        proc.setParamFromUi (i, v, num (2) > .5);
        pageVals[(size_t) i] = proc.apvts.getRawParameterValue (paramDefs()[i].id)->load();   // (as stored: no echo)
        return {};
    }
    if (cmd == "song") { proc.setSong (a[0].toString(), a.size() > 1 ? a[1] : juce::var()); return {}; }
    if (cmd == "transport") { proc.transport (a[0].toString(), num (1)); return {}; }
    if (cmd == "audition") { float n[4]; const int c = notes (0, n); proc.audition (n, c, (float) num (1, .8), num (2) > .5); return {}; }
    if (cmd == "auditionPitches") { float n[4]; const int c = notes (0, n); proc.auditionPitches (n, c); return {}; }
    if (cmd == "auditionOff") { proc.auditionOff(); return {}; }
    if (cmd == "note") { proc.uiNote ((int) num (0), num (1) > .5, (float) num (2, .9)); return {}; }
    if (cmd == "bend") { proc.setBend ((float) num (0)); return {}; }
    if (cmd == "mod") { proc.setMod ((float) num (0)); return {}; }
    if (cmd == "levelRoll") { proc.levelRoll ((float) num (0, -23.5)); return {}; }
    if (cmd == "extras") { auto v = juce::JSON::parse (a[0].toString()); if (v.isObject()) proc.uiExtras = v; return {}; }
    return {};
}

/* ------------------------------------------------------------ to the page, 30 times a second */
void OrpheusEditor::timerCallback()
{
    if (uiTestFile != juce::File()) uiTestStep();
    if (! pageReady) return;
    if (proc.stateGen.load() != stateGen)                    // a saved state was loaded (the DAW opened a project)
    {
        stateGen = proc.stateGen.load();
        juce::DynamicObject::Ptr o (new juce::DynamicObject()); o->setProperty ("state", stateForPage()); emit (juce::var (o.get()));
        return;
    }
    juce::DynamicObject::Ptr o (new juce::DynamicObject()), ch (new juce::DynamicObject()), pos (new juce::DynamicObject());
    // settings the DAW moved (automation)
    bool any = false;
    for (int i = 0; i < P_COUNT; ++i)
    {
        const float v = proc.apvts.getRawParameterValue (paramDefs()[i].id)->load();
        if (! (v == pageVals[(size_t) i])) { pageVals[(size_t) i] = v; ch->setProperty (paramDefs()[i].id, v); any = true; }
    }
    if (any) o->setProperty ("params", juce::var (ch.get()));
    // where the player is
    const auto ps = proc.position();
    pos->setProperty ("ph", ps.ph); pos->setProperty ("play", ps.playing); pos->setProperty ("host", ps.host);
    const auto key = juce::String (ps.ph, 4) + (ps.playing ? "p" : "s");
    const bool posChanged = key != lastPos; lastPos = key;
    if (posChanged) o->setProperty ("pos", juce::var (pos.get()));
    const float roll = proc.takeRollTrim();
    if (! std::isnan (roll)) o->setProperty ("roll", roll);
    if (any || posChanged || ! std::isnan (roll)) emit (juce::var (o.get()));
}

/* ------------------------------------------------------------ a check of the whole chain (page <-> bridge <-> engine) */
void OrpheusEditor::uiTestStep()
{
    ++uiTestTick;
    auto js = [this] (const juce::String& label, const juce::String& code)
    {
        web->evaluateJavascript (code, [this, label] (juce::WebBrowserComponent::EvaluationResult r)
        {
            uiTestLog.add (label + ": " + (r.getResult() != nullptr ? juce::JSON::toString (*r.getResult(), true) : juce::String ("(error) ") + (r.getError() ? r.getError()->message : "")));
        });
    };
    if (uiTestTick == 30 * 6)                                  // the page has loaded
    {
        uiTestLog.add ("pageReady (hello received): " + juce::String (pageReady ? "yes" : "NO"));
        js ("page", "JSON.stringify({ kit: !!window.__sirenKit, native: !!(window.__sirenKit && window.__sirenKit.native), cutoff: window.__sirenKit.S['syn.flt.cutoff'] })");
        js ("set+play", "(() => { const K = window.__sirenKit; K.set('syn.flt.cutoff', 2345, 'ui'); document.querySelector('.or-k.play').click(); return 'clicked'; })()");
    }
    if (uiTestTick == 30 * 8)
    {
        const auto ps = proc.position();
        uiTestLog.add ("engine: cutoff " + juce::String (proc.apvts.getRawParameterValue ("syn.flt.cutoff")->load()) + ", playing " + juce::String (ps.playing ? "yes" : "NO") + " at beat " + juce::String (ps.ph, 2));
        js ("page while playing", "JSON.stringify({ playing: window.__sirenKit.orphLink.playing() })");
        if (auto* q = proc.apvts.getParameter ("syn.fx.rev.on")) q->setValueNotifyingHost (1.f);   // as DAW automation would
        if (auto* q = proc.apvts.getParameter ("syn.flt.res")) q->setValueNotifyingHost (q->convertTo0to1 (77.f));
    }
    if (uiTestTick == 30 * 9)
    {
        js ("page after automation", "JSON.stringify({ rev: window.__sirenKit.S['syn.fx.rev.on'], res: window.__sirenKit.S['syn.flt.res'] })");
        js ("stop", "(() => { document.querySelector('.or-k.play').click(); return 'clicked'; })()");
    }
    if (uiTestTick == 30 * 10)
    {
        uiTestLog.add ("engine after stop: playing " + juce::String (proc.position().playing ? "YES" : "no"));
        uiTestFile.replaceWithText (uiTestLog.joinIntoString ("\n") + "\n");
        if (auto* app = juce::JUCEApplicationBase::getInstance()) app->systemRequestedQuit();
    }
}

/* ------------------------------------------------------------ the page's files (compiled in) */
std::optional<juce::WebBrowserComponent::Resource> OrpheusEditor::provide (const juce::String& url)
{
    auto path = url.upToFirstOccurrenceOf ("?", false, false).trimCharactersAtStart ("/");
    if (path.isEmpty()) path = "index.html";
    const auto name = path.fromLastOccurrenceOf ("/", false, false);
    juce::MemoryBlock mb; bool found = false;
    for (int i = 0; i < OrpheusAssets::namedResourceListSize; ++i)
        if (juce::String (OrpheusAssets::originalFilenames[i]) == name)
        {
            int size = 0;
            if (const char* d = OrpheusAssets::getNamedResource (OrpheusAssets::namedResourceList[i], size)) { mb.append (d, (size_t) size); found = true; }
            break;
        }
    if (! found) return std::nullopt;
    if (name == "index.html")
    {
        auto html = mb.toString();
        const juce::String inject = "<script src=\"orpheus-bridge.js\"></script>\n";
        const int at = html.lastIndexOfIgnoreCase ("</body>");
        html = at >= 0 ? html.substring (0, at) + inject + html.substring (at) : html + inject;
        const auto utf8 = html.toUTF8();
        return juce::WebBrowserComponent::Resource { bytesOf (utf8.getAddress(), utf8.sizeInBytes() - 1), "text/html" };
    }
    return juce::WebBrowserComponent::Resource { bytesOf (mb.getData(), mb.getSize()), mimeFor (name) };
}
} // namespace orph
