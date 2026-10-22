#pragma once

#include "PianoDspMath.h"
#include "PianoTuning.h"
#include "TemperamentEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <juce_core/juce_core.h>

// Per-voice sympathetic resonance pool extracted verbatim from
// PianoSynthVoice.h for R1. PianoSynthVoice continues to own one instance;
// this header intentionally has no dependency on PianoSynthVoice.h.
namespace devpiano::audio {

struct SympatheticResonancePool {
    static constexpr auto numPoolResonators = 12;
    bool pedalDown = false;
    int shockSamplesRemaining = 0;
    std::array<int, numPoolResonators> openNoteCount {};
    float c1[numPoolResonators] {};
    float c2[numPoolResonators] {};
    float g[numPoolResonators] {};
    float s1[numPoolResonators] {};
    float s2[numPoolResonators] {};

    void setPedalDown(bool isDown) noexcept {
        pedalDown = isDown;
    }

    void noteOnKey(int midiNoteNumber) noexcept {
        const auto pc = (midiNoteNumber % 12 + 12) % 12;
        ++openNoteCount[static_cast<std::size_t>(pc)];
    }

    void noteOffKey(int midiNoteNumber) noexcept {
        const auto pc = (midiNoteNumber % 12 + 12) % 12;
        auto& count = openNoteCount[static_cast<std::size_t>(pc)];
        if (count > 0) {
            --count;
        }
    }

    double lastSampleRate = 0.0;
    Temperament lastTemperament = Temperament::equal;
    double lastReferencePitchA4 = 0.0;
    bool lastStretchTuningEnabled = true;

    void updateCoefficients(double sampleRate, Temperament temperament = Temperament::equal,
                            double referencePitchA4 = TemperamentEngine::kDefaultReferencePitch,
                            bool stretchTuningEnabled = true) noexcept {
        if (sampleRate <= 0.0
            || (std::abs(sampleRate - lastSampleRate) < 1e-6 && temperament == lastTemperament
                && juce::exactlyEqual(referencePitchA4, lastReferencePitchA4)
                && stretchTuningEnabled == lastStretchTuningEnabled)) {
            return;
        }
        lastSampleRate = sampleRate;
        lastTemperament = temperament;
        lastReferencePitchA4 = referencePitchA4;
        lastStretchTuningEnabled = stretchTuningEnabled;
        for (int i = 0; i < numPoolResonators; ++i) {
            const auto frequency = std::min(devpiano::audio::pianoFirstPartialFrequency(
                                                36 + i, temperament, referencePitchA4, stretchTuningEnabled),
                                            sampleRate * 0.480);
            const auto theta = juce::MathConstants<double>::twoPi * frequency / sampleRate;
            const auto bandwidth = frequency / 12.0;
            const auto r = std::exp(-juce::MathConstants<double>::pi * bandwidth / sampleRate);
            c1[i] = static_cast<float>(2.0 * r * devpiano::audio::dsp::boundedCos(theta));
            c2[i] = static_cast<float>(-r * r);
            g[i] = static_cast<float>((1.0 - r * r) * 0.5);
        }
    }

    void reset() noexcept {
        shockSamplesRemaining = 0;
        openNoteCount.fill(0);
        for (int i = 0; i < numPoolResonators; ++i) {
            s1[i] = 0.0f;
            s2[i] = 0.0f;
        }
    }
    void triggerShock(float amount, double sampleRate) noexcept {
        if (sampleRate > 0.0 && amount > 1e-4f) {
            shockSamplesRemaining = juce::jmax(1, static_cast<int>(0.35 * sampleRate));
        }
        for (int i = 0; i < numPoolResonators; ++i) {
            s1[i] += amount * ((i % 2 == 0) ? 0.003f : -0.003f);
        }
    }

    [[nodiscard]] bool isShockActive() const noexcept {
        return shockSamplesRemaining > 0;
    }

    // Bounded residual drain estimate for the shock-only tail. The coefficients
    // are the same modal poles used by the note-driven pool; the estimate uses
    // the slowest pole and a -80 dB relative residual. It is not used by the
    // note-driven processing path.
    [[nodiscard]] int shockTailDrainSamples() const noexcept {
        auto maxPole = 0.0f;
        for (const auto coefficient : c2) {
            maxPole = std::max(maxPole, std::sqrt(std::max(0.0f, -coefficient)));
        }
        if (maxPole <= 0.0f || maxPole >= 1.0f) {
            return 0;
        }
        constexpr auto residual = 1.0e-4f;
        const auto samples = std::ceil(std::log(residual) / std::log(maxPole));
        return std::clamp(static_cast<int>(samples), 0, 1 << 20);
    }

    [[nodiscard]] float process(float in) noexcept {
        if (shockSamplesRemaining > 0) {
            --shockSamplesRemaining;
        }
        if (!pedalDown) {
            auto hasOpenNotes = false;
            for (int i = 0; i < numPoolResonators; ++i) {
                if (openNoteCount[static_cast<std::size_t>(i)] > 0) {
                    hasOpenNotes = true;
                    break;
                }
            }
            if (!hasOpenNotes && shockSamplesRemaining <= 0) {
                for (int i = 0; i < numPoolResonators; ++i) {
                    s1[i] = (std::abs(s1[i]) > 1e-6f) ? s1[i] * 0.90f : 0.0f;
                    s2[i] = (std::abs(s2[i]) > 1e-6f) ? s2[i] * 0.90f : 0.0f;
                }
                return 0.0f;
            }
        }
        auto sum = 0.0f;
        for (int i = 0; i < numPoolResonators; ++i) {
            const auto isOpen = pedalDown || (openNoteCount[static_cast<std::size_t>(i)] > 0);
            if (!isOpen) {
                s1[i] = (std::abs(s1[i]) > 1e-6f) ? s1[i] * 0.90f : 0.0f;
                s2[i] = (std::abs(s2[i]) > 1e-6f) ? s2[i] * 0.90f : 0.0f;
                continue;
            }
            const auto drive = in * (pedalDown ? 0.08f : 0.04f);
            const auto w = drive + c1[i] * s1[i] + c2[i] * s2[i];
            const auto y = g[i] * (w - s2[i]);
            s2[i] = s1[i];
            s1[i] = w;
            sum += y;
        }
        return sum;
    }
};

} // namespace devpiano::audio
