#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
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

class MainComponent final : public juce::AudioAppComponent,
                            private juce::Timer,
                            public juce::FileDragAndDropTarget,
                            private juce::MidiKeyboardState::Listener {
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
    /// Hot reloads design_tokens.json and style_sheets.json at runtime without restarting.
    void reloadStylesAndTokens();

    [[nodiscard]] SettingsModel& getAppSettings() noexcept {
        return appSettings;
    }
    bool keyPressed(const juce::KeyPress& key) override;
    bool keyStateChanged(bool isKeyDown) override;
    [[nodiscard]] bool isKeyboardInputSuppressed() const noexcept;
    void setBuiltinSynthTone(SettingsModel::BuiltinTone tone);
    [[nodiscard]] bool shouldTakeKeyboardFocus() const noexcept;
    void handleWindowFocusLost();

protected:
    void focusGained(juce::Component::FocusChangeType cause) override;
    void focusLost(juce::Component::FocusChangeType cause) override;

private:
    struct RuntimeAudioConfig {
        double sampleRate = 44100.0;
        int blockSize = 512;
    };

    // MidiKeyboardState::Listener interface
    void handleNoteOn(juce::MidiKeyboardState* source, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState* source, int midiChannel, int midiNoteNumber, float velocity) override;
    void timerCallback() override;

    void initialiseFromPreset();
    void initialiseUi();
    [[nodiscard]] juce::Rectangle<int> getInitialMainContentBounds() const;
    [[nodiscard]] SettingsModel::PerformanceSettingsView getPerformanceSettingsFromUi() const;
    [[nodiscard]] SettingsModel::BuiltinTone getBuiltinToneFromUi() const;
    [[nodiscard]] float getPianoBrightness() const;
    [[nodiscard]] float getPianoHammerHardness() const;
    [[nodiscard]] float getPianoResonance() const;
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
    void refreshPluginStatusEllipsis();
    void updatePluginPanelState(const devpiano::ui::PluginPanelState& state);
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
    void setControlsPianoValues(SettingsModel::BuiltinTone tone, float brightness, float hammerHardness,
                                float resonance);
    void setControlsPlaybackSpeed(double speed);
    void setControlsPresets(const juce::StringArray& presetIds, const juce::String& currentPresetId,
                            const juce::StringArray& presetDisplayNames);
    [[nodiscard]] juce::String getSelectedPresetId() const;
    void updateControlsPresetActionButtons();
    void setRecordingControlsState(devpiano::ui::RecordingControlsState state);
    [[nodiscard]] juce::Rectangle<int> getRecentFilesButtonScreenBounds() const;
    void refreshControlsTexts();

    // ── JIVE keyboard area accessors ──
    CustomKeyboard& getCustomKeyboard();
    void setKeyboardLayout(const devpiano::core::KeyboardLayout& layout);
    void setKeyboardViewPosition(int midiNote, int pixelOffset = -1);
    [[nodiscard]] int getKeyboardViewPositionX() const noexcept;

    // ── JIVE status bar accessors ──
    void updateStatusBar();
    void showStatusMessage(const juce::String& text, int timeoutMs = 3000);
    void notifyMidiActivity();
    [[nodiscard]] class StatusBarMidiDot* getStatusBarMidiDot() const;

    void refreshReadOnlyUiStateFromCurrentSnapshot();
    void refreshPluginUiState();
    void finishPluginUiAction(bool shouldSaveSettings);
    [[nodiscard]] devpiano::core::AppState buildAppStateSnapshot() const;
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

    // Single JIVE tree for the whole window (replaces the native panels)
    std::unique_ptr<::jive::Interpreter> jiveInterpreter;
    std::unique_ptr<::jive::GuiItem> jiveRootItem;
    juce::String lastPluginStatusText; // full text; re-ellipsised on resize
    bool isUpdatingPluginSelector = false; // guard against onChange re-entrancy during programmatic UI refresh
    bool isUpdatingPresets = false; // guard against onChange re-entrancy during preset list refresh
    juce::StringArray availablePresetIds;
    devpiano::ui::RecordingControlsState recordingControlsState;
    CustomKeyboard* customKeyboardRef = nullptr;
    juce::String statusToastText;
    int statusToastTicksRemaining = 0;
    int statusBarThrottleCounter = 0;
    juce::Time lastTokensModTime;
    juce::Time lastStylesModTime;
    int hotReloadCheckCounter = 0;
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
