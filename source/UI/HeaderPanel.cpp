#include "HeaderPanel.h"

HeaderPanel::HeaderPanel()
{
    titleLabel.setText("devpiano", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    hintLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(hintLabel);

    midiStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    addAndMakeVisible(midiStatusLabel);

    audioStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightblue);
    addAndMakeVisible(audioStatusLabel);

    addAndMakeVisible(settingsButton);
    settingsButton.onClick = [this]
    {
        if (onSettingsRequested)
            onSettingsRequested();
    };
}

void HeaderPanel::resized()
{
    auto area = getLocalBounds();

    auto topRow = area.removeFromTop(30);
    titleLabel.setBounds(topRow.removeFromLeft(180));
    settingsButton.setBounds(topRow.removeFromRight(110));

    hintLabel.setBounds(area.removeFromTop(24));
    midiStatusLabel.setBounds(area.removeFromTop(22));
    audioStatusLabel.setBounds(area.removeFromTop(22));
}

void HeaderPanel::setHintText(const juce::String& text)
{
    hintLabel.setText(text, juce::dontSendNotification);
}

void HeaderPanel::updateMidiStatus(const MidiStatus& status)
{
    auto text = "MIDI Inputs: " + juce::String(status.openInputCount);

    if (status.activityCount > 0)
    {
        text << " | Activity: " << juce::String(status.activityCount);

        if (status.lastMessage.isNotEmpty())
            text << " | Last: " << status.lastMessage;
    }

    midiStatusLabel.setText(text, juce::dontSendNotification);
}

void HeaderPanel::updateAudioStatus(const AudioStatus& status)
{
    audioStatusLabel.setText(status.summary, juce::dontSendNotification);
}
