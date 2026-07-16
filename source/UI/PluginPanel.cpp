#include "PluginPanel.h"

PluginPanel::PluginPanel() {
    pluginStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightblue);
    addAndMakeVisible(pluginStatusLabel);

    pluginPathLabel.setText(TRANS("VST3 Path"), juce::dontSendNotification);
    addAndMakeVisible(pluginPathLabel);

    pluginSelectionLabel.setText(TRANS("Plugin"), juce::dontSendNotification);
    addAndMakeVisible(pluginSelectionLabel);

    pluginListLabel.setText(TRANS("Discovered Plugins"), juce::dontSendNotification);
    addAndMakeVisible(pluginListLabel);

    addAndMakeVisible(scanPluginsButton);
    scanPluginsButton.onClick = [this] {
        if (onScanRequested)
            onScanRequested();
    };

    addAndMakeVisible(browseButton);
    setupBrowseButton();

    addAndMakeVisible(loadPluginButton);
    loadPluginButton.onClick = [this] {
        if (onLoadRequested)
            onLoadRequested();
    };

    addAndMakeVisible(unloadPluginButton);
    unloadPluginButton.onClick = [this] {
        if (onUnloadRequested)
            onUnloadRequested();
    };

    addAndMakeVisible(openEditorButton);
    openEditorButton.onClick = [this] {
        if (onToggleEditorRequested)
            onToggleEditorRequested();
    };

    pluginPathEditor.setMultiLine(false);
    pluginPathEditor.setReturnKeyStartsNewLine(false);
    pluginPathEditor.onReturnKey = [this] {
        if (onScanRequested)
            onScanRequested();
    };
    addAndMakeVisible(pluginPathEditor);

    pluginSelector.setTextWhenNothingSelected("Select a scanned plugin...");
    pluginSelector.setWantsKeyboardFocus(false);
    pluginSelector.onChange = [this] {
        if (onLoadRequested && pluginSelector.getSelectedItemIndex() >= 0)
            onLoadRequested();
    };
    addAndMakeVisible(pluginSelector);

    pluginListEditor.setMultiLine(true);
    pluginListEditor.setReadOnly(true);
    pluginListEditor.setScrollbarsShown(true);
    pluginListEditor.setCaretVisible(false);
    addAndMakeVisible(pluginListEditor);

    instrumentFilterCombo.setVisible(false);
    instrumentFilterCombo.addItem(TRANS("All"), 1);
    instrumentFilterCombo.addItem(TRANS("Instruments Only"), 2);
    instrumentFilterCombo.addItem(TRANS("Effects Only"), 3);
    instrumentFilterCombo.setSelectedId(1, juce::dontSendNotification);
    instrumentFilterCombo.onChange = [this] {
        // Re-apply the last state to refill pluginSelector based on the new filter mode.
        updateState(lastState);
    };
    addAndMakeVisible(instrumentFilterCombo);
}

void PluginPanel::resized() {
    auto area = getLocalBounds();
    const auto rowHeight = 28;

    pluginStatusLabel.setBounds(area.removeFromTop(rowHeight));
    area.removeFromTop(8);

    // Path row: [VST3 Path(80)] [pathEditor] [Browse(40)] [Scan VST3(80)]
    auto pathRow = area.removeFromTop(rowHeight);
    pluginPathLabel.setBounds(pathRow.removeFromLeft(80));
    scanPluginsButton.setBounds(pathRow.removeFromRight(80));
    pathRow.removeFromRight(6);
    browseButton.setBounds(pathRow.removeFromRight(40));
    pathRow.removeFromRight(6);
    pluginPathEditor.setBounds(pathRow);

    area.removeFromTop(8);

    // Selector row: [Plugin(80)] [pluginSelector] [Filter(100)] [Load(80)] [Unload(80)] [Open Editor(100)]
    auto selectorRow = area.removeFromTop(rowHeight);
    pluginSelectionLabel.setBounds(selectorRow.removeFromLeft(80));
    openEditorButton.setBounds(selectorRow.removeFromRight(100));
    selectorRow.removeFromRight(6);
    unloadPluginButton.setBounds(selectorRow.removeFromRight(80));
    selectorRow.removeFromRight(6);
    loadPluginButton.setBounds(selectorRow.removeFromRight(80));
    selectorRow.removeFromRight(6);
    instrumentFilterCombo.setBounds(selectorRow.removeFromRight(100));
    selectorRow.removeFromRight(6);
    pluginSelector.setBounds(selectorRow);

    area.removeFromTop(12);

    pluginListLabel.setBounds(area.removeFromTop(rowHeight));
    area.removeFromTop(6);
    pluginListEditor.setBounds(area);
}

void PluginPanel::updateState(const State& state) {
    lastState = state;
    if (state.isCurrentlyScanning) {
        pluginSelector.clear(juce::dontSendNotification);
        pluginSelector.setTextWhenNothingSelected(TRANS("Scanning..."));

        auto scanText = TRANS("Scanning VST3 plugins...") + "\n";
        if (state.scanningPluginName.isNotEmpty())
            scanText << state.scanningPluginName;
        else
            scanText << TRANS("Preparing...");

        pluginListEditor.setText(scanText, juce::dontSendNotification);

        scanPluginsButton.setEnabled(false);
        browseButton.setEnabled(false);
        loadPluginButton.setEnabled(false);
    } else {
        pluginSelector.clear(juce::dontSendNotification);
        pluginSelector.setTextWhenNothingSelected(TRANS("Select a scanned plugin..."));

        // Pick the plugin name array based on the current filter mode.
        const auto& names = [&]() -> const juce::StringArray& {
            const auto filterId = instrumentFilterCombo.getSelectedId();
            if (filterId == 2 && !state.instrumentPluginNames.isEmpty())
                return state.instrumentPluginNames;
            if (filterId == 3 && !state.effectPluginNames.isEmpty())
                return state.effectPluginNames;
            return state.availablePluginNames;
        }();

        auto itemId = 1;
        auto selectedIndex = -1;
        for (const auto& name : names) {
            pluginSelector.addItem(name, itemId);

            if (name.equalsIgnoreCase(state.preferredSelection))
                selectedIndex = itemId - 1;

            ++itemId;
        }

        if (names.isEmpty())
            pluginSelector.setSelectedItemIndex(-1, juce::dontSendNotification);
        else if (selectedIndex >= 0)
            pluginSelector.setSelectedItemIndex(selectedIndex, juce::dontSendNotification);
        else
            pluginSelector.setSelectedItemIndex(0, juce::dontSendNotification);

        pluginListEditor.setText(TRANS(state.pluginListText), juce::dontSendNotification);

        scanPluginsButton.setEnabled(true);
        browseButton.setEnabled(true);
        loadPluginButton.setEnabled(!names.isEmpty());
        unloadPluginButton.setEnabled(state.hasLoadedPlugin);
        openEditorButton.setEnabled(state.hasLoadedPlugin);
        pluginPathEditor.setEnabled(true);
    }

    auto text = TRANS(state.availableFormatsDescription);
    if (state.supportsVst3)
        text << TRANS(" [VST3 ready]");

    if (state.isCurrentlyScanning) {
        text << TRANS(" | Scanning: ") << state.scanningPluginName << "...";
    } else {
        auto summary = state.lastScanSummary;
        if (summary.startsWith("VST3 scan complete: ") && !summary.contains("no plugins")) {
            auto resultSuffix = (state.scanFailedCount > 0) ? TRANS(" failed (see log).") : TRANS(" failed.");
            text << " | " << TRANS("VST3 scan complete: ") << juce::String(state.scanPluginCount)
                 << TRANS(" plugin(s), ") << juce::String(state.scanFailedCount) << resultSuffix;
        } else if (summary.startsWith("VST3 scan found no plugins; ")) {
            text << " | " << TRANS("VST3 scan found no plugins: ") << juce::String(state.scanFailedCount)
                 << TRANS(" failed (see log).");
        } else if (summary.startsWith("Loaded cached plugin list: ")) {
            text << " | " << TRANS("Loaded cached plugin list: ") << juce::String(state.scanPluginCount)
                 << TRANS(" plugin(s).");
        } else {
            text << " | " << TRANS(summary);
        }
    }

    if (state.hasLoadedPlugin) {
        text << TRANS(" | Loaded: ") << state.currentPluginName;

        if (state.isPrepared)
            text << " @ " << juce::String(state.preparedSampleRate, 0) << " Hz / "
                 << juce::String(state.preparedBlockSize);
        else
            text << TRANS(" [not prepared]");

        if (state.isEditorOpen)
            text << TRANS(" | Editor open");
    } else if (state.lastLoadError.isNotEmpty() && state.lastLoadError != "No plugin load attempted yet.") {
        text << TRANS(" | Load error: ") << state.lastLoadError;
    } else if (state.lastPluginName.isNotEmpty()) {
        text << TRANS(" | Last plugin: ") << state.lastPluginName;
    }

    pluginStatusLabel.setText(text, juce::dontSendNotification);
}

void PluginPanel::setPluginPathText(const juce::String& text) {
    pluginPathEditor.setText(text, juce::dontSendNotification);
}

juce::String PluginPanel::getPluginPathText() const {
    return pluginPathEditor.getText();
}

void PluginPanel::setInstrumentFilterVisible(bool visible) {
    instrumentFilterCombo.setVisible(visible);
    resized();
}
juce::String PluginPanel::getSelectedPluginName() const {
    return pluginSelector.getText();
}

juce::File PluginPanel::getInitialBrowseDirectory() const {
    auto path = pluginPathEditor.getText();
    if (path.isNotEmpty())
        return juce::File(path);
    return {};
}

void PluginPanel::setupBrowseButton() {
    browseButton.onClick = [this] {
        auto chooser = std::make_shared<juce::FileChooser>(TRANS("Select VST3 Plugin Folder"),
                                                           getInitialBrowseDirectory(), "", true);
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                             [this, chooser](const juce::FileChooser& fc) {
                                 auto folder = fc.getResult();
                                 if (folder.exists()) {
                                     pluginPathEditor.setText(folder.getFullPathName(), juce::dontSendNotification);
                                     if (onScanRequested)
                                         onScanRequested();
                                 }
                             });
    };
}

void PluginPanel::refreshTexts() {
    // Static label and button text (constructor initializers are evaluated once)
    pluginPathLabel.setText(TRANS("VST3 Path"), juce::dontSendNotification);
    pluginSelectionLabel.setText(TRANS("Plugin"), juce::dontSendNotification);
    pluginListLabel.setText(TRANS("Discovered Plugins"), juce::dontSendNotification);
    scanPluginsButton.setButtonText(TRANS("Scan VST3"));
    loadPluginButton.setButtonText(TRANS("Load"));
    unloadPluginButton.setButtonText(TRANS("Unload"));
    openEditorButton.setButtonText(TRANS("Open Editor"));
    pluginSelector.setTextWhenNothingSelected(TRANS("Select a scanned plugin..."));

    const auto previousFilterId = instrumentFilterCombo.getSelectedId();
    instrumentFilterCombo.clear(juce::dontSendNotification);
    instrumentFilterCombo.addItem(TRANS("All"), 1);
    instrumentFilterCombo.addItem(TRANS("Instruments Only"), 2);
    instrumentFilterCombo.addItem(TRANS("Effects Only"), 3);
    instrumentFilterCombo.setSelectedId(previousFilterId > 0 ? previousFilterId : 1, juce::dontSendNotification);

    // Re-apply the last state to refresh status text and plugin list (covers TRANS inside updateState).
    updateState(lastState);
}
