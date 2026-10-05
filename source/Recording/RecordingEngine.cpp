#include "Recording/RecordingEngine.h"

#include "Audio/OrderedMidi.h"
#include "Diagnostics/Log.h"
#include "Diagnostics/MidiTrace.h"
#include "Recording/RenderPipeline.h"
#include "Recording/TimelineValidation.h"

#include <algorithm>
#include <cmath>

namespace devpiano::recording {
namespace {
constexpr auto maxRealtimeMidiMessageBytes = 16;

bool isUsablePlaybackTake(const RecordingTake& take, double currentSampleRate) noexcept {
    if (!isUsableTimelineSampleRate(currentSampleRate)
        || !isRepresentableTimelineLength(take.lengthSamples, take.sampleRate)) {
        return false;
    }
    for (const auto& preset : take.presets) {
        if (!isAcousticSnapshotValid(preset.acoustic)) {
            return false;
        }
    }
    std::int64_t previousTimestamp = 0;
    for (const auto& event : take.events) {
        if (event.timestampSamples < previousTimestamp || event.timestampSamples > take.lengthSamples) {
            return false;
        }
        if (event.type == PerformanceEventType::presetChange && event.presetId >= take.presets.size()) {
            return false;
        }
        previousTimestamp = event.timestampSamples;
    }
    return true;
}
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
    return recordingEventLimit;
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
    recordingEventLimit = expectedEventCount;
    currentTake.events.reserve(expectedEventCount * 2 + 48);
}

void RecordingEngine::startRecording(double sampleRate) {
    currentTake.events.clear();
    currentTake.presets.clear();
    recordedPresetQueue.discardPublished();
    abLoopEngine.clear();
    currentTake.sampleRate = std::max(sampleRate, 0.0);
    currentTake.lengthSamples = 0;
    currentPositionSamples.store(0, std::memory_order_relaxed);
    deviceSampleRate = currentTake.sampleRate;
    recordingSampleFraction = 0.0L;
    capturedNotes = {};
    capturedPedals = {};
    droppedEventCount.store(0, std::memory_order_relaxed);
    playbackEndedPending.store(false, std::memory_order_release);
    state.store(RecordingState::recording, std::memory_order_release);

    DP_DEBUG_LOG("[RecordingEngine] recording STARTED");
}

void RecordingEngine::armRecording(double sampleRate) {
    startRecording(sampleRate);
    state.store(RecordingState::countingIn, std::memory_order_release);
}

void RecordingEngine::startArmedRecording() noexcept {
    auto expected = RecordingState::countingIn;
    state.compare_exchange_strong(expected, RecordingState::recording, std::memory_order_acq_rel);
}

void RecordingEngine::cancelArmedRecording() noexcept {
    auto expected = RecordingState::countingIn;
    state.compare_exchange_strong(expected, RecordingState::idle, std::memory_order_acq_rel);
}

void RecordingEngine::prepareForAudioDevice(double sampleRate) noexcept {
    if (!isUsableTimelineSampleRate(sampleRate)) {
        return;
    }
    const auto currentState = getState();
    if ((currentState == RecordingState::playing || currentState == RecordingState::playingPaused)
        && playbackTake.sampleRate > 0.0) {
        const auto oldRatio = playbackSampleRateRatio.load(std::memory_order_relaxed);
        const auto newRatio = sampleRate / playbackTake.sampleRate;
        const auto position = (static_cast<long double>(playbackPositionSamples.load(std::memory_order_relaxed))
                               + playbackSampleFraction)
            * newRatio / oldRatio;
        const auto integralPosition = static_cast<std::int64_t>(std::floor(position + 1e-9L));
        playbackSampleFraction = position - static_cast<long double>(integralPosition);
        playbackSampleRateRatio.store(newRatio, std::memory_order_relaxed);
        scaledPlaybackLengthSamples.store(getScaledPlaybackLengthSamples(), std::memory_order_relaxed);
        playbackPositionSamples.store(integralPosition, std::memory_order_relaxed);
        lastRenderedLoopRange = getScaledLoopRange(newRatio / getEffectivePlaybackSpeedMultiplier());
        hasRenderedPlaybackBlock = false;
    }
    deviceSampleRate = sampleRate;
}

RecordingTake RecordingEngine::stopRecording() {
    // Finalise even when stopping from the paused-recording state (events and
    // length must still be merged/updated).
    const auto recordingActive
        = isRecording() || state.load(std::memory_order_acquire) == RecordingState::recordingPaused;
    if (recordingActive) {
        capturePendingPresetChanges(getCurrentPositionSamples());
        closeCapturedPerformance();

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
    currentTake.presets.clear();
    recordedPresetQueue.discardPublished();
    scheduledPresetChanges.clear();
    pendingPresetNotification.store(noPreset, std::memory_order_relaxed);
    currentTake.sampleRate = 0.0;
    currentTake.lengthSamples = 0;
    playbackTake = {};
    currentPositionSamples.store(0, std::memory_order_relaxed);
    playbackPositionSamples.store(0, std::memory_order_relaxed);
    scaledPlaybackLengthSamples.store(0, std::memory_order_relaxed);
    requestedSeekSample.store(0, std::memory_order_relaxed);
    const auto sequence = seekSequence.load(std::memory_order_relaxed);
    appliedSeekSequence.store(sequence, std::memory_order_relaxed);
    const auto spdSeq = speedSequence.load(std::memory_order_relaxed);
    appliedSpeedSequence.store(spdSeq, std::memory_order_relaxed);
    const auto retainedSpeed = targetSpeedMultiplier.load(std::memory_order_relaxed);
    requestedSpeedMultiplier.store(retainedSpeed, std::memory_order_relaxed);
    effectiveSpeedMultiplier.store(retainedSpeed, std::memory_order_relaxed);
    const auto stpSeq = stopSequence.load(std::memory_order_relaxed);
    appliedStopSequence.store(stpSeq, std::memory_order_relaxed);
    stopRequested.store(false, std::memory_order_relaxed);
    droppedEventCount.store(0, std::memory_order_relaxed);
    playbackEventIndex = 0;
    ++playbackGeneration;
    playbackNoteOnCount = 0;
    playbackSampleFraction = 0.0L;
    playbackCleanupPending = false;
    playbackCleanupDelivered = false;
    playbackStateRestorePending = false;
    restoredStateEventIndex = std::numeric_limits<std::size_t>::max();
    recordingSampleFraction = 0.0L;
    capturedNotes = {};
    capturedPedals = {};
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

    const auto samples
        = recordingSampleFraction + static_cast<long double>(numSamples) * currentTake.sampleRate / deviceSampleRate;
    const auto integralSamples = static_cast<std::int64_t>(std::floor(samples + 1.0e-9L));
    recordingSampleFraction = samples - static_cast<long double>(integralSamples);
    currentPositionSamples.fetch_add(integralSamples, std::memory_order_relaxed);
    currentTake.lengthSamples
        = std::max(currentTake.lengthSamples, currentPositionSamples.load(std::memory_order_relaxed));
}

void RecordingEngine::recordEvent(const juce::MidiMessage& message, RecordingEventSource source,
                                  std::int64_t timestampSamples) {
    if (!isRecording()) {
        return;
    }
    capturePendingPresetChanges(std::max<std::int64_t>(timestampSamples, 0));

    if (message.isNoteOff()
        && capturedNotes[static_cast<std::size_t>(message.getChannel() - 1)]
                        [static_cast<std::size_t>(message.getNoteNumber())]
            == 0) {
        return;
    }
    const auto clampedTimestamp = std::max<std::int64_t>(timestampSamples, 0);
    const auto requiredRelease = message.isNoteOff()
        || (message.isController()
            && (message.getControllerNumber() >= 120
                || ((message.getControllerNumber() == 64 || message.getControllerNumber() == 66
                     || message.getControllerNumber() == 67)
                    && message.getControllerValue() == 0)));
    const auto releases = message.isNoteOff() ? capturedNotes[static_cast<std::size_t>(message.getChannel() - 1)]
                                                             [static_cast<std::size_t>(message.getNoteNumber())]
                                              : std::size_t { 1 };
    for (auto remaining = releases; remaining > 0; --remaining) {
        if (isCapacityExhausted(clampedTimestamp, requiredRelease)) {
            return;
        }
        currentTake.events.push_back({ clampedTimestamp, PerformanceEventType::midi, 0, source, message });
        updateCapturedPerformance(message);
    }
    currentTake.lengthSamples = std::max(currentTake.lengthSamples, clampedTimestamp);
}

void RecordingEngine::recordMidiBufferBlock(const juce::MidiBuffer& midiBuffer, RecordingEventSource source,
                                            std::int64_t blockStartSamples, int firstSample) {
    if (!isRecording()) {
        return;
    }

    const auto clampedBlockStart = std::max<std::int64_t>(blockStartSamples, 0);
    capturePendingPresetChanges(clampedBlockStart);

    for (const auto metadata : midiBuffer) {
        if (metadata.samplePosition < firstSample) {
            continue;
        }
        const auto offset = recordingSampleFraction
            + static_cast<long double>(metadata.samplePosition - firstSample) * currentTake.sampleRate
                / deviceSampleRate;
        const auto timestamp = clampedBlockStart + static_cast<std::int64_t>(std::floor(offset + 1.0e-9L));
        if (metadata.numBytes > maxRealtimeMidiMessageBytes) {
            droppedEventCount.fetch_add(1, std::memory_order_relaxed);
            currentTake.lengthSamples = std::max(currentTake.lengthSamples, timestamp);
            continue;
        }

        auto message = metadata.getMessage();
        message.setTimeStamp(0.0);
        recordEvent(message, source, timestamp);
    }
}

void RecordingEngine::updateCapturedPerformance(const juce::MidiMessage& message) noexcept {
    const auto channel = message.getChannel() - 1;
    if (channel < 0 || channel >= 16) {
        return;
    }
    auto& notes = capturedNotes[static_cast<std::size_t>(channel)];
    auto& pedals = capturedPedals[static_cast<std::size_t>(channel)];
    if (message.isNoteOn()) {
        ++notes[static_cast<std::size_t>(message.getNoteNumber())];
    } else if (message.isNoteOff()) {
        auto& count = notes[static_cast<std::size_t>(message.getNoteNumber())];
        if (count > 0) {
            --count;
        }
    } else if (message.isController()) {
        const auto controller = message.getControllerNumber();
        if (controller == 64 || controller == 66 || controller == 67) {
            const auto index = controller == 64 ? 0 : controller - 65;
            pedals[static_cast<std::size_t>(index)] = message.getControllerValue();
        } else if (controller == 120 || controller == 123) {
            notes.fill(0);
        } else if (controller == 121) {
            pedals.fill(0);
        }
    }
}

void RecordingEngine::closeCapturedPerformance() {
    auto boundary = std::max(currentTake.lengthSamples, getCurrentPositionSamples());
    if (!currentTake.events.empty()) {
        boundary = std::max(boundary, currentTake.events.back().timestampSamples);
    }
    auto required = currentTake.events.size();
    for (std::size_t channel = 0; channel < capturedNotes.size(); ++channel) {
        for (const auto count : capturedNotes[channel]) {
            required += count;
        }
        for (const auto value : capturedPedals[channel]) {
            required += value != 0 ? 1U : 0U;
        }
    }
    currentTake.events.reserve(required);
    for (std::size_t channel = 0; channel < capturedNotes.size(); ++channel) {
        for (std::size_t note = 0; note < capturedNotes[channel].size(); ++note) {
            for (auto count = capturedNotes[channel][note]; count > 0; --count) {
                currentTake.events.push_back(
                    { boundary, PerformanceEventType::midi, 0, RecordingEventSource::realtimeMidiBuffer,
                      juce::MidiMessage::noteOff(static_cast<int>(channel) + 1, static_cast<int>(note)) });
            }
        }
        for (std::size_t pedal = 0; pedal < capturedPedals[channel].size(); ++pedal) {
            if (capturedPedals[channel][pedal] != 0) {
                const auto controller = pedal == 0 ? 64 : static_cast<int>(pedal) + 65;
                currentTake.events.push_back(
                    { boundary, PerformanceEventType::midi, 0, RecordingEventSource::realtimeMidiBuffer,
                      juce::MidiMessage::controllerEvent(static_cast<int>(channel) + 1, controller, 0) });
            }
        }
    }
    capturedNotes = {};
    capturedPedals = {};
    currentTake.lengthSamples = boundary;
}

// ---- Preset-change recording ----

void RecordingEngine::recordPresetChange(const RecordedPreset& preset) {
    if (!isRecording() && getState() != RecordingState::countingIn) {
        return;
    }
    const auto slot = static_cast<std::uint32_t>(currentTake.presets.size());
    currentTake.presets.push_back(preset);
    if (!recordedPresetQueue.push(slot)) {
        currentTake.presets.pop_back();
        droppedEventCount.fetch_add(1, std::memory_order_relaxed);
    }
}

void RecordingEngine::capturePendingPresetChanges(std::int64_t timestampSamples) noexcept {
    std::uint32_t slot = 0;
    for (std::size_t count = 0; count < 1024 && recordedPresetQueue.pop(slot); ++count) {
        if (isCapacityExhausted(timestampSamples)) {
            continue;
        }
        currentTake.events.push_back(
            { timestampSamples, PerformanceEventType::presetChange, slot, RecordingEventSource::computerKeyboard, {} });
    }
}

bool RecordingEngine::isCapacityExhausted(std::int64_t timestamp, bool requiredRelease) noexcept {
    const auto limit = requiredRelease ? currentTake.events.capacity() : recordingEventLimit;
    if (currentTake.events.size() >= limit) {
        droppedEventCount.fetch_add(1, std::memory_order_relaxed);
        currentTake.lengthSamples = std::max(currentTake.lengthSamples, timestamp);
        return true;
    }
    return false;
}

void RecordingEngine::startPlayback(const RecordingTake& take, double currentSampleRate,
                                    std::int64_t resumeFromSamples) {
    if (!isUsablePlaybackTake(take, currentSampleRate)) {
        DP_LOG_ERROR("[RecordingEngine] playback rejected: invalid or unrepresentable timeline");
        return;
    }
    playbackTake = take;
    std::ranges::stable_sort(playbackTake.events, [](const auto& a, const auto& b) {
        if (a.timestampSamples != b.timestampSamples) {
            return a.timestampSamples < b.timestampSamples;
        }
        if (a.type != b.type) {
            return a.type == PerformanceEventType::presetChange;
        }
        return false;
    });
    ++playbackGeneration;
    playbackNoteOnCount = static_cast<std::size_t>(std::ranges::count_if(take.events, [](const auto& event) {
        return event.type == PerformanceEventType::midi && event.message.isNoteOn();
    }));
    deviceSampleRate = currentSampleRate;
    playbackSampleFraction = 0.0L;
    playbackCleanupPending = false;
    playbackCleanupDelivered = false;
    restoredStateEventIndex = std::numeric_limits<std::size_t>::max();
    const auto activeSpeed = targetSpeedMultiplier.load(std::memory_order_relaxed);
    effectiveSpeedMultiplier.store(activeSpeed, std::memory_order_relaxed);

    playbackSampleRateRatio.store(currentSampleRate / take.sampleRate, std::memory_order_relaxed);
    scaledPlaybackLengthSamples.store(getScaledPlaybackLengthSamples());
    const auto combinedRatio = playbackSampleRateRatio.load(std::memory_order_relaxed) / activeSpeed;
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
    playbackStateRestorePending = initialResume > 0;

    const auto seekVersion = seekSequence.load(std::memory_order_acquire);
    appliedSeekSequence.store(seekVersion, std::memory_order_release);
    const auto speedVersion = speedSequence.load(std::memory_order_acquire);
    appliedSpeedSequence.store(speedVersion, std::memory_order_release);
    const auto stopVersion = stopSequence.load(std::memory_order_acquire);
    appliedStopSequence.store(stopVersion, std::memory_order_release);
    stopRequested.store(false, std::memory_order_relaxed);
    pendingPresetNotification.store(noPreset, std::memory_order_relaxed);
    scheduledPresetChanges.clear();
    const auto presetEventCount = static_cast<std::size_t>(std::ranges::count_if(
        take.events, [](const auto& event) { return event.type == PerformanceEventType::presetChange; }));
    scheduledPresetChanges.reserve(presetEventCount * 2 + 2);
    std::size_t midiBytes = 0;
    for (const auto& event : take.events) {
        if (event.type == PerformanceEventType::midi) {
            midiBytes += static_cast<std::size_t>(event.message.getRawDataSize()) + 6;
        }
    }
    playbackMidiCapacityBytes = std::max<std::size_t>(131072, midiBytes * 2 + presetEventCount * 288 + 98304);
    restoredPresetId = noPreset;

    smoothedPitchBend.fill(8192.0f);
    state.store(RecordingState::playing, std::memory_order_release);

    DP_DEBUG_LOG("[RecordingEngine] playback STARTED: " + juce::String(take.events.size())
                 + " events, ratio=" + juce::String(playbackSampleRateRatio.load(std::memory_order_relaxed))
                 + ", speed=" + juce::String(effectiveSpeedMultiplier.load())
                 + ", scaledLen=" + juce::String(scaledPlaybackLengthSamples.load()));
}

void RecordingEngine::startPlaybackAtTakeSample(const RecordingTake& take, double currentSampleRate,
                                                std::int64_t resumeFromTakeSamples) {
    if (!isUsablePlaybackTake(take, currentSampleRate)) {
        DP_LOG_ERROR("[RecordingEngine] playback seek rejected: invalid or unrepresentable timeline");
        return;
    }
    const auto takeSample
        = juce::jlimit<std::int64_t>(0, std::max<std::int64_t>(take.lengthSamples, 0), resumeFromTakeSamples);
    const auto sampleRateRatio = currentSampleRate / take.sampleRate;
    const auto activeSpeed = targetSpeedMultiplier.load(std::memory_order_relaxed);
    const auto combinedRatio = sampleRateRatio / activeSpeed;
    const auto scaledPosition = *checkedScaleSamples(takeSample, combinedRatio);
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

    requestedSeekSample.store(std::max<std::int64_t>(takeSample, 0), std::memory_order_release);
    seekSequence.store(observedVersion + 2, std::memory_order_release);
}

bool RecordingEngine::readPendingPlaybackSeek(std::int64_t& takeSample, std::uint32_t& sequence) const noexcept {
    const auto observedVersion = seekSequence.load(std::memory_order_acquire);
    if ((observedVersion & 1U) != 0U || observedVersion == appliedSeekSequence.load(std::memory_order_acquire)) {
        return false;
    }

    const auto requestedSample = requestedSeekSample.load(std::memory_order_acquire);
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

bool RecordingEngine::readPendingSpeedChange(double& newSpeed, std::uint32_t& sequence) const noexcept {
    const auto observedVersion = speedSequence.load(std::memory_order_acquire);
    if ((observedVersion & 1U) != 0U || observedVersion == appliedSpeedSequence.load(std::memory_order_acquire)) {
        return false;
    }

    const auto requestedSpeed = requestedSpeedMultiplier.load(std::memory_order_acquire);
    if (speedSequence.load(std::memory_order_acquire) != observedVersion) {
        return false;
    }

    newSpeed = requestedSpeed;
    sequence = observedVersion;
    return true;
}

bool RecordingEngine::readPendingStop(std::uint32_t& sequence) const noexcept {
    const auto observedVersion = stopSequence.load(std::memory_order_acquire);
    if ((observedVersion & 1U) != 0U || observedVersion == appliedStopSequence.load(std::memory_order_acquire)) {
        return false;
    }

    const auto requestedStop = stopRequested.load(std::memory_order_acquire);
    if (stopSequence.load(std::memory_order_acquire) != observedVersion) {
        return false;
    }

    if (!requestedStop) {
        return false;
    }

    sequence = observedVersion;
    return true;
}

RecordingEngine::TransportCommandResult
RecordingEngine::applyPendingTransportCommands(juce::MidiBuffer& midiBuffer) noexcept {
    TransportCommandResult result;

    std::uint32_t stopSeq = 0;
    const bool hasPendingStop = readPendingStop(stopSeq);

    double newSpeed = 0.0;
    std::uint32_t speedSeq = 0;
    const bool hasPendingSpeed = readPendingSpeedChange(newSpeed, speedSeq);

    std::int64_t seekSample = 0;
    std::uint32_t seekSeq = 0;
    const bool hasPendingSeek = readPendingPlaybackSeek(seekSample, seekSeq);

    if (!hasPendingStop && !hasPendingSpeed && !hasPendingSeek) {
        return result;
    }

    if (hasPendingStop) {
        if (hasPendingSpeed) {
            appliedSpeedSequence.store(speedSeq, std::memory_order_release);
        }
        if (hasPendingSeek) {
            appliedSeekSequence.store(seekSeq, std::memory_order_release);
        }

        state.store(RecordingState::stopped, std::memory_order_release);
        playbackEndedPending.store(false, std::memory_order_release);
        playbackEventIndex = 0;
        hasRenderedPlaybackBlock = false;
        loopWrapPending = false;
        playbackCleanupPending = false;
        playbackCleanupDelivered = true;
        playbackStateRestorePending = false;
        addAllNotesOffMessages(midiBuffer, 0);

        appliedStopSequence.store(stopSeq, std::memory_order_release);
        result.stopApplied = true;
        return result;
    }

    if (hasPendingSpeed) {
        const auto oldSpeed = effectiveSpeedMultiplier.load(std::memory_order_relaxed);
        effectiveSpeedMultiplier.store(newSpeed, std::memory_order_relaxed);

        const auto currentPos = playbackPositionSamples.load(std::memory_order_relaxed);
        const auto precisePosition
            = (static_cast<long double>(currentPos) + playbackSampleFraction) * oldSpeed / newSpeed;
        const auto newPosition = static_cast<std::int64_t>(std::floor(precisePosition));
        playbackSampleFraction = precisePosition - static_cast<long double>(newPosition);
        scaledPlaybackLengthSamples.store(getScaledPlaybackLengthSamples());
        const auto clampedPosition
            = juce::jlimit<std::int64_t>(0, scaledPlaybackLengthSamples.load(std::memory_order_relaxed), newPosition);
        playbackPositionSamples.store(clampedPosition, std::memory_order_relaxed);

        hasRenderedPlaybackBlock = false;

        appliedSpeedSequence.store(speedSeq, std::memory_order_release);
        result.speedChanged = true;
    }

    if (hasPendingSeek) {
        const auto clampedTakeSample
            = juce::jlimit<std::int64_t>(0, std::max<std::int64_t>(playbackTake.lengthSamples, 0), seekSample);
        const auto combinedRatio = playbackSampleRateRatio.load(std::memory_order_relaxed)
            / effectiveSpeedMultiplier.load(std::memory_order_relaxed);
        const auto scaledPosition = *checkedScaleSamples(clampedTakeSample, combinedRatio);
        const auto clampedPosition = juce::jlimit<std::int64_t>(
            0, scaledPlaybackLengthSamples.load(std::memory_order_relaxed), scaledPosition);

        playbackPositionSamples.store(clampedPosition, std::memory_order_relaxed);
        playbackSampleFraction = 0.0L;
        playbackCleanupPending = false;
        playbackCleanupDelivered = false;
        resetPlaybackEventCursor(clampedPosition, combinedRatio);
        smoothedPitchBend.fill(8192.0f);
        hasRenderedPlaybackBlock = false;
        loopWrapPending = false;
        addAllNotesOffMessages(midiBuffer, 0);

        appliedSeekSequence.store(seekSeq, std::memory_order_release);
        result.seekApplied = true;
    }

    return result;
}

void RecordingEngine::applyPendingTransportCommandsQuiescent() noexcept {
    juce::MidiBuffer unusedMidiBuffer;
    static_cast<void>(applyPendingTransportCommands(unusedMidiBuffer));
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
        / effectiveSpeedMultiplier.load(std::memory_order_relaxed);
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

    closeCapturedPerformance();
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

void RecordingEngine::requestPlaybackStop() noexcept {
    auto observedVersion = stopSequence.load(std::memory_order_relaxed);
    for (;;) {
        if ((observedVersion & 1U) != 0U) {
            observedVersion = stopSequence.load(std::memory_order_relaxed);
            continue;
        }
        if (stopSequence.compare_exchange_weak(observedVersion, observedVersion + 1, std::memory_order_acq_rel,
                                               std::memory_order_relaxed)) {
            break;
        }
    }

    stopRequested.store(true, std::memory_order_release);
    stopSequence.store(observedVersion + 2, std::memory_order_release);
}

void RecordingEngine::stopPlaybackQuiescent() noexcept {
    const auto seekVersion = seekSequence.load(std::memory_order_acquire);
    appliedSeekSequence.store(seekVersion, std::memory_order_release);
    const auto speedVersion = speedSequence.load(std::memory_order_acquire);
    appliedSpeedSequence.store(speedVersion, std::memory_order_release);
    const auto stopVersion = stopSequence.load(std::memory_order_acquire);
    appliedStopSequence.store(stopVersion, std::memory_order_release);
    stopRequested.store(false, std::memory_order_relaxed);

    state.store(RecordingState::stopped, std::memory_order_release);
    playbackEndedPending.store(false, std::memory_order_release);
    playbackEventIndex = 0;
    hasRenderedPlaybackBlock = false;
    loopWrapPending = false;
    playbackCleanupPending = false;
    playbackCleanupDelivered = true;
    playbackStateRestorePending = false;
}

void RecordingEngine::setPlaybackSpeedMultiplier(double multiplier) noexcept {
    if (!std::isfinite(multiplier)) {
        return;
    }
    const auto clamped = std::clamp(multiplier, 0.5, 2.0);
    targetSpeedMultiplier.store(clamped, std::memory_order_release);

    auto observedVersion = speedSequence.load(std::memory_order_relaxed);
    for (;;) {
        if ((observedVersion & 1U) != 0U) {
            observedVersion = speedSequence.load(std::memory_order_relaxed);
            continue;
        }
        if (speedSequence.compare_exchange_weak(observedVersion, observedVersion + 1, std::memory_order_acq_rel,
                                                std::memory_order_relaxed)) {
            break;
        }
    }

    requestedSpeedMultiplier.store(clamped, std::memory_order_release);
    speedSequence.store(observedVersion + 2, std::memory_order_release);
}

double RecordingEngine::getPlaybackSpeedMultiplier() const noexcept {
    return targetSpeedMultiplier.load(std::memory_order_relaxed);
}

double RecordingEngine::getEffectivePlaybackSpeedMultiplier() const noexcept {
    return effectiveSpeedMultiplier.load(std::memory_order_relaxed);
}
void RecordingEngine::setPlaybackBlockSize(int blockSize) noexcept {
    playbackBlockSize.store(std::max(1, blockSize), std::memory_order_relaxed);
}

void RecordingEngine::renderPlaybackBlock(juce::MidiBuffer& midiBuffer, std::int64_t blockStartSamples,
                                          int numSamples) {
    scheduledPresetChanges.clear();
    renderedMidiEventCount = static_cast<std::size_t>(midiBuffer.getNumEvents());
    lastRenderedMidiSample = midiBuffer.getLastEventTime();
    if (playbackCleanupPending && numSamples > 0) {
        addAllNotesOffMessages(midiBuffer, 0);
        playbackCleanupPending = false;
        playbackCleanupDelivered = true;
        playbackEndedPending.store(true, std::memory_order_release);
        hasRenderedPlaybackBlock = false;
        return;
    }
    if (!isPlaying() || numSamples <= 0) {
        hasRenderedPlaybackBlock = false;
        return;
    }

    auto currentBlockSize = playbackBlockSize.load(std::memory_order_relaxed);
    while (numSamples > currentBlockSize
           && !playbackBlockSize.compare_exchange_weak(currentBlockSize, numSamples, std::memory_order_relaxed,
                                                       std::memory_order_relaxed)) { }

    const auto combinedRatio = playbackSampleRateRatio.load(std::memory_order_relaxed)
        / effectiveSpeedMultiplier.load(std::memory_order_relaxed);
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

    if (playbackStateRestorePending) {
        restorePlaybackChannelState(midiBuffer, 0);
        playbackStateRestorePending = false;
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
                restorePlaybackChannelState(midiBuffer, blockOffset);
                playbackStateRestorePending = false;
                continue;
            }

            loopWrapPending = true;
            break;
        }
    }
    if (!loopRange.active && !playbackCleanupDelivered) {
        const auto end = scaledPlaybackLengthSamples.load(std::memory_order_relaxed);
        if (end >= blockStartSamples && end < blockStartSamples + numSamples) {
            addAllNotesOffMessages(midiBuffer, static_cast<int>(end - blockStartSamples));
            playbackCleanupDelivered = true;
        } else if (end == blockStartSamples + numSamples) {
            playbackCleanupPending = true;
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
    while (playbackEventIndex < totalEvents) {
        const auto& event = playbackTake.events[playbackEventIndex];
        const auto scaledTimestamp = *checkedScaleSamples(event.timestampSamples, combinedRatio);
        if (scaledTimestamp < rangeStartSamples) {
            ++playbackEventIndex;
            continue;
        }
        if (scaledTimestamp >= rangeEndSamples) {
            break;
        }

        if (event.type == PerformanceEventType::presetChange) {
            const auto sampleOffset = segmentOffset + static_cast<int>(scaledTimestamp - rangeStartSamples);
            schedulePresetChange(event.presetId, sampleOffset, renderedMidiEventCount);
        } else {
            const auto sampleOffset = segmentOffset + static_cast<int>(scaledTimestamp - rangeStartSamples);
            if (event.message.isPitchWheel()) {
                const auto channel = static_cast<std::size_t>(juce::jlimit(0, 15, event.message.getChannel() - 1));
                const auto target = static_cast<float>(event.message.getPitchWheelValue());
                smoothedPitchBend[channel] += 0.3f * (target - smoothedPitchBend[channel]);
                auto smoothedMessage = juce::MidiMessage::pitchWheel(
                    static_cast<int>(channel) + 1, static_cast<int>(std::round(smoothedPitchBend[channel])));
                smoothedMessage.setTimeStamp(event.message.getTimeStamp());
                appendPlaybackMidi(midiBuffer, smoothedMessage, juce::jlimit(0, numSamples - 1, sampleOffset));
            } else {
                appendPlaybackMidi(midiBuffer, event.message, juce::jlimit(0, numSamples - 1, sampleOffset));
            }
            ++renderedMidiEventCount;
        }

        ++playbackEventIndex;
    }
}

void RecordingEngine::appendPlaybackMidi(juce::MidiBuffer& midiBuffer, const juce::MidiMessage& message,
                                         int sampleOffset) {
    if (sampleOffset >= lastRenderedMidiSample) {
        devpiano::audio::appendOrderedMidi(midiBuffer, message, sampleOffset);
    } else {
        midiBuffer.addEvent(message, sampleOffset);
    }
    lastRenderedMidiSample = std::max(lastRenderedMidiSample, sampleOffset);
}

void RecordingEngine::addAllNotesOffMessages(juce::MidiBuffer& midiBuffer, int sampleOffset) {
    for (int channel = 1; channel <= 16; ++channel) {
        appendPlaybackMidi(midiBuffer, juce::MidiMessage::controllerEvent(channel, 64, 0), sampleOffset);
        appendPlaybackMidi(midiBuffer, juce::MidiMessage::controllerEvent(channel, 120, 0), sampleOffset);
        appendPlaybackMidi(midiBuffer, juce::MidiMessage::allNotesOff(channel), sampleOffset);
    }
    renderedMidiEventCount += 48;
}

void RecordingEngine::advancePlaybackPosition(std::int64_t numSamples) noexcept {
    if (!isPlaying() || numSamples <= 0) {
        return;
    }

    const auto renderedBlock = hasRenderedPlaybackBlock;
    ScaledLoopRange loopRange;
    if (renderedBlock
        || !tryGetScaledLoopRange(playbackSampleRateRatio.load(std::memory_order_relaxed)
                                      / effectiveSpeedMultiplier.load(std::memory_order_relaxed),
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
        if (playbackCleanupDelivered || !renderedBlock) {
            playbackEndedPending.store(true, std::memory_order_release);
        }
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
    const auto slot = pendingPresetNotification.exchange(noPreset, std::memory_order_acq_rel);
    if (const auto* snapshot = getPlaybackPreset(slot)) {
        return { { slot, snapshot } };
    }
    return {};
}

const RecordedPreset* RecordingEngine::getPlaybackPreset(std::uint32_t presetId) const noexcept {
    return presetId < playbackTake.presets.size() ? &playbackTake.presets[presetId] : nullptr;
}

const std::vector<ScheduledPresetChange>& RecordingEngine::getScheduledPresetChanges() const noexcept {
    return scheduledPresetChanges;
}

std::size_t RecordingEngine::getPlaybackMidiCapacityBytes() const noexcept {
    return playbackMidiCapacityBytes;
}

std::size_t RecordingEngine::consumePresetNotificationCoalescedCount() noexcept {
    return presetNotificationCoalescedCount.exchange(0, std::memory_order_acq_rel);
}

void RecordingEngine::schedulePresetChange(std::uint32_t presetId, int sampleOffset,
                                           std::size_t midiEventCount) noexcept {
    if (presetId >= playbackTake.presets.size() || scheduledPresetChanges.size() == scheduledPresetChanges.capacity()) {
        return;
    }
    scheduledPresetChanges.push_back({ presetId, sampleOffset, midiEventCount });
    if (pendingPresetNotification.exchange(presetId, std::memory_order_acq_rel) != noPreset) {
        presetNotificationCoalescedCount.fetch_add(1, std::memory_order_relaxed);
    }
}

std::int64_t RecordingEngine::getScaledPlaybackLengthSamples() const noexcept {
    const auto ratio = playbackSampleRateRatio.load(std::memory_order_relaxed)
        / effectiveSpeedMultiplier.load(std::memory_order_relaxed);
    auto length = *checkedScaleSamples(playbackTake.lengthSamples, ratio);
    if (!playbackTake.events.empty()) {
        length = std::max(length, *checkedScaleSamples(playbackTake.events.back().timestampSamples, ratio) + 1);
    }
    return length;
}

std::uint64_t RecordingEngine::getPlaybackGeneration() const noexcept {
    return playbackGeneration;
}

std::size_t RecordingEngine::getPlaybackNoteOnCount() const noexcept {
    return playbackNoteOnCount;
}

bool RecordingEngine::needsPlaybackRender() const noexcept {
    return isPlaying() || playbackCleanupPending;
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

    const auto scaledStart = *checkedScaleSamples(startInTake, combinedRatio);
    const auto scaledEnd = *checkedScaleSamples(endInTake, combinedRatio);
    const auto startSamples = juce::jlimit<std::int64_t>(0, scaledLength - 1, scaledStart);
    const auto endSamples = juce::jlimit<std::int64_t>(startSamples + 1, scaledLength, scaledEnd);
    const auto minBlockSize = static_cast<std::int64_t>(std::max(1, playbackBlockSize.load(std::memory_order_relaxed)));
    if (endSamples - startSamples >= minBlockSize) {
        output = { startSamples, endSamples, true };
    }
    return true;
}

void RecordingEngine::restorePlaybackChannelState(juce::MidiBuffer& midiBuffer, int sampleOffset) {
    if (restoredStateEventIndex != playbackEventIndex) {
        restoredChannelState = {};
        for (auto& channel : restoredChannelState) {
            channel.controllers.fill(-1);
        }
        restoredPresetId = noPreset;
        for (std::size_t index = 0; index < playbackEventIndex; ++index) {
            const auto& event = playbackTake.events[index];
            if (event.type == PerformanceEventType::presetChange) {
                restoredPresetId = event.presetId;
                continue;
            }
            const auto& message = event.message;
            const auto channel = message.getChannel() - 1;
            if (channel < 0 || channel >= 16) {
                continue;
            }
            auto& snapshot = restoredChannelState[static_cast<std::size_t>(channel)];
            if (message.isController()) {
                const auto controller = message.getControllerNumber();
                const auto value = message.getControllerValue();
                if (controller == 0) {
                    snapshot.bankMsb = value;
                } else if (controller == 32) {
                    snapshot.bankLsb = value;
                } else if (controller == 121) {
                    snapshot.controllers.fill(-1);
                    snapshot.pitch = 8192;
                    snapshot.pressure = 0;
                } else if (controller < 120) {
                    snapshot.controllers[static_cast<std::size_t>(controller)] = static_cast<std::int16_t>(value);
                }
            } else if (message.isProgramChange()) {
                snapshot.program = message.getProgramChangeNumber();
                snapshot.programBankMsb = snapshot.bankMsb;
                snapshot.programBankLsb = snapshot.bankLsb;
            } else if (message.isPitchWheel()) {
                snapshot.pitch = message.getPitchWheelValue();
            } else if (message.isChannelPressure()) {
                snapshot.pressure = message.getChannelPressureValue();
            }
        }
        restoredStateEventIndex = playbackEventIndex;
    }
    if (restoredPresetId != noPreset) {
        schedulePresetChange(restoredPresetId, sampleOffset, renderedMidiEventCount);
    }
    for (std::size_t index = 0; index < restoredChannelState.size(); ++index) {
        const auto channel = static_cast<int>(index) + 1;
        const auto& snapshot = restoredChannelState[index];
        appendPlaybackMidi(midiBuffer, juce::MidiMessage::controllerEvent(channel, 121, 0), sampleOffset);
        appendPlaybackMidi(midiBuffer, juce::MidiMessage::controllerEvent(channel, 0, snapshot.programBankMsb),
                           sampleOffset);
        appendPlaybackMidi(midiBuffer, juce::MidiMessage::controllerEvent(channel, 32, snapshot.programBankLsb),
                           sampleOffset);
        appendPlaybackMidi(midiBuffer, juce::MidiMessage::programChange(channel, snapshot.program), sampleOffset);
        if (snapshot.bankMsb != snapshot.programBankMsb) {
            appendPlaybackMidi(midiBuffer, juce::MidiMessage::controllerEvent(channel, 0, snapshot.bankMsb),
                               sampleOffset);
        }
        if (snapshot.bankLsb != snapshot.programBankLsb) {
            appendPlaybackMidi(midiBuffer, juce::MidiMessage::controllerEvent(channel, 32, snapshot.bankLsb),
                               sampleOffset);
        }
        for (std::size_t controller = 1; controller < 120; ++controller) {
            if (controller != 32 && snapshot.controllers[controller] >= 0) {
                appendPlaybackMidi(midiBuffer,
                                   juce::MidiMessage::controllerEvent(channel, static_cast<int>(controller),
                                                                      snapshot.controllers[controller]),
                                   sampleOffset);
            }
        }
        appendPlaybackMidi(midiBuffer, juce::MidiMessage::pitchWheel(channel, snapshot.pitch), sampleOffset);
        appendPlaybackMidi(midiBuffer, juce::MidiMessage::channelPressureChange(channel, snapshot.pressure),
                           sampleOffset);
        smoothedPitchBend[index] = static_cast<float>(snapshot.pitch);
    }
    renderedMidiEventCount = static_cast<std::size_t>(midiBuffer.getNumEvents());
}

void RecordingEngine::resetPlaybackEventCursor(std::int64_t positionSamples, double combinedRatio) noexcept {
    playbackStateRestorePending = true;
    if (positionSamples <= 0 || playbackTake.events.empty()) {
        playbackEventIndex = 0;
        return;
    }

    const auto it = std::ranges::lower_bound(playbackTake.events, positionSamples, {},
                                             [combinedRatio](const PerformanceEvent& event) noexcept {
                                                 return *checkedScaleSamples(event.timestampSamples, combinedRatio);
                                             });
    playbackEventIndex = static_cast<std::size_t>(std::distance(playbackTake.events.begin(), it));
}

} // namespace devpiano::recording
