#pragma once

#include <JuceHeader.h>

#include <unordered_set>

#include "Core/KeyMapTypes.h"

namespace devpiano::midi {
class MidiChannelMapper;
}

class KeyboardMidiMapper {
public:
    KeyboardMidiMapper();

    void setLayout(devpiano::core::KeyboardLayout newLayout);
    void setLayoutDisplayName(juce::String newDisplayName);
    [[nodiscard]] const devpiano::core::KeyboardLayout& getLayout() const noexcept;
    void resetToDefaultLayout();

    bool handleKeyPressed(const juce::KeyPress& key, juce::MidiKeyboardState& keyboardState);
    bool handleKeyStateChanged(juce::MidiKeyboardState& keyboardState);
    void setChannelMapper(devpiano::midi::MidiChannelMapper* mapper) noexcept;

private:
    [[nodiscard]] int normaliseKeyCode(const juce::KeyPress& key) const;
    bool triggerBinding(const devpiano::core::KeyBinding& binding, juce::MidiKeyboardState& keyboardState,
                        bool isKeyDownEvent);
    void sendNoteOff(int midiChannel, int midiNote, float velocity, juce::MidiKeyboardState& keyboardState);

    devpiano::midi::MidiChannelMapper* channelMapper = nullptr;
    devpiano::core::KeyboardLayout layout;
    std::unordered_set<int> heldKeys;
};
