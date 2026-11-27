#pragma once

#include "PianoDspMath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <string_view>
#include <vector>

class RoomReverbEngineTest;

namespace devpiano::audio {

/**
 * Reverb space presets for physical acoustic room modeling (Phase 31-B / 44-1).
 */
enum class ReverbSpace : std::uint8_t {
    studio = 0, ///< Studio: compact space (RT60 ~ 0.6 s), tight reflections, maximum dry note clarity.
    chamber = 1, ///< Chamber: intimate hall (RT60 ~ 1.5 s), warm wooden decay, balanced solo intimacy.
    concertHall = 2 ///< Concert Hall: large auditorium (RT60 ~ 2.4 s), spacious diffuse tail, high immersion.
};

/**
 * RoomReverbEngine (Phase 44-1 / 46-3)
 *
 * Stereo early-reflection taps feeding an 8-line Householder feedback delay
 * network with one-pole damping in every feedback branch. The dry/wet mix uses
 * constant-power gains from boundedSinCos; wet=0 is an exact bypass.
 *
 * No external IR, no allocation in processStereo, no locks and no library
 * trigonometric calls in the per-sample loop. Delay and tap lengths are scaled
 * from the 48 kHz reference by the active sample rate.
 */
class RoomReverbEngine {
public:
    static constexpr int kFdnSize = 8;
    static constexpr int kEarlyTaps = 4;
    static constexpr float kDefaultWetLevel = 0.20f;

    RoomReverbEngine() noexcept {
        updatePresetParameters();
        currentWet = targetWet;
    }

    void prepare(double newSampleRate) noexcept {
        if (newSampleRate <= 0.0) {
            return;
        }
        sampleRate = newSampleRate;

        // Prime base delays in samples at 48 kHz. They are scaled by the
        // active rate so RT60 remains a time-domain property.
        constexpr std::array<int, kFdnSize> kBaseDelays { 1601, 1861, 2027, 2237, 2371, 2539, 2683, 2903 };
        for (std::size_t i = 0; i < kFdnSize; ++i) {
            const auto samples = std::max(
                32, static_cast<int>(std::lround(static_cast<double>(kBaseDelays[i]) * sampleRate / 48000.0)));
            delaySamples[i] = static_cast<std::size_t>(samples);
            delayLines[i].assign(delaySamples[i], 0.0f);
        }

        earlySize = std::max(64, static_cast<int>(std::lround(0.050 * sampleRate)));
        earlyLeft.assign(static_cast<std::size_t>(earlySize), 0.0f);
        earlyRight.assign(static_cast<std::size_t>(earlySize), 0.0f);

        updatePresetParameters();
        reset();
    }

    void reset() noexcept {
        for (auto& line : delayLines) {
            std::fill(line.begin(), line.end(), 0.0f);
        }
        delayWrite.fill(0);
        dampingState.fill(0.0f);
        std::fill(earlyLeft.begin(), earlyLeft.end(), 0.0f);
        std::fill(earlyRight.begin(), earlyRight.end(), 0.0f);
        earlyWrite = 0;
        tailSilent = false;
        snapToTarget();
    }

    void snapToTarget() noexcept {
        currentWet = targetWet;
    }

    void setSpace(ReverbSpace newSpace) noexcept {
        if (space != newSpace) {
            space = newSpace;
            updatePresetParameters();
        }
    }

    void setWetLevel(float newWet, bool immediateSnap = false) noexcept {
        targetWet = std::clamp(newWet, 0.0f, 1.0f);
        if (immediateSnap) {
            snapToTarget();
        }
    }

    [[nodiscard]] float getWetLevel() const noexcept {
        return targetWet;
    }

    void processStereo(float* leftChannel, float* rightChannel, int numSamples) noexcept {
        if (leftChannel == nullptr || rightChannel == nullptr || numSamples <= 0) {
            return;
        }
        if (targetWet <= 1.0e-4f && currentWet <= 1.0e-4f) {
            return;
        }

        auto hasInput = false;
        for (auto sample = 0; sample < numSamples; ++sample) {
            if (leftChannel[sample] != 0.0f || rightChannel[sample] != 0.0f) {
                hasInput = true;
                break;
            }
        }
        if (!hasInput) {
            if (tailSilent || maxTailState() < 1.0e-4f) {
                tailSilent = true;
                for (auto sample = 0; sample < numSamples; ++sample) {
                    leftChannel[sample] = 0.0f;
                    rightChannel[sample] = 0.0f;
                }
                return;
            }
        } else {
            tailSilent = false;
        }

        constexpr float kSmoothCoeff = 0.998f;
        constexpr float kInputGain = 0.22f;
        constexpr float kWetOutputGain = 0.55f;
        constexpr float kEarlyGain = 0.35f;

        for (int sample = 0; sample < numSamples; ++sample) {
            if (std::abs(currentWet - targetWet) > 1.0e-4f) {
                currentWet = targetWet + (currentWet - targetWet) * kSmoothCoeff;
            } else {
                currentWet = targetWet;
            }

            const auto inL = leftChannel[sample];
            const auto inR = rightChannel[sample];

            // Stereo early reflections: keep the bridge/tail L/R geometry.
            earlyLeft[static_cast<std::size_t>(earlyWrite)] = inL;
            earlyRight[static_cast<std::size_t>(earlyWrite)] = inR;
            const auto readEarly = [&](const std::vector<float>& line, double seconds) {
                const auto delay = std::max(1, static_cast<int>(std::lround(seconds * sampleRate)));
                const auto index = (earlyWrite + earlySize - delay + earlySize) % earlySize;
                return line[static_cast<std::size_t>(index)];
            };
            const auto earlyL = kEarlyGain
                * (0.52f * readEarly(earlyLeft, 0.0071) + 0.20f * readEarly(earlyRight, 0.0113)
                   + 0.11f * readEarly(earlyLeft, 0.0179) + 0.06f * readEarly(earlyRight, 0.0233));
            const auto earlyR = kEarlyGain
                * (0.52f * readEarly(earlyRight, 0.0071) + 0.20f * readEarly(earlyLeft, 0.0113)
                   + 0.11f * readEarly(earlyRight, 0.0179) + 0.06f * readEarly(earlyLeft, 0.0233));

            // Read FDN delay lines and apply frequency-dependent damping.
            std::array<float, kFdnSize> delayOut {};
            std::array<float, kFdnSize> damped {};
            for (std::size_t i = 0; i < kFdnSize; ++i) {
                const auto index = delayWrite[i];
                delayOut[i] = delayLines[i][index];
                dampingState[i] = delayOut[i] * (1.0f - dampingPole) + dampingState[i] * dampingPole;
                if (std::abs(dampingState[i]) < 1.0e-7f) {
                    dampingState[i] = 0.0f;
                }
                damped[i] = dampingState[i];
            }

            // Normalized 8-line Householder feedback: A = I - (2/N) 1 1^T.
            // The transform is orthogonal and preserves branch energy.
            const auto feedback = householderFeedback(damped);
            for (std::size_t i = 0; i < kFdnSize; ++i) {
                const auto injected = (i % 2 == 0) ? kInputGain * inL : kInputGain * inR;
                const auto next = injected + feedback[i] * feedbackGain[i];
                delayLines[i][delayWrite[i]] = std::abs(next) < 1.0e-7f ? 0.0f : next;
                if (++delayWrite[i] >= delaySamples[i]) {
                    delayWrite[i] = 0;
                }
            }
            if (++earlyWrite >= earlySize) {
                earlyWrite = 0;
            }

            const auto wetLeft = static_cast<float>(
                kWetOutputGain * 0.25f * (delayOut[0] + delayOut[2] + delayOut[4] + delayOut[6]) + earlyL);
            const auto wetRight = static_cast<float>(
                kWetOutputGain * 0.25f * (delayOut[1] + delayOut[3] + delayOut[5] + delayOut[7]) + earlyR);
            const auto mix
                = devpiano::audio::dsp::boundedSinCos(static_cast<double>(currentWet) * devpiano::audio::dsp::kHalfPi);
            const auto dryGain = static_cast<float>(mix.cosVal);
            const auto wetGain = static_cast<float>(mix.sinVal);
            leftChannel[sample] = dryGain * inL + wetGain * wetLeft;
            rightChannel[sample] = dryGain * inR + wetGain * wetRight;
        }
    }

    [[nodiscard]] static constexpr std::string_view toIdentifier(ReverbSpace s) noexcept {
        switch (s) {
        case ReverbSpace::studio:
            return "studio";
        case ReverbSpace::chamber:
            return "chamber";
        case ReverbSpace::concertHall:
            return "concert_hall";
        }
        return "chamber";
    }

    [[nodiscard]] static ReverbSpace fromIdentifier(std::string_view id) noexcept {
        if (id == "studio") {
            return ReverbSpace::studio;
        }
        if (id == "concert_hall") {
            return ReverbSpace::concertHall;
        }
        return ReverbSpace::chamber;
    }

private:
    [[nodiscard]] static std::array<float, kFdnSize>
    householderFeedback(const std::array<float, kFdnSize>& input) noexcept {
        auto sum = 0.0f;
        for (const auto value : input) {
            sum += value;
        }
        const auto correction = sum * (2.0f / static_cast<float>(kFdnSize));
        std::array<float, kFdnSize> output {};
        for (std::size_t i = 0; i < kFdnSize; ++i) {
            output[i] = input[i] - correction;
        }
        return output;
    }

    [[nodiscard]] float maxTailState() const noexcept {
        auto maximum = 0.0f;
        for (const auto& line : delayLines) {
            for (const auto value : line) {
                maximum = std::max(maximum, std::abs(value));
            }
        }
        for (const auto value : dampingState) {
            maximum = std::max(maximum, std::abs(value));
        }
        for (const auto value : earlyLeft) {
            maximum = std::max(maximum, std::abs(value));
        }
        for (const auto value : earlyRight) {
            maximum = std::max(maximum, std::abs(value));
        }
        return maximum;
    }

    void updatePresetParameters() noexcept {
        float presetPoleAt48k = 0.60f;
        switch (space) {
        case ReverbSpace::studio:
            presetPoleAt48k = 0.35f;
            presetFeedback = 0.83f;
            break;
        case ReverbSpace::chamber:
            presetPoleAt48k = 0.28f;
            presetFeedback = 0.945f;
            break;
        case ReverbSpace::concertHall:
            presetPoleAt48k = 0.20f;
            presetFeedback = 0.967f;
            break;
        }

        // 一阶阻尼极点在 48 kHz 参考下定义；换采样率时按同一物理截止换算，
        // 使 44.1/48/96/192 kHz 的高频衰减保持同一条时间常数曲线。
        dampingPole = static_cast<float>(std::pow(static_cast<double>(presetPoleAt48k), 48000.0 / sampleRate));
        feedbackGain.fill(presetFeedback);
    }

    ReverbSpace space = ReverbSpace::chamber;
    double sampleRate = 48000.0;
    float dampingPole = 0.60f;
    float targetWet = kDefaultWetLevel;
    float currentWet = kDefaultWetLevel;

    std::array<std::size_t, kFdnSize> delaySamples {};
    std::array<std::size_t, kFdnSize> delayWrite {};
    std::array<std::vector<float>, kFdnSize> delayLines;
    std::array<float, kFdnSize> dampingState {};
    std::array<float, kFdnSize> feedbackGain {};
    float presetFeedback = 0.945f;

    std::vector<float> earlyLeft;
    std::vector<float> earlyRight;
    int earlySize = 2400;
    int earlyWrite = 0;
    bool tailSilent = false;

    friend class ::RoomReverbEngineTest;
};

} // namespace devpiano::audio