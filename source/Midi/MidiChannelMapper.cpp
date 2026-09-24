#include "Midi/MidiChannelMapper.h"

namespace devpiano::midi {

MidiChannelMapper::MidiChannelMapper(const ChannelMatrix& matrixVal, bool midiTransposeVal, int keySignatureVal)
    : matrix(matrixVal)
    , midiTranspose(midiTransposeVal)
    , keySignature(keySignatureVal) {
}

const PerChannelConfig& MidiChannelMapper::configForChannel(int inputChannel) const {
    const auto idx = static_cast<size_t>(juce::jlimit(0, 15, inputChannel));
    return matrix.channels[idx];
}

juce::MidiMessage MidiChannelMapper::applyTransform(const juce::MidiMessage& message) {
    if (!message.isNoteOnOrOff()) {
        return message;
    }

    if (!matrix.active) {
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

devpiano::core::MidiNoteIdentity MidiChannelMapper::sendNoteOn(int inputChannel, int midiNote, float velocity,
                                                               juce::MidiKeyboardState& keyboardState) {
    using devpiano::core::MidiChannel;
    using devpiano::core::MidiNoteIdentity;
    using devpiano::core::MidiNoteNumber;

    if (!matrix.active) {
        const MidiNoteIdentity identity { MidiNoteNumber::fromClamped(midiNote),
                                          MidiChannel::fromClamped(inputChannel + 1) };
        keyboardState.noteOn(identity.channel.value, identity.note.value, velocity);
        return identity;
    }

    const auto& cfg = configForChannel(inputChannel);
    auto transformed = applyMatrixToNoteOn(cfg, midiNote, velocity);
    if (cfg.followKey && midiTranspose) {
        const auto fn = juce::jlimit(0, 127, transformed.getNoteNumber() + keySignature);
        transformed = juce::MidiMessage::noteOn(transformed.getChannel(), fn, transformed.getFloatVelocity());
    }

    const MidiNoteIdentity identity { MidiNoteNumber::fromClamped(transformed.getNoteNumber()),
                                      MidiChannel::fromClamped(transformed.getChannel()) };
    keyboardState.noteOn(identity.channel.value, identity.note.value, transformed.getFloatVelocity());
    return identity;
}

void MidiChannelMapper::sendNoteOff(const devpiano::core::MidiNoteIdentity& identity, float velocity,
                                    juce::MidiKeyboardState& keyboardState) {
    keyboardState.noteOff(identity.channel.value, identity.note.value, velocity);
}

} // namespace devpiano::midi
