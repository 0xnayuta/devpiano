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
    // 支持 -5..11 范围的正负升降半音索引
    static constexpr const char* kKeyNames[] = {
        "G",       "G# / Ab", "A", "A# / Bb", "B", // -5 .. -1
        "C",       "C# / Db", "D", "D# / Eb", "E", // 0 .. 4
        "F",       "F# / Gb", "G", "G# / Ab", "A", // 5 .. 9
        "A# / Bb", "B" // 10 .. 11
    };
    if (ks >= -5 && ks <= 11) {
        return kKeyNames[ks + 5];
    }
    return "C";
}

} // namespace

// ── JIVE plugin panel accessors ────────────────────────────────────────────

void MainComponent::setPluginPathText(const juce::String& text) {
    if (auto* editor = viewHost.find<juce::TextEditor>("plugin-path-editor")) {
        editor->setText(text, juce::dontSendNotification);
        editor->repaint();
    }
}

juce::String MainComponent::getPluginPathText() const {
    if (auto* editor = viewHost.find<juce::TextEditor>("plugin-path-editor")) {
        return editor->getText();
    }
    return {};
}

juce::String MainComponent::getSelectedPluginName() const {
    if (auto* combo = viewHost.find<juce::ComboBox>("plugin-selector")) {
        return combo->getText();
    }
    return {};
}

void MainComponent::setPluginPanelExpanded(bool expanded) {
    appSettings.pluginPanelExpanded = expanded;
    if (viewHost.isValid()) {
        viewHost.setProperty("plugin-expanded-area", "height", expanded ? 112 : 0);
        viewHost.setProperty("plugin-panel", "height", expanded ? 160 : 42);
        viewHost.relayoutContainer("plugin-panel");
        viewHost.relayoutContainer("main-area");
    }
    settingsStore.scheduleSave(appSettings);
}

void MainComponent::setInstrumentFilterVisible(bool visible) {
    if (!viewHost.isValid()) {
        return;
    }
    viewHost.setProperty("plugin-filter-combo", "visibility", visible);
    viewHost.setProperty("plugin-filter-combo", "width", visible ? 100 : 0);
    viewHost.setProperty("plugin-filter-combo", "margin", visible ? "0 6 0 0" : "0");
    viewHost.setProperty("plugin-selector", "width", visible ? 180 : 286);
    viewHost.relayoutContainer("plugin-action-row");
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

namespace {

juce::String formatPluginStatusSummary(const devpiano::ui::PluginPanelState& state) {
    if (state.hasLoadedPlugin) {
        auto text = TRANS("Loaded: ") + state.currentPluginName;
        if (state.isPrepared) {
            text << " @ " << juce::String(state.preparedSampleRate, 0) << " Hz / "
                 << juce::String(state.preparedBlockSize);
        } else {
            text << TRANS(" [not prepared]");
        }
        if (state.isEditorOpen) {
            text << TRANS(" | Editor open");
        }
        return text;
    }

    if (state.isCurrentlyScanning) {
        return TRANS("Scanning: ") + state.scanningPluginName + "...";
    }

    if (state.lastLoadError.isNotEmpty() && state.lastLoadError != "No plugin load attempted yet.") {
        return TRANS("Load error: ") + state.lastLoadError;
    }

    if (state.lastPluginName.isNotEmpty()) {
        return TRANS("Last plugin: ") + state.lastPluginName;
    }

    const auto& summary = state.lastScanSummary;
    if (summary.startsWith("VST3 scan complete: ") && !summary.contains("no plugins")) {
        auto resultSuffix = (state.scanFailedCount > 0) ? TRANS(" failed (see log).") : TRANS(" failed.");
        return TRANS("VST3 scan complete: ") + juce::String(state.scanPluginCount) + TRANS(" plugin(s), ")
            + juce::String(state.scanFailedCount) + resultSuffix;
    }
    if (summary.startsWith("VST3 scan found no plugins; ")) {
        return TRANS("VST3 scan found no plugins: ") + juce::String(state.scanFailedCount)
            + TRANS(" failed (see log).");
    }
    if (summary.startsWith("Loaded cached plugin list: ")) {
        return TRANS("Loaded cached plugin list: ") + juce::String(state.scanPluginCount) + TRANS(" plugin(s).");
    }
    if (summary.isNotEmpty()) {
        return TRANS(summary);
    }

    auto text = TRANS(state.availableFormatsDescription);
    if (state.supportsVst3) {
        text << TRANS(" [VST3 ready]");
    }
    return text;
}

void updateScanningPluginPanel(devpiano::ui::ViewHost& viewHost, const devpiano::ui::PluginPanelState& state,
                               juce::ComboBox* selectorCombo, juce::TextEditor* listEditor) {
    if (selectorCombo != nullptr) {
        selectorCombo->clear(juce::dontSendNotification);
        selectorCombo->setTextWhenNothingSelected(TRANS("Scanning..."));
    }
    if (listEditor != nullptr) {
        auto scanText = TRANS("Scanning VST3 plugins...") + "\n";
        scanText << (state.scanningPluginName.isNotEmpty() ? state.scanningPluginName : TRANS("Preparing..."));
        listEditor->setText(scanText, juce::dontSendNotification);
    }
    viewHost.setEnabled("scan-btn", false);
    viewHost.setEnabled("browse-btn", false);
    viewHost.setEnabled("load-btn", false);
}

void updateIdlePluginPanel(devpiano::ui::ViewHost& viewHost, const devpiano::ui::PluginPanelState& state,
                           juce::ComboBox* selectorCombo, juce::ComboBox* filterCombo, juce::TextEditor* listEditor) {
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

    viewHost.setEnabled("scan-btn", true);
    viewHost.setEnabled("browse-btn", true);
    viewHost.setEnabled("load-btn", !names.isEmpty());
    viewHost.setEnabled("unload-btn", state.hasLoadedPlugin);
    viewHost.setEnabled("editor-btn", state.hasLoadedPlugin);
    viewHost.setEnabled("plugin-path-editor", true);
}

} // namespace

void MainComponent::updatePluginPanelState(const devpiano::ui::PluginPanelState& state) {
    if (!viewHost.isValid()) {
        return;
    }

    const juce::ScopedValueSetter<bool> svs(isUpdatingPluginSelector, true);

    auto* selectorCombo = viewHost.find<juce::ComboBox>("plugin-selector");
    auto* filterCombo = viewHost.find<juce::ComboBox>("plugin-filter-combo");
    auto* listEditor = viewHost.find<juce::TextEditor>("plugin-list-editor");

    if (state.isCurrentlyScanning) {
        updateScanningPluginPanel(viewHost, state, selectorCombo, listEditor);
    } else {
        updateIdlePluginPanel(viewHost, state, selectorCombo, filterCombo, listEditor);
    }

    lastPluginStatusText = formatPluginStatusSummary(state);
    refreshPluginStatusEllipsis();
}

void MainComponent::refreshPluginStatusEllipsis() {
    // Re-truncate the status text to the label's current flex-allocated
    // width. updatePluginPanelState can run before the first layout, when
    // the label width is still 0 (and the 470 px fallback overflows narrow
    // windows) — re-apply after every layout so the text never spills into
    // the combo to its right.
    if (!viewHost.isValid() || lastPluginStatusText.isEmpty()) {
        return;
    }
    if (auto* statusComp = viewHost.find("plugin-status-label")) {
        const auto labelWidth = static_cast<float>(statusComp->getWidth());
        viewHost.setProperty("plugin-status-label", "text",
                             ellipsiseForStatus(lastPluginStatusText, labelWidth > 0.0f ? labelWidth - 4.0f : 470.0f));
        viewHost.setProperty("plugin-status-label", "tooltip", lastPluginStatusText);
    }
}

void MainComponent::refreshPluginPanelTexts() {
    if (!viewHost.isValid()) {
        return;
    }

    viewHost.setText("plugin-path-label", TRANS("VST3 Path"));
    viewHost.setButtonLabel("scan-btn", TRANS("Scan VST3"));
    viewHost.setButtonLabel("load-btn", TRANS("Load"));
    viewHost.setButtonLabel("unload-btn", TRANS("Unload"));
    viewHost.setButtonLabel("editor-btn", TRANS("Open Editor"));

    if (auto* combo = viewHost.find<juce::ComboBox>("plugin-filter-combo")) {
        const auto prevId = combo->getSelectedId();
        combo->clear(juce::dontSendNotification);
        combo->addItem(TRANS("All"), 1);
        combo->addItem(TRANS("Instruments Only"), 2);
        combo->addItem(TRANS("Effects Only"), 3);
        combo->setSelectedId(prevId > 0 ? prevId : 1, juce::dontSendNotification);
    }
    if (auto* combo = viewHost.find<juce::ComboBox>("plugin-selector")) {
        combo->setTextWhenNothingSelected(TRANS("Select a scanned plugin..."));
    }

    // Re-apply the last state to refresh status text (locale-dependent).
    refreshPluginUiState();
}

// ── JIVE controls panel accessors ──────────────────────────────────────────

float MainComponent::getMasterGain() const {
    return static_cast<float>(viewHost.getSliderValue("volume-knob", 0.0));
}

float MainComponent::getAttack() const {
    return static_cast<float>(viewHost.getSliderValue("attack-knob", 0.0));
}

float MainComponent::getDecay() const {
    return static_cast<float>(viewHost.getSliderValue("decay-knob", 0.0));
}

float MainComponent::getSustain() const {
    return static_cast<float>(viewHost.getSliderValue("sustain-knob", 0.0));
}

float MainComponent::getRelease() const {
    return static_cast<float>(viewHost.getSliderValue("release-knob", 0.0));
}

double MainComponent::getControlsPlaybackSpeed() const {
    return viewHost.getSliderValue("speed-knob", 1.0);
}

SettingsModel::BuiltinTone MainComponent::getBuiltinToneFromSettings() const {
    return appSettings.builtinTone;
}

float MainComponent::getPianoBrightness() const {
    return static_cast<float>(viewHost.getSliderValue("brightness-knob", 0.5));
}

float MainComponent::getPianoHammerHardness() const {
    return static_cast<float>(viewHost.getSliderValue("hardness-knob", 0.5));
}

float MainComponent::getPianoResonance() const {
    return static_cast<float>(viewHost.getSliderValue("resonance-knob", 0.5));
}

void MainComponent::setControlsValues(float masterGain, float attack, float decay, float sustain, float release) {
    if (!viewHost.isValid()) {
        return;
    }
    viewHost.setSliderValue("volume-knob", masterGain);
    viewHost.setSliderValue("attack-knob", attack);
    viewHost.setSliderValue("decay-knob", decay);
    viewHost.setSliderValue("sustain-knob", sustain);
    viewHost.setSliderValue("release-knob", release);
    if (auto* curve = viewHost.find<AdsrCurveComponent>("adsr-curve")) {
        curve->setParameters(attack, decay, sustain, release);
    }
}

void MainComponent::setControlsPianoValues(SettingsModel::BuiltinTone tone, float brightness, float hammerHardness,
                                           float resonance) {
    juce::ignoreUnused(tone);
    if (!viewHost.isValid()) {
        return;
    }
    viewHost.setSliderValue("brightness-knob", brightness);
    viewHost.setSliderValue("hardness-knob", hammerHardness);
    viewHost.setSliderValue("resonance-knob", resonance);
}

void MainComponent::setControlsPlaybackSpeed(double speed) {
    viewHost.setSliderValue("speed-knob", juce::jlimit(0.5, 2.0, speed));
}

void MainComponent::setControlsPresets(const juce::StringArray& presetIds, const juce::String& currentPresetId,
                                       const juce::StringArray& presetDisplayNames) {
    availablePresetIds = presetIds;
    if (!viewHost.isValid()) {
        return;
    }

    const juce::ScopedValueSetter<bool> svs(isUpdatingPresets, true);
    auto* combo = viewHost.find<juce::ComboBox>("preset-combo");

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
    if (auto* combo = viewHost.find<juce::ComboBox>("preset-combo")) {
        const auto index = combo->getSelectedItemIndex();
        if (juce::isPositiveAndBelow(index, availablePresetIds.size())) {
            return availablePresetIds[index];
        }
    }
    return {};
}

void MainComponent::updateControlsPresetActionButtons() {
    const auto isUserPreset = getSelectedPresetId().isNotEmpty();
    viewHost.setEnabled("rename-preset-btn", isUserPreset);
    viewHost.setEnabled("delete-preset-btn", isUserPreset);
}

void MainComponent::setRecordingControlsState(devpiano::ui::RecordingControlsState state) {
    recordingControlsState = state;
    if (!viewHost.isValid()) {
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
        saveEnabled = state.hasTake;
        openEnabled = true;
        break;
    case devpiano::ui::RecordingState::recording:
    case devpiano::ui::RecordingState::recordingPaused:
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

    viewHost.setEnabled("record-btn", recordEnabled);
    viewHost.setEnabled("play-btn", playEnabled);
    viewHost.setEnabled("stop-btn", stopEnabled);
    viewHost.setEnabled("back-btn", backToStartEnabled);
    viewHost.setEnabled("import-midi-btn", importMidiEnabled);
    viewHost.setEnabled("export-midi-btn", exportMidiEnabled);
    viewHost.setEnabled("export-wav-btn", exportWavEnabled);
    viewHost.setEnabled("save-perf-btn", saveEnabled);
    viewHost.setEnabled("open-perf-btn", openEnabled);

    const auto setLatched = [this](const char* id, bool active) {
        if (auto* btn = viewHost.find<juce::Button>(id)) {
            btn->setToggleState(active, juce::dontSendNotification);
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
    if (auto* btn = viewHost.find("recent-btn")) {
        return btn->getScreenBounds();
    }
    return {};
}

void MainComponent::refreshControlsTexts() {
    if (!viewHost.isValid()) {
        return;
    }

    viewHost.setText("volume-label", TRANS("Volume"));
    viewHost.setText("attack-label", TRANS("Attack"));
    viewHost.setText("decay-label", TRANS("Decay"));
    viewHost.setText("sustain-label", TRANS("Sustain"));
    viewHost.setText("release-label", TRANS("Release"));
    viewHost.setText("tone-label", TRANS("Tone"));
    viewHost.setText("brightness-label", TRANS("Brightness"));
    viewHost.setText("hardness-label", TRANS("Hammer"));
    viewHost.setText("resonance-label", TRANS("Resonance"));
    viewHost.setText("preset-card-title", TRANS("Performance Preset"));
    viewHost.setText("adsr-curve-title", TRANS("ADSR Curve"));
    viewHost.setText("transport-card-title", TRANS("Transport Controls"));
    if (auto* combo = viewHost.find<juce::ComboBox>("preset-combo")) {
        combo->setTextWhenNothingSelected(TRANS("Default"));
    }
    viewHost.setText("speed-label", TRANS("Playback Speed"));
    viewHost.setButtonLabel("save-preset-btn", TRANS("New"));
    viewHost.setButtonLabel("rename-preset-btn", TRANS("Rename"));
    viewHost.setButtonLabel("delete-preset-btn", TRANS("Delete"));
    viewHost.setProperty("record-btn", "tooltip", TRANS("Record"));
    viewHost.setProperty("play-btn", "tooltip", TRANS("Play"));
    viewHost.setProperty("stop-btn", "tooltip", TRANS("Stop"));
    viewHost.setProperty("back-btn", "tooltip", TRANS("Back to Start"));
    viewHost.setButtonLabel("export-midi-btn", TRANS("Export"));
    viewHost.setButtonLabel("export-wav-btn", TRANS("Export WAV"));
    viewHost.setButtonLabel("import-midi-btn", TRANS("Import"));
    viewHost.setButtonLabel("recent-btn", TRANS("Recent"));
    viewHost.setButtonLabel("save-perf-btn", TRANS("Save"));
    viewHost.setButtonLabel("open-perf-btn", TRANS("Open"));
    viewHost.setButtonLabel("song-info-btn", TRANS("Info"));

    if (auto* curve = viewHost.find<AdsrCurveComponent>("adsr-curve")) {
        curve->repaint();
    }

    setRecordingControlsState(recordingControlsState);
}

CustomKeyboard& MainComponent::getCustomKeyboard() {
    jassert(customKeyboardRef != nullptr);
    if (customKeyboardRef == nullptr) {
        static CustomKeyboard fallbackKeyboard(audioEngine.getKeyboardState());
        return fallbackKeyboard;
    }
    return *customKeyboardRef;
}

void MainComponent::setKeyboardLayout(const devpiano::core::KeyboardLayout& layout) {
    if (auto* viewport = viewHost.find<KeyboardViewport>("custom-keyboard")) {
        viewport->getCustomKeyboard().setKeyboardLayout(layout);
    }
}

void MainComponent::setKeyboardViewPosition(int midiNote, int pixelOffset) {
    auto* viewport = viewHost.find<KeyboardViewport>("custom-keyboard");
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
    if (auto* viewport = viewHost.find<KeyboardViewport>("custom-keyboard")) {
        return viewport->getViewPositionX();
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
    if (!viewHost.isValid()) {
        return;
    }
    refreshPluginPanelTexts();
    refreshControlsTexts();
    viewHost.refreshTitles();
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

namespace {

juce::String getRecentFileIconPrefix(const juce::String& extension) {
    if (extension == ".devpiano") {
        return juce::String::fromUTF8("\xe2\x99\xaa "); // ♪
    }
    if (extension == ".mid" || extension == ".midi") {
        return juce::String::fromUTF8("\xe2\x99\xab "); // ♫
    }
    return "? ";
}

} // namespace

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
            menu.addItem(itemId++, getRecentFileIconPrefix(ext) + name);
        }
    }

    const int clearId = itemId;
    if (numFiles > 0) {
        menu.addSeparator();
        menu.addItem(clearId, TRANS("Clear Recent Files"));
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(getRecentFilesButtonScreenBounds()),
                       [safe = juce::Component::SafePointer<MainComponent>(this), numFiles, clearId](int result) {
                           if (safe == nullptr || result == 0) {
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
    if (!viewHost.isValid()) {
        return;
    }

    // 1. Left: active plugin/synth & preset name (or transient status toast)
    juce::String displayText;
    if (statusToastTicksRemaining > 0 && statusToastText.isNotEmpty()) {
        displayText = statusToastText;
    } else {
        juce::String sourceName;
        if (const auto* desc = pluginHost.getLoadedPluginDescription()) {
            sourceName = "VST3: " + desc->name;
        } else {
            sourceName
                = (appSettings.builtinTone == SettingsModel::BuiltinTone::piano) ? "Built-in: Piano" : "Built-in: Sine";
        }
        const auto preset = (presetFlowSupport != nullptr) ? presetFlowSupport->getCurrentPresetId() : juce::String {};
        displayText = preset.isNotEmpty() ? (sourceName + " (" + preset + ")") : sourceName;
    }
    viewHost.setText("plugin-name-label", displayText);

    // 2. Centre: audio driver backend, sample rate, buffer size, latency, CPU load
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
    viewHost.setText("audio-info-label", audioText);

    // 3. Right: key signature, transpose, keyboard layout
    const auto keyName = keySignatureToString(appSettings.keySignature);
    const auto transposeStr = (appSettings.midiTranspose ? (TRANS("Transpose: On") + " / ") : "")
        + (appSettings.keySignature >= 0 ? "+" : "") + juce::String(appSettings.keySignature);
    auto layoutName = keyboardMidiMapper.getLayout().name;
    if (layoutName.isEmpty()) {
        layoutName = "Standard";
    }
    const auto statusRight = keyName + " (" + transposeStr + ") • " + layoutName;
    viewHost.setText("time-label", statusRight);
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
    return viewHost.find<StatusBarMidiDot>("midi-dot");
}
