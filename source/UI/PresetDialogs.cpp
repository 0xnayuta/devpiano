#include "UI/PresetDialogs.h"

#include "UI/jive/JiveModalDialog.h"

// ============================================================================
// PresetNameDialog & PresetConfirmDialog Implementation
//
// Refactored in Phase 15-B to use JiveModalDialog declarative templates,
// eliminating redundant DialogContentBase classes and manual pixel calculations.
// ============================================================================

void PresetNameDialog::launch(const juce::String& title, const juce::String& initialName,
                              juce::Component* componentToCentreAround,
                              std::function<void(std::optional<juce::String>)> onComplete) {
    devpiano::ui::jive::JiveModalDialog::launchSingleInput(title, TRANS("Preset Name:"), initialName,
                                                           componentToCentreAround, std::move(onComplete));
}

void PresetConfirmDialog::show(const juce::String& title, const juce::String& message, const juce::String& okLabel,
                               const juce::String& cancelLabel, juce::Component* componentToCentreAround,
                               std::function<void(bool)> onComplete) {
    devpiano::ui::jive::JiveModalDialog::launchConfirm(title, message, okLabel, cancelLabel, componentToCentreAround,
                                                       std::move(onComplete));
}
