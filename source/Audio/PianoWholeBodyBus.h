#pragma once

#include "PerspectiveProcessor.h"
#include "PianoDspMath.h"
#include "PianoLidAcoustics.h"
#include "PianoVoiceResonance.h"
#include "TemperamentEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <juce_audio_basics/juce_audio_basics.h>

namespace devpiano::audio {

// Bounded per-subblock input scratch filled by PianoSynthVoice and the pedal
// mechanical path. The three channels are dry stereo and one shared drive
// (wet body + sympathetic) mono signal. No allocation happens in render.
struct PianoBusInput {
    float* dryLeft = nullptr;
    float* dryRight = nullptr;
    float* drive = nullptr;
    int numSamples = 0;
    bool hasSignal = false;

    void add(int index, float left, float right, float driveValue) noexcept {
        if (index < 0 || index >= numSamples) {
            return;
        }
        dryLeft[index] += left;
        dryRight[index] += right;
        drive[index] += driveValue;
        hasSignal = hasSignal || left != 0.0f || right != 0.0f || driveValue != 0.0f;
    }
};

// One whole-piano acoustic tail owner per Piano endpoint. It owns the soundboard,
// sympathetic strings, lid and perspective once, independent of voice lifetime.
struct PianoWholeBodyBus {
    static constexpr int numResonators = 16;

    struct ResonatorSpec {
        float frequency = 48.0f;
        float q = 6.0f;
        float weightLeft = 0.12f;
        float weightRight = 0.04f;
    };

    [[nodiscard]] static ResonatorSpec resonatorSpec(int index) noexcept {
        constexpr ResonatorSpec specs[numResonators] = {
            { 48.0f, 6.0f, 0.12f, 0.04f },   { 68.0f, 6.0f, 0.12f, 0.04f },   { 95.0f, 5.5f, 0.11f, 0.05f },
            { 135.0f, 5.5f, 0.10f, 0.05f },  { 185.0f, 5.0f, 0.09f, 0.06f },  { 250.0f, 4.8f, 0.08f, 0.07f },
            { 340.0f, 4.5f, 0.08f, 0.08f },  { 460.0f, 4.2f, 0.07f, 0.08f },  { 620.0f, 3.8f, 0.06f, 0.09f },
            { 820.0f, 3.5f, 0.05f, 0.09f },  { 1080.0f, 3.2f, 0.04f, 0.09f }, { 1380.0f, 3.0f, 0.03f, 0.08f },
            { 1680.0f, 2.8f, 0.02f, 0.07f }, { 1850.0f, 2.5f, 0.01f, 0.05f }, { 2050.0f, 2.4f, 0.01f, 0.03f },
            { 2250.0f, 2.2f, 0.01f, 0.03f },
        };
        const auto clamped = std::clamp(index, 0, numResonators - 1);
        return specs[clamped];
    }

    struct BodyResonator {
        float frequency = 110.0f;
        float q = 6.0f;
        float weightLeft = 0.40f;
        float weightRight = 0.40f;

        float c1 = 0.0f;
        float c2 = 0.0f;
        float g = 0.0f;
        float s1 = 0.0f;
        float s2 = 0.0f;
        double lastSampleRate = 0.0;

        void updateCoefficients(double sampleRate) noexcept {
            if (sampleRate <= 0.0 || std::abs(sampleRate - lastSampleRate) < 1e-6) {
                return;
            }
            lastSampleRate = sampleRate;
            const auto theta = juce::MathConstants<double>::twoPi * static_cast<double>(frequency) / sampleRate;
            const auto bandwidth = static_cast<double>(frequency / q);
            const auto r = std::exp(-juce::MathConstants<double>::pi * bandwidth / sampleRate);
            c1 = static_cast<float>(2.0 * r * devpiano::audio::dsp::boundedCos(theta));
            c2 = static_cast<float>(-r * r);
            g = static_cast<float>((1.0 - r * r) * 0.5);
        }

        void reset() noexcept {
            s1 = 0.0f;
            s2 = 0.0f;
        }

        [[nodiscard]] float process(float in) noexcept {
            const auto w = in + c1 * s1 + c2 * s2;
            const auto y = g * (w - s2);
            s2 = s1;
            s1 = w;
            return y;
        }
    };

    struct SpruceSoundboardFilter {
        float lpLeft = 0.0f;
        float lpRight = 0.0f;
        float lpCoeff = 0.0f;

        void updateCoefficients(double sampleRate) noexcept {
            if (sampleRate <= 0.0) {
                return;
            }
            constexpr float fc = 8000.0f;
            const auto safeFc = std::min(fc, static_cast<float>(sampleRate * 0.480));
            lpCoeff = std::exp(-juce::MathConstants<float>::twoPi * safeFc / static_cast<float>(sampleRate));
        }

        void reset() noexcept {
            lpLeft = 0.0f;
            lpRight = 0.0f;
        }

        void processStereo(float& left, float& right) noexcept {
            if (lpCoeff > 0.0f) {
                lpLeft = (1.0f - lpCoeff) * left + lpCoeff * lpLeft;
                lpRight = (1.0f - lpCoeff) * right + lpCoeff * lpRight;
                left = lpLeft;
                right = lpRight;
            }
        }
    };

    std::array<BodyResonator, numResonators> bodyResonators = [] {
        std::array<BodyResonator, numResonators> array {};
        for (std::size_t i = 0; i < numResonators; ++i) {
            const auto spec = resonatorSpec(static_cast<int>(i));
            array[i].frequency = spec.frequency;
            array[i].q = spec.q;
            array[i].weightLeft = spec.weightLeft;
            array[i].weightRight = spec.weightRight;
        }
        return array;
    }();
    SpruceSoundboardFilter spruceSoundboardFilter;
    SympatheticResonancePool sympatheticPool;
    LidAcoustics lid;
    PerspectiveProcessor perspective;

    double sampleRate = 48000.0;
    Temperament temperament = Temperament::equal;
    double referencePitchA4 = TemperamentEngine::kDefaultReferencePitch;
    bool stretchTuningEnabled = true;
    LidPosition lidPosition = LidPosition::fullOpen;
    SoundPerspective soundPerspective = SoundPerspective::player;
    int tailSamples = 0;
    int tailSamplesCapacity = 1;

    PianoWholeBodyBus() noexcept {
        prepare(48000.0);
    }

    void prepare(double newSampleRate) noexcept {
        if (newSampleRate > 0.0) {
            sampleRate = newSampleRate;
        }
        tailSamplesCapacity = std::max(1, static_cast<int>(2.0 * sampleRate));
        for (auto& resonator : bodyResonators) {
            resonator.updateCoefficients(sampleRate);
        }
        spruceSoundboardFilter.updateCoefficients(sampleRate);
        sympatheticPool.updateCoefficients(sampleRate, temperament, referencePitchA4, stretchTuningEnabled);
        lid.setPosition(lidPosition, sampleRate);
        perspective.prepare(sampleRate);
        reset();
    }

    void applyTuning(Temperament newTemperament, double newReferencePitchA4, bool newStretchTuningEnabled) noexcept {
        temperament = newTemperament;
        referencePitchA4 = newReferencePitchA4;
        stretchTuningEnabled = newStretchTuningEnabled;
        if (sampleRate > 0.0) {
            sympatheticPool.updateCoefficients(sampleRate, temperament, referencePitchA4, stretchTuningEnabled);
        }
    }

    void setLidPosition(LidPosition position) noexcept {
        lidPosition = position;
        if (sampleRate > 0.0) {
            lid.setPosition(position, sampleRate);
        }
    }

    void setSoundPerspective(SoundPerspective newPerspective) noexcept {
        soundPerspective = newPerspective;
        perspective.setPerspective(newPerspective, !isActive());
    }

    void setSustainPedal(bool isDown) noexcept {
        sympatheticPool.setPedalDown(isDown);
    }

    void triggerPedalShock(float amount) noexcept {
        sympatheticPool.triggerShock(amount, sampleRate);
        tailSamples = tailSamplesCapacity;
    }

    void noteOnKey(int midiNoteNumber) noexcept {
        sympatheticPool.noteOnKey(midiNoteNumber);
    }

    void noteOffKey(int midiNoteNumber) noexcept {
        sympatheticPool.noteOffKey(midiNoteNumber);
    }

    [[nodiscard]] bool isActive() const noexcept {
        return tailSamples > 0 || sympatheticPool.isShockActive();
    }

    void reset() noexcept {
        for (auto& resonator : bodyResonators) {
            resonator.reset();
        }
        spruceSoundboardFilter.reset();
        sympatheticPool.reset();
        sympatheticPool.setPedalDown(false);
        lid.reset();
        perspective.reset();
        tailSamples = 0;
    }

    template <typename FloatType>
    void render(const PianoBusInput& input, juce::AudioBuffer<FloatType>& output, int startSample,
                int numSamples) noexcept {
        if (numSamples <= 0 || input.numSamples < numSamples) {
            return;
        }
        if (input.hasSignal) {
            tailSamples = tailSamplesCapacity;
        } else if (tailSamples <= 0) {
            return;
        }

        const auto hasStereo = output.getNumChannels() >= 2;
        for (auto sample = 0; sample < numSamples; ++sample) {
            const auto drive = input.drive[sample];
            auto bodyLeft = 0.0f;
            auto bodyRight = 0.0f;
            for (auto& resonator : bodyResonators) {
                const auto value = resonator.process(drive);
                bodyLeft += resonator.weightLeft * value;
                bodyRight += resonator.weightRight * value;
            }
            spruceSoundboardFilter.processStereo(bodyLeft, bodyRight);

            const auto sympathetic = sympatheticPool.process(drive);
            auto left = input.dryLeft[sample] + bodyLeft + 0.40f * sympathetic;
            auto right = input.dryRight[sample] + bodyRight + 0.60f * sympathetic;
            lid.processStereo(left, right);
            perspective.processStereo(left, right);

            if (hasStereo) {
                output.addSample(0, startSample + sample, left);
                output.addSample(1, startSample + sample, right);
            } else {
                output.addSample(0, startSample + sample, 0.5f * (left + right));
            }

            if (!input.hasSignal && tailSamples > 0) {
                --tailSamples;
            }
        }
    }
};

} // namespace devpiano::audio