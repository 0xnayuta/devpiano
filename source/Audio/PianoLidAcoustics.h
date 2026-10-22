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

// The existing PianoSynthVoice lid stage, extracted without changing its
// fixed taps, coefficients or processing formula.
struct LidAcoustics {
    static constexpr std::size_t delaySize = 1024;
    std::array<float, delaySize> leftBuffer {};
    std::array<float, delaySize> rightBuffer {};
    std::size_t writePos = 0;
    LidPosition position = LidPosition::fullOpen;
    float lpLeft = 0.0f;
    float lpRight = 0.0f;
    float lpCoeff = 0.0f;

    void updateCoefficients(double sampleRate) noexcept {
        if (sampleRate <= 0.0) {
            return;
        }
        const auto sr = static_cast<float>(sampleRate);
        const auto safeNyquist = sr * 0.480f;
        if (position == LidPosition::fullOpen) {
            lpCoeff = 0.0f;
        } else if (position == LidPosition::halfStick) {
            constexpr float fc = 6500.0f;
            const auto safeFc = std::min(fc, safeNyquist);
            lpCoeff = std::exp(-juce::MathConstants<float>::twoPi * safeFc / sr);
        } else {
            constexpr float fc = 2600.0f;
            const auto safeFc = std::min(fc, safeNyquist);
            lpCoeff = std::exp(-juce::MathConstants<float>::twoPi * safeFc / sr);
        }
    }

    void setPosition(LidPosition newPosition, double sampleRate) noexcept {
        position = newPosition;
        updateCoefficients(sampleRate);
    }

    void reset() noexcept {
        leftBuffer.fill(0.0f);
        rightBuffer.fill(0.0f);
        writePos = 0;
        lpLeft = 0.0f;
        lpRight = 0.0f;
    }

    void processStereo(float& left, float& right) noexcept {
        if (lpCoeff > 0.0f) {
            lpLeft = (1.0f - lpCoeff) * left + lpCoeff * lpLeft;
            lpRight = (1.0f - lpCoeff) * right + lpCoeff * lpRight;
            left = lpLeft;
            right = lpRight;
        }

        leftBuffer[writePos] = left;
        rightBuffer[writePos] = right;

        const auto tap1Pos = (writePos + delaySize - 141) % delaySize;
        const auto tap2Pos = (writePos + delaySize - 344) % delaySize;
        const auto tap3Pos = (writePos + delaySize - 507) % delaySize;

        float gDirect = 0.82f;
        float g1 = 0.10f;
        float g2 = 0.06f;
        float g3 = 0.04f;

        if (position == LidPosition::halfStick) {
            gDirect = 0.75f;
            g1 = 0.14f;
            g2 = 0.09f;
            g3 = 0.06f;
        } else if (position == LidPosition::closed) {
            gDirect = 0.68f;
            g1 = 0.18f;
            g2 = 0.12f;
            g3 = 0.08f;
        }

        const auto earlyL = g1 * leftBuffer[tap1Pos] + g2 * rightBuffer[tap2Pos] + g3 * leftBuffer[tap3Pos];
        const auto earlyR = g1 * rightBuffer[tap1Pos] + g2 * leftBuffer[tap2Pos] + g3 * rightBuffer[tap3Pos];

        left = left * gDirect + earlyL;
        right = right * gDirect + earlyR;

        writePos = (writePos + 1) % delaySize;
    }
};

} // namespace devpiano::audio
