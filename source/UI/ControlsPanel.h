#pragma once

#include <JuceHeader.h>

class ControlsPanel final : public juce::Component {
public:
    ControlsPanel();
    ~ControlsPanel() override;

    void resized() override;

    void setValues(float masterGain, float attack, float decay, float sustain, float release);

    [[nodiscard]] float getMasterGain() const;
    [[nodiscard]] float getAttack() const;
    [[nodiscard]] float getDecay() const;
    [[nodiscard]] float getSustain() const;
    [[nodiscard]] float getRelease() const;

    std::function<void()> onValuesChanged;
    std::function<void(const juce::String&)> onPresetChanged;
    std::function<void()> onSaveAsNewPresetRequested;
    std::function<void()> onRenamePresetRequested;
    std::function<void()> onDeletePresetRequested;
    std::function<void()> onRecordClicked;
    std::function<void()> onPlayClicked;
    std::function<void()> onStopClicked;
    std::function<void()> onBackToStartClicked;
    std::function<void()> onExportMidiClicked;
    std::function<void()> onExportWavClicked;
    std::function<void()> onImportMidiClicked;
    std::function<void()> onSavePerformanceClicked;
    std::function<void()> onOpenPerformanceClicked;
    std::function<void()> onRecentFilesClicked;
    std::function<void(double)> onPlaybackSpeedChange;

    void setPresets(const juce::StringArray& presetIds, const juce::String& currentPresetId,
                    const juce::StringArray& presetDisplayNames);

    [[nodiscard]] juce::String getSelectedPresetId() const;

    enum class RecordingState { idle, recording, playing };
    struct RecordingControlsState {
        RecordingState state = RecordingState::idle;
        bool hasTake = false;
        bool canExportMidiTake = false;
        bool canExportWavTake = false;
    };

    void setRecordingControlsState(RecordingControlsState state);
    void setPlaybackSpeed(double speed);
    void refreshTexts();
    [[nodiscard]] juce::Rectangle<int> getRecentFilesButtonScreenBounds() const noexcept;

private:
    void updateRecordingActionButtons();
    void updatePresetActionButtons();

    void configureSlider(juce::Slider& slider, juce::Label& label, const juce::String& text, double minimum,
                         double maximum, double interval = 0.001);

    juce::StringArray availablePresetIds;

    juce::Label volumeLabel;
    juce::Label attackLabel;
    juce::Label decayLabel;
    juce::Label sustainLabel;
    juce::Label releaseLabel;
    juce::Label presetLabel;

    juce::Slider volumeSlider;
    juce::Slider attackSlider;
    juce::Slider decaySlider;
    juce::Slider sustainSlider;
    juce::Slider releaseSlider;
    juce::ComboBox presetComboBox;
    juce::TextButton saveAsNewPresetButton { TRANS("Save As New") };
    juce::TextButton renamePresetButton { TRANS("Rename") };
    juce::TextButton deletePresetButton { TRANS("Delete") };

    juce::Label recordStatusLabel;
    juce::TextButton recordButton { TRANS("Record") };
    juce::TextButton playButton { TRANS("Play") };
    juce::TextButton stopButton { TRANS("Stop") };
    juce::TextButton backToStartButton { TRANS("Back") };
    juce::TextButton exportMidiButton { TRANS("Export MIDI") };
    juce::TextButton exportWavButton { TRANS("Export WAV") };
    juce::TextButton importMidiButton { TRANS("Import MIDI") };
    juce::TextButton savePerformanceButton { TRANS("Save") };
    juce::TextButton openPerformanceButton { TRANS("Open") };
    juce::TextButton recentFilesButton { TRANS("Recent") };

    juce::Label playbackSpeedLabel;

    juce::Slider playbackSpeedSlider;

    RecordingControlsState recordingControlsState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlsPanel)
};
