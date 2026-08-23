#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
namespace devpiano::diagnostics {

//! Returns a human-readable description for a MIDI message.
//! Handles: note on/off, control change, pitch bend, program change,
//! channel pressure, and provides a fallback for unknown types.
juce::String describeMidiMessage(const juce::MidiMessage& message);

} // namespace devpiano::diagnostics