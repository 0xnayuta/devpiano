#pragma once

#include <JuceHeader.h>

namespace devpiano::ui::jive {

// ============================================================================
// ValueTree factory functions — one per panel, plus the root layout.
//
// Each function returns a pure juce::ValueTree describing the panel's
// Component hierarchy in JIVE's declarative markup language.
// These ValueTrees are interpreted by jive::Interpreter to create the
// actual juce::Component tree at runtime.
//
// All panels are styled via style_sheets.json (colors, fonts, spacings)
// and design_tokens.json (design tokens).
// ============================================================================

[[nodiscard]] juce::ValueTree makeHeaderTree();
[[nodiscard]] juce::ValueTree makeStatusBarTree();
[[nodiscard]] juce::ValueTree makePluginPanelTree();
[[nodiscard]] juce::ValueTree makeControlsPanelTree();
[[nodiscard]] juce::ValueTree makeKeyboardAreaTree();

} // namespace devpiano::ui::jive
