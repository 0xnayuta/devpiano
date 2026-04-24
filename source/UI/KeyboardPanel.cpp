#include "KeyboardPanel.h"

KeyboardPanel::KeyboardPanel(juce::MidiKeyboardState& keyboardState)
    : keyboardComponent(keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    addAndMakeVisible(keyboardComponent);
    keyboardComponent.setAvailableRange(24, 96);
    keyboardComponent.setKeyWidth(24.0f);
    keyboardComponent.setScrollButtonsVisible(true);

    // MidiKeyboardComponent installs its own default QWERTY mappings
    // ("awsedftgyhujkolp;") in the constructor. devpiano owns computer-keyboard
    // to MIDI translation via KeyboardMidiMapper, so leaving JUCE's built-in
    // mappings enabled can double-trigger notes if the virtual keyboard gains
    // focus, for example after using the scroll buttons.
    keyboardComponent.clearKeyMappings();
    keyboardComponent.setWantsKeyboardFocus(false);
    keyboardComponent.setMouseClickGrabsKeyboardFocus(false);
}

void KeyboardPanel::resized()
{
    keyboardComponent.setBounds(getLocalBounds());
}
