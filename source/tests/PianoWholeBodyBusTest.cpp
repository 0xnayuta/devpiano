#include <JuceHeader.h>

#include <array>
#include <cmath>
#include <utility>

#include "Audio/BuiltinSynthesiser.h"
#include "Audio/PianoSynthVoice.h"
#include "Audio/PianoWholeBodyBus.h"

class PianoWholeBodyBusTest final : public juce::UnitTest {
public:
    PianoWholeBodyBusTest()
        : juce::UnitTest("PianoWholeBodyBus", "DevPiano/Audio") {
    }

    void runTest() override {
        testCrossNoteOpenStringResonance();
        testVoiceEndDoesNotResetBusTail();
        testPianoAudibleWithoutPedalAcoustics();
    }

private:
    static constexpr int kSampleRate = 48000;
    static constexpr int kBlockSize = 512;

    void testCrossNoteOpenStringResonance() {
        beginTest("shared pool: an open C4 adds sympathetic energy to a C3-class strike");
        constexpr int sampleCount = 4096;
        const auto render = [](bool holdC4) {
            devpiano::audio::PianoWholeBodyBus bus;
            bus.prepare(kSampleRate);
            bus.setSustainPedal(false);
            if (holdC4) {
                bus.noteOnKey(60);
            }
            std::array<float, sampleCount> dryLeft {};
            std::array<float, sampleCount> dryRight {};
            std::array<float, sampleCount> drive {};
            // Short broadband piano-like drive, not a single dirac, so the
            // shared pool is exercised through its open-string modes.
            for (auto sample = 0; sample < 512; ++sample) {
                drive[static_cast<std::size_t>(sample)] = 0.20f
                    * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * 196.0 * sample / kSampleRate));
            }
            devpiano::audio::PianoBusInput input {
                dryLeft.data(), dryRight.data(), drive.data(), sampleCount, true,
            };
            juce::AudioBuffer<float> output(2, sampleCount);
            output.clear();
            bus.render(input, output, 0, sampleCount);
            auto energy = 0.0;
            for (auto sample = 0; sample < sampleCount; ++sample) {
                energy += static_cast<double>(output.getSample(0, sample)) * output.getSample(0, sample)
                    + static_cast<double>(output.getSample(1, sample)) * output.getSample(1, sample);
            }
            return std::pair { energy, bus.sympatheticPool.openNoteCount[0] };
        };

        const auto closed = render(false);
        const auto open = render(true);
        expectEquals(closed.second, 0, "no note is open without a held key");
        expectEquals(open.second, 1, "held C4 opens the shared class-0 string");
        expect(std::abs(open.first - closed.first) > 1.0e-4, "held C4 changes the shared whole-body response");

        // Isolate the pool contribution: a held C4 opens class 0, so a C3
        // strike drive produces a non-zero sympathetic tail while the
        // unopened pool stays silent.
        devpiano::audio::SympatheticResonancePool closedPool;
        devpiano::audio::SympatheticResonancePool openPool;
        closedPool.updateCoefficients(static_cast<double>(kSampleRate));
        openPool.updateCoefficients(static_cast<double>(kSampleRate));
        openPool.noteOnKey(60);
        auto closedPoolEnergy = 0.0;
        auto openPoolEnergy = 0.0;
        for (auto sample = 0; sample < 4096; ++sample) {
            const auto drive = sample < 512 ? 0.20f
                    * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * 196.0 * sample / kSampleRate))
                                            : 0.0f;
            const auto closedValue = closedPool.process(drive);
            const auto openValue = openPool.process(drive);
            closedPoolEnergy += static_cast<double>(closedValue) * closedValue;
            openPoolEnergy += static_cast<double>(openValue) * openValue;
        }
        expect(openPoolEnergy > closedPoolEnergy + 1.0e-6,
               "open C4 produces a non-zero sympathetic tail from a C3-class strike");
    }

    void testVoiceEndDoesNotResetBusTail() {
        beginTest("voice end leaves the shared soundboard/lid tail active");
        devpiano::audio::BuiltinSynthesiser synth;
        synth.setCurrentPlaybackSampleRate(kSampleRate);
        synth.setPedalAcousticsEnabled(true);
        synth.addSound(new PianoSynthSound());
        auto* voice = new PianoSynthVoice();
        voice->setAdsrParameters({ 0.01f, 0.2f, 0.8f, 0.3f });
        synth.addVoice(voice);

        synth.noteOn(1, 60, 0.8f);
        juce::AudioBuffer<float> buffer(2, kBlockSize);
        for (auto block = 0; block < 8; ++block) {
            buffer.clear();
            synth.renderNextBlock(buffer, {}, 0, kBlockSize);
        }
        synth.noteOff(1, 60, 0.5f, true);

        auto voiceEnded = false;
        auto busActiveAtVoiceEnd = false;
        auto busDrained = false;
        for (auto block = 0; block < 400; ++block) {
            buffer.clear();
            synth.renderNextBlock(buffer, {}, 0, kBlockSize);
            if (!voice->isVoiceActive() && !voiceEnded) {
                voiceEnded = true;
                busActiveAtVoiceEnd = synth.getWholeBodyBus().isActive();
            }
            if (!synth.getWholeBodyBus().isActive()) {
                busDrained = true;
                break;
            }
        }
        // 活动计数是固定时间预算，不等于仍有可听能量；此处只验证状态
        // 不被声部结束清空，音频尾音由总线其他用例覆盖。
        expect(voiceEnded, "voice releases after the note-off tail");
        expect(busActiveAtVoiceEnd, "shared bus must still be active when the voice ends");
        expect(busDrained, "shared bus tail reaches a bounded end");
    }

    void testPianoAudibleWithoutPedalAcoustics() {
        beginTest("piano voices stay audible when pedal acoustics are disabled");
        devpiano::audio::BuiltinSynthesiser synth;
        synth.setCurrentPlaybackSampleRate(kSampleRate);
        // 默认关闭踏板机械声，钢琴声部仍必须经共享总线发声。
        expect(!synth.isPedalAcousticsEnabled(), "pedal acoustics must default to disabled");
        synth.addSound(new PianoSynthSound());
        synth.addVoice(new PianoSynthVoice());
        synth.noteOn(1, 60, 0.8f);

        juce::AudioBuffer<float> buffer(2, kBlockSize);
        auto peak = 0.0f;
        for (auto block = 0; block < 4; ++block) {
            buffer.clear();
            synth.renderNextBlock(buffer, {}, 0, kBlockSize);
            peak = std::max(peak, buffer.getMagnitude(0, 0, kBlockSize));
        }
        expect(peak > 1.0e-3f, "disabled pedal acoustics must not silence the piano");
        expect(std::isfinite(buffer.getRMSLevel(0, 0, kBlockSize)), "disabled switch output stays finite");
    }
};

static PianoWholeBodyBusTest pianoWholeBodyBusTest;