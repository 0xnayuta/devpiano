#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace devpiano::recording {

inline constexpr double minimumTimelineSampleRate = 8000.0;
inline constexpr double maximumTimelineSampleRate = 384000.0;

[[nodiscard]] inline bool isUsableTimelineSampleRate(double sampleRate) noexcept {
    return std::isfinite(sampleRate) && sampleRate >= 1.0 && sampleRate <= maximumTimelineSampleRate;
}

[[nodiscard]] inline bool isSupportedTimelineSampleRate(double sampleRate) noexcept {
    return sampleRate >= minimumTimelineSampleRate && isUsableTimelineSampleRate(sampleRate);
}

[[nodiscard]] inline std::optional<std::int64_t> checkedSampleCount(double samples) noexcept {
    constexpr auto exclusiveLimit = 9223372036854775808.0;
    if (!std::isfinite(samples) || samples < 0.0 || samples >= exclusiveLimit) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(samples);
}

[[nodiscard]] inline std::optional<std::int64_t> checkedScaleSamples(std::int64_t samples, double ratio) noexcept {
    if (samples < 0 || !std::isfinite(ratio) || ratio <= 0.0) {
        return std::nullopt;
    }
    if (ratio == 1.0) {
        return samples;
    }
    return checkedSampleCount(std::round(static_cast<double>(samples) * ratio));
}

[[nodiscard]] inline std::optional<std::int64_t> checkedAddSamples(std::int64_t samples,
                                                                   std::int64_t additionalSamples) noexcept {
    if (samples < 0 || additionalSamples < 0
        || samples > std::numeric_limits<std::int64_t>::max() - additionalSamples) {
        return std::nullopt;
    }
    return samples + additionalSamples;
}

[[nodiscard]] inline bool isRepresentableTimelineLength(std::int64_t length, double sampleRate) noexcept {
    if (length < 0 || !isUsableTimelineSampleRate(sampleRate)) {
        return false;
    }
    const auto maximumPlaybackRatio = 2.0 * maximumTimelineSampleRate / sampleRate;
    const auto scaled = checkedSampleCount(std::ceil(static_cast<double>(length) * maximumPlaybackRatio));
    return scaled.has_value() && *scaled <= std::numeric_limits<std::int64_t>::max() - std::numeric_limits<int>::max();
}

} // namespace devpiano::recording
