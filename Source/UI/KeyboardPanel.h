#pragma once

#include <JuceHeader.h>

class KeyboardPanel final : public juce::Component
{
public:
    explicit KeyboardPanel(juce::MidiKeyboardState& keyboardState);

    void resized() override;

private:
    juce::MidiKeyboardComponent keyboardComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyboardPanel)
};
