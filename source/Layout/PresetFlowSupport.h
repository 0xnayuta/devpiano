#pragma once

#include <JuceHeader.h>

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

private:
    void refreshCache();
    void commitPreset(const PerformancePreset& preset);
    void updateUiAfterCommit();

    MainComponent& owner;
    std::vector<PerformancePreset> cachedPresets;
    juce::String currentPresetId;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetFlowSupport)
};

} // namespace devpiano::layout
