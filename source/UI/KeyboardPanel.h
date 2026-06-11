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

    void resized() override;

    CustomKeyboard& getCustomKeyboard() noexcept {
        return *customKeyboard;
    }

    void setKeyboardLayout(const devpiano::core::KeyboardLayout& layout);

private:
    std::unique_ptr<CustomKeyboard> customKeyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyboardPanel)
};
