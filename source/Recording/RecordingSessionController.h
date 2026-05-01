#pragma once

#include <JuceHeader.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>

#include "Recording/RecordingEngine.h"
#include "UI/ControlsPanel.h"

class AudioEngine;
class MainComponent;
struct SettingsModel;

namespace devpiano::exporting
{
enum class ExportFileType;
}

namespace devpiano::recording
{

class RecordingSessionController final
{
public:
    struct RecordingSession
    {
        RecordingTake take;
        bool canExportMidi = false;
        ControlsPanel::RecordingState state = ControlsPanel::RecordingState::idle;

        [[nodiscard]] bool hasTake() const noexcept { return ! take.isEmpty(); }
        [[nodiscard]] bool isRecording() const noexcept { return state == ControlsPanel::RecordingState::recording; }
        [[nodiscard]] bool isPlaying() const noexcept { return state == ControlsPanel::RecordingState::playing; }
        [[nodiscard]] bool isIdle() const noexcept { return state == ControlsPanel::RecordingState::idle; }
    };

    RecordingSessionController(MainComponent& owner,
                               RecordingEngine& recordingEngine,
                               AudioEngine& audioEngine,
                               SettingsModel& appSettings,
                               ControlsPanel& controlsPanel);
    ~RecordingSessionController();

    void handleRecordClicked();
    void handlePlayClicked();
    void handleStopClicked();
    void handleBackToStartClicked();
    void handleExportMidiClicked();
    void handleExportWavClicked();
    void handleImportMidiClicked();

    // Called from MainComponent::timerCallback() to check if playback ended.
    void checkPlaybackEnded();

private:
    [[nodiscard]] double getCurrentRuntimeSampleRate() const;
    [[nodiscard]] int getCurrentRuntimeBlockSize() const;

    void startInternalRecording(std::size_t expectedEventCapacity);
    [[nodiscard]] RecordingTake stopInternalRecording();
    void startInternalPlayback(const RecordingTake& take);
    [[nodiscard]] RecordingTake stopInternalPlayback();
    void syncRecordingSessionToUi();

    void runExportRecordingFlow(devpiano::exporting::ExportFileType type,
                                std::unique_ptr<juce::FileChooser>& chooser,
                                const juce::String& dialogTitle,
                                const juce::String& filePattern,
                                std::function<bool(const juce::File&)> doExport);

    [[nodiscard]] std::optional<RecordingTake> tryImportMidiFile(const juce::File& file) const;
    void replaceTakeAndStartPlayback(RecordingTake take);

    MainComponent& owner;
    RecordingEngine& recordingEngine;
    AudioEngine& audioEngine;
    SettingsModel& appSettings;
    ControlsPanel& controlsPanel;

    RecordingSession recordingSession;

    std::unique_ptr<juce::FileChooser> exportMidiChooser;
    std::unique_ptr<juce::FileChooser> exportWavChooser;
    std::unique_ptr<juce::FileChooser> importMidiChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecordingSessionController)
};

} // namespace devpiano::recording
