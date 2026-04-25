#pragma once

#include <JuceHeader.h>

class HeaderPanel final : public juce::Component
{
public:
    struct MidiStatus
    {
        int openInputCount = 0;
        int activityCount = 0;
        juce::String lastMessage;
    };

    struct AudioStatus
    {
        juce::String summary;
    };

    HeaderPanel();

    void resized() override;

    void setHintText(const juce::String& text);
    void updateMidiStatus(const MidiStatus& status);

    std::function<void()> onSettingsRequested;

private:
    juce::Label titleLabel;
    juce::Label hintLabel;
    juce::Label midiStatusLabel;
    juce::TextButton settingsButton { "Settings" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeaderPanel)
};
