#pragma once

#include "JuceHeader.h"
#include <memory>

#include "Audio/AudioEngine.h"
#include "Audio/AudioDeviceDiagnostics.h"
#include "Core/AppState.h"
#include "Core/AppStateBuilder.h"
#include "Input/KeyboardMidiMapper.h"
#include "Midi/MidiRouter.h"
#include "Plugin/PluginHost.h"
#include "Plugin/PluginFlowSupport.h"
#include "Plugin/PluginOperationController.h"
#include "Recording/RecordingEngine.h"
#include "Recording/RecordingSessionController.h"
#include "Settings/SettingsComponent.h"
#include "Settings/SettingsModel.h"
#include "Settings/SettingsStore.h"
#include "UI/ControlsPanel.h"
#include "UI/HeaderPanel.h"
#include "UI/KeyboardPanel.h"
#include "UI/PluginEditorWindow.h"
#include "UI/PluginPanel.h"
#include "Layout/LayoutDirectoryScanner.h"
#include "Layout/LayoutFlowSupport.h"

class MainComponent final : public juce::AudioAppComponent,
                            private juce::Timer
{
    friend class devpiano::layout::LayoutFlowSupport;
    friend class devpiano::recording::RecordingSessionController;
    friend class devpiano::plugin::PluginOperationController;
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

    void restoreKeyboardFocus();
    static juce::Rectangle<int> getMainContentResizeLimits();
    void persistMainContentSize(int width, int height);

    bool keyPressed(const juce::KeyPress& key) override;
    bool keyStateChanged(bool isKeyDown) override;

protected:
    void focusGained(juce::Component::FocusChangeType cause) override;
    void focusLost(juce::Component::FocusChangeType cause) override;

private:
    struct RuntimeAudioConfig
    {
        double sampleRate = 44100.0;
        int blockSize = 512;
    };

    void timerCallback() override;

    void initialiseInputMappingFromSettings();
    void initialiseUi();
    [[nodiscard]] juce::Rectangle<int> getInitialMainContentBounds() const;
    void initialiseMidiRouting();
    [[nodiscard]] SettingsModel::PerformanceSettingsView getPerformanceSettingsFromUi() const;
    [[nodiscard]] juce::String getLastPluginNameForRecoveryStateFromUi() const;
    [[nodiscard]] SettingsModel::PluginRecoverySettingsView getPluginRecoverySettingsFromUi() const;
    [[nodiscard]] SettingsModel::PluginRecoverySettingsView getPluginRecoverySettingsWithFallback() const;
    void applyPerformanceSettingsToUi(const SettingsModel::PerformanceSettingsView& performance);
    void applyPerformanceSettingsToAudioEngine(const SettingsModel::PerformanceSettingsView& performance);
    void applyPluginRecoverySettings(const SettingsModel::PluginRecoverySettingsView& pluginRecovery);
    void handlePerformanceUiChanged();
    void applyUiStateToAudioEngine();
    void syncUiFromSettings();
    void syncSettingsFromUi();
    void suppressTextInputMethods();
    void initialiseAudioDevice();
    void captureAudioDeviceState();
    void prepareForAudioDeviceRebuild();
    void finishAudioDeviceRebuild();
    void collectCurrentSettingsState();
    void saveSettingsNow();
    void saveSettingsSoon();
    void showSettingsDialog();
    [[nodiscard]] bool isSettingsWindowDirty() const;
    [[nodiscard]] bool isSettingsWindowOpen() const;
    void closeSettingsWindow();
    void closeSettingsWindowAsync();
    void saveAndCloseSettingsWindow();
    [[nodiscard]] SettingsComponent* getSettingsContent() const;
    void refreshMidiStatusFromCurrentSnapshot();
    void logCurrentAudioDeviceDiagnostics(const juce::String& context) const;
    void renderReadOnlyUiState(const devpiano::core::AppState& appState);
    void refreshReadOnlyUiStateFromCurrentSnapshot();
    void finishPluginUiAction(bool shouldSaveSettings);
    [[nodiscard]] devpiano::core::RuntimeAudioState buildRuntimeAudioStateSnapshot() const;
    [[nodiscard]] devpiano::core::RuntimePluginState buildRuntimePluginStateSnapshot() const;
    [[nodiscard]] devpiano::core::RuntimeInputState buildRuntimeInputStateSnapshot() const;
    [[nodiscard]] devpiano::core::AppState buildCurrentAppStateSnapshot() const;
    double getCurrentRuntimeSampleRate() const;
    int getCurrentRuntimeBlockSize() const;
    [[nodiscard]] RuntimeAudioConfig getCurrentRuntimeAudioConfig() const;
    void runPluginActionWithAudioDeviceRebuild(const std::function<void(const RuntimeAudioConfig&)>& action);
    void runPluginActionWithAudioDeviceRebuild(const std::function<void()>& action);

    devpiano::recording::RecordingEngine recordingEngine;
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
    std::unique_ptr<devpiano::layout::LayoutFlowSupport> layoutFlowSupport;
    std::unique_ptr<devpiano::recording::RecordingSessionController> recordingSessionController;
    std::unique_ptr<devpiano::plugin::PluginOperationController> pluginOperationController;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
