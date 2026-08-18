#include <JuceHeader.h>

#include "Audio/AudioEngine.h"
#include "Audio/PianoSynthVoice.h"

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
//   - Fundamental and harmonics present (single-bin DFT; bass region
//     checks harmonics 2..7, mid region checks 2..5)
//   - Velocity 0.2 vs 0.9 loudness is monotonically increasing
//   - noteOff tail decays and the voice releases itself
//   - Immediate stopNote (allowTailOff=false) silences and clears the voice
//   - Long renders stay finite (no NaN/Inf/explosion) — heap-allocation-free
//     path exercised without crashing
//   - allNotesOff stops output
// =============================================================================

namespace {
constexpr auto sampleRate = 44100.0;
constexpr auto blockSize = 2048;
constexpr auto analysisWindow = 16384; // ≈ 0.37 s for the DFT

struct VoiceFixture {
    juce::Synthesiser synth;

    VoiceFixture() {
        synth.setCurrentPlaybackSampleRate(sampleRate);
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
        const auto angle = juce::MathConstants<double>::twoPi * frequency * i / sampleRate;
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
        beginTest("region table boundaries");
        {
            expectEquals(PianoSynthVoice::partialCountForNote(0), 8, "bottom note (C-1) keeps 7 harmonics");
            expectEquals(PianoSynthVoice::partialCountForNote(47), 8, "B3 still low-bass region");
            expectEquals(PianoSynthVoice::partialCountForNote(48), 6, "C4 mid region: 5 harmonics");
            expectEquals(PianoSynthVoice::partialCountForNote(71), 6, "B4 still mid region");
            expectEquals(PianoSynthVoice::partialCountForNote(72), 4, "C5 high-mid: 3 harmonics");
            expectEquals(PianoSynthVoice::partialCountForNote(95), 4, "B6 still high-mid");
            expectEquals(PianoSynthVoice::partialCountForNote(96), 3, "C7 treble converges to 2 harmonics");
            expectEquals(PianoSynthVoice::partialCountForNote(127), 3, "top note stays treble region");

            expectWithinAbsoluteError(PianoSynthVoice::decaySecondsForNote(0), 4.0f, 0.001f, "bass decay is long");
            expectWithinAbsoluteError(PianoSynthVoice::decaySecondsForNote(60), 2.5f, 0.001f, "mid decay");
            expectWithinAbsoluteError(PianoSynthVoice::decaySecondsForNote(80), 1.5f, 0.001f, "high-mid decay");
            expectWithinAbsoluteError(PianoSynthVoice::decaySecondsForNote(100), 0.8f, 0.001f, "treble decay is short");

            expectWithinAbsoluteError(PianoSynthVoice::inharmonicityBForNote(0), 4.0e-4, 1e-7, "bass B is large");
            expectWithinAbsoluteError(PianoSynthVoice::inharmonicityBForNote(60), 1.0e-4, 1e-7, "mid B");
            expectWithinAbsoluteError(PianoSynthVoice::inharmonicityBForNote(80), 3.0e-5, 1e-7, "high-mid B");
            expectWithinAbsoluteError(PianoSynthVoice::inharmonicityBForNote(100), 1.0e-5, 1e-7, "treble B is small");

            expectWithinAbsoluteError(PianoSynthVoice::decayDampingCForNote(0), 0.35f, 0.001f,
                                      "bass damping slope is high");
            expectWithinAbsoluteError(PianoSynthVoice::decayDampingCForNote(60), 0.25f, 0.001f, "mid damping slope");
            expectWithinAbsoluteError(PianoSynthVoice::decayDampingCForNote(80), 0.18f, 0.001f,
                                      "high-mid damping slope");
            expectWithinAbsoluteError(PianoSynthVoice::decayDampingCForNote(100), 0.12f, 0.001f,
                                      "treble damping slope");
        }

        beginTest("renders non-zero finite output at normalised level");
        {
            VoiceFixture fixture;
            juce::AudioBuffer<float> buffer(1, blockSize);
            fixture.noteOnBlock(60, 0.8f, buffer);
            const auto peak = peakMagnitude(buffer);
            expect(peak > 0.05f, "note must produce audible output");
            expect(peak < 1.0f, "normalised output must not clip");

            auto finite = true;
            for (auto sample = 0; sample < buffer.getNumSamples(); ++sample) {
                finite = finite && std::isfinite(buffer.getSample(0, sample));
            }
            expect(finite, "render output must stay finite (no NaN/Inf)");
        }

        beginTest("fundamental and harmonics present (single-bin DFT)");
        {
            VoiceFixture bassFixture;
            juce::AudioBuffer<float> bass(1, analysisWindow);
            bassFixture.noteOnBlock(36, 0.9f, bass); // low-bass region: 7 harmonics (C2 ≈ 65.41 Hz)
            const auto bassFundamental
                = magnitudeAtFrequency(bass, PianoSynthVoice::partialFrequency(36, 0), analysisWindow);
            expect(bassFundamental > 0.01, "low-bass fundamental must be present");
            for (auto harmonic = 2; harmonic <= 7; ++harmonic) {
                const auto partialFreq = PianoSynthVoice::partialFrequency(36, harmonic - 1);
                const auto magnitude = magnitudeAtFrequency(bass, partialFreq, analysisWindow);
                expect(magnitude > 0.05 * bassFundamental,
                       "low-bass harmonic " + juce::String(harmonic) + " must be present (mag="
                           + juce::String(magnitude, 5) + " base=" + juce::String(bassFundamental, 5) + ")");
            }

            VoiceFixture midFixture;
            juce::AudioBuffer<float> mid(1, analysisWindow);
            midFixture.noteOnBlock(60, 0.9f, mid);
            const auto midFundamental
                = magnitudeAtFrequency(mid, PianoSynthVoice::partialFrequency(60, 0), analysisWindow);
            expect(midFundamental > 0.03, "MIDI 60 fundamental ~ 261.63 Hz must dominate");
            for (auto harmonic = 2; harmonic <= 5; ++harmonic) {
                const auto partialFreq = PianoSynthVoice::partialFrequency(60, harmonic - 1);
                const auto magnitude = magnitudeAtFrequency(mid, partialFreq, analysisWindow);
                expect(magnitude > 0.05 * midFundamental,
                       "mid harmonic " + juce::String(harmonic) + " must be present");
            }
        }

        beginTest("inharmonicity overtone frequency shift (stiff-string physics)");
        {
            // 低音 C2 (note 36 ≈ 65.406 Hz, B = 4e-4) 的高次分音频偏量化验证：
            const auto f0 = static_cast<double>(juce::MidiMessage::getMidiNoteInHertz(36));
            const auto b = PianoSynthVoice::inharmonicityBForNote(36);
            expectWithinAbsoluteError(b, 4.0e-4, 1e-7, "note 36 B coefficient");

            // 验证分音频率计算与物理公式一致：f_m = m·f0·√(1 + B·m^2)
            // 第 5 分音：m=5, √(1 + 25 * 4e-4) = √1.01 ≈ 1.0049875, 偏移 +0.50%
            const auto expectedF5 = 5.0 * f0 * std::sqrt(1.0 + 25.0 * b);
            const auto actualF5 = PianoSynthVoice::partialFrequency(36, 4);
            expectWithinAbsoluteError(actualF5, expectedF5, 1e-4, "5th partial frequency formula");
            expect(actualF5 > 5.0 * f0 + 1.0, "5th partial is shifted up by > 1 Hz (stiff string)");

            // 第 7 分音：m=7, √(1 + 49 * 4e-4) = √1.0196 ≈ 1.009752, 偏移 +0.975%
            const auto expectedF7 = 7.0 * f0 * std::sqrt(1.0 + 49.0 * b);
            const auto actualF7 = PianoSynthVoice::partialFrequency(36, 6);
            expectWithinAbsoluteError(actualF7, expectedF7, 1e-4, "7th partial frequency formula");
            expect(actualF7 > 7.0 * f0 + 4.0, "7th partial is shifted up by > 4 Hz in bass region");

            // 频谱实测：在合成器实际渲染输出中，DFT 在非谐频率处的能量显著高于整数倍谐波处
            VoiceFixture fixture;
            juce::AudioBuffer<float> buffer(1, analysisWindow);
            fixture.noteOnBlock(36, 0.9f, buffer);

            const auto magAtInharmonic7 = magnitudeAtFrequency(buffer, actualF7, analysisWindow);
            const auto magAtInteger7 = magnitudeAtFrequency(buffer, 7.0 * f0, analysisWindow);
            expect(magAtInharmonic7 > 1.5 * magAtInteger7,
                   "DFT energy at stiff-string 7th partial (" + juce::String(actualF7, 2)
                       + " Hz) must be significantly higher than integer harmonic (" + juce::String(7.0 * f0, 2)
                       + " Hz)");

            const auto magAtInharmonic5 = magnitudeAtFrequency(buffer, actualF5, analysisWindow);
            const auto magAtInteger5 = magnitudeAtFrequency(buffer, 5.0 * f0, analysisWindow);
            expect(magAtInharmonic5 > 1.2 * magAtInteger5,
                   "DFT energy at stiff-string 5th partial (" + juce::String(actualF5, 2)
                       + " Hz) must be higher than integer harmonic (" + juce::String(5.0 * f0, 2) + " Hz)");
        }
        beginTest("modal overtone decay rates (physical energy dissipation)");
        {
            // 验证时间常数物理公式：τ_m = τ_base / (1 + c_eff * (m - 1))
            const auto tau1 = PianoSynthVoice::partialDecaySeconds(36, 0);
            expectWithinAbsoluteError(tau1, 4.0, 1e-3, "fundamental decay equals base decay");

            const auto tau6
                = PianoSynthVoice::partialDecaySeconds(36, 5); // m=6, 4.0 / (1 + 0.35 * 5) = 4.0 / 2.75 ≈ 1.4545s
            expectWithinAbsoluteError(tau6, 4.0 / (1.0 + 0.35 * 5.0), 1e-3, "6th partial modal decay formula");
            expect(tau6 < tau1 * 0.4, "6th partial decays more than 2.5x faster than fundamental");

            const auto tau8
                = PianoSynthVoice::partialDecaySeconds(36, 7); // m=8, 4.0 / (1 + 0.35 * 7) = 4.0 / 3.45 ≈ 1.1594s
            expectWithinAbsoluteError(tau8, 4.0 / (1.0 + 0.35 * 7.0), 1e-3, "8th partial modal decay formula");
            expect(tau8 < tau6, "overtone decay times are strictly monotonically decreasing");

            // 动态时域 / 频域验证（Phase 13-5 模态衰减对比断言）：
            // 在低音 note 36（C2）按键后，对比早期 t0 与后期 t1 的第 6 分音 / 基频幅度比
            VoiceFixture fixture;
            juce::AudioBuffer<float> earlyBuffer(1, analysisWindow);
            fixture.noteOnBlock(36, 0.9f, earlyBuffer); // 0 ~ 0.37s

            const auto f1 = PianoSynthVoice::partialFrequency(36, 0);
            const auto f6 = PianoSynthVoice::partialFrequency(36, 5);

            const auto earlyF1 = magnitudeAtFrequency(earlyBuffer, f1, analysisWindow);
            const auto earlyF6 = magnitudeAtFrequency(earlyBuffer, f6, analysisWindow);
            const auto earlyRatio = earlyF6 / juce::jmax(1e-6, earlyF1);
            expect(earlyRatio > 0.05, "6th partial is present in the early strike window");

            // 推进到约 2.0s 处（再渲染 5 个 analysisWindow，中心点 t ≈ 2.0s）
            juce::AudioBuffer<float> lateBuffer(1, analysisWindow);
            for (auto step = 0; step < 5; ++step) {
                fixture.renderBlock(lateBuffer);
            }
            const auto lateF1 = magnitudeAtFrequency(lateBuffer, f1, analysisWindow);
            const auto lateF6 = magnitudeAtFrequency(lateBuffer, f6, analysisWindow);
            const auto lateRatio = lateF6 / juce::jmax(1e-6, lateF1);

            // 高次分音衰减远快于基频，后期分音比显著下降（Ratio(t1) < 0.5 * Ratio(t0)）
            expect(lateRatio < 0.5 * earlyRatio,
                   "upper harmonic ratio at t1 (" + juce::String(lateRatio, 5) + ") must drop below 50% of t0 ratio ("
                       + juce::String(earlyRatio, 5) + ") due to modal energy dissipation");
        }

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
            fixture.synth.noteOff(1, 60, 0.5f, true); // allow tail-off

            const auto& tailBlock = fixture.renderBlock(buffer);
            expect(peakMagnitude(tailBlock) > 0.0f, "release tail still rings");
            // release = 0.3 s → 渲染 1 s 后包络归零并自清。
            for (auto block = 0; block < 19; ++block) {
                fixture.renderBlock(buffer);
            }
            expect(peakMagnitude(buffer) == 0.0f, "tail must converge to silence");
            expect(!fixture.voice()->isVoiceActive(), "voice must clear itself after release");
        }

        beginTest("natural decay clears the voice without noteOff");
        {
            VoiceFixture fixture;
            juce::AudioBuffer<float> buffer(1, blockSize);
            fixture.noteOnBlock(96, 0.8f, buffer); // treble: short decay ≈ 0.8 s
            expect(fixture.voice()->isVoiceActive(), "voice must still be active right after noteOn");
            // 渲染 ~8 s（3 分音，约 1M 次 sin）足够衰减到 silentLevelThreshold 以下。
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
            fixture.synth.noteOff(1, 60, 0.0f, false); // no tail-off
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

        beginTest("audio engine tone switching");
        {
            AudioEngine engine;
            expect(engine.getBuiltinSynthTone() == AudioEngine::BuiltinSynthTone::sine, "default tone is sine");

            engine.setBuiltinSynthTone(AudioEngine::BuiltinSynthTone::piano);
            expect(engine.getBuiltinSynthTone() == AudioEngine::BuiltinSynthTone::piano, "switch to piano");
            engine.setAdsr(0.01f, 0.2f, 0.8f, 0.3f); // 对新 voice 的 ADSR 接线不崩溃

            engine.setBuiltinSynthTone(AudioEngine::BuiltinSynthTone::sine);
            expect(engine.getBuiltinSynthTone() == AudioEngine::BuiltinSynthTone::sine, "switch back to sine");

            engine.prepareToPlay(512, 44100.0); // 重建后 prepare 不崩溃
            engine.releaseResources();
        }

        beginTest("piano parameters shape the tone");
        {
            const auto midF4 = PianoSynthVoice::partialFrequency(60, 3);
            const auto midF5 = PianoSynthVoice::partialFrequency(60, 4);
            const auto midF1 = PianoSynthVoice::partialFrequency(60, 0);

            // 高 brightness → 高次谐波相对幅度更大（upper-harmonic ratio 提升）。
            VoiceFixture dim;
            dim.voice()->setPianoParameters(0.0f, 0.5f, 0.5f);
            juce::AudioBuffer<float> dimBuffer(1, analysisWindow);
            dim.noteOnBlock(60, 1.0f, dimBuffer);
            const auto dimFourth = magnitudeAtFrequency(dimBuffer, midF4, analysisWindow)
                / juce::jmax(1e-6, magnitudeAtFrequency(dimBuffer, midF1, analysisWindow));

            VoiceFixture bright;
            bright.voice()->setPianoParameters(1.0f, 0.5f, 0.5f);
            juce::AudioBuffer<float> brightBuffer(1, analysisWindow);
            bright.noteOnBlock(60, 1.0f, brightBuffer);
            const auto brightFourth = magnitudeAtFrequency(brightBuffer, midF4, analysisWindow)
                / juce::jmax(1e-6, magnitudeAtFrequency(brightBuffer, midF1, analysisWindow));

            expect(brightFourth > dimFourth, "higher brightness must boost the upper-harmonic ratio");

            // 高 hammerHardness → 最高次谐波相对幅度更大。
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
            // 高 resonance → 衰减更慢（长时窗口 RMS 更强）。
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
};

static PianoSynthVoiceTest pianoSynthVoiceTest;
