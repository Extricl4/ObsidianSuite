#pragma once
/*  ORPHEUS · the window: the same page as the browser preview (WebUI/index.html, built by tools/make_ui.py) in a
    WebView (Edge WebView2 on Windows), with WebUI/orpheus-bridge.js added so it talks to the engine.            */
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

namespace orph
{
class OrpheusEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit OrpheusEditor (OrpheusProcessor&);
    ~OrpheusEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kWidth = 1368, kHeight = 620;      // the page's own size (it scales itself to fit)

private:
    void timerCallback() override;
    juce::var handle (const juce::String& cmd, const juce::Array<juce::var>& args);
    juce::var stateForPage();
    std::optional<juce::WebBrowserComponent::Resource> provide (const juce::String& url);
    void emit (const juce::var& payload);

    OrpheusProcessor& proc;
    std::unique_ptr<juce::WebBrowserComponent> web;
    juce::String fallbackMessage;
    juce::ComponentBoundsConstrainer constrainer;
    std::array<float, P_COUNT> pageVals {};                 // what the page shows (so only real changes are sent)
    bool pageReady = false; int stateGen = 0;
    juce::String lastPos;
    void uiTestStep();                                    // ORPHEUS_UITEST=<report file>: drives the page and checks the bridge
    juce::File uiTestFile; int uiTestTick = 0; juce::StringArray uiTestLog;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrpheusEditor)
};
} // namespace orph
