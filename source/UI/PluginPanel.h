#pragma once

#include <JuceHeader.h>
#include "PluginTypes.h"

class PluginPanel final : public juce::Component {
public:
    using State = PluginPanelState;

    PluginPanel();

    void resized() override;

    void updateState(const State& state);
    void setPluginPathText(const juce::String& text);
    [[nodiscard]] juce::String getPluginPathText() const;
    void setInstrumentFilterVisible(bool visible);
    [[nodiscard]] juce::String getSelectedPluginName() const;
    void setExpanded(bool expanded);
    [[nodiscard]] bool isExpanded() const {
        return expanded;
    }
    [[nodiscard]] int getPreferredHeight() const;

    std::function<void()> onScanRequested;
    std::function<void()> onScanStarted;
    std::function<void()> onLoadRequested;
    std::function<void()> onUnloadRequested;
    std::function<void()> onToggleEditorRequested;
    std::function<void()> onPanelSizeChanged;

    void refreshTexts();

private:
    juce::File getInitialBrowseDirectory() const;
    void setupBrowseButton();
    juce::Label pluginStatusLabel;
    juce::Label pluginPathLabel;

    juce::TextButton scanPluginsButton { TRANS("Scan VST3") };
    juce::TextButton browseButton { "..." }; // too short to translate
    juce::TextButton loadPluginButton { TRANS("Load") };
    juce::TextButton unloadPluginButton { TRANS("Unload") };
    juce::TextButton openEditorButton { TRANS("Open Editor") };
    juce::TextButton toggleButton { "\u22EF" }; // vertical ellipsis for expand/collapse

    juce::TextEditor pluginPathEditor;
    juce::ComboBox pluginSelector;
    juce::TextEditor pluginListEditor;
    State lastState;
    juce::ComboBox instrumentFilterCombo;
    bool expanded = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginPanel)
};
