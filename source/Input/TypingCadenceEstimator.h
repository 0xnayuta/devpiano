#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace devpiano::input {

/// Typing Cadence Dynamics & Velocity Humanizer (Phase 35-B)
///
/// Converts computer keyboard typing intervals (inter-keystroke delta time) into
/// expressive musical velocity dynamics and adds subtle deterministic humanization.
class TypingCadenceEstimator {
public:
    static constexpr float kDefaultBaseVelocity = 100.0f / 127.0f; // ~0.787f
    static constexpr float kMinDynamicVelocity = 76.0f / 127.0f; // ~0.598f
    static constexpr float kMaxDynamicVelocity = 122.0f / 127.0f; // ~0.960f

    static constexpr double kFastIntervalSeconds = 0.060; // <= 60ms: rapid runs / arpeggios
    static constexpr double kSlowIntervalSeconds = 0.500; // >= 500ms: slow lyrical notes
    static constexpr double kIdleTimeoutSeconds = 1.000; // > 1.0s: phrase start reset to base

    TypingCadenceEstimator() noexcept = default;

    void setEnabled(bool isEnabled) noexcept {
        enabled = isEnabled;
    }

    [[nodiscard]] bool isEnabled() const noexcept {
        return enabled;
    }

    void setBaseVelocity(float velocity) noexcept {
        constexpr float kMinMidi = 1.0f / 127.0f;
        baseVelocity = std::clamp(velocity, kMinMidi, 1.0f);
    }

    [[nodiscard]] float getBaseVelocity() const noexcept {
        return baseVelocity;
    }

    void reset() noexcept {
        lastKeystrokeTimeSeconds = 0.0;
        lastEstimatedVelocity = baseVelocity;
    }

    /// Calculate dynamic velocity based on the timestamp of the current keystroke.
    /// Thread safety: message thread only.
    [[nodiscard]] float estimateVelocity(double currentTimestampSeconds) noexcept {
        if (!enabled) {
            lastKeystrokeTimeSeconds = currentTimestampSeconds;
            lastEstimatedVelocity = baseVelocity;
            return baseVelocity;
        }

        float estimated = baseVelocity;
        if (lastKeystrokeTimeSeconds > 0.0) {
            const double delta = currentTimestampSeconds - lastKeystrokeTimeSeconds;
            if (delta > kIdleTimeoutSeconds) {
                // Long pause / phrase boundary -> reset to standard baseline
                estimated = baseVelocity;
            } else if (delta <= kFastIntervalSeconds) {
                // Rapid passage or simultaneous strike (runs, trills, fast arpeggios, chords)
                estimated = kMaxDynamicVelocity;
            } else if (delta >= kSlowIntervalSeconds) {
                // Deliberate, lyrical slow press
                estimated = kMinDynamicVelocity;
            } else {
                // Smooth interpolation between fast and slow thresholds
                const double t = (delta - kFastIntervalSeconds) / (kSlowIntervalSeconds - kFastIntervalSeconds);
                // Non-linear power curve for natural musical decay
                const auto factor = static_cast<float>(1.0 - std::pow(t, 0.85));
                estimated = kMinDynamicVelocity + (kMaxDynamicVelocity - kMinDynamicVelocity) * factor;
            }
        } else {
            // First keystroke after reset
            estimated = baseVelocity;
        }

        lastKeystrokeTimeSeconds = currentTimestampSeconds;
        lastEstimatedVelocity = estimated;
        return estimated;
    }

    [[nodiscard]] float getLastEstimatedVelocity() const noexcept {
        return lastEstimatedVelocity;
    }

private:
    bool enabled = false;
    float baseVelocity = kDefaultBaseVelocity;
    double lastKeystrokeTimeSeconds = 0.0;
    float lastEstimatedVelocity = kDefaultBaseVelocity;
};

/// Deterministic Pseudo-Random Velocity Humanizer
class VelocityHumanizer {
public:
    static constexpr float kDefaultHumanizeAmount = 0.035f; // ~ +/- 4.5 MIDI velocity

    VelocityHumanizer() noexcept = default;

    void setAmount(float amount) noexcept {
        humanizeAmount = std::clamp(amount, 0.0f, 0.15f);
    }

    [[nodiscard]] float getAmount() const noexcept {
        return humanizeAmount;
    }

    void setEnabled(bool isEnabled) noexcept {
        enabled = isEnabled;
    }

    [[nodiscard]] bool isEnabled() const noexcept {
        return enabled;
    }

    /// Apply deterministic pseudo-random jitter to the input velocity.
    /// Output is strictly clamped within [1/127, 1.0].
    [[nodiscard]] float applyHumanize(float velocity, int midiNote, std::uint32_t counter) const noexcept {
        if (!enabled || humanizeAmount <= 0.0001f || velocity <= 0.0f || velocity >= 1.0f) {
            return velocity;
        }

        // Fast deterministic hash (FNV/Murmur hybrid)
        std::uint32_t h = static_cast<std::uint32_t>(midiNote) * 2654435761u + counter * 2246822519u;
        h ^= (h >> 16);
        h *= 0x85ebca6bu;
        h ^= (h >> 13);

        // Map bottom 16 bits to normalized [-1.0f, +1.0f]
        const auto rawVal = static_cast<int>(h & 0xFFFFu) - 32768;
        const float norm = static_cast<float>(rawVal) / 32768.0f;
        const float jitter = norm * humanizeAmount;

        constexpr float kMinMidi = 1.0f / 127.0f;
        return std::clamp(velocity + jitter, kMinMidi, 1.0f);
    }

private:
    bool enabled = false;
    float humanizeAmount = kDefaultHumanizeAmount;
};

} // namespace devpiano::input
