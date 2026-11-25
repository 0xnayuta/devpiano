#pragma once

#include "HammerVoicing.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace devpiano::audio {

enum class PianoStyle : std::uint8_t { custom, standard, bright, warm, intimate, vintage, nySteinwayDClassical };

struct PianoStyleParameters {
    float brightness = 0.50f;
    float hammerHardness = 0.50f;
    HammerVoicing hammerVoicing = kCalibratedHammerVoicing;
    float resonance = 0.50f;
    bool stretchTuningEnabled = true;
    float duplexResonance = 0.15f;
    std::uint8_t lidPosition = 0;
    float feltAgeingAmount = 0.0f;
    std::uint8_t reverbSpace = 1;
    float reverbWet = 0.0f;
};

/// NY Steinway D Classical 整音锚点：pp 更暗、ff 更亮。
/// 相对当前中性力度基线的增量，仅在用户选择该风格时生效；默认 Standard 保持全零。
inline constexpr HammerVoicing kNySteinwayDClassicalVoicing { { {
    { 41.0f, -0.15f, 0.0f, 1.20f },
    { 70.0f, 0.0f, 0.0f, 0.0f },
    { 98.0f, 0.18f, 0.5f, -1.20f },
} } };

inline constexpr std::array<PianoStyleParameters, 6> pianoStyles { PianoStyleParameters {},
                                                                   { .brightness = 0.75f,
                                                                     .hammerHardness = 0.70f,
                                                                     .resonance = 0.55f,
                                                                     .stretchTuningEnabled = true,
                                                                     .duplexResonance = 0.25f,
                                                                     .lidPosition = 0,
                                                                     .feltAgeingAmount = 0.0f },
                                                                   { .brightness = 0.35f,
                                                                     .hammerHardness = 0.35f,
                                                                     .resonance = 0.60f,
                                                                     .stretchTuningEnabled = true,
                                                                     .duplexResonance = 0.10f,
                                                                     .lidPosition = 1,
                                                                     .feltAgeingAmount = 0.05f },
                                                                   { .brightness = 0.40f,
                                                                     .hammerHardness = 0.30f,
                                                                     .resonance = 0.35f,
                                                                     .stretchTuningEnabled = true,
                                                                     .duplexResonance = 0.05f,
                                                                     .lidPosition = 2,
                                                                     .feltAgeingAmount = 0.15f },
                                                                   { .brightness = 0.45f,
                                                                     .hammerHardness = 0.55f,
                                                                     .resonance = 0.45f,
                                                                     .stretchTuningEnabled = false,
                                                                     .duplexResonance = 0.0f,
                                                                     .lidPosition = 1,
                                                                     .feltAgeingAmount = 0.40f },
                                                                   { .brightness = 0.45f,
                                                                     .hammerHardness = 0.50f,
                                                                     .hammerVoicing = kNySteinwayDClassicalVoicing,
                                                                     .resonance = 0.55f,
                                                                     .stretchTuningEnabled = true,
                                                                     .duplexResonance = 0.12f,
                                                                     .lidPosition = 0,
                                                                     .feltAgeingAmount = 0.0f,
                                                                     .reverbSpace = 2,
                                                                     .reverbWet = 0.09f } };

[[nodiscard]] constexpr const PianoStyleParameters* getPianoStyleParameters(PianoStyle style) noexcept {
    const auto index = static_cast<std::size_t>(style);
    return index >= 1 && index <= pianoStyles.size() ? &pianoStyles[index - 1] : nullptr;
}

[[nodiscard]] constexpr PianoStyle identifyPianoStyle(const PianoStyleParameters& params) noexcept {
    const auto close = [](float a, float b) noexcept { return ((a > b) ? a - b : b - a) <= 1e-4f; };
    for (std::size_t i = 0; i < pianoStyles.size(); ++i) {
        const auto& candidate = pianoStyles[i];
        if (close(params.brightness, candidate.brightness) && close(params.hammerHardness, candidate.hammerHardness)
            && isSameHammerVoicing(params.hammerVoicing, candidate.hammerVoicing)
            && close(params.resonance, candidate.resonance)
            && params.stretchTuningEnabled == candidate.stretchTuningEnabled
            && close(params.duplexResonance, candidate.duplexResonance) && params.lidPosition == candidate.lidPosition
            && close(params.feltAgeingAmount, candidate.feltAgeingAmount) && params.reverbSpace == candidate.reverbSpace
            && close(params.reverbWet, candidate.reverbWet)) {
            return static_cast<PianoStyle>(i + 1);
        }
    }
    return PianoStyle::custom;
}

} // namespace devpiano::audio
