#include "KeyboardPanel.h"

#include "UI/CustomKeyboard.h"

KeyboardPanel::KeyboardPanel(juce::MidiKeyboardState& keyboardState)
    : customKeyboard(std::make_unique<CustomKeyboard>(keyboardState)) {
    addAndMakeVisible(customKeyboard.get());
}

void KeyboardPanel::resized() {
    if (customKeyboard != nullptr)
        customKeyboard->setBounds(getLocalBounds());
}

void KeyboardPanel::setKeyboardLayout(const devpiano::core::KeyboardLayout& layout) {
    customKeyboard->setKeyboardLayout(layout);
}
