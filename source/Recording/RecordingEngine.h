#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <vector>

#include "Recording/AbLoopEngine.h"

namespace devpiano::recording {
enum class RecordingEventSource : std::uint8_t { computerKeyboard, realtimeMidiBuffer, playback };

enum class RecordingState : std::uint8_t { idle, recording, recordingPaused, playing, playingPaused, stopped };

enum class PerformanceEventType : uint8_t { midi = 0, presetChange = 1 };

struct PerformanceEvent {
    std::int64_t timestampSamples = 0;
    PerformanceEventType type = PerformanceEventType::midi;
    uint8_t presetId = 0; // meaningful only when type == presetChange
    RecordingEventSource source = RecordingEventSource::computerKeyboard;
    juce::MidiMessage message; // meaningful only when type == midi
};
struct RecordingTake {
    double sampleRate = 0.0;
    std::int64_t lengthSamples = 0;
    std::vector<PerformanceEvent> events;

    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] double durationSeconds() const noexcept;
};

struct PendingPresetChange {
    uint8_t presetId;
};

class RecordingEngine {
public:
    // M6 MVP recording/playback model. Message-thread code owns structural changes
    // such as start/stop/clear/reserve while audio-thread code may record or render
    // preallocated MIDI events during active recording/playback.
    // Keep the audio-thread path bounded: no file IO, UI calls, waits, or vector
    // growth. Playback completion is reported via a lightweight atomic flag and
    // must be consumed from the message thread.
    [[nodiscard]] RecordingState getState() const noexcept;
    [[nodiscard]] bool isRecording() const noexcept;
    [[nodiscard]] bool hasTake() const noexcept;
    [[nodiscard]] std::size_t getDroppedEventCount() const noexcept;
    [[nodiscard]] std::size_t getReservedEventCapacity() const noexcept;
    [[nodiscard]] std::int64_t getCurrentPositionSamples() const noexcept;
    [[nodiscard]] double getSampleRate() const noexcept;
    [[nodiscard]] const RecordingTake& getCurrentTake() const noexcept;
    [[nodiscard]] RecordingTake createTakeSnapshot() const;

    void reserveEvents(std::size_t expectedEventCount);
    void startRecording(double sampleRate);
    RecordingTake stopRecording();
    void clear();
    void advanceRecordingPosition(std::int64_t numSamples) noexcept;
    void recordEvent(const juce::MidiMessage& message, RecordingEventSource source, std::int64_t timestampSamples);
    // Converts block-local MidiBuffer sample offsets into absolute timestampSamples.
    // The copied MidiMessage timestamp is normalised to 0.0; PerformanceEvent::timestampSamples
    // is the only authoritative timeline value stored by RecordingEngine. Events are dropped
    // before MidiMessage materialisation when capacity is exhausted or the message is too large
    // for the first realtime-safe recording path.
    void recordMidiBufferBlock(const juce::MidiBuffer& midiBuffer, RecordingEventSource source,
                               std::int64_t blockStartSamples);

    // Records a preset-change event at the current recording position.
    // presetId is a 0-based index into the preset list.
    void recordPresetChange(uint8_t presetId, std::int64_t timestampSamples);

    void startPlayback(const RecordingTake& take, double currentSampleRate, std::int64_t resumeFromSamples = 0);
    void startPlaybackAtTakeSample(const RecordingTake& take, double currentSampleRate,
                                   std::int64_t resumeFromTakeSamples);
    void requestPlaybackSeek(std::int64_t takeSample) noexcept;
    [[nodiscard]] bool getPendingPlaybackSeekSample(std::int64_t& takeSample) const noexcept;
    [[nodiscard]] bool applyPendingPlaybackSeek(juce::MidiBuffer& midiBuffer) noexcept;
    void setPlaybackLoopStartSample(std::int64_t sample) noexcept;
    void setPlaybackLoopEndSample(std::int64_t sample) noexcept;
    void clearPlaybackLoop() noexcept;
    [[nodiscard]] AbLoopRange getPlaybackLoopRange() const noexcept;
    [[nodiscard]] std::int64_t getPlaybackPositionInTakeSamples() const noexcept;
    [[nodiscard]] std::int64_t getPlaybackTakeLengthSamples() const noexcept;
    // Pauses active playback at the current position (retained for resume).
    void pausePlayback();
    // Pauses / resumes an active recording. The recording timeline freezes while
    // paused (events are not captured, position does not advance); live keyboard
    // performance keeps sounding through AudioEngine.
    void pauseRecording();
    void resumeRecording();
    void stopPlayback();
    // Sets the playback speed multiplier. Affects the next startPlayback or immediately
    // if playback is active. Values: 0.5, 0.75, 1.0, 1.25, 1.5, 2.0.
    void setPlaybackSpeedMultiplier(double multiplier) noexcept;
    [[nodiscard]] double getPlaybackSpeedMultiplier() const noexcept;
    // Renders playback events whose scaled timestamp falls within [blockStartSamples, blockStartSamples + numSamples).
    // Uses the same midiBuffer that AudioEngine will then pass to plugin/synth rendering.
    void renderPlaybackBlock(juce::MidiBuffer& midiBuffer, std::int64_t blockStartSamples, int numSamples);
    void setPlaybackBlockSize(int blockSize) noexcept;
    void advancePlaybackPosition(std::int64_t numSamples) noexcept;
    [[nodiscard]] bool consumePlaybackEndedFlag() noexcept;
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] std::int64_t getPlaybackPositionSamples() const noexcept;
    [[nodiscard]] std::vector<PendingPresetChange> drainPendingPresetChanges();

private:
    struct ScaledLoopRange {
        std::int64_t startSamples = 0;
        std::int64_t endSamples = 0;
        bool active = false;
    };

    [[nodiscard]] std::int64_t getScaledPlaybackLengthSamples() const noexcept;
    [[nodiscard]] ScaledLoopRange getScaledLoopRange(double combinedRatio) const noexcept;
    [[nodiscard]] bool readPendingPlaybackSeek(std::int64_t& takeSample, std::uint32_t& sequence) const noexcept;
    void resetPlaybackEventCursor(std::int64_t positionSamples, double combinedRatio) noexcept;
    void renderPlaybackEventsInRange(juce::MidiBuffer& midiBuffer, std::int64_t rangeStartSamples,
                                     std::int64_t rangeEndSamples, int segmentOffset, int numSamples,
                                     double combinedRatio);
    void addAllNotesOffMessages(juce::MidiBuffer& midiBuffer, int sampleOffset);
    [[nodiscard]] bool isCapacityExhausted(std::int64_t timestamp) noexcept;

    AbLoopEngine abLoopEngine;
    std::atomic<std::uint32_t> seekSequence { 0 };
    std::atomic<std::uint32_t> appliedSeekSequence { 0 };
    std::atomic<std::int64_t> requestedSeekSample { 0 };

    RecordingTake currentTake;
    std::atomic<RecordingState> state { RecordingState::idle };
    std::atomic<std::int64_t> currentPositionSamples { 0 };
    std::atomic<std::size_t> droppedEventCount { 0 };

    // Playback state
    RecordingTake playbackTake;
    std::atomic<double> playbackSampleRateRatio { 1.0 };
    std::atomic<double> playbackSpeedMultiplier { 1.0 };
    std::atomic<std::int64_t> scaledPlaybackLengthSamples { 0 };
    std::atomic<std::int64_t> playbackPositionSamples { 0 };
    std::atomic<int> playbackBlockSize { 1 };
    std::atomic_bool playbackEndedPending { false };

    std::size_t playbackEventIndex { 0 };
    ScaledLoopRange lastRenderedLoopRange;
    bool hasRenderedPlaybackBlock = false;
    bool loopWrapPending = false;
    // Preset-change notification queue (audio thread → message thread)
    std::vector<PendingPresetChange> pendingPresetChanges;
    // Preset-change events recorded from the message thread during active
    // recording.  Separate from currentTake.events (audio-thread writes) to
    // avoid concurrent vector push_back data races (see REC-003).  Merged
    // into currentTake.events at stopRecording / clear time on the message
    // thread — no lock needed, single-threaded access.
    std::vector<PerformanceEvent> pendingPresetEvents;

    juce::CriticalSection presetChangeLock;

    // Per-channel pitch bend EMA state for playback zipper-noise reduction.
    // Indexed by MIDI channel (0-15). Initialised to 8192.0f (center) in
    // startPlayback. Audio-thread access is gated by isPlaying() with
    // happens-before ordering via the RecordingState atomic.
    std::array<float, 16> smoothedPitchBend {};
};
} // namespace devpiano::recording
