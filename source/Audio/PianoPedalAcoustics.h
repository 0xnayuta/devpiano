#pragma once

#include "PianoMechanicalTransients.h"
#include "PianoWholeBodyBus.h"

#include <algorithm>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>

namespace devpiano::audio {

// Slim pedal action owner. The whole-piano bus owns the shock pool, lid and
// perspective; this struct only renders the mechanical whoosh/thump.
struct PianoPedalAcoustics {
    PedalTransient mechanical;
    float pedalNoiseLevel = 0.6f;
    double sampleRate = 48000.0;

    PianoPedalAcoustics() noexcept {
        prepare(48000.0);
    }

    void prepare(double newSampleRate) noexcept {
        if (newSampleRate > 0.0) {
            sampleRate = newSampleRate;
        }
        mechanical.updateSampleRate(sampleRate);
        reset();
    }

    void setPedalNoiseLevel(float level) noexcept {
        pedalNoiseLevel = juce::jlimit(0.0f, 1.0f, level);
    }

    [[nodiscard]] float getPedalNoiseLevel() const noexcept {
        return pedalNoiseLevel;
    }

    void onSustainEdge(bool isDown, float velocity) noexcept {
        if (sampleRate <= 0.0) {
            return;
        }
        mechanical.trigger(sampleRate, isDown, velocity, pedalNoiseLevel);
    }

    [[nodiscard]] bool isActive() const noexcept {
        return mechanical.isActive();
    }

    void reset() noexcept {
        mechanical.reset();
    }

    void renderToBus(PianoBusInput& input, int numSamples) noexcept {
        if (numSamples <= 0 || !mechanical.isActive()) {
            return;
        }
        const auto count = std::min(numSamples, input.numSamples);
        for (auto sample = 0; sample < count; ++sample) {
            const auto mechanicalSample = mechanical.getNextSample() * 0.7071f;
            input.add(sample, mechanicalSample, mechanicalSample, 0.0f);
        }
    }
};

} // namespace devpiano::audio