#include "ControlsPanel.h"

ControlsPanel::ControlsPanel()
{
    configureSlider(volumeSlider, volumeLabel, "Volume", 0.0, 1.0);
    configureSlider(attackSlider, attackLabel, "Attack", 0.001, 2.0);
    configureSlider(decaySlider, decayLabel, "Decay", 0.001, 2.0);
    configureSlider(sustainSlider, sustainLabel, "Sustain", 0.0, 1.0);
    configureSlider(releaseSlider, releaseLabel, "Release", 0.001, 3.0);

    layoutLabel.setText("Layout", juce::dontSendNotification);
    layoutLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(layoutLabel);

    addAndMakeVisible(layoutComboBox);
    layoutComboBox.setTextWhenNothingSelected("default.freepiano.minimal");
    layoutComboBox.onChange = [this]
    {
        const auto selectedId = layoutComboBox.getSelectedId();
        if (selectedId <= 0 || ! onLayoutChanged)
            return;

        onLayoutChanged(layoutComboBox.getItemText(selectedId - 1));
    };
}

ControlsPanel::~ControlsPanel()
{
    volumeSlider.onValueChange = nullptr;
    attackSlider.onValueChange = nullptr;
    decaySlider.onValueChange = nullptr;
    sustainSlider.onValueChange = nullptr;
    releaseSlider.onValueChange = nullptr;
    layoutComboBox.onChange = nullptr;
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

    auto row = area.removeFromTop(rowHeight);
    layoutLabel.setBounds(row.removeFromLeft(80));
    layoutComboBox.setBounds(row.removeFromLeft(200));
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

void ControlsPanel::setLayouts(const juce::StringArray& layoutIds, const juce::String& currentLayoutId)
{
    layoutComboBox.clear(juce::dontSendNotification);
    for (int i = 0; i < layoutIds.size(); ++i)
        layoutComboBox.addItem(layoutIds[i], i + 1);

    const auto index = layoutIds.indexOf(currentLayoutId);
    const auto selectedId = index >= 0 ? (index + 1) : 1;
    layoutComboBox.setSelectedId(selectedId, juce::dontSendNotification);
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
