#include "UI/CustomKeyboard.h"

#include "Diagnostics/Log.h"
#include "Layout/PresetFlowSupport.h"
#include "MainComponent.h"
#include "Recording/RecordedPreset.h"
#include "UI/jive/JiveModalDialog.h"

namespace devpiano::layout {

PresetFlowSupport::PresetFlowSupport(MainComponent& ownerIn)
    : owner(ownerIn) {
    refreshCache();
}

PresetFlowSupport::~PresetFlowSupport() = default;

// ---- Cache management ----

void PresetFlowSupport::refreshCache(bool force) {
    const auto dir = getPresetDirectory();
    if (!dir.exists()) {
        cachedPresets.clear();
        lastDirModificationTime = {};
        lastScannedDir = dir;
        return;
    }

    const auto currentModTime = dir.getLastModificationTime();
    if (!force && lastScannedDir == dir && currentModTime == lastDirModificationTime && !cachedPresets.empty()) {
        // Directory modification time has not changed — reuse in-memory cache (PERF-005)
        return;
    }

    cachedPresets = scanPresetDirectory(dir);
    lastDirModificationTime = currentModTime;
    lastScannedDir = dir;
}
// ---- UI data ----

juce::StringArray PresetFlowSupport::getPresetIds() const {
    juce::StringArray ids;
    for (const auto& p : cachedPresets) {
        ids.add(p.uuid);
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

bool PresetFlowSupport::applyPresetById(const juce::String& presetId) {
    auto id = presetId.trim();
    if (id.isEmpty()) {
        return false;
    }

    refreshCache();

    if (std::ranges::count_if(cachedPresets, [&id](const auto& p) { return p.uuid == id; }) > 1) {
        DP_LOG_WARN("[Preset] ambiguous permanent identity rejected: " + id);
        return false;
    }
    // 1. Direct match by permanent UUID identity
    for (const auto& p : cachedPresets) {
        if (p.uuid == id) {
            applyPresetData(p, true);
            return true;
        }
    }

    // 2. Legacy name migration: if not matched by UUID, check unique preset name
    const PerformancePreset* uniqueNameMatch = nullptr;
    int matchCount = 0;
    for (const auto& p : cachedPresets) {
        if (p.name == id) {
            uniqueNameMatch = &p;
            ++matchCount;
        }
    }

    if (matchCount == 1 && uniqueNameMatch != nullptr) {
        DP_LOG_INFO("[Preset] Migrated legacy preset name '" + id + "' to permanent UUID " + uniqueNameMatch->uuid);
        return applyPresetById(uniqueNameMatch->uuid);
    }

    if (matchCount > 1) {
        DP_LOG_WARN("[Preset] Ambiguous legacy preset name '" + id + "': " + juce::String(matchCount)
                    + " presets share this name. Migration rejected without silent redirect.");
        return false;
    }

    DP_LOG_WARN("[Preset] preset not found: " + id);
    return false;
}

void PresetFlowSupport::applyPresetByIndex(int index) {
    refreshCache();
    if (index < 0 || static_cast<std::size_t>(index) >= cachedPresets.size()) {
        return;
    }

    applyPresetData(cachedPresets[static_cast<std::size_t>(index)], true);
}

void PresetFlowSupport::applyPresetData(const PerformancePreset& preset, bool fileBacked) {
    commitPreset(preset, fileBacked);

    if (owner.recordingEngine.isRecording()) {
        devpiano::recording::RecordedPreset recordedPreset { preset, owner.audioEngine.captureAcousticSnapshot() };
        recordedPreset.acoustic.unaCorda = preset.unaCorda;
        owner.recordingEngine.recordPresetChange(recordedPreset);
    }

    updateUiAfterCommit();
}

void PresetFlowSupport::applyRecordedPresetUi(const devpiano::recording::RecordedPreset& recordedPreset) {
    const auto& preset = recordedPreset.preset;

    if (preset.uuid.isNotEmpty()) {
        currentPresetId = preset.uuid;
    } else {
        currentPresetId = juce::String();
    }

    // 1. KeyboardLayout
    owner.keyboardMidiMapper.setLayout(preset.layout, false);

    // 2. ChannelMatrix
    auto& s = owner.appSettings;
    s.channelMatrix = preset.channelMatrix;
    s.midiTranspose = recordedPreset.acoustic.transposeEnabled;
    s.keySignature = recordedPreset.acoustic.transposeOffset;
    owner.reconfigureChannelMapper(false);

    // 3. Keyboard display settings
    s.keyboardDisplay.colourMode = preset.colourMode;
    s.keyboardDisplay.noteDisplay = preset.noteDisplay;
    s.keyboardDisplay.fadeSpeed = preset.fadeSpeed;
    s.keyboardDisplay.customKeyLabels = preset.customKeyLabels;
    s.keyboardDisplay.customKeyColours = preset.customKeyColours;

    const auto& acoustic = recordedPreset.acoustic;
    s.masterGain = acoustic.masterGain;
    s.adsrAttack = acoustic.adsr.attack;
    s.adsrDecay = acoustic.adsr.decay;
    s.adsrSustain = acoustic.adsr.sustain;
    s.adsrRelease = acoustic.adsr.release;
    s.builtinTone = static_cast<SettingsModel::BuiltinTone>(acoustic.builtinTone);
    s.pianoBrightness = acoustic.brightness;
    s.pianoHammerHardness = acoustic.hammerHardness;
    s.pianoResonance = acoustic.resonance;
    s.lidPosition = static_cast<SettingsModel::LidPosition>(acoustic.lidPosition);
    s.touchVelocityCurve = preset.touchVelocityCurve;
    owner.keyboardMidiMapper.setTouchVelocityCurve(preset.touchVelocityCurve);
    s.unaCorda = acoustic.unaCorda;
    owner.keyboardMidiMapper.setSoftPedalDown(acoustic.unaCorda, false);
    s.temperament = acoustic.temperament;
    s.referencePitchA4 = acoustic.referencePitchA4;
    s.soundPerspective = acoustic.soundPerspective;
    s.reverbSpace = acoustic.reverbSpace;
    s.reverbWet = acoustic.reverbWet;
    s.pedalNoiseLevel = acoustic.pedalNoiseLevel;
    s.feltAgeingAmount = acoustic.feltAgeingAmount;

    // 5. UI refresh
    owner.syncUiFromSettings(false);
    owner.getCustomKeyboard().repaint();
}

void PresetFlowSupport::commitPreset(const PerformancePreset& preset, bool fileBacked) {
    auto& s = owner.appSettings;
    currentPresetId = fileBacked ? preset.uuid : juce::String();
    s.lastActivePresetId = currentPresetId;

    // 1. KeyboardLayout
    owner.keyboardMidiMapper.setLayout(preset.layout);

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

    // 4. Acoustics
    s.lidPosition = preset.lidPosition;
    owner.audioEngine.setLidPosition(static_cast<AudioEngine::LidPosition>(preset.lidPosition));
    s.touchVelocityCurve = preset.touchVelocityCurve;
    owner.keyboardMidiMapper.setTouchVelocityCurve(preset.touchVelocityCurve);
    s.unaCorda = preset.unaCorda;
    owner.keyboardMidiMapper.setSoftPedalDown(preset.unaCorda);
    s.temperament = preset.temperament;
    owner.audioEngine.setTemperament(preset.temperament);
    s.referencePitchA4 = devpiano::audio::TemperamentEngine::clampReferencePitch(preset.referencePitchA4);
    owner.audioEngine.setReferencePitchA4(s.referencePitchA4);
    s.soundPerspective = preset.soundPerspective;
    owner.audioEngine.setSoundPerspective(preset.soundPerspective);
    s.reverbSpace = preset.reverbSpace;
    owner.audioEngine.setReverbSpace(preset.reverbSpace);
    s.reverbWet = juce::jlimit(0.0f, 1.0f, preset.reverbWet);
    owner.audioEngine.setReverbWet(s.reverbWet);
    s.pedalNoiseLevel = juce::jlimit(0.0f, 1.0f, preset.pedalNoiseLevel);
    owner.audioEngine.setPedalNoiseLevel(s.pedalNoiseLevel);
    s.feltAgeingAmount = juce::jlimit(0.0f, 1.0f, preset.feltAgeingAmount);
    owner.audioEngine.setFeltAgeingAmount(s.feltAgeingAmount);
}

void PresetFlowSupport::updateUiAfterCommit() {
    refreshCache(true);
    owner.syncUiFromSettings();
    owner.getCustomKeyboard().repaint();
    owner.saveSettingsSoon();
}

// ---- Capture current state as a preset ----

PerformancePreset PresetFlowSupport::captureCurrentState(const juce::String& name, const juce::String& uuid) const {
    PerformancePreset preset;
    preset.uuid = uuid.isNotEmpty() ? uuid : juce::Uuid().toDashedString();
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
    preset.lidPosition = owner.appSettings.lidPosition;
    preset.touchVelocityCurve = owner.appSettings.touchVelocityCurve;
    preset.unaCorda = owner.keyboardMidiMapper.isSoftPedalDown();
    preset.temperament = owner.appSettings.temperament;
    preset.referencePitchA4 = owner.appSettings.referencePitchA4;
    preset.soundPerspective = owner.appSettings.soundPerspective;
    preset.reverbSpace = owner.appSettings.reverbSpace;
    preset.reverbWet = owner.appSettings.reverbWet;
    preset.pedalNoiseLevel = owner.appSettings.pedalNoiseLevel;
    preset.feltAgeingAmount = owner.appSettings.feltAgeingAmount;
    return preset;
}

bool PresetFlowSupport::autoSaveCurrentPreset() {
    if (currentPresetId.isEmpty()) {
        return false;
    }
    refreshCache();
    if (std::ranges::count_if(cachedPresets, [this](const auto& p) { return p.uuid == currentPresetId; }) != 1) {
        DP_LOG_WARN("[Preset] auto-save rejected: missing or ambiguous permanent identity");
        return false;
    }
    auto it = std::ranges::find_if(cachedPresets, [this](const auto& p) { return p.uuid == currentPresetId; });
    if (it == cachedPresets.end()) {
        DP_LOG_WARN("[Preset] auto-save failed: current preset UUID not in cache: " + currentPresetId);
        return false;
    }

    auto updatedPreset = captureCurrentState(it->name, it->uuid);
    auto presetFile = resolvePresetFile(it->name);
    if (!savePreset(updatedPreset, presetFile)) {
        DP_LOG_WARN("[Preset] failed to auto-save after binding edit: " + it->name);
        return false;
    }
    return true;
}

// ---- CRUD ----

void PresetFlowSupport::handleSaveAsNewPreset() {
    devpiano::ui::jive::JiveModalDialog::launchSingleInput({
        .title = TRANS("Save as New Preset"),
        .labelText = TRANS("Preset Name:"),
        .initialValue = {},
        .componentToCentreAround = &owner,
        .onComplete =
            [this](std::optional<juce::String> nameOpt) {
                if (!nameOpt.has_value()) {
                    return;
                }
                auto rawName = nameOpt->trim();
                if (rawName.isEmpty()) {
                    return;
                }

                auto file = resolvePresetFile(rawName);

                if (file.existsAsFile()) {
                    devpiano::ui::jive::JiveModalDialog::launchConfirm({
                        .title = TRANS("Overwrite Preset?"),
                        .message = TRANS("A preset named \"") + rawName
                            + TRANS("\" already exists.\nDo you want to overwrite it?"),
                        .okLabel = TRANS("Overwrite"),
                        .cancelLabel = TRANS("Cancel"),
                        .componentToCentreAround = &owner,
                        .onComplete =
                            [this, rawName, file](bool overwrite) {
                                if (overwrite) {
                                    savePresetFromCurrentState(rawName, file, juce::Uuid().toDashedString());
                                }
                            },
                    });
                    return;
                }
                savePresetFromCurrentState(rawName, file, juce::Uuid().toDashedString());
            },
    });
}

void PresetFlowSupport::savePresetFromCurrentState(const juce::String& name, const juce::File& file,
                                                   const juce::String& uuid) {
    auto preset = captureCurrentState(name, uuid.isNotEmpty() ? uuid : juce::Uuid().toDashedString());

    if (savePreset(preset, file)) {
        DP_LOG_INFO("[Preset] saved: " + file.getFullPathName() + " (UUID: " + preset.uuid + ")");
        refreshCache();
        currentPresetId = preset.uuid;
        owner.appSettings.lastActivePresetId = currentPresetId;
        updateUiAfterCommit();
        owner.showStatusMessage(TRANS("Saved preset: ") + preset.name, 2500);
    } else {
        DP_LOG_ERROR("[Preset] save FAILED: " + file.getFullPathName());
    }
}

void PresetFlowSupport::handleRenamePreset() {
    const auto targetId = owner.getSelectedPresetId();
    if (targetId.isEmpty()) {
        return;
    }

    refreshCache();
    auto it = std::ranges::find_if(cachedPresets, [&targetId](const auto& p) { return p.uuid == targetId; });
    if (it == cachedPresets.end()) {
        return;
    }

    const auto oldName = it->name;
    const auto presetUuid = it->uuid;
    const auto oldFile = resolvePresetFile(oldName);

    devpiano::ui::jive::JiveModalDialog::launchSingleInput({
        .title = TRANS("Rename Preset"),
        .labelText = TRANS("Preset Name:"),
        .initialValue = oldName,
        .componentToCentreAround = &owner,
        .onComplete =
            [this, oldName, presetUuid, oldFile](std::optional<juce::String> nameOpt) {
                if (!nameOpt.has_value()) {
                    return;
                }
                const auto newName = nameOpt->trim();
                if (newName.isEmpty() || newName == oldName) {
                    return;
                }

                const auto newFile = resolvePresetFile(newName);
                const bool isSamePath = oldFile == newFile;

                auto executeRename = [this, oldName, newName, presetUuid](bool allowOverwrite) {
                    const auto result = renamePreset(oldName, newName, allowOverwrite);
                    if (result == PresetRenameResult::success) {
                        currentPresetId = presetUuid;
                        owner.appSettings.lastActivePresetId = currentPresetId;
                        refreshCache(true);
                        updateUiAfterCommit();
                        owner.showStatusMessage(TRANS("Renamed preset to: ") + newName, 2500);
                    } else {
                        DP_LOG_ERROR("[Preset] rename failed: " + oldName + " -> " + newName);
                    }
                };

                if (!isSamePath && newFile.existsAsFile()) {
                    devpiano::ui::jive::JiveModalDialog::launchConfirm({
                        .title = TRANS("Overwrite Preset?"),
                        .message = TRANS("A preset named \"") + newName
                            + TRANS("\" already exists.\nDo you want to overwrite it?"),
                        .okLabel = TRANS("Overwrite"),
                        .cancelLabel = TRANS("Cancel"),
                        .componentToCentreAround = &owner,
                        .onComplete =
                            [executeRename](bool overwrite) {
                                if (overwrite) {
                                    executeRename(true);
                                }
                            },
                    });
                    return;
                }

                executeRename(false);
            },
    });
}

void PresetFlowSupport::handleDeletePreset() {
    const auto targetId = owner.getSelectedPresetId();
    if (targetId.isEmpty()) {
        return;
    }

    refreshCache();
    auto it = std::ranges::find_if(cachedPresets, [&targetId](const auto& p) { return p.uuid == targetId; });
    if (it == cachedPresets.end()) {
        return;
    }
    auto name = it->name;
    auto uuid = it->uuid;
    devpiano::ui::jive::JiveModalDialog::launchConfirm({
        .title = TRANS("Delete Preset"),
        .message = TRANS("Delete preset \"") + name + "\"? " + TRANS("This cannot be undone."),
        .okLabel = TRANS("Delete"),
        .cancelLabel = TRANS("Cancel"),
        .componentToCentreAround = &owner,
        .onComplete =
            [this, name, uuid](bool confirmed) {
                if (!confirmed) {
                    return;
                }
                auto file = resolvePresetFile(name);
                if (file.deleteFile()) {
                    DP_LOG_INFO("[Preset] deleted: " + name + " (UUID: " + uuid + ")");
                } else {
                    DP_LOG_WARN("[Preset] failed to delete preset file: " + file.getFullPathName());
                }

                // If the deleted preset was current, revert to default
                if (currentPresetId == uuid) {
                    applyPresetData(makeDefaultPreset(), false);
                } else {
                    refreshCache(true);
                    updateUiAfterCommit();
                }
                owner.showStatusMessage(TRANS("Deleted preset: ") + name, 2500);
            },
    });
}

void PresetFlowSupport::handleImportPresetFile(const juce::File& file) {
    auto loaded = loadPreset(file);
    if (!loaded.has_value()) {
        DP_LOG_ERROR("[Preset] import FAILED: " + file.getFullPathName());
        return;
    }

    auto destFile = resolvePresetFile(loaded->name);

    auto performImport = [this, preset = *loaded, destFile] {
        if (savePreset(preset, destFile)) {
            DP_LOG_INFO("[Preset] imported: " + destFile.getFullPathName());
            refreshCache();
            applyPresetData(preset, true);
        }
    };

    if (destFile.existsAsFile()) {
        devpiano::ui::jive::JiveModalDialog::launchConfirm({
            .title = TRANS("Overwrite Preset?"),
            .message
            = TRANS("A preset named \"") + loaded->name + TRANS("\" already exists.\nDo you want to overwrite it?"),
            .okLabel = TRANS("Overwrite"),
            .cancelLabel = TRANS("Cancel"),
            .componentToCentreAround = &owner,
            .onComplete =
                [importAction = std::move(performImport)](bool confirmed) {
                    if (confirmed) {
                        importAction();
                    }
                },
        });
    } else {
        performImport();
    }
}

} // namespace devpiano::layout
