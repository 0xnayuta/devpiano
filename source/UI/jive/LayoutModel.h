#pragma once

#include <JuceHeader.h>

namespace devpiano::ui::jive {

/// ValueTree factories for the application layout.
///
/// Every node carries explicit sizing: JIVE components (Text especially)
/// report no intrinsic size to JUCE's FlexBox, so height must be stated.
///
/// Styles are NOT set here — StyleCatalog::applyToTree() merges the global
/// style_sheets.json rules into each node before interpretation.

/// Top bar: app title + settings button.
[[nodiscard]] juce::ValueTree makeHeaderTree();

/// Bottom status bar: MIDI activity dot, plugin name, audio info, time.
[[nodiscard]] juce::ValueTree makeStatusBarTree();

/// Plugin panel: selector toolbar + expandable scan/path/list area.
/// Collapsed height 40, expanded 160 (MainComponent positions it).
[[nodiscard]] juce::ValueTree makePluginPanelTree();

/// Controls panel: knob row, ADSR curve, preset row, transport row.
/// Full-width horizontal strip (MainComponent positions it).
[[nodiscard]] juce::ValueTree makeControlsPanelTree();

/// Keyboard area: CustomKeyboard inside a scrolling viewport.
/// Fills the remaining vertical space (MainComponent positions it).
[[nodiscard]] juce::ValueTree makeKeyboardAreaTree();

} // namespace devpiano::ui::jive
