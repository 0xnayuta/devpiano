#pragma once

#include "HammerVoicing.h"
#include "Piano88KeyTable.h"
#include "PianoDspMath.h"
#include "PianoTuning.h"
#include "TemperamentEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <juce_core/juce_core.h>

// Pure per-note excitation setup for PianoSynthVoice. The voice owns the note
// identity, trigger counter and state installation; this function only derives
// the values needed by the existing string/partial admission and render paths.
// Expressions, types and evaluation order are intentionally copied from the
// original startNote calculation.
namespace devpiano::audio {

struct PianoNoteSetup {
    double firstPartialFrequency = 0.0;
    double effectiveInharmonicityB = 0.0;
    double baseFrequency = 0.0;
    float keyPos = 0.0f;
    float clampedVelocity = 0.0f;
    float velocityLevel = 0.0f;
    float unaCordaMu = 0.0f;
    float hammerHardness = 0.0f;
    float contactHardness = 0.0f;
    float effectiveBrightness = 0.0f;
    float hammerVoicingRolloffDelta = 0.0f;
    float effectiveStrikePos = 0.0f;
    float effectiveTc = 0.0f;
    float jitterPhase = 0.0f;
    double baseDecaySeconds = 0.0;
    double k1 = 0.0;
    double alpha1 = 0.0;
};

[[nodiscard]] inline PianoNoteSetup
computePianoNoteSetup(int midiNoteNumber, float velocity, const PianoNoteParams& params, float brightness,
                      float hammerHardnessSetting, float resonance, float feltAgeingAmount,
                      const HammerVoicing& hammerVoicing, bool softPedalDown, float softPedalAmount,
                      Temperament temperament, double referencePitchA4, bool stretchTuningEnabled,
                      std::uint32_t triggerSequence) noexcept {
    // Felt ageing and tuning dispersion (Phase 32-C).
    const auto pitchJitterCents = (feltAgeingAmount > 0.0f)
        ? deterministicNoteJitter(midiNoteNumber, 0x13579bdfu) * 1.2f * feltAgeingAmount
        : 0.0f;
    const auto pitchJitterRatio = std::pow(2.0, static_cast<double>(pitchJitterCents) / 1200.0);

    const auto bJitterRatio = (feltAgeingAmount > 0.0f)
        ? (1.0 + static_cast<double>(deterministicNoteJitter(midiNoteNumber, 0x2468ace0u) * 0.045f * feltAgeingAmount))
        : 1.0;
    const auto effectiveInharmonicityB = params.inharmonicityB * bJitterRatio;
    const auto firstPartialFrequency
        = pianoFirstPartialFrequency(midiNoteNumber, temperament, referencePitchA4, stretchTuningEnabled)
        * pitchJitterRatio;
    const auto baseFrequency = firstPartialFrequency / std::sqrt(1.0 + effectiveInharmonicityB);

    const auto clampedVelocity = juce::jlimit(0.0f, 1.0f, velocity);
    // Three-anchor hammer voicing: default anchors return {0.0f, 1.0f}.
    const auto voicingSample = sampleHammerVoicing(hammerVoicing, clampedVelocity * 127.0f);
    const auto velocityLevel = juce::jlimit(0.0f, 1.0f, std::pow(clampedVelocity, 1.20f) * voicingSample.loudnessGain);
    const auto decayScale = 1.0f + (resonance - 0.5f) * 0.6f;
    const auto baseDecaySeconds = static_cast<double>(params.decaySeconds * decayScale);

    const auto keyPos = std::clamp((static_cast<float>(midiNoteNumber) - 21.0f) / 87.0f, 0.0f, 1.0f);

    // Una corda shift (Phase 29-B).
    const auto unaCordaMu = softPedalDown ? juce::jmax(0.60f, softPedalAmount) : 0.0f;

    // Dynamic felt hardness (Phase 23-A / Phase 32-C).
    const auto hardnessOffset = (feltAgeingAmount > 0.0f)
        ? deterministicNoteJitter(midiNoteNumber, 0x9e3779b9u) * 0.06f * feltAgeingAmount
        : 0.0f;
    const auto brightnessOffset = (feltAgeingAmount > 0.0f)
        ? deterministicNoteJitter(midiNoteNumber, 0x4b7e1516u) * 0.04f * feltAgeingAmount
        : 0.0f;

    // Single hardness state (39-3): macro hardness, voicing and ageing first;
    // CC67 then derives contact hardness without counting the same effect twice.
    const auto hammerHardness
        = std::clamp(0.15f + 0.85f * std::pow(clampedVelocity, 1.5f) * (0.5f + 0.5f * hammerHardnessSetting)
                         + voicingSample.hardnessDelta + hardnessOffset,
                     0.05f, 1.0f);
    const auto contactHardness = std::clamp(hammerHardness * (1.0f - 0.25f * unaCordaMu), 0.05f, 1.0f);
    const auto effectiveBrightness = std::clamp(brightness + brightnessOffset, 0.0f, 1.0f);
    const auto tc = params.tcBase * (2.5f - 1.9f * contactHardness) * (1.0f + 0.20f * unaCordaMu);

    // Strike micro-perturbation (Phase 20-B). The caller owns triggerCounter and
    // passes the already-advanced sequence value.
    auto rngState = static_cast<std::uint32_t>(midiNoteNumber) * 1009u + triggerSequence * 1013u
        + static_cast<std::uint32_t>(clampedVelocity * 1000.0f);
    auto hashJitter = [](std::uint32_t& s) noexcept -> float {
        s = s * 1664525u + 1013904223u;
        return static_cast<float>(s >> 16) / 65535.0f * 2.0f - 1.0f;
    };
    const auto jitterStrike = 1.0f + 0.006f * hashJitter(rngState);
    const auto jitterTc = 1.0f + 0.008f * hashJitter(rngState);
    const auto jitterPhase = 0.012f * hashJitter(rngState);

    const auto effectiveStrikePos = params.strikePosRatio * jitterStrike;
    const auto effectiveTc = tc * jitterTc;

    const auto piOverL = juce::MathConstants<double>::pi / static_cast<double>(params.stringLength);
    const auto k1 = piOverL * piOverL;
    const auto alpha1 = static_cast<double>(params.b1) + static_cast<double>(params.b2) * k1;

    PianoNoteSetup setup;
    setup.firstPartialFrequency = firstPartialFrequency;
    setup.effectiveInharmonicityB = effectiveInharmonicityB;
    setup.baseFrequency = baseFrequency;
    setup.keyPos = keyPos;
    setup.clampedVelocity = clampedVelocity;
    setup.velocityLevel = velocityLevel;
    setup.unaCordaMu = unaCordaMu;
    setup.hammerHardness = hammerHardness;
    setup.contactHardness = contactHardness;
    setup.effectiveBrightness = effectiveBrightness;
    setup.hammerVoicingRolloffDelta = voicingSample.rolloffDelta;
    setup.effectiveStrikePos = effectiveStrikePos;
    setup.effectiveTc = effectiveTc;
    setup.jitterPhase = jitterPhase;
    setup.baseDecaySeconds = baseDecaySeconds;
    setup.k1 = k1;
    setup.alpha1 = alpha1;
    return setup;
}

} // namespace devpiano::audio
