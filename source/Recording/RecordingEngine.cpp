#include "Recording/RecordingEngine.h"

#include "Diagnostics/Log.h"
#include "Diagnostics/MidiTrace.h"

#include <algorithm>
#include <cmath>

namespace devpiano::recording {
namespace {
constexpr auto maxRealtimeMidiMessageBytes = 16;
}

bool RecordingTake::isEmpty() const noexcept {
    return events.empty();
}

double RecordingTake::durationSeconds() const noexcept {
    if (sampleRate <= 0.0 || lengthSamples <= 0) {
        return 0.0;
    }

    return static_cast<double>(lengthSamples) / sampleRate;
}

RecordingState RecordingEngine::getState() const noexcept {
    return state.load(std::memory_order_acquire);
}

bool RecordingEngine::isRecording() const noexcept {
    return state.load(std::memory_order_acquire) == RecordingState::recording;
}

bool RecordingEngine::hasTake() const noexcept {
    // Must not be called during active recording — events vector is being
    // mutated by the audio thread.  Callers should use RecordingSessionController's
    // local-take copy (recordingSession.hasTake()) for during-recording checks.
    jassert(!isRecording());
    return !currentTake.isEmpty();
}

std::size_t RecordingEngine::getDroppedEventCount() const noexcept {
    return droppedEventCount.load(std::memory_order_relaxed);
}

std::int64_t RecordingEngine::getCurrentPositionSamples() const noexcept {
    return currentPositionSamples.load(std::memory_order_relaxed);
}

double RecordingEngine::getSampleRate() const noexcept {
    return currentTake.sampleRate;
}

std::size_t RecordingEngine::getReservedEventCapacity() const noexcept {
    // capacity() is a read-only query on vector metadata — safe even during recording.
    return currentTake.events.capacity();
}

const RecordingTake& RecordingEngine::getCurrentTake() const noexcept {
    // Return an empty take if called during active recording — the audio thread
    // may be mutating the events vector. Callers should use createTakeSnapshot()
    // or stop recording first.
    if (isRecording()) {
        static const RecordingTake empty;
        return empty;
    }
    return currentTake;
}

RecordingTake RecordingEngine::createTakeSnapshot() const {
    // Must not be called during active recording — copies the events vector
    // while the audio thread may be pushing new events into it.
    jassert(!isRecording());
    return currentTake;
}

void RecordingEngine::reserveEvents(std::size_t expectedEventCount) {
    currentTake.events.reserve(expectedEventCount);
}

void RecordingEngine::startRecording(double sampleRate) {
    currentTake.events.clear();
    pendingPresetEvents.clear();
    abLoopEngine.clear();
    currentTake.sampleRate = std::max(sampleRate, 0.0);
    currentTake.lengthSamples = 0;
    currentPositionSamples.store(0, std::memory_order_relaxed);
    droppedEventCount.store(0, std::memory_order_relaxed);
    playbackEndedPending.store(false, std::memory_order_release);
    state.store(RecordingState::recording, std::memory_order_release);

    DP_DEBUG_LOG("[RecordingEngine] recording STARTED");
}

RecordingTake RecordingEngine::stopRecording() {
    // Finalise even when stopping from the paused-recording state (events and
    // length must still be merged/updated).
    const auto recordingActive
        = isRecording() || state.load(std::memory_order_acquire) == RecordingState::recordingPaused;
    if (recordingActive) {
        // Merge pending preset-change events (message-thread writes) into the
        // recorded events vector before finalising the take.
        if (!pendingPresetEvents.empty()) {
            for (auto& ev : pendingPresetEvents) {
                currentTake.events.push_back(std::move(ev));
            }
            pendingPresetEvents.clear();

            std::ranges::stable_sort(currentTake.events,
                                     [](const PerformanceEvent& a, const PerformanceEvent& b) noexcept {
                                         return a.timestampSamples < b.timestampSamples;
                                     });
        }

        currentTake.lengthSamples
            = std::max(currentTake.lengthSamples, currentPositionSamples.load(std::memory_order_relaxed));
    }

    state.store(RecordingState::stopped, std::memory_order_release);

    DP_DEBUG_LOG("[RecordingEngine] recording STOPPED: " + juce::String(currentTake.events.size())
                 + " events, duration=" + juce::String(currentTake.durationSeconds(), 2) + "s");

    return currentTake;
}

void RecordingEngine::clear() {
    currentTake.events.clear();
    pendingPresetEvents.clear();
    currentTake.sampleRate = 0.0;
    currentTake.lengthSamples = 0;
    playbackTake = {};
    currentPositionSamples.store(0, std::memory_order_relaxed);
    playbackPositionSamples.store(0, std::memory_order_relaxed);
    scaledPlaybackLengthSamples.store(0, std::memory_order_relaxed);
    requestedSeekSample.store(0, std::memory_order_relaxed);
    const auto sequence = seekSequence.load(std::memory_order_relaxed);
    appliedSeekSequence.store(sequence, std::memory_order_relaxed);
    droppedEventCount.store(0, std::memory_order_relaxed);
    playbackEventIndex = 0;
    hasRenderedPlaybackBlock = false;
    lastRenderedLoopRange = {};
    loopWrapPending = false;
    abLoopEngine.clear();
    playbackEndedPending.store(false, std::memory_order_release);
    state.store(RecordingState::idle, std::memory_order_release);
}

void RecordingEngine::advanceRecordingPosition(std::int64_t numSamples) noexcept {
    if (!isRecording() || numSamples <= 0) {
        return;
    }

    currentPositionSamples.fetch_add(numSamples, std::memory_order_relaxed);
    currentTake.lengthSamples
        = std::max(currentTake.lengthSamples, currentPositionSamples.load(std::memory_order_relaxed));
}

void RecordingEngine::recordEvent(const juce::MidiMessage& message, RecordingEventSource source,
                                  std::int64_t timestampSamples) {
    if (!isRecording()) {
        return;
    }

    const auto clampedTimestamp = std::max<std::int64_t>(timestampSamples, 0);
    if (isCapacityExhausted(clampedTimestamp)) {
        // 丢弃只计入 droppedEventCount（原子）；stopRecording() 在消息线程
        // 统一输出 dropped 数（ERR-003），实时线程不做任何日志/IO。
        return;
    }

    currentTake.events.push_back({ clampedTimestamp, PerformanceEventType::midi, 0, source, message });
    currentTake.lengthSamples = std::max(currentTake.lengthSamples, clampedTimestamp);
}

void RecordingEngine::recordMidiBufferBlock(const juce::MidiBuffer& midiBuffer, RecordingEventSource source,
                                            std::int64_t blockStartSamples) {
    if (!isRecording()) {
        return;
    }

    const auto clampedBlockStart = std::max<std::int64_t>(blockStartSamples, 0);

    for (const auto metadata : midiBuffer) {
        const auto timestamp = clampedBlockStart + std::max(metadata.samplePosition, 0);
        if (currentTake.events.size() >= currentTake.events.capacity()
            || metadata.numBytes > maxRealtimeMidiMessageBytes) {
            droppedEventCount.fetch_add(1, std::memory_order_relaxed);
            currentTake.lengthSamples = std::max(currentTake.lengthSamples, timestamp);
            continue;
        }

        auto message = metadata.getMessage();
        message.setTimeStamp(0.0);
        recordEvent(message, source, timestamp);
    }
}

// ---- Preset-change recording ----

void RecordingEngine::recordPresetChange(uint8_t presetId, std::int64_t timestampSamples) {
    // Write to a dedicated message-thread queue to avoid racing with the
    // audio thread's writes to currentTake.events via recordMidiBufferBlock.
    // Merged into currentTake.events at stopRecording() or clear() time.
    if (!isRecording()) {
        return;
    }

    const auto ts = std::max<std::int64_t>(timestampSamples, 0);
    pendingPresetEvents.push_back(
        { ts, PerformanceEventType::presetChange, presetId, RecordingEventSource::computerKeyboard, {} });
}

bool RecordingEngine::isCapacityExhausted(std::int64_t timestamp) noexcept {
    if (currentTake.events.size() >= currentTake.events.capacity()) {
        droppedEventCount.fetch_add(1, std::memory_order_relaxed);
        currentTake.lengthSamples = std::max(currentTake.lengthSamples, timestamp);
        return true;
    }
    return false;
}

void RecordingEngine::startPlayback(const RecordingTake& take, double currentSampleRate,
                                    std::int64_t resumeFromSamples) {
    playbackTake = take;
    playbackSampleRateRatio.store(
        (take.sampleRate > 0.0 && currentSampleRate > 0.0) ? (currentSampleRate / take.sampleRate) : 1.0,
        std::memory_order_relaxed);
    scaledPlaybackLengthSamples.store(getScaledPlaybackLengthSamples());
    const auto combinedRatio = playbackSampleRateRatio.load(std::memory_order_relaxed)
        / playbackSpeedMultiplier.load(std::memory_order_relaxed);
    auto initialResume = juce::jlimit<std::int64_t>(0, scaledPlaybackLengthSamples.load(), resumeFromSamples);
    const auto loopRange = getScaledLoopRange(combinedRatio);
    if (loopRange.active && initialResume >= loopRange.endSamples) {
        initialResume = loopRange.startSamples;
    }
    playbackPositionSamples.store(initialResume, std::memory_order_relaxed);
    playbackEndedPending.store(false, std::memory_order_release);
    hasRenderedPlaybackBlock = false;
    lastRenderedLoopRange = loopRange;
    loopWrapPending = false;
    resetPlaybackEventCursor(initialResume, combinedRatio);

    const auto seekVersion = seekSequence.load(std::memory_order_acquire);
    appliedSeekSequence.store(seekVersion, std::memory_order_release);
    {
        juce::CriticalSection::ScopedLockType lock(presetChangeLock);
        pendingPresetChanges.clear();
        std::size_t presetEventCount = 0;
        for (const auto& event : take.events) {
            if (event.type == PerformanceEventType::presetChange) {
                ++presetEventCount;
            }
        }
        pendingPresetChanges.reserve(presetEventCount);
    }

    smoothedPitchBend.fill(8192.0f);
    state.store(RecordingState::playing, std::memory_order_release);

    DP_DEBUG_LOG("[RecordingEngine] playback STARTED: " + juce::String(take.events.size())
                 + " events, ratio=" + juce::String(playbackSampleRateRatio.load(std::memory_order_relaxed))
                 + ", speed=" + juce::String(playbackSpeedMultiplier.load())
                 + ", scaledLen=" + juce::String(scaledPlaybackLengthSamples.load()));
}

void RecordingEngine::startPlaybackAtTakeSample(const RecordingTake& take, double currentSampleRate,
                                                std::int64_t resumeFromTakeSamples) {
    const auto takeSample
        = juce::jlimit<std::int64_t>(0, std::max<std::int64_t>(take.lengthSamples, 0), resumeFromTakeSamples);
    const auto sampleRateRatio
        = (take.sampleRate > 0.0 && currentSampleRate > 0.0) ? currentSampleRate / take.sampleRate : 1.0;
    const auto combinedRatio = sampleRateRatio / playbackSpeedMultiplier.load(std::memory_order_relaxed);
    const auto scaledPosition = static_cast<std::int64_t>(static_cast<double>(takeSample) * combinedRatio);
    startPlayback(take, currentSampleRate, scaledPosition);
}

void RecordingEngine::requestPlaybackSeek(std::int64_t takeSample) noexcept {
    auto observedVersion = seekSequence.load(std::memory_order_relaxed);
    for (;;) {
        if ((observedVersion & 1U) != 0U) {
            observedVersion = seekSequence.load(std::memory_order_relaxed);
            continue;
        }
        if (seekSequence.compare_exchange_weak(observedVersion, observedVersion + 1, std::memory_order_acq_rel,
                                               std::memory_order_relaxed)) {
            break;
        }
    }

    requestedSeekSample.store(std::max<std::int64_t>(takeSample, 0), std::memory_order_relaxed);
    seekSequence.store(observedVersion + 2, std::memory_order_release);
}

bool RecordingEngine::readPendingPlaybackSeek(std::int64_t& takeSample, std::uint32_t& sequence) const noexcept {
    const auto observedVersion = seekSequence.load(std::memory_order_acquire);
    if ((observedVersion & 1U) != 0U || observedVersion == appliedSeekSequence.load(std::memory_order_acquire)) {
        return false;
    }

    const auto requestedSample = requestedSeekSample.load(std::memory_order_relaxed);
    if (seekSequence.load(std::memory_order_acquire) != observedVersion) {
        return false;
    }

    takeSample = requestedSample;
    sequence = observedVersion;
    return true;
}

bool RecordingEngine::getPendingPlaybackSeekSample(std::int64_t& takeSample) const noexcept {
    std::uint32_t sequence = 0;
    if (!readPendingPlaybackSeek(takeSample, sequence)) {
        return false;
    }

    takeSample = juce::jlimit<std::int64_t>(0, std::max<std::int64_t>(playbackTake.lengthSamples, 0), takeSample);
    return true;
}

bool RecordingEngine::applyPendingPlaybackSeek(juce::MidiBuffer& midiBuffer) noexcept {
    std::int64_t takeSample = 0;
    std::uint32_t sequence = 0;
    if (!readPendingPlaybackSeek(takeSample, sequence)) {
        return false;
    }

    takeSample = juce::jlimit<std::int64_t>(0, std::max<std::int64_t>(playbackTake.lengthSamples, 0), takeSample);
    const auto combinedRatio = playbackSampleRateRatio.load(std::memory_order_relaxed)
        / playbackSpeedMultiplier.load(std::memory_order_relaxed);
    const auto scaledPosition = static_cast<std::int64_t>(static_cast<double>(takeSample) * combinedRatio);
    const auto clampedPosition
        = juce::jlimit<std::int64_t>(0, scaledPlaybackLengthSamples.load(std::memory_order_relaxed), scaledPosition);

    playbackPositionSamples.store(clampedPosition, std::memory_order_relaxed);
    resetPlaybackEventCursor(clampedPosition, combinedRatio);
    smoothedPitchBend.fill(8192.0f);
    hasRenderedPlaybackBlock = false;
    loopWrapPending = false;
    addAllNotesOffMessages(midiBuffer, 0);
    appliedSeekSequence.store(sequence, std::memory_order_release);
    return true;
}

void RecordingEngine::setPlaybackLoopStartSample(std::int64_t sample) noexcept {
    abLoopEngine.setStartSample(sample);
}

void RecordingEngine::setPlaybackLoopEndSample(std::int64_t sample) noexcept {
    abLoopEngine.setEndSample(sample);
}

void RecordingEngine::clearPlaybackLoop() noexcept {
    abLoopEngine.clear();
}

AbLoopRange RecordingEngine::getPlaybackLoopRange() const noexcept {
    return abLoopEngine.getRange();
}

std::int64_t RecordingEngine::getPlaybackPositionInTakeSamples() const noexcept {
    if (playbackTake.lengthSamples <= 0) {
        return 0;
    }

    const auto combinedRatio = playbackSampleRateRatio.load(std::memory_order_relaxed)
        / playbackSpeedMultiplier.load(std::memory_order_relaxed);
    if (combinedRatio <= 0.0) {
        return 0;
    }

    const auto takeSample = static_cast<std::int64_t>(
        static_cast<double>(playbackPositionSamples.load(std::memory_order_relaxed)) / combinedRatio);
    return juce::jlimit<std::int64_t>(0, playbackTake.lengthSamples, takeSample);
}

std::int64_t RecordingEngine::getPlaybackTakeLengthSamples() const noexcept {
    return std::max<std::int64_t>(playbackTake.lengthSamples, 0);
}

void RecordingEngine::pausePlayback() {
    if (state.load(std::memory_order_acquire) != RecordingState::playing) {
        return;
    }

    state.store(RecordingState::playingPaused, std::memory_order_release);
    playbackEndedPending.store(false, std::memory_order_release);
    DP_DEBUG_LOG("[RecordingEngine] playback PAUSED at pos=" + juce::String(playbackPositionSamples.load()));
}

void RecordingEngine::pauseRecording() {
    if (state.load(std::memory_order_acquire) != RecordingState::recording) {
        return;
    }

    state.store(RecordingState::recordingPaused, std::memory_order_release);
    DP_DEBUG_LOG("[RecordingEngine] recording PAUSED at pos=" + juce::String(currentPositionSamples.load()));
}

void RecordingEngine::resumeRecording() {
    if (state.load(std::memory_order_acquire) != RecordingState::recordingPaused) {
        return;
    }

    state.store(RecordingState::recording, std::memory_order_release);
    DP_DEBUG_LOG("[RecordingEngine] recording RESUMED at pos=" + juce::String(currentPositionSamples.load()));
}

void RecordingEngine::stopPlayback() {
    {
        juce::CriticalSection::ScopedLockType lock(presetChangeLock);
        pendingPresetChanges.clear();
    }

    const auto seekVersion = seekSequence.load(std::memory_order_acquire);
    appliedSeekSequence.store(seekVersion, std::memory_order_release);
    state.store(RecordingState::stopped, std::memory_order_release);
    playbackEndedPending.store(false, std::memory_order_release);
    playbackEventIndex = 0;
    hasRenderedPlaybackBlock = false;
    loopWrapPending = false;
}

void RecordingEngine::setPlaybackSpeedMultiplier(double multiplier) noexcept {
    const auto clamped = std::clamp(multiplier, 0.5, 2.0);
    const auto oldSpeed = playbackSpeedMultiplier.load();
    playbackSpeedMultiplier.store(clamped);

    const auto currentState = state.load(std::memory_order_acquire);
    const auto hasRetainedPlaybackPosition = currentState == RecordingState::playing
        || currentState == RecordingState::playingPaused
        || (currentState == RecordingState::stopped && scaledPlaybackLengthSamples.load(std::memory_order_relaxed) > 0);
    if (hasRetainedPlaybackPosition) {
        const auto newPosition
            = static_cast<std::int64_t>(static_cast<double>(playbackPositionSamples.load()) * oldSpeed / clamped);
        playbackPositionSamples.store(newPosition);
        scaledPlaybackLengthSamples.store(getScaledPlaybackLengthSamples());
        const auto combinedRatio = playbackSampleRateRatio.load(std::memory_order_relaxed) / clamped;
        if (currentState == RecordingState::playing) {
            resetPlaybackEventCursor(newPosition, combinedRatio);
            hasRenderedPlaybackBlock = false;
        }

        DP_DEBUG_LOG("[RecordingEngine] playback speed updated to " + juce::String(clamped)
                     + ", scaledLen=" + juce::String(scaledPlaybackLengthSamples.load()));
    }
}

double RecordingEngine::getPlaybackSpeedMultiplier() const noexcept {
    return playbackSpeedMultiplier.load();
}
void RecordingEngine::setPlaybackBlockSize(int blockSize) noexcept {
    playbackBlockSize.store(std::max(1, blockSize), std::memory_order_relaxed);
}


void RecordingEngine::renderPlaybackBlock(juce::MidiBuffer& midiBuffer, std::int64_t blockStartSamples,
                                          int numSamples) {
    if (!isPlaying() || numSamples <= 0) {
        hasRenderedPlaybackBlock = false;
        return;
    }

    auto currentBlockSize = playbackBlockSize.load(std::memory_order_relaxed);
    while (numSamples > currentBlockSize
           && !playbackBlockSize.compare_exchange_weak(
               currentBlockSize, numSamples, std::memory_order_relaxed, std::memory_order_relaxed)) {
    }

    const auto combinedRatio = playbackSampleRateRatio.load(std::memory_order_relaxed)
        / playbackSpeedMultiplier.load(std::memory_order_relaxed);
    ScaledLoopRange loopRange;
    if (!tryGetScaledLoopRange(combinedRatio, loopRange)) {
        loopRange = lastRenderedLoopRange;
    }
    auto takePosition = blockStartSamples;
    auto blockOffset = 0;

    if (loopWrapPending) {
        addAllNotesOffMessages(midiBuffer, 0);
        loopWrapPending = false;
        resetPlaybackEventCursor(takePosition, combinedRatio);
    }

    if (loopRange.active && takePosition >= loopRange.endSamples) {
        addAllNotesOffMessages(midiBuffer, 0);
        takePosition = loopRange.startSamples;
        playbackPositionSamples.store(takePosition, std::memory_order_relaxed);
        resetPlaybackEventCursor(takePosition, combinedRatio);
    }

    while (blockOffset < numSamples) {
        const auto remainingSamples = static_cast<std::int64_t>(numSamples - blockOffset);
        const auto rangeEnd = loopRange.active ? std::min(takePosition + remainingSamples, loopRange.endSamples)
                                               : takePosition + remainingSamples;
        renderPlaybackEventsInRange(midiBuffer, takePosition, rangeEnd, blockOffset, numSamples, combinedRatio);

        blockOffset += static_cast<int>(rangeEnd - takePosition);
        takePosition = rangeEnd;
        if (!loopRange.active) {
            break;
        }

        if (takePosition == loopRange.endSamples) {
            if (blockOffset < numSamples) {
                addAllNotesOffMessages(midiBuffer, blockOffset);
                takePosition = loopRange.startSamples;
                resetPlaybackEventCursor(takePosition, combinedRatio);
                continue;
            }

            loopWrapPending = true;
            break;
        }
    }

    lastRenderedLoopRange = loopRange;
    hasRenderedPlaybackBlock = true;
}

void RecordingEngine::renderPlaybackEventsInRange(juce::MidiBuffer& midiBuffer, std::int64_t rangeStartSamples,
                                                  std::int64_t rangeEndSamples, int segmentOffset, int numSamples,
                                                  double combinedRatio) {
    if (rangeEndSamples <= rangeStartSamples) {
        return;
    }

    const auto totalEvents = playbackTake.events.size();
    if (playbackEventIndex < totalEvents) {
        const auto currentTimestamp = static_cast<std::int64_t>(
            static_cast<double>(playbackTake.events[playbackEventIndex].timestampSamples) * combinedRatio);
        if (currentTimestamp > rangeStartSamples) {
            resetPlaybackEventCursor(rangeStartSamples, combinedRatio);
        }
    }

    while (playbackEventIndex < totalEvents) {
        const auto& event = playbackTake.events[playbackEventIndex];
        const auto scaledTimestamp
            = static_cast<std::int64_t>(static_cast<double>(event.timestampSamples) * combinedRatio);
        if (scaledTimestamp < rangeStartSamples) {
            ++playbackEventIndex;
            continue;
        }
        if (scaledTimestamp >= rangeEndSamples) {
            break;
        }

        if (event.type == PerformanceEventType::presetChange) {
            juce::CriticalSection::ScopedLockType lock(presetChangeLock);
            pendingPresetChanges.push_back({ event.presetId });
        } else {
            const auto sampleOffset = segmentOffset + static_cast<int>(scaledTimestamp - rangeStartSamples);
            if (event.message.isPitchWheel()) {
                const auto channel = static_cast<std::size_t>(juce::jlimit(0, 15, event.message.getChannel() - 1));
                const auto target = static_cast<float>(event.message.getPitchWheelValue());
                smoothedPitchBend[channel] += 0.3f * (target - smoothedPitchBend[channel]);
                auto smoothedMessage = juce::MidiMessage::pitchWheel(
                    static_cast<int>(channel) + 1, static_cast<int>(std::round(smoothedPitchBend[channel])));
                smoothedMessage.setTimeStamp(event.message.getTimeStamp());
                midiBuffer.addEvent(smoothedMessage, juce::jlimit(0, numSamples - 1, sampleOffset));
            } else {
                midiBuffer.addEvent(event.message, juce::jlimit(0, numSamples - 1, sampleOffset));
            }
        }

        ++playbackEventIndex;
    }
}

void RecordingEngine::addAllNotesOffMessages(juce::MidiBuffer& midiBuffer, int sampleOffset) {
    for (int channel = 1; channel <= 16; ++channel) {
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 64, 0), sampleOffset);
        midiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 120, 0), sampleOffset);
        midiBuffer.addEvent(juce::MidiMessage::allNotesOff(channel), sampleOffset);
    }
}

void RecordingEngine::advancePlaybackPosition(std::int64_t numSamples) noexcept {
    if (!isPlaying() || numSamples <= 0) {
        return;
    }

    const auto renderedBlock = hasRenderedPlaybackBlock;
    ScaledLoopRange loopRange;
    if (renderedBlock) {
        loopRange = lastRenderedLoopRange;
    } else if (!tryGetScaledLoopRange(playbackSampleRateRatio.load(std::memory_order_relaxed)
                                          / playbackSpeedMultiplier.load(std::memory_order_relaxed),
                                      loopRange)) {
        loopRange = lastRenderedLoopRange;
    }
    hasRenderedPlaybackBlock = false;
    const auto currentPosition = playbackPositionSamples.load(std::memory_order_relaxed);

    if (loopRange.active) {
        const auto loopLength = loopRange.endSamples - loopRange.startSamples;
        const auto samplesToEnd = loopRange.endSamples - currentPosition;
        const auto crossedEnd = currentPosition >= loopRange.endSamples || numSamples >= samplesToEnd;
        if (crossedEnd) {
            const auto overflow = currentPosition >= loopRange.endSamples
                ? currentPosition - loopRange.endSamples + numSamples
                : numSamples - samplesToEnd;
            playbackPositionSamples.store(loopRange.startSamples + overflow % loopLength, std::memory_order_relaxed);
            if (!renderedBlock) {
                loopWrapPending = true;
            }
        } else {
            playbackPositionSamples.store(currentPosition + numSamples, std::memory_order_relaxed);
        }
        return;
    }

    const auto newPosition = currentPosition + numSamples;
    if (newPosition >= scaledPlaybackLengthSamples.load(std::memory_order_relaxed)) {
        state.store(RecordingState::stopped, std::memory_order_release);
        playbackPositionSamples.store(scaledPlaybackLengthSamples.load(std::memory_order_relaxed),
                                      std::memory_order_relaxed);
        playbackEndedPending.store(true, std::memory_order_release);
        return;
    }

    playbackPositionSamples.store(newPosition, std::memory_order_relaxed);
}

bool RecordingEngine::consumePlaybackEndedFlag() noexcept {
    return playbackEndedPending.exchange(false, std::memory_order_acq_rel);
}

bool RecordingEngine::isPlaying() const noexcept {
    return state.load(std::memory_order_acquire) == RecordingState::playing;
}

std::int64_t RecordingEngine::getPlaybackPositionSamples() const noexcept {
    return playbackPositionSamples;
}

std::vector<PendingPresetChange> RecordingEngine::drainPendingPresetChanges() {
    juce::CriticalSection::ScopedLockType lock(presetChangeLock);
    std::vector<PendingPresetChange> drained;
    drained.swap(pendingPresetChanges);
    pendingPresetChanges.reserve(drained.capacity());
    return drained;
}

std::int64_t RecordingEngine::getScaledPlaybackLengthSamples() const noexcept {
    if (playbackTake.lengthSamples <= 0) {
        return 0;
    }

    const auto scaledLength = static_cast<double>(playbackTake.lengthSamples)
        * playbackSampleRateRatio.load(std::memory_order_relaxed) / playbackSpeedMultiplier.load();
    if (scaledLength <= 0.0) {
        return playbackTake.lengthSamples;
    }

    return std::max<std::int64_t>(1, static_cast<std::int64_t>(std::ceil(scaledLength)));
}

RecordingEngine::ScaledLoopRange RecordingEngine::getScaledLoopRange(double combinedRatio) const noexcept {
    ScaledLoopRange range;
    return tryGetScaledLoopRange(combinedRatio, range) ? range : ScaledLoopRange {};
}

bool RecordingEngine::tryGetScaledLoopRange(double combinedRatio, ScaledLoopRange& output) const noexcept {
    AbLoopRange takeRange;
    if (!abLoopEngine.tryGetRange(takeRange)) {
        return false;
    }
    output = {};
    if (!takeRange.isValid() || playbackTake.lengthSamples <= 1 || combinedRatio <= 0.0) {
        return true;
    }

    const auto startInTake = juce::jlimit<std::int64_t>(0, playbackTake.lengthSamples, takeRange.startSamples);
    const auto endInTake = juce::jlimit<std::int64_t>(0, playbackTake.lengthSamples, takeRange.endSamples);
    if (endInTake <= startInTake) {
        return true;
    }

    const auto scaledLength = scaledPlaybackLengthSamples.load(std::memory_order_relaxed);
    if (scaledLength <= 1) {
        return true;
    }

    const auto scaledStart = static_cast<std::int64_t>(static_cast<double>(startInTake) * combinedRatio);
    const auto scaledEnd = static_cast<std::int64_t>(static_cast<double>(endInTake) * combinedRatio);
    const auto startSamples = juce::jlimit<std::int64_t>(0, scaledLength - 1, scaledStart);
    const auto endSamples = juce::jlimit<std::int64_t>(startSamples + 1, scaledLength, scaledEnd);
    const auto minBlockSize = static_cast<std::int64_t>(std::max(1, playbackBlockSize.load(std::memory_order_relaxed)));
    if (endSamples - startSamples >= minBlockSize) {
        output = { startSamples, endSamples, true };
    }
    return true;
}

void RecordingEngine::resetPlaybackEventCursor(std::int64_t positionSamples, double combinedRatio) noexcept {
    if (positionSamples <= 0 || playbackTake.events.empty()) {
        playbackEventIndex = 0;
        return;
    }

    const auto it = std::ranges::lower_bound(
        playbackTake.events, positionSamples, {}, [combinedRatio](const PerformanceEvent& event) noexcept {
            return static_cast<std::int64_t>(static_cast<double>(event.timestampSamples) * combinedRatio);
        });
    playbackEventIndex = static_cast<std::size_t>(std::distance(playbackTake.events.begin(), it));
}

} // namespace devpiano::recording
