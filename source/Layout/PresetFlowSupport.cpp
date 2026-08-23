#include "UI/CustomKeyboard.h"

#include "Layout/PresetFlowSupport.h"

#include "Diagnostics/Log.h"
#include "MainComponent.h"
#include "UI/PresetDialogs.h"

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
    for (const auto& p : cachedPresets) {
        ids.add(p.name);
    }
    return ids;
}

juce::StringArray PresetFlowSupport::getPresetDisplayNames() const {
    juce::StringArray names;
    for (const auto& p : cachedPresets) {
        names.add(p.name.isNotEmpty() ? p.name : "Untitled");
    }
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
    // Defensive copy: applyPresetData → updateUiAfterCommit → setPresets may
    // reallocate the caller's string storage, invalidating the reference.
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization) - intentional defensive copy
    auto id = presetId;

    refreshCache();

    for (const auto& p : cachedPresets) {
        if (p.name == id) {
            // Set currentPresetId BEFORE applyPresetData so that
            // updateUiAfterCommit → setPresets sees the new ID.
            currentPresetId = id;
            applyPresetData(p);
            return;
        }
    }

    DP_LOG_WARN("[Preset] preset not found: " + id);
}

void PresetFlowSupport::applyPresetByIndex(int index) {
    refreshCache();
    if (index < 0 || static_cast<std::size_t>(index) >= cachedPresets.size()) {
        return;
    }

    // Set currentPresetId BEFORE applyPresetData (same race as applyPresetById).
    currentPresetId = cachedPresets[static_cast<std::size_t>(index)].name;
    applyPresetData(cachedPresets[static_cast<std::size_t>(index)]);
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
    owner.setKeyboardLayout(preset.layout);

    // 2. ChannelMatrix
    s.channelMatrix = preset.channelMatrix;
    owner.reconfigureChannelMapper();

    // 3. Keyboard display settings
    // NOTE: keySignature and midiTranspose are live app-level settings managed
    // by the Audio Settings dialog, NOT by presets. They are intentionally NOT
    // overwritten here so that persisted values survive preset loading at startup.
    s.keyboardDisplay.colourMode = preset.colourMode;
    s.keyboardDisplay.noteDisplay = preset.noteDisplay;
    s.keyboardDisplay.fadeSpeed = preset.fadeSpeed;
    s.keyboardDisplay.customKeyLabels = preset.customKeyLabels;
    s.keyboardDisplay.customKeyColours = preset.customKeyColours;

    // 4. Persist preset identity
    s.lastActivePresetId = preset.name;
}

void PresetFlowSupport::updateUiAfterCommit() {
    owner.syncUiFromSettings();
    owner.getCustomKeyboard().repaint();
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
    preset.colourMode = owner.appSettings.keyboardDisplay.colourMode;
    preset.noteDisplay = owner.appSettings.keyboardDisplay.noteDisplay;
    preset.fadeSpeed = owner.appSettings.keyboardDisplay.fadeSpeed;
    preset.previewAlpha = 0.0f;
    preset.customKeyLabels = owner.appSettings.keyboardDisplay.customKeyLabels;
    preset.customKeyColours = owner.appSettings.keyboardDisplay.customKeyColours;
    return preset;
}

// ---- CRUD ----

void PresetFlowSupport::handleSaveAsNewPreset() {
    PresetNameDialog::launch(TRANS("Save as New Preset"), {}, &owner, [this](std::optional<juce::String> nameOpt) {
        if (!nameOpt.has_value()) {
            return;
        }
        auto rawName = nameOpt->trim();
        if (rawName.isEmpty()) {
            return;
        }

        auto fileName = sanitisePresetFileName(rawName);
        auto file = getPresetDirectory().getChildFile(fileName + ".devpiano.preset");

        if (file.existsAsFile()) {
            PresetConfirmDialog::show(
                TRANS("Overwrite Preset?"),
                TRANS("A preset named \"") + rawName + TRANS("\" already exists.\nDo you want to overwrite it?"),
                TRANS("Overwrite"), TRANS("Cancel"), &owner, [this, rawName, file](bool overwrite) {
                    if (overwrite) {
                        savePresetFromCurrentState(rawName, file);
                    }
                });
            return;
        }
        savePresetFromCurrentState(rawName, file);
    });
}

void PresetFlowSupport::savePresetFromCurrentState(const juce::String& name, const juce::File& file) {
    auto preset = captureCurrentState(name);

    if (savePreset(preset, file)) {
        DP_LOG_INFO("[Preset] saved: " + file.getFullPathName());
        refreshCache();
        currentPresetId = preset.name;
        owner.appSettings.lastActivePresetId = currentPresetId;
        updateUiAfterCommit();
        owner.showStatusMessage(TRANS("Saved preset: ") + preset.name, 2500);
    } else {
        DP_LOG_ERROR("[Preset] save FAILED: " + file.getFullPathName());
    }
}

void PresetFlowSupport::handleRenamePreset() {
    // Target the preset currently shown in the combo box. The stored
    // currentPresetId can be out of sync (e.g. right after startup the combo
    // selects the first user preset while currentPresetId still refers to the
    // built-in default), which previously made Rename silently do nothing.
    const auto targetId = owner.getSelectedPresetId();
    if (targetId.isEmpty()) {
        return;
    }

    refreshCache();
    auto it = std::ranges::find_if(cachedPresets, [&targetId](const auto& p) { return p.name == targetId; });
    if (it == cachedPresets.end()) {
        return;
    }

    auto oldName = it->name;
    auto oldFile = getPresetDirectory().getChildFile(sanitisePresetFileName(oldName) + ".devpiano.preset");

    PresetNameDialog::launch(
        TRANS("Rename Preset"), oldName, &owner,
        [this, targetId, oldName, oldFile](std::optional<juce::String> nameOpt) {
            if (!nameOpt.has_value()) {
                return;
            }
            auto newName = nameOpt->trim();
            if (newName.isEmpty() || newName == oldName) {
                return;
            }

            refreshCache();
            auto it2 = std::ranges::find_if(cachedPresets, [&targetId](const auto& p) { return p.name == targetId; });
            if (it2 == cachedPresets.end()) {
                return;
            }

            auto preset = *it2;
            preset.name = newName;
            preset.layout.name = newName;

            auto newFile = getPresetDirectory().getChildFile(sanitisePresetFileName(newName) + ".devpiano.preset");

            if (savePreset(preset, newFile)) {
                if (oldFile.deleteFile()) {
                    DP_LOG_INFO("[Preset] renamed: " + oldName + " -> " + newName);
                } else {
                    DP_LOG_WARN("[Preset] renamed preset saved but old file could not be deleted: "
                                + oldFile.getFullPathName());
                }
                currentPresetId = newName;
                owner.appSettings.lastActivePresetId = currentPresetId;
                refreshCache();
                updateUiAfterCommit();
                owner.showStatusMessage(TRANS("Renamed preset to: ") + newName, 2500);
            }
        });
}

void PresetFlowSupport::handleDeletePreset() {
    // Same as Rename: operate on the combo's current selection so the button
    // works even when currentPresetId is stale after startup.
    const auto targetId = owner.getSelectedPresetId();
    if (targetId.isEmpty()) {
        return;
    }

    refreshCache();
    auto it = std::ranges::find_if(cachedPresets, [&targetId](const auto& p) { return p.name == targetId; });
    if (it == cachedPresets.end()) {
        return;
    }
    auto name = it->name;

    PresetConfirmDialog::show(TRANS("Delete Preset?"), TRANS("Delete preset \"") + name + "\"?", TRANS("Delete"),
                              TRANS("Cancel"), &owner, [this, targetId, name](bool confirmed) {
                                  if (!confirmed) {
                                      return;
                                  }

                                  auto file = getPresetDirectory().getChildFile(sanitisePresetFileName(name)
                                                                                + ".devpiano.preset");
                                  if (file.deleteFile()) {
                                      DP_LOG_INFO("[Preset] deleted: " + name);
                                  } else {
                                      DP_LOG_WARN("[Preset] failed to delete preset file: " + file.getFullPathName());
                                  }

                                  // If the deleted preset was current, revert to default
                                  if (currentPresetId == name) {
                                      applyPresetData(makeDefaultPreset());
                                      currentPresetId.clear();
                                      owner.appSettings.lastActivePresetId.clear();
                                  }
                                  refreshCache();
                                  updateUiAfterCommit();
                                  owner.showStatusMessage(TRANS("Deleted preset: ") + name, 2500);
                              });
}

void PresetFlowSupport::handleImportPresetFile(const juce::File& file) {
    auto loaded = loadPreset(file);
    if (!loaded.has_value()) {
        DP_LOG_ERROR("[Preset] import FAILED: " + file.getFullPathName());
        return;
    }

    auto destFile = getPresetDirectory().getChildFile(sanitisePresetFileName(loaded->name) + ".devpiano.preset");

    auto performImport = [this, preset = *loaded, destFile] {
        if (savePreset(preset, destFile)) {
            DP_LOG_INFO("[Preset] imported: " + destFile.getFullPathName());
            refreshCache();
            applyPresetData(preset); // applies settings + updates UI (with old preset ID)
            currentPresetId = preset.name;
            updateUiAfterCommit(); // re-update so combo shows the newly imported preset selected
        }
    };

    if (destFile.existsAsFile()) {
        PresetConfirmDialog::show(
            TRANS("Overwrite Preset?"),
            TRANS("A preset named \"") + loaded->name + TRANS("\" already exists.\nDo you want to overwrite it?"),
            TRANS("Overwrite"), TRANS("Cancel"), &owner, [importAction = std::move(performImport)](bool confirmed) {
                if (confirmed) {
                    importAction();
                }
            });
    } else {
        performImport();
    }
}

} // namespace devpiano::layout
