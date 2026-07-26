#pragma once

#include "JuceHeader.h"
#include <memory>

#include "Audio/AudioDeviceDiagnostics.h"
#include "Audio/AudioEngine.h"
#include "Input/KeyboardInputHandler.h"
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
#include "UI/native/AdsrCurveComponent.h"
#include <jive_layouts/jive_layouts.h>
#if DEBUG
#include <melatonin_inspector/melatonin_inspector.h>
#endif

class MainComponent;

namespace devpiano::ui::jive {
void wireAllCallbacks(::jive::GuiItem& rootItem, MainComponent& mc);
} // namespace devpiano::ui::jive

class MainComponent final : public juce::AudioAppComponent, private juce::Timer, public juce::FileDragAndDropTarget {
    friend class devpiano::layout::PresetFlowSupport;
    friend class devpiano::recording::RecordingSessionController;
    friend class devpiano::plugin::PluginOperationController;
    friend class devpiano::settings::SettingsWindowManager;
    friend class devpiano::input::KeyboardInputHandler;

    friend void devpiano::ui::jive::wireAllCallbacks(::jive::GuiItem& rootItem, MainComponent& mc);

    // Window dimension constants
    static constexpr int kPreferredWidth = 1120;
    static constexpr int kPreferredHeight = 760;
    static constexpr int kMinWidth = 980;
    static constexpr int kMinHeight = 700;
    static constexpr int kMaxWidth = 3840;
    static constexpr int kMaxHeight = 2160;

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
    void suppressTextInputMethods();
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
    // ── JIVE component accessors (replace direct panel member access) ──
    CustomKeyboard& getCustomKeyboard();
    void setCustomKeyboardLayout(const devpiano::core::KeyboardLayout& layout);
    void setCustomKeyboardSettings(const devpiano::ui::KeyboardSettings& ks);
    void setKeyboardViewPosition(int midiNote, int pixelOffset = -1);
    int getKeyboardViewPositionX() const;

    // Plugin panel accessors (through JIVE tree)
    juce::String getPluginPanelPath() const;
    void setPluginPanelPath(const juce::String& path);
    juce::String getSelectedPluginName() const;
    void setInstrumentFilterVisible(bool visible);
    void updatePluginPanelState(const PluginPanelState& state);
    void setPluginPanelExpanded(bool expanded);
    bool isPluginPanelExpanded() const;

    // Controls panel accessors (through JIVE tree)
    void setControlsValues(float gain, float a, float d, float s, float r);
    float getControlsMasterGain() const;
    float getControlsAttack() const;
    float getControlsDecay() const;
    float getControlsSustain() const;
    float getControlsRelease() const;
    void setControlsPresets(const juce::StringArray& ids, const juce::String& current, const juce::StringArray& names);
    juce::String getControlsSelectedPresetId() const;
    void setControlsRecordingState(RecordingControlsState state);
    void setControlsPlaybackSpeed(double speed);
    juce::Rectangle<int> getRecentFilesButtonScreenBounds() const;

    // Status bar accessors (through JIVE tree)
    void setStatusPluginName(const juce::String& name);
    void setStatusAudioInfo(const juce::String& info);
    void setStatusTimeDisplay(const juce::String& time);
    void setStatusMidiActivity(bool active);
    AudioEngine audioEngine;
    KeyboardMidiMapper keyboardMidiMapper;
    std::unique_ptr<devpiano::input::KeyboardInputHandler> keyboardInputHandler;
    PluginHost pluginHost;
    SettingsModel appSettings;
    SettingsStore settingsStore;
    juce::RecentlyOpenedFilesList recentFiles;

    std::unique_ptr<DevPianoLookAndFeel> lookAndFeel;

    bool dropActive = false;

    AdsrCurveComponent adsrCurve;
    std::unique_ptr<jive::Interpreter> jiveInterpreter;
    std::unique_ptr<jive::GuiItem> jiveRootItem;
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
