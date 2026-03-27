#include "ControlsPanel.h"

ControlsPanel::ControlsPanel()
{
    configureSlider(volumeSlider, volumeLabel, "Volume", 0.0, 1.0);
    configureSlider(attackSlider, attackLabel, "Attack", 0.001, 2.0);
    configureSlider(decaySlider, decayLabel, "Decay", 0.001, 2.0);
    configureSlider(sustainSlider, sustainLabel, "Sustain", 0.0, 1.0);
    configureSlider(releaseSlider, releaseLabel, "Release", 0.001, 3.0);
}

void ControlsPanel::resized()
{
    auto area = getLocalBounds();
    const auto rowHeight = 28;

    auto layoutSliderRow = [&](juce::Slider& slider, juce::Label& label)
    {
        auto row = area.removeFromTop(rowHeight);
        label.setBounds(row.removeFromLeft(80));
        slider.setBounds(row);
        area.removeFromTop(8);
    };

    layoutSliderRow(volumeSlider, volumeLabel);
    layoutSliderRow(attackSlider, attackLabel);
    layoutSliderRow(decaySlider, decayLabel);
    layoutSliderRow(sustainSlider, sustainLabel);
    layoutSliderRow(releaseSlider, releaseLabel);
}

void ControlsPanel::setValues(float masterGain,
                              float attack,
                              float decay,
                              float sustain,
                              float release)
{
    volumeSlider.setValue(masterGain, juce::dontSendNotification);
    attackSlider.setValue(attack, juce::dontSendNotification);
    decaySlider.setValue(decay, juce::dontSendNotification);
    sustainSlider.setValue(sustain, juce::dontSendNotification);
    releaseSlider.setValue(release, juce::dontSendNotification);
}

float ControlsPanel::getMasterGain() const
{
    return static_cast<float>(volumeSlider.getValue());
}

float ControlsPanel::getAttack() const
{
    return static_cast<float>(attackSlider.getValue());
}

float ControlsPanel::getDecay() const
{
    return static_cast<float>(decaySlider.getValue());
}

float ControlsPanel::getSustain() const
{
    return static_cast<float>(sustainSlider.getValue());
}

float ControlsPanel::getRelease() const
{
    return static_cast<float>(releaseSlider.getValue());
}

void ControlsPanel::configureSlider(juce::Slider& slider,
                                    juce::Label& label,
                                    const juce::String& text,
                                    double minimum,
                                    double maximum,
                                    double interval)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(label);

    slider.setRange(minimum, maximum, interval);
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 22);
    slider.onValueChange = [this]
    {
        if (onValuesChanged)
            onValuesChanged();
    };
    addAndMakeVisible(slider);
}
