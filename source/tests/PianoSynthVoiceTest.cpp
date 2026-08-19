#include <JuceHeader.h>

#include <complex>

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
//   - Magic Circle recursive oscillator long-term frequency stability
//     (dual-window complex DFT phase-difference over 20 s, drift < 1e-4)
//   - Two-stage decay envelope (early strike slope > 2x late tail slope)
//   - Triple-string unison beating (interference modulation dip and rebound)
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
            expectEquals(PianoSynthVoice::partialCountForNote(0), 20, "bottom note (C-1) keeps 19 harmonics");
            expectEquals(PianoSynthVoice::partialCountForNote(47), 20, "B3 still low-bass region");
            expectEquals(PianoSynthVoice::partialCountForNote(48), 14, "C4 mid region: 13 harmonics");
            expectEquals(PianoSynthVoice::partialCountForNote(71), 14, "B4 still mid region");
            expectEquals(PianoSynthVoice::partialCountForNote(72), 8, "C5 high-mid: 7 harmonics");
            expectEquals(PianoSynthVoice::partialCountForNote(95), 8, "B6 still high-mid");
            expectEquals(PianoSynthVoice::partialCountForNote(96), 6, "C7 treble region: 5 harmonics");
            expectEquals(PianoSynthVoice::partialCountForNote(127), 6, "top note stays treble region");

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

            expectWithinAbsoluteError(PianoSynthVoice::fastDecayRatioForNote(0), 0.15f, 0.001f, "bass fast ratio");
            expectWithinAbsoluteError(PianoSynthVoice::fastDecayRatioForNote(60), 0.20f, 0.001f, "mid fast ratio");
            expectWithinAbsoluteError(PianoSynthVoice::fastDecayRatioForNote(100), 0.15f, 0.001f, "treble fast ratio");
            expectWithinAbsoluteError(PianoSynthVoice::slowWeightForNote(0), 0.30f, 0.001f, "bass slow weight");
            expectWithinAbsoluteError(PianoSynthVoice::slowWeightForNote(60), 0.25f, 0.001f, "mid slow weight");
            expectWithinAbsoluteError(PianoSynthVoice::slowWeightForNote(100), 0.15f, 0.001f, "treble slow weight");

            expectWithinAbsoluteError(PianoSynthVoice::beatingDetuneRatioForNote(0), 0.0020f, 1e-5f,
                                      "bass beating ratio");
            expectWithinAbsoluteError(PianoSynthVoice::beatingDetuneRatioForNote(60), 0.0015f, 1e-5f,
                                      "mid beating ratio");
            expectWithinAbsoluteError(PianoSynthVoice::beatingDetuneRatioForNote(80), 0.0010f, 1e-5f, "high-mid ratio");
            expectWithinAbsoluteError(PianoSynthVoice::beatingDetuneRatioForNote(100), 0.0f, 1e-5f, "treble beating 0");
            expectEquals(PianoSynthVoice::beatingPartialCountForNote(0), 6, "bass 6 beating partials");
            expectEquals(PianoSynthVoice::beatingPartialCountForNote(60), 6, "mid 6 beating partials");
            expectEquals(PianoSynthVoice::beatingPartialCountForNote(80), 4, "high-mid 4 beating partials");
            expectEquals(PianoSynthVoice::beatingPartialCountForNote(100), 0, "treble 0 beating partials");

            // 低音基频不设第二弦（锁定音高），第 2 分音及中音分音设第二振荡器
            expectEquals(PianoSynthVoice::beatingFrequency(36, 0), PianoSynthVoice::partialFrequency(36, 0),
                         "bass fundamental stays single oscillator");
            expectWithinAbsoluteError(PianoSynthVoice::beatingFrequency(36, 1),
                                      PianoSynthVoice::partialFrequency(36, 1) * 1.0020, 1e-4,
                                      "bass 2nd partial has beating doublet");
            expectWithinAbsoluteError(PianoSynthVoice::beatingFrequency(60, 0),
                                      PianoSynthVoice::partialFrequency(60, 0) * 1.0015, 1e-4,
                                      "mid fundamental has beating doublet");

            // τ_fast = τ_slow × ratio 的解析公式（低音 m=6：τ_slow ≈ 1.4545 s → τ_fast ≈ 0.218 s）。
            expectWithinAbsoluteError(PianoSynthVoice::partialFastDecaySeconds(36, 5), 4.0 / (1.0 + 0.35 * 5.0) * 0.15,
                                      1e-3, "6th partial fast decay formula");

            expectWithinAbsoluteError(PianoSynthVoice::bodyWet(), 0.25f, 0.001f, "25% body wet ratio");
            expectEquals(PianoSynthVoice::resonatorCount(), 3, "3 body resonators");
            expectWithinAbsoluteError(PianoSynthVoice::resonatorSpec(0).frequency, 110.0f, 0.1f, "peak 1 freq");
            expectWithinAbsoluteError(PianoSynthVoice::resonatorSpec(1).frequency, 220.0f, 0.1f, "peak 2 freq");
            expectWithinAbsoluteError(PianoSynthVoice::resonatorSpec(2).frequency, 360.0f, 0.1f, "peak 3 freq");
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
            // 低音区（Phase 14-A 后 20 分音）：全部高次分音在非谐频率处可测。
            VoiceFixture bassFixture;
            juce::AudioBuffer<float> bass(1, analysisWindow);
            bassFixture.noteOnBlock(36, 0.9f, bass); // low-bass region: 20 partials (C2 ≈ 65.41 Hz)
            const auto bassFundamental
                = magnitudeAtFrequency(bass, PianoSynthVoice::partialFrequency(36, 0), analysisWindow);
            expect(bassFundamental > 0.01, "low-bass fundamental must be present");
            for (auto harmonic = 2; harmonic <= 20; ++harmonic) {
                const auto partialFreq = PianoSynthVoice::partialFrequency(36, harmonic - 1);
                const auto magnitude = magnitudeAtFrequency(bass, partialFreq, analysisWindow);
                expect(magnitude > 0.025 * bassFundamental,
                       "low-bass harmonic " + juce::String(harmonic) + " must be present (mag="
                           + juce::String(magnitude, 5) + " base=" + juce::String(bassFundamental, 5) + ")");
            }

            // 中音区（14 分音）。
            VoiceFixture midFixture;
            juce::AudioBuffer<float> mid(1, analysisWindow);
            midFixture.noteOnBlock(60, 0.9f, mid);
            const auto midFundamental
                = magnitudeAtFrequency(mid, PianoSynthVoice::partialFrequency(60, 0), analysisWindow);
            expect(midFundamental > 0.02, "MIDI 60 fundamental ~ 261.63 Hz must dominate");
            for (auto harmonic = 2; harmonic <= 14; ++harmonic) {
                const auto partialFreq = PianoSynthVoice::partialFrequency(60, harmonic - 1);
                const auto magnitude = magnitudeAtFrequency(mid, partialFreq, analysisWindow);
                expect(magnitude > 0.03 * midFundamental,
                       "mid harmonic " + juce::String(harmonic) + " must be present");
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
                expect(magnitude > 0.03 * highFundamental,
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
                expect(magnitude > 0.03 * topFundamental,
                       "treble harmonic " + juce::String(harmonic) + " must be present");
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

        beginTest("recursive oscillator frequency stability (Magic Circle long render)");
        {
            // Phase 14-A：双窗复 DFT 相位差法测量长时频偏。
            // 渲染 20 s 低音 C2（resonance=1 → τ_slow = 5.2 s，双阶段衰减后 voice
            // 自清时间 ≈ 24 s，20 s 处慢分量仍有 ≥ 2e-4 幅度、voice 活跃），取两段
            // 16384 样本对称 Hann 窗单点 DFT，arg(X2) - arg(X1) = 2π·δf·ΔT 直接
            // 给出频率漂移（相位分辨率 ≈1e-8 相对，远优于 1e-4 断言阈值）。
            VoiceFixture fixture;
            fixture.voice()->setPianoParameters(0.5f, 0.5f, 1.0f);
            constexpr auto totalSeconds = 20.0;
            constexpr auto totalSamples = static_cast<int>(totalSeconds * sampleRate); // 882000
            juce::AudioBuffer<float> stream(1, totalSamples);
            stream.clear();

            auto rendered = 0;
            {
                juce::MidiBuffer midi;
                midi.addEvent(juce::MidiMessage::noteOn(1, 36, 0.9f), 0);
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

            const auto f0 = PianoSynthVoice::partialFrequency(36, 0);
            auto complexDft = [](const juce::AudioBuffer<float>& buffer, int start, int count, double frequency) {
                auto real = 0.0;
                auto imag = 0.0;
                for (auto i = 0; i < count; ++i) {
                    const auto window = 0.5 * (1.0 - std::cos(juce::MathConstants<double>::twoPi * i / (count - 1)));
                    const auto angle = juce::MathConstants<double>::twoPi * frequency * i / sampleRate;
                    const auto value = buffer.getSample(0, start + i) * window;
                    real += value * std::cos(angle);
                    imag -= value * std::sin(angle);
                }
                return std::complex<double> { real, imag };
            };

            const auto window1Start = blockSize; // 跳过 attack ramp，保证两窗完全对称
            const auto window2Start = totalSamples - analysisWindow;
            const auto x1 = complexDft(stream, window1Start, analysisWindow, f0);
            const auto x2 = complexDft(stream, window2Start, analysisWindow, f0);
            expect(std::abs(x1) > 1e-6, "early window fundamental energy present");
            expect(std::abs(x2) > 1e-7, "late window fundamental energy still measurable");

            const auto phaseDelta = std::arg(x2) - std::arg(x1);
            const auto deltaT = static_cast<double>(window2Start - window1Start) / sampleRate;
            const auto measuredFreq = f0 + phaseDelta / (juce::MathConstants<double>::twoPi * deltaT);
            expectWithinAbsoluteError(measuredFreq, f0, 1e-4 * f0,
                                      "recursive oscillator frequency drift < 1e-4 relative over 20 s");
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

            // 推进到约 3.15 s 处（再渲染 8 个 analysisWindow，中心点 t ≈ 3.15 s；
            // Phase 14-B 双阶段衰减后高次分音的慢分量残存更多，晚期时点后移保持对比余量）。
            juce::AudioBuffer<float> lateBuffer(1, analysisWindow);
            for (auto step = 0; step < 8; ++step) {
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

        beginTest("two-stage decay envelope (fast strike then slow tail)");
        {
            // Phase 14-B：低音 note 36 双指数衰减 A(t) = A[(1-w)e^{-t/τ_f} + w·e^{-t/τ_s}]，
            // τ_f = 0.6 s（ratio 0.15）、τ_s = 4.0 s、w = 0.30（基频，resonance 中性）。
            // 渲染 4.2 s 长流，短窗（4096 样本 ≈ 0.093 s）单点 DFT 在 f1 处取 5 个早期
            // 点（0.05~0.25 s）与 5 个晚期点（2.0~4.0 s），对数幅度线性回归斜率：
            // 预期早期 ≈ -1.18 /s（快分量主导）、晚期 ≈ -0.25 /s（慢分量主导）。
            VoiceFixture fixture;
            constexpr auto renderSeconds = 4.2;
            constexpr auto renderSamples = static_cast<int>(renderSeconds * sampleRate); // 185220
            juce::AudioBuffer<float> stream(1, renderSamples);
            stream.clear();

            auto rendered = 0;
            {
                juce::MidiBuffer midi;
                midi.addEvent(juce::MidiMessage::noteOn(1, 36, 0.9f), 0);
                fixture.synth.renderNextBlock(stream, midi, 0, blockSize);
                rendered += blockSize;
            }
            while (rendered < renderSamples) {
                juce::MidiBuffer empty;
                const auto count = juce::jmin(blockSize, renderSamples - rendered);
                fixture.synth.renderNextBlock(stream, empty, rendered, count);
                rendered += count;
            }
            const auto f1 = PianoSynthVoice::partialFrequency(36, 0);
            constexpr auto shortWindow = 4096;
            // 短窗单点 DFT（Hann 窗，带 start 偏移）：测 t 处窗口的基频幅度。
            auto windowMagnitude = [&](int start) {
                auto real = 0.0;
                auto imag = 0.0;
                for (auto i = 0; i < shortWindow; ++i) {
                    const auto window
                        = 0.5 * (1.0 - std::cos(juce::MathConstants<double>::twoPi * i / (shortWindow - 1)));
                    const auto angle = juce::MathConstants<double>::twoPi * f1 * i / sampleRate;
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
                    const auto magnitude = windowMagnitude(static_cast<int>(t * sampleRate));
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

            expect(slopeEarly < -0.6, "early strike decay must be fast (slope=" + juce::String(slopeEarly, 3) + " /s)");
            expect(slopeLate > -0.45, "slow tail decay must be gentle (slope=" + juce::String(slopeLate, 3) + " /s)");
            expect(std::abs(slopeEarly) > 2.0 * std::abs(slopeLate),
                   "early decay must be more than 2x faster than the tail (early=" + juce::String(slopeEarly, 3)
                       + " late=" + juce::String(slopeLate, 3) + ")");
        }

        beginTest("triple-string unison beating (interference modulation and spectrum doublet)");
        {
            // Phase 14-C：同音三弦微失谐干涉验证
            // MIDI 60（C4 ≈ 261.63 Hz, beatingDetuneRatio = 0.0015 -> Δf ≈ 0.3924 Hz, 周期 T_beat ≈ 2.55 s）。
            // 理论干涉包络：0.5*(sin(ω1 t) + sin(ω2 t)) = cos(π Δf t) * sin((ω1+ω2)/2 t)。
            // 在 t_dip ≈ 1/(2Δf) ≈ 1.28 s 处反相相消（包络下陷）；在 t_rebound ≈ 2.55 s 处同相相长（包络回弹）。
            VoiceFixture fixture;
            constexpr auto testSeconds = 3.2;
            constexpr auto testSamples = static_cast<int>(testSeconds * sampleRate);
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
                const auto start = static_cast<int>(t * sampleRate);
                auto real = 0.0;
                auto imag = 0.0;
                for (auto i = 0; i < windowSize; ++i) {
                    const auto window
                        = 0.5 * (1.0 - std::cos(juce::MathConstants<double>::twoPi * i / (windowSize - 1)));
                    const auto angle = juce::MathConstants<double>::twoPi * targetFreq * i / sampleRate;
                    const auto value = stream.getSample(0, start + i) * window;
                    real += value * std::cos(angle);
                    imag -= value * std::sin(angle);
                }
                return 4.0 * std::sqrt(real * real + imag * imag) / windowSize;
            };

            const auto magEarly = getWindowMag(0.1, f1); // 初始同相（能量高）
            const auto magDip = getWindowMag(1.28, f1); // 反相干涉下陷点（Δθ ≈ π）
            const auto magRebound = getWindowMag(2.55, f1); // 同相干涉回弹峰（Δθ ≈ 2π）

            expect(magEarly > 0.01, "early C4 fundamental is audible");
            expect(magDip < 0.5 * magEarly,
                   "anti-phase dip causes destructive interference (dip=" + juce::String(magDip, 5)
                       + " early=" + juce::String(magEarly, 5) + ")");
            expect(magRebound > 1.3 * magDip,
                   "constructive interference causes envelope rebound at 2.55 s (rebound=" + juce::String(magRebound, 5)
                       + " dip=" + juce::String(magDip, 5) + ")");

            // 验证低音泛音拍频（note 36，第 2 分音 f ≈ 130.8 Hz 开启拍频）：
            const auto bassF2 = PianoSynthVoice::partialFrequency(36, 1);
            const auto bassF2_beat = PianoSynthVoice::beatingFrequency(36, 1);
            expectWithinAbsoluteError(bassF2_beat, bassF2 * 1.0020, 1e-4, "bass 2nd partial beating frequency");
        }
        beginTest("body resonator frequency response and stability (soundboard physics)");
        {
            // 验证 110 Hz（A2）音符在音板主共振峰下的稳态渲染与能量表现
            VoiceFixture a2Fixture;
            juce::AudioBuffer<float> a2Buffer(1, analysisWindow);
            a2Fixture.noteOnBlock(45, 0.9f, a2Buffer); // MIDI 45 = A2 (110.0 Hz)
            const auto fA2 = PianoSynthVoice::partialFrequency(45, 0);
            const auto magA2 = magnitudeAtFrequency(a2Buffer, fA2, analysisWindow);
            expect(magA2 > 0.02, "A2 fundamental ~ 110 Hz is boosted by soundboard peak 1");
            expect(peakMagnitude(a2Buffer) < 1.0f, "wet/dry mixed output remains strictly normalised");

            // 验证 220 Hz（A3）音符在音板第 2 共鸣峰下的表现
            VoiceFixture a3Fixture;
            juce::AudioBuffer<float> a3Buffer(1, analysisWindow);
            a3Fixture.noteOnBlock(57, 0.9f, a3Buffer); // MIDI 57 = A3 (220.0 Hz)
            const auto fA3 = PianoSynthVoice::partialFrequency(57, 0);
            const auto magA3 = magnitudeAtFrequency(a3Buffer, fA3, analysisWindow);
            expect(magA3 > 0.02, "A3 fundamental ~ 220 Hz is boosted by soundboard peak 2");

            // 谐振器状态在 stopNote(false) 后彻底重置
            a2Fixture.synth.noteOff(1, 45, 0.0f, false);
            a2Fixture.renderBlock(a2Buffer);
            expect(peakMagnitude(a2Buffer) == 0.0f, "resonator state is cleared on immediate stop");
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
            expect(engine.getBuiltinSynthTone() == AudioEngine::BuiltinSynthTone::piano, "default tone is piano");

            engine.setBuiltinSynthTone(AudioEngine::BuiltinSynthTone::sine);
            expect(engine.getBuiltinSynthTone() == AudioEngine::BuiltinSynthTone::sine, "switch to sine");
            engine.setAdsr(0.01f, 0.2f, 0.8f, 0.3f); // 对新 voice 的 ADSR 接线不崩溃

            engine.setBuiltinSynthTone(AudioEngine::BuiltinSynthTone::piano);
            expect(engine.getBuiltinSynthTone() == AudioEngine::BuiltinSynthTone::piano, "switch back to piano");

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
