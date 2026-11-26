#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <juce_core/juce_core.h>

namespace devpiano::audio {

enum class LidPosition : std::uint8_t {
    fullOpen = 0,
    halfStick = 1,
    closed = 2,
};

// 三角钢琴琴盖的近场早期反射。两级 Schroeder 全通扩散器保持 |H|=1，
// 用相位群延迟提供纵深；不再用固定三抽头前馈梳状滤波制造周期性凹陷。
struct LidAcoustics {
    static constexpr std::size_t delaySize = 4096;

    struct AllpassStage {
        std::array<float, delaySize> buffer {};
        std::size_t writePos = 0;
        std::size_t delay = 1;
        float feedback = 0.0f;

        void setParameters(std::size_t newDelay, float newFeedback) noexcept {
            delay = std::clamp<std::size_t>(newDelay, 1, delaySize - 1);
            feedback = std::clamp(newFeedback, 0.0f, 0.95f);
        }

        void reset() noexcept {
            buffer.fill(0.0f);
            writePos = 0;
        }

        [[nodiscard]] float process(float input) noexcept {
            const auto readPos = (writePos + delaySize - delay) % delaySize;
            const auto delayed = buffer[readPos];
            const auto output = delayed - feedback * input;
            buffer[writePos] = input + feedback * output;
            writePos = (writePos + 1) % delaySize;
            return output;
        }
    };

    AllpassStage leftStage1;
    AllpassStage rightStage1;
    AllpassStage leftStage2;
    AllpassStage rightStage2;

    std::size_t delay1 = 141;
    std::size_t delay2 = 507;
    float feedback1 = 0.72f;
    float feedback2 = 0.45f;
    LidPosition position = LidPosition::fullOpen;
    float lpLeft = 0.0f;
    float lpRight = 0.0f;
    float lpCoeff = 0.0f;
    float outputGain = 1.0f;
    int tailSamples = 0;

    // 2x2 正交微交叉散射：保持总功率，只交换少量左右声道信息。
    static constexpr float crossMix = 0.10f;
    static constexpr float crossDirect = 0.99498745f;

    LidAcoustics() noexcept {
        updateCoefficients(48000.0);
    }

    void updateCoefficients(double sampleRate) noexcept {
        if (sampleRate <= 0.0) {
            return;
        }
        const auto sr = static_cast<float>(sampleRate);
        const auto toSamples = [sampleRate](double seconds) {
            const auto raw = std::lround(seconds * sampleRate);
            return static_cast<std::size_t>(
                std::clamp<std::int64_t>(static_cast<std::int64_t>(raw), 1, static_cast<std::int64_t>(delaySize - 1)));
        };
        delay1 = toSamples(141.0 / 48000.0);
        delay2 = toSamples(507.0 / 48000.0);

        // 开合度用反馈系数与低通截止区分：closed 尾巴更长、高频更暗。
        float cutoff = 0.0f;
        if (position == LidPosition::fullOpen) {
            feedback1 = 0.72f;
            feedback2 = 0.45f;
            outputGain = 1.0f;
        } else if (position == LidPosition::halfStick) {
            feedback1 = 0.78f;
            feedback2 = 0.55f;
            outputGain = 0.94f;
            cutoff = 6500.0f;
        } else {
            feedback1 = 0.86f;
            feedback2 = 0.65f;
            outputGain = 0.88f;
            cutoff = 2600.0f;
        }

        const auto safeNyquist = sr * 0.480f;
        if (cutoff <= 0.0f) {
            lpCoeff = 0.0f;
        } else {
            const auto safeFc = std::min(cutoff, safeNyquist);
            lpCoeff = std::exp(-juce::MathConstants<float>::twoPi * safeFc / sr);
        }

        leftStage1.setParameters(delay1, feedback1);
        rightStage1.setParameters(delay1, feedback1);
        leftStage2.setParameters(delay2, feedback2);
        rightStage2.setParameters(delay2, feedback2);

        // 保守尾音预算：最慢反馈衰减到 -60 dB 所需的回声数乘最长延迟。
        // 不是无限尾音；PianoPedalAcoustics 用它决定何时停止渲染。
        const auto slowestFeedback = std::max(feedback1, feedback2);
        const auto longestDelay = std::max(delay1, delay2);
        const auto echoes = slowestFeedback > 0.0f && slowestFeedback < 1.0f
            ? std::ceil(std::log(1.0e-3f) / std::log(slowestFeedback))
            : 0.0f;
        tailSamples = std::max(1,
                               static_cast<int>(std::ceil(echoes * static_cast<float>(longestDelay)))
                                   + 2 * static_cast<int>(longestDelay));
    }

    void setPosition(LidPosition newPosition, double sampleRate) noexcept {
        position = newPosition;
        updateCoefficients(sampleRate);
    }

    void reset() noexcept {
        leftStage1.reset();
        rightStage1.reset();
        leftStage2.reset();
        rightStage2.reset();
        lpLeft = 0.0f;
        lpRight = 0.0f;
    }

    [[nodiscard]] int tailDrainSamples() const noexcept {
        return tailSamples;
    }

    void processStereo(float& left, float& right) noexcept {
        if (lpCoeff > 0.0f) {
            lpLeft = (1.0f - lpCoeff) * left + lpCoeff * lpLeft;
            lpRight = (1.0f - lpCoeff) * right + lpCoeff * lpRight;
            left = lpLeft;
            right = lpRight;
        }

        left = leftStage1.process(left);
        right = rightStage1.process(right);
        left = leftStage2.process(left);
        right = rightStage2.process(right);

        const auto scatteredLeft = crossDirect * left + crossMix * right;
        const auto scatteredRight = crossDirect * right - crossMix * left;
        left = outputGain * scatteredLeft;
        right = outputGain * scatteredRight;
    }
};

} // namespace devpiano::audio
