#pragma once

#include <functional>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <optional>

// ============================================================================
// Dark, theme-consistent modal dialogs for the preset flows.
//
// PresetFlowSupport previously used juce::AlertWindow for save-as-new /
// rename / delete, which rendered with the light default theme and looked
// nothing like the main UI or the Settings / Song Information windows.
// These helpers launch DialogWindow-based dialogs (same pattern as
// PerformanceMetadataDialog): dark background, the parent's DevPiano
// LookAndFeel, and consistent spacing.
// ============================================================================

class PresetNameDialog {
public:
    PresetNameDialog() = delete;

    /// Modal single-line text input. `onComplete` receives the trimmed name,
    /// or std::nullopt when the user cancels / closes the dialog.
    static void launch(const juce::String& title, const juce::String& initialName,
                       juce::Component* componentToCentreAround,
                       std::function<void(std::optional<juce::String>)> onComplete);

private:
    JUCE_DECLARE_NON_COPYABLE(PresetNameDialog)
};

class PresetConfirmDialog {
public:
    PresetConfirmDialog() = delete;

    /// Modal yes/no confirmation. `onComplete(true)` when the user confirms.
    static void show(const juce::String& title, const juce::String& message, const juce::String& okLabel,
                     const juce::String& cancelLabel, juce::Component* componentToCentreAround,
                     std::function<void(bool)> onComplete);

private:
    JUCE_DECLARE_NON_COPYABLE(PresetConfirmDialog)
};
