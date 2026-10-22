#pragma once

#include "PerspectiveProcessor.h"
#include "PianoLidAcoustics.h"
#include "PianoMechanicalTransients.h"
#include "PianoVoiceResonance.h"

#include <algorithm>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>

namespace devpiano::audio {

// One per built-in Piano instance. The owner is deliberately limited to the
// mechanical pedal action: the per-voice note-driven sympathetic pools remain
// in PianoSynthVoice and are not made global here.
struct PianoPedalAcoustics {
    PedalTransient mechanical;
    SympatheticResonancePool shockTail;
    LidAcoustics lid;
    PerspectiveProcessor perspective;

    float pedalNoiseLevel = 0.6f;
    double sampleRate = 48000.0;
    Temperament temperament = Temperament::equal;
    double referencePitchA4 = TemperamentEngine::kDefaultReferencePitch;
    bool stretchTuningEnabled = true;
    LidPosition lidPosition = LidPosition::fullOpen;
    SoundPerspective soundPerspective = SoundPerspective::player;

    // Remaining samples for the independent lid/perspective tail and for the
    // shock-only modal decay. Both are explicit state, not zero-output probes.
    int tailDrainSamples = 0;
    int shockDrainSamples = 0;
    bool shockWasActive = false;

    PianoPedalAcoustics() noexcept {
        prepare(48000.0);
    }

    void prepare(double newSampleRate) noexcept {
        if (newSampleRate > 0.0) {
            sampleRate = newSampleRate;
        }
        mechanical.updateSampleRate(sampleRate);
        shockTail.updateCoefficients(sampleRate, temperament, referencePitchA4, stretchTuningEnabled);
        lid.setPosition(lidPosition, sampleRate);
        perspective.prepare(sampleRate);
        reset();
    }

    void setTuning(Temperament newTemperament, double newReferencePitchA4, bool newStretchTuningEnabled) noexcept {
        temperament = newTemperament;
        referencePitchA4 = newReferencePitchA4;
        stretchTuningEnabled = newStretchTuningEnabled;
        if (sampleRate > 0.0) {
            shockTail.updateCoefficients(sampleRate, temperament, referencePitchA4, stretchTuningEnabled);
        }
    }

    void setPedalNoiseLevel(float level) noexcept {
        pedalNoiseLevel = juce::jlimit(0.0f, 1.0f, level);
    }

    void setLidPosition(LidPosition position) noexcept {
        lidPosition = position;
        if (sampleRate > 0.0) {
            lid.setPosition(lidPosition, sampleRate);
        }
    }

    void setSoundPerspective(SoundPerspective newPerspective) noexcept {
        soundPerspective = newPerspective;
        perspective.setPerspective(newPerspective, !isActive());
    }

    // Called once per real aggregate pedal edge by BuiltinSynthesiser. The
    // caller owns the per-channel truth; this owner only receives the edge and
    // its raw-CC-derived velocity.
    void onSustainEdge(bool isDown, float velocity) noexcept {
        if (sampleRate <= 0.0) {
            return;
        }
        mechanical.trigger(sampleRate, isDown, velocity, pedalNoiseLevel);
        if (isDown) {
            shockTail.setPedalDown(true);
            shockTail.triggerShock(velocity * pedalNoiseLevel, sampleRate);
            shockDrainSamples = 0;
            shockWasActive = false;
        } else {
            shockTail.setPedalDown(false);
        }

        if (mechanical.isActive() || shockTail.isShockActive()) {
            const auto lidDrain = static_cast<int>(LidAcoustics::delaySize);
            const auto perspectiveDrain = perspective.tailDrainSamples();
            tailDrainSamples = std::max(lidDrain, perspectiveDrain);
        }
    }

    [[nodiscard]] bool isActive() const noexcept {
        return mechanical.isActive() || shockTail.isShockActive() || tailDrainSamples > 0 || shockDrainSamples > 0;
    }

    void reset() noexcept {
        mechanical.reset();
        shockTail.reset();
        shockTail.setPedalDown(false);
        lid.reset();
        perspective.reset();
        tailDrainSamples = 0;
        shockDrainSamples = 0;
        shockWasActive = false;
    }

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) noexcept {
        if (outputBuffer.getNumChannels() <= 0 || numSamples <= 0 || !isActive()) {
            return;
        }

        for (auto sample = 0; sample < numSamples; ++sample) {
            const auto sampleIndex = startSample + sample;
            const auto mechanicalSample = mechanical.isActive() ? mechanical.getNextSample() : 0.0f;

            auto shockSample = 0.0f;
            if (shockTail.isShockActive()) {
                shockSample = shockTail.process(0.0f);
                shockWasActive = true;
            } else if (shockDrainSamples > 0) {
                shockSample = shockTail.process(0.0f);
                --shockDrainSamples;
            } else if (shockWasActive) {
                shockDrainSamples = shockTail.shockTailDrainSamples();
                shockWasActive = false;
                if (shockDrainSamples > 0) {
                    shockSample = shockTail.process(0.0f);
                    --shockDrainSamples;
                }
            }

            auto outL = mechanicalSample * 0.7071f + shockSample * 0.40f;
            auto outR = mechanicalSample * 0.7071f + shockSample * 0.60f;
            lid.processStereo(outL, outR);
            perspective.processStereo(outL, outR);

            if (outputBuffer.getNumChannels() >= 2) {
                outputBuffer.addSample(0, sampleIndex, outL);
                outputBuffer.addSample(1, sampleIndex, outR);
            } else {
                outputBuffer.addSample(0, sampleIndex, 0.5f * (outL + outR));
            }

            if (!mechanical.isActive() && !shockTail.isShockActive() && shockDrainSamples == 0
                && tailDrainSamples > 0) {
                --tailDrainSamples;
            }
        }

        if (!isActive()) {
            lid.reset();
            perspective.reset();
        }
    }
};

} // namespace devpiano::audio
