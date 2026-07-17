#pragma once

#include <JuceHeader.h>

class HeaderPanel final : public juce::Component {
public:
    struct AudioStatus {
        juce::String summary;
    };

    HeaderPanel();

    void resized() override;

    void setHintText(const juce::String& text);
    std::function<void()> onSettingsRequested;

    void refreshTexts();

private:
    juce::Label titleLabel;
    juce::Label hintLabel;
    juce::TextButton settingsButton { TRANS("Settings") };
    juce::String lastHintText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeaderPanel)
};
