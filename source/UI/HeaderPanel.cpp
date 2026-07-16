#include "HeaderPanel.h"

HeaderPanel::HeaderPanel() {
    titleLabel.setText("devpiano", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    hintLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(hintLabel);

    midiStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    addAndMakeVisible(midiStatusLabel);

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

    hintLabel.setBounds(area.removeFromTop(24));
    midiStatusLabel.setBounds(area);
}

void HeaderPanel::setHintText(const juce::String& text) {
    lastHintText = text;
    hintLabel.setText(TRANS(text), juce::dontSendNotification);
}

void HeaderPanel::updateMidiStatus(const MidiStatus& status) {
    lastMidiStatus = status;
    auto text = TRANS("MIDI Inputs: ") + juce::String(status.openInputCount);

    if (status.activityCount > 0) {
        text << TRANS(" | Activity: ") << juce::String(status.activityCount);

        if (status.lastMessage.isNotEmpty())
            text << TRANS(" | Last: ") << status.lastMessage;
    }

    midiStatusLabel.setText(text, juce::dontSendNotification);
}

void HeaderPanel::refreshTexts() {
    settingsButton.setButtonText(TRANS("Settings"));
    // Re-apply the last hint text and MIDI status so they pick up the new locale.
    hintLabel.setText(TRANS(lastHintText), juce::dontSendNotification);
    updateMidiStatus(lastMidiStatus);
}
