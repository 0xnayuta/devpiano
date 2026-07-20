#include "UI/CustomKeyboard.h"

#include "Layout/PresetFlowSupport.h"

#include "Diagnostics/Log.h"
#include "MainComponent.h"

namespace devpiano::layout {

PresetFlowSupport::PresetFlowSupport(MainComponent& ownerIn)
    : owner(ownerIn) {
    refreshCache();
}

PresetFlowSupport::~PresetFlowSupport() = default;

// ---- Cache management ----

void PresetFlowSupport::refreshCache() {
    cachedPresets = scanPresetDirectory();
}

// ---- UI data ----

juce::StringArray PresetFlowSupport::getPresetIds() const {
    juce::StringArray ids;
    for (const auto& p : cachedPresets)
        ids.add(p.name);
    return ids;
}

juce::StringArray PresetFlowSupport::getPresetDisplayNames() const {
    juce::StringArray names;
    for (const auto& p : cachedPresets)
        names.add(p.name.isNotEmpty() ? p.name : "Untitled");
    return names;
}

juce::String PresetFlowSupport::getCurrentPresetId() const {
    return currentPresetId;
}

int PresetFlowSupport::getPresetCount() const {
    return static_cast<int>(cachedPresets.size());
}

// ---- Apply ----

void PresetFlowSupport::applyPresetById(const juce::String& presetId) {
    refreshCache();

    for (const auto& p : cachedPresets) {
        if (p.name == presetId) {
            applyPresetData(p);
            currentPresetId = presetId;
            return;
        }
    }

    DP_LOG_WARN("[Preset] preset not found: " + presetId);
}

void PresetFlowSupport::applyPresetByIndex(int index) {
    refreshCache();
    if (index < 0 || index >= static_cast<int>(cachedPresets.size()))
        return;

    const auto& p = cachedPresets[static_cast<std::size_t>(index)];
    currentPresetId = p.name;
    applyPresetData(p);
}

void PresetFlowSupport::applyPresetData(const PerformancePreset& preset) {
    // 0. Recording integration: record the preset change if currently recording.
    // Only record file-backed (user) presets — the built-in default has no persistent
    // identity and recording it would write a wrong index.
    if (owner.recordingEngine.isRecording() && preset.layout.id != "default.preset.builtin") {
        // Find the preset's index in our cached list for the presetId field
        uint8_t presetIdx = 0;
        for (std::size_t i = 0; i < cachedPresets.size(); ++i) {
            if (cachedPresets[i].name == preset.name) {
                presetIdx = static_cast<uint8_t>(i);
                break;
            }
        }
        auto pos = owner.recordingEngine.getCurrentPositionSamples();
        owner.recordingEngine.recordPresetChange(presetIdx, pos);
    }

    commitPreset(preset);
    updateUiAfterCommit();
}

void PresetFlowSupport::commitPreset(const PerformancePreset& preset) {
    auto& s = owner.appSettings;

    // 1. KeyboardLayout
    owner.keyboardMidiMapper.setLayout(preset.layout);
    owner.keyboardPanel.setKeyboardLayout(preset.layout);

    // 2. ChannelMatrix
    s.channelMatrix = preset.channelMatrix;
    owner.reconfigureChannelMapper();

    // 3. Keyboard display / musical settings
    s.keySignature = preset.keySignature;
    s.midiTranspose = preset.midiTranspose;
    s.keyboardColourMode = preset.colourMode;
    s.keyboardNoteDisplay = preset.noteDisplay;
    s.keyboardFadeSpeed = preset.fadeSpeed;
    s.customKeyLabels = preset.customKeyLabels;
    s.customKeyColours = preset.customKeyColours;

    // 4. Persist preset identity
    s.lastActivePresetId = preset.name;
}

void PresetFlowSupport::updateUiAfterCommit() {
    owner.syncUiFromSettings();
    owner.keyboardPanel.getCustomKeyboard().repaint();
    owner.saveSettingsSoon();
}

// ---- Capture current state as a preset ----

PerformancePreset PresetFlowSupport::captureCurrentState(const juce::String& name) const {
    PerformancePreset preset;
    preset.name = name;
    preset.layout = owner.keyboardMidiMapper.getLayout();
    preset.layout.name = name; // Override layout name to match preset name
    preset.channelMatrix = owner.appSettings.channelMatrix;
    preset.keySignature = owner.appSettings.keySignature;
    preset.midiTranspose = owner.appSettings.midiTranspose;
    preset.colourMode = owner.appSettings.keyboardColourMode;
    preset.noteDisplay = owner.appSettings.keyboardNoteDisplay;
    preset.fadeSpeed = owner.appSettings.keyboardFadeSpeed;
    preset.previewAlpha = 0.0f;
    preset.customKeyLabels = owner.appSettings.customKeyLabels;
    preset.customKeyColours = owner.appSettings.customKeyColours;
    return preset;
}

// ---- CRUD ----

void PresetFlowSupport::handleSaveAsNewPreset() {
    juce::AlertWindow dialog(TRANS("Save as New Preset"), TRANS("Preset Name:"), juce::AlertWindow::NoIcon);
    dialog.addTextEditor("name", {}, {});
    dialog.addButton(TRANS("OK"), 1);
    dialog.addButton(TRANS("Cancel"), 0);

    if (dialog.runModalLoop() != 1)
        return;

    auto rawName = dialog.getTextEditorContents("name").trim();
    if (rawName.isEmpty())
        return;

    auto fileName = sanitisePresetFileName(rawName);
    auto file = getPresetDirectory().getChildFile(fileName + ".devpiano.preset");

    if (file.existsAsFile()) {
        auto overwrite = juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::QuestionIcon, TRANS("Overwrite Preset?"),
            TRANS("A preset named \"") + rawName + TRANS("\" already exists.\nDo you want to overwrite it?"),
            TRANS("Overwrite"), TRANS("Cancel"));
        if (!overwrite)
            return;
    }

    auto preset = captureCurrentState(rawName);

    if (savePreset(preset, file)) {
        DP_LOG_INFO("[Preset] saved: " + file.getFullPathName());
        refreshCache();
        currentPresetId = preset.name;
        owner.appSettings.lastActivePresetId = currentPresetId;
        updateUiAfterCommit();
    } else {
        DP_LOG_ERROR("[Preset] save FAILED: " + file.getFullPathName());
    }
}

void PresetFlowSupport::handleRenamePreset() {
    if (currentPresetId.isEmpty())
        return;

    refreshCache();
    auto it = std::find_if(cachedPresets.begin(), cachedPresets.end(),
                           [this](const auto& p) { return p.name == currentPresetId; });
    if (it == cachedPresets.end())
        return;

    auto oldName = it->name;
    auto oldFile = getPresetDirectory().getChildFile(sanitisePresetFileName(oldName) + ".devpiano.preset");

    juce::AlertWindow dialog(TRANS("Rename Preset"), TRANS("Preset Name:"), juce::AlertWindow::NoIcon);
    dialog.addTextEditor("name", oldName, {});
    dialog.addButton(TRANS("OK"), 1);
    dialog.addButton(TRANS("Cancel"), 0);

    if (dialog.runModalLoop() != 1)
        return;

    auto newName = dialog.getTextEditorContents("name").trim();
    if (newName.isEmpty() || newName == oldName)
        return;

    auto preset = *it;
    preset.name = newName;
    preset.layout.name = newName;

    auto newFile = getPresetDirectory().getChildFile(sanitisePresetFileName(newName) + ".devpiano.preset");

    if (savePreset(preset, newFile)) {
        oldFile.deleteFile();
        DP_LOG_INFO("[Preset] renamed: " + oldName + " -> " + newName);
        currentPresetId = newName;
        owner.appSettings.lastActivePresetId = currentPresetId;
        refreshCache();
        updateUiAfterCommit();
    }
}

void PresetFlowSupport::handleDeletePreset() {
    if (currentPresetId.isEmpty())
        return;

    refreshCache();
    auto it = std::find_if(cachedPresets.begin(), cachedPresets.end(),
                           [this](const auto& p) { return p.name == currentPresetId; });
    if (it == cachedPresets.end())
        return;

    auto result = juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::QuestionIcon, TRANS("Delete Preset?"),
        TRANS("Delete preset \"") + it->name + "\"?",
        TRANS("Delete"), TRANS("Cancel"));

    if (!result)
        return;

    auto file = getPresetDirectory().getChildFile(sanitisePresetFileName(it->name) + ".devpiano.preset");
    file.deleteFile();

    DP_LOG_INFO("[Preset] deleted: " + it->name);

    // If the deleted preset was current, revert to default
    if (currentPresetId == it->name) {
        applyPresetData(makeDefaultPreset());
        currentPresetId.clear();
        owner.appSettings.lastActivePresetId = {};
    }
    refreshCache();
    updateUiAfterCommit();  // must run after refreshCache so combo reflects the deletion
}

void PresetFlowSupport::handleImportPresetFile(const juce::File& file) {
    auto loaded = loadPreset(file);
    if (!loaded.has_value()) {
        DP_LOG_ERROR("[Preset] import FAILED: " + file.getFullPathName());
        return;
    }

    auto destFile = getPresetDirectory().getChildFile(
        sanitisePresetFileName(loaded->name) + ".devpiano.preset");

    if (savePreset(*loaded, destFile)) {
        DP_LOG_INFO("[Preset] imported: " + destFile.getFullPathName());
        refreshCache();
        applyPresetData(*loaded);             // applies settings + updates UI (with old preset ID)
        currentPresetId = loaded->name;
        updateUiAfterCommit();                // re-update so combo shows the newly imported preset selected
    }
}

} // namespace devpiano::layout
