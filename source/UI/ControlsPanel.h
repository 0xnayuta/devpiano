#pragma once

#include <JuceHeader.h>

class ControlsPanel final : public juce::Component
{
public:
    ControlsPanel();
    ~ControlsPanel() override;

    void resized() override;

    void setValues(float masterGain,
                   float attack,
                   float decay,
                   float sustain,
                   float release);

    [[nodiscard]] float getMasterGain() const;
    [[nodiscard]] float getAttack() const;
    [[nodiscard]] float getDecay() const;
    [[nodiscard]] float getSustain() const;
    [[nodiscard]] float getRelease() const;

    std::function<void()> onValuesChanged;
    std::function<void(const juce::String&)> onLayoutChanged;
    std::function<void()> onSaveLayoutRequested;
    std::function<void()> onResetLayoutRequested;
    std::function<void()> onImportLayoutRequested;
    std::function<void()> onRenameLayoutRequested;
    std::function<void()> onDeleteLayoutRequested;
    std::function<void()> onRecordClicked;
    std::function<void()> onPlayClicked;
    std::function<void()> onStopClicked;
    std::function<void()> onBackToStartClicked;
    std::function<void()> onExportMidiClicked;
    std::function<void()> onExportWavClicked;
    std::function<void()> onImportMidiClicked;
    std::function<void()> onSavePerformanceClicked;
    std::function<void()> onOpenPerformanceClicked;
    std::function<void(double)> onPlaybackSpeedChange;

    void setLayouts(const juce::StringArray& layoutIds,
                const juce::String& currentLayoutId,
                const juce::StringArray& layoutDisplayNames);

    [[nodiscard]] juce::String getSelectedLayoutId() const;

    enum class RecordingState { idle, recording, playing };
    struct RecordingControlsState
    {
        RecordingState state = RecordingState::idle;
        bool hasTake = false;
        bool canExportMidiTake = false;
        bool canExportWavTake = false;
    };

    void setRecordingControlsState(RecordingControlsState state);
    void setPlaybackSpeed(double speed);

private:
    [[nodiscard]] static juce::String makeLayoutDisplayName(const juce::String& layoutId);
    void updateRecordingActionButtons();
    void updateLayoutActionButtons();
    [[nodiscard]] double getCurrentPlaybackSpeed() const;

    void configureSlider(juce::Slider& slider,
                         juce::Label& label,
                         const juce::String& text,
                         double minimum,
                         double maximum,
                         double interval = 0.001);

    juce::StringArray availableLayoutIds;

    juce::Label volumeLabel;
    juce::Label attackLabel;
    juce::Label decayLabel;
    juce::Label sustainLabel;
    juce::Label releaseLabel;
    juce::Label layoutLabel;

    juce::Slider volumeSlider;
    juce::Slider attackSlider;
    juce::Slider decaySlider;
    juce::Slider sustainSlider;
    juce::Slider releaseSlider;
    juce::ComboBox layoutComboBox;
    juce::TextButton saveLayoutButton { "Save Layout" };
    juce::TextButton resetLayoutButton { "Reset" };
    juce::TextButton importLayoutButton { "Import" };
    juce::TextButton renameLayoutButton { "Rename" };
    juce::TextButton deleteLayoutButton { "Delete" };

    juce::Label recordStatusLabel;
    juce::TextButton recordButton { "Record" };
    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton backToStartButton { "Back" };
    juce::TextButton exportMidiButton { "Export MIDI" };
    juce::TextButton exportWavButton { "Export WAV" };
    juce::TextButton importMidiButton { "Import MIDI" };
    juce::TextButton savePerformanceButton { "Save" };
    juce::TextButton openPerformanceButton { "Open" };

    juce::Label playbackSpeedLabel { "Speed" };
    juce::TextButton speedDownButton { "-" };
    juce::TextButton speedUpButton { "+" };
    juce::Label playbackSpeedValueLabel { "1.00x" };

    double currentPlaybackSpeed = 1.0;

    RecordingControlsState recordingControlsState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlsPanel)
};
