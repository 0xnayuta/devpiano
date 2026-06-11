#include "Midi/MidiChannelMapper.h"

MidiChannelMapper::MidiChannelMapper(const devpiano::midi::ChannelMatrix& m)
    : matrix(m) {
}

const devpiano::midi::PerChannelConfig& MidiChannelMapper::configFor(int sourceChannel) const {
    auto idx = static_cast<std::size_t>(juce::jlimit(0, 15, sourceChannel));
    return matrix.channels[idx];
}

juce::MidiMessage MidiChannelMapper::mapNoteOn(int sourceChannel, int midiNote, float velocity) const {
    if (!matrix.active)
        return juce::MidiMessage::noteOn(sourceChannel + 1, midiNote, velocity);

    return devpiano::midi::applyMatrixToNoteOn(configFor(sourceChannel), sourceChannel, midiNote, velocity);
}

juce::MidiMessage MidiChannelMapper::mapNoteOff(int sourceChannel, int midiNote, float velocity) const {
    if (!matrix.active)
        return juce::MidiMessage::noteOff(sourceChannel + 1, midiNote, velocity);

    return devpiano::midi::applyMatrixToNoteOff(configFor(sourceChannel), sourceChannel, midiNote, velocity);
}

std::function<void(const juce::MidiMessage&)>
MidiChannelMapper::installRouterCallback(std::function<void(const juce::MidiMessage&)> userCb) const {
    if (!matrix.active || userCb == nullptr)
        return userCb;

    return [this, cb = std::move(userCb)](const juce::MidiMessage& msg) {
        auto copy = msg;

        if (copy.isNoteOn()) {
            auto ch = copy.getChannel() - 1; // 1-based → 0-based
            copy = mapNoteOn(ch, copy.getNoteNumber(), copy.getFloatVelocity());
        } else if (copy.isNoteOff()) {
            auto ch = copy.getChannel() - 1;
            copy = mapNoteOff(ch, copy.getNoteNumber(), copy.getFloatVelocity());
        }

        cb(copy);
    };
}
