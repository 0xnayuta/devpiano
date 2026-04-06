#pragma once

#include <JuceHeader.h>

class ControlsPanel final : public juce::Component
{
public:
    ControlsPanel();
    ~ControlsPanel() override;

    void resized() override;

    void setValues(float masterGain,
                   float attack,
                   float decay,
                   float sustain,
                   float release);

    [[nodiscard]] float getMasterGain() const;
    [[nodiscard]] float getAttack() const;
    [[nodiscard]] float getDecay() const;
    [[nodiscard]] float getSustain() const;
    [[nodiscard]] float getRelease() const;

    std::function<void()> onValuesChanged;
    std::function<void(const juce::String&)> onLayoutChanged;
    std::function<void()> onSaveLayoutRequested;
    std::function<void()> onResetLayoutRequested;

    void setLayouts(const juce::StringArray& layoutIds, const juce::String& currentLayoutId);

private:
    [[nodiscard]] static juce::String makeLayoutDisplayName(const juce::String& layoutId);

    void configureSlider(juce::Slider& slider,
                         juce::Label& label,
                         const juce::String& text,
                         double minimum,
                         double maximum,
                         double interval = 0.001);

    juce::StringArray availableLayoutIds;

    juce::Label volumeLabel;
    juce::Label attackLabel;
    juce::Label decayLabel;
    juce::Label sustainLabel;
    juce::Label releaseLabel;
    juce::Label layoutLabel;

    juce::Slider volumeSlider;
    juce::Slider attackSlider;
    juce::Slider decaySlider;
    juce::Slider sustainSlider;
    juce::Slider releaseSlider;
    juce::ComboBox layoutComboBox;
    juce::TextButton saveLayoutButton { "Save Layout" };
    juce::TextButton resetLayoutButton { "Reset Current Layout" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlsPanel)
};
