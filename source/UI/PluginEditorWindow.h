#pragma once

#include "JuceHeader.h"

#include <functional>
#include <memory>

class PluginEditorWindow final : public juce::DocumentWindow
{
public:
    PluginEditorWindow(const juce::String& pluginName,
                       std::unique_ptr<juce::AudioProcessorEditor> editor,
                       std::function<void()> onClose);

    void closeButtonPressed() override;

private:
    [[nodiscard]] static juce::String makeWindowTitle(const juce::String& pluginName);

    std::function<void()> closeCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditorWindow)
};
