#include "Midi/MidiChannelMapper.h"

namespace devpiano::midi {

MidiChannelMapper::MidiChannelMapper(const ChannelMatrix& matrixRef, const bool& midiTransposeRef,
                                     const int& keySignatureRef)
    : matrix(matrixRef)
    , midiTranspose(midiTransposeRef)
    , keySignature(keySignatureRef) {
}

const PerChannelConfig& MidiChannelMapper::configForChannel(int inputChannel) const {
    const auto idx = static_cast<size_t>(juce::jlimit(0, 15, inputChannel));
    return matrix.channels[idx];
}

juce::MidiMessage MidiChannelMapper::applyTransform(const juce::MidiMessage& message) {
    if (!message.isNoteOnOrOff()) {
        return message;
    }

    // inputChannel here is the message's original MIDI channel (0-based).
    const auto inputChannel = message.getChannel() - 1;
    const auto& cfg = configForChannel(inputChannel);

    auto transformed = message.isNoteOn()
        ? applyMatrixToNoteOn(cfg, message.getNoteNumber(), message.getFloatVelocity())
        : applyMatrixToNoteOff(cfg, message.getNoteNumber(), message.getFloatVelocity());

    if (cfg.followKey && midiTranspose) {
        auto fn = juce::jlimit(0, 127, transformed.getNoteNumber() + keySignature);
        transformed = message.isNoteOn()
            ? juce::MidiMessage::noteOn(transformed.getChannel(), fn, transformed.getFloatVelocity())
            : juce::MidiMessage::noteOff(transformed.getChannel(), fn, transformed.getFloatVelocity());
    }
    return transformed;
}

void MidiChannelMapper::sendNoteOn(int inputChannel, int midiNote, float velocity,
                                   juce::MidiKeyboardState& keyboardState) {
    const auto& cfg = configForChannel(inputChannel);
    auto transformed = applyMatrixToNoteOn(cfg, midiNote, velocity);
    if (cfg.followKey && midiTranspose) {
        auto fn = juce::jlimit(0, 127, transformed.getNoteNumber() + keySignature);
        transformed = juce::MidiMessage::noteOn(transformed.getChannel(), fn, transformed.getFloatVelocity());
    }
    keyboardState.noteOn(transformed.getChannel(), transformed.getNoteNumber(), transformed.getFloatVelocity());
}

void MidiChannelMapper::sendNoteOff(int inputChannel, int midiNote, float velocity,
                                    juce::MidiKeyboardState& keyboardState) {
    const auto& cfg = configForChannel(inputChannel);
    auto transformed = applyMatrixToNoteOff(cfg, midiNote, velocity);
    if (cfg.followKey && midiTranspose) {
        auto fn = juce::jlimit(0, 127, transformed.getNoteNumber() + keySignature);
        transformed = juce::MidiMessage::noteOff(transformed.getChannel(), fn, transformed.getFloatVelocity());
    }
    keyboardState.noteOff(transformed.getChannel(), transformed.getNoteNumber(), transformed.getFloatVelocity());
}

} // namespace devpiano::midi
