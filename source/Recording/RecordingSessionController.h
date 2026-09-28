#pragma once

#include <cstddef>
#include <functional>
#include <juce_core/juce_core.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
#include <optional>

#include "Recording/PerformanceFile.h"
#include "Recording/RecordingEngine.h"
#include "UI/RecordingTypes.h"

class AudioEngine;
class MainComponent;
struct SettingsModel;
class WavExportTask;

namespace devpiano::exporting {
enum class ExportFileType : std::uint8_t;
}

namespace devpiano::recording {

class RecordingSessionController final {
public:
    struct RecordingSession {
        RecordingTake take;
        bool canExportMidi = false;
        PerformanceFileMetadata currentMetadata;
        juce::File currentPerformanceFile;
        ui::RecordingState state = ui::RecordingState::idle;
        [[nodiscard]] bool hasTake() const noexcept {
            return !take.isEmpty();
        }
        // "Recording flow active": includes the paused-recording state so import /
        // back-to-start guards treat a paused recording as still recording.
        [[nodiscard]] bool isRecording() const noexcept {
            return state == ui::RecordingState::recording || state == ui::RecordingState::recordingPaused;
        }
        // "Playback flow active": includes the paused-playback state.
        [[nodiscard]] bool isPlaying() const noexcept {
            return state == ui::RecordingState::playing || state == ui::RecordingState::playingPaused;
        }
        [[nodiscard]] bool isIdle() const noexcept {
            return state == ui::RecordingState::idle;
        }
    };
    struct PlaybackTimelineSnapshot {
        std::int64_t positionSamples = 0;
        std::int64_t lengthSamples = 0;
        double sampleRate = 0.0;
        AbLoopRange loopRange;
        bool enabled = false;
    };

    RecordingSessionController(MainComponent& owner, RecordingEngine& recordingEngine, AudioEngine& audioEngine,
                               SettingsModel& appSettings);
    ~RecordingSessionController();

    void handleRecordClicked();
    void handlePlayClicked();
    void handleStopClicked();
    void handleBackToStartClicked();
    void handleExportMidiClicked();
    void handleExportWavClicked();
    void handleImportMidiClicked();
    void handleSavePerformanceClicked();
    void handleOpenPerformanceClicked();
    void handleSongInfoClicked();
    void handleOpenPerformanceFile(const juce::File& file);
    void handleImportMidiFile(const juce::File& file);
    void handlePlaybackSpeedChange(double speed);
    void seekPlaybackToSample(std::int64_t takeSample);
    void setPlaybackLoopStart();
    void setPlaybackLoopEnd();
    void clearPlaybackLoop();
    [[nodiscard]] PlaybackTimelineSnapshot getPlaybackTimelineSnapshot() const noexcept;

    // Called from MainComponent::timerCallback() to check if playback ended.
    void checkPlaybackEnded();
    std::function<void(const juce::File&)> onFileOpened;

private:
    [[nodiscard]] double getCurrentRuntimeSampleRate() const;
    [[nodiscard]] int getCurrentRuntimeBlockSize() const;

    void startInternalRecording(std::size_t expectedEventCapacity);
    [[nodiscard]] RecordingTake stopInternalRecording();
    void startInternalPlayback(const RecordingTake& take, std::int64_t resumeFromTakeSamples = 0);
    void stopInternalPlayback();
    void syncRecordingSessionToUi();
    void checkCountIn();
    bool cancelCountIn(bool notifyUser = false);

    void runExportRecordingFlow(devpiano::exporting::ExportFileType type, std::unique_ptr<juce::FileChooser>& chooser,
                                const juce::String& dialogTitle, const juce::String& filePattern,
                                std::function<bool(const juce::File&)> doExport);

    void runImportOpenFlow(const juce::String& logPrefix, const juce::String& dialogTitle, const juce::File& startDir,
                           const juce::String& filePattern, std::unique_ptr<juce::FileChooser>& chooser,
                           std::function<std::optional<RecordingTake>(const juce::File&)> loadTake);

    [[nodiscard]] std::optional<RecordingTake> tryImportMidiFile(const juce::File& file);
    void replaceTakeAndStartPlayback(RecordingTake take);

    MainComponent& owner;
    RecordingEngine& recordingEngine;
    AudioEngine& audioEngine;
    SettingsModel& appSettings;

    RecordingSession recordingSession;
    std::optional<std::int64_t> idleSeekPositionSamples;
    // aliveFlag_ shared with async lambdas so they can detect destruction
    std::shared_ptr<bool> aliveFlag_;
    int countInRemainingBeats = 0;
    std::uint32_t lastCountInSequence = 0;

    std::unique_ptr<juce::FileChooser> exportMidiChooser;
    std::unique_ptr<juce::FileChooser> exportWavChooser;
    std::unique_ptr<juce::FileChooser> importMidiChooser;
    std::unique_ptr<juce::FileChooser> performanceFileChooser;
    std::unique_ptr<WavExportTask> activeWavExportTask;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecordingSessionController)
};

} // namespace devpiano::recording
