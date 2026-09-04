#include "UI/PresetDialogs.h"

#include "UI/jive/JiveModalDialog.h"

// ============================================================================
// PresetNameDialog & PresetConfirmDialog Implementation
// Declarative dialog wrappers backed by JiveModalDialog templates.
// ============================================================================

void PresetNameDialog::launch(const juce::String& title, const juce::String& initialName,
                              juce::Component* componentToCentreAround,
                              const std::function<void(std::optional<juce::String>)>& onComplete) {
    devpiano::ui::jive::JiveModalDialog::launchSingleInput({
        .title = title,
        .labelText = TRANS("Preset Name:"),
        .initialValue = initialName,
        .componentToCentreAround = componentToCentreAround,
        .onComplete = onComplete,
    });
}

void PresetConfirmDialog::show(const juce::String& title, const juce::String& message, const juce::String& okLabel,
                               const juce::String& cancelLabel, juce::Component* componentToCentreAround,
                               const std::function<void(bool)>& onComplete) {
    devpiano::ui::jive::JiveModalDialog::launchConfirm({
        .title = title,
        .message = message,
        .okLabel = okLabel,
        .cancelLabel = cancelLabel,
        .componentToCentreAround = componentToCentreAround,
        .onComplete = onComplete,
    });
}
