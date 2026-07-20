#pragma once

#include <JuceHeader.h>

#include <functional>
#include <optional>

#include "Recording/PerformanceFile.h"

// ============================================================================
// Modal dialog for editing song metadata (title + notes).
//
// Launched after recording stops, when opening a .devpiano file,
// or via the "Song Info" button in ControlsPanel.
// ============================================================================
class PerformanceMetadataDialog {
public:
    PerformanceMetadataDialog() = delete;

    // Launch a modal metadata editor.
    // Returns std::nullopt if the user cancels.
    // `componentToCentreAround` is used to position the dialog relative to
    // the main window (typically the ControlsPanel or MainComponent).
    static void launch(const devpiano::recording::PerformanceFileMetadata& initialMetadata,
                       juce::Component* componentToCentreAround,
                       std::function<void(std::optional<devpiano::recording::PerformanceFileMetadata>)> onComplete);

private:
    JUCE_DECLARE_NON_COPYABLE(PerformanceMetadataDialog)
};
