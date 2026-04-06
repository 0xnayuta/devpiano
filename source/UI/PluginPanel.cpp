#include "PluginPanel.h"

PluginPanel::PluginPanel()
{
    pluginStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightblue);
    addAndMakeVisible(pluginStatusLabel);

    pluginPathLabel.setText("VST3 Path", juce::dontSendNotification);
    addAndMakeVisible(pluginPathLabel);

    pluginSelectionLabel.setText("Plugin", juce::dontSendNotification);
    addAndMakeVisible(pluginSelectionLabel);

    pluginListLabel.setText("Discovered Plugins", juce::dontSendNotification);
    addAndMakeVisible(pluginListLabel);

    addAndMakeVisible(scanPluginsButton);
    scanPluginsButton.onClick = [this]
    {
        if (onScanRequested)
            onScanRequested();
    };

    addAndMakeVisible(loadPluginButton);
    loadPluginButton.onClick = [this]
    {
        if (onLoadRequested)
            onLoadRequested();
    };

    addAndMakeVisible(unloadPluginButton);
    unloadPluginButton.onClick = [this]
    {
        if (onUnloadRequested)
            onUnloadRequested();
    };

    addAndMakeVisible(openEditorButton);
    openEditorButton.onClick = [this]
    {
        if (onToggleEditorRequested)
            onToggleEditorRequested();
    };

    pluginPathEditor.setMultiLine(false);
    pluginPathEditor.setReturnKeyStartsNewLine(false);
    addAndMakeVisible(pluginPathEditor);

    pluginSelector.setTextWhenNothingSelected("Select a scanned plugin...");
    pluginSelector.setWantsKeyboardFocus(false);
    addAndMakeVisible(pluginSelector);

    pluginListEditor.setMultiLine(true);
    pluginListEditor.setReadOnly(true);
    pluginListEditor.setScrollbarsShown(true);
    pluginListEditor.setCaretVisible(false);
    pluginListEditor.setPopupMenuEnabled(true);
    pluginListEditor.setWantsKeyboardFocus(false);
    pluginListEditor.setMouseClickGrabsKeyboardFocus(false);
    addAndMakeVisible(pluginListEditor);
}

void PluginPanel::resized()
{
    auto area = getLocalBounds();

    pluginStatusLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(10);

    auto pathRow = area.removeFromTop(28);
    pluginPathLabel.setBounds(pathRow.removeFromLeft(80));
    scanPluginsButton.setBounds(pathRow.removeFromRight(120));
    pluginPathEditor.setBounds(pathRow.reduced(6, 0));

    area.removeFromTop(8);

    auto selectorRow = area.removeFromTop(28);
    pluginSelectionLabel.setBounds(selectorRow.removeFromLeft(80));
    openEditorButton.setBounds(selectorRow.removeFromRight(110));
    unloadPluginButton.setBounds(selectorRow.removeFromRight(90));
    loadPluginButton.setBounds(selectorRow.removeFromRight(90));
    pluginSelector.setBounds(selectorRow.reduced(6, 0));

    area.removeFromTop(8);

    pluginListLabel.setBounds(area.removeFromTop(22));
    pluginListEditor.setBounds(area);
}

void PluginPanel::updateState(const State& state)
{
    pluginSelector.clear(juce::dontSendNotification);

    auto itemId = 1;
    auto selectedIndex = -1;
    for (const auto& name : state.availablePluginNames)
    {
        pluginSelector.addItem(name, itemId);

        if (name.equalsIgnoreCase(state.preferredSelection))
            selectedIndex = itemId - 1;

        ++itemId;
    }

    if (state.availablePluginNames.isEmpty())
        pluginSelector.setSelectedItemIndex(-1, juce::dontSendNotification);
    else if (selectedIndex >= 0)
        pluginSelector.setSelectedItemIndex(selectedIndex, juce::dontSendNotification);
    else
        pluginSelector.setSelectedItemIndex(0, juce::dontSendNotification);

    pluginListEditor.setText(state.pluginListText, juce::dontSendNotification);

    loadPluginButton.setEnabled(! state.availablePluginNames.isEmpty());
    unloadPluginButton.setEnabled(state.hasLoadedPlugin);
    openEditorButton.setEnabled(state.hasLoadedPlugin);
    openEditorButton.setButtonText(state.isEditorOpen ? "Close Editor" : "Open Editor");

    auto text = state.availableFormatsDescription;
    if (state.supportsVst3)
        text << " [VST3 ready]";

    text << " | " << state.lastScanSummary;

    if (state.hasLoadedPlugin)
    {
        text << " | Loaded: " << state.currentPluginName;

        if (state.isPrepared)
            text << " @ " << juce::String(state.preparedSampleRate, 0)
                 << " Hz / " << juce::String(state.preparedBlockSize);
        else
            text << " [not prepared]";

        if (state.isEditorOpen)
            text << " | Editor open";
    }
    else if (state.lastLoadError.isNotEmpty() && state.lastLoadError != "No plugin load attempted yet.")
    {
        text << " | Load error: " << state.lastLoadError;
    }
    else if (state.lastPluginName.isNotEmpty())
    {
        text << " | Last plugin: " << state.lastPluginName;
    }

    pluginStatusLabel.setText(text, juce::dontSendNotification);
}

void PluginPanel::setPluginPathText(const juce::String& text)
{
    pluginPathEditor.setText(text, juce::dontSendNotification);
}

juce::String PluginPanel::getPluginPathText() const
{
    return pluginPathEditor.getText();
}


juce::String PluginPanel::getSelectedPluginName() const
{
    return pluginSelector.getText();
}

