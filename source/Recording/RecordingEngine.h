#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <limits>
#include <vector>

#include "Audio/RealtimeExchange.h"
#include "Recording/AbLoopEngine.h"
#include "Recording/RecordedPreset.h"

namespace devpiano::recording {
enum class RecordingEventSource : std::uint8_t { computerKeyboard, realtimeMidiBuffer, playback };

enum class RecordingState : std::uint8_t {
    idle,
    recording,
    recordingPaused,
    playing,
    playingPaused,
    stopped,
    countingIn
};

enum class PerformanceEventType : uint8_t { midi = 0, presetChange = 1 };

struct PerformanceEvent {
    std::int64_t timestampSamples = 0;
    PerformanceEventType type = PerformanceEventType::midi;
    std::uint32_t presetId = 0;
    RecordingEventSource source = RecordingEventSource::computerKeyboard;
    juce::MidiMessage message; // meaningful only when type == midi
};
struct RecordingTake {
    double sampleRate = 0.0;
    std::int64_t lengthSamples = 0;
    std::vector<PerformanceEvent> events;
    std::vector<RecordedPreset> presets;

    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] double durationSeconds() const noexcept;
};

struct PendingPresetChange {
    std::uint32_t presetId = 0;
    const RecordedPreset* snapshot = nullptr;
};

struct ScheduledPresetChange {
    std::uint32_t presetId = 0;
    int sampleOffset = 0;
    std::size_t midiEventCount = 0;
};

class RecordingEngine {
public:
    struct TransportCommandResult {
        bool seekApplied = false;
        bool speedChanged = false;
        bool stopApplied = false;
    };

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
    void armRecording(double sampleRate);
    void startArmedRecording() noexcept;
    void cancelArmedRecording() noexcept;
    void prepareForAudioDevice(double sampleRate) noexcept;
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
                               std::int64_t blockStartSamples, int firstSample = 0);

    void recordPresetChange(const RecordedPreset& preset);

    void startPlayback(const RecordingTake& take, double currentSampleRate, std::int64_t resumeFromSamples = 0);
    void startPlaybackAtTakeSample(const RecordingTake& take, double currentSampleRate,
                                   std::int64_t resumeFromTakeSamples);
    void requestPlaybackSeek(std::int64_t takeSample) noexcept;
    [[nodiscard]] bool getPendingPlaybackSeekSample(std::int64_t& takeSample) const noexcept;
    [[nodiscard]] TransportCommandResult applyPendingTransportCommands(juce::MidiBuffer& midiBuffer) noexcept;
    void applyPendingTransportCommandsQuiescent() noexcept;
    void setPlaybackLoopStartSample(std::int64_t sample) noexcept;
    void setPlaybackLoopEndSample(std::int64_t sample) noexcept;
    void clearPlaybackLoop() noexcept;
    [[nodiscard]] AbLoopRange getPlaybackLoopRange() const noexcept;
    [[nodiscard]] std::int64_t getPlaybackPositionInTakeSamples() const noexcept;
    [[nodiscard]] std::int64_t getPlaybackTakeLengthSamples() const noexcept;
    [[nodiscard]] std::uint64_t getPlaybackGeneration() const noexcept;
    [[nodiscard]] std::size_t getPlaybackNoteOnCount() const noexcept;
    [[nodiscard]] bool needsPlaybackRender() const noexcept;
    // Pauses active playback at the current position (retained for resume).
    void pausePlayback();
    // Pauses / resumes an active recording. The recording timeline freezes while
    // paused (events are not captured, position does not advance); live keyboard
    // performance keeps sounding through AudioEngine.
    void pauseRecording();
    void resumeRecording();
    void requestPlaybackStop() noexcept;
    void stopPlaybackQuiescent() noexcept;
    // Sets the playback speed multiplier. Affects the next startPlayback or publishes
    // a transport command applied at the next audio block boundary. Values: 0.5 to 2.0.
    void setPlaybackSpeedMultiplier(double multiplier) noexcept;
    [[nodiscard]] double getPlaybackSpeedMultiplier() const noexcept;
    [[nodiscard]] double getEffectivePlaybackSpeedMultiplier() const noexcept;
    // Renders playback events whose scaled timestamp falls within [blockStartSamples, blockStartSamples + numSamples).
    // Uses the same midiBuffer that AudioEngine will then pass to plugin/synth rendering.
    void renderPlaybackBlock(juce::MidiBuffer& midiBuffer, std::int64_t blockStartSamples, int numSamples);
    void setPlaybackBlockSize(int blockSize) noexcept;
    void advancePlaybackPosition(std::int64_t numSamples) noexcept;
    [[nodiscard]] bool consumePlaybackEndedFlag() noexcept;
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] std::int64_t getPlaybackPositionSamples() const noexcept;
    [[nodiscard]] std::vector<PendingPresetChange> drainPendingPresetChanges();
    [[nodiscard]] const RecordedPreset* getPlaybackPreset(std::uint32_t presetId) const noexcept;
    [[nodiscard]] const std::vector<ScheduledPresetChange>& getScheduledPresetChanges() const noexcept;
    [[nodiscard]] std::size_t getPlaybackMidiCapacityBytes() const noexcept;
    [[nodiscard]] std::size_t consumePresetNotificationCoalescedCount() noexcept;

private:
    struct PlaybackChannelState {
        std::array<std::int16_t, 128> controllers;
        int bankMsb = 0;
        int bankLsb = 0;
        int programBankMsb = 0;
        int programBankLsb = 0;
        int program = 0;
        int pitch = 8192;
        int pressure = 0;
    };

    struct ScaledLoopRange {
        std::int64_t startSamples = 0;
        std::int64_t endSamples = 0;
        bool active = false;
    };

    [[nodiscard]] std::int64_t getScaledPlaybackLengthSamples() const noexcept;
    [[nodiscard]] ScaledLoopRange getScaledLoopRange(double combinedRatio) const noexcept;
    [[nodiscard]] bool tryGetScaledLoopRange(double combinedRatio, ScaledLoopRange& output) const noexcept;
    [[nodiscard]] bool readPendingPlaybackSeek(std::int64_t& takeSample, std::uint32_t& sequence) const noexcept;
    [[nodiscard]] bool readPendingSpeedChange(double& newSpeed, std::uint32_t& sequence) const noexcept;
    [[nodiscard]] bool readPendingStop(std::uint32_t& sequence) const noexcept;
    void resetPlaybackEventCursor(std::int64_t positionSamples, double combinedRatio) noexcept;
    void renderPlaybackEventsInRange(juce::MidiBuffer& midiBuffer, std::int64_t rangeStartSamples,
                                     std::int64_t rangeEndSamples, int segmentOffset, int numSamples,
                                     double combinedRatio);
    void addAllNotesOffMessages(juce::MidiBuffer& midiBuffer, int sampleOffset);
    void restorePlaybackChannelState(juce::MidiBuffer& midiBuffer, int sampleOffset);
    void appendPlaybackMidi(juce::MidiBuffer& midiBuffer, const juce::MidiMessage& message, int sampleOffset);
    void schedulePresetChange(std::uint32_t presetId, int sampleOffset, std::size_t midiEventCount) noexcept;
    void capturePendingPresetChanges(std::int64_t timestampSamples) noexcept;
    void closeCapturedPerformance();
    void updateCapturedPerformance(const juce::MidiMessage& message) noexcept;
    [[nodiscard]] bool isCapacityExhausted(std::int64_t timestamp, bool requiredRelease = false) noexcept;

    AbLoopEngine abLoopEngine;
    std::atomic<std::uint32_t> seekSequence { 0 };
    std::atomic<std::uint32_t> appliedSeekSequence { 0 };
    std::atomic<std::int64_t> requestedSeekSample { 0 };
    // Transport command mailbox
    std::atomic<std::uint32_t> speedSequence { 0 };
    std::atomic<std::uint32_t> appliedSpeedSequence { 0 };
    std::atomic<double> requestedSpeedMultiplier { 1.0 };
    std::atomic<double> effectiveSpeedMultiplier { 1.0 };
    std::atomic<double> targetSpeedMultiplier { 1.0 };

    std::atomic<std::uint32_t> stopSequence { 0 };
    std::atomic<std::uint32_t> appliedStopSequence { 0 };
    std::atomic_bool stopRequested { false };

    RecordingTake currentTake;
    std::size_t recordingEventLimit = 0;
    std::atomic<RecordingState> state { RecordingState::idle };
    std::atomic<std::int64_t> currentPositionSamples { 0 };
    std::atomic<std::size_t> droppedEventCount { 0 };
    double deviceSampleRate = 48000.0;
    long double recordingSampleFraction = 0.0L;
    std::array<std::array<std::size_t, 128>, 16> capturedNotes {};
    std::array<std::array<int, 3>, 16> capturedPedals {};

    // Playback state
    RecordingTake playbackTake;
    std::atomic<double> playbackSampleRateRatio { 1.0 };
    std::atomic<std::int64_t> scaledPlaybackLengthSamples { 0 };
    std::atomic<std::int64_t> playbackPositionSamples { 0 };
    std::atomic<int> playbackBlockSize { 1 };
    std::atomic_bool playbackEndedPending { false };
    std::uint64_t playbackGeneration = 0;
    std::size_t playbackNoteOnCount = 0;
    long double playbackSampleFraction = 0.0L;
    bool playbackCleanupPending = false;
    bool playbackCleanupDelivered = false;
    bool playbackStateRestorePending = false;
    std::array<PlaybackChannelState, 16> restoredChannelState;
    std::size_t restoredStateEventIndex = std::numeric_limits<std::size_t>::max();

    std::size_t playbackEventIndex { 0 };
    ScaledLoopRange lastRenderedLoopRange;
    bool hasRenderedPlaybackBlock = false;
    bool loopWrapPending = false;
    static constexpr auto noPreset = std::numeric_limits<std::uint32_t>::max();
    std::vector<ScheduledPresetChange> scheduledPresetChanges;
    std::size_t playbackMidiCapacityBytes = 131072;
    std::atomic<std::uint32_t> pendingPresetNotification { noPreset };
    std::atomic<std::size_t> presetNotificationCoalescedCount { 0 };
    std::size_t renderedMidiEventCount = 0;
    int lastRenderedMidiSample = 0;
    devpiano::audio::RealtimeQueue<std::uint32_t, 1024> recordedPresetQueue;
    std::uint32_t restoredPresetId = noPreset;

    // Per-channel pitch bend EMA state for playback zipper-noise reduction.
    // Indexed by MIDI channel (0-15). Initialised to 8192.0f (center) in
    // startPlayback. Audio-thread access is gated by isPlaying() with
    // happens-before ordering via the RecordingState atomic.
    std::array<float, 16> smoothedPitchBend {};
};
} // namespace devpiano::recording
