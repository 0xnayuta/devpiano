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

        beginTest("Coherent band keeps the retained edge partial sounding and prunes past it");
        for (const auto rate : { 44100.0, 48000.0 }) {
            // Same production-render DFT evidence as the voice suite, through
            // the BuiltinSynthesiser path: retained edge partial shows real
            // energy at target with a nearby independent peak; the pruned
            // neighbour does not. No oscillator-state getters.
            struct EdgeCase {
                int note;
                double a4;
                int partial;
                bool retained;
            };
            // 临界 A4 由 ceiling(rate)/f6(note 102) 关系标定（Task 39-2 P1：84 以上按
            // 高音桥标准斜率外推后，note 93 的 8 分音在 A4<=480 内已无法越过 ceiling，
            // 改用 note 102 的末分音（第 6 分音）；retained 侧低于 coherent band ceiling、
            // past 侧高于 ceiling，两侧余量约 228-285 Hz。
            const auto edge = (rate == 48000.0) ? EdgeCase { 102, 442.0, 6, true } : EdgeCase { 102, 405.0, 6, true };
            const auto past = (rate == 48000.0) ? EdgeCase { 102, 452.0, 6, false } : EdgeCase { 102, 415.0, 6, false };
            for (const auto& band : { edge, past }) {
                PianoFixture fixture(rate, true, 0.0f);
                fixture.voice->setFeltAgeingAmount(0.0f);
                fixture.voice->setReferencePitchA4(band.a4);
                fixture.synth.noteOn(1, band.note, 0.35f);
                const auto totalSamples = static_cast<int>(rate);
                juce::AudioBuffer<float> stream(2, totalSamples);
                stream.clear();
                auto rendered = 0;
                {
                    juce::AudioBuffer<float> first(2, 2048);
                    first.clear();
                    fixture.synth.renderNextBlock(first, juce::MidiBuffer(), 0, 2048);
                    for (auto ch = 0; ch < 2; ++ch) {
                        stream.copyFrom(ch, 0, first, ch, 0, 2048);
                    }
                    rendered = 2048;
                }
                while (rendered < totalSamples) {
                    juce::AudioBuffer<float> block(2, 2048);
                    block.clear();
                    const auto count = std::min(2048, totalSamples - rendered);
                    fixture.synth.renderNextBlock(block, juce::MidiBuffer(), 0, count);
                    for (auto ch = 0; ch < 2; ++ch) {
                        stream.copyFrom(ch, rendered, block, ch, 0, count);
                    }
                    rendered += count;
                }
                const auto target
                    = PianoSynthVoice::partialFrequency(band.note, band.partial - 1, fixture.voice->getTemperament(),
                                                        fixture.voice->getReferencePitchA4(), true);
                const auto windowStart = static_cast<int>(0.05 * rate);
                const auto windowCount = juce::jmin(16384, totalSamples - windowStart);
                auto windowEnergy = [&](double frequency) {
                    auto real = 0.0;
                    auto imag = 0.0;
                    for (auto i = 0; i < windowCount; ++i) {
                        const auto window
                            = 0.5 * (1.0 - std::cos(juce::MathConstants<double>::twoPi * i / (windowCount - 1)));
                        const auto angle = juce::MathConstants<double>::twoPi * frequency
                            * (static_cast<double>(windowStart) + static_cast<double>(i)) / rate;
                        const auto value = 0.5
                            * (stream.getSample(0, windowStart + i) + stream.getSample(1, windowStart + i)) * window;
                        real += value * std::cos(angle);
                        imag -= value * std::sin(angle);
                    }
                    return real * real + imag * imag;
                };
                const auto targetEnergy = windowEnergy(target);
                auto bestOffset = 0.0;
                auto bestEnergy = -1.0;
                for (auto step = -40; step <= 40; ++step) {
                    const auto candidate = target + static_cast<double>(step) * 0.5;
                    if (candidate <= 0.0 || candidate >= rate * 0.5) {
                        continue;
                    }
                    const auto energy = windowEnergy(candidate);
                    if (energy > bestEnergy) {
                        bestEnergy = energy;
                        bestOffset = candidate - target;
                    }
                }
                if (band.retained) {
                    const auto neighbourEnergy = windowEnergy(target * 1.01);
                    expect(targetEnergy > 0.25 * neighbourEnergy,
                           "retained target energy is credible against a 1% neighbour");
                    expect(std::abs(bestOffset) <= 8.0, "retained edge peak sits near its target");
                } else {
                    const auto neighbourEnergy = windowEnergy(target * 1.01);
                    // 1e-9 是远高于浮点/滤波器噪声底的 credible peak 门限；
                    // 仅比较能量比会在双方都处于 ~1e-13 噪声时产生假阳性。
                    constexpr auto kCrediblePeakEnergyFloor = 1e-9;
                    const auto crediblePeak = targetEnergy > kCrediblePeakEnergyFloor
                        && targetEnergy > 4.0 * neighbourEnergy && std::abs(bestOffset) <= 2.0;
                    expect(!crediblePeak, "pruned edge partial leaves no credible peak at target");
                }
            }
        }

        beginTest("Voice mono tail equals processed stereo downmix across partitions");
        for (const auto partition : { 64, 512 }) {
            juce::AudioBuffer<float> stereoTail(2, 8192);
            juce::AudioBuffer<float> monoTail(1, 8192);
            for (const auto channels : { 1, 2 }) {
                PianoFixture fixture(44100.0, true, 0.15f);
                fixture.voice->setLidPosition(PianoSynthVoice::LidPosition::closed);
                fixture.voice->setSoundPerspective(PianoSynthVoice::SoundPerspective::audience);
                fixture.voice->setFeltAgeingAmount(0.0f);
                fixture.synth.noteOn(1, 60, 0.35f);
                juce::AudioBuffer<float> held(channels, 1024);
                held.clear();
                fixture.synth.renderNextBlock(held, juce::MidiBuffer(), 0, held.getNumSamples());
                auto& tail = (channels == 2) ? stereoTail : monoTail;
                tail.clear();
                for (int offset = 0; offset < tail.getNumSamples(); offset += partition) {
                    const auto blockCount = std::min(partition, tail.getNumSamples() - offset);
                    fixture.synth.renderNextBlock(tail, {}, offset, blockCount);
                }
            }
            auto maxDelta = 0.0f;
            for (int sample = 0; sample < monoTail.getNumSamples(); ++sample) {
                const auto expected = 0.5f * (stereoTail.getSample(0, sample) + stereoTail.getSample(1, sample));
                maxDelta = std::max(maxDelta, std::abs(monoTail.getSample(0, sample) - expected));
            }
            expectWithinAbsoluteError(maxDelta, 0.0f, 1e-5f, "mono tail must equal the processed stereo downmix");
        }
    }
};

static PianoTuningAndDuplexTest pianoTuningAndDuplexTest;
