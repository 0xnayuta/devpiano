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
    if (sampleRate <= 0.0 || lengthSamples <= 0)
        return 0.0;

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

bool RecordingEngine::hasDroppedEvents() const noexcept {
    return droppedEventCount.load(std::memory_order_relaxed) > 0;
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
    // Must not be called during active recording — the returned reference aliases
    // the events vector that the audio thread is mutating.
    jassert(!isRecording());
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
    currentTake.sampleRate = std::max(sampleRate, 0.0);
    currentTake.lengthSamples = 0;
    currentPositionSamples.store(0, std::memory_order_relaxed);
    droppedEventCount.store(0, std::memory_order_relaxed);
    playbackEndedPending.store(false, std::memory_order_release);
    state.store(RecordingState::recording, std::memory_order_release);

    DP_DEBUG_LOG("[RecordingEngine] recording STARTED");
}

RecordingTake RecordingEngine::stopRecording() {
    if (isRecording()) {
        // Merge pending preset-change events (message-thread writes) into the
        // recorded events vector before finalising the take.
        for (auto& ev : pendingPresetEvents)
            currentTake.events.push_back(std::move(ev));
        pendingPresetEvents.clear();

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
    currentPositionSamples.store(0, std::memory_order_relaxed);
    droppedEventCount.store(0, std::memory_order_relaxed);
    playbackEndedPending.store(false, std::memory_order_release);
    state.store(RecordingState::idle, std::memory_order_release);
}

void RecordingEngine::advanceRecordingPosition(std::int64_t numSamples) noexcept {
    if (!isRecording() || numSamples <= 0)
        return;

    currentPositionSamples.fetch_add(numSamples, std::memory_order_relaxed);
    currentTake.lengthSamples
        = std::max(currentTake.lengthSamples, currentPositionSamples.load(std::memory_order_relaxed));
}

void RecordingEngine::recordEvent(const juce::MidiMessage& message, RecordingEventSource source,
                                  std::int64_t timestampSamples) {
    if (!isRecording())
        return;

    const auto clampedTimestamp = std::max<std::int64_t>(timestampSamples, 0);
    if (isCapacityExhausted(clampedTimestamp)) {
        DP_DEBUG_LOG("[RecordingEngine] event DROPPED: capacity exhausted");
        return;
    }

    currentTake.events.push_back({ clampedTimestamp, PerformanceEventType::midi, 0, source, message });
    currentTake.lengthSamples = std::max(currentTake.lengthSamples, clampedTimestamp);
}

void RecordingEngine::recordMidiBufferBlock(const juce::MidiBuffer& midiBuffer, RecordingEventSource source,
                                            std::int64_t blockStartSamples) {
    if (!isRecording())
        return;

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
    if (!isRecording())
        return;

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

void RecordingEngine::startPlayback(const RecordingTake& take, double currentSampleRate) {
    playbackTake = take;
    playbackSampleRateRatio
        = (take.sampleRate > 0.0 && currentSampleRate > 0.0) ? (currentSampleRate / take.sampleRate) : 1.0;
    scaledPlaybackLengthSamples.store(getScaledPlaybackLengthSamples());
    playbackPositionSamples.store(0);
    playbackEndedPending.store(false, std::memory_order_release);

    // Pre-allocate the preset-change queue so renderPlaybackBlock never allocates
    {
        juce::CriticalSection::ScopedLockType lock(presetChangeLock);
        pendingPresetChanges.clear();
        std::size_t presetEventCount = 0;
        for (const auto& event : take.events)
            if (event.type == PerformanceEventType::presetChange)
                ++presetEventCount;
        pendingPresetChanges.reserve(presetEventCount);
    }

    // Reset per-channel pitch bend smoothing for a clean playback start.
    smoothedPitchBend.fill(8192.0f);

    state.store(RecordingState::playing, std::memory_order_release);

    DP_DEBUG_LOG("[RecordingEngine] playback STARTED: " + juce::String(take.events.size()) + " events, ratio="
                 + juce::String(playbackSampleRateRatio) + ", speed=" + juce::String(playbackSpeedMultiplier.load())
                 + ", scaledLen=" + juce::String(scaledPlaybackLengthSamples.load()));
}

void RecordingEngine::stopPlayback() {
    {
        juce::CriticalSection::ScopedLockType lock(presetChangeLock);
        pendingPresetChanges.clear();
    }

    state.store(RecordingState::stopped, std::memory_order_release);
    playbackEndedPending.store(false, std::memory_order_release);
}

void RecordingEngine::setPlaybackSpeedMultiplier(double multiplier) noexcept {
    const auto clamped = std::clamp(multiplier, 0.5, 2.0);
    const auto oldSpeed = playbackSpeedMultiplier.load();
    playbackSpeedMultiplier.store(clamped);

    if (isPlaying()) {
        // Re-align playbackPositionSamples so the take-time position stays the same
        playbackPositionSamples.store(
            static_cast<std::int64_t>(static_cast<double>(playbackPositionSamples.load()) * oldSpeed / clamped));
        scaledPlaybackLengthSamples.store(getScaledPlaybackLengthSamples());
        DP_DEBUG_LOG("[RecordingEngine] playback speed updated to " + juce::String(clamped)
                     + ", scaledLen=" + juce::String(scaledPlaybackLengthSamples.load()));
    }
}

double RecordingEngine::getPlaybackSpeedMultiplier() const noexcept {
    return playbackSpeedMultiplier.load();
}

void RecordingEngine::renderPlaybackBlock(juce::MidiBuffer& midiBuffer, std::int64_t blockStartSamples,
                                          int numSamples) {
    if (!isPlaying())
        return;

    const auto combinedRatio = playbackSampleRateRatio / playbackSpeedMultiplier.load();
    const auto blockEndSamples = blockStartSamples + static_cast<std::int64_t>(numSamples);

    for (const auto& event : playbackTake.events) {
        const auto scaledTimestamp
            = static_cast<std::int64_t>(static_cast<double>(event.timestampSamples) * combinedRatio);

        // >= on the upper bound is intentional — the interval is half-open:
        // events at blockEndSamples belong to the next block.
        if (scaledTimestamp < blockStartSamples || scaledTimestamp >= blockEndSamples)
            continue;

        if (event.type == PerformanceEventType::presetChange) {
            juce::CriticalSection::ScopedLockType lock(presetChangeLock);
            pendingPresetChanges.push_back({ event.presetId });
            continue;
        }

        const auto sampleOffset = static_cast<int>(scaledTimestamp - blockStartSamples);

        // Apply EMA smoothing to pitch bend events to eliminate zipper noise.
        // Raw events are preserved in the recording take; smoothing only affects
        // what the plugin / synth hears during playback.
        if (event.message.isPitchWheel()) {
            auto ch = static_cast<std::size_t>(juce::jlimit(0, 15, event.message.getChannel() - 1));
            auto target = static_cast<float>(event.message.getPitchWheelValue());
            smoothedPitchBend[ch] += 0.3f * (target - smoothedPitchBend[ch]);

            auto smoothedMsg = juce::MidiMessage::pitchWheel(static_cast<int>(ch) + 1,
                                                             static_cast<int>(std::round(smoothedPitchBend[ch])));
            smoothedMsg.setTimeStamp(event.message.getTimeStamp());
            midiBuffer.addEvent(smoothedMsg, juce::jlimit(0, numSamples - 1, sampleOffset));
        } else {
            midiBuffer.addEvent(event.message, juce::jlimit(0, numSamples - 1, sampleOffset));
        }
    }
}

void RecordingEngine::advancePlaybackPosition(std::int64_t numSamples) noexcept {
    if (!isPlaying() || numSamples <= 0)
        return;

    playbackPositionSamples.fetch_add(numSamples);
    if (playbackPositionSamples.load() >= scaledPlaybackLengthSamples.load()) {
        state.store(RecordingState::stopped, std::memory_order_release);
        playbackPositionSamples.store(scaledPlaybackLengthSamples.load());
        playbackEndedPending.store(true, std::memory_order_release);
        DP_LOG_INFO("[RecordingEngine] playback ENDED: pos=" + juce::String(playbackPositionSamples.load())
                    + " >= scaledLen=" + juce::String(scaledPlaybackLengthSamples.load())
                    + " (ratio=" + juce::String(playbackSampleRateRatio) + ")");
    }
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
    pendingPresetChanges.reserve(drained.capacity()); // preserve the pre-allocated capacity
    return drained;
}

std::int64_t RecordingEngine::getScaledPlaybackLengthSamples() const noexcept {
    if (playbackTake.lengthSamples <= 0)
        return 0;

    const auto scaledLength
        = static_cast<double>(playbackTake.lengthSamples) * playbackSampleRateRatio / playbackSpeedMultiplier;
    if (scaledLength <= 0.0)
        return playbackTake.lengthSamples;

    return std::max<std::int64_t>(1, static_cast<std::int64_t>(std::ceil(scaledLength)));
}
} // namespace devpiano::recording
