#pragma once

#include <JuceHeader.h>

#include <cctype>
#include <vector>

#include "Core/MidiTypes.h"

namespace devpiano::core
{
enum class KeyActionType
{
    note,
};

enum class KeyTrigger
{
    keyDown,
    keyUp,
};

struct KeyAction
{
    KeyActionType type = KeyActionType::note;
    KeyTrigger trigger = KeyTrigger::keyDown;

    // 保持当前裸字段以兼容现有序列化与调用路径。
    // 后续模块可优先通过下方强类型 helper 访问这些值。
    int midiNote = 60;
    int midiChannel = 1;
    float velocity = 1.0f;

    [[nodiscard]] MidiNoteNumber getMidiNoteNumber() const noexcept
    {
        return MidiNoteNumber::fromClamped(midiNote);
    }

    [[nodiscard]] MidiChannel getMidiChannel() const noexcept
    {
        return MidiChannel::fromClamped(midiChannel);
    }

    [[nodiscard]] Velocity getVelocity() const noexcept
    {
        return Velocity::fromClamped(velocity);
    }

    void setMidiNoteNumber(MidiNoteNumber note) noexcept
    {
        midiNote = note.value;
    }

    void setMidiChannel(MidiChannel channel) noexcept
    {
        midiChannel = channel.value;
    }

    void setVelocity(Velocity newVelocity) noexcept
    {
        velocity = newVelocity.value;
    }
};

struct KeyBinding
{
    int keyCode = 0;
    juce::String displayText;
    KeyAction action;
};

struct KeyboardLayout
{
    juce::String id { "default.freepiano.minimal" };
    juce::String name { "Default FreePiano Minimal" };
    std::vector<KeyBinding> bindings;

    [[nodiscard]] const KeyBinding* findByKeyCode(int keyCodeToFind) const noexcept
    {
        for (const auto& binding : bindings)
            if (binding.keyCode == keyCodeToFind)
                return &binding;

        return nullptr;
    }
};

[[nodiscard]] inline int normaliseAlphaNumericKeyCode(int keyCode)
{
    if (! std::isalnum(static_cast<unsigned char>(keyCode)))
        return 0;

    return juce::KeyPress(std::toupper(static_cast<unsigned char>(keyCode)), 0, 0).getKeyCode();
}

[[nodiscard]] inline int makeAlphaNumericKeyCode(char character)
{
    return normaliseAlphaNumericKeyCode(static_cast<unsigned char>(character));
}

[[nodiscard]] inline KeyBinding makeNoteBinding(char character,
                                                int midiNote,
                                                int midiChannel = 1,
                                                float velocity = 1.0f,
                                                KeyTrigger trigger = KeyTrigger::keyDown)
{
    KeyBinding binding;
    binding.keyCode = makeAlphaNumericKeyCode(character);
    binding.displayText = juce::String::charToString(character);
    binding.action.type = KeyActionType::note;
    binding.action.trigger = trigger;
    binding.action.setMidiNoteNumber(MidiNoteNumber::fromClamped(midiNote));
    binding.action.setMidiChannel(MidiChannel::fromClamped(midiChannel));
    binding.action.setVelocity(Velocity::fromClamped(velocity));
    return binding;
}

[[nodiscard]] inline KeyBinding makeNoteBinding(char character,
                                                MidiNoteNumber midiNote,
                                                MidiChannel midiChannel = MidiChannel::fromClamped(1),
                                                Velocity velocity = Velocity::fromClamped(1.0f),
                                                KeyTrigger trigger = KeyTrigger::keyDown)
{
    KeyBinding binding;
    binding.keyCode = makeAlphaNumericKeyCode(character);
    binding.displayText = juce::String::charToString(character);
    binding.action.type = KeyActionType::note;
    binding.action.trigger = trigger;
    binding.action.setMidiNoteNumber(midiNote);
    binding.action.setMidiChannel(midiChannel);
    binding.action.setVelocity(velocity);
    return binding;
}

[[nodiscard]] inline KeyboardLayout makeDefaultKeyboardLayout()
{
    constexpr int baseC123Row = 84;
    constexpr int baseCQweRow = 72;
    constexpr int baseCAsdRow = 60;
    constexpr int baseCZxcRow = 48;

    KeyboardLayout layout;
    auto& bindings = layout.bindings;
    bindings.reserve(36);

    const auto c5 = baseC123Row;
    bindings.push_back(makeNoteBinding('1', c5 + 0));
    bindings.push_back(makeNoteBinding('2', c5 + 2));
    bindings.push_back(makeNoteBinding('3', c5 + 4));
    bindings.push_back(makeNoteBinding('4', c5 + 5));
    bindings.push_back(makeNoteBinding('5', c5 + 7));
    bindings.push_back(makeNoteBinding('6', c5 + 9));
    bindings.push_back(makeNoteBinding('7', c5 + 11));
    bindings.push_back(makeNoteBinding('8', c5 + 12));
    bindings.push_back(makeNoteBinding('9', c5 + 14));
    bindings.push_back(makeNoteBinding('0', c5 + 16));

    const auto c4 = baseCQweRow;
    bindings.push_back(makeNoteBinding('Q', c4 + 0));
    bindings.push_back(makeNoteBinding('W', c4 + 2));
    bindings.push_back(makeNoteBinding('E', c4 + 4));
    bindings.push_back(makeNoteBinding('R', c4 + 5));
    bindings.push_back(makeNoteBinding('T', c4 + 7));
    bindings.push_back(makeNoteBinding('Y', c4 + 9));
    bindings.push_back(makeNoteBinding('U', c4 + 11));
    bindings.push_back(makeNoteBinding('I', c5 + 0));
    bindings.push_back(makeNoteBinding('O', c5 + 2));
    bindings.push_back(makeNoteBinding('P', c5 + 4));

    const auto c3 = baseCAsdRow;
    bindings.push_back(makeNoteBinding('A', c3 + 0));
    bindings.push_back(makeNoteBinding('S', c3 + 2));
    bindings.push_back(makeNoteBinding('D', c3 + 4));
    bindings.push_back(makeNoteBinding('F', c3 + 5));
    bindings.push_back(makeNoteBinding('G', c3 + 7));
    bindings.push_back(makeNoteBinding('H', c3 + 9));
    bindings.push_back(makeNoteBinding('J', c3 + 11));
    bindings.push_back(makeNoteBinding('K', c4 + 0));
    bindings.push_back(makeNoteBinding('L', c4 + 2));

    const auto c2 = baseCZxcRow;
    bindings.push_back(makeNoteBinding('Z', c2 + 0));
    bindings.push_back(makeNoteBinding('X', c2 + 2));
    bindings.push_back(makeNoteBinding('C', c2 + 4));
    bindings.push_back(makeNoteBinding('V', c2 + 5));
    bindings.push_back(makeNoteBinding('B', c2 + 7));
    bindings.push_back(makeNoteBinding('N', c2 + 9));
    bindings.push_back(makeNoteBinding('M', c2 + 11));

    return layout;
}

[[nodiscard]] inline KeyboardLayout makeFullPianoLayout()
{
    // A wider range layout for demonstration
    constexpr int baseC123Row = 96; // C6
    constexpr int baseCQweRow = 84; // C5
    constexpr int baseCAsdRow = 72; // C4
    constexpr int baseCZxcRow = 60; // C3

    KeyboardLayout layout;
    layout.id = "default.freepiano.full";
    layout.name = "Full Piano Layout";
    auto& bindings = layout.bindings;
    bindings.reserve(36);

    const auto c5 = baseC123Row;
    bindings.push_back(makeNoteBinding('1', c5 + 0));
    bindings.push_back(makeNoteBinding('2', c5 + 2));
    bindings.push_back(makeNoteBinding('3', c5 + 4));
    bindings.push_back(makeNoteBinding('4', c5 + 5));
    bindings.push_back(makeNoteBinding('5', c5 + 7));
    bindings.push_back(makeNoteBinding('6', c5 + 9));
    bindings.push_back(makeNoteBinding('7', c5 + 11));
    bindings.push_back(makeNoteBinding('8', c5 + 12));
    bindings.push_back(makeNoteBinding('9', c5 + 14));
    bindings.push_back(makeNoteBinding('0', c5 + 16));

    const auto c4 = baseCQweRow;
    bindings.push_back(makeNoteBinding('Q', c4 + 0));
    bindings.push_back(makeNoteBinding('W', c4 + 2));
    bindings.push_back(makeNoteBinding('E', c4 + 4));
    bindings.push_back(makeNoteBinding('R', c4 + 5));
    bindings.push_back(makeNoteBinding('T', c4 + 7));
    bindings.push_back(makeNoteBinding('Y', c4 + 9));
    bindings.push_back(makeNoteBinding('U', c4 + 11));
    bindings.push_back(makeNoteBinding('I', c5 + 0));
    bindings.push_back(makeNoteBinding('O', c5 + 2));
    bindings.push_back(makeNoteBinding('P', c5 + 4));

    const auto c3 = baseCAsdRow;
    bindings.push_back(makeNoteBinding('A', c3 + 0));
    bindings.push_back(makeNoteBinding('S', c3 + 2));
    bindings.push_back(makeNoteBinding('D', c3 + 4));
    bindings.push_back(makeNoteBinding('F', c3 + 5));
    bindings.push_back(makeNoteBinding('G', c3 + 7));
    bindings.push_back(makeNoteBinding('H', c3 + 9));
    bindings.push_back(makeNoteBinding('J', c3 + 11));
    bindings.push_back(makeNoteBinding('K', c4 + 0));
    bindings.push_back(makeNoteBinding('L', c4 + 2));

    const auto c2 = baseCZxcRow;
    bindings.push_back(makeNoteBinding('Z', c2 + 0));
    bindings.push_back(makeNoteBinding('X', c2 + 2));
    bindings.push_back(makeNoteBinding('C', c2 + 4));
    bindings.push_back(makeNoteBinding('V', c2 + 5));
    bindings.push_back(makeNoteBinding('B', c2 + 7));
    bindings.push_back(makeNoteBinding('N', c2 + 9));
    bindings.push_back(makeNoteBinding('M', c2 + 11));

    return layout;
}

}