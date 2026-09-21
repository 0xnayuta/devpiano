#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string_view>

namespace devpiano::audio {

/**
 * Sound perspective options for spatial acoustic rendering.
 */
enum class SoundPerspective : std::uint8_t {
    player = 0, ///< Subjective near-field perspective (seated at keyboard: low strings left, high right, wide stereo,
                ///< direct transients).
    audience = 1 ///< Objective far-field perspective (seated in audience: flipped stereo matching listener, convergent
                 ///< width, air absorption).
};

/**
 * PerspectiveProcessor
 *
 * Implements real-time dual-perspective stereo field transformation and acoustic imaging conversion (Phase 31-A).
 *
 * Features:
 * 1. Mid/Side (M/S) stereo imaging matrix with seamless continuous soundstage flipping:
 *    - Player: standard physical keyboard panning (left=bass, right=treble, width = 100%).
 *    - Audience: perspective-flipped acoustic image (left=treble, right=bass to match audience facing the open piano
 * lid), with natural spatial convergence (width = 65%).
 * 2. High-frequency air absorption low-pass filter (simulating acoustic dissipation over distance in a hall).
 * 3. Smooth lock-free parameter interpolation (~30ms time constant) ensuring click-free, pop-free,
 *    and phase-safe transitions during live performance and playback.
 * 4. Strictly zero-allocation and real-time safe.
 */
class PerspectiveProcessor {
public:
    static constexpr float kPlayerWidth = 1.0f;
    static constexpr float kAudienceWidth = 0.65f;
    static constexpr float kPlayerInversion = 0.0f;
    static constexpr float kAudienceInversion = 1.0f;
    static constexpr float kAudienceAirDamping = 0.60f;
    static constexpr float kSmoothTimeSeconds = 0.030f; // 30 ms crossfade

    PerspectiveProcessor() noexcept {
        updateTargets();
        snapToTarget();
    }

    void prepare(double sampleRate) noexcept {
        if (sampleRate > 0.0) {
            sr = sampleRate;
            smoothCoeff = std::exp(-1.0f / (kSmoothTimeSeconds * static_cast<float>(sampleRate)));

            // Air absorption cutoff: ~8.5 kHz in audience mode (clamped to safe Nyquist)
            constexpr float cutoffHz = 8500.0f;
            const auto safeCutoff = std::min(cutoffHz, static_cast<float>(sampleRate * 0.45));
            const auto normCutoff = static_cast<float>(2.0 * std::numbers::pi * safeCutoff / sampleRate);
            airFilterAlpha = std::clamp(normCutoff / (1.0f + normCutoff), 0.01f, 0.99f);
        }
        reset();
    }

    void reset() noexcept {
        filterStateLeft = 0.0f;
        filterStateRight = 0.0f;
    }

    /**
     * Immediately snaps all internal parameters to their target values, bypassing smoothing.
     * Useful during audio thread initialization or preset loading.
     */
    void snapToTarget() noexcept {
        currentInversion = targetInversion;
        currentWidth = targetWidth;
        currentAir = targetAir;
    }

    void setPerspective(SoundPerspective newPerspective, bool immediateSnap = false) noexcept {
        perspective = newPerspective;
        updateTargets();
        if (immediateSnap) {
            snapToTarget();
        }
    }

    [[nodiscard]] SoundPerspective getPerspective() const noexcept {
        return perspective;
    }

    /**
     * Processes a single stereo sample pair in-place.
     * Guaranteed real-time safe, lock-free, and allocation-free.
     */
    void processStereo(float& left, float& right) noexcept {
        advanceSmoothers();

        // 1. Mid / Side Decomposition
        const auto mid = 0.5f * (left + right);
        const auto side = 0.5f * (left - right);

        // 2. Continuous Stereophonic Inversion & Width Scaling
        // When currentInversion == 0.0 (Player): invertedSide = side
        // When currentInversion == 1.0 (Audience): invertedSide = -side (swaps Left and Right)
        const auto inversionFactor = 1.0f - 2.0f * currentInversion;
        const auto modulatedSide = currentWidth * inversionFactor * side;

        auto pannedLeft = mid + modulatedSide;
        auto pannedRight = mid - modulatedSide;

        // 3. Air Absorption High-Frequency Dissipation (Audience far-field distance simulation)
        if (currentAir > 1e-4f) {
            filterStateLeft += airFilterAlpha * (pannedLeft - filterStateLeft);
            filterStateRight += airFilterAlpha * (pannedRight - filterStateRight);

            // Flush denormals
            if (std::abs(filterStateLeft) < 1e-15f) {
                filterStateLeft = 0.0f;
            }
            if (std::abs(filterStateRight) < 1e-15f) {
                filterStateRight = 0.0f;
            }

            pannedLeft = (1.0f - currentAir) * pannedLeft + currentAir * filterStateLeft;
            pannedRight = (1.0f - currentAir) * pannedRight + currentAir * filterStateRight;
        }

        left = pannedLeft;
        right = pannedRight;
    }

    /**
     * Serializes SoundPerspective to a stable string identifier.
     */
    [[nodiscard]] static constexpr std::string_view toIdentifier(SoundPerspective p) noexcept {
        switch (p) {
        case SoundPerspective::player:
            return "player";
        case SoundPerspective::audience:
            return "audience";
        }
        return "player";
    }

    /**
     * Parses a string identifier into SoundPerspective with safe default fallback.
     */
    [[nodiscard]] static SoundPerspective fromIdentifier(std::string_view id) noexcept {
        if (id == "audience") {
            return SoundPerspective::audience;
        }
        return SoundPerspective::player;
    }

private:
    void updateTargets() noexcept {
        if (perspective == SoundPerspective::player) {
            targetInversion = kPlayerInversion;
            targetWidth = kPlayerWidth;
            targetAir = 0.0f;
        } else {
            targetInversion = kAudienceInversion;
            targetWidth = kAudienceWidth;
            targetAir = kAudienceAirDamping;
        }
    }

    void advanceSmoothers() noexcept {
        if (std::abs(currentInversion - targetInversion) > 1e-5f) {
            currentInversion = targetInversion + (currentInversion - targetInversion) * smoothCoeff;
        } else {
            currentInversion = targetInversion;
        }

        if (std::abs(currentWidth - targetWidth) > 1e-5f) {
            currentWidth = targetWidth + (currentWidth - targetWidth) * smoothCoeff;
        } else {
            currentWidth = targetWidth;
        }

        if (std::abs(currentAir - targetAir) > 1e-5f) {
            currentAir = targetAir + (currentAir - targetAir) * smoothCoeff;
        } else {
            currentAir = targetAir;
        }
    }

    SoundPerspective perspective = SoundPerspective::player;

    double sr = 44100.0;
    float smoothCoeff = 0.999f;
    float airFilterAlpha = 0.5f;

    float targetInversion = 0.0f;
    float targetWidth = 1.0f;
    float targetAir = 0.0f;

    float currentInversion = 0.0f;
    float currentWidth = 1.0f;
    float currentAir = 0.0f;

    float filterStateLeft = 0.0f;
    float filterStateRight = 0.0f;
};

} // namespace devpiano::audio
