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
#include "UI/CustomKeyboard.h"
#include "UI/DevPianoLookAndFeel.h"
#include "UI/PluginEditorWindow.h"
#include "UI/PluginTypes.h"
#include "UI/RecordingTypes.h"
#include "UI/jive/LayoutModel.h"
#include "UI/jive/StyleCatalog.h"
#include "UI/native/KeyboardViewport.h"

#include <jive_layouts/jive_layouts.h>
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

    // ── JIVE plugin panel accessors ──
    void setPluginPathText(const juce::String& text);
    [[nodiscard]] juce::String getPluginPathText() const;
    [[nodiscard]] juce::String getSelectedPluginName() const;
    void setPluginPanelExpanded(bool expanded);
    void updatePluginPanelState(const PluginPanelState& state);
    void setInstrumentFilterVisible(bool visible);
    void showPluginBrowseDialog();
    void refreshPluginPanelTexts();

    // ── JIVE controls panel accessors ──
    [[nodiscard]] float getMasterGain() const;
    [[nodiscard]] float getAttack() const;
    [[nodiscard]] float getDecay() const;
    [[nodiscard]] float getSustain() const;
    [[nodiscard]] float getRelease() const;
    [[nodiscard]] double getControlsPlaybackSpeed() const;
    void setControlsValues(float masterGain, float attack, float decay, float sustain, float release);
    void setControlsPlaybackSpeed(double speed);
    void setControlsPresets(const juce::StringArray& presetIds, const juce::String& currentPresetId,
                            const juce::StringArray& presetDisplayNames);
    [[nodiscard]] juce::String getSelectedPresetId() const;
    void updateControlsPresetActionButtons();
    void setRecordingControlsState(RecordingControlsState state);
    [[nodiscard]] juce::Rectangle<int> getRecentFilesButtonScreenBounds() const;
    void refreshControlsTexts();

    // ── JIVE keyboard area accessors ──
    CustomKeyboard& getCustomKeyboard();
    void setKeyboardLayout(const devpiano::core::KeyboardLayout& layout);
    void setKeyboardViewPosition(int midiNote, int pixelOffset = -1);
    [[nodiscard]] int getKeyboardViewPositionX() const noexcept;
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

    std::unique_ptr<DevPianoLookAndFeel> lookAndFeel;

    bool dropActive = false;

    // JIVE header bar (replaces native HeaderPanel)
    std::unique_ptr<::jive::Interpreter> jiveInterpreter;
    std::unique_ptr<::jive::GuiItem> jiveHeaderItem;
    // JIVE status bar (replaces native StatusBar)
    std::unique_ptr<::jive::GuiItem> jiveStatusBarItem;
    // JIVE plugin panel (replaces native PluginPanel)
    std::unique_ptr<::jive::GuiItem> jivePluginPanelItem;
    // JIVE controls panel (replaces native ControlsPanel)
    std::unique_ptr<::jive::GuiItem> jiveControlsPanelItem;
    juce::StringArray availablePresetIds;
    RecordingControlsState recordingControlsState;

    // JIVE keyboard area (replaces native KeyboardPanel)
    std::unique_ptr<::jive::GuiItem> jiveKeyboardAreaItem;
    CustomKeyboard* customKeyboardRef = nullptr;

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
