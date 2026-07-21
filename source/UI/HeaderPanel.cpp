#include "HeaderPanel.h"
#include "DevPianoLookAndFeel.h"

HeaderPanel::HeaderPanel() {
    titleLabel.setText("devpiano", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    hintLabel.setColour(juce::Label::textColourId, DevPianoLookAndFeel::kTextSecondary);
    addAndMakeVisible(hintLabel);

    addAndMakeVisible(settingsButton);
    settingsButton.onClick = [this] {
        if (onSettingsRequested)
            onSettingsRequested();
    };
}

void HeaderPanel::resized() {
    auto area = getLocalBounds();

    auto topRow = area.removeFromTop(30);
    titleLabel.setBounds(topRow.removeFromLeft(180));
    settingsButton.setBounds(topRow.removeFromRight(110));

    hintLabel.setBounds(area);
}

void HeaderPanel::setHintText(const juce::String& text) {
    lastHintText = text;
    hintLabel.setText(TRANS(text), juce::dontSendNotification);
}

void HeaderPanel::refreshTexts() {
    settingsButton.setButtonText(TRANS("Settings"));
    // Re-apply the last hint text so it picks up the new locale.
    hintLabel.setText(TRANS(lastHintText), juce::dontSendNotification);
}
