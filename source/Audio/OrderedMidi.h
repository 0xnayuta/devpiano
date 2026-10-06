#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <juce_audio_basics/juce_audio_basics.h>

namespace devpiano::audio {

inline bool appendOrderedMidi(juce::MidiBuffer& buffer, const void* data, int size, int sample) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    if (size < 1 || size > 3 || bytes[0] < 0x80 || bytes[0] >= 0xf0) {
        return buffer.addEvent(data, size, sample);
    }
    static_assert(sizeof(juce::int32) == 4 && sizeof(juce::uint16) == 2);
    static_assert(JUCE_MAJOR_VERSION == 9);
    std::array<std::uint8_t, 9> packed;
    const auto position = static_cast<juce::int32>(sample);
    const auto length = static_cast<juce::uint16>(size);
    std::memcpy(packed.data(), &position, sizeof(position));
    std::memcpy(packed.data() + sizeof(position), &length, sizeof(length));
    std::memcpy(packed.data() + sizeof(position) + sizeof(length), data, static_cast<std::size_t>(size));
    buffer.data.addArray(packed.data(), size + 6);
    return true;
}

inline bool appendOrderedMidi(juce::MidiBuffer& buffer, const juce::MidiMessage& message, int sample) {
    return appendOrderedMidi(buffer, message.getRawData(), message.getRawDataSize(), sample);
}

} // namespace devpiano::audio
