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
        std::uint64_t takeGeneration = 0;

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

        void detachForNewRecording() {
            take = {};
            canExportMidi = false;
            currentPerformanceFile = juce::File();
            currentMetadata = PerformanceFileMetadata();
            ++takeGeneration;
        }

        void commitRecordedTake(RecordingTake newTake) {
            take = std::move(newTake);
            canExportMidi = !take.isEmpty();
            currentPerformanceFile = juce::File();
            ++takeGeneration;
        }

        bool commitImportedMidi(RecordingTake newTake, const juce::String& songTitle) {
            if (newTake.isEmpty()) {
                return false;
            }
            take = std::move(newTake);
            canExportMidi = false;
            currentPerformanceFile = juce::File();
            currentMetadata = PerformanceFileMetadata();
            currentMetadata.title = songTitle;
            ++takeGeneration;
            return true;
        }

        bool openFromFile(const juce::File& file) {
            auto newTake = loadPerformanceFile(file);
            if (!newTake.has_value() || newTake->isEmpty()) {
                return false;
            }
            auto metadata = loadPerformanceFileMetadata(file);
            if (!metadata.has_value()) {
                return false;
            }
            if (metadata->title.isEmpty()) {
                metadata->title = file.getFileNameWithoutExtension();
            }
            take = std::move(*newTake);
            canExportMidi = false;
            currentPerformanceFile = file;
            currentMetadata = std::move(*metadata);
            ++takeGeneration;
            return true;
        }

        bool saveToFile(const juce::File& file, std::uint64_t expectedGeneration) {
            if (takeGeneration != expectedGeneration || take.isEmpty() || file == juce::File()) {
                return false;
            }
            auto metadata = currentMetadata;
            metadata.createdAt = juce::Time::getCurrentTime().toISO8601(true);
            if (!savePerformanceFile(take, file, metadata)) {
                return false;
            }
            currentPerformanceFile = file;
            currentMetadata = std::move(metadata);
            ++takeGeneration;
            return true;
        }

        bool updateMetadata(juce::String title, juce::String notes, std::uint64_t expectedGeneration) {
            if (takeGeneration != expectedGeneration) {
                return false;
            }
            auto metadata = currentMetadata;
            metadata.title = std::move(title);
            metadata.notes = std::move(notes);
            if (currentPerformanceFile != juce::File() && hasTake()) {
                if (metadata.createdAt.isEmpty()) {
                    metadata.createdAt = juce::Time::getCurrentTime().toISO8601(true);
                }
                if (!savePerformanceFile(take, currentPerformanceFile, metadata)) {
                    return false;
                }
            }
            currentMetadata = std::move(metadata);
            return true;
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

    [[nodiscard]] bool prepareForShutdown();
    // Called from MainComponent::timerCallback() to check if playback ended.
    void checkPlaybackEnded();
    std::function<void(const juce::File&)> onFileOpened;

private:
    struct PausedPlaybackCursor {
        std::int64_t scaledPositionSamples = 0;
        double combinedRatio = 1.0;
    };

    [[nodiscard]] double getCurrentRuntimeSampleRate() const;
    [[nodiscard]] int getCurrentRuntimeBlockSize() const;

    void startInternalRecording(std::size_t expectedEventCapacity);
    [[nodiscard]] RecordingTake stopInternalRecording();
    void startInternalPlayback(const RecordingTake& take, std::int64_t resumeFromTakeSamples = 0,
                               std::optional<PausedPlaybackCursor> pausedCursor = std::nullopt);
    void stopInternalPlayback();
    void syncRecordingSessionToUi();
    void checkCountIn();
    bool cancelCountIn(bool notifyUser = false);

    void runExportRecordingFlow(devpiano::exporting::ExportFileType type, std::unique_ptr<juce::FileChooser>& chooser,
                                const juce::String& dialogTitle, const juce::String& filePattern,
                                std::function<bool(const juce::File&)> doExport);

    void runImportOpenFlow(const juce::String& logPrefix, const juce::String& dialogTitle, const juce::File& startDir,
                           const juce::String& filePattern, std::unique_ptr<juce::FileChooser>& chooser,
                           std::function<bool(const juce::File&)> loadAndCommit);

    bool commitImportedMidiFile(const juce::File& file);
    bool commitOpenedPerformanceFile(const juce::File& file);
    void startPlaybackAfterTakeCommitted();

    MainComponent& owner;
    RecordingEngine& recordingEngine;
    AudioEngine& audioEngine;
    SettingsModel& appSettings;

    RecordingSession recordingSession;
    std::optional<std::int64_t> idleSeekPositionSamples;
    std::optional<PausedPlaybackCursor> pausedPlaybackCursor;
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
