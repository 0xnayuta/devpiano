#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace devpiano::audio {

enum class PianoStyle : std::uint8_t { custom, standard, bright, warm, intimate, vintage };

struct PianoStyleParameters {
    float brightness = 0.50f;
    float hammerHardness = 0.50f;
    float resonance = 0.50f;
    bool stretchTuningEnabled = true;
    float duplexResonance = 0.15f;
    std::uint8_t lidPosition = 0;
    float feltAgeingAmount = 0.0f;
};

inline constexpr std::array<PianoStyleParameters, 5> pianoStyles { PianoStyleParameters {},
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
                                                                     .feltAgeingAmount = 0.40f } };

[[nodiscard]] constexpr const PianoStyleParameters* getPianoStyleParameters(PianoStyle style) noexcept {
    const auto index = static_cast<std::size_t>(style);
    return index >= 1 && index <= pianoStyles.size() ? &pianoStyles[index - 1] : nullptr;
}

[[nodiscard]] constexpr PianoStyle identifyPianoStyle(const PianoStyleParameters& params) noexcept {
    const auto close = [](float a, float b) noexcept { return ((a > b) ? a - b : b - a) <= 1e-4f; };
    for (std::size_t i = 0; i < pianoStyles.size(); ++i) {
        const auto& candidate = pianoStyles[i];
        if (close(params.brightness, candidate.brightness) && close(params.hammerHardness, candidate.hammerHardness)
            && close(params.resonance, candidate.resonance)
            && params.stretchTuningEnabled == candidate.stretchTuningEnabled
            && close(params.duplexResonance, candidate.duplexResonance) && params.lidPosition == candidate.lidPosition
            && close(params.feltAgeingAmount, candidate.feltAgeingAmount)) {
            return static_cast<PianoStyle>(i + 1);
        }
    }
    return PianoStyle::custom;
}

} // namespace devpiano::audio
