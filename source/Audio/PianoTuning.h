#pragma once

#include "Piano88KeyTable.h"
#include "TemperamentEngine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace devpiano::audio {

[[nodiscard]] inline double normalizedPianoPartialRatio(double stiffness, int partialNumber) noexcept {
    const auto b = std::max(0.0, stiffness);
    const auto m = static_cast<double>(std::max(1, partialNumber));
    return m * std::sqrt((1.0 + b * m * m) / (1.0 + b));
}

namespace pianoTuningDetail {

[[nodiscard]] inline double octaveStretchCents(int lowerNote) noexcept {
    const auto lowerPartial = lowerNote < 69 ? 4 : 2;
    const auto upperPartial = lowerPartial / 2;
    const auto lowerRatio = normalizedPianoPartialRatio(getNoteParams(lowerNote).inharmonicityB, lowerPartial);
    const auto upperRatio = normalizedPianoPartialRatio(getNoteParams(lowerNote + 12).inharmonicityB, upperPartial);
    return 1200.0 * std::log2(lowerRatio / (2.0 * upperRatio));
}

[[nodiscard]] inline std::array<double, 128> makeStretchRatios() noexcept {
    std::array<double, 128> cents {};
    const auto lowerAnchor = -octaveStretchCents(57);
    const auto upperAnchor = octaveStretchCents(69);
    for (int note = 57; note <= 69; ++note) {
        cents[static_cast<std::size_t>(note)] = lowerAnchor * static_cast<double>(69 - note) / 12.0;
    }
    for (int note = 70; note <= 81; ++note) {
        cents[static_cast<std::size_t>(note)] = upperAnchor * static_cast<double>(note - 69) / 12.0;
    }
    for (int note = 56; note >= 21; --note) {
        const auto index = static_cast<std::size_t>(note);
        cents[index] = cents[index + 12] - octaveStretchCents(note);
    }
    for (int note = 82; note <= 108; ++note) {
        const auto index = static_cast<std::size_t>(note);
        cents[index] = cents[index - 12] + octaveStretchCents(note - 12);
    }
    for (int note = 0; note < 21; ++note) {
        cents[static_cast<std::size_t>(note)] = cents[21];
    }
    for (int note = 109; note < 128; ++note) {
        cents[static_cast<std::size_t>(note)] = cents[108];
    }
    for (auto& value : cents) {
        value = std::exp2(value / 1200.0);
    }
    return cents;
}

} // namespace pianoTuningDetail

[[nodiscard]] inline const std::array<double, 128>& getPianoStretchRatios() noexcept {
    static const auto ratios = pianoTuningDetail::makeStretchRatios();
    return ratios;
}

[[nodiscard]] inline double pianoFirstPartialFrequency(int midiNoteNumber, Temperament temperament = Temperament::equal,
                                                       double referencePitchA4
                                                       = TemperamentEngine::kDefaultReferencePitch,
                                                       bool stretchTuningEnabled = true) noexcept {
    const auto note = std::clamp(midiNoteNumber, 0, 127);
    const auto nominal = TemperamentEngine::getFrequency(note, temperament, referencePitchA4);
    return nominal * (stretchTuningEnabled ? getPianoStretchRatios()[static_cast<std::size_t>(note)] : 1.0);
}

[[nodiscard]] inline double pianoPartialFrequency(int midiNoteNumber, int partialIndex,
                                                  Temperament temperament = Temperament::equal,
                                                  double referencePitchA4 = TemperamentEngine::kDefaultReferencePitch,
                                                  bool stretchTuningEnabled = true) noexcept {
    return pianoFirstPartialFrequency(midiNoteNumber, temperament, referencePitchA4, stretchTuningEnabled)
        * normalizedPianoPartialRatio(getNoteParams(midiNoteNumber).inharmonicityB, partialIndex + 1);
}

} // namespace devpiano::audio
