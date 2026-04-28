#include "Recording/RecordingEngine.h"

#include <algorithm>

namespace devpiano::recording
{
namespace
{
constexpr auto maxRealtimeMidiMessageBytes = 16;
}

bool RecordingTake::isEmpty() const noexcept
{
    return events.empty();
}

double RecordingTake::durationSeconds() const noexcept
{
    if (sampleRate <= 0.0 || lengthSamples <= 0)
        return 0.0;

    return static_cast<double>(lengthSamples) / sampleRate;
}

RecordingState RecordingEngine::getState() const noexcept
{
    return state;
}

bool RecordingEngine::isRecording() const noexcept
{
    return state == RecordingState::recording;
}

bool RecordingEngine::hasTake() const noexcept
{
    return !currentTake.isEmpty();
}

bool RecordingEngine::hasDroppedEvents() const noexcept
{
    return droppedEventCount > 0;
}

std::size_t RecordingEngine::getDroppedEventCount() const noexcept
{
    return droppedEventCount;
}

std::size_t RecordingEngine::getReservedEventCapacity() const noexcept
{
    return currentTake.events.capacity();
}

std::int64_t RecordingEngine::getCurrentPositionSamples() const noexcept
{
    return currentPositionSamples;
}

double RecordingEngine::getSampleRate() const noexcept
{
    return currentTake.sampleRate;
}

const RecordingTake& RecordingEngine::getCurrentTake() const noexcept
{
    return currentTake;
}

RecordingTake RecordingEngine::createTakeSnapshot() const
{
    return currentTake;
}

void RecordingEngine::reserveEvents(std::size_t expectedEventCount)
{
    currentTake.events.reserve(expectedEventCount);
}

void RecordingEngine::startRecording(double sampleRate)
{
    currentTake.events.clear();
    currentTake.sampleRate = std::max(sampleRate, 0.0);
    currentTake.lengthSamples = 0;
    currentPositionSamples = 0;
    droppedEventCount = 0;
    state = RecordingState::recording;
}

RecordingTake RecordingEngine::stopRecording()
{
    if (isRecording())
        currentTake.lengthSamples = std::max(currentTake.lengthSamples, currentPositionSamples);

    state = RecordingState::stopped;
    return currentTake;
}

void RecordingEngine::clear()
{
    currentTake.events.clear();
    currentTake.sampleRate = 0.0;
    currentTake.lengthSamples = 0;
    currentPositionSamples = 0;
    droppedEventCount = 0;
    state = RecordingState::idle;
}

void RecordingEngine::advanceRecordingPosition(std::int64_t numSamples) noexcept
{
    if (!isRecording() || numSamples <= 0)
        return;

    currentPositionSamples += numSamples;
    currentTake.lengthSamples = std::max(currentTake.lengthSamples, currentPositionSamples);
}

void RecordingEngine::recordEvent(const juce::MidiMessage& message,
                                  RecordingEventSource source,
                                  std::int64_t timestampSamples)
{
    if (!isRecording())
        return;

    const auto clampedTimestamp = std::max<std::int64_t>(timestampSamples, 0);
    if (currentTake.events.size() >= currentTake.events.capacity())
    {
        ++droppedEventCount;
        currentTake.lengthSamples = std::max(currentTake.lengthSamples, clampedTimestamp);
        return;
    }

    currentTake.events.push_back({ clampedTimestamp, source, message });
    currentTake.lengthSamples = std::max(currentTake.lengthSamples, clampedTimestamp);
}

void RecordingEngine::recordEventAtCurrentPosition(const juce::MidiMessage& message,
                                                   RecordingEventSource source)
{
    recordEvent(message, source, currentPositionSamples);
}

void RecordingEngine::recordMidiBufferBlock(const juce::MidiBuffer& midiBuffer,
                                            RecordingEventSource source,
                                            std::int64_t blockStartSamples)
{
    if (!isRecording())
        return;

    const auto clampedBlockStart = std::max<std::int64_t>(blockStartSamples, 0);

    for (const auto metadata : midiBuffer)
    {
        const auto timestamp = clampedBlockStart + std::max(metadata.samplePosition, 0);
        if (currentTake.events.size() >= currentTake.events.capacity()
            || metadata.numBytes > maxRealtimeMidiMessageBytes)
        {
            ++droppedEventCount;
            currentTake.lengthSamples = std::max(currentTake.lengthSamples, timestamp);
            continue;
        }

        auto message = metadata.getMessage();
        message.setTimeStamp(0.0);
        recordEvent(message, source, timestamp);
    }
}

void RecordingEngine::startPlayback(const RecordingTake& take, double currentSampleRate)
{
    playbackTake = take;
    playbackSampleRateRatio = (take.sampleRate > 0.0 && currentSampleRate > 0.0)
                                  ? (currentSampleRate / take.sampleRate)
                                  : 1.0;
    playbackPositionSamples = 0;
    state = RecordingState::playing;
}

void RecordingEngine::stopPlayback()
{
    state = RecordingState::stopped;
    playbackPositionSamples = 0;
}

void RecordingEngine::renderPlaybackBlock(juce::MidiBuffer& midiBuffer,
                                          std::int64_t blockStartSamples,
                                          int numSamples)
{
    if (!isPlaying())
        return;

    const auto blockEndSamples = blockStartSamples + static_cast<std::int64_t>(numSamples);

    for (const auto& event : playbackTake.events)
    {
        const auto scaledTimestamp = static_cast<std::int64_t>(
            static_cast<double>(event.timestampSamples) * playbackSampleRateRatio);

        if (scaledTimestamp < blockStartSamples || scaledTimestamp >= blockEndSamples)
            continue;

        const auto sampleOffset = static_cast<int>(scaledTimestamp - blockStartSamples);
        midiBuffer.addEvent(event.message, juce::jlimit(0, numSamples - 1, sampleOffset));
    }
}

void RecordingEngine::advancePlaybackPosition(std::int64_t numSamples) noexcept
{
    if (!isPlaying() || numSamples <= 0)
        return;

    playbackPositionSamples += numSamples;
    if (playbackPositionSamples >= playbackTake.lengthSamples && onPlaybackEnded)
    {
        state = RecordingState::stopped;
        playbackPositionSamples = playbackTake.lengthSamples;
        onPlaybackEnded();
    }
}

bool RecordingEngine::isPlaying() const noexcept
{
    return state == RecordingState::playing;
}

std::int64_t RecordingEngine::getPlaybackPositionSamples() const noexcept
{
    return playbackPositionSamples;
}

void RecordingEngine::setOnPlaybackEnded(std::function<void()> callback)
{
    onPlaybackEnded = std::move(callback);
}
} // namespace devpiano::recording
