#include "Midi/MidiChannelMapper.h"

namespace devpiano::midi {

MidiChannelMapper::MidiChannelMapper(const ChannelMatrix& matrixRef)
    : matrix(matrixRef) {
}

const PerChannelConfig& MidiChannelMapper::configForChannel(int inputChannel) const {
    const auto idx = static_cast<size_t>(juce::jlimit(0, 15, inputChannel));
    return matrix.channels[idx];
}

juce::MidiMessage MidiChannelMapper::applyTransform(const juce::MidiMessage& message) {
    if (!matrix.active)
        return message;

    if (!message.isNoteOnOrOff())
        return message;

    // inputChannel here is the message's original MIDI channel (0-based).
    const auto inputChannel = message.getChannel() - 1;
    const auto& cfg = configForChannel(inputChannel);

    if (message.isNoteOn())
        return applyMatrixToNoteOn(cfg, inputChannel, message.getNoteNumber(), message.getFloatVelocity());

    return applyMatrixToNoteOff(cfg, inputChannel, message.getNoteNumber(), message.getFloatVelocity());
}

void MidiChannelMapper::sendNoteOn(int inputChannel, int midiNote, float velocity,
                                   juce::MidiKeyboardState& keyboardState) {
    if (!matrix.active) {
        keyboardState.noteOn(inputChannel + 1, midiNote, velocity);
        return;
    }

    const auto& cfg = configForChannel(inputChannel);
    const auto transformed = applyMatrixToNoteOn(cfg, inputChannel, midiNote, velocity);
    keyboardState.noteOn(transformed.getChannel(), transformed.getNoteNumber(), transformed.getFloatVelocity());
}

void MidiChannelMapper::sendNoteOff(int inputChannel, int midiNote, float velocity,
                                    juce::MidiKeyboardState& keyboardState) {
    if (!matrix.active) {
        keyboardState.noteOff(inputChannel + 1, midiNote, velocity);
        return;
    }

    const auto& cfg = configForChannel(inputChannel);
    const auto transformed = applyMatrixToNoteOff(cfg, inputChannel, midiNote, velocity);
    keyboardState.noteOff(transformed.getChannel(), transformed.getNoteNumber(), transformed.getFloatVelocity());
}

} // namespace devpiano::midi
