#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "ChannelMatrix.h"
#include "Core/MidiTypes.h"

namespace devpiano::midi {

// ============================================================================
// Matrix-aware MIDI routing service.
//
// Routes NoteOn events through the 16-channel ChannelMatrix. NoteOff events use
// a captured output identity, and raw MIDI messages are transformed explicitly
// with applyTransform.
// ============================================================================

class MidiChannelMapper {
public:
    explicit MidiChannelMapper(const ChannelMatrix& matrixVal, bool midiTransposeVal, int keySignatureVal);

    // Transform a single MIDI message through the matrix.
    // Selects PerChannelConfig based on the message's original MIDI channel.
    // Non-note messages (CC, pitch wheel, etc.) pass through unchanged.
    [[nodiscard]] juce::MidiMessage applyTransform(const juce::MidiMessage& message);

    [[nodiscard]] devpiano::core::MidiNoteIdentity sendNoteOn(int inputChannel, int midiNote, float velocity,
                                                              juce::MidiKeyboardState& keyboardState);
    void sendNoteOff(const devpiano::core::MidiNoteIdentity& identity, float velocity,
                     juce::MidiKeyboardState& keyboardState);

private:
    [[nodiscard]] const PerChannelConfig& configForChannel(int inputChannel) const;
    ChannelMatrix matrix;
    bool midiTranspose = false;
    int keySignature = 0;
};

} // namespace devpiano::midi
