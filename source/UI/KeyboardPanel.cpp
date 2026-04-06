#include "KeyboardPanel.h"

KeyboardPanel::KeyboardPanel(juce::MidiKeyboardState& keyboardState)
    : keyboardComponent(keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    addAndMakeVisible(keyboardComponent);
    keyboardComponent.setAvailableRange(24, 96);
    keyboardComponent.setKeyWidth(24.0f);
    keyboardComponent.setScrollButtonsVisible(true);
    keyboardComponent.setWantsKeyboardFocus(false);
    keyboardComponent.setMouseClickGrabsKeyboardFocus(false);
}

void KeyboardPanel::resized()
{
    keyboardComponent.setBounds(getLocalBounds());
}
