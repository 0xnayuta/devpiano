#pragma once

#include <JuceHeader.h>

#include <functional>
#include <optional>

#include "Core/KeyMapTypes.h"

// ============================================================================
// Modal dialog for editing a computer-key → MIDI-note binding.
//
// Launched by double-clicking a key on the CustomKeyboard.
// Shows the current mapping for that MIDI note, lets the user change
// the target MIDI note, channel, and velocity.
// ============================================================================
class KeyBindingEditDialog {
public:
    KeyBindingEditDialog() = delete;

    // Opens a modal dialog.
    //
    //  midiNote         — the MIDI note whose key was clicked
    //  noteName         — display name ("C4", "D#5", …)
    //  existingBinding  — the first computer-key binding for this note, or
    //                     nullptr when no key is mapped yet (read-only mode)
    //  onComplete       — called when the dialog closes:
    //                       std::nullopt → cancelled / window closed
    //                       KeyBinding   → user confirmed; caller should update
    //                                      the layout with this binding
    static void launch(int midiNote, const juce::String& noteName, const devpiano::core::KeyBinding* existingBinding,
                       std::function<void(std::optional<devpiano::core::KeyBinding>)> onComplete);

private:
    JUCE_DECLARE_NON_COPYABLE(KeyBindingEditDialog)
};
