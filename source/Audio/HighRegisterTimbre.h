#pragma once

#include <algorithm>
#include <cmath>

namespace devpiano::audio::dsp {

/// High-register timbre experiment: a note-dependent, one-pole low-pass blend.
/// It leaves the register loudness curve untouched and only tilts the upper
/// spectrum of notes from C7 upward; pedal mechanical noise is never filtered.
class HighRegisterTimbre {
public:
    static constexpr float cutoffHz = 3000.0f;

    void prepare(double sampleRate) noexcept {
        if (sampleRate <= 0.0) {
            lowPassCoefficient = 0.0f;
        } else {
            lowPassCoefficient = std::exp(-2.0f * 3.14159265358979323846f * cutoffHz / static_cast<float>(sampleRate));
        }
        reset();
    }

    void reset() noexcept {
        lowLeft = 0.0f;
        lowRight = 0.0f;
    }

    /// Set the spectral tilt for the current note. MIDI 96..108 is the only
    /// affected range; velocity increases the effect so hard attacks retain
    /// less of the upper-harmonic edge.
    void setNote(int midiNoteNumber, float velocity) noexcept {
        if (midiNoteNumber < 96) {
            depth = 0.0f;
            return;
        }
        const auto position = std::clamp(static_cast<float>(midiNoteNumber - 96) / 12.0f, 0.0f, 1.0f);
        const auto velocityScale = 0.75f + 0.25f * std::clamp(velocity, 0.0f, 1.0f);
        depth = (0.55f + 0.30f * position) * velocityScale;
    }

    [[nodiscard]] bool isActive() const noexcept {
        return depth > 0.0f;
    }

    void processStereo(float& left, float& right) noexcept {
        if (depth <= 0.0f || lowPassCoefficient <= 0.0f) {
            return;
        }
        lowLeft = (1.0f - lowPassCoefficient) * left + lowPassCoefficient * lowLeft;
        lowRight = (1.0f - lowPassCoefficient) * right + lowPassCoefficient * lowRight;
        left = (1.0f - depth) * left + depth * lowLeft;
        right = (1.0f - depth) * right + depth * lowRight;
    }

private:
    float lowPassCoefficient = 0.0f;
    float lowLeft = 0.0f;
    float lowRight = 0.0f;
    float depth = 0.0f;
};

} // namespace devpiano::audio::dsp
