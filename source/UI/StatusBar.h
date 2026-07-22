#pragma once

#include <JuceHeader.h>

// ============================================================================
// Bottom status bar: MIDI activity dot, plugin name, audio device info, time.
// Fixed 22px height, sits below all main panels.
// ============================================================================
class StatusBar final : public juce::Component {
public:
    StatusBar();

    void paint(juce::Graphics&) override;
    void resized() override;

    void setPluginName(const juce::String& name);
    void setAudioInfo(const juce::String& info);
    void setTimeDisplay(const juce::String& time);
    void setMidiActivity(bool active);

    void refreshTexts();

private:
    juce::Label pluginNameLabel;
    juce::Label audioInfoLabel;
    juce::Label timeLabel;
    bool midiActive = false;
    juce::Rectangle<float> midiDotBounds;

    static constexpr int dotDiameter = 6;
    static constexpr int dotMargin = 6;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusBar)
};
