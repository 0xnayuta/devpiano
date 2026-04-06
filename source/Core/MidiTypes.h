#pragma once

#include <JuceHeader.h>

#include <algorithm>

namespace devpiano::core
{
struct MidiNoteNumber
{
    int value = 60;

    [[nodiscard]] static constexpr int minValue() noexcept { return 0; }
    [[nodiscard]] static constexpr int maxValue() noexcept { return 127; }

    [[nodiscard]] static constexpr MidiNoteNumber fromClamped(int note) noexcept
    {
        return { std::clamp(note, minValue(), maxValue()) };
    }

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return value >= minValue() && value <= maxValue();
    }
};

struct MidiChannel
{
    int value = 1;

    [[nodiscard]] static constexpr int minValue() noexcept { return 1; }
    [[nodiscard]] static constexpr int maxValue() noexcept { return 16; }

    [[nodiscard]] static constexpr MidiChannel fromClamped(int channel) noexcept
    {
        return { std::clamp(channel, minValue(), maxValue()) };
    }

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return value >= minValue() && value <= maxValue();
    }

    [[nodiscard]] constexpr int toZeroBased() const noexcept
    {
        return value - 1;
    }
};

struct Velocity
{
    float value = 1.0f;

    [[nodiscard]] static constexpr float minValue() noexcept { return 0.0f; }
    [[nodiscard]] static constexpr float maxValue() noexcept { return 1.0f; }

    [[nodiscard]] static constexpr Velocity fromClamped(float velocity) noexcept
    {
        return { std::clamp(velocity, minValue(), maxValue()) };
    }

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return value >= minValue() && value <= maxValue();
    }

    [[nodiscard]] inline juce::uint8 toMidiByte() const noexcept
    {
        return static_cast<juce::uint8>(std::clamp(value, 0.0f, 1.0f) * 127.0f);
    }
};

struct NoteRange
{
    MidiNoteNumber low = MidiNoteNumber::fromClamped(0);
    MidiNoteNumber high = MidiNoteNumber::fromClamped(127);

    [[nodiscard]] constexpr bool contains(MidiNoteNumber note) const noexcept
    {
        return note.value >= low.value && note.value <= high.value;
    }

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return low.isValid() && high.isValid() && low.value <= high.value;
    }
};
}
