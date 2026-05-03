#include "MidiTrace.h"

#include <array>

namespace
{

constexpr auto noteNames = std::array<const char*, 12>{
    "C",  "C#", "D", "D#", "E", "F",
    "F#", "G",  "G#", "A", "A#", "B"
};

const char* noteNameFromSemitone(int note)
{
    return noteNames[note % 12];
}

int octaveFromMidiNote(int note)
{
    return (note / 12) - 1;
}

} // anonymous namespace

namespace devpiano::diagnostics
{

juce::String describeMidiMessage(const juce::MidiMessage& message)
{
    const auto timestamp = message.getTimeStamp();
    const auto channel = message.getChannel();

    if (message.isNoteOn())
    {
        return "NoteOn ts=" + juce::String(timestamp, 3) +
               " ch=" + juce::String(channel) +
               " note=" + juce::String(message.getNoteNumber()) +
               "(" + juce::String(noteNameFromSemitone(message.getNoteNumber())) +
               juce::String(octaveFromMidiNote(message.getNoteNumber())) + ")" +
               " vel=" + juce::String(static_cast<int>(message.getVelocity() * 127.0f));
    }

    if (message.isNoteOff())
    {
        return "NoteOff ts=" + juce::String(timestamp, 3) +
               " ch=" + juce::String(channel) +
               " note=" + juce::String(message.getNoteNumber()) +
               "(" + juce::String(noteNameFromSemitone(message.getNoteNumber())) +
               juce::String(octaveFromMidiNote(message.getNoteNumber())) + ")";
    }

    if (message.isController())
    {
        return "CC ts=" + juce::String(timestamp, 3) +
               " ch=" + juce::String(channel) +
               " cc=" + juce::String(message.getControllerNumber()) +
               " val=" + juce::String(message.getControllerValue());
    }

    if (message.isPitchWheel())
    {
        return "PitchBend ts=" + juce::String(timestamp, 3) +
               " ch=" + juce::String(channel) +
               " val=" + juce::String(message.getPitchWheelValue());
    }

    if (message.isProgramChange())
    {
        return "ProgramChange ts=" + juce::String(timestamp, 3) +
               " ch=" + juce::String(channel) +
               " prog=" + juce::String(message.getProgramChangeNumber());
    }

    if (message.isChannelPressure())
    {
        return "ChannelPressure ts=" + juce::String(timestamp, 3) +
               " ch=" + juce::String(channel) +
               " pressure=" + juce::String(message.getChannelPressureValue());
    }

    if (message.isAftertouch())
    {
        return "Aftertouch ts=" + juce::String(timestamp, 3) +
               " ch=" + juce::String(channel) +
               " note=" + juce::String(message.getNoteNumber()) +
               " pressure=" + juce::String(message.getAfterTouchValue());
    }

    if (message.isSysEx())
    {
        return "SysEx len=" + juce::String(message.getSysExDataSize());
    }

    if (message.isMetaEvent())
    {
        return "Meta type=" + juce::String(message.getMetaEventType());
    }

    // Fallback for any other message type
    juce::String raw;
    const auto* data = message.getRawData();
    const auto size = message.getRawDataSize();
    for (int i = 0; i < size; ++i)
        raw += juce::String::toHexString(static_cast<unsigned char>(data[i])) + " ";
    return "Unknown [" + raw.trim() + "]";
}

} // namespace devpiano::diagnostics