#pragma once

#include <JuceHeader.h>

class MainComponent;

namespace devpiano::input {

class KeyboardInputHandler final {
public:
    explicit KeyboardInputHandler(MainComponent& owner);

    [[nodiscard]] bool isKeyboardInputSuppressed() const noexcept;
    [[nodiscard]] bool shouldTakeKeyboardFocus() const noexcept;

    bool keyPressed(const juce::KeyPress& key);
    bool keyStateChanged(bool isKeyDown);
    void focusGained(juce::Component::FocusChangeType cause);
    void focusLost(juce::Component::FocusChangeType cause);
    void visibilityChanged();
    void suppressTextInputMethods();
    void restoreKeyboardFocus();

private:
    MainComponent& owner_;
};

} // namespace devpiano::input
