#pragma once

#include "JuceHeader.h"
#include <memory>

#include "Audio/AudioEngine.h"
#include "Core/AppState.h"
#include "Core/AppStateBuilder.h"
#include "Input/KeyboardMidiMapper.h"
#include "Midi/MidiRouter.h"
#include "Plugin/PluginHost.h"
#include "Settings/SettingsComponent.h"
#include "Settings/SettingsModel.h"
#include "Settings/SettingsStore.h"
#include "UI/ControlsPanel.h"
#include "UI/HeaderPanel.h"
#include "UI/KeyboardPanel.h"
#include "UI/PluginPanel.h"

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
    void initialiseInputMappingFromSettings();
    void initialiseUi();
    void initialiseMidiRouting();
    [[nodiscard]] SettingsModel::PerformanceSettingsView getPerformanceSettingsFromUi() const;
    void applyPerformanceSettingsToUi(const SettingsModel::PerformanceSettingsView& performance);
    void applyPerformanceSettingsToAudioEngine(const SettingsModel::PerformanceSettingsView& performance);
    void handlePerformanceUiChanged();
    void applyUiStateToAudioEngine();
    void syncUiFromSettings();
    void syncSettingsFromUi();
    void suppressTextInputMethods();
    void restoreKeyboardFocus();
    void initialiseAudioDevice();
    void captureAudioDeviceState();
    void prepareForAudioDeviceRebuild();
    void finishAudioDeviceRebuild();
    void collectCurrentSettingsState();
    void saveSettingsNow();
    void saveSettingsSoon();
    void showSettingsDialog();
    [[nodiscard]] bool isSettingsWindowDirty() const;
    void closeSettingsWindow();
    void closeSettingsWindowAsync();
    void saveAndCloseSettingsWindow();
    void updateMidiStatusLabel();
    void refreshPluginPanel();
    void applyReadOnlyUiState(const devpiano::core::AppState& appState);
    void refreshReadOnlyUiState();
    void finishPluginUiAction(bool shouldSaveSettings);
    [[nodiscard]] devpiano::core::RuntimePluginState createRuntimePluginStateSnapshot() const;
    [[nodiscard]] devpiano::core::RuntimeInputState createRuntimeInputStateSnapshot() const;
    [[nodiscard]] devpiano::core::AppState createAppStateSnapshot() const;
    void restorePluginStateOnStartup();
    void restorePluginScanAndLoadState();
    void restorePluginScanPathOnStartup();
    void restoreLastPluginOnStartup();
    [[nodiscard]] juce::FileSearchPath resolvePluginScanPath() const;
    double getCurrentRuntimeSampleRate() const;
    int getCurrentRuntimeBlockSize() const;
    void runPluginActionWithAudioDeviceRebuild(const std::function<void(double, int)>& action);
    void runPluginActionWithAudioDeviceRebuild(const std::function<void()>& action);
    void loadSelectedPlugin();
    void unloadCurrentPlugin();
    void togglePluginEditor();
    void scanPlugins();

    AudioEngine audioEngine;
    KeyboardMidiMapper keyboardMidiMapper;
    MidiRouter midiRouter;
    PluginHost pluginHost;
    SettingsModel appSettings;
    SettingsStore settingsStore;

    int externalMidiMessageCount = 0;
    juce::String lastExternalMidiMessage;

    HeaderPanel headerPanel;
    PluginPanel pluginPanel;
    ControlsPanel controlsPanel;
    KeyboardPanel keyboardPanel;
    std::unique_ptr<juce::DialogWindow> settingsWindow;
    std::unique_ptr<juce::DocumentWindow> pluginEditorWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
