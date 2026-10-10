#pragma once

#include <algorithm>
#include <juce_audio_basics/juce_audio_basics.h>

namespace devpiano::audio {

constexpr int kMinLatencyCapacity = 16384;
constexpr int kMaxLatencyCapacity = 192000;

[[nodiscard]] constexpr int sanitizePluginLatency(int reportedLatency) noexcept {
    if (reportedLatency <= 0) {
        return 0;
    }
    return std::min(reportedLatency, kMaxLatencyCapacity);
}

[[nodiscard]] constexpr int calculateDelayCapacity(int reportedLatency) noexcept {
    const int safeLatency = sanitizePluginLatency(reportedLatency);
    const int doubled = safeLatency > (kMaxLatencyCapacity / 2) ? kMaxLatencyCapacity : safeLatency * 2;
    return std::clamp(doubled, kMinLatencyCapacity, kMaxLatencyCapacity);
}

/**
 * BoundedStereoDelayLine
 *
 * Pre-allocated ring buffer delay line for real-time latency compensation.
 * Guarantees zero heap allocation and lock-free execution in audio callbacks.
 * Clamps delay to bounded pre-allocated capacity, providing bounded cost across block boundaries.
 */
class BoundedStereoDelayLine {
public:
    void prepare(int maxDelaySamples, int numChannels = 2) {
        capacity = std::clamp(maxDelaySamples, kMinLatencyCapacity, kMaxLatencyCapacity);
        channels = std::max(1, numChannels);
        buffer.setSize(channels, capacity, false, false, true);
        buffer.clear();
        writePos = 0;
        currentDelay = 0;
    }

    void setDelay(int delaySamples) noexcept {
        currentDelay = std::clamp(delaySamples, 0, capacity);
    }

    [[nodiscard]] int getDelay() const noexcept {
        return currentDelay;
    }

    [[nodiscard]] int getCapacity() const noexcept {
        return capacity;
    }

    [[nodiscard]] bool isOverCapacity(int requestedDelay) const noexcept {
        return requestedDelay > capacity;
    }

    void reset() noexcept {
        buffer.clear();
        writePos = 0;
    }

    void process(const juce::AudioBuffer<float>& inAudio, juce::AudioBuffer<float>& outAudio, int numSamples) noexcept {
        if (numSamples <= 0) {
            return;
        }

        const auto numChannels = std::min({ channels, inAudio.getNumChannels(), outAudio.getNumChannels() });

        for (int i = 0; i < numSamples; ++i) {
            int readPos = writePos - currentDelay;
            if (readPos < 0) {
                readPos += capacity;
            }

            for (int ch = 0; ch < numChannels; ++ch) {
                const auto value = inAudio.getSample(ch, i);
                outAudio.setSample(ch, i, currentDelay == 0 ? value : buffer.getSample(ch, readPos));
                buffer.setSample(ch, writePos, value);
            }

            writePos = (writePos + 1);
            if (writePos >= capacity) {
                writePos = 0;
            }
        }
    }

private:
    juce::AudioBuffer<float> buffer;
    int capacity = 16384;
    int channels = 2;
    int writePos = 0;
    int currentDelay = 0;
};

} // namespace devpiano::audio
