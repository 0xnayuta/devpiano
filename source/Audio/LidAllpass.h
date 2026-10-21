#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace devpiano::audio::dsp {

/// Full-open lid reflection path: a unity-magnitude first-order allpass in
/// Mid and Side. The delayed reflection is redistributed in time without a
/// direct-plus-sparse-FIR amplitude comb. Half-stick and closed lid paths are
/// intentionally not part of this class.
class LidAllpass {
public:
    static constexpr std::size_t maxDelay = 1024;
    static constexpr float direct = 0.5f;
    static constexpr float quietThreshold = 1e-7f;

    void prepare(double sampleRate) noexcept {
        if (sampleRate <= 0.0) {
            midDelay = 141;
            sideDelay = 344;
        } else {
            midDelay = static_cast<std::size_t>(
                std::clamp(std::lround(141.0 * sampleRate / 48000.0), 1L, static_cast<long>(maxDelay - 1)));
            sideDelay = static_cast<std::size_t>(
                std::clamp(std::lround(344.0 * sampleRate / 48000.0), 1L, static_cast<long>(maxDelay - 1)));
        }
        reset();
    }

    void reset() noexcept {
        midState.fill(0.0f);
        sideState.fill(0.0f);
        writePos = 0;
        quietSamples = maxDelay;
    }

    void processStereo(float& left, float& right) noexcept {
        const auto mid = 0.5f * (left + right);
        const auto side = 0.5f * (left - right);
        const auto midPos = (writePos + maxDelay - midDelay) % maxDelay;
        const auto sidePos = (writePos + maxDelay - sideDelay) % maxDelay;

        const auto outMid = direct * mid + midState[midPos];
        const auto outSide = direct * side + sideState[sidePos];
        midState[writePos] = mid - direct * outMid;
        sideState[writePos] = side - direct * outSide;

        if (std::max({ std::abs(mid), std::abs(side), std::abs(outMid), std::abs(outSide) }) > quietThreshold) {
            quietSamples = 0;
        } else if (quietSamples < maxDelay) {
            ++quietSamples;
        }

        left = outMid + outSide;
        right = outMid - outSide;
        writePos = (writePos + 1) % maxDelay;
    }

    [[nodiscard]] bool isActive() const noexcept {
        return quietSamples < std::max(midDelay, sideDelay);
    }

private:
    std::array<float, maxDelay> midState {};
    std::array<float, maxDelay> sideState {};
    std::size_t writePos = 0;
    std::size_t midDelay = 141;
    std::size_t sideDelay = 344;
    std::size_t quietSamples = maxDelay;
};

} // namespace devpiano::audio::dsp
