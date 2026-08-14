// ═══════════════════════════════════════════════════════════════════════════
// JIVE component accessors — #included into MainComponent.cpp.
//
// Kept in a separate file (following the Phase 11 architecture) because the
// accessors reach into MainComponent private members and therefore must be
// compiled in its translation unit.
// ═══════════════════════════════════════════════════════════════════════════

namespace {

/// JIVE Button's "text" property maps to Button::setTitle (accessibility),
/// not the visible label — that lives in the button's Text child.
void setButtonLabel(::jive::GuiItem* buttonItem, const juce::String& text) {
    if (buttonItem == nullptr)
        return;
    for (auto child : buttonItem->state)
        if (child.getType() == juce::Identifier("Text")) {
            child.setProperty("text", text, nullptr);
            return;
        }
}

/// Ellipsise a string to fit `maxWidth` pixels of the 14 pt UI font.
///
/// JIVE's TextComponent paints its AttributedString without clipping, so a
/// long single-line status text spills over the plugin selector combo to its
/// right. Truncate here instead (the full text goes into the tooltip).
juce::String ellipsiseForStatus(const juce::String& text, float maxWidth) {
    if (text.isEmpty())
        return text;

    const juce::Font font(juce::FontOptions(14.0f));
    if (juce::GlyphArrangement::getStringWidth(font, text) <= maxWidth)
        return text;

    const auto ellipsis = juce::String::charToString(0x2026);
    juce::String result = text;
    while (result.length() > 1 && juce::GlyphArrangement::getStringWidth(font, result + ellipsis) > maxWidth)
        result = result.dropLastCharacters(1);

    return result + ellipsis;
}

} // namespace

// ── JIVE plugin panel accessors ────────────────────────────────────────────

void MainComponent::setPluginPathText(const juce::String& text) {
    if (jiveRootItem == nullptr)
        return;
    if (auto* item = jive::findItemWithID(*jiveRootItem, "plugin-path-editor"))
        if (auto* editor = dynamic_cast<juce::TextEditor*>(item->getComponent().get()))
            editor->setText(text, juce::dontSendNotification);
}

juce::String MainComponent::getPluginPathText() const {
    if (jiveRootItem == nullptr)
        return {};
    if (auto* item = jive::findItemWithID(*jiveRootItem, "plugin-path-editor"))
        if (auto* editor = dynamic_cast<juce::TextEditor*>(item->getComponent().get()))
            return editor->getText();
    return {};
}

juce::String MainComponent::getSelectedPluginName() const {
    if (jiveRootItem == nullptr)
        return {};
    if (auto* item = jive::findItemWithID(*jiveRootItem, "plugin-selector"))
        if (auto* combo = dynamic_cast<juce::ComboBox*>(item->getComponent().get()))
            return combo->getText();
    return {};
}

void MainComponent::setPluginPanelExpanded(bool expanded) {
    appSettings.pluginPanelExpanded = expanded;
    if (jiveRootItem != nullptr) {
        // Order matters: update the expandable area FIRST, so that the panel's
        // layout pass (triggered by the height change below) reads the final
        // area height. The old order laid the panel out while the area was
        // still 112 px tall, flex-compressing the toolbar row to 0 height.
        if (auto* expandedArea = jive::findItemWithID(*jiveRootItem, "plugin-expanded-area"))
            expandedArea->state.setProperty("height", expanded ? 112 : 0, nullptr);

        if (auto* item = jive::findItemWithID(*jiveRootItem, "plugin-panel")) {
            item->state.setProperty("height", expanded ? 160 : 40, nullptr);
            // JIVE's boxModelChanged only re-lays-out the decorated item
            // itself (the top of its decorator chain), never its siblings in
            // the parent column — so the controls/keyboard below would stay
            // put and overlap the expanded panel. Reflow the main column
            // explicitly. (A root reflow is not enough: components whose
            // bounds did not change are skipped, so the panel's own children
            // would keep their compressed layout.)
            if (auto* panel = dynamic_cast<jive::FlexContainer*>(item))
                panel->layOutChildren();

            if (auto* mainArea = dynamic_cast<jive::FlexContainer*>(jive::findItemWithID(*jiveRootItem, "main-area")))
                mainArea->layOutChildren();
        }
    }
    settingsStore.scheduleSave(appSettings);
}

void MainComponent::setInstrumentFilterVisible(bool visible) {
    if (jiveRootItem == nullptr)
        return;
    if (auto* item = jive::findItemWithID(*jiveRootItem, "plugin-filter-combo"))
        item->state.setProperty("visibility", visible, nullptr);
}

void MainComponent::showPluginBrowseDialog() {
    auto chooser = std::make_shared<juce::FileChooser>(TRANS("Select VST3 Plugin Folder"),
                                                       juce::File(getPluginPathText()), "", true);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                         [this, chooser](const juce::FileChooser& fc) {
                             auto folder = fc.getResult();
                             if (folder.exists()) {
                                 setPluginPathText(folder.getFullPathName());
                                 pluginOperationController->scanPlugins();
                             }
                         });
}

void MainComponent::updatePluginPanelState(const PluginPanelState& state) {
    if (jiveRootItem == nullptr)
        return;

    auto* selectorItem = jive::findItemWithID(*jiveRootItem, "plugin-selector");
    auto* statusItem = jive::findItemWithID(*jiveRootItem, "plugin-status-label");
    auto* listItem = jive::findItemWithID(*jiveRootItem, "plugin-list-editor");
    auto* filterItem = jive::findItemWithID(*jiveRootItem, "plugin-filter-combo");

    auto* selectorCombo
        = selectorItem != nullptr ? dynamic_cast<juce::ComboBox*>(selectorItem->getComponent().get()) : nullptr;
    auto* listEditor = listItem != nullptr ? dynamic_cast<juce::TextEditor*>(listItem->getComponent().get()) : nullptr;

    const auto setEnabled = [](::jive::GuiItem* item, bool enabled) {
        if (item != nullptr)
            item->state.setProperty("enabled", enabled, nullptr);
    };

    if (state.isCurrentlyScanning) {
        if (selectorItem != nullptr)
            selectorItem->state.removeAllChildren(nullptr);
        if (selectorCombo != nullptr)
            selectorCombo->setTextWhenNothingSelected(TRANS("Scanning..."));
        if (listEditor != nullptr) {
            auto scanText = TRANS("Scanning VST3 plugins...") + "\n";
            scanText << (state.scanningPluginName.isNotEmpty() ? state.scanningPluginName : TRANS("Preparing..."));
            listEditor->setText(scanText, juce::dontSendNotification);
        }
        setEnabled(jive::findItemWithID(*jiveRootItem, "scan-btn"), false);
        setEnabled(jive::findItemWithID(*jiveRootItem, "browse-btn"), false);
        setEnabled(jive::findItemWithID(*jiveRootItem, "load-btn"), false);
    } else {
        const auto& names = [&]() -> const juce::StringArray& {
            // JIVE "selected" is a 0-based item index (All=0, Instruments=1, Effects=2).
            const auto filterId = filterItem != nullptr ? filterItem->state["selected"].toString().getIntValue() : 0;
            if (filterId == 1 && !state.instrumentPluginNames.isEmpty())
                return state.instrumentPluginNames;
            if (filterId == 2 && !state.effectPluginNames.isEmpty())
                return state.effectPluginNames;
            return state.availablePluginNames;
        }();

        if (selectorItem != nullptr) {
            selectorItem->state.removeAllChildren(nullptr);
            auto selectedIndex = -1;
            auto index = 0;
            for (const auto& name : names) {
                auto option = juce::ValueTree("Option");
                option.setProperty("text", name, nullptr);
                option.setProperty("enabled", true, nullptr); // JIVE defaults to false
                selectorItem->state.addChild(option, index, nullptr);
                if (name.equalsIgnoreCase(state.preferredSelection))
                    selectedIndex = index;
                ++index;
            }
            const auto finalIndex = names.isEmpty() ? -1 : (selectedIndex >= 0 ? selectedIndex : 0);
            selectorItem->state.setProperty("selected", finalIndex, nullptr);

            // Synchronously restore the ComboBox label (bypasses the JIVE
            // Property chain so the text is correct even if an async change
            // notification later overwrites "selected" with -1).
            if (selectorCombo != nullptr)
                selectorCombo->setSelectedItemIndex(juce::jmax(0, finalIndex), juce::dontSendNotification);

            // And re-assert the property after pending async change messages
            // have been dispatched (ValueTree is ref-counted, so the lambda
            // outlives the tree safely — isCurrent()/isValid() guards).
            auto selectorState = selectorItem->state;
            juce::MessageManager::callAsync([selectorState, finalIndex]() mutable {
                if (selectorState.isValid())
                    selectorState.setProperty("selected", finalIndex, nullptr);
            });
        }

        if (selectorCombo != nullptr)
            selectorCombo->setTextWhenNothingSelected(TRANS("Select a scanned plugin..."));
        if (listEditor != nullptr)
            listEditor->setText(TRANS(state.pluginListText), juce::dontSendNotification);

        setEnabled(jive::findItemWithID(*jiveRootItem, "scan-btn"), true);
        setEnabled(jive::findItemWithID(*jiveRootItem, "browse-btn"), true);
        setEnabled(jive::findItemWithID(*jiveRootItem, "load-btn"), !names.isEmpty());
        setEnabled(jive::findItemWithID(*jiveRootItem, "unload-btn"), state.hasLoadedPlugin);
        setEnabled(jive::findItemWithID(*jiveRootItem, "editor-btn"), state.hasLoadedPlugin);
        if (auto* item = jive::findItemWithID(*jiveRootItem, "plugin-path-editor"))
            item->state.setProperty("enabled", true, nullptr);
    }

    // Status line: formats + scan summary + loaded plugin info.
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

    lastPluginStatusText = text;
    if (statusItem != nullptr)
        refreshPluginStatusEllipsis();
}

void MainComponent::refreshPluginStatusEllipsis() {
    // Re-truncate the status text to the label's current flex-allocated
    // width. updatePluginPanelState can run before the first layout, when
    // the label width is still 0 (and the 470 px fallback overflows narrow
    // windows) — re-apply after every layout so the text never spills into
    // the combo to its right.
    if (jiveRootItem == nullptr || lastPluginStatusText.isEmpty())
        return;
    if (auto* statusItem = jive::findItemWithID(*jiveRootItem, "plugin-status-label")) {
        const auto labelWidth = static_cast<float>(statusItem->getComponent()->getWidth());
        statusItem->state.setProperty(
            "text", ellipsiseForStatus(lastPluginStatusText, labelWidth > 0.0f ? labelWidth - 4.0f : 470.0f), nullptr);
        statusItem->state.setProperty("tooltip", lastPluginStatusText, nullptr);
    }
}

void MainComponent::refreshPluginPanelTexts() {
    if (jiveRootItem == nullptr)
        return;

    if (auto* item = jive::findItemWithID(*jiveRootItem, "plugin-path-label"))
        item->state.setProperty("text", TRANS("VST3 Path"), nullptr);
    const auto setButtonText = [this](const char* id, const juce::String& text) {
        setButtonLabel(jive::findItemWithID(*jiveRootItem, id), text);
    };
    setButtonText("scan-btn", TRANS("Scan VST3"));
    setButtonText("load-btn", TRANS("Load"));
    setButtonText("unload-btn", TRANS("Unload"));
    setButtonText("editor-btn", TRANS("Open Editor"));
    if (auto* item = jive::findItemWithID(*jiveRootItem, "plugin-filter-combo")) {
        const juce::StringArray filterTexts { TRANS("All"), TRANS("Instruments Only"), TRANS("Effects Only") };
        auto childIndex = 0;
        for (auto child : item->state) {
            if (childIndex < filterTexts.size())
                child.setProperty("text", filterTexts[childIndex], nullptr);
            ++childIndex;
        }
    }
    if (auto* item = jive::findItemWithID(*jiveRootItem, "plugin-selector"))
        if (auto* combo = dynamic_cast<juce::ComboBox*>(item->getComponent().get()))
            combo->setTextWhenNothingSelected(TRANS("Select a scanned plugin..."));

    // Re-apply the last state to refresh status text (locale-dependent).
    refreshPluginUiState();
}

// ── JIVE controls panel accessors ──────────────────────────────────────────

float MainComponent::getMasterGain() const {
    if (jiveRootItem == nullptr)
        return 0.0f;
    if (auto* item = jive::findItemWithID(*jiveRootItem, "volume-knob"))
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get()))
            return static_cast<float>(slider->getValue());
    return 0.0f;
}

float MainComponent::getAttack() const {
    if (jiveRootItem == nullptr)
        return 0.0f;
    if (auto* item = jive::findItemWithID(*jiveRootItem, "attack-knob"))
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get()))
            return static_cast<float>(slider->getValue());
    return 0.0f;
}

float MainComponent::getDecay() const {
    if (jiveRootItem == nullptr)
        return 0.0f;
    if (auto* item = jive::findItemWithID(*jiveRootItem, "decay-knob"))
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get()))
            return static_cast<float>(slider->getValue());
    return 0.0f;
}

float MainComponent::getSustain() const {
    if (jiveRootItem == nullptr)
        return 0.0f;
    if (auto* item = jive::findItemWithID(*jiveRootItem, "sustain-knob"))
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get()))
            return static_cast<float>(slider->getValue());
    return 0.0f;
}

float MainComponent::getRelease() const {
    if (jiveRootItem == nullptr)
        return 0.0f;
    if (auto* item = jive::findItemWithID(*jiveRootItem, "release-knob"))
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get()))
            return static_cast<float>(slider->getValue());
    return 0.0f;
}

double MainComponent::getControlsPlaybackSpeed() const {
    if (jiveRootItem == nullptr)
        return 1.0;
    if (auto* item = jive::findItemWithID(*jiveRootItem, "speed-knob"))
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get()))
            return slider->getValue();
    return 1.0;
}

void MainComponent::setControlsValues(float masterGain, float attack, float decay, float sustain, float release) {
    if (jiveRootItem == nullptr)
        return;
    const auto setSlider = [this](const char* id, double value) {
        if (auto* item = jive::findItemWithID(*jiveRootItem, id))
            if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get()))
                slider->setValue(value, juce::dontSendNotification);
    };
    setSlider("volume-knob", masterGain);
    setSlider("attack-knob", attack);
    setSlider("decay-knob", decay);
    setSlider("sustain-knob", sustain);
    setSlider("release-knob", release);
    if (auto* item = jive::findItemWithID(*jiveRootItem, "adsr-curve"))
        if (auto* curve = dynamic_cast<AdsrCurveComponent*>(item->getComponent().get()))
            curve->setParameters(attack, decay, sustain, release);
}

void MainComponent::setControlsPlaybackSpeed(double speed) {
    if (jiveRootItem == nullptr)
        return;
    if (auto* item = jive::findItemWithID(*jiveRootItem, "speed-knob"))
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get()))
            slider->setValue(juce::jlimit(0.5, 2.0, speed), juce::dontSendNotification);
}

void MainComponent::setControlsPresets(const juce::StringArray& presetIds, const juce::String& currentPresetId,
                                       const juce::StringArray& presetDisplayNames) {
    availablePresetIds = presetIds;
    if (jiveRootItem == nullptr)
        return;

    auto* comboItem = jive::findItemWithID(*jiveRootItem, "preset-combo");

    if (comboItem != nullptr)
        comboItem->state.removeAllChildren(nullptr);
    auto selectedIndex = 0;
    for (int i = 0; i < presetIds.size(); ++i) {
        const auto displayName = (i < presetDisplayNames.size()) ? presetDisplayNames[i] : presetIds[i];
        if (comboItem != nullptr) {
            auto option = juce::ValueTree("Option");
            option.setProperty("text", displayName, nullptr);
            option.setProperty("enabled", true, nullptr); // JIVE defaults to false
            comboItem->state.addChild(option, i, nullptr);
        }
        if (presetIds[i] == currentPresetId)
            selectedIndex = i;
    }
    if (comboItem != nullptr)
        comboItem->state.setProperty("selected", selectedIndex, nullptr);

    updateControlsPresetActionButtons();
}

juce::String MainComponent::getSelectedPresetId() const {
    if (jiveRootItem == nullptr)
        return {};
    if (auto* item = jive::findItemWithID(*jiveRootItem, "preset-combo"))
        if (auto* combo = dynamic_cast<juce::ComboBox*>(item->getComponent().get())) {
            const auto index = combo->getSelectedItemIndex();
            if (juce::isPositiveAndBelow(index, availablePresetIds.size()))
                return availablePresetIds[index];
        }
    return {};
}

void MainComponent::updateControlsPresetActionButtons() {
    const auto isUserPreset = getSelectedPresetId().isNotEmpty();
    if (jiveRootItem == nullptr)
        return;
    const auto setEnabled = [this](const char* id, bool enabled) {
        if (auto* item = jive::findItemWithID(*jiveRootItem, id))
            item->state.setProperty("enabled", enabled, nullptr);
    };
    setEnabled("rename-preset-btn", isUserPreset);
    setEnabled("delete-preset-btn", isUserPreset);
}

void MainComponent::setRecordingControlsState(RecordingControlsState state) {
    recordingControlsState = state;
    if (jiveRootItem == nullptr)
        return;

    auto recordEnabled = true;
    auto playEnabled = state.hasTake;
    auto stopEnabled = false;
    auto backToStartEnabled = state.hasTake;
    auto exportMidiEnabled = state.hasTake && state.canExportMidiTake;
    auto exportWavEnabled = state.hasTake && state.canExportWavTake;
    auto importMidiEnabled = true;
    auto saveEnabled = false;
    auto openEnabled = false;

    switch (state.state) {
    case RecordingState::idle:
        playEnabled = state.hasTake;
        backToStartEnabled = state.hasTake;
        exportMidiEnabled = state.hasTake && state.canExportMidiTake;
        exportWavEnabled = state.hasTake && state.canExportWavTake;
        importMidiEnabled = true;
        saveEnabled = state.hasTake;
        openEnabled = true;
        break;
    case RecordingState::recording:
        recordEnabled = false;
        playEnabled = false;
        backToStartEnabled = false;
        exportMidiEnabled = false;
        exportWavEnabled = false;
        importMidiEnabled = false;
        stopEnabled = true;
        saveEnabled = false;
        openEnabled = false;
        break;
    case RecordingState::playing:
        recordEnabled = false;
        playEnabled = false;
        backToStartEnabled = state.hasTake;
        exportMidiEnabled = false;
        exportWavEnabled = false;
        importMidiEnabled = false;
        stopEnabled = true;
        saveEnabled = false;
        openEnabled = false;
        break;
    }

    const auto setEnabled = [this](const char* id, bool enabled) {
        if (auto* item = jive::findItemWithID(*jiveRootItem, id))
            item->state.setProperty("enabled", enabled, nullptr);
    };
    setEnabled("record-btn", recordEnabled);
    setEnabled("play-btn", playEnabled);
    setEnabled("stop-btn", stopEnabled);
    setEnabled("back-btn", backToStartEnabled);
    setEnabled("import-midi-btn", importMidiEnabled);
    setEnabled("export-midi-btn", exportMidiEnabled);
    setEnabled("export-wav-btn", exportWavEnabled);
    setEnabled("save-perf-btn", saveEnabled);
    setEnabled("open-perf-btn", openEnabled);
}

juce::Rectangle<int> MainComponent::getRecentFilesButtonScreenBounds() const {
    if (jiveRootItem == nullptr)
        return {};
    if (auto* item = jive::findItemWithID(*jiveRootItem, "recent-btn"))
        return item->getComponent()->getScreenBounds();
    return {};
}

void MainComponent::refreshControlsTexts() {
    if (jiveRootItem == nullptr)
        return;

    const auto setText = [this](const char* id, const juce::String& text) {
        if (auto* item = jive::findItemWithID(*jiveRootItem, id))
            item->state.setProperty("text", text, nullptr);
    };
    const auto setButtonText = [this](const char* id, const juce::String& text) {
        setButtonLabel(jive::findItemWithID(*jiveRootItem, id), text);
    };
    const auto setTooltip = [this](const char* id, const juce::String& tooltip) {
        if (auto* item = jive::findItemWithID(*jiveRootItem, id))
            item->state.setProperty("tooltip", tooltip, nullptr);
    };

    setText("volume-label", TRANS("Volume"));
    setText("attack-label", TRANS("Attack"));
    setText("decay-label", TRANS("Decay"));
    setText("sustain-label", TRANS("Sustain"));
    setText("release-label", TRANS("Release"));
    setText("preset-label", TRANS("Preset"));
    setText("speed-label", TRANS("Speed"));
    setButtonText("save-preset-btn", TRANS("Save As New"));
    setButtonText("rename-preset-btn", TRANS("Rename"));
    setButtonText("delete-preset-btn", TRANS("Delete"));
    setTooltip("record-btn", TRANS("Record"));
    setTooltip("play-btn", TRANS("Play"));
    setTooltip("stop-btn", TRANS("Stop"));
    setTooltip("back-btn", TRANS("Back to Start"));
    setButtonText("export-midi-btn", TRANS("Export MIDI"));
    setButtonText("export-wav-btn", TRANS("Export WAV"));
    setButtonText("import-midi-btn", TRANS("Import MIDI"));
    setButtonText("recent-btn", TRANS("Recent"));
    setButtonText("save-perf-btn", TRANS("Save"));
    setButtonText("open-perf-btn", TRANS("Open"));
    setButtonText("song-info-btn", TRANS("Song Info"));

    setRecordingControlsState(recordingControlsState);
}

// ── JIVE keyboard area accessors ───────────────────────────────────────────

CustomKeyboard& MainComponent::getCustomKeyboard() {
    jassert(customKeyboardRef != nullptr);
    return *customKeyboardRef;
}

void MainComponent::setKeyboardLayout(const devpiano::core::KeyboardLayout& layout) {
    if (jiveRootItem == nullptr)
        return;
    if (auto* item = jive::findItemWithID(*jiveRootItem, "custom-keyboard"))
        if (auto* viewport = dynamic_cast<KeyboardViewport*>(item->getComponent().get()))
            viewport->getCustomKeyboard().setKeyboardLayout(layout);
}

void MainComponent::setKeyboardViewPosition(int midiNote, int pixelOffset) {
    if (jiveRootItem == nullptr)
        return;
    auto* item = jive::findItemWithID(*jiveRootItem, "custom-keyboard");
    auto* viewport = item != nullptr ? dynamic_cast<KeyboardViewport*>(item->getComponent().get()) : nullptr;
    if (viewport == nullptr)
        return;

    auto& keyboard = viewport->getCustomKeyboard();
    if (pixelOffset >= 0) {
        viewport->setViewPosition(pixelOffset, 0);
    } else if (midiNote >= 0 && midiNote <= 127) {
        int whiteCount = 0;
        for (int n = 0; n < midiNote; ++n)
            if (devpiano::ui::isWhiteKey(n))
                ++whiteCount;
        auto x = static_cast<int>(static_cast<float>(whiteCount) * keyboard.getKeyboardSettings().keyWidth);
        viewport->setViewPosition(x, 0);
    }
}

int MainComponent::getKeyboardViewPositionX() const noexcept {
    if (jiveRootItem == nullptr)
        return 0;
    if (auto* item = jive::findItemWithID(*jiveRootItem, "custom-keyboard"))
        if (auto* viewport = dynamic_cast<KeyboardViewport*>(item->getComponent().get()))
            return viewport->getViewPositionX();
    return 0;
}

void MainComponent::finishPluginUiAction(bool shouldSaveSettings) {
    if (shouldSaveSettings)
        saveSettingsSoon();

    refreshReadOnlyUiStateFromCurrentSnapshot();
    restoreKeyboardFocus();
}

void MainComponent::logCurrentAudioDeviceDiagnostics(const juce::String& context) const {
    const auto diagnostics
        = devpiano::audio::buildAudioDeviceDiagnostics(appSettings.audioDeviceState.get(), deviceManager);
    DP_LOG_INFO("[AudioDevice] " + context + "\n" + diagnostics.detailedSummary);
}

devpiano::core::AppState MainComponent::buildCurrentAppStateSnapshot() const {
    return devpiano::core::buildCurrentAppStateSnapshot(
        appSettings, deviceManager, pluginHost,
        pluginOperationController != nullptr && pluginOperationController->hasEditorWindowOpen(), keyboardMidiMapper);
}

void MainComponent::applyLanguage(const juce::String& code) {
    devpiano::locale::activate(devpiano::locale::codeToLanguage(code));
    refreshAllTexts();
}

void MainComponent::refreshAllTexts() {
    refreshPluginPanelTexts();
    refreshControlsTexts();
}

double MainComponent::getCurrentRuntimeSampleRate() const {
    if (auto* device = deviceManager.getCurrentAudioDevice()) {
        const auto rate = device->getCurrentSampleRate();
        if (rate > 0.0)
            return rate;
    }

    return appSettings.getAudioSettingsView().sampleRate;
}

int MainComponent::getCurrentRuntimeBlockSize() const {
    if (auto* device = deviceManager.getCurrentAudioDevice()) {
        const auto size = device->getCurrentBufferSizeSamples();
        if (size > 0)
            return size;
    }

    return appSettings.getAudioSettingsView().bufferSize;
}

MainComponent::RuntimeAudioConfig MainComponent::getCurrentRuntimeAudioConfig() const {
    return { .sampleRate = getCurrentRuntimeSampleRate(), .blockSize = getCurrentRuntimeBlockSize() };
}

void MainComponent::runPluginActionWithAudioDeviceRebuild(
    const std::function<void(const RuntimeAudioConfig&)>& action) {
    struct AudioDeviceRebuildGuard final {
        explicit AudioDeviceRebuildGuard(MainComponent& ownerIn)
            : owner(ownerIn) {
        }
        ~AudioDeviceRebuildGuard() {
            owner.finishAudioDeviceRebuild();
        }

        MainComponent& owner;
    };

    const auto runtimeAudioConfig = getCurrentRuntimeAudioConfig();

    prepareForAudioDeviceRebuild();
    const AudioDeviceRebuildGuard rebuildGuard(*this);
    action(runtimeAudioConfig);
}

void MainComponent::runPluginActionWithAudioDeviceRebuild(const std::function<void()>& action) {
    runPluginActionWithAudioDeviceRebuild([&action](const RuntimeAudioConfig&) { action(); });
}

void MainComponent::saveRecentFiles() {
    appSettings.recentFilesSerialized = recentFiles.toString();
    saveSettingsSoon();
}

void MainComponent::showRecentFilesMenu() {
    juce::PopupMenu menu;
    recentFiles.removeNonExistentFiles();

    const auto numFiles = recentFiles.getNumFiles();
    int itemId = 1;

    if (numFiles == 0) {
        menu.addItem(0, TRANS("(no recent files)"), false, false);
    } else {
        for (int i = 0; i < numFiles; ++i) {
            auto file = recentFiles.getFile(i);
            auto name = file.getFileName();
            auto ext = file.getFileExtension().toLowerCase();
            juce::String prefix;
            if (ext == ".devpiano")
                prefix = juce::String::fromUTF8("\xe2\x99\xaa "); // ♪
            else if (ext == ".mid" || ext == ".midi")
                prefix = juce::String::fromUTF8("\xe2\x99\xab "); // ♫
            else
                prefix = "? ";

            menu.addItem(itemId, prefix + name);
            ++itemId;
        }
    }

    int clearId = itemId;
    if (numFiles > 0) {
        menu.addSeparator();
        clearId = itemId;
        menu.addItem(clearId, TRANS("Clear Recent Files"));
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(getRecentFilesButtonScreenBounds()),
                       [safe = juce::Component::SafePointer<MainComponent>(this), numFiles, clearId](int result) {
                           if (safe == nullptr)
                               return;

                           if (result == 0)
                               return;

                           if (result == clearId) {
                               safe->recentFiles.clear();
                               safe->saveRecentFiles();
                               return;
                           }

                           const auto index = result - 1;
                           if (!juce::isPositiveAndBelow(index, numFiles))
                               return;

                           auto file = safe->recentFiles.getFile(index);
                           if (!file.exists())
                               return;

                           auto ext = file.getFileExtension().toLowerCase();
                           if (ext == ".devpiano")
                               safe->recordingSessionController->handleOpenPerformanceFile(file);
                           else if (ext == ".mid" || ext == ".midi")
                               safe->recordingSessionController->handleImportMidiFile(file);
                       });
}
