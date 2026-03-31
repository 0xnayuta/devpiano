#include "PluginEditorWindow.h"

juce::String PluginEditorWindow::makeWindowTitle(const juce::String& pluginName)
{
    if (pluginName.isEmpty())
        return "Plugin Editor";

    auto title = pluginName;
    title << " Editor";
    return title;
}

PluginEditorWindow::PluginEditorWindow(const juce::String& pluginName,
                                       std::unique_ptr<juce::AudioProcessorEditor> editor,
                                       std::function<void()> onClose)
    : juce::DocumentWindow(makeWindowTitle(pluginName),
                           juce::Desktop::getInstance().getDefaultLookAndFeel()
                               .findColour(juce::ResizableWindow::backgroundColourId),
                           juce::DocumentWindow::closeButton),
      closeCallback(std::move(onClose))
{
    jassert(editor != nullptr);

    setUsingNativeTitleBar(true);
    setResizable(editor->isResizable(), true);
    setContentOwned(editor.release(), true);
}

void PluginEditorWindow::closeButtonPressed()
{
    if (closeCallback)
        closeCallback();
}
