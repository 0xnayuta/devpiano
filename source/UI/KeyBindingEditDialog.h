#pragma once

#include <JuceHeader.h>

#include <functional>
#include <optional>

#include "Core/KeyMapTypes.h"

// ============================================================================
// Result returned by KeyBindingEditDialog on close.
// ============================================================================
struct KeyBindingEditResult {
    std::optional<devpiano::core::KeyBinding> binding;
    juce::String customLabel; // empty = no change
    juce::Colour customColour { 0x00000000 }; // transparent = no change
    bool labelChanged = false;
    bool colourChanged = false;
};

// ============================================================================
// Modal dialog for editing a computer-key → MIDI-note binding,
// plus per-key custom label and colour.
//
// Launched by double-clicking a key on the CustomKeyboard.
// Shows the current mapping for that MIDI note, lets the user change
// the target MIDI note, channel, velocity, custom label, and custom colour.
// ============================================================================
class KeyBindingEditDialog {
public:
    KeyBindingEditDialog() = delete;

    static void launch(int midiNote, const juce::String& noteName, const devpiano::core::KeyBinding* existingBinding,
                       const juce::String& currentCustomLabel, const juce::Colour& currentCustomColour,
                       std::function<void(KeyBindingEditResult)> onComplete, juce::Component* parent = nullptr);

private:
    JUCE_DECLARE_NON_COPYABLE(KeyBindingEditDialog)
};
