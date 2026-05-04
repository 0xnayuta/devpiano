#pragma once

#include <JuceHeader.h>

class PluginPanel final : public juce::Component
{
public:
    struct State
    {
        juce::StringArray availablePluginNames;
        juce::String preferredSelection;
        juce::String pluginListText;
        juce::String availableFormatsDescription;
        juce::String lastScanSummary;
        juce::String currentPluginName;
        juce::String lastLoadError;
        juce::String lastPluginName;
        double preparedSampleRate = 0.0;
        int preparedBlockSize = 0;
        bool supportsVst3 = false;
        bool hasLoadedPlugin = false;
        bool isPrepared = false;
        bool isEditorOpen = false;
    };

    PluginPanel();

    void resized() override;

    void updateState(const State& state);
    void setPluginPathText(const juce::String& text);
    [[nodiscard]] juce::String getPluginPathText() const;
    [[nodiscard]] juce::String getSelectedPluginName() const;

    std::function<void()> onScanRequested;
    std::function<void()> onLoadRequested;
    std::function<void()> onUnloadRequested;
    std::function<void()> onToggleEditorRequested;

private:
    juce::File getInitialBrowseDirectory() const;
    void setupBrowseButton();
    juce::Label pluginStatusLabel;
    juce::Label pluginPathLabel;
    juce::Label pluginSelectionLabel;
    juce::Label pluginListLabel;

    juce::TextButton scanPluginsButton { "Scan VST3" };
    juce::TextButton browseButton { "..." };
    juce::TextButton loadPluginButton { "Load" };
    juce::TextButton unloadPluginButton { "Unload" };
    juce::TextButton openEditorButton { "Open Editor" };

    juce::TextEditor pluginPathEditor;
    juce::ComboBox pluginSelector;
    juce::TextEditor pluginListEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginPanel)
};
