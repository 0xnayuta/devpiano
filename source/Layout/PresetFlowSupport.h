#pragma once

#include <juce_core/juce_core.h>
#include <memory>
#include <vector>

#include "Layout/PerformancePreset.h"

class MainComponent;

namespace devpiano::layout {

class PresetFlowSupport final {
public:
    explicit PresetFlowSupport(MainComponent& owner);
    ~PresetFlowSupport();

    // ---- Apply ----

    void applyPresetById(const juce::String& presetId);
    void applyPresetByIndex(int zeroBasedIndex);
    void applyPresetData(const PerformancePreset& preset);

    // ---- CRUD ----

    void handleSaveAsNewPreset();
    void handleRenamePreset();
    void handleDeletePreset();
    void handleImportPresetFile(const juce::File& file);

    // ---- UI data ----

    [[nodiscard]] juce::StringArray getPresetIds() const;
    [[nodiscard]] juce::StringArray getPresetDisplayNames() const;
    [[nodiscard]] juce::String getCurrentPresetId() const;
    [[nodiscard]] int getPresetCount() const;

    [[nodiscard]] PerformancePreset captureCurrentState(const juce::String& name) const;
    /// Auto-saves the currently active preset (if any) with the latest state.
    bool autoSaveCurrentPreset();

private:
    void refreshCache(bool force = false);
    void commitPreset(const PerformancePreset& preset);
    void updateUiAfterCommit();
    /// Save the current live state as a preset to `file` and refresh the UI.
    void savePresetFromCurrentState(const juce::String& name, const juce::File& file);

    MainComponent& owner;
    std::vector<PerformancePreset> cachedPresets;
    juce::File lastScannedDir;
    juce::Time lastDirModificationTime;
    juce::String currentPresetId;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetFlowSupport)
};

} // namespace devpiano::layout
