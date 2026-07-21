#pragma once

#include <JuceHeader.h>

#include "Core/ChannelMatrix.h"

namespace devpiano::midi {

// ============================================================================
// Matrix-aware MIDI routing service.
//
// Applies the 16-channel ChannelMatrix to note-on/off events and raw MIDI
// messages.  When the matrix is inactive (`ChannelMatrix::active == false`)
// all operations pass through unchanged, preserving backward compatibility.
//
// Used by all two input paths:
//   1. Computer keyboard input (via KeyboardMidiMapper)
//   2. Piano-UI mouse clicks   (via CustomKeyboard callbacks)
// ============================================================================
class MidiChannelMapper {
public:
    explicit MidiChannelMapper(const ChannelMatrix& matrixRef, const bool& midiTransposeRef,
                               const int& keySignatureRef);

    // Transform a single MIDI message through the matrix.
    // Selects PerChannelConfig based on the message's original MIDI channel.
    // Non-note messages (CC, pitch wheel, etc.) pass through unchanged.
    [[nodiscard]] juce::MidiMessage applyTransform(const juce::MidiMessage& message);

    // Convenience: apply matrix and send to keyboardState.
    // When matrix is inactive, forwards with original channel/note/velocity.
    void sendNoteOn(int inputChannel, int midiNote, float velocity, juce::MidiKeyboardState& keyboardState);
    void sendNoteOff(int inputChannel, int midiNote, float velocity, juce::MidiKeyboardState& keyboardState);

private:
    [[nodiscard]] const PerChannelConfig& configForChannel(int inputChannel) const;
    const ChannelMatrix& matrix;
    const bool& midiTranspose;
    const int& keySignature;
};

} // namespace devpiano::midi
