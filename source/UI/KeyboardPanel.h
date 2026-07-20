#pragma once

#include <JuceHeader.h>

namespace devpiano::core {
struct KeyboardLayout;
}
namespace devpiano::ui {
struct KeyboardSettings;
}

class CustomKeyboard;

class KeyboardPanel final : public juce::Component {
public:
    explicit KeyboardPanel(juce::MidiKeyboardState& keyboardState);
    ~KeyboardPanel() override;

    void resized() override;

    CustomKeyboard& getCustomKeyboard() noexcept {
        return *customKeyboard;
    }

    void setKeyboardLayout(const devpiano::core::KeyboardLayout& layout);

    // Scroll viewport so that the given MIDI note's white key is at the left edge.
    // negative note = restore from persisted pixel offset.
    void setViewPosition(int midiNote, int pixelOffset = 0);
    [[nodiscard]] int getViewPositionX() const noexcept;

private:
    std::unique_ptr<juce::Viewport> viewport;
    std::unique_ptr<CustomKeyboard> customKeyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyboardPanel)
};
