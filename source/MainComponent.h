#pragma once

#include "JuceHeader.h"
#include <memory>

#include "Audio/AudioDeviceDiagnostics.h"
#include "Audio/AudioEngine.h"
#include "Core/AppState.h"
#include "Diagnostics/DevPianoLogger.h"
#include "Input/KeyboardMidiMapper.h"
#include "Layout/PresetFlowSupport.h"
#include "Locale/LocaleManager.h"
#include "Midi/MidiChannelMapper.h"
#include "Settings/AppStateBuilder.h"

#include "Plugin/PluginHost.h"
#include "Plugin/PluginOperationController.h"
#include "Recording/RecordingEngine.h"
#include "Recording/RecordingSessionController.h"
#include "Settings/SettingsModel.h"
#include "Settings/SettingsStore.h"
#include "Settings/SettingsWindowManager.h"
#include "UI/ControlsPanel.h"
#include "UI/DevPianoLookAndFeel.h"
#include "UI/HeaderPanel.h"
#include "UI/KeyboardPanel.h"
#include "UI/PluginEditorWindow.h"
#include "UI/PluginPanel.h"
#include "UI/StatusBar.h"
#if DEBUG
#include <melatonin_inspector/melatonin_inspector.h>
#endif

class MainComponent final : public juce::AudioAppComponent, private juce::Timer, public juce::FileDragAndDropTarget {
    friend class devpiano::layout::PresetFlowSupport;
    friend class devpiano::recording::RecordingSessionController;
    friend class devpiano::plugin::PluginOperationController;
    friend class devpiano::settings::SettingsWindowManager;

public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

    // FileDragAndDropTarget interface
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragMove(const juce::StringArray&, int, int) override {
    }
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void restoreKeyboardFocus();
    static juce::Rectangle<int> getMainContentResizeLimits();
    void persistMainContentSize(int width, int height);

    [[nodiscard]] SettingsModel& getAppSettings() noexcept {
        return appSettings;
    }
    bool keyPressed(const juce::KeyPress& key) override;
    bool keyStateChanged(bool isKeyDown) override;
    [[nodiscard]] bool isKeyboardInputSuppressed() const noexcept;
    [[nodiscard]] bool shouldTakeKeyboardFocus() const noexcept;

protected:
    void focusGained(juce::Component::FocusChangeType cause) override;
    void focusLost(juce::Component::FocusChangeType cause) override;

private:
    struct RuntimeAudioConfig {
        double sampleRate = 44100.0;
        int blockSize = 512;
    };

    void timerCallback() override;

    void initialiseFromPreset();
    void initialiseUi();
    [[nodiscard]] juce::Rectangle<int> getInitialMainContentBounds() const;
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
    void reconfigureChannelMapper();
    void handlePresetShortcut(int index);
    void suppressTextInputMethods();
    void initialiseAudioDevice();
    void captureAudioDeviceState();
    void prepareForAudioDeviceRebuild();
    void finishAudioDeviceRebuild();
    void collectCurrentSettingsState();
    void saveSettingsNow();
    void saveSettingsSoon();
    void showSettingsDialog();
    [[nodiscard]] bool isSettingsWindowOpen() const;
    void logCurrentAudioDeviceDiagnostics(const juce::String& context) const;
    void renderReadOnlyUiState(const devpiano::core::AppState& appState);
    void refreshReadOnlyUiStateFromCurrentSnapshot();
    void refreshPluginUiState();
    void finishPluginUiAction(bool shouldSaveSettings);
    [[nodiscard]] devpiano::core::AppState buildCurrentAppStateSnapshot() const;
    double getCurrentRuntimeSampleRate() const;
    int getCurrentRuntimeBlockSize() const;
    void applyLanguage(const juce::String& code);
    void refreshAllTexts();
    void showRecentFilesMenu();
    void saveRecentFiles();

    [[nodiscard]] RuntimeAudioConfig getCurrentRuntimeAudioConfig() const;
    void runPluginActionWithAudioDeviceRebuild(const std::function<void(const RuntimeAudioConfig&)>& action);
    void runPluginActionWithAudioDeviceRebuild(const std::function<void()>& action);

    devpiano::recording::RecordingEngine recordingEngine;
    AudioEngine audioEngine;
    KeyboardMidiMapper keyboardMidiMapper;
    PluginHost pluginHost;
    SettingsModel appSettings;
    SettingsStore settingsStore;
    juce::RecentlyOpenedFilesList recentFiles;

    DevPianoLookAndFeel lookAndFeel;

    bool dropActive = false;

    HeaderPanel headerPanel;
    PluginPanel pluginPanel;
    ControlsPanel controlsPanel;
    KeyboardPanel keyboardPanel;
    StatusBar statusBar;
    std::unique_ptr<devpiano::settings::SettingsWindowManager> settingsWindowManager;
    std::unique_ptr<devpiano::layout::PresetFlowSupport> presetFlowSupport;
    std::unique_ptr<devpiano::recording::RecordingSessionController> recordingSessionController;
    std::unique_ptr<devpiano::plugin::PluginOperationController> pluginOperationController;
    std::unique_ptr<devpiano::diagnostics::DevPianoLogger> devPianoLogger;
    std::unique_ptr<devpiano::midi::MidiChannelMapper> midiChannelMapper;
#if DEBUG
    std::unique_ptr<melatonin::Inspector> inspector;
#endif
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
