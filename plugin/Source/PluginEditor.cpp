#include "PluginEditor.h"
#include "ObsidianAssets.h"

namespace obs
{
ObsidianEditor::ObsidianEditor (ObsidianProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setOpaque (true);

    // Bridge first: the browser's native function forwards into it.
    bridge = std::make_unique<UiBridge> (proc,
        [this] (const juce::String& ev, const juce::var& payload) { if (web != nullptr) web->emitEventIfBrowserIsVisible (ev, payload); },
        [this] (float s) { setUiScale (s); });

    using WBC = juce::WebBrowserComponent;
    auto options = WBC::Options{}
       #if JUCE_WINDOWS
        .withBackend (WBC::Options::Backend::webview2)      // Edge WebView2 (part of Windows 10/11)
        .withWinWebView2Options (WBC::Options::WinWebView2{}
                                    .withUserDataFolder (AppSettings::dataFolder().getChildFile ("WebView2"))
                                    .withStatusBarDisabled()
                                    .withBuiltInErrorPageDisabled()
                                    .withBackgroundColour (juce::Colour (0xff0c0c0d)))
       #endif
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withNativeFunction ("obsidianCall", [this] (const juce::Array<juce::var>& args, WBC::NativeFunctionCompletion done)
        {
            if (args.isEmpty()) { done (juce::var()); return; }
            juce::Array<juce::var> rest;
            for (int i = 1; i < args.size(); ++i) rest.add (args[i]);
            bridge->handle (args[0].toString(), rest, [done] (juce::var r) { done (r); });
        })
       #if JUCE_WEB_BROWSER_RESOURCE_PROVIDER_AVAILABLE && ! JUCE_LINUX
        .withResourceProvider ([this] (const juce::String& url) { return provide (url); })
       #endif
        ;

    if (WBC::areOptionsSupported (options))
    {
        web = std::make_unique<WBC> (options);
        addAndMakeVisible (*web);
       #if JUCE_LINUX
        // Linux (development only): JUCE's WebKit pipe can't carry a page this big, so load it from a temp folder.
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("obsidian-ui");
        dir.createDirectory(); dir.getChildFile ("fonts").createDirectory();
        for (int i = 0; i < ObsidianAssets::namedResourceListSize; ++i)
        {
            const juce::String name (ObsidianAssets::originalFilenames[i]);
            if (name.endsWithIgnoreCase (".json")) continue;
            if (auto r = provide (name))
                dir.getChildFile (name.endsWithIgnoreCase (".woff2") ? "fonts/" + name : name).replaceWithData (r->data.data(), r->data.size());
        }
        // amp faces come from the amp pack (Documents/<product>/Faces)
        const auto faces = Rig::contentFolder().getChildFile ("Faces");
        if (faces.isDirectory()) { dir.getChildFile ("faces").deleteRecursively(); faces.createSymbolicLink (dir.getChildFile ("faces"), true); }
        web->goToURL (juce::URL (dir.getChildFile ("obsidian-demo.html")).toString (false));
       #elif JUCE_WEB_BROWSER_RESOURCE_PROVIDER_AVAILABLE
        web->goToURL (WBC::getResourceProviderRoot());
       #endif
    }
    else
    {
        fallbackMessage = "Obsidian needs the Microsoft Edge WebView2 Runtime to show its interface.\n"
                          "It is part of Windows 11; on older systems install it from\n"
                          "https://developer.microsoft.com/microsoft-edge/webview2\n\n"
                          "The sound engine is running normally.";
    }

    // Resizable, keeps the 1360 x 836 shape; the UI scales itself to fit.
    constrainer.setFixedAspectRatio ((double) kWidth / kHeight);
    constrainer.setSizeLimits (kWidth / 2, kHeight / 2, kWidth * 2, kHeight * 2);
    setConstrainer (&constrainer);
    setResizable (true, true);
    const float scale = juce::jlimit (0.5f, 2.0f, (float) (double) proc.settings().get ("uiScale"));
    setSize (juce::roundToInt (kWidth * scale), juce::roundToInt (kHeight * scale));
}

ObsidianEditor::~ObsidianEditor()
{
    web.reset();      // stop the page before the bridge goes away
    bridge.reset();
}

void ObsidianEditor::setUiScale (float s)
{
    s = juce::jlimit (0.5f, 2.0f, s);
    setSize (juce::roundToInt (kWidth * s), juce::roundToInt (kHeight * s));
}

void ObsidianEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c0c0d));
    if (fallbackMessage.isNotEmpty())
    {
        g.setColour (juce::Colour (0xffe9e6df));
        g.setFont (juce::FontOptions (16.0f));
        g.drawFittedText (fallbackMessage, getLocalBounds().reduced (40), juce::Justification::centred, 8);
    }
}

void ObsidianEditor::resized()
{
    if (web != nullptr) web->setBounds (getLocalBounds());
}

// ------------------------------------------------------------------------ resources
std::vector<std::byte> ObsidianEditor::bytes (const void* data, size_t size)
{
    const auto* b = static_cast<const std::byte*> (data);
    return std::vector<std::byte> (b, b + size);
}

juce::String ObsidianEditor::mimeFor (const juce::String& path)
{
    const auto ext = path.fromLastOccurrenceOf (".", false, false).toLowerCase();
    if (ext == "html") return "text/html";
    if (ext == "js")   return "text/javascript";
    if (ext == "css")  return "text/css";
    if (ext == "png")  return "image/png";
    if (ext == "webp") return "image/webp";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "svg")  return "image/svg+xml";
    if (ext == "json") return "application/json";
    if (ext == "woff2") return "font/woff2";
    return "application/octet-stream";
}

/** Developer mode: Settings key "uiDevFolder" (or env OBSIDIAN_UI_DEV) points at a folder
    containing obsidian-demo.html etc.; the UI is then loaded from disk, so UI edits show up
    after a reload, without rebuilding the plugin. */
juce::File ObsidianEditor::devFolder() const
{
    auto env = juce::SystemStats::getEnvironmentVariable ("OBSIDIAN_UI_DEV", {});
    juce::File f (env.isNotEmpty() ? env : proc.settings().get ("uiDevFolder").toString());
    return (f.getFullPathName().isNotEmpty() && f.isDirectory() && f.getChildFile ("obsidian-demo.html").existsAsFile()) ? f : juce::File();
}

std::optional<juce::WebBrowserComponent::Resource> ObsidianEditor::provide (const juce::String& url)
{
    auto path = url.upToFirstOccurrenceOf ("?", false, false).trimCharactersAtStart ("/");
    if (path.isEmpty() || path == "index.html") path = "obsidian-demo.html";
    const auto name = path.fromLastOccurrenceOf ("/", false, false);

    juce::MemoryBlock mb;
    bool found = false;
    // amp faces (images of the amp pack's amps): Documents\<product>\Faces\img\<file>
    if (path.startsWith ("faces/"))
    {
        const auto rel = path.fromFirstOccurrenceOf ("faces/", false, false);
        if (rel.contains ("..") || rel.containsChar ('\\') || rel.containsChar (':')) return std::nullopt;
        const auto f = Rig::contentFolder().getChildFile ("Faces").getChildFile (rel);
        if (! f.existsAsFile() || ! f.loadFileAsData (mb)) return std::nullopt;
        return juce::WebBrowserComponent::Resource { bytes (mb.getData(), mb.getSize()), mimeFor (name) };
    }
    if (auto dev = devFolder(); dev != juce::File())
    {
        auto f = dev.getChildFile (path);
        if (! f.existsAsFile()) f = dev.getChildFile (name);
        found = f.existsAsFile() && f.loadFileAsData (mb);
    }
    if (! found)
    {
        // Compiled-in copy (BinaryData names: dots become underscores).
        for (int i = 0; i < ObsidianAssets::namedResourceListSize; ++i)
            if (juce::String (ObsidianAssets::originalFilenames[i]) == name)
            {
                int size = 0;
                if (const char* d = ObsidianAssets::getNamedResource (ObsidianAssets::namedResourceList[i], size))
                { mb.append (d, (size_t) size); found = true; }
                break;
            }
    }
    if (! found) return std::nullopt;

    if (name == "obsidian-demo.html")
    {
        // Inject the native bridge into the shared UI file (the browser demo runs without it).
        auto html = mb.toString();
        const juce::String inject = "<link rel=\"stylesheet\" href=\"obsidian-native.css\">\n<script src=\"obsidian-pedals.js\"></script>\n<script src=\"obsidian-bridge.js\"></script>\n";
        const int at = html.lastIndexOfIgnoreCase ("</body>");
        html = at >= 0 ? html.substring (0, at) + inject + html.substring (at) : html + inject;
        const auto utf8 = html.toUTF8();
        return juce::WebBrowserComponent::Resource { bytes (utf8.getAddress(), utf8.sizeInBytes() - 1), "text/html" };
    }
    return juce::WebBrowserComponent::Resource { bytes (mb.getData(), mb.getSize()), mimeFor (name) };
}
} // namespace obs
