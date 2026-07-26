#pragma once

#include <jive_layouts/jive_layouts.h>

namespace devpiano::ui::jive {

// ============================================================================
// JIVE View subclass representing the main application UI layout.
//
// initialise() returns a ValueTree describing the full Component hierarchy.
// createComponent() handles DrawableButton icons that JIVE's standard
// <Button> (TextButton) cannot express.
// setup() will be used in Phase 11d for callback wiring.
// ============================================================================
class AppView final : public ::jive::View {
public:
    AppView();

protected:
    [[nodiscard]] juce::ValueTree initialise() override;
    [[nodiscard]] std::unique_ptr<juce::Component> createComponent(const juce::ValueTree& tree) override;
    void setup(::jive::GuiItem& item) override;
};

/// Return the root layout ValueTree without requiring an AppView instance.
/// Useful for pre-building or inspecting the layout.
[[nodiscard]] juce::ValueTree makeRootLayout();

} // namespace devpiano::ui::jive
