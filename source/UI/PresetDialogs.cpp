#include "UI/PresetDialogs.h"

#include "UI/jive/JiveModalDialog.h"

// ============================================================================
// PresetNameDialog & PresetConfirmDialog Implementation
// Declarative dialog wrappers backed by JiveModalDialog templates.
// ============================================================================

void PresetNameDialog::launch(const juce::String& title, const juce::String& initialName,
                              juce::Component* componentToCentreAround,
                              const std::function<void(std::optional<juce::String>)>& onComplete) {
    devpiano::ui::jive::JiveModalDialog::launchSingleInput(title, TRANS("Preset Name:"), initialName,
                                                           componentToCentreAround, onComplete);
}

void PresetConfirmDialog::show(const juce::String& title, const juce::String& message, const juce::String& okLabel,
                               const juce::String& cancelLabel, juce::Component* componentToCentreAround,
                               const std::function<void(bool)>& onComplete) {
    devpiano::ui::jive::JiveModalDialog::launchConfirm(title, message, okLabel, cancelLabel, componentToCentreAround,
                                                       onComplete);
}
