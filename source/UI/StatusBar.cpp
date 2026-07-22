#include "StatusBar.h"
#include "DevPianoLookAndFeel.h"

StatusBar::StatusBar() {
    pluginNameLabel.setColour(juce::Label::textColourId, DevPianoLookAndFeel::kTextSecondary);
    pluginNameLabel.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(pluginNameLabel);

    audioInfoLabel.setColour(juce::Label::textColourId, DevPianoLookAndFeel::kTextSecondary);
    audioInfoLabel.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(audioInfoLabel);

    timeLabel.setColour(juce::Label::textColourId, DevPianoLookAndFeel::kTextSecondary);
    timeLabel.setFont(juce::FontOptions(12.0f));
    timeLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(timeLabel);
}

void StatusBar::paint(juce::Graphics& g) {
    // Background: slightly darker than panels
    g.fillAll(DevPianoLookAndFeel::kPanelBg.darker(0.2f));

    // Top separator line
    g.setColour(DevPianoLookAndFeel::kControlBg);
    g.drawHorizontalLine(0, 0.0f, static_cast<float>(getWidth()));

    // MIDI activity dot
    auto& k = DevPianoLookAndFeel::kPlayActive;
    g.setColour(midiActive ? k : k.withAlpha(0.25f));
    g.fillEllipse(midiDotBounds);
}

void StatusBar::resized() {
    auto area = getLocalBounds();
    constexpr int padX = 8;

    // MIDI dot on the left
    area.removeFromLeft(dotMargin);
    midiDotBounds = area.removeFromLeft(dotDiameter).toFloat().withHeight(static_cast<float>(dotDiameter));
    midiDotBounds.setY((static_cast<float>(getHeight()) - static_cast<float>(dotDiameter)) * 0.5f);
    area.removeFromLeft(dotMargin);

    // Time on the right
    timeLabel.setBounds(area.removeFromRight(120));

    // Plugin name on the left
    pluginNameLabel.setBounds(area.removeFromLeft(area.getWidth() / 2).withTrimmedLeft(padX));

    // Audio info fills the rest
    audioInfoLabel.setBounds(area);
}

void StatusBar::setPluginName(const juce::String& name) {
    pluginNameLabel.setText(name, juce::dontSendNotification);
}

void StatusBar::setAudioInfo(const juce::String& info) {
    audioInfoLabel.setText(info, juce::dontSendNotification);
}

void StatusBar::setTimeDisplay(const juce::String& time) {
    timeLabel.setText(time, juce::dontSendNotification);
}

void StatusBar::setMidiActivity(bool active) {
    if (midiActive != active) {
        midiActive = active;
        repaint();
    }
}

void StatusBar::refreshTexts() {
    // All labels are locale-independent (device names, file paths, etc.)
    // Re-apply to pick up any locale changes
    setPluginName(pluginNameLabel.getText());
    setAudioInfo(audioInfoLabel.getText());
    setTimeDisplay(timeLabel.getText());
}
