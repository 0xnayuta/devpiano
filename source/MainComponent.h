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
#include "Recording/RecordingEngine.h"
#include "Settings/SettingsComponent.h"
#include "Settings/SettingsModel.h"
#include "Settings/SettingsStore.h"
#include "UI/ControlsPanel.h"
#include "UI/HeaderPanel.h"
#include "UI/KeyboardPanel.h"
#include "UI/PluginEditorWindow.h"
#include "UI/PluginPanel.h"
#include "Layout/LayoutDirectoryScanner.h"

class MainComponent final : public juce::AudioAppComponent,
                            private juce::Timer
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

    void restoreKeyboardFocus();

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
    void initialiseMidiRouting();
    [[nodiscard]] SettingsModel::PerformanceSettingsView getPerformanceSettingsFromUi() const;
    [[nodiscard]] juce::String getLastPluginNameForRecoveryStateFromUi() const;
    [[nodiscard]] juce::String getPersistedPluginSearchPath() const;
    [[nodiscard]] SettingsModel::PluginRecoverySettingsView getPluginRecoverySettingsFromUi() const;
    [[nodiscard]] SettingsModel::PluginRecoverySettingsView getPluginRecoverySettingsWithFallback() const;
    void applyPerformanceSettingsToUi(const SettingsModel::PerformanceSettingsView& performance);
    void applyPerformanceSettingsToAudioEngine(const SettingsModel::PerformanceSettingsView& performance);
    void applyPluginRecoverySettings(const SettingsModel::PluginRecoverySettingsView& pluginRecovery);
    void commitPluginRecoveryStateAndFinishUi(const SettingsModel::PluginRecoverySettingsView& pluginRecovery,
                                              bool shouldSaveSettings);
    void handlePerformanceUiChanged();
    void handleLayoutChanged(const juce::String& newLayoutId);
    void handleSaveLayoutRequested();
    void handleResetLayoutToDefaultRequested();
    void handleImportLayoutRequested();
    void handleRenameLayoutRequested();
    void handleDeleteLayoutRequested();
    void handleRecordClicked();
    void handlePlayClicked();
    void handleStopClicked();
    void handleExportMidiClicked();
    void handleExportWavClicked();
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
    void refreshMidiStatusFromCurrentSnapshot();
    void logCurrentAudioDeviceDiagnostics(const juce::String& context) const;
    void renderReadOnlyUiState(const devpiano::core::AppState& appState);
    void refreshReadOnlyUiStateFromCurrentSnapshot();
    void finishPluginUiAction(bool shouldSaveSettings);
    [[nodiscard]] devpiano::core::RuntimeAudioState buildRuntimeAudioStateSnapshot() const;
    [[nodiscard]] devpiano::core::RuntimePluginState buildRuntimePluginStateSnapshot() const;
    [[nodiscard]] devpiano::core::RuntimeInputState buildRuntimeInputStateSnapshot() const;
    [[nodiscard]] devpiano::core::AppState buildCurrentAppStateSnapshot() const;
    void restorePluginStateOnStartup();
    void restorePluginScanPathOnStartup(const devpiano::plugin::StartupPluginRestorePlan& plan);
    void restoreLastPluginOnStartup(const devpiano::plugin::StartupPluginRestorePlan& plan);
    void restorePluginByNameOnStartup(const juce::String& pluginName);
    [[nodiscard]] juce::FileSearchPath resolvePluginScanPath() const;
    double getCurrentRuntimeSampleRate() const;
    int getCurrentRuntimeBlockSize() const;
    [[nodiscard]] RuntimeAudioConfig getCurrentRuntimeAudioConfig() const;
    void startInternalRecording(std::size_t expectedEventCapacity);
    [[nodiscard]] devpiano::recording::RecordingTake stopInternalRecording();
    void startInternalPlayback(const devpiano::recording::RecordingTake& take);
    [[nodiscard]] devpiano::recording::RecordingTake stopInternalPlayback();
    void runPluginActionWithAudioDeviceRebuild(const std::function<void(const RuntimeAudioConfig&)>& action);
    void runPluginActionWithAudioDeviceRebuild(const std::function<void()>& action);
    [[nodiscard]] juce::String getSelectedPluginNameForLoad() const;
    void loadPluginByNameAndCommitState(const juce::String& pluginName);
    [[nodiscard]] std::unique_ptr<juce::AudioProcessorEditor> tryCreatePluginEditor() const;
    void handlePluginEditorWindowClosedAsync();
    void closePluginEditorWindow();
    void openPluginEditorWindow(std::unique_ptr<juce::AudioProcessorEditor> editor);
    void unloadPluginAndCommitState();
    void scanPluginsAtPathAndApplyRecoveryState(const juce::FileSearchPath& path,
                                                const juce::String& lastPluginName);
    void scanPluginsAtPathAndCommitState(const juce::FileSearchPath& path);
    void loadSelectedPlugin();
    void unloadCurrentPlugin();
    void togglePluginEditor();
    void scanPlugins();

    devpiano::recording::RecordingEngine recordingEngine;
    devpiano::recording::RecordingTake currentTake;
    AudioEngine audioEngine;
    ControlsPanel::RecordingState currentRecordingState = ControlsPanel::RecordingState::idle;
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
    std::unique_ptr<juce::FileChooser> saveLayoutChooser;
    std::unique_ptr<juce::FileChooser> importLayoutChooser;
    std::unique_ptr<juce::FileChooser> exportMidiChooser;
    std::unique_ptr<juce::FileChooser> exportWavChooser;
    std::unique_ptr<PluginEditorWindow> pluginEditorWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
