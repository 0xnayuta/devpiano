#pragma once

#include <JuceHeader.h>

class HeaderPanel final : public juce::Component {
public:
    HeaderPanel();

    void resized() override;

    std::function<void()> onSettingsRequested;
    void refreshTexts();

private:
    juce::Label titleLabel;
    juce::DrawableButton settingsButton { "", juce::DrawableButton::ImageFitted };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeaderPanel)
};
