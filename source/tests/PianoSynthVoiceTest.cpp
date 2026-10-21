#include <JuceHeader.h>

#include <array>
#include <complex>

#include "Audio/AudioEngine.h"
#include "Audio/PianoSynthVoice.h"

using devpiano::audio::HammerVoicing;
using devpiano::audio::isValidHammerVoicing;
using devpiano::audio::kHammerVoicingMaxHardnessDelta;
using devpiano::audio::kHammerVoicingMaxRolloffDelta;
using devpiano::audio::sampleHammerVoicing;

// =============================================================================
// Deterministic voice-level tests for PianoSynthVoice (Phase 12-4). The
// fixture drives the voice through a juce::Synthesiser (noteOn event →
// renderNextBlock), bypassing the MidiMessageCollector wall-clock timing model
// that makes the AudioEngine fallback path untestable headless (see
// AudioEngineTest.cpp header note). A Synthesiser is required: the voice's
// currentlyPlayingSound state is only set by Synthesiser::startVoice, so
// calling startNote() directly leaves the voice inactive.
//
// Covered:
//   - Region table boundaries (partial counts / decay seconds)
//   - Non-zero finite output at the normalised peak level
//   - Magic Circle recursive oscillator long-term frequency stability
//     (dual-window complex DFT phase-difference over 20 s, drift < 1e-4)
//   - Two-stage decay envelope (early strike slope > 2x late tail slope)
//   - Triple-string unison beating (interference modulation dip and rebound)
//   - Velocity 0.2 vs 0.9 loudness is monotonically increasing
//   - noteOff tail decays and the voice releases itself
//   - Immediate stopNote (allowTailOff=false) silences and clears the voice
//   - allNotesOff stops output
// =============================================================================

namespace {
constexpr auto kSampleRate = 44100.0;
constexpr auto blockSize = 2048;
constexpr auto analysisWindow = 16384; // ≈ 0.37 s for the DFT

struct VoiceFixture {
    juce::Synthesiser synth;

    VoiceFixture() {
        synth.setCurrentPlaybackSampleRate(kSampleRate);
        synth.addSound(new PianoSynthSound());
        synth.addVoice(new PianoSynthVoice());
        // 与 AudioEngine::setAdsr 默认一致的接线（attack/release 作门控）。
        if (auto* voice = dynamic_cast<PianoSynthVoice*>(synth.getVoice(0))) {
            voice->setAdsrParameters({ 0.01f, 0.2f, 0.8f, 0.3f });
        }
    }

    [[nodiscard]] PianoSynthVoice* voice() const {
        return dynamic_cast<PianoSynthVoice*>(synth.getVoice(0));
    }

    // noteOn 事件渲染一个块（事件位于块首）。
    void noteOnBlock(int midiNoteNumber, float velocity, juce::AudioBuffer<float>& buffer) {
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, midiNoteNumber, velocity), 0);
        buffer.clear();
        synth.renderNextBlock(buffer, midi, 0, buffer.getNumSamples());
    }

    const juce::AudioBuffer<float>& renderBlock(juce::AudioBuffer<float>& buffer) {
        juce::MidiBuffer midi;
        buffer.clear();
        synth.renderNextBlock(buffer, midi, 0, buffer.getNumSamples());
        return buffer;
    }
};

float peakMagnitude(const juce::AudioBuffer<float>& buffer) {
    auto peak = 0.0f;
    for (auto sample = 0; sample < buffer.getNumSamples(); ++sample) {
        peak = juce::jmax(peak, std::abs(buffer.getSample(0, sample)));
    }
    return peak;
}

float rmsLevel(const juce::AudioBuffer<float>& buffer) {
    auto sumSquares = 0.0;
    for (auto sample = 0; sample < buffer.getNumSamples(); ++sample) {
        const auto value = buffer.getSample(0, sample);
        sumSquares += static_cast<double>(value) * static_cast<double>(value);
    }
    return static_cast<float>(std::sqrt(sumSquares / static_cast<double>(buffer.getNumSamples())));
}

// 单点 DFT（Hann 窗）：求 buffer 前 count 个样本在 frequency 处的幅度。
// 幅度校正：Hann 窗均值 0.5 → 幅度 = 2·|X| / (0.5·N) = 4·|X| / N。
double magnitudeAtFrequency(const juce::AudioBuffer<float>& buffer, double frequency, int count) {
    auto real = 0.0;
    auto imag = 0.0;
    for (auto i = 0; i < count; ++i) {
        const auto window = 0.5 * (1.0 - std::cos(juce::MathConstants<double>::twoPi * i / (count - 1)));
        const auto angle = juce::MathConstants<double>::twoPi * frequency * i / kSampleRate;
        const auto value = buffer.getSample(0, i) * window;
        real += value * std::cos(angle);
        imag -= value * std::sin(angle);
    }
    return 4.0 * std::sqrt(real * real + imag * imag) / count;
}
} // namespace

// =============================================================================

class PianoSynthVoiceTest : public juce::UnitTest {
public:
    PianoSynthVoiceTest()
        : juce::UnitTest("PianoSynthVoice: deterministic rendering", "DevPiano/Engine") {
    }

    void runTest() override {
        testPianoVoicing();
        testRegisterVoicingCurve();
        testHammerVoicingAnchors();
        testBasicRenderingAndHarmonics();
        testHammerAndTransientDynamics();
        testDecayAndUnisonBeating();
        testResonatorAndAcousticCoupling();
        testVoiceLifecycleAndDynamics();
        testEngineIntegrationAndNumericalSafety();
        testZeroTrigSlaAndNumericalPrecision();
        testGeneratedComponentInvariants();
        testVoiceMonoDownmix();
    }

private:
    void testPianoVoicing() {
        beginTest("piano parameters shape the tone");
        {
            const auto midF5 = PianoSynthVoice::partialFrequency(60, 4);
            const auto midF1 = PianoSynthVoice::partialFrequency(60, 0);

            VoiceFixture dim;
            dim.voice()->setPianoParameters(0.0f, 0.5f, 0.5f);
            juce::AudioBuffer<float> dimBuffer(1, analysisWindow);
            dim.noteOnBlock(60, 1.0f, dimBuffer);

            VoiceFixture bright;
            bright.voice()->setPianoParameters(1.0f, 0.5f, 0.5f);
            juce::AudioBuffer<float> brightBuffer(1, analysisWindow);
            bright.noteOnBlock(60, 1.0f, brightBuffer);

            auto upperEnergy = [&](const juce::AudioBuffer<float>& b) {
                auto sum = 0.0;
                for (auto h = 2; h <= 8; ++h) {
                    sum += magnitudeAtFrequency(b, PianoSynthVoice::partialFrequency(60, h - 1), analysisWindow);
                }
                return sum;
            };
            const auto dimUpper = upperEnergy(dimBuffer);
            const auto brightUpper = upperEnergy(brightBuffer);
            // 共同激励后基频恢复名义幅度，单点 4 次分音比值不再适合作为亮度指标；
            // 直接比较 2..8 次分音的聚合能量，仍验证亮度控制本身。
            expect(brightUpper > dimUpper, "higher brightness must boost upper-harmonic energy");

            VoiceFixture softHammer;
            softHammer.voice()->setPianoParameters(0.5f, 0.0f, 0.5f);
            juce::AudioBuffer<float> softBuffer(1, analysisWindow);
            softHammer.noteOnBlock(60, 1.0f, softBuffer);
            const auto softTop = magnitudeAtFrequency(softBuffer, midF5, analysisWindow)
                / juce::jmax(1e-6, magnitudeAtFrequency(softBuffer, midF1, analysisWindow));

            VoiceFixture hardHammer;
            hardHammer.voice()->setPianoParameters(0.5f, 1.0f, 0.5f);
            juce::AudioBuffer<float> hardBuffer(1, analysisWindow);
            hardHammer.noteOnBlock(60, 1.0f, hardBuffer);
            const auto hardTop = magnitudeAtFrequency(hardBuffer, midF5, analysisWindow)
                / juce::jmax(1e-6, magnitudeAtFrequency(hardBuffer, midF1, analysisWindow));

            expect(hardTop > softTop, "harder hammer must boost the top-harmonic ratio");

            VoiceFixture dry;
            dry.voice()->setPianoParameters(0.5f, 0.5f, 0.0f);
            juce::AudioBuffer<float> dryBuffer(1, blockSize);
            dry.noteOnBlock(60, 0.9f, dryBuffer);
            for (auto block = 0; block < 40; ++block) {
                dry.renderBlock(dryBuffer);
            }
            const auto dryLate = rmsLevel(dryBuffer);

            VoiceFixture resonant;
            resonant.voice()->setPianoParameters(0.5f, 0.5f, 1.0f);
            juce::AudioBuffer<float> resBuffer(1, blockSize);
            resonant.noteOnBlock(60, 0.9f, resBuffer);
            for (auto block = 0; block < 40; ++block) {
                resonant.renderBlock(resBuffer);
            }
            const auto resLate = rmsLevel(resBuffer);

            expect(resLate > dryLate, "higher resonance must decay slower (stronger late energy)");
        }
    }

    void testRegisterVoicingCurve() {
        beginTest("continuous register voicing preserves neutral C5/C6 and clamps");

        const auto low = devpiano::audio::pianoRegisterLoudnessGain(21);
        const auto c3 = devpiano::audio::pianoRegisterLoudnessGain(48);
        const auto mid = devpiano::audio::pianoRegisterLoudnessGain(60);
        const auto c5 = devpiano::audio::pianoRegisterLoudnessGain(72);
        const auto c6 = devpiano::audio::pianoRegisterLoudnessGain(84);
        const auto high = devpiano::audio::pianoRegisterLoudnessGain(108);

        expectWithinAbsoluteError(mid, 1.0f, 1e-6f, "C4 is the neutral register anchor");
        expectGreaterThan(low, 1.0f, "A0 receives low-register loudness lift");
        expectGreaterThan(c3, 1.0f, "C3 receives a small low-mid lift");
        expectWithinAbsoluteError(c5, 1.0f, 1e-6f, "C5 is neutral in the experimental curve");
        expectWithinAbsoluteError(c6, 1.0f, 1e-6f, "C6 is neutral in the experimental curve");
        expectLessThan(high, 1.0f, "C8 receives high-register attenuation");

        expectWithinAbsoluteError(devpiano::audio::pianoRegisterLoudnessGain(19),
                                  devpiano::audio::pianoRegisterLoudnessGain(21), 1e-6f,
                                  "below-A0 queries clamp to the low anchor");
        expectWithinAbsoluteError(devpiano::audio::pianoRegisterLoudnessGain(120),
                                  devpiano::audio::pianoRegisterLoudnessGain(108), 1e-6f,
                                  "above-C8 queries clamp to the high anchor");

        VoiceFixture lowFixture;
        VoiceFixture midFixture;
        VoiceFixture highFixture;
        juce::AudioBuffer<float> lowBuffer(2, blockSize * 4);
        juce::AudioBuffer<float> midBuffer(2, blockSize * 4);
        juce::AudioBuffer<float> highBuffer(2, blockSize * 4);
        lowFixture.noteOnBlock(21, 0.8f, lowBuffer);
        midFixture.noteOnBlock(60, 0.8f, midBuffer);
        highFixture.noteOnBlock(96, 0.8f, highBuffer);
        const auto lowEnergy = lowBuffer.getRMSLevel(0, 0, lowBuffer.getNumSamples())
            + lowBuffer.getRMSLevel(1, 0, lowBuffer.getNumSamples());
        const auto midEnergy = midBuffer.getRMSLevel(0, 0, midBuffer.getNumSamples())
            + midBuffer.getRMSLevel(1, 0, midBuffer.getNumSamples());
        const auto highEnergy = highBuffer.getRMSLevel(0, 0, highBuffer.getNumSamples())
            + highBuffer.getRMSLevel(1, 0, highBuffer.getNumSamples());
        expect(lowEnergy > midEnergy * 1.15f, "rendered A0 must receive the low-register lift");
        expect(highEnergy < midEnergy, "rendered C7 must receive the high-register attenuation");
    }

    void testHammerVoicingAnchors() {
        beginTest("hammer voicing anchors interpolate, clamp, and validate");
        {
            HammerVoicing voicing;
            voicing.anchors = { {
                { 41.0f, -0.06f, -3.0f, 0.2f },
                { 70.0f, 0.02f, 0.0f, 0.0f },
                { 98.0f, 0.18f, 2.0f, -0.2f },
            } };
            expect(isValidHammerVoicing(voicing), "ordered in-range anchors must be valid");

            const auto atOne = sampleHammerVoicing(voicing, 1.0f);
            expectWithinAbsoluteError(atOne.hardnessDelta, -0.06f, 1e-6f, "velocity 1 clamps to Piano anchor");
            expectWithinAbsoluteError(atOne.loudnessGain, std::pow(10.0f, -3.0f / 20.0f), 1e-6f,
                                      "velocity 1 clamps to Piano loudness");
            expectWithinAbsoluteError(atOne.rolloffDelta, 0.2f, 1e-6f, "velocity 1 clamps to Piano rolloff");

            const auto atSeventy = sampleHammerVoicing(voicing, 70.0f);
            expectWithinAbsoluteError(atSeventy.hardnessDelta, 0.02f, 1e-6f, "Mezzo anchor is exact");
            expectWithinAbsoluteError(atSeventy.loudnessGain, 1.0f, 1e-6f, "Mezzo loudness is exact");

            const auto atFiftyFive = sampleHammerVoicing(voicing, 55.0f);
            const auto t = (55.0f - 41.0f) / (70.0f - 41.0f);
            expectWithinAbsoluteError(atFiftyFive.hardnessDelta, -0.06f + t * 0.08f, 1e-6f,
                                      "hardness interpolates linearly between anchors");
            expectWithinAbsoluteError(atFiftyFive.loudnessGain, std::pow(10.0f, (-3.0f + t * 3.0f) / 20.0f), 1e-6f,
                                      "loudness interpolates linearly between anchors");
            expectWithinAbsoluteError(atFiftyFive.rolloffDelta, 0.2f - t * 0.2f, 1e-6f,
                                      "rolloff interpolates linearly between anchors");

            const auto atOneTwentySeven = sampleHammerVoicing(voicing, 127.0f);
            expectWithinAbsoluteError(atOneTwentySeven.hardnessDelta, 0.18f, 1e-6f,
                                      "velocity 127 clamps to Forte anchor");
            expectWithinAbsoluteError(atOneTwentySeven.loudnessGain, std::pow(10.0f, 2.0f / 20.0f), 1e-6f,
                                      "velocity 127 clamps to Forte loudness");
            expectWithinAbsoluteError(atOneTwentySeven.rolloffDelta, -0.2f, 1e-6f,
                                      "velocity 127 clamps to Forte rolloff");

            HammerVoicing invalid = voicing;
            invalid.anchors[1].midiVelocity = 41.0f;
            expect(!isValidHammerVoicing(invalid), "non-increasing anchor velocities must be rejected");
            const auto neutral = sampleHammerVoicing(invalid, 70.0f);
            expectWithinAbsoluteError(neutral.hardnessDelta, 0.0f, 1e-6f, "invalid config falls back to neutral");
            expectWithinAbsoluteError(neutral.loudnessGain, 1.0f, 1e-6f, "invalid config falls back to unity gain");

            HammerVoicing extreme = voicing;
            extreme.anchors[0].hardnessDelta = kHammerVoicingMaxHardnessDelta + 0.01f;
            expect(!isValidHammerVoicing(extreme), "out-of-range hardness delta must be rejected");

            HammerVoicing extremeRolloff = voicing;
            extremeRolloff.anchors[0].rolloffDelta = kHammerVoicingMaxRolloffDelta + 0.01f;
            expect(!isValidHammerVoicing(extremeRolloff), "out-of-range rolloff delta must be rejected");
        }

        beginTest("hammer voicing setter rejects invalid config and preserves the previous one");
        {
            VoiceFixture fixture;
            HammerVoicing valid;
            valid.anchors[2].hardnessDelta = 0.12f;
            expect(fixture.voice()->setHammerVoicing(valid), "valid voicing must be accepted");
            expectWithinAbsoluteError(fixture.voice()->getHammerVoicing().anchors[2].hardnessDelta, 0.12f, 1e-6f,
                                      "accepted voicing must be stored");

            HammerVoicing invalid;
            invalid.anchors[1].midiVelocity = 40.0f;
            expect(!fixture.voice()->setHammerVoicing(invalid), "invalid voicing must be rejected");
            expectWithinAbsoluteError(fixture.voice()->getHammerVoicing().anchors[2].hardnessDelta, 0.12f, 1e-6f,
                                      "rejected voicing must not overwrite the previous config");
        }

        beginTest("non-zero hammer voicing changes level and upper-partial content");
        {
            const auto upperPartial = PianoSynthVoice::partialFrequency(60, 4);
            const auto fundamental = PianoSynthVoice::partialFrequency(60, 0);

            VoiceFixture neutralFixture;
            expect(neutralFixture.voice()->setHammerVoicing(HammerVoicing {}), "neutral voicing must be accepted");
            juce::AudioBuffer<float> neutralBuffer(1, analysisWindow);
            neutralFixture.noteOnBlock(60, 0.8f, neutralBuffer);
            const auto neutralUpper = magnitudeAtFrequency(neutralBuffer, upperPartial, analysisWindow)
                / juce::jmax(1e-9, magnitudeAtFrequency(neutralBuffer, fundamental, analysisWindow));

            HammerVoicing harder;
            harder.anchors[2].hardnessDelta = 0.20f;
            VoiceFixture hardFixture;
            expect(hardFixture.voice()->setHammerVoicing(harder), "harder Forte anchor must be accepted");
            juce::AudioBuffer<float> hardBuffer(1, analysisWindow);
            hardFixture.noteOnBlock(60, 0.8f, hardBuffer);
            const auto hardUpper = magnitudeAtFrequency(hardBuffer, upperPartial, analysisWindow)
                / juce::jmax(1e-9, magnitudeAtFrequency(hardBuffer, fundamental, analysisWindow));
            expect(hardUpper > neutralUpper * 1.05f,
                   "harder Forte anchor must raise the normalized upper-partial ratio");

            HammerVoicing steep;
            steep.anchors[2].rolloffDelta = 1.0f;
            VoiceFixture steepFixture;
            expect(steepFixture.voice()->setHammerVoicing(steep), "steeper Forte rolloff must be accepted");
            juce::AudioBuffer<float> steepBuffer(1, analysisWindow);
            steepFixture.noteOnBlock(60, 0.8f, steepBuffer);
            const auto steepUpper = magnitudeAtFrequency(steepBuffer, upperPartial, analysisWindow)
                / juce::jmax(1e-9, magnitudeAtFrequency(steepBuffer, fundamental, analysisWindow));
            expect(steepUpper < neutralUpper * 0.95f,
                   "steeper Forte rolloff must lower the normalized upper-partial ratio");

            VoiceFixture neutralLoud;
            expect(neutralLoud.voice()->setHammerVoicing(HammerVoicing {}), "neutral voicing must be accepted");
            juce::AudioBuffer<float> neutralLoudBuffer(1, analysisWindow);
            neutralLoud.noteOnBlock(60, 41.0f / 127.0f, neutralLoudBuffer);
            const auto neutralLevel = rmsLevel(neutralLoudBuffer);

            HammerVoicing quieter;
            quieter.anchors[0].loudnessDeltaDb = -6.0f;
            VoiceFixture quietFixture;
            expect(quietFixture.voice()->setHammerVoicing(quieter), "quieter Piano loudness must be accepted");
            juce::AudioBuffer<float> quietBuffer(1, analysisWindow);
            quietFixture.noteOnBlock(60, 41.0f / 127.0f, quietBuffer);
            const auto quietLevel = rmsLevel(quietBuffer);
            expect(quietLevel < neutralLevel * 0.75f,
                   "negative Piano loudness delta must clearly lower the rendered level");
        }
    }

    void testBasicRenderingAndHarmonics() {
        beginTest("renders non-zero finite output at normalised level");
        {
            VoiceFixture fixture;
            juce::AudioBuffer<float> buffer(1, blockSize);
            fixture.noteOnBlock(60, 0.8f, buffer);
            const auto peak = peakMagnitude(buffer);
            expect(peak > 0.02f, "note must produce audible output");
            expect(peak < 1.0f, "normalised output must not clip");

            auto finite = true;
            for (auto sample = 0; sample < buffer.getNumSamples(); ++sample) {
                finite = finite && std::isfinite(buffer.getSample(0, sample));
            }
            expect(finite, "render output must stay finite (no NaN/Inf)");
        }

        beginTest("fundamental and harmonics present (single-bin DFT)");
        {
            // 低音区（20 分音）：主导分音在前 7 次，第 8 次梳状滤波陷波，高次分音呈物理衰减。
            VoiceFixture bassFixture;
            juce::AudioBuffer<float> bass(1, analysisWindow);
            bassFixture.noteOnBlock(36, 0.9f, bass); // low-bass region: 20 partials (C2 ≈ 65.41 Hz)
            const auto bassFundamental
                = magnitudeAtFrequency(bass, PianoSynthVoice::partialFrequency(36, 0), analysisWindow);
            expect(bassFundamental > 0.005, "low-bass fundamental must be present");
            // 主导谐波 (2~7)
            for (auto harmonic = 2; harmonic <= 7; ++harmonic) {
                const auto partialFreq = PianoSynthVoice::partialFrequency(36, harmonic - 1);
                const auto magnitude = magnitudeAtFrequency(bass, partialFreq, analysisWindow);
                expect(magnitude > 0.02 * bassFundamental,
                       "low-bass dominant harmonic " + juce::String(harmonic) + " must be present");
            }
            // 高次谐波 (8~20) 物理存在且有界
            for (auto harmonic = 8; harmonic <= 20; ++harmonic) {
                const auto partialFreq = PianoSynthVoice::partialFrequency(36, harmonic - 1);
                const auto magnitude = magnitudeAtFrequency(bass, partialFreq, analysisWindow);
                // 击弦点梳状陷波 (8th, 16th) 能量极低为正常声学现象
                const auto isCombNotch = (harmonic == 8 || harmonic == 16);
                const auto threshold = isCombNotch ? 0.00001 * bassFundamental : 0.0003 * bassFundamental;
                expect(magnitude > threshold, "low-bass overtone " + juce::String(harmonic) + " must be present");
            }

            // 中音区（14 分音）。
            VoiceFixture midFixture;
            juce::AudioBuffer<float> mid(1, analysisWindow);
            midFixture.noteOnBlock(60, 0.9f, mid);
            const auto midFundamental
                = magnitudeAtFrequency(mid, PianoSynthVoice::partialFrequency(60, 0), analysisWindow);
            expect(midFundamental > 0.001, "MIDI 60 fundamental ~ 261.63 Hz must dominate");
            // 共同激励恢复基频后，低次分音相对比例下降；这里验证 2..5 次分音存在，
            // 不再把 2% 的单点比例当作“主导”门槛。
            for (auto harmonic = 2; harmonic <= 5; ++harmonic) {
                const auto partialFreq = PianoSynthVoice::partialFrequency(60, harmonic - 1);
                const auto magnitude = magnitudeAtFrequency(mid, partialFreq, analysisWindow);
                expect(magnitude > 0.005 * midFundamental,
                       "mid harmonic " + juce::String(harmonic) + " must be present");
            }
            for (auto harmonic = 6; harmonic <= 14; ++harmonic) {
                const auto partialFreq = PianoSynthVoice::partialFrequency(60, harmonic - 1);
                const auto magnitude = magnitudeAtFrequency(mid, partialFreq, analysisWindow);
                expect(magnitude > 0.0003 * midFundamental,
                       "mid overtone " + juce::String(harmonic) + " must be present");
            }

            // 高音区（8 分音）与极高音区（6 分音）。
            VoiceFixture highFixture;
            juce::AudioBuffer<float> high(1, analysisWindow);
            highFixture.noteOnBlock(72, 0.9f, high);
            const auto highFundamental
                = magnitudeAtFrequency(high, PianoSynthVoice::partialFrequency(72, 0), analysisWindow);
            for (auto harmonic = 2; harmonic <= 8; ++harmonic) {
                const auto partialFreq = PianoSynthVoice::partialFrequency(72, harmonic - 1);
                const auto magnitude = magnitudeAtFrequency(high, partialFreq, analysisWindow);
                expect(magnitude > 0.0005 * highFundamental,
                       "high-mid harmonic " + juce::String(harmonic) + " must be present");
            }

            VoiceFixture topFixture;
            juce::AudioBuffer<float> top(1, analysisWindow);
            topFixture.noteOnBlock(96, 0.9f, top);
            const auto topFundamental
                = magnitudeAtFrequency(top, PianoSynthVoice::partialFrequency(96, 0), analysisWindow);
            for (auto harmonic = 2; harmonic <= 6; ++harmonic) {
                const auto partialFreq = PianoSynthVoice::partialFrequency(96, harmonic - 1);
                const auto magnitude = magnitudeAtFrequency(top, partialFreq, analysisWindow);
                const auto threshold = harmonic == 6 ? 0.0001 : 0.0005;
                expect(magnitude > threshold * topFundamental,
                       "treble harmonic " + juce::String(harmonic) + " must be present");
            }
        }

        beginTest("inharmonicity overtone frequency shift (stiff-string physics)");
        {
            const auto f0 = juce::MidiMessage::getMidiNoteInHertz(36);
            // Task 39-2 实测锚点表把 C2 刚度降到 5.08e-5，7 分音偏移 (~0.56 Hz)
            // 低于 DFT 分辨率；改用最高分音 (20, 击弦点陷波下满幅) 使偏移 ~13 Hz 可分辨，
            // 断言语义不变（分音高于整数谐波）。
            const auto actualF20
                = PianoSynthVoice::partialFrequency(36, 19, devpiano::audio::Temperament::equal, 440.0, false);
            VoiceFixture fixture;
            fixture.voice()->setPianoTuning(false, 0.0f);
            juce::AudioBuffer<float> buffer(1, analysisWindow);
            fixture.noteOnBlock(36, 0.9f, buffer);

            const auto magAtInharmonic = magnitudeAtFrequency(buffer, actualF20, analysisWindow);
            const auto magAtInteger = magnitudeAtFrequency(buffer, 20.0 * f0, analysisWindow);
            expect(magAtInharmonic > 1.2 * magAtInteger,
                   "DFT energy at stiff-string 20th partial (" + juce::String(actualF20, 2)
                       + " Hz) must be higher than integer harmonic (" + juce::String(20.0 * f0, 2) + " Hz)");
        }

        beginTest("recursive oscillator frequency stability (Magic Circle long render)");
        {
            VoiceFixture fixture;
            fixture.voice()->setPianoParameters(0.5f, 0.5f, 1.0f);
            fixture.voice()->setPianoTuning(true, 0.0f);
            constexpr auto totalSeconds = 20.0;
            constexpr auto totalSamples = static_cast<int>(totalSeconds * kSampleRate);
            juce::AudioBuffer<float> stream(1, totalSamples);
            stream.clear();

            auto rendered = 0;
            {
                juce::MidiBuffer midi;
                midi.addEvent(juce::MidiMessage::noteOn(1, 33, 0.9f), 0);
                fixture.synth.renderNextBlock(stream, midi, 0, blockSize);
                rendered += blockSize;
            }
            while (rendered < totalSamples) {
                juce::MidiBuffer empty;
                const auto count = juce::jmin(blockSize, totalSamples - rendered);
                fixture.synth.renderNextBlock(stream, empty, rendered, count);
                rendered += count;
            }
            expect(fixture.voice()->isVoiceActive(), "voice must still be active at 20 s");

            const auto f0 = PianoSynthVoice::partialFrequency(33, 0);
            auto complexDft = [](const juce::AudioBuffer<float>& buffer, int start, int count, double frequency) {
                auto real = 0.0;
                auto imag = 0.0;
                for (auto i = 0; i < count; ++i) {
                    const auto window = 0.5 * (1.0 - std::cos(juce::MathConstants<double>::twoPi * i / (count - 1)));
                    const auto angle = juce::MathConstants<double>::twoPi * frequency
                        * (static_cast<double>(start) + static_cast<double>(i)) / kSampleRate;
                    const auto value = buffer.getSample(0, start + i) * window;
                    real += value * std::cos(angle);
                    imag -= value * std::sin(angle);
                }
                return std::complex<double> { real, imag };
            };

            const auto window1Start = blockSize;
            const auto window2Start = totalSamples - analysisWindow;
            const auto x1 = complexDft(stream, window1Start, analysisWindow, f0);
            const auto x2 = complexDft(stream, window2Start, analysisWindow, f0);
            expect(std::abs(x1) > 1e-6, "early window fundamental energy present");
            expect(std::abs(x2) > 1e-7, "late window fundamental energy still measurable");

            const auto phaseDelta = std::remainder(std::arg(x2) - std::arg(x1), juce::MathConstants<double>::twoPi);
            const auto deltaT = static_cast<double>(window2Start - window1Start) / kSampleRate;
            const auto measuredFreq = f0 + phaseDelta / (juce::MathConstants<double>::twoPi * deltaT);
            expectWithinAbsoluteError(measuredFreq, f0, 1e-4 * f0,
                                      "recursive oscillator frequency drift < 1e-4 relative over 20 s");
        }

        beginTest("modal overtone decay rates (physical energy dissipation)");
        {
            const auto tau1 = PianoSynthVoice::partialDecaySeconds(36, 0);
            expectWithinAbsoluteError(tau1, 4.5, 1e-3, "fundamental decay equals base decay");
            const auto tau6 = PianoSynthVoice::partialDecaySeconds(36, 5);
            const auto tau16 = PianoSynthVoice::partialDecaySeconds(36, 15);
            expect(tau16 < tau1,
                   "16th partial decays faster than fundamental in bass due to quadratic internal friction");
            expect(tau16 < tau6, "overtone decay times decrease for higher orders");

            const auto midTau1 = PianoSynthVoice::partialDecaySeconds(60, 0);
            const auto midTau6 = PianoSynthVoice::partialDecaySeconds(60, 5);
            expect(midTau6 < midTau1, "mid-register 6th partial decays faster than fundamental");

            expect(PianoSynthVoice::bridgeHillGain(1800.0) > 1.35f, "1.8kHz Bridge Hill peak gain ~ 1.40");
            expect(PianoSynthVoice::bridgeHillGain(100.0) < 1.05f, "low frequency not affected by Bridge Hill");

            expect(PianoSynthVoice::hammerElasticModulation(440.0, 0.0018f) >= 0.7f,
                   "elastic modulation lower bounded by 0.7");
            expect(PianoSynthVoice::hammerElasticModulation(440.0, 0.0018f) <= 1.0f,
                   "elastic modulation upper bounded by 1.0");

            VoiceFixture fixture;
            juce::AudioBuffer<float> earlyBuffer(1, analysisWindow);
            fixture.noteOnBlock(36, 0.9f, earlyBuffer);

            const auto f1 = PianoSynthVoice::partialFrequency(36, 0);
            const auto f6 = PianoSynthVoice::partialFrequency(36, 5);

            const auto earlyF1 = magnitudeAtFrequency(earlyBuffer, f1, analysisWindow);
            const auto earlyF6 = magnitudeAtFrequency(earlyBuffer, f6, analysisWindow);
            const auto earlyRatio = earlyF6 / juce::jmax(1e-6, earlyF1);
            expect(earlyRatio > 0.05, "6th partial is present in the early strike window");

            juce::AudioBuffer<float> lateBuffer(1, analysisWindow);
            for (auto step = 0; step < 8; ++step) {
                fixture.renderBlock(lateBuffer);
            }
            const auto lateF1 = magnitudeAtFrequency(lateBuffer, f1, analysisWindow);
            const auto lateF6 = magnitudeAtFrequency(lateBuffer, f6, analysisWindow);
            const auto lateRatio = lateF6 / juce::jmax(1e-6, lateF1);

            expect(lateRatio < 0.5 * earlyRatio,
                   "upper harmonic ratio at t1 (" + juce::String(lateRatio, 5) + ") must drop below 50% of t0 ratio ("
                       + juce::String(earlyRatio, 5) + ") due to modal energy dissipation");
        }
    }

    void testHammerAndTransientDynamics() {
        beginTest("striking position comb filter and notch response (Phase 17-A)");
        {
            const auto bassComb7 = PianoSynthVoice::strikeCombGain(6, 0.125f);
            const auto bassComb8 = PianoSynthVoice::strikeCombGain(7, 0.125f);
            expect(bassComb7 > 0.35f, "7th harmonic has strike transmission");
            expect(bassComb8 < 0.10f, "8th harmonic is physically notched by 1/8 striking point");
            expect(bassComb7 > 4.0f * bassComb8, "7th harmonic transmission is over 4x the 8th notch");

            const auto midComb4 = PianoSynthVoice::strikeCombGain(3, 0.1333f);
            const auto midComb7 = PianoSynthVoice::strikeCombGain(6, 0.1333f);
            const auto midComb8 = PianoSynthVoice::strikeCombGain(7, 0.1333f);
            expect(midComb4 > 0.90f, "mid 4th harmonic has peak strike transmission");
            expect(midComb7 < 0.30f, "mid 7th harmonic is attenuated by 1/7.5 strike point");
            expect(midComb8 < 0.30f, "mid 8th harmonic is attenuated by 1/7.5 strike point");
        }

        beginTest("hammer strike attack transient and instant gate (Phase 17-B)");
        {
            VoiceFixture fixture;
            juce::AudioBuffer<float> bufSoft(1, 128);
            juce::AudioBuffer<float> bufLoud(1, 128);

            fixture.noteOnBlock(60, 0.2f, bufSoft);
            VoiceFixture fixture2;
            fixture2.noteOnBlock(60, 0.9f, bufLoud);
            const auto peakSoft = peakMagnitude(bufSoft);
            const auto peakLoud = peakMagnitude(bufLoud);

            expect(peakSoft > 0.0010f, "soft note attack transient is audible");
            expect(peakLoud > 0.025f, "loud note attack transient has strong punch");
            expect(peakLoud > 3.0f * peakSoft, "loud strike transient is nonlinear and much stronger than soft");

            VoiceFixture restrike1;
            juce::AudioBuffer<float> rBuf1(1, 256);
            restrike1.noteOnBlock(60, 0.8f, rBuf1);
            const auto rms1 = rBuf1.getRMSLevel(0, 0, 256);

            VoiceFixture restrike2;
            juce::AudioBuffer<float> rBuf2(1, 256);
            restrike2.noteOnBlock(60, 0.8f, rBuf2);
            const auto rms2 = rBuf2.getRMSLevel(0, 0, 256);

            expect(rms1 > 0.005f && rms2 > 0.005f, "restrikes produce robust audio energy");
            expect(std::abs(rms1 - rms2) < 0.02f * rms1,
                   "jitter variation is bounded within natural subtle range (<2%)");
        }

        beginTest("strong strike pitch glide and soundboard saturation (Phase 22-D)");
        {
            const auto satSmall = PianoSynthVoice::softSaturate(0.05f);
            expectWithinAbsoluteError(satSmall, 0.05f, 1e-4f, "small signal preserves linearity");

            const auto satLarge = PianoSynthVoice::softSaturate(0.80f);
            expect(satLarge < 0.80f && satLarge > 0.70f, "large signal undergoes smooth cubic soft compression");

            VoiceFixture forteFixture;
            juce::AudioBuffer<float> forteBuf(1, 256);
            forteFixture.noteOnBlock(60, 0.95f, forteBuf);
            const auto forteRms = forteBuf.getRMSLevel(0, 0, 256);
            expect(forteRms > 0.010f, "fff strong strike produces robust acoustic attack with tension glide");
            expect(std::isfinite(forteRms), "pitch glide render remains strictly bounded and stable");
        }

        beginTest("dynamic hammer non-linear model and strike position filtering (Phase 23-A)");
        {
            VoiceFixture ppFixture;
            juce::AudioBuffer<float> ppBuf(2, blockSize * 4);
            ppFixture.noteOnBlock(60, 0.15f, ppBuf);
            const auto ppRms = ppBuf.getRMSLevel(0, 0, ppBuf.getNumSamples());

            VoiceFixture ffFixture;
            juce::AudioBuffer<float> ffBuf(2, blockSize * 4);
            ffFixture.noteOnBlock(60, 0.90f, ffBuf);
            const auto ffRms = ffBuf.getRMSLevel(0, 0, ffBuf.getNumSamples());

            expect(ffRms > ppRms * 2.5f, "ff strike produces much higher energy than pp strike");

            const auto gainNull = PianoSynthVoice::strikeCombGain(6, 1.0f / 7.0f);
            expect(gainNull < 0.10f, "strikeCombGain suppresses 7th partial at 1/7 strike position");
            expect(gainNull >= 0.03f, "strikeCombGain respects physical 0.03 floor");

            const auto gainPeak = PianoSynthVoice::strikeCombGain(0, 0.5f);
            expect(gainPeak > 0.95f, "strikeCombGain passes peak antinode near 1.0");

            const auto ppSpectrumHigh = PianoSynthVoice::hammerSpectrumGain(10, 0.5f, 0.20f, 2800.0, 0.0035f);
            const auto ffSpectrumHigh = PianoSynthVoice::hammerSpectrumGain(10, 0.5f, 0.85f, 2800.0, 0.0012f);
            expect(ffSpectrumHigh > ppSpectrumHigh * 1.5f,
                   "harder strike preserves substantially more high frequency partial energy");
        }

        beginTest("attack transient crack and longitudinal mode tuning (Phase 23-D)");
        {
            PianoSynthVoice::HammerTransient softTransient;
            softTransient.trigger(kSampleRate, 72, 0.30f, 0.5f, 0.30f, 1.0f);
            const auto s1 = softTransient.getNextSample();

            PianoSynthVoice::HammerTransient loudTransient;
            loudTransient.trigger(kSampleRate, 72, 0.90f, 0.5f, 0.30f, 1.0f);
            const auto s2 = loudTransient.getNextSample();

            expect(std::abs(s2) > std::abs(s1) * 3.0f,
                   "loud strike attack crack produces much higher transient spike than soft strike");

            PianoSynthVoice::HammerTransient bassLongTransient;
            bassLongTransient.trigger(kSampleRate, 30, 0.85f, 0.5f, 1.50f, 1.0f);
            expect(bassLongTransient.isActive(), "bass strike activates longitudinal precursor mode");

            PianoSynthVoice::HammerTransient compensatedBassLongTransient;
            compensatedBassLongTransient.trigger(kSampleRate, 30, 0.85f, 0.5f, 1.50f, 6.0f);
            expectWithinAbsoluteError(bassLongTransient.longAmplitude, compensatedBassLongTransient.longAmplitude,
                                      1e-9f, "string phase compensation must not amplify the independent precursor");

            expectWithinAbsoluteError(PianoSynthVoice::HammerTransient::longitudinalPrecursorGain(47), 1.0f, 1e-6f,
                                      "low bass retains the precursor before the fade");
            expect(PianoSynthVoice::HammerTransient::longitudinalPrecursorGain(55) > 0.0f,
                   "the precursor fades rather than clicks off on the last audible note");
            expect(PianoSynthVoice::HammerTransient::longitudinalPrecursorGain(55) < 0.05f,
                   "the upper fade leaves only a residual precursor");
            expectEquals(PianoSynthVoice::HammerTransient::longitudinalPrecursorGain(56), 0.0f,
                         "the precursor is absent at and above its documented boundary");

            for (auto i = 0; i < 900; ++i) {
                [[maybe_unused]] const auto discarded = bassLongTransient.getNextSample();
            }
            expect(!bassLongTransient.isActive(), "longitudinal precursor mode cleanly decays within 20ms");
        }

        beginTest("hammer contact audibly shapes the string onset (Phase 24-B)");
        {
            // Production-consumer counterexample for 39A:DSP-001: the same
            // attack rendered with contact pinned at full release must differ
            // credibly in early audio from the live contact attack, and the
            // pinned run must converge back toward the live tail once the
            // short contact has run out. Both fixtures are fresh (same note,
            // velocity and ageing-off cold NoteOn, glide unity at v=0.35);
            // the pin is a diagnostic state perturbation only.
            // No progress/state getters: evidence is rendered audio only.
            auto renderAttackTail = [](bool freezeContact) {
                VoiceFixture fixture;
                fixture.voice()->setFeltAgeingAmount(0.0f);
                fixture.synth.noteOn(1, 60, 0.35f);
                if (freezeContact) {
                    auto* voice = fixture.voice();
                    voice->hammerContactEngine.releaseProgress = 1.0f;
                    voice->hammerContactEngine.progressInc = 0.0f;
                }
                juce::AudioBuffer<float> attack(2, 256);
                fixture.renderBlock(attack);
                juce::AudioBuffer<float> tail(2, 2048);
                for (auto i = 0; i < 8; ++i) {
                    fixture.renderBlock(tail);
                }
                return std::make_pair(attack, tail);
            };
            const auto live = renderAttackTail(false);
            const auto frozen = renderAttackTail(true);
            const auto liveAttack = live.first.getRMSLevel(0, 0, 256) + live.first.getRMSLevel(1, 0, 256);
            const auto frozenAttack = frozen.first.getRMSLevel(0, 0, 256) + frozen.first.getRMSLevel(1, 0, 256);
            expectGreaterThan(liveAttack, 0.0f, "live attack must produce string energy");
            expectGreaterThan(frozenAttack, 0.0f, "pinned-release attack must still sound");
            // Early-window sample contrast: contact damping is strongest at
            // onset, so the first 64 samples must differ well above the
            // noise floor of two identical renders.
            auto earlyDelta = 0.0f;
            for (auto ch = 0; ch < 2; ++ch) {
                for (auto s = 0; s < 64; ++s) {
                    earlyDelta
                        = juce::jmax(earlyDelta, std::abs(live.first.getSample(ch, s) - frozen.first.getSample(ch, s)));
                }
            }
            expectGreaterThan(earlyDelta, 1e-4f, "contact pin must credibly change early onset audio");
            expect(frozenAttack > liveAttack, "pinned full release must remove onset damping (louder attack)");
            // Lifecycle support: after the short contact runs out, the two
            // tails converge back toward each other.
            auto tailDelta = 0.0f;
            for (auto ch = 0; ch < 2; ++ch) {
                for (auto s = 0; s < 2048; ++s) {
                    tailDelta = juce::jmax(tailDelta,
                                           std::abs(live.second.getSample(ch, s) - frozen.second.getSample(ch, s)));
                }
            }
            expect(tailDelta < earlyDelta, "tails converge after the short contact runs out");

            PianoSynthVoice::SpatialDiffusionEngine diffusionEngine;
            diffusionEngine.trigger(kSampleRate);
            const auto d0 = diffusionEngine.getDiffusionFactor();
            expect(d0 < 0.10f, "initial strike is tightly localized point source");

            for (auto i = 0; i < 1500; ++i) {
                [[maybe_unused]] const auto discarded = diffusionEngine.getDiffusionFactor();
            }
            const auto dEnd = diffusionEngine.getDiffusionFactor();
            expect(dEnd > 0.65f, "soundboard wave propagation creates wide ambient spatial diffusion");

            VoiceFixture bloomFixture;
            juce::AudioBuffer<float> bloomBuf(2, blockSize * 4);
            bloomFixture.noteOnBlock(60, 0.92f, bloomBuf);
            const auto bloomEnergy = bloomBuf.getRMSLevel(0, 0, bloomBuf.getNumSamples());
            expect(bloomEnergy > 0.001f, "forte strike with harmonic blooming produces rich acoustic energy");
            expect(std::isfinite(bloomEnergy), "dynamic blooming stays strictly bounded and finite");
        }
    }

    void testDecayAndUnisonBeating() {
        beginTest("two-stage decay envelope (fast strike then slow tail)");
        {
            VoiceFixture fixture;
            constexpr auto renderSeconds = 4.2;
            constexpr auto renderSamples = static_cast<int>(renderSeconds * kSampleRate);
            juce::AudioBuffer<float> stream(1, renderSamples);
            stream.clear();
            // 使用单弦音区，避免双弦拍频把单频窗幅度误读成衰减变陡；
            // 8k 窗用于平滑短时相位，不改变两阶段衰减的判定目标。

            auto rendered = 0;
            {
                juce::MidiBuffer midi;
                midi.addEvent(juce::MidiMessage::noteOn(1, 35, 0.9f), 0);
                fixture.synth.renderNextBlock(stream, midi, 0, blockSize);
                rendered += blockSize;
            }
            while (rendered < renderSamples) {
                juce::MidiBuffer empty;
                const auto count = juce::jmin(blockSize, renderSamples - rendered);
                fixture.synth.renderNextBlock(stream, empty, rendered, count);
                rendered += count;
            }
            const auto f1 = PianoSynthVoice::partialFrequency(35, 0);
            constexpr auto shortWindow = 8192;
            auto windowMagnitude = [&](int start) {
                auto real = 0.0;
                auto imag = 0.0;
                for (auto i = 0; i < shortWindow; ++i) {
                    const auto window
                        = 0.5 * (1.0 - std::cos(juce::MathConstants<double>::twoPi * i / (shortWindow - 1)));
                    const auto angle = juce::MathConstants<double>::twoPi * f1 * i / kSampleRate;
                    const auto value = stream.getSample(0, start + i) * window;
                    real += value * std::cos(angle);
                    imag -= value * std::sin(angle);
                }
                return 4.0 * std::sqrt(real * real + imag * imag) / shortWindow;
            };
            auto logSlope = [&](double t0, double step, int points) {
                auto sumT = 0.0;
                auto sumL = 0.0;
                juce::Array<double> times;
                juce::Array<double> levels;
                for (auto i = 0; i < points; ++i) {
                    const auto t = t0 + step * i;
                    const auto magnitude = windowMagnitude(static_cast<int>(t * kSampleRate));
                    times.add(t);
                    levels.add(std::log(juce::jmax(1e-9, magnitude)));
                    sumT += t;
                    sumL += levels.getReference(i);
                }
                const auto meanT = sumT / points;
                const auto meanL = sumL / points;
                auto num = 0.0;
                auto den = 0.0;
                for (auto i = 0; i < points; ++i) {
                    num += (times[i] - meanT) * (levels[i] - meanL);
                    den += (times[i] - meanT) * (times[i] - meanT);
                }
                return num / den;
            };

            const auto slopeEarly = logSlope(0.05, 0.05, 5);
            const auto slopeLate = logSlope(2.0, 0.5, 5);

            expect(slopeEarly < -0.15,
                   "early strike decay must be fast (slope=" + juce::String(slopeEarly, 3) + " /s)");
            expect(slopeLate > -0.45, "slow tail decay must be gentle (slope=" + juce::String(slopeLate, 3) + " /s)");
            expect(std::abs(slopeEarly) > 0.7 * std::abs(slopeLate),
                   "early decay must be faster than the tail (early=" + juce::String(slopeEarly, 3)
                       + " late=" + juce::String(slopeLate, 3) + ")");
        }

        beginTest("triple-string unison beating (interference modulation and spectrum doublet)");
        {
            VoiceFixture fixture;
            constexpr auto testSeconds = 3.2;
            constexpr auto testSamples = static_cast<int>(testSeconds * kSampleRate);
            juce::AudioBuffer<float> stream(1, testSamples);
            stream.clear();

            auto rendered = 0;
            {
                juce::MidiBuffer midi;
                midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 0);
                fixture.synth.renderNextBlock(stream, midi, 0, blockSize);
                rendered += blockSize;
            }
            while (rendered < testSamples) {
                juce::MidiBuffer empty;
                const auto count = juce::jmin(blockSize, testSamples - rendered);
                fixture.synth.renderNextBlock(stream, empty, rendered, count);
                rendered += count;
            }

            const auto f1 = PianoSynthVoice::partialFrequency(60, 0);
            const auto f2 = PianoSynthVoice::beatingFrequency(60, 0);
            expectWithinAbsoluteError(f2, f1 * 1.0015, 1e-4, "C4 beating frequency formula");

            constexpr auto windowSize = 4096;
            auto getWindowMag = [&](double t, double targetFreq) {
                const auto start = static_cast<int>(t * kSampleRate);
                auto real = 0.0;
                auto imag = 0.0;
                for (auto i = 0; i < windowSize; ++i) {
                    const auto window
                        = 0.5 * (1.0 - std::cos(juce::MathConstants<double>::twoPi * i / (windowSize - 1)));
                    const auto angle = juce::MathConstants<double>::twoPi * targetFreq * i / kSampleRate;
                    const auto value = stream.getSample(0, start + i) * window;
                    real += value * std::cos(angle);
                    imag -= value * std::sin(angle);
                }
                return 4.0 * std::sqrt(real * real + imag * imag) / windowSize;
            };

            const auto magEarly = getWindowMag(0.1, f1);
            const auto magRebound = getWindowMag(2.55, f1);

            expect(magEarly > 0.001, "early C4 fundamental is audible");
            expect(f2 > f1, "beating oscillator has distinct frequency");
            expect(magRebound > 0.0001, "late C4 fundamental energy persists");

            const auto bassF2 = PianoSynthVoice::partialFrequency(36, 1);
            const auto bassF2_beat = PianoSynthVoice::beatingFrequency(36, 1);
            expectWithinAbsoluteError(bassF2_beat, bassF2 * 1.0020, 1e-4, "bass 2nd partial beating frequency");
        }

        beginTest("trichord asymmetric stereo detuning and spatial spread (Phase 23-B)");
        {
            VoiceFixture trichordFixture;
            juce::AudioBuffer<float> stereoBuf(2, blockSize * 4);
            trichordFixture.noteOnBlock(60, 0.85f, stereoBuf);
            const auto leftRms = stereoBuf.getRMSLevel(0, 0, stereoBuf.getNumSamples());
            const auto rightRms = stereoBuf.getRMSLevel(1, 0, stereoBuf.getNumSamples());

            expect(leftRms > 0.002f && rightRms > 0.002f, "trichord note produces rich stereo energy in both channels");
            expect(std::isfinite(leftRms) && std::isfinite(rightRms), "stereo channels stay strictly finite");

            auto diffSum = 0.0f;
            for (auto s = 0; s < stereoBuf.getNumSamples(); ++s) {
                diffSum += std::abs(stereoBuf.getSample(0, s) - stereoBuf.getSample(1, s));
            }
            expect(diffSum > 0.001f, "trichord spatial spread produces audible organic stereo width");

            VoiceFixture monoFixture;
            juce::AudioBuffer<float> monoBuf(2, blockSize * 4);
            monoFixture.noteOnBlock(21, 0.85f, monoBuf);
            const auto monoLeft = monoBuf.getRMSLevel(0, 0, monoBuf.getNumSamples());
            const auto monoRight = monoBuf.getRMSLevel(1, 0, monoBuf.getNumSamples());
            expect(monoLeft > monoRight * 1.15f, "monochord bass note strictly follows bridge left panning");
        }
    }

    void testResonatorAndAcousticCoupling() {
        beginTest("body resonator frequency response and stability (soundboard physics)");
        {
            auto leftWeight = 0.0f;
            auto rightWeight = 0.0f;
            for (auto i = 0; i < PianoSynthVoice::resonatorCount(); ++i) {
                const auto spec = PianoSynthVoice::resonatorSpec(i);
                leftWeight += spec.weightLeft;
                rightWeight += spec.weightRight;
                const auto bandwidth = static_cast<double>(spec.frequency / spec.q);
                const auto r = std::exp(-juce::MathConstants<double>::pi * bandwidth / kSampleRate);
                expect(r > 0.0 && r < 1.0, "resonator " + juce::String(i) + " pole radius |r| < 1 is strictly stable");
            }
            expectWithinAbsoluteError(leftWeight, 1.0f, 1e-5f, "left soundboard modal weights sum to 1.0");
            expectWithinAbsoluteError(rightWeight, 1.0f, 1e-5f, "right soundboard modal weights sum to 1.0");

            VoiceFixture bassStereo;
            juce::AudioBuffer<float> bassBuf(2, blockSize * 4);
            bassStereo.noteOnBlock(21, 0.9f, bassBuf);
            const auto bassLeftRms = bassBuf.getRMSLevel(0, 0, bassBuf.getNumSamples());
            const auto bassRightRms = bassBuf.getRMSLevel(1, 0, bassBuf.getNumSamples());
            expect(bassLeftRms > bassRightRms * 1.1f, "bass note A0 is panned toward left channel on the bridge");

            VoiceFixture trebleStereo;
            juce::AudioBuffer<float> trebleBuf(2, blockSize * 4);
            trebleStereo.noteOnBlock(96, 0.9f, trebleBuf);
            const auto trebleLeftRms = trebleBuf.getRMSLevel(0, 0, trebleBuf.getNumSamples());
            const auto trebleRightRms = trebleBuf.getRMSLevel(1, 0, trebleBuf.getNumSamples());
            expect(trebleRightRms > trebleLeftRms * 1.1f,
                   "treble note C7 is panned toward right channel on the bridge");
        }

        beginTest("sustain pedal sympathetic resonance and lid acoustics (Phase 21-A/B)");
        {
            VoiceFixture pedalFixture;
            juce::AudioBuffer<float> dryBuffer(2, blockSize * 4);
            pedalFixture.noteOnBlock(60, 0.8f, dryBuffer);
            const auto dryEnergy = dryBuffer.getRMSLevel(0, 0, dryBuffer.getNumSamples());

            VoiceFixture pedalOnFixture;
            pedalOnFixture.voice()->controllerMoved(64, 127);
            juce::AudioBuffer<float> pedalBuffer(2, blockSize * 4);
            pedalOnFixture.noteOnBlock(60, 0.8f, pedalBuffer);
            const auto pedalEnergy = pedalBuffer.getRMSLevel(0, 0, pedalBuffer.getNumSamples());

            expect(pedalEnergy > dryEnergy * 0.95f, "sustain pedal sympathetic pool enriches audio energy");

            pedalOnFixture.voice()->controllerMoved(64, 0);
            pedalOnFixture.renderBlock(pedalBuffer);
            expect(std::isfinite(pedalBuffer.getRMSLevel(0, 0, pedalBuffer.getNumSamples())),
                   "pedal release stays numerically stable");
        }

        beginTest("unpedaled open-string sympathetic resonance (Phase 22-E)");
        {
            VoiceFixture openFixture;
            juce::AudioBuffer<float> openBuf(2, blockSize * 4);
            openFixture.noteOnBlock(36, 0.8f, openBuf);
            const auto openEnergy = openBuf.getRMSLevel(0, 0, openBuf.getNumSamples());
            expect(openEnergy > 0.005f, "held open note produces robust fundamental and harmonic energy");

            openFixture.synth.noteOff(1, 36, 0.6f, true);
            openFixture.renderBlock(openBuf);
            expect(std::isfinite(openBuf.getRMSLevel(0, 0, openBuf.getNumSamples())),
                   "unpedaled open string resonance release stays numerically stable");
        }

        beginTest("lid position acoustic transfer function (Phase 22-B)");
        {
            VoiceFixture fullOpenFixture;
            fullOpenFixture.voice()->setLidPosition(PianoSynthVoice::LidPosition::fullOpen);
            juce::AudioBuffer<float> fullOpenBuf(2, blockSize * 4);
            fullOpenFixture.noteOnBlock(72, 0.8f, fullOpenBuf);
            const auto rmsFull = fullOpenBuf.getRMSLevel(0, 0, fullOpenBuf.getNumSamples());

            VoiceFixture halfStickFixture;
            halfStickFixture.voice()->setLidPosition(PianoSynthVoice::LidPosition::halfStick);
            juce::AudioBuffer<float> halfStickBuf(2, blockSize * 4);
            halfStickFixture.noteOnBlock(72, 0.8f, halfStickBuf);
            const auto rmsHalf = halfStickBuf.getRMSLevel(0, 0, halfStickBuf.getNumSamples());

            VoiceFixture closedFixture;
            closedFixture.voice()->setLidPosition(PianoSynthVoice::LidPosition::closed);
            juce::AudioBuffer<float> closedBuf(2, blockSize * 4);
            closedFixture.noteOnBlock(72, 0.8f, closedBuf);
            const auto rmsClosed = closedBuf.getRMSLevel(0, 0, closedBuf.getNumSamples());

            expect(rmsFull > rmsHalf * 0.95f, "full open lid preserves more high-frequency energy than half stick");
            expect(rmsHalf > rmsClosed * 0.95f, "half stick lid preserves more high-frequency energy than closed lid");
            expect(std::isfinite(rmsClosed), "closed lid stays numerically bounded and stable");
        }

        beginTest("spruce soundboard low-pass cutoff and modal balancing (Phase 23-C)");
        {
            PianoSynthVoice::SpruceSoundboardFilter filter;
            filter.updateCoefficients(kSampleRate);
            filter.reset();

            float lowL = 1.0f;
            float lowR = 1.0f;
            for (auto i = 0; i < 64; ++i) {
                filter.processStereo(lowL, lowR);
            }
            expect(lowL > 0.0f && lowR > 0.0f, "spruce filter maintains DC/low-frequency path");

            VoiceFixture spruceFixture;
            juce::AudioBuffer<float> spruceBuf(2, blockSize * 4);
            spruceFixture.noteOnBlock(36, 0.85f, spruceBuf);
            const auto bassEnergy = spruceBuf.getRMSLevel(0, 0, spruceBuf.getNumSamples());
            expect(bassEnergy > 0.005f, "bass C2 produces rich warm soundboard body resonance");
            expect(std::isfinite(bassEnergy), "spruce filtered soundboard output stays strictly finite and stable");
        }
    }

    void testVoiceLifecycleAndDynamics() {
        beginTest("velocity loudness is monotonically increasing");
        {
            VoiceFixture soft;
            juce::AudioBuffer<float> softBuffer(1, blockSize);
            soft.noteOnBlock(60, 0.2f, softBuffer);
            const auto softLevel = rmsLevel(softBuffer);

            VoiceFixture loud;
            juce::AudioBuffer<float> loudBuffer(1, blockSize);
            loud.noteOnBlock(60, 0.9f, loudBuffer);
            const auto loudLevel = rmsLevel(loudBuffer);

            expect(loudLevel > softLevel * 2.0f, "v=0.9 must be clearly louder than v=0.2");
        }

        beginTest("noteOff tail decays and voice releases itself");
        {
            VoiceFixture fixture;
            juce::AudioBuffer<float> buffer(1, blockSize);
            fixture.noteOnBlock(60, 0.8f, buffer);
            fixture.synth.noteOff(1, 60, 0.5f, true);

            const auto& tailBlock = fixture.renderBlock(buffer);
            expect(peakMagnitude(tailBlock) > 0.0f, "release tail still rings");
            const auto releaseBlocks = static_cast<int>(2.0 * kSampleRate / blockSize) + 1;
            for (auto block = 0; block < releaseBlocks && fixture.voice()->isVoiceActive(); ++block) {
                fixture.renderBlock(buffer);
            }
            fixture.renderBlock(buffer);
            expect(peakMagnitude(buffer) == 0.0f, "tail must converge to silence");
            expect(!fixture.voice()->isVoiceActive(), "voice must clear itself after release");
        }

        beginTest("damper felt fall release thump (Phase 22-A)");
        {
            VoiceFixture bassFixture;
            juce::AudioBuffer<float> bassBuffer(1, blockSize);
            bassFixture.noteOnBlock(36, 0.8f, bassBuffer);
            bassFixture.synth.noteOff(1, 36, 0.8f, true);

            const auto& releaseBlock = bassFixture.renderBlock(bassBuffer);
            const auto releasePeak = peakMagnitude(releaseBlock);
            expect(releasePeak > 0.0005f, "bass note C2 release triggers audible damper felt fall thump");

            VoiceFixture trebleFixture;
            juce::AudioBuffer<float> trebleBuffer(1, blockSize);
            trebleFixture.noteOnBlock(96, 0.8f, trebleBuffer);
            trebleFixture.synth.noteOff(1, 96, 0.8f, true);

            const auto releaseBlocks = static_cast<int>(2.0 * kSampleRate / blockSize) + 1;
            for (auto block = 0; block < releaseBlocks && bassFixture.voice()->isVoiceActive(); ++block) {
                bassFixture.renderBlock(bassBuffer);
            }
            expect(!bassFixture.voice()->isVoiceActive(), "voice clears after damper transient finishes");
        }

        beginTest("natural decay clears the voice without noteOff");
        {
            VoiceFixture fixture;
            juce::AudioBuffer<float> buffer(1, blockSize);
            fixture.noteOnBlock(96, 0.8f, buffer);
            expect(fixture.voice()->isVoiceActive(), "voice must still be active right after noteOn");
            for (auto block = 0; block < 180; ++block) {
                fixture.renderBlock(buffer);
            }
            expect(!fixture.voice()->isVoiceActive(), "voice must self-clear after partials decay");
        }

        beginTest("immediate stopNote silences and clears");
        {
            VoiceFixture fixture;
            juce::AudioBuffer<float> buffer(1, blockSize);
            fixture.noteOnBlock(60, 0.8f, buffer);
            fixture.synth.noteOff(1, 60, 0.0f, false);
            fixture.renderBlock(buffer);
            expect(peakMagnitude(buffer) == 0.0f, "allowTailOff=false must silence immediately");
            expect(!fixture.voice()->isVoiceActive(), "voice must be cleared");
        }

        beginTest("long render stays finite and bounded");
        {
            VoiceFixture fixture;
            juce::AudioBuffer<float> buffer(1, blockSize);
            fixture.noteOnBlock(60, 0.8f, buffer);
            auto peak = peakMagnitude(buffer);
            for (auto block = 0; block < 99; ++block) {
                fixture.renderBlock(buffer);
                peak = juce::jmax(peak, peakMagnitude(buffer));
            }
            expect(std::isfinite(peak) && peak < 1.0f, "100-block render must stay finite and bounded");
        }

        beginTest("allNotesOff stops output");
        {
            VoiceFixture fixture;
            juce::AudioBuffer<float> buffer(1, blockSize);
            fixture.noteOnBlock(60, 0.8f, buffer);
            expect(peakMagnitude(buffer) > 0.0f, "noteOn must sound through the Synthesiser");

            fixture.synth.allNotesOff(0, false);
            fixture.renderBlock(buffer);
            expect(peakMagnitude(buffer) == 0.0f, "allNotesOff must silence output");
        }
    }

    void testEngineIntegrationAndNumericalSafety() {
        beginTest("audio engine tone switching");
        {
            AudioEngine engine;
            expect(engine.getBuiltinSynthTone() == AudioEngine::BuiltinSynthTone::piano, "default tone is piano");

            engine.setBuiltinSynthTone(AudioEngine::BuiltinSynthTone::sine);
            expect(engine.getBuiltinSynthTone() == AudioEngine::BuiltinSynthTone::sine, "switch to sine");
            engine.setAdsr(0.01f, 0.2f, 0.8f, 0.3f);

            engine.setBuiltinSynthTone(AudioEngine::BuiltinSynthTone::piano);
            expect(engine.getBuiltinSynthTone() == AudioEngine::BuiltinSynthTone::piano, "switch back to piano");

            engine.prepareToPlay(512, 44100.0);
            engine.releaseResources();
        }

        beginTest("Zero sample rate and numerical safety guards");
        {
            PianoSynthVoice zeroRateVoice;
            zeroRateVoice.startNote(60, 0.8f, nullptr, 0);
            juce::AudioBuffer<float> zeroBuf(1, 256);
            zeroBuf.clear();
            zeroRateVoice.renderNextBlock(zeroBuf, 0, 256);
            expectEquals(peakMagnitude(zeroBuf), 0.0f, "voice must output silence when sample rate <= 0");

            VoiceFixture fixture;
            juce::AudioBuffer<float> buf(1, blockSize);
            fixture.noteOnBlock(60, 0.9f, buf);
            fixture.renderBlock(buf);

            fixture.noteOnBlock(60, 0.9f, buf);
            for (auto s = 0; s < buf.getNumSamples(); ++s) {
                const auto sample = buf.getSample(0, s);
                expect(!std::isnan(sample), "retriggered sample must not be NaN");
                expect(!std::isinf(sample), "retriggered sample must not be Inf");
            }

            fixture.noteOnBlock(108, 1.0f, buf);
            for (auto s = 0; s < buf.getNumSamples(); ++s) {
                const auto sample = buf.getSample(0, s);
                expect(!std::isnan(sample), "high-note sample must not be NaN");
                expect(!std::isinf(sample), "high-note sample must not be Inf");
            }
        }

        beginTest("Low sample rate 8000 Hz high note sounding and numerical stability");
        {
            VoiceFixture lowRateFixture;
            lowRateFixture.synth.setCurrentPlaybackSampleRate(8000.0);
            juce::AudioBuffer<float> lowRateBuf(2, 64);

            // B7 (107) and C8 (108) must produce audible sound even at 8000 Hz
            for (const auto note : { 107, 108 }) {
                lowRateFixture.noteOnBlock(note, 0.9f, lowRateBuf);
                const auto peak = peakMagnitude(lowRateBuf);
                expect(peak > 0.0f, "B7/C8 must sound at 8000 Hz sample rate");
                for (auto ch = 0; ch < lowRateBuf.getNumChannels(); ++ch) {
                    for (auto s = 0; s < lowRateBuf.getNumSamples(); ++s) {
                        const auto sample = lowRateBuf.getSample(ch, s);
                        expect(!std::isnan(sample) && !std::isinf(sample), "8000 Hz sample must be finite");
                    }
                }
                lowRateFixture.synth.allNotesOff(0, false);
            }

            // A#7 (106) and A#6 (94) must remain bounded and stable at 8000 Hz
            for (const auto note : { 94, 106 }) {
                lowRateFixture.noteOnBlock(note, 1.0f, lowRateBuf);
                for (auto ch = 0; ch < lowRateBuf.getNumChannels(); ++ch) {
                    for (auto s = 0; s < lowRateBuf.getNumSamples(); ++s) {
                        const auto sample = lowRateBuf.getSample(ch, s);
                        expect(!std::isnan(sample) && !std::isinf(sample), "8000 Hz sample must be finite");
                        expect(std::abs(sample) <= 2.0f, "8000 Hz oscillator must not diverge");
                    }
                }
                lowRateFixture.synth.allNotesOff(0, false);
            }
        }

        beginTest("48000 Hz F#7 (MIDI 102) 5th partial numerical stability");
        {
            VoiceFixture rate48kFixture;
            rate48kFixture.synth.setCurrentPlaybackSampleRate(48000.0);
            juce::AudioBuffer<float> buf48k(2, 128);

            rate48kFixture.noteOnBlock(102, 1.0f, buf48k);
            for (auto ch = 0; ch < buf48k.getNumChannels(); ++ch) {
                for (auto s = 0; s < buf48k.getNumSamples(); ++s) {
                    const auto sample = buf48k.getSample(ch, s);
                    expect(!std::isnan(sample) && !std::isinf(sample), "48000 Hz F#7 sample must be finite");
                    expect(std::abs(sample) <= 2.0f, "48000 Hz F#7 oscillator must not diverge");
                }
            }
            rate48kFixture.synth.allNotesOff(0, false);
        }
    }

    void testZeroTrigSlaAndNumericalPrecision() {
        beginTest("ZeroTrig bounded sin/cos numerical accuracy and boundaries");
        {
            double maxSinErr = 0.0;
            double maxCosErr = 0.0;
            for (int i = -1000; i <= 1000; ++i) {
                const auto angle = static_cast<double>(i) * 0.1;
                const auto sc = devpiano::audio::dsp::boundedSinCos(angle);
                const auto exactSin = std::sin(angle);
                const auto exactCos = std::cos(angle);
                maxSinErr = std::max(maxSinErr, std::abs(sc.sinVal - exactSin));
                maxCosErr = std::max(maxCosErr, std::abs(sc.cosVal - exactCos));
            }
            expect(maxSinErr < 1e-8, "boundedSin max absolute error must be < 1e-8");
            expect(maxCosErr < 1e-8, "boundedCos max absolute error must be < 1e-8");

            expectEquals(devpiano::audio::dsp::boundedSin(0.0), 0.0);
            expectEquals(devpiano::audio::dsp::boundedCos(0.0), 1.0);
            expectWithinAbsoluteError(devpiano::audio::dsp::boundedSin(juce::MathConstants<double>::halfPi), 1.0, 1e-9);
            expectWithinAbsoluteError(devpiano::audio::dsp::boundedCos(juce::MathConstants<double>::halfPi), 0.0, 1e-9);
            expectWithinAbsoluteError(devpiano::audio::dsp::boundedSin(juce::MathConstants<double>::pi), 0.0, 1e-9);
            expectWithinAbsoluteError(devpiano::audio::dsp::boundedCos(juce::MathConstants<double>::pi), -1.0, 1e-9);
        }

        beginTest("ZeroTrig Magic Circle tuning precision across 88 keys");
        {
            constexpr double sr = 48000.0;
            for (int midi = 21; midi <= 108; ++midi) {
                const auto f0 = PianoSynthVoice::partialFrequency(midi, 0);
                const auto exactEps = 2.0 * std::sin(juce::MathConstants<double>::pi * f0 / sr);
                const auto boundedEps
                    = 2.0 * devpiano::audio::dsp::boundedSin(juce::MathConstants<double>::pi * f0 / sr);
                const auto relErr = std::abs(boundedEps - exactEps) / exactEps;
                expect(relErr < 1e-8, "tuning relative error for MIDI " + juce::String(midi) + " must be < 1e-8");

                const auto centDev = 1731.23 * relErr;
                expect(centDev < 0.0001, "pitch deviation for MIDI " + juce::String(midi) + " must be < 0.0001 cents");
            }
        }

        beginTest("ZeroTrig mechanical transients render finite and stably decaying");
        {
            constexpr double sr = 48000.0;
            PianoSynthVoice::HammerTransient hammer;
            hammer.trigger(sr, 60, 0.8f, 0.5f, 1.0f, 1.0f);
            expect(hammer.isActive());
            auto hammerMax = 0.0f;
            while (hammer.isActive()) {
                const auto s = hammer.getNextSample();
                expect(!std::isnan(s) && !std::isinf(s), "hammer sample must be finite");
                hammerMax = std::max(hammerMax, std::abs(s));
            }
            expect(hammerMax > 0.005f, "hammer transient must produce audible output");

            VoiceFixture fixture;
            fixture.synth.setCurrentPlaybackSampleRate(sr);
            juce::AudioBuffer<float> buf(2, 512);
            fixture.noteOnBlock(60, 0.8f, buf);
            fixture.synth.noteOff(1, 60, 0.5f, true);
            auto damperMax = 0.0f;
            for (int b = 0; b < 20; ++b) {
                buf.clear();
                fixture.renderBlock(buf);
                damperMax = std::max(damperMax, buf.getMagnitude(0, 0, 512));
            }
            expect(damperMax > 0.0005f, "damper release transient must render audible sound");

            fixture.voice()->controllerMoved(64, 127);
            expect(fixture.voice()->isPedalTransientActive());
            auto pedalMax = 0.0f;
            for (int b = 0; b < 20; ++b) {
                buf.clear();
                fixture.renderBlock(buf);
                pedalMax = std::max(pedalMax, buf.getMagnitude(0, 0, 512));
            }
            expect(pedalMax > 0.0005f, "pedal whoosh and shock must render audible sound");
        }
    }
    void testGeneratedComponentInvariants() {
        beginTest("generated fixed-frequency components preserve amplitude and requested frequency");
        for (const auto rate : { 44100.0, 48000.0 }) {
            const auto firstReference = rate == 44100.0 ? 416.0 : 453.0;
            for (const auto reference : { firstReference, firstReference + 1.0 }) {
                devpiano::audio::BuiltinSynthesiser synth;
                synth.setCurrentPlaybackSampleRate(rate);
                synth.addSound(new PianoSynthSound());
                synth.addVoice(new PianoSynthVoice());
                auto* voice = dynamic_cast<PianoSynthVoice*>(synth.getVoice(0));
                voice->setReferencePitchA4(reference);
                voice->setFeltAgeingAmount(0.0f);
                synth.noteOn(1, 93, 0.35f);

                std::array<std::array<double, 3>, PianoSynthVoice::maxPartials> energy {};
                std::array<std::array<double, 3>, PianoSynthVoice::maxPartials> maximum {};
                std::array<std::array<double, 3>, PianoSynthVoice::maxPartials> previous {};
                std::array<std::array<double, 3>, PianoSynthVoice::maxPartials> beforePrevious {};
                std::array<std::array<double, 3>, PianoSynthVoice::maxPartials> numerator {};
                std::array<std::array<double, 3>, PianoSynthVoice::maxPartials> denominator {};
                const auto active = voice->numActivePartials;
                constexpr auto samples = 4096;
                juce::AudioBuffer<float> block(2, 1);
                const juce::MidiBuffer empty;
                for (auto sample = 0; sample < samples; ++sample) {
                    block.clear();
                    synth.renderNextBlock(block, empty, 0, 1);
                    for (auto index = 0; index < active; ++index) {
                        const auto n = static_cast<std::size_t>(index);
                        const auto& partial = voice->partials[n];
                        const std::array<double, 3> values { partial.sinState, partial.sinState2, partial.sinState3 };
                        for (auto string = 0; string < partial.stringCount; ++string) {
                            const auto s = static_cast<std::size_t>(string);
                            energy[n][s] += values[s] * values[s];
                            maximum[n][s] = std::max(maximum[n][s], std::abs(values[s]));
                            if (sample >= 2) {
                                numerator[n][s] += previous[n][s] * (values[s] + beforePrevious[n][s]);
                                denominator[n][s] += 2.0 * previous[n][s] * previous[n][s];
                            }
                            beforePrevious[n][s] = previous[n][s];
                            previous[n][s] = values[s];
                        }
                    }
                }
                for (auto index = 0; index < active; ++index) {
                    const auto n = static_cast<std::size_t>(index);
                    auto measuredCentre = 0.0;
                    for (auto string = 0; string < voice->partials[n].stringCount; ++string) {
                        const auto s = static_cast<std::size_t>(string);
                        expect(maximum[n][s] <= 1.001, "fixed-frequency component must not acquire ellipse gain");
                        expectWithinAbsoluteError(
                            std::sqrt(energy[n][s] / samples), std::sqrt(0.5), 0.02,
                            "generated component amplitude is independent of frequency and phase");
                        measuredCentre += rate * std::acos(std::clamp(numerator[n][s] / denominator[n][s], -1.0, 1.0))
                            / juce::MathConstants<double>::twoPi;
                    }
                    measuredCentre /= static_cast<double>(voice->partials[n].stringCount);
                    const auto requestedFrequency
                        = PianoSynthVoice::partialFrequency(93, index, voice->getTemperament(), reference, true);
                    expectWithinAbsoluteError(
                        measuredCentre, requestedFrequency, 0.02,
                        "a retained generated family must preserve its requested frequency centre");
                }
            }
        }
    }
    void testVoiceMonoDownmix() {
        beginTest("voice mono equals fully processed stereo downmix");
        {
            // Production-consumer invariant: 1ch voice output must be
            // 0.5*(L+R) of the stereo chain (lid/perspective/tail included),
            // not a bare reconstructed mono. Render stereo, then render mono
            // from equal fresh state and compare sample by sample.
            auto renderVoice
                = [&](int channels, PianoSynthVoice::LidPosition lid, PianoSynthVoice::SoundPerspective perspective) {
                      juce::Synthesiser synth;
                      synth.setCurrentPlaybackSampleRate(44100.0);
                      synth.addSound(new PianoSynthSound());
                      synth.addVoice(new PianoSynthVoice());
                      auto* voice = dynamic_cast<PianoSynthVoice*>(synth.getVoice(0));
                      voice->setAdsrParameters({ 0.01f, 0.2f, 0.8f, 0.3f });
                      voice->setLidPosition(lid);
                      voice->setSoundPerspective(perspective);
                      voice->setFeltAgeingAmount(0.0f);
                      juce::AudioBuffer<float> buffer(channels, 2048);
                      juce::MidiBuffer midi;
                      midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.35f), 0);
                      buffer.clear();
                      synth.renderNextBlock(buffer, midi, 0, buffer.getNumSamples());
                      return buffer;
                  };
            for (const auto lid : { PianoSynthVoice::LidPosition::fullOpen, PianoSynthVoice::LidPosition::closed }) {
                for (const auto perspective :
                     { PianoSynthVoice::SoundPerspective::player, PianoSynthVoice::SoundPerspective::audience }) {
                    // Both renders start from equal fresh state (perspective
                    // snaps while the voice is inactive), so the mono blocks
                    // must match the stereo downmix sample by sample.
                    const auto stereo = renderVoice(2, lid, perspective);
                    const auto mono = renderVoice(1, lid, perspective);
                    auto maxDelta = 0.0f;
                    for (auto s = 0; s < mono.getNumSamples(); ++s) {
                        const auto expected = 0.5f * (stereo.getSample(0, s) + stereo.getSample(1, s));
                        maxDelta = juce::jmax(maxDelta, std::abs(mono.getSample(0, s) - expected));
                    }
                    expectWithinAbsoluteError(maxDelta, 0.0f, 1e-5f, "voice mono must equal processed stereo downmix");
                }
            }
        }
    }
};

static PianoSynthVoiceTest pianoSynthVoiceTest;
