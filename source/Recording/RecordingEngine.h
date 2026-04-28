#pragma once

#include <JuceHeader.h>

#include <cstdint>
#include <vector>

namespace devpiano::recording
{
enum class RecordingEventSource
{
    computerKeyboard,
    externalMidi,
    realtimeMidiBuffer,
    playback
};

enum class RecordingState
{
    idle,
    recording,
    playing,
    stopped
};

struct PerformanceEvent
{
    std::int64_t timestampSamples = 0;
    RecordingEventSource source = RecordingEventSource::computerKeyboard;
    juce::MidiMessage message;
};

struct RecordingTake
{
    double sampleRate = 0.0;
    std::int64_t lengthSamples = 0;
    std::vector<PerformanceEvent> events;

    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] double durationSeconds() const noexcept;
};

class RecordingEngine
{
public:
    // Minimal M6-1 lifecycle/model skeleton.
    // This class is not thread-safe. Future audio-thread integration must provide
    // an explicit ownership/synchronisation boundary and preallocate event storage
    // before calling recordEvent*() from any realtime path. Preallocation only
    // avoids vector growth while capacity is sufficient; large MIDI messages may
    // still allocate when copied into juce::MidiMessage.
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
    void recordEvent(const juce::MidiMessage& message,
                     RecordingEventSource source,
                     std::int64_t timestampSamples);
    void recordEventAtCurrentPosition(const juce::MidiMessage& message,
                                      RecordingEventSource source);
    // Converts block-local MidiBuffer sample offsets into absolute timestampSamples.
    // The copied MidiMessage timestamp is normalised to 0.0; PerformanceEvent::timestampSamples
    // is the only authoritative timeline value stored by RecordingEngine. Events are dropped
    // before MidiMessage materialisation when capacity is exhausted or the message is too large
    // for the first realtime-safe recording path.
    void recordMidiBufferBlock(const juce::MidiBuffer& midiBuffer,
                               RecordingEventSource source,
                               std::int64_t blockStartSamples);

    void startPlayback(const RecordingTake& take, double currentSampleRate);
    void stopPlayback();
    // Renders playback events whose scaled timestamp falls within [blockStartSamples, blockStartSamples + numSamples).
    // Uses the same midiBuffer that AudioEngine will then pass to plugin/synth rendering.
    void renderPlaybackBlock(juce::MidiBuffer& midiBuffer,
                            std::int64_t blockStartSamples,
                            int numSamples);
    void advancePlaybackPosition(std::int64_t numSamples) noexcept;
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] std::int64_t getPlaybackPositionSamples() const noexcept;

    void setOnPlaybackEnded(std::function<void()> callback);

private:
    RecordingTake currentTake;
    RecordingState state = RecordingState::idle;
    std::int64_t currentPositionSamples = 0;
    std::size_t droppedEventCount = 0;

    // Playback state
    RecordingTake playbackTake;
    double playbackSampleRateRatio = 1.0;
    std::int64_t playbackPositionSamples = 0;
    std::function<void()> onPlaybackEnded;
};
} // namespace devpiano::recording
