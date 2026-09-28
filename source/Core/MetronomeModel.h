#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>

namespace devpiano::core {

enum class TimeSignature : std::uint8_t {
    twoFour = 0, // 2/4
    threeFour = 1, // 3/4
    fourFour = 2, // 4/4
    sixEight = 3, // 6/8
};

[[nodiscard]] constexpr int getTimeSignatureNumerator(TimeSignature sig) noexcept {
    switch (sig) {
    case TimeSignature::twoFour:
        return 2;
    case TimeSignature::threeFour:
        return 3;
    case TimeSignature::fourFour:
        return 4;
    case TimeSignature::sixEight:
        return 6;
    }
    return 4;
}

[[nodiscard]] constexpr int getTimeSignatureDenominator(TimeSignature sig) noexcept {
    switch (sig) {
    case TimeSignature::twoFour:
    case TimeSignature::threeFour:
    case TimeSignature::fourFour:
        return 4;
    case TimeSignature::sixEight:
        return 8;
    }
    return 4;
}

[[nodiscard]] constexpr const char* getTimeSignatureName(TimeSignature sig) noexcept {
    switch (sig) {
    case TimeSignature::twoFour:
        return "2/4";
    case TimeSignature::threeFour:
        return "3/4";
    case TimeSignature::fourFour:
        return "4/4";
    case TimeSignature::sixEight:
        return "6/8";
    }
    return "4/4";
}

enum class CountInBars : std::uint8_t {
    none = 0,
    oneBar = 1,
    twoBars = 2,
};

[[nodiscard]] constexpr int getCountInBarCount(CountInBars countIn) noexcept {
    switch (countIn) {
    case CountInBars::none:
        return 0;
    case CountInBars::oneBar:
        return 1;
    case CountInBars::twoBars:
        return 2;
    }
    return 0;
}

/// Lightweight rolling tap tempo calculator.
/// Deterministic and allocation-free.
class TapTempoCalculator {
public:
    static constexpr int kMaxIntervals = 3;
    static constexpr int kMaxTaps = kMaxIntervals + 1;
    static constexpr double kMinBpm = 40.0;
    static constexpr double kMaxBpm = 280.0;
    static constexpr double kTimeoutSeconds = 2.0;

    void recordTap(double currentTimeSeconds) noexcept {
        if (tapCount > 0 && (currentTimeSeconds - lastTapTime) > kTimeoutSeconds) {
            reset();
        }

        if (tapCount < kMaxTaps) {
            tapTimes[static_cast<std::size_t>(tapCount++)] = currentTimeSeconds;
        } else {
            for (std::size_t i = 0; i < static_cast<std::size_t>(kMaxTaps - 1); ++i) {
                tapTimes[i] = tapTimes[i + 1];
            }
            tapTimes[static_cast<std::size_t>(kMaxTaps - 1)] = currentTimeSeconds;
        }
        lastTapTime = currentTimeSeconds;
    }

    [[nodiscard]] std::optional<double> calculateBpm() const noexcept {
        if (tapCount < 2) {
            return std::nullopt;
        }
        double totalInterval = 0.0;
        for (int i = 1; i < tapCount; ++i) {
            totalInterval += (tapTimes[static_cast<std::size_t>(i)] - tapTimes[static_cast<std::size_t>(i - 1)]);
        }
        const double avgInterval = totalInterval / static_cast<double>(tapCount - 1);
        if (avgInterval <= 0.0001) {
            return std::nullopt;
        }
        const double bpm = 60.0 / avgInterval;
        return std::make_optional(std::clamp(bpm, kMinBpm, kMaxBpm));
    }

    void reset() noexcept {
        tapCount = 0;
        lastTapTime = 0.0;
    }

    [[nodiscard]] int getTapCount() const noexcept {
        return tapCount;
    }

private:
    std::array<double, kMaxTaps> tapTimes {};
    int tapCount = 0;
    double lastTapTime = 0.0;
};

} // namespace devpiano::core
