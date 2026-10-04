#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <juce_audio_basics/juce_audio_basics.h>

#include "Core/MetronomeModel.h"

namespace devpiano::audio {

/// Sample-accurate, lock-free, zero-allocation metronome audio processor.
class MetronomeProcessor {
public:
    static constexpr double kTwoPi = 6.28318530717958647692;

private:
    enum class RunState : std::uint8_t { disabled = 0, startPending = 1, active = 2 };

public:
    MetronomeProcessor() noexcept = default;

    void prepareToPlay(double newSampleRate) noexcept {
        sampleRate = (newSampleRate > 1000.0) ? newSampleRate : 48000.0;

        const auto currentSig = timeSignature.load(std::memory_order_relaxed);
        const auto denominator = static_cast<double>(devpiano::core::getTimeSignatureDenominator(currentSig));
        const double currentBpm = bpm.load(std::memory_order_relaxed);
        const double newSamplesPerBeat = (sampleRate * 60.0 / currentBpm) * (4.0 / denominator);

        if (countInArmed.load(std::memory_order_acquire) || audioCountInActive) {
            rebaseBeatPhase(newSamplesPerBeat);
            return;
        }

        reset();
    }

    void reset() noexcept {
        if (countInArmed.load(std::memory_order_acquire) || audioCountInActive) {
            return;
        }

        auto expected = RunState::active;
        runState.compare_exchange_strong(expected, RunState::startPending, std::memory_order_acq_rel,
                                         std::memory_order_relaxed);
        currentBeatNumber.store(0, std::memory_order_relaxed);
        isDownbeat.store(true, std::memory_order_relaxed);
        cancelCountIn();
    }

    void setEnabled(bool isEnabled) noexcept {
        if (isEnabled) {
            auto expected = RunState::disabled;
            runState.compare_exchange_strong(expected, RunState::startPending, std::memory_order_acq_rel,
                                             std::memory_order_relaxed);
        } else {
            runState.store(RunState::disabled, std::memory_order_release);
        }
    }

    [[nodiscard]] bool isEnabled() const noexcept {
        const auto state = runState.load(std::memory_order_acquire);
        return state != RunState::disabled;
    }

    void setBpm(double newBpm) noexcept {
        const double clampedBpm = std::clamp(newBpm, devpiano::core::TapTempoCalculator::kMinBpm,
                                             devpiano::core::TapTempoCalculator::kMaxBpm);
        bpm.store(clampedBpm, std::memory_order_relaxed);
    }

    [[nodiscard]] double getBpm() const noexcept {
        return bpm.load(std::memory_order_relaxed);
    }

    void setTimeSignature(devpiano::core::TimeSignature sig) noexcept {
        timeSignature.store(sig, std::memory_order_relaxed);
    }

    [[nodiscard]] devpiano::core::TimeSignature getTimeSignature() const noexcept {
        return timeSignature.load(std::memory_order_relaxed);
    }

    void setVolume(float newVolume) noexcept {
        volume.store(std::clamp(newVolume, 0.0f, 1.0f), std::memory_order_relaxed);
    }

    [[nodiscard]] float getVolume() const noexcept {
        return volume.load(std::memory_order_relaxed);
    }

    [[nodiscard]] int getCurrentBeatNumber() const noexcept {
        return currentBeatNumber.load(std::memory_order_relaxed);
    }

    [[nodiscard]] bool getIsDownbeat() const noexcept {
        return isDownbeat.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::uint32_t getBeatSequence() const noexcept {
        return beatSequence.load(std::memory_order_acquire);
    }
    void armCountIn(int beats) noexcept {
        if (beats <= 0) {
            cancelCountIn();
            return;
        }
        countInArmed.store(true, std::memory_order_release);
        countInRemainingBeats.store(beats, std::memory_order_release);
        pendingCountInCancel.store(false, std::memory_order_release);
        pendingCountInBeats.store(beats, std::memory_order_release);
        auto expected = RunState::disabled;
        runState.compare_exchange_strong(expected, RunState::startPending, std::memory_order_acq_rel,
                                         std::memory_order_relaxed);
    }

    void cancelCountIn() noexcept {
        countInArmed.store(false, std::memory_order_release);
        countInRemainingBeats.store(0, std::memory_order_release);
        pendingCountInBeats.store(0, std::memory_order_release);
        pendingCountInCancel.store(true, std::memory_order_release);
    }

    [[nodiscard]] int getCountInRemainingBeats() const noexcept {
        return countInRemainingBeats.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool isCountInArmed() const noexcept {
        return countInArmed.load(std::memory_order_acquire);
    }

    [[nodiscard]] int consumeCountInStartOffset(int numSamples) noexcept {
        if (numSamples <= 0) {
            return -1;
        }

        handlePendingCountInOnAudioThread();

        if (!audioCountInActive || !isEnabled()) {
            return -1;
        }

        const auto currentSig = timeSignature.load(std::memory_order_relaxed);
        const auto denominator = static_cast<double>(devpiano::core::getTimeSignatureDenominator(currentSig));
        const double currentBpm = bpm.load(std::memory_order_relaxed);
        const double samplesPerBeat = (sampleRate * 60.0 / currentBpm) * (4.0 / denominator);
        if (samplesPerBeat <= 0.0) {
            return -1;
        }

        rebaseBeatPhase(samplesPerBeat);

        const double samplesRemainingInBeat = std::max(0.0, samplesPerBeat - samplePositionInBeat);
        const double samplesUntilTargetDownbeat = audioCountInRemainingBeats <= 0
            ? 0.0
            : samplesRemainingInBeat + static_cast<double>(audioCountInRemainingBeats - 1) * samplesPerBeat;
        const auto roundedSamples = std::ceil(samplesUntilTargetDownbeat - 1.0e-9);

        if (roundedSamples < static_cast<double>(numSamples)) {
            const auto offset = static_cast<int>(std::max(0.0, roundedSamples));
            audioCountInActive = false;
            countInArmed.store(false, std::memory_order_release);
            countInRemainingBeats.store(0, std::memory_order_release);
            return offset;
        }

        return -1;
    }

    /// Process and mix metronome clicks into the provided audio buffer.
    /// Thread safety: Audio thread only. Lock-free and zero-allocation.
    void processAndMix(juce::AudioBuffer<float>* buffer, int startSample, int numSamples) noexcept {
        if (buffer == nullptr || numSamples <= 0) {
            return;
        }

        handlePendingCountInOnAudioThread();

        const auto state = runState.load(std::memory_order_acquire);
        if (state == RunState::disabled) {
            pulseActive = false;
            pulseEnvelope = 0.0f;
            return;
        }

        const auto numChannels = buffer->getNumChannels();
        if (numChannels <= 0) {
            return;
        }

        handleStartPendingOnAudioThread();
        if (!isEnabled()) {
            pulseActive = false;
            pulseEnvelope = 0.0f;
            return;
        }

        const float masterVol = volume.load(std::memory_order_relaxed);
        if (masterVol <= 0.0001f) {
            // Still advance timing even if silent so UI pulse stays in sync
            advanceTimingOnly(numSamples);
            return;
        }

        const auto currentSig = timeSignature.load(std::memory_order_relaxed);
        const int numerator = devpiano::core::getTimeSignatureNumerator(currentSig);
        const auto denominator = static_cast<double>(devpiano::core::getTimeSignatureDenominator(currentSig));
        const double currentBpm = bpm.load(std::memory_order_relaxed);
        const double samplesPerBeat = (sampleRate * 60.0 / currentBpm) * (4.0 / denominator);
        rebaseBeatPhase(samplesPerBeat);

        auto* channel0 = buffer->getWritePointer(0, startSample);
        auto* channel1 = (numChannels > 1) ? buffer->getWritePointer(1, startSample) : nullptr;

        for (int i = 0; i < numSamples; ++i) {
            float sampleVal = 0.0f;
            if (pulseActive) {
                sampleVal = pulseSin * pulseEnvelope * masterVol;
                advancePulseOneSample();
            }

            channel0[i] += sampleVal;
            if (channel1 != nullptr) {
                channel1[i] += sampleVal;
            }

            samplePositionInBeat += 1.0;
            if (samplePositionInBeat >= samplesPerBeat) {
                samplePositionInBeat -= samplesPerBeat;
                currentBeatIndex = (currentBeatIndex + 1) % numerator;
                if (audioCountInActive && audioCountInRemainingBeats > 0) {
                    --audioCountInRemainingBeats;
                    countInRemainingBeats.store(audioCountInRemainingBeats, std::memory_order_release);
                }
                triggerBeat(currentBeatIndex);
            }
        }
    }

private:
    void handlePendingCountInOnAudioThread() noexcept {
        if (pendingCountInCancel.exchange(false, std::memory_order_acq_rel)) {
            audioCountInActive = false;
            audioCountInRemainingBeats = 0;
            countInArmed.store(false, std::memory_order_release);
            countInRemainingBeats.store(0, std::memory_order_release);
        }

        const int newBeats = pendingCountInBeats.exchange(0, std::memory_order_acq_rel);
        if (newBeats > 0) {
            audioCountInActive = true;
            audioCountInRemainingBeats = newBeats;
            countInArmed.store(true, std::memory_order_release);
            countInRemainingBeats.store(newBeats, std::memory_order_release);
            samplePositionInBeat = 0.0;
            currentBeatIndex = 0;
            lastSamplesPerBeat = 0.0;
            runState.store(RunState::active, std::memory_order_release);
            triggerBeat(0);
        }
    }

    void handleStartPendingOnAudioThread() noexcept {
        auto expected = RunState::startPending;
        if (runState.compare_exchange_strong(expected, RunState::active, std::memory_order_acq_rel,
                                             std::memory_order_relaxed)) {
            samplePositionInBeat = 0.0;
            currentBeatIndex = 0;
            lastSamplesPerBeat = 0.0;
            triggerBeat(0);
        }
    }

    void advancePulseOneSample() noexcept {
        if (!pulseActive) {
            return;
        }

        const float nextSin = pulseSin * pulseDeltaCos + pulseCos * pulseDeltaSin;
        const float nextCos = pulseCos * pulseDeltaCos - pulseSin * pulseDeltaSin;
        pulseSin = nextSin;
        pulseCos = nextCos;
        pulseEnvelope *= pulseDecay;
        if (pulseEnvelope < 0.0005f) {
            pulseActive = false;
            pulseEnvelope = 0.0f;
        }
    }

    void rebaseBeatPhase(double samplesPerBeat) noexcept {
        if (lastSamplesPerBeat > 0.0 && samplesPerBeat > 0.0 && samplesPerBeat != lastSamplesPerBeat) {
            const auto phase = std::clamp(samplePositionInBeat / lastSamplesPerBeat, 0.0, 1.0);
            samplePositionInBeat = std::min(phase * samplesPerBeat, std::nextafter(samplesPerBeat, 0.0));
        }
        lastSamplesPerBeat = samplesPerBeat;
    }

    void triggerBeat(int beatIdx) noexcept {
        const auto currentSig = timeSignature.load(std::memory_order_relaxed);
        const bool down = (beatIdx == 0);

        float freq = 800.0f;
        float decaySec = 0.020f;
        float amp = 0.70f;

        if (down) {
            freq = 1600.0f;
            decaySec = 0.030f;
            amp = 1.0f;
        } else if (currentSig == devpiano::core::TimeSignature::sixEight && beatIdx == 3) {
            // Secondary accent for 6/8 meter (beat 4 / division 4)
            freq = 1100.0f;
            decaySec = 0.025f;
            amp = 0.85f;
        }

        const auto omega = static_cast<float>(kTwoPi * static_cast<double>(freq) / sampleRate);
        pulseDeltaSin = std::sin(omega);
        pulseDeltaCos = std::cos(omega);

        pulseSin = 0.0f;
        pulseCos = 1.0f;
        pulseEnvelope = amp;
        pulseDecay = std::exp(-1.0f / static_cast<float>(sampleRate * static_cast<double>(decaySec)));
        pulseActive = true;

        currentBeatNumber.store(beatIdx, std::memory_order_relaxed);
        isDownbeat.store(down, std::memory_order_relaxed);
        beatSequence.fetch_add(1, std::memory_order_release);
    }

    void advanceTimingOnly(int numSamples) noexcept {
        if (numSamples <= 0) {
            return;
        }

        handlePendingCountInOnAudioThread();

        const auto state = runState.load(std::memory_order_acquire);
        if (state == RunState::disabled) {
            pulseActive = false;
            pulseEnvelope = 0.0f;
            return;
        }

        handleStartPendingOnAudioThread();
        if (!isEnabled()) {
            pulseActive = false;
            pulseEnvelope = 0.0f;
            return;
        }

        const auto currentSig = timeSignature.load(std::memory_order_relaxed);
        const int numerator = devpiano::core::getTimeSignatureNumerator(currentSig);
        const auto denominator = static_cast<double>(devpiano::core::getTimeSignatureDenominator(currentSig));
        const double currentBpm = bpm.load(std::memory_order_relaxed);
        const double samplesPerBeat = (sampleRate * 60.0 / currentBpm) * (4.0 / denominator);
        rebaseBeatPhase(samplesPerBeat);
        for (int i = 0; i < numSamples; ++i) {
            advancePulseOneSample();
            samplePositionInBeat += 1.0;
            if (samplePositionInBeat >= samplesPerBeat) {
                samplePositionInBeat -= samplesPerBeat;
                currentBeatIndex = (currentBeatIndex + 1) % numerator;
                if (audioCountInActive && audioCountInRemainingBeats > 0) {
                    --audioCountInRemainingBeats;
                    countInRemainingBeats.store(audioCountInRemainingBeats, std::memory_order_release);
                }
                triggerBeat(currentBeatIndex);
            }
        }
    }

    double lastSamplesPerBeat = 0.0;

    double sampleRate = 48000.0;
    double samplePositionInBeat = 0.0;
    int currentBeatIndex = 0;

    bool pulseActive = false;
    float pulseSin = 0.0f;
    float pulseCos = 1.0f;
    float pulseDeltaSin = 0.0f;
    float pulseDeltaCos = 1.0f;
    float pulseEnvelope = 0.0f;
    float pulseDecay = 0.0f;

    std::atomic<RunState> runState { RunState::disabled };
    std::atomic<double> bpm { 120.0 };
    std::atomic<devpiano::core::TimeSignature> timeSignature { devpiano::core::TimeSignature::fourFour };
    std::atomic<float> volume { 0.7f };
    std::atomic<int> currentBeatNumber { 0 };
    std::atomic<bool> isDownbeat { true };
    std::atomic<std::uint32_t> beatSequence { 0 };
    bool audioCountInActive = false;
    int audioCountInRemainingBeats = 0;

    std::atomic<bool> countInArmed { false };
    std::atomic<int> countInRemainingBeats { 0 };
    std::atomic<int> pendingCountInBeats { 0 };
    std::atomic<bool> pendingCountInCancel { false };
};

} // namespace devpiano::audio
