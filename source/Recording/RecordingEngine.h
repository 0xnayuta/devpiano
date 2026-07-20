#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <cstdint>
#include <vector>

namespace devpiano::recording {
enum class RecordingEventSource { computerKeyboard, realtimeMidiBuffer, playback };

enum class RecordingState { idle, recording, playing, stopped };

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
    [[nodiscard]] bool hasDroppedEvents() const noexcept;
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

    void startPlayback(const RecordingTake& take, double currentSampleRate);
    void stopPlayback();
    // Sets the playback speed multiplier. Affects the next startPlayback or immediately
    // if playback is active. Values: 0.5, 0.75, 1.0, 1.25, 1.5, 2.0.
    void setPlaybackSpeedMultiplier(double multiplier) noexcept;
    [[nodiscard]] double getPlaybackSpeedMultiplier() const noexcept;
    // Renders playback events whose scaled timestamp falls within [blockStartSamples, blockStartSamples + numSamples).
    // Uses the same midiBuffer that AudioEngine will then pass to plugin/synth rendering.
    void renderPlaybackBlock(juce::MidiBuffer& midiBuffer, std::int64_t blockStartSamples, int numSamples);
    void advancePlaybackPosition(std::int64_t numSamples) noexcept;
    [[nodiscard]] bool consumePlaybackEndedFlag() noexcept;
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] std::int64_t getPlaybackPositionSamples() const noexcept;
    [[nodiscard]] std::vector<PendingPresetChange> drainPendingPresetChanges();

private:
    [[nodiscard]] std::int64_t getScaledPlaybackLengthSamples() const noexcept;
    [[nodiscard]] bool isCapacityExhausted(std::int64_t timestamp) noexcept;

    RecordingTake currentTake;
    std::atomic<RecordingState> state { RecordingState::idle };
    std::atomic<std::int64_t> currentPositionSamples { 0 };
    std::atomic<std::size_t> droppedEventCount { 0 };

    // Playback state
    RecordingTake playbackTake;
    double playbackSampleRateRatio = 1.0;
    std::atomic<double> playbackSpeedMultiplier { 1.0 };
    std::atomic<std::int64_t> scaledPlaybackLengthSamples { 0 };
    std::atomic<std::int64_t> playbackPositionSamples { 0 };
    std::atomic_bool playbackEndedPending { false };

    // Preset-change notification queue (audio thread → message thread)
    std::vector<PendingPresetChange> pendingPresetChanges;
    juce::CriticalSection presetChangeLock;

    // Per-channel pitch bend EMA state for playback zipper-noise reduction.
    // Indexed by MIDI channel (0-15).  Initialised to 8192.0f (center) in
    // startPlayback / stopPlayback.  Audio-thread access is gated by isPlaying()
    // with happens-before ordering via the RecordingState atomic.
    std::array<float, 16> smoothedPitchBend {};
};
} // namespace devpiano::recording
