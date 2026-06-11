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

juce::MidiMessage MidiChannelMapper::applyTransform(const juce::MidiMessage& msg) const {
    if (!matrix.active)
        return msg;

    auto copy = msg;
    auto sourceChannel = copy.getChannel() - 1; // 1-based → 0-based

    if (copy.isNoteOn())
        copy = mapNoteOn(sourceChannel, copy.getNoteNumber(), copy.getFloatVelocity());
    else if (copy.isNoteOff())
        copy = mapNoteOff(sourceChannel, copy.getNoteNumber(), copy.getFloatVelocity());

    return copy;
}
