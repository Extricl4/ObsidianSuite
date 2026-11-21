#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "Editor/UiBridge.h"

namespace obs
{
/** The plugin window: hosts the Obsidian web UI (../ui/obsidian-demo.html + WebUI/obsidian-bridge.js). */
class ObsidianEditor : public juce::AudioProcessorEditor
{
public:
    static constexpr int kWidth = 1360, kHeight = 836;

    explicit ObsidianEditor (ObsidianProcessor&);
    ~ObsidianEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setUiScale (float scale);

private:
    std::optional<juce::WebBrowserComponent::Resource> provide (const juce::String& url);
    static std::vector<std::byte> bytes (const void* data, size_t size);
    static juce::String mimeFor (const juce::String& path);
    juce::File devFolder() const;
    juce::File liveUiFolder() const;

    ObsidianProcessor& proc;
    std::unique_ptr<UiBridge> bridge;
    std::unique_ptr<juce::WebBrowserComponent> web;
    juce::String fallbackMessage;
    juce::ComponentBoundsConstrainer constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ObsidianEditor)
};
} // namespace obs
