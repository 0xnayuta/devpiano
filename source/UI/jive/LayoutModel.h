#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <jive_layouts/jive_layouts.h>

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

/// Full application layout: header, plugin panel, controls + keyboard
/// content area, status bar. One tree, laid out entirely by JIVE FlexBox.
/// The controls/keyboard split uses flex-grow (1 : 1.5) with min/max
/// heights, reproducing the native dynamic allocation.
[[nodiscard]] juce::ValueTree makeRootLayout();

/// Re-apply the static semantic titles (id -> TRANS key) to a live
/// interpreted tree. Runtime language switching calls this so every node
/// whose title is evaluated once from TRANS() at build time (containers,
/// editors, labels, icon buttons, combo boxes, knobs) follows the locale;
/// text-button titles are refreshed by the accessors alongside their text.
void refreshTitles(::jive::GuiItem& root);

} // namespace devpiano::ui::jive
