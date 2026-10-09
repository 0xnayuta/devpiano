#include <JuceHeader.h>

#include "Audio/BuiltinSynthesiser.h"
#include "Audio/PianoSynthVoice.h"
#include "Audio/PianoTuning.h"
#include <algorithm>
#include <cmath>

namespace {

struct PianoFixture {
    devpiano::audio::BuiltinSynthesiser synth;
    PianoSynthVoice* voice = nullptr;

    PianoFixture(double rate, bool stretch, float duplex) {
        synth.addSound(new PianoSynthSound());
        voice = new PianoSynthVoice();
        voice->setAdsrParameters({ 0.0002f, 0.001f, 1.0f, 0.01f });
        voice->setPianoTuning(stretch, duplex);
        voice->setPedalNoiseLevel(0.0f);
        synth.addVoice(voice);
        synth.setCurrentPlaybackSampleRate(rate);
    }

    void render(juce::AudioBuffer<float>& buffer) {
        buffer.clear();
        synth.renderNextBlock(buffer, juce::MidiBuffer(), 0, buffer.getNumSamples());
    }
};

double spectralEnergy(const juce::AudioBuffer<float>& buffer, double rate, double frequency) {
    const auto start = static_cast<int>(0.08 * rate);
    const auto count = buffer.getNumSamples() - start;
    const auto step = juce::MathConstants<double>::twoPi * frequency / rate;
    const auto cosine = std::cos(step);
    const auto sine = std::sin(step);
    const auto windowStep = juce::MathConstants<double>::twoPi / static_cast<double>(count - 1);
    const auto windowCosine = std::cos(windowStep);
    const auto windowSine = std::sin(windowStep);
    double phaseX = 1.0;
    double phaseY = 0.0;
    double windowX = 1.0;
    double windowY = 0.0;
    double real = 0.0;
    double imaginary = 0.0;
    for (int i = 0; i < count; ++i) {
        const auto sample = 0.5 * static_cast<double>(buffer.getSample(0, start + i) + buffer.getSample(1, start + i));
        const auto weighted = sample * 0.5 * (1.0 - windowX);
        real += weighted * phaseX;
        imaginary += weighted * phaseY;
        const auto nextX = phaseX * cosine - phaseY * sine;
        phaseY = phaseX * sine + phaseY * cosine;
        phaseX = nextX;
        const auto nextWindowX = windowX * windowCosine - windowY * windowSine;
        windowY = windowX * windowSine + windowY * windowCosine;
        windowX = nextWindowX;
    }
    return real * real + imaginary * imaginary;
}

double centralPeak(const juce::AudioBuffer<float>& buffer, double rate, double reference) {
    auto bestFrequency = reference;
    auto bestEnergy = -1.0;
    for (int step = -30; step <= 30; ++step) {
        const auto frequency = reference + static_cast<double>(step) * 0.01;
        const auto energy = spectralEnergy(buffer, rate, frequency);
        if (energy > bestEnergy) {
            bestEnergy = energy;
            bestFrequency = frequency;
        }
    }
    return bestFrequency;
}

juce::AudioBuffer<float> renderReleasedTail(int partition, float duplex, PianoSynthVoice::LidPosition lid,
                                            PianoSynthVoice::SoundPerspective perspective) {
    PianoFixture fixture(48000.0, false, duplex);
    fixture.voice->setLidPosition(lid);
    fixture.voice->setSoundPerspective(perspective);
    fixture.synth.noteOn(2, 84, 0.5f);
    juce::AudioBuffer<float> held(2, 6144);
    fixture.render(held);
    fixture.synth.noteOff(2, 84, 1.0f, true);

    juce::AudioBuffer<float> released(2, 48000);
    released.clear();
    for (int offset = 0; offset < released.getNumSamples(); offset += partition) {
        const auto count = std::min(partition, released.getNumSamples() - offset);
        fixture.synth.renderNextBlock(released, {}, offset, count);
    }
    return released;
}

} // namespace

class PianoTuningAndDuplexTest final : public juce::UnitTest {
public:
    PianoTuningAndDuplexTest()
        : juce::UnitTest("Piano tuning and passive duplex", "DevPiano/Engine") {
    }

    void runTest() override {
        beginTest("Actual A4 centre remains anchored independently of stiffness and stretch switch");
        for (const auto reference : { 400.0, 440.0, 480.0 }) {
            PianoFixture fixture(48000.0, reference != 400.0, 0.0f);
            fixture.voice->setReferencePitchA4(reference);
            fixture.synth.noteOn(1, 69, 0.35f);
            juce::AudioBuffer<float> buffer(2, 48000 * 4);
            fixture.render(buffer);
            expectWithinAbsoluteError(centralPeak(buffer, 48000.0, reference), reference, 0.07);
        }

        beginTest("Zero stiffness has harmonic partials and an unchanged first partial");
        for (int partial = 1; partial <= 20; ++partial) {
            expectEquals(devpiano::audio::normalizedPianoPartialRatio(0.0, partial), static_cast<double>(partial));
        }

        beginTest("Unpedaled duplex outlives the main release without losing its MIDI channel owner");
        PianoFixture dry(48000.0, false, 0.0f);
        PianoFixture wet(48000.0, false, 1.0f);
        juce::AudioBuffer<float> dryBlock(2, 512);
        juce::AudioBuffer<float> wetBlock(2, 512);
        dry.synth.noteOn(2, 84, 0.5f);
        wet.synth.noteOn(2, 84, 0.5f);
        for (int i = 0; i < 12; ++i) {
            dry.render(dryBlock);
            wet.render(wetBlock);
        }
        dry.synth.noteOff(2, 84, 1.0f, true);
        wet.synth.noteOff(2, 84, 1.0f, true);
        for (int i = 0; i < 20; ++i) {
            dry.render(dryBlock);
            wet.render(wetBlock);
        }
        expectEquals(dryBlock.getMagnitude(0, dryBlock.getNumSamples()), 0.0f);
        expectGreaterThan(wetBlock.getMagnitude(0, wetBlock.getNumSamples()), 1e-8f);
        wet.synth.handleController(1, 120, 0);
        wet.render(wetBlock);
        expectGreaterThan(wetBlock.getMagnitude(0, wetBlock.getNumSamples()), 1e-8f);
        wet.synth.handleController(2, 120, 0);
        wet.render(wetBlock);
        expectEquals(wetBlock.getMagnitude(0, wetBlock.getNumSamples()), 0.0f);
        expect(!wet.voice->isVoiceActive());

        beginTest("Duplex release preserves shared output filters across rendering partitions");
        for (const auto duplex : { 0.15f, 1.0f }) {
            for (const auto lid : { PianoSynthVoice::LidPosition::fullOpen, PianoSynthVoice::LidPosition::closed }) {
                for (const auto perspective :
                     { PianoSynthVoice::SoundPerspective::player, PianoSynthVoice::SoundPerspective::audience }) {
                    const auto reference = renderReleasedTail(1, duplex, lid, perspective);
                    expectGreaterThan(reference.getMagnitude(0, 4096, 512), 1e-8f);
                    for (const auto partition : { 64, 512, 1024 }) {
                        const auto partitioned = renderReleasedTail(partition, duplex, lid, perspective);
                        auto maxError = 0.0f;
                        for (int channel = 0; channel < reference.getNumChannels(); ++channel) {
                            for (int sample = 0; sample < reference.getNumSamples(); ++sample) {
                                maxError = std::max(maxError,
                                                    std::abs(reference.getSample(channel, sample)
                                                             - partitioned.getSample(channel, sample)));
                            }
                        }
                        expectWithinAbsoluteError(maxError, 0.0f, 1e-7f);
                    }
                }
            }
        }

        beginTest("Out-of-band duplex segments are bypassed rather than aliased or frequency-clamped");
        PianoFixture lowDry(8000.0, true, 0.0f);
        PianoFixture lowWet(8000.0, true, 1.0f);
        lowDry.synth.noteOn(1, 108, 0.5f);
        lowWet.synth.noteOn(1, 108, 0.5f);
        for (int block = 0; block < 16; ++block) {
            lowDry.render(dryBlock);
            lowWet.render(wetBlock);
            for (int channel = 0; channel < 2; ++channel) {
                for (int sample = 0; sample < 512; ++sample) {
                    const auto dryValue = dryBlock.getSample(channel, sample);
                    const auto wetValue = wetBlock.getSample(channel, sample);
                    expect(std::isfinite(wetValue) && std::abs(wetValue) < 2.0f);
                    expectWithinAbsoluteError(wetValue, dryValue, 1e-7f);
                }
            }
        }
    }
};

static PianoTuningAndDuplexTest pianoTuningAndDuplexTest;
