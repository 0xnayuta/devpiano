#pragma once

#include "UI/jive/JiveUtils.h"
#include <juce_data_structures/juce_data_structures.h>

namespace devpiano::ui::jive {

// ============================================================================
/// Declarative ValueTree factories for the Settings Window / Panel layout.
///
/// Refactored in Phase 15-C to eliminate 300+ lines of manual setBounds /
/// layoutContent in SettingsComponent, replacing them with JIVE FlexBox
/// and CSS Grid layout declarations.
// ============================================================================

/// Audio device selector section (contains the native AudioDeviceSelector).
[[nodiscard]] juce::ValueTree makeAudioDeviceSectionTree();

/// Key signature & channel follow key section (includes 16-channel CSS Grid).
[[nodiscard]] juce::ValueTree makeKeySignatureSectionTree();

/// Keyboard display & language section.
[[nodiscard]] juce::ValueTree makeKeyboardDisplaySectionTree();

/// Diagnostics log viewer section (contains multi-line ListEditor).
[[nodiscard]] juce::ValueTree makeDiagnosticsSectionTree();

/// Save button row (flex-end right-aligned).
[[nodiscard]] juce::ValueTree makeSaveActionSectionTree();

/// Full Settings Panel layout tree: combines Audio, Key Signature,
/// Keyboard Display, Diagnostics, and Save Action into a scrollable column.
[[nodiscard]] juce::ValueTree makeSettingsLayoutTree();

} // namespace devpiano::ui::jive
