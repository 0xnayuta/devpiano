#pragma once

#include <JuceHeader.h>

#include <unordered_set>

#include "Core/KeyMapTypes.h"

class MidiChannelMapper;
class KeyboardMidiMapper {
public:
    KeyboardMidiMapper();

    void setLayout(devpiano::core::KeyboardLayout newLayout);
    void setLayoutDisplayName(juce::String newDisplayName);
    [[nodiscard]] const devpiano::core::KeyboardLayout& getLayout() const noexcept;
    void resetToDefaultLayout();

    void setChannelMapper(const MidiChannelMapper* mapper) noexcept;
    bool handleKeyPressed(const juce::KeyPress& key, juce::MidiKeyboardState& keyboardState);
    bool handleKeyStateChanged(juce::MidiKeyboardState& keyboardState);

private:
    [[nodiscard]] int normaliseKeyCode(const juce::KeyPress& key) const;
    bool triggerBinding(const devpiano::core::KeyBinding& binding, juce::MidiKeyboardState& keyboardState,
                        bool isKeyDownEvent);

    devpiano::core::KeyboardLayout layout;
    const MidiChannelMapper* channelMapper = nullptr;
    std::unordered_set<int> heldKeys;
};
