#pragma once

#include <cctype>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

#include "Core/MidiTypes.h"

namespace devpiano::core {
enum class KeyActionType : std::uint8_t {
    note,
};

enum class KeyTrigger : std::uint8_t {
    keyDown,
    keyUp,
};

enum class SustainPolicy : std::uint8_t {
    normal = 0, // Standard direct sustain pedal: Space down = 127, Space up = 0
    syncPedal = 1, // Syncopated legato pedal: Space up hangs cut; next NoteOn triggers CC64(0)->NoteOn->CC64(127)
};

// ============================================================================
// Performance Modifier State (Phase 34-D: Event-time Transformation Pipeline)
// ============================================================================
struct PerformanceModifierState {
    bool shiftActive = false; // Shift held: transient velocity boost (1.0f / 127)
    bool altActive = false; // Alt held: transient octave shift (+12 semitones)
    bool ctrlActive = false; // Ctrl held: transient transform

    float velocityMultiplier = 1.0f;
    float velocityBoost = 0.0f;
    int8_t octaveOffset = 0;
    int8_t semitoneOffset = 0;

    [[nodiscard]] float transformVelocity(float baseVelocity) const noexcept {
        if (shiftActive) {
            return 1.0f; // Maximum fortissimo accent
        }
        return juce::jlimit(0.0f, 1.0f, (baseVelocity * velocityMultiplier) + velocityBoost);
    }

    [[nodiscard]] int transformPitch(int basePitch) const noexcept {
        int shifted = basePitch;
        if (altActive) {
            shifted += (octaveOffset != 0) ? (static_cast<int>(octaveOffset) * 12) : 12;
        }
        shifted += semitoneOffset;
        return juce::jlimit(0, 127, shifted);
    }
};

struct KeyAction {
    KeyActionType type = KeyActionType::note;
    KeyTrigger trigger = KeyTrigger::keyDown;

    // 保持当前裸字段以兼容现有序列化与调用路径。
    // 后续模块可优先通过下方强类型 helper 访问这些值。
    int midiNote = 60;
    int midiChannel = 1;
    float velocity = 1.0f;

    [[nodiscard]] MidiNoteNumber getMidiNoteNumber() const noexcept {
        return MidiNoteNumber::fromClamped(midiNote);
    }

    [[nodiscard]] MidiChannel getMidiChannel() const noexcept {
        return MidiChannel::fromClamped(midiChannel);
    }

    [[nodiscard]] Velocity getVelocity() const noexcept {
        return Velocity::fromClamped(velocity);
    }

    void setMidiNoteNumber(MidiNoteNumber note) noexcept {
        midiNote = note.value;
    }

    void setMidiChannel(MidiChannel channel) noexcept {
        midiChannel = channel.value;
    }

    void setVelocity(Velocity newVelocity) noexcept {
        velocity = newVelocity.value;
    }
};

struct KeyBinding {
    int keyCode = 0;
    juce::String displayText;
    KeyAction action;
};

struct KeyGroup {
    int8_t transposeOffset = 0; // Transpose shift in semitones (-12..+12)
    int8_t octaveShift = 0; // Octave shift (-3..+3, 12 semitones per octave)
    uint8_t channel = 0; // MIDI channel override (0: inherit from binding, 1..16: override)
    juce::String name; // Group label ("A", "B", "C", "D")
};

[[nodiscard]] inline int calculateSoundingNote(int baseNote, const KeyGroup& group) noexcept {
    const auto totalShift = static_cast<int>(group.transposeOffset) + static_cast<int>(group.octaveShift) * 12;
    return juce::jlimit(0, 127, baseNote + totalShift);
}

[[nodiscard]] inline int calculateSoundingChannel(int baseChannel, const KeyGroup& group) noexcept {
    if (group.channel >= 1 && group.channel <= 16) {
        return static_cast<int>(group.channel);
    }
    return baseChannel;
}

struct HeldKeyIdentity {
    int physicalKeyCode = 0; // Physical key code (e.g. 'A')
    int soundingMidiNote = 60; // Sounding MIDI note locked at NoteOn
    int soundingMidiChannel = 1; // Sounding MIDI channel locked at NoteOn
    float velocity = 1.0f; // Trigger velocity
};

struct KeyboardLayout {
    juce::String id { "devpiano.default" };
    juce::String name { "DevPiano Default" };
    std::vector<KeyBinding> bindings;
    std::array<KeyGroup, 4> groups { KeyGroup { 0, 0, 0, "A" }, KeyGroup { 0, 0, 0, "B" }, KeyGroup { 0, 0, 0, "C" },
                                     KeyGroup { 0, 0, 0, "D" } };
    uint8_t activeGroupIndex = 0;

    [[nodiscard]] const KeyGroup& getActiveGroup() const noexcept {
        return groups[activeGroupIndex % 4];
    }

    [[nodiscard]] KeyGroup& getActiveGroup() noexcept {
        return groups[activeGroupIndex % 4];
    }

    [[nodiscard]] const KeyBinding* findByKeyCode(int keyCodeToFind) const noexcept {
        for (const auto& binding : bindings) {
            if (binding.keyCode == keyCodeToFind) {
                return &binding;
            }
        }

        return nullptr;
    }
};

[[nodiscard]] inline int normaliseAlphaNumericKeyCode(int keyCode) {
    if (!std::isalnum(static_cast<unsigned char>(keyCode))) {
        return 0;
    }

    return juce::KeyPress(std::toupper(static_cast<unsigned char>(keyCode)), 0, 0).getKeyCode();
}

[[nodiscard]] inline int makeAlphaNumericKeyCode(char character) {
    return normaliseAlphaNumericKeyCode(static_cast<unsigned char>(character));
}

[[nodiscard]] inline KeyBinding makeNoteBinding(char character, int midiNote, int midiChannel = 1,
                                                float velocity = 1.0f, KeyTrigger trigger = KeyTrigger::keyDown) {
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

[[nodiscard]] inline KeyBinding makeNoteBinding(char character, MidiNoteNumber midiNote,
                                                MidiChannel midiChannel = MidiChannel::fromClamped(1),
                                                Velocity velocity = Velocity::fromClamped(1.0f),
                                                KeyTrigger trigger = KeyTrigger::keyDown) {
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

[[nodiscard]] inline KeyboardLayout makeDefaultKeyboardLayout() {
    constexpr int baseC123Row = 84;
    constexpr int baseCQweRow = 72;
    constexpr int baseCAsdRow = 60;
    constexpr int baseCZxcRow = 48;

    KeyboardLayout layout;
    layout.name = "DevPiano Default";
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
