#pragma once

#include <JuceHeader.h>

#include <functional>

#include "Core/ChannelMatrix.h"

// ============================================================================
// Per-channel MIDI matrix mapper.
//
// Applies the 16-channel configuration matrix to note-on/off messages
// before they reach the synth.  Three entry points:
//
//  - mapNoteOn / mapNoteOff: transform a single note message.
//  - installRouterCallback: wrap a MidiRouter's onMessage callback to
//    apply the matrix to external MIDI input.
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

    // Wrap a MidiRouter callback to apply the matrix to incoming external MIDI.
    // Returns a new callback; the original should be set as the MidiRouter's onMessage.
    [[nodiscard]] std::function<void(const juce::MidiMessage&)>
    installRouterCallback(std::function<void(const juce::MidiMessage&)> originalCallback) const;

private:
    const devpiano::midi::ChannelMatrix& matrix;

    [[nodiscard]] const devpiano::midi::PerChannelConfig& configFor(int sourceChannel) const;

    JUCE_DECLARE_NON_COPYABLE(MidiChannelMapper)
};
