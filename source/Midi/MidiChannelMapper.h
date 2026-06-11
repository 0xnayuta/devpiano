#pragma once

#include <JuceHeader.h>

#include "Core/ChannelMatrix.h"

// ============================================================================
// Per-channel MIDI matrix mapper.
//
// Applies the 16-channel configuration matrix to note-on/off messages
// before they reach the synth.  Entry points:
//
//  - mapNoteOn / mapNoteOff: transform a single note message.
//  - applyTransform: transform a full juce::MidiMessage (note-on/off only;
//    all other message types pass through unchanged).
//
// When the matrix is inactive (ChannelMatrix::active == false), all
// messages pass through unchanged.
// ============================================================================
class MidiChannelMapper {
public:
    explicit MidiChannelMapper(const devpiano::midi::ChannelMatrix& matrix);

    // Transform a note-on (returns transformed message).
    // `sourceChannel` is the 0-based channel the message originated on.
    [[nodiscard]] juce::MidiMessage mapNoteOn(int sourceChannel, int midiNote, float velocity) const;

    // Transform a note-off.
    [[nodiscard]] juce::MidiMessage mapNoteOff(int sourceChannel, int midiNote, float velocity) const;

    // Apply the matrix transform to a full MIDI message.
    // Non-note messages pass through unchanged.
    [[nodiscard]] juce::MidiMessage applyTransform(const juce::MidiMessage& msg) const;

private:
    const devpiano::midi::ChannelMatrix& matrix;

    [[nodiscard]] const devpiano::midi::PerChannelConfig& configFor(int sourceChannel) const;

    JUCE_DECLARE_NON_COPYABLE(MidiChannelMapper)
};
