// ═══════════════════════════════════════════════════════════════════════════
// JIVE component accessors — independent translation unit (ENG-003/QUAL-014).
//
// These are MainComponent member-function definitions (declared in
// MainComponent.h), so they reach private members from their own TU; the
// former #include-into-MainComponent.cpp arrangement is gone.
// ═══════════════════════════════════════════════════════════════════════════

#include "MainComponent.h"

#include "Diagnostics/Log.h"
#include "UI/ComboSelection.h"
#include "UI/native/AdsrCurveComponent.h"
#include "UI/native/StatusBarMidiDot.h"

namespace {

/// JIVE Button's "text" property maps to Button::setTitle (accessibility),
/// not the visible label — that lives in the button's Text child.
/// The semantic "title" is kept in sync with the label so the
/// accessibility/inspector title follows runtime language switching.
void setButtonLabel(::jive::GuiItem* buttonItem, const juce::String& text) {
    if (buttonItem == nullptr) {
        return;
    }
    buttonItem->state.setProperty("title", text, nullptr);
    for (auto child : buttonItem->state) {
        if (child.getType() == juce::Identifier("Text")) {
            child.setProperty("text", text, nullptr);
            child.setProperty("title", text, nullptr);
            child.setProperty("word-wrap", "none", nullptr);
            return;
        }
    }
}

/// Ellipsise a string to fit `maxWidth` pixels of the 14 pt UI font.
///
/// JIVE's TextComponent paints its AttributedString without clipping, so a
/// long single-line status text spills over the plugin selector combo to its
/// right. Truncate here instead (the full text goes into the tooltip).
juce::String ellipsiseForStatus(const juce::String& text, float maxWidth) {
    if (text.isEmpty()) {
        return text;
    }

    const auto safeWidth = juce::jmax(20.0f, maxWidth - 16.0f);
    const juce::Font font(juce::FontOptions(14.0f));
    if (juce::GlyphArrangement::getStringWidth(font, text) <= safeWidth) {
        return text;
    }

    const auto ellipsis = juce::String::charToString(0x2026);
    juce::String result = text;
    while (result.length() > 1 && juce::GlyphArrangement::getStringWidth(font, result + ellipsis) > safeWidth) {
        result = result.dropLastCharacters(1);
    }

    return result + ellipsis;
}
juce::String keySignatureToString(int ks) {
    switch (ks) {
    case 0:
        return "C";
    case 1:
        return "C# / Db";
    case 2:
        return "D";
    case 3:
        return "D# / Eb";
    case 4:
        return "E";
    case 5:
        return "F";
    case 6:
        return "F# / Gb";
    case -5:
    case 7:
        return "G";
    case -4:
        return "Ab";
    case -3:
        return "A";
    case -2:
        return "Bb";
    case -1:
        return "B";
    default:
        return "C";
    }
}

} // namespace

// ── JIVE plugin panel accessors ────────────────────────────────────────────

void MainComponent::setPluginPathText(const juce::String& text) {
    if (jiveRootItem == nullptr) {
        return;
    }
    if (auto* item = jive::findItemWithID(*jiveRootItem, "plugin-path-editor")) {
        if (auto* editor = dynamic_cast<juce::TextEditor*>(item->getComponent().get())) {
            editor->setText(text, juce::dontSendNotification);
            // The text can be set before the first layout (editor width 0);
            // repaint so the label renders once the panel expands. Also
            // re-assert on the next tick in case a JIVE property change
            // rebuilt the component mid-flight.
            editor->repaint();
        }
    }
}

juce::String MainComponent::getPluginPathText() const {
    if (jiveRootItem == nullptr) {
        return {};
    }
    if (auto* item = jive::findItemWithID(*jiveRootItem, "plugin-path-editor")) {
        if (auto* editor = dynamic_cast<juce::TextEditor*>(item->getComponent().get())) {
            return editor->getText();
        }
    }
    return {};
}

juce::String MainComponent::getSelectedPluginName() const {
    if (jiveRootItem == nullptr) {
        return {};
    }
    if (auto* item = jive::findItemWithID(*jiveRootItem, "plugin-selector")) {
        if (auto* combo = dynamic_cast<juce::ComboBox*>(item->getComponent().get())) {
            return combo->getText();
        }
    }
    return {};
}

void MainComponent::setPluginPanelExpanded(bool expanded) {
    appSettings.pluginPanelExpanded = expanded;
    if (jiveRootItem != nullptr) {
        // Order matters: update the expandable area FIRST, so that the panel's
        // layout pass (triggered by the height change below) reads the final
        // area height. The old order laid the panel out while the area was
        // still 112 px tall, flex-compressing the toolbar row to 0 height.
        if (auto* expandedArea = jive::findItemWithID(*jiveRootItem, "plugin-expanded-area")) {
            expandedArea->state.setProperty("height", expanded ? 112 : 0, nullptr);
        }

        if (auto* item = jive::findItemWithID(*jiveRootItem, "plugin-panel")) {
            item->state.setProperty("height", expanded ? 160 : 42, nullptr);
            // JIVE's boxModelChanged only re-lays-out the decorated item
            // itself (the top of its decorator chain), never its siblings in
            // the parent column — so the controls/keyboard below would stay
            // put and overlap the expanded panel. Reflow the main column
            // explicitly. (A root reflow is not enough: components whose
            // bounds did not change are skipped, so the panel's own children
            // would keep their compressed layout.)
            if (auto* panel = dynamic_cast<jive::FlexContainer*>(item)) {
                panel->layOutChildren();
            }

            if (auto* mainArea = dynamic_cast<jive::FlexContainer*>(jive::findItemWithID(*jiveRootItem, "main-area"))) {
                mainArea->layOutChildren();
            }
        }
    }
    settingsStore.scheduleSave(appSettings);
}

void MainComponent::setInstrumentFilterVisible(bool visible) {
    if (jiveRootItem == nullptr) {
        return;
    }
    if (auto* item = jive::findItemWithID(*jiveRootItem, "plugin-filter-combo")) {
        item->state.setProperty("visibility", visible, nullptr);
        item->state.setProperty("width", visible ? 100 : 0, nullptr);
        item->state.setProperty("margin", visible ? "0 6 0 0" : "0", nullptr);
    }
    if (auto* selectorItem = jive::findItemWithID(*jiveRootItem, "plugin-selector")) {
        selectorItem->state.setProperty("width", visible ? 180 : 286, nullptr);
    }
    if (auto* actionRow
        = dynamic_cast<jive::FlexContainer*>(jive::findItemWithID(*jiveRootItem, "plugin-action-row"))) {
        actionRow->layOutChildren();
    }
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

void MainComponent::updatePluginPanelState(const devpiano::ui::PluginPanelState& state) {
    if (jiveRootItem == nullptr) {
        return;
    }

    const juce::ScopedValueSetter<bool> svs(isUpdatingPluginSelector, true);

    auto* selectorItem = jive::findItemWithID(*jiveRootItem, "plugin-selector");
    auto* statusItem = jive::findItemWithID(*jiveRootItem, "plugin-status-label");
    auto* listItem = jive::findItemWithID(*jiveRootItem, "plugin-list-editor");
    auto* filterItem = jive::findItemWithID(*jiveRootItem, "plugin-filter-combo");

    auto* selectorCombo
        = selectorItem != nullptr ? dynamic_cast<juce::ComboBox*>(selectorItem->getComponent().get()) : nullptr;
    auto* filterCombo
        = filterItem != nullptr ? dynamic_cast<juce::ComboBox*>(filterItem->getComponent().get()) : nullptr;
    auto* listEditor = listItem != nullptr ? dynamic_cast<juce::TextEditor*>(listItem->getComponent().get()) : nullptr;

    const auto setEnabled = [](::jive::GuiItem* item, bool enabled) {
        if (item != nullptr) {
            item->state.setProperty("enabled", enabled, nullptr);
        }
    };

    if (state.isCurrentlyScanning) {
        if (selectorCombo != nullptr) {
            selectorCombo->clear(juce::dontSendNotification);
            selectorCombo->setTextWhenNothingSelected(TRANS("Scanning..."));
        }
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
            const auto filterId = (filterCombo != nullptr) ? filterCombo->getSelectedId() : 1;
            if (filterId == 2 && !state.instrumentPluginNames.isEmpty()) {
                return state.instrumentPluginNames;
            }
            if (filterId == 3 && !state.effectPluginNames.isEmpty()) {
                return state.effectPluginNames;
            }
            return state.availablePluginNames;
        }();

        if (selectorCombo != nullptr) {
            selectorCombo->clear(juce::dontSendNotification);
            selectorCombo->setTextWhenNothingSelected(TRANS("Select a scanned plugin..."));

            auto selectedIndex = devpiano::ui::preferredNameIndex(names, state.preferredSelection);
            for (int i = 0; i < names.size(); ++i) {
                selectorCombo->addItem(names[i], i + 1);
            }

            if (names.isEmpty()) {
                selectorCombo->setSelectedItemIndex(-1, juce::dontSendNotification);
            } else if (selectedIndex >= 0) {
                selectorCombo->setSelectedItemIndex(selectedIndex, juce::dontSendNotification);
            } else {
                selectorCombo->setSelectedItemIndex(0, juce::dontSendNotification);
            }
        }

        if (listEditor != nullptr) {
            listEditor->setText(TRANS(state.pluginListText), juce::dontSendNotification);
        }

        setEnabled(jive::findItemWithID(*jiveRootItem, "scan-btn"), true);
        setEnabled(jive::findItemWithID(*jiveRootItem, "browse-btn"), true);
        setEnabled(jive::findItemWithID(*jiveRootItem, "load-btn"), !names.isEmpty());
        setEnabled(jive::findItemWithID(*jiveRootItem, "unload-btn"), state.hasLoadedPlugin);
        setEnabled(jive::findItemWithID(*jiveRootItem, "editor-btn"), state.hasLoadedPlugin);
        if (auto* item = jive::findItemWithID(*jiveRootItem, "plugin-path-editor")) {
            item->state.setProperty("enabled", true, nullptr);
        }
    }

    // Status line: concise on-toolbar summary + full diagnostic info in tooltip.
    auto text = juce::String();
    if (state.hasLoadedPlugin) {
        text << TRANS("Loaded: ") << state.currentPluginName;
        if (state.isPrepared) {
            text << " @ " << juce::String(state.preparedSampleRate, 0) << " Hz / "
                 << juce::String(state.preparedBlockSize);
        } else {
            text << TRANS(" [not prepared]");
        }

        if (state.isEditorOpen) {
            text << TRANS(" | Editor open");
        }
    } else if (state.isCurrentlyScanning) {
        text << TRANS("Scanning: ") << state.scanningPluginName << "...";
    } else if (state.lastLoadError.isNotEmpty() && state.lastLoadError != "No plugin load attempted yet.") {
        text << TRANS("Load error: ") << state.lastLoadError;
    } else if (state.lastPluginName.isNotEmpty()) {
        text << TRANS("Last plugin: ") << state.lastPluginName;
    } else {
        auto summary = state.lastScanSummary;
        if (summary.startsWith("VST3 scan complete: ") && !summary.contains("no plugins")) {
            auto resultSuffix = (state.scanFailedCount > 0) ? TRANS(" failed (see log).") : TRANS(" failed.");
            text << TRANS("VST3 scan complete: ") << juce::String(state.scanPluginCount) << TRANS(" plugin(s), ")
                 << juce::String(state.scanFailedCount) << resultSuffix;
        } else if (summary.startsWith("VST3 scan found no plugins; ")) {
            text << TRANS("VST3 scan found no plugins: ") << juce::String(state.scanFailedCount)
                 << TRANS(" failed (see log).");
        } else if (summary.startsWith("Loaded cached plugin list: ")) {
            text << TRANS("Loaded cached plugin list: ") << juce::String(state.scanPluginCount) << TRANS(" plugin(s).");
        } else if (summary.isNotEmpty()) {
            text << TRANS(summary);
        } else {
            text << TRANS(state.availableFormatsDescription);
            if (state.supportsVst3) {
                text << TRANS(" [VST3 ready]");
            }
        }
    }

    lastPluginStatusText = text;
    if (statusItem != nullptr) {
        refreshPluginStatusEllipsis();
    }
}

void MainComponent::refreshPluginStatusEllipsis() {
    // Re-truncate the status text to the label's current flex-allocated
    // width. updatePluginPanelState can run before the first layout, when
    // the label width is still 0 (and the 470 px fallback overflows narrow
    // windows) — re-apply after every layout so the text never spills into
    // the combo to its right.
    if (jiveRootItem == nullptr || lastPluginStatusText.isEmpty()) {
        return;
    }
    if (auto* statusItem = jive::findItemWithID(*jiveRootItem, "plugin-status-label")) {
        const auto labelWidth = static_cast<float>(statusItem->getComponent()->getWidth());
        statusItem->state.setProperty(
            "text", ellipsiseForStatus(lastPluginStatusText, labelWidth > 0.0f ? labelWidth - 4.0f : 470.0f), nullptr);
        statusItem->state.setProperty("tooltip", lastPluginStatusText, nullptr);
    }
}

void MainComponent::refreshPluginPanelTexts() {
    if (jiveRootItem == nullptr) {
        return;
    }

    if (auto* item = jive::findItemWithID(*jiveRootItem, "plugin-path-label")) {
        item->state.setProperty("text", TRANS("VST3 Path"), nullptr);
        item->state.setProperty("title", TRANS("VST3 Path"), nullptr);
    }
    const auto setButtonText = [this](const char* id, const juce::String& text) {
        setButtonLabel(jive::findItemWithID(*jiveRootItem, id), text);
    };
    setButtonText("scan-btn", TRANS("Scan VST3"));
    setButtonText("load-btn", TRANS("Load"));
    setButtonText("unload-btn", TRANS("Unload"));
    setButtonText("editor-btn", TRANS("Open Editor"));
    if (auto* item = jive::findItemWithID(*jiveRootItem, "plugin-filter-combo")) {
        if (auto* combo = dynamic_cast<juce::ComboBox*>(item->getComponent().get())) {
            const auto prevId = combo->getSelectedId();
            combo->clear(juce::dontSendNotification);
            combo->addItem(TRANS("All"), 1);
            combo->addItem(TRANS("Instruments Only"), 2);
            combo->addItem(TRANS("Effects Only"), 3);
            combo->setSelectedId(prevId > 0 ? prevId : 1, juce::dontSendNotification);
        }
    }
    if (auto* item = jive::findItemWithID(*jiveRootItem, "plugin-selector")) {
        if (auto* combo = dynamic_cast<juce::ComboBox*>(item->getComponent().get())) {
            combo->setTextWhenNothingSelected(TRANS("Select a scanned plugin..."));
        }
    }

    // Re-apply the last state to refresh status text (locale-dependent).
    refreshPluginUiState();
}

// ── JIVE controls panel accessors ──────────────────────────────────────────

float MainComponent::getMasterGain() const {
    if (jiveRootItem == nullptr) {
        return 0.0f;
    }
    if (auto* item = jive::findItemWithID(*jiveRootItem, "volume-knob")) {
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get())) {
            return static_cast<float>(slider->getValue());
        }
    }
    return 0.0f;
}

float MainComponent::getAttack() const {
    if (jiveRootItem == nullptr) {
        return 0.0f;
    }
    if (auto* item = jive::findItemWithID(*jiveRootItem, "attack-knob")) {
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get())) {
            return static_cast<float>(slider->getValue());
        }
    }
    return 0.0f;
}

float MainComponent::getDecay() const {
    if (jiveRootItem == nullptr) {
        return 0.0f;
    }
    if (auto* item = jive::findItemWithID(*jiveRootItem, "decay-knob")) {
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get())) {
            return static_cast<float>(slider->getValue());
        }
    }
    return 0.0f;
}

float MainComponent::getSustain() const {
    if (jiveRootItem == nullptr) {
        return 0.0f;
    }
    if (auto* item = jive::findItemWithID(*jiveRootItem, "sustain-knob")) {
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get())) {
            return static_cast<float>(slider->getValue());
        }
    }
    return 0.0f;
}

float MainComponent::getRelease() const {
    if (jiveRootItem == nullptr) {
        return 0.0f;
    }
    if (auto* item = jive::findItemWithID(*jiveRootItem, "release-knob")) {
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get())) {
            return static_cast<float>(slider->getValue());
        }
    }
    return 0.0f;
}

double MainComponent::getControlsPlaybackSpeed() const {
    if (jiveRootItem == nullptr) {
        return 1.0;
    }
    if (auto* item = jive::findItemWithID(*jiveRootItem, "speed-knob")) {
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get())) {
            return slider->getValue();
        }
    }
    return 1.0;
}

SettingsModel::BuiltinTone MainComponent::getBuiltinToneFromUi() const {
    return appSettings.builtinTone;
}

float MainComponent::getPianoBrightness() const {
    if (jiveRootItem == nullptr) {
        return 0.5f;
    }
    if (auto* item = jive::findItemWithID(*jiveRootItem, "brightness-knob")) {
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get())) {
            return static_cast<float>(slider->getValue());
        }
    }
    return 0.5f;
}

float MainComponent::getPianoHammerHardness() const {
    if (jiveRootItem == nullptr) {
        return 0.5f;
    }
    if (auto* item = jive::findItemWithID(*jiveRootItem, "hardness-knob")) {
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get())) {
            return static_cast<float>(slider->getValue());
        }
    }
    return 0.5f;
}

float MainComponent::getPianoResonance() const {
    if (jiveRootItem == nullptr) {
        return 0.5f;
    }
    if (auto* item = jive::findItemWithID(*jiveRootItem, "resonance-knob")) {
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get())) {
            return static_cast<float>(slider->getValue());
        }
    }
    return 0.5f;
}

void MainComponent::setControlsValues(float masterGain, float attack, float decay, float sustain, float release) {
    if (jiveRootItem == nullptr) {
        return;
    }
    const auto setSlider = [this](const char* id, double value) {
        if (auto* item = jive::findItemWithID(*jiveRootItem, id)) {
            if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get())) {
                slider->setValue(value, juce::dontSendNotification);
            }
        }
    };
    setSlider("volume-knob", masterGain);
    setSlider("attack-knob", attack);
    setSlider("decay-knob", decay);
    setSlider("sustain-knob", sustain);
    setSlider("release-knob", release);
    if (auto* item = jive::findItemWithID(*jiveRootItem, "adsr-curve")) {
        if (auto* curve = dynamic_cast<AdsrCurveComponent*>(item->getComponent().get())) {
            curve->setParameters(attack, decay, sustain, release);
        }
    }
}

void MainComponent::setControlsPianoValues(SettingsModel::BuiltinTone tone, float brightness, float hammerHardness,
                                           float resonance) {
    juce::ignoreUnused(tone);
    if (jiveRootItem == nullptr) {
        return;
    }
    const auto setSlider = [this](const char* id, double value) {
        if (auto* item = jive::findItemWithID(*jiveRootItem, id)) {
            if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get())) {
                slider->setValue(value, juce::dontSendNotification);
            }
        }
    };
    setSlider("brightness-knob", brightness);
    setSlider("hardness-knob", hammerHardness);
    setSlider("resonance-knob", resonance);
}

void MainComponent::setControlsPlaybackSpeed(double speed) {
    if (jiveRootItem == nullptr) {
        return;
    }
    if (auto* item = jive::findItemWithID(*jiveRootItem, "speed-knob")) {
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get())) {
            slider->setValue(juce::jlimit(0.5, 2.0, speed), juce::dontSendNotification);
        }
    }
}

void MainComponent::setControlsPresets(const juce::StringArray& presetIds, const juce::String& currentPresetId,
                                       const juce::StringArray& presetDisplayNames) {
    availablePresetIds = presetIds;
    if (jiveRootItem == nullptr) {
        return;
    }

    const juce::ScopedValueSetter<bool> svs(isUpdatingPresets, true);
    auto* comboItem = jive::findItemWithID(*jiveRootItem, "preset-combo");
    auto* combo = comboItem != nullptr ? dynamic_cast<juce::ComboBox*>(comboItem->getComponent().get()) : nullptr;

    if (combo != nullptr) {
        combo->clear(juce::dontSendNotification);
        combo->setTextWhenNothingSelected(TRANS("Default"));

        const auto selectedIndex = devpiano::ui::presetIdIndex(presetIds, currentPresetId);
        for (int i = 0; i < presetIds.size(); ++i) {
            const auto displayName = (i < presetDisplayNames.size()) ? presetDisplayNames[i] : presetIds[i];
            combo->addItem(displayName, i + 1);
        }

        if (presetIds.isEmpty()) {
            combo->setSelectedItemIndex(-1, juce::dontSendNotification);
        } else {
            combo->setSelectedItemIndex(selectedIndex, juce::dontSendNotification);
        }
    }

    updateControlsPresetActionButtons();
}

juce::String MainComponent::getSelectedPresetId() const {
    if (jiveRootItem == nullptr) {
        return {};
    }
    if (auto* item = jive::findItemWithID(*jiveRootItem, "preset-combo")) {
        if (auto* combo = dynamic_cast<juce::ComboBox*>(item->getComponent().get())) {
            const auto index = combo->getSelectedItemIndex();
            if (juce::isPositiveAndBelow(index, availablePresetIds.size())) {
                return availablePresetIds[index];
            }
        }
    }
    return {};
}

void MainComponent::updateControlsPresetActionButtons() {
    const auto isUserPreset = getSelectedPresetId().isNotEmpty();
    if (jiveRootItem == nullptr) {
        return;
    }
    const auto setEnabled = [this](const char* id, bool enabled) {
        if (auto* item = jive::findItemWithID(*jiveRootItem, id)) {
            item->state.setProperty("enabled", enabled, nullptr);
        }
    };
    setEnabled("rename-preset-btn", isUserPreset);
    setEnabled("delete-preset-btn", isUserPreset);
}

void MainComponent::setRecordingControlsState(devpiano::ui::RecordingControlsState state) {
    recordingControlsState = state;
    if (jiveRootItem == nullptr) {
        return;
    }

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
    case devpiano::ui::RecordingState::idle:
        // playEnabled/backToStartEnabled/exportMidiEnabled/exportWavEnabled/
        // importMidiEnabled 的 idle 期望值与上方初始化相同，无需重复赋值。
        saveEnabled = state.hasTake;
        openEnabled = true;
        break;
    case devpiano::ui::RecordingState::recording:
    case devpiano::ui::RecordingState::recordingPaused:
        // Play/Pause is the combined pause/continue control during recording.
        recordEnabled = false;
        playEnabled = true;
        backToStartEnabled = false;
        exportMidiEnabled = false;
        exportWavEnabled = false;
        importMidiEnabled = false;
        stopEnabled = true;
        saveEnabled = false;
        openEnabled = false;
        break;
    case devpiano::ui::RecordingState::playing:
    case devpiano::ui::RecordingState::playingPaused:
        // Play/Pause toggles playback; Stop is always available (industry
        // standard: pausing never disables Stop); BackToStart rewinds.
        recordEnabled = false;
        playEnabled = true;
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
        if (auto* item = jive::findItemWithID(*jiveRootItem, id)) {
            item->state.setProperty("enabled", enabled, nullptr);
        }
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

    // Latched highlight: red while recording (incl. paused), green while
    // playing or recording — the combined Play/Pause button shows its Pause
    // glyph whenever an activity is running.
    const auto setLatched = [this](const char* id, bool active) {
        if (auto* item = jive::findItemWithID(*jiveRootItem, id)) {
            if (auto* btn = dynamic_cast<juce::Button*>(item->getComponent().get())) {
                btn->setToggleState(active, juce::dontSendNotification);
            }
        }
    };
    setLatched("record-btn",
               state.state == devpiano::ui::RecordingState::recording
                   || state.state == devpiano::ui::RecordingState::recordingPaused);
    setLatched("play-btn",
               state.state == devpiano::ui::RecordingState::playing
                   || state.state == devpiano::ui::RecordingState::recording);
    setLatched("stop-btn", false);
    setLatched("back-btn", false);
}

juce::Rectangle<int> MainComponent::getRecentFilesButtonScreenBounds() const {
    if (jiveRootItem == nullptr) {
        return {};
    }
    if (auto* item = jive::findItemWithID(*jiveRootItem, "recent-btn")) {
        return item->getComponent()->getScreenBounds();
    }
    return {};
}

void MainComponent::refreshControlsTexts() {
    if (jiveRootItem == nullptr) {
        return;
    }

    const auto setText = [this](const char* id, const juce::String& text) {
        if (auto* item = jive::findItemWithID(*jiveRootItem, id)) {
            item->state.setProperty("text", text, nullptr);
            item->state.setProperty("title", text, nullptr);
        }
    };
    const auto setButtonText = [this](const char* id, const juce::String& text) {
        setButtonLabel(jive::findItemWithID(*jiveRootItem, id), text);
    };
    const auto setTooltip = [this](const char* id, const juce::String& tooltip) {
        if (auto* item = jive::findItemWithID(*jiveRootItem, id)) {
            item->state.setProperty("tooltip", tooltip, nullptr);
        }
    };

    setText("volume-label", TRANS("Volume"));
    setText("attack-label", TRANS("Attack"));
    setText("decay-label", TRANS("Decay"));
    setText("sustain-label", TRANS("Sustain"));
    setText("release-label", TRANS("Release"));
    setText("tone-label", TRANS("Tone"));
    setText("brightness-label", TRANS("Brightness"));
    setText("hardness-label", TRANS("Hammer"));
    setText("resonance-label", TRANS("Resonance"));
    setText("preset-card-title", TRANS("Performance Preset"));
    setText("adsr-curve-title", TRANS("ADSR Curve"));
    setText("transport-card-title", TRANS("Transport Controls"));
    if (auto* item = jive::findItemWithID(*jiveRootItem, "preset-combo")) {
        if (auto* combo = dynamic_cast<juce::ComboBox*>(item->getComponent().get())) {
            combo->setTextWhenNothingSelected(TRANS("Default"));
        }
    }
    setText("speed-label", TRANS("Playback Speed"));
    setButtonText("save-preset-btn", TRANS("New"));
    setButtonText("rename-preset-btn", TRANS("Rename"));
    setButtonText("delete-preset-btn", TRANS("Delete"));
    setTooltip("record-btn", TRANS("Record"));
    setTooltip("play-btn", TRANS("Play"));
    setTooltip("stop-btn", TRANS("Stop"));
    setTooltip("back-btn", TRANS("Back to Start"));
    setButtonText("export-midi-btn", TRANS("Export"));
    setButtonText("export-wav-btn", TRANS("Export WAV"));
    setButtonText("import-midi-btn", TRANS("Import"));
    setButtonText("recent-btn", TRANS("Recent"));
    setButtonText("save-perf-btn", TRANS("Save"));
    setButtonText("open-perf-btn", TRANS("Open"));
    setButtonText("song-info-btn", TRANS("Info"));

    if (auto* item = jive::findItemWithID(*jiveRootItem, "adsr-curve")) {
        if (auto comp = item->getComponent()) {
            comp->repaint();
        }
    }

    setRecordingControlsState(recordingControlsState);
}

// ── JIVE keyboard area accessors ───────────────────────────────────────────

CustomKeyboard& MainComponent::getCustomKeyboard() {
    jassert(customKeyboardRef != nullptr);
    return *customKeyboardRef;
}

void MainComponent::setKeyboardLayout(const devpiano::core::KeyboardLayout& layout) {
    if (jiveRootItem == nullptr) {
        return;
    }
    if (auto* item = jive::findItemWithID(*jiveRootItem, "custom-keyboard")) {
        if (auto* viewport = dynamic_cast<KeyboardViewport*>(item->getComponent().get())) {
            viewport->getCustomKeyboard().setKeyboardLayout(layout);
        }
    }
}

void MainComponent::setKeyboardViewPosition(int midiNote, int pixelOffset) {
    if (jiveRootItem == nullptr) {
        return;
    }
    auto* item = jive::findItemWithID(*jiveRootItem, "custom-keyboard");
    auto* viewport = item != nullptr ? dynamic_cast<KeyboardViewport*>(item->getComponent().get()) : nullptr;
    if (viewport == nullptr) {
        return;
    }

    auto& keyboard = viewport->getCustomKeyboard();
    if (pixelOffset >= 0) {
        viewport->setViewPosition(pixelOffset, 0);
    } else if (midiNote >= 0 && midiNote <= 127) {
        int whiteCount = 0;
        for (int n = 0; n < midiNote; ++n) {
            if (devpiano::ui::isWhiteKey(n)) {
                ++whiteCount;
            }
        }
        auto x = static_cast<int>(static_cast<float>(whiteCount) * keyboard.getKeyboardSettings().keyWidth);
        viewport->setViewPosition(x, 0);
    }
}

int MainComponent::getKeyboardViewPositionX() const noexcept {
    if (jiveRootItem == nullptr) {
        return 0;
    }
    if (auto* item = jive::findItemWithID(*jiveRootItem, "custom-keyboard")) {
        if (auto* viewport = dynamic_cast<KeyboardViewport*>(item->getComponent().get())) {
            return viewport->getViewPositionX();
        }
    }
    return 0;
}

void MainComponent::finishPluginUiAction(bool shouldSaveSettings) {
    if (shouldSaveSettings) {
        saveSettingsSoon();
    }

    refreshReadOnlyUiStateFromCurrentSnapshot();
    restoreKeyboardFocus();
}

void MainComponent::logCurrentAudioDeviceDiagnostics(const juce::String& context) const {
    const auto diagnostics
        = devpiano::audio::buildAudioDeviceDiagnostics(appSettings.audioDeviceState.get(), deviceManager);
    DP_LOG_INFO("[AudioDevice] " + context + "\n" + diagnostics.detailedSummary);
}

devpiano::core::AppState MainComponent::buildAppStateSnapshot() const {
    return devpiano::core::buildCurrentAppStateSnapshot(
        appSettings, deviceManager, pluginHost,
        pluginOperationController != nullptr && pluginOperationController->hasEditorWindowOpen(), keyboardMidiMapper);
}

void MainComponent::applyLanguage(const juce::String& code) {
    devpiano::locale::activate(devpiano::locale::codeToLanguage(code));
    refreshAllTexts();
}

void MainComponent::refreshAllTexts() {
    if (jiveRootItem == nullptr) {
        return;
    }
    refreshPluginPanelTexts();
    refreshControlsTexts();
    devpiano::ui::jive::refreshTitles(*jiveRootItem);
    if (customKeyboardRef != nullptr) {
        customKeyboardRef->repaint();
    }
    updateStatusBar();
}

double MainComponent::getCurrentRuntimeSampleRate() const {
    if (auto* device = deviceManager.getCurrentAudioDevice()) {
        const auto rate = device->getCurrentSampleRate();
        if (rate > 0.0) {
            return rate;
        }
    }

    return appSettings.getAudioSettingsView().sampleRate;
}

int MainComponent::getCurrentRuntimeBlockSize() const {
    if (auto* device = deviceManager.getCurrentAudioDevice()) {
        const auto size = device->getCurrentBufferSizeSamples();
        if (size > 0) {
            return size;
        }
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
            if (ext == ".devpiano") {
                prefix = juce::String::fromUTF8("\xe2\x99\xaa "); // ♪
            } else if (ext == ".mid" || ext == ".midi") {
                prefix = juce::String::fromUTF8("\xe2\x99\xab "); // ♫
            } else {
                prefix = "? ";
            }

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
                           if (safe == nullptr) {
                               return;
                           }

                           if (result == 0) {
                               return;
                           }

                           if (result == clearId) {
                               safe->recentFiles.clear();
                               safe->saveRecentFiles();
                               return;
                           }

                           const auto index = result - 1;
                           if (!juce::isPositiveAndBelow(index, numFiles)) {
                               return;
                           }

                           auto file = safe->recentFiles.getFile(index);
                           if (!file.exists()) {
                               return;
                           }

                           auto ext = file.getFileExtension().toLowerCase();
                           if (ext == ".devpiano") {
                               safe->recordingSessionController->handleOpenPerformanceFile(file);
                           } else if (ext == ".mid" || ext == ".midi") {
                               safe->recordingSessionController->handleImportMidiFile(file);
                           }
                       });
}
// ── JIVE status bar accessors ──────────────────────────────────────────────

void MainComponent::updateStatusBar() {
    if (jiveRootItem == nullptr) {
        return;
    }

    // 1. Left: active plugin/synth & preset name (or transient status toast)
    if (auto* pluginLabelItem = jive::findItemWithID(*jiveRootItem, "plugin-name-label")) {
        juce::String displayText;
        if (statusToastTicksRemaining > 0 && statusToastText.isNotEmpty()) {
            displayText = statusToastText;
        } else {
            juce::String sourceName;
            if (auto* desc = pluginHost.getLoadedPluginDescription()) {
                sourceName = "VST3: " + desc->name;
            } else {
                sourceName = (appSettings.builtinTone == SettingsModel::BuiltinTone::piano) ? "Built-in: Piano"
                                                                                            : "Built-in: Sine";
            }
            const auto preset
                = (presetFlowSupport != nullptr) ? presetFlowSupport->getCurrentPresetId() : juce::String {};
            displayText = preset.isNotEmpty() ? (sourceName + " (" + preset + ")") : sourceName;
        }
        pluginLabelItem->state.setProperty("text", displayText, nullptr);
        pluginLabelItem->state.setProperty("title", displayText, nullptr);
    }

    // 2. Centre: audio driver backend, sample rate, buffer size, latency, CPU load
    if (auto* audioInfoItem = jive::findItemWithID(*jiveRootItem, "audio-info-label")) {
        juce::String audioText;
        if (auto* dev = deviceManager.getCurrentAudioDevice()) {
            const auto type = dev->getTypeName();
            const auto sr = dev->getCurrentSampleRate();
            const auto bs = dev->getCurrentBufferSizeSamples();
            const auto latencyMs = (sr > 0.0) ? (static_cast<float>(bs) / static_cast<float>(sr) * 1000.0f) : 0.0f;
            const auto cpu = juce::roundToInt(deviceManager.getCpuUsage() * 100.0f);

            audioText = type + " • " + juce::String(sr / 1000.0, 1) + " kHz / " + juce::String(bs) + " spl ("
                + juce::String(latencyMs, 1) + " ms) • CPU: " + juce::String(cpu) + "%";
        } else {
            audioText = TRANS("No Audio Device");
        }
        audioInfoItem->state.setProperty("text", audioText, nullptr);
        audioInfoItem->state.setProperty("title", audioText, nullptr);
    }

    // 3. Right: key signature, transpose, keyboard layout
    if (auto* timeLabelItem = jive::findItemWithID(*jiveRootItem, "time-label")) {
        const auto keyName = keySignatureToString(appSettings.keySignature);
        const auto transposeStr = (appSettings.midiTranspose ? (TRANS("Transpose: On") + " / ") : "")
            + (appSettings.keySignature >= 0 ? "+" : "") + juce::String(appSettings.keySignature);
        auto layoutName = keyboardMidiMapper.getLayout().name;
        if (layoutName.isEmpty()) {
            layoutName = "Standard";
        }
        const auto statusRight = keyName + " (" + transposeStr + ") • " + layoutName;
        timeLabelItem->state.setProperty("text", statusRight, nullptr);
        timeLabelItem->state.setProperty("title", statusRight, nullptr);
    }
}

void MainComponent::showStatusMessage(const juce::String& text, int timeoutMs) {
    statusToastText = text;
    statusToastTicksRemaining = juce::jmax(1, timeoutMs * 30 / 1000);
    updateStatusBar();
}

void MainComponent::notifyMidiActivity() {
    if (auto* dot = getStatusBarMidiDot()) {
        dot->triggerActivity(4);
    }
}

StatusBarMidiDot* MainComponent::getStatusBarMidiDot() const {
    if (jiveRootItem == nullptr) {
        return nullptr;
    }
    if (auto* dotItem = jive::findItemWithID(*jiveRootItem, "midi-dot")) {
        return dynamic_cast<StatusBarMidiDot*>(dotItem->getComponent().get());
    }
    return nullptr;
}
