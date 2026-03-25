#pragma once

#include <JuceHeader.h>
#include <memory>

#include "Audio/AudioEngine.h"
#include "Input/KeyboardMidiMapper.h"
#include "Midi/MidiRouter.h"
#include "Plugin/PluginHost.h"
#include "Settings/SettingsComponent.h"
#include "Settings/SettingsModel.h"
#include "Settings/SettingsStore.h"

class MainComponent final : public juce::AudioAppComponent
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

    bool keyPressed(const juce::KeyPress& key) override;
    bool keyStateChanged(bool isKeyDown) override;

private:
    void configureSlider(juce::Slider& slider,
                         juce::Label& label,
                         const juce::String& text,
                         double minimum,
                         double maximum,
                         double initialValue,
                         double interval = 0.001);

    void applyUiStateToAudioEngine();
    void syncUiFromSettings();
    void syncSettingsFromUi();
    void restoreKeyboardFocus();
    void initialiseAudioDevice();
    void captureAudioDeviceState();
    void saveSettingsSoon();
    void showSettingsDialog();
    void updateMidiStatusLabel();
    void updatePluginStatusLabel();
    void scanPlugins();

    AudioEngine audioEngine;
    KeyboardMidiMapper keyboardMidiMapper;
    MidiRouter midiRouter;
    PluginHost pluginHost;
    SettingsModel appSettings;
    SettingsStore settingsStore;

    juce::Label titleLabel;
    juce::Label hintLabel;
    juce::Label midiStatusLabel;
    juce::Label pluginStatusLabel;
    juce::Label pluginPathLabel;
    juce::Label pluginListLabel;
    juce::Label volumeLabel;
    juce::Label attackLabel;
    juce::Label decayLabel;
    juce::Label sustainLabel;
    juce::Label releaseLabel;

    juce::Slider volumeSlider;
    juce::Slider attackSlider;
    juce::Slider decaySlider;
    juce::Slider sustainSlider;
    juce::Slider releaseSlider;

    juce::TextButton settingsButton { "Settings" };
    juce::TextButton scanPluginsButton { "Scan VST3" };
    juce::TextEditor pluginPathEditor;
    juce::TextEditor pluginListEditor;
    juce::MidiKeyboardComponent keyboardComponent;
    std::unique_ptr<juce::DialogWindow> settingsWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
