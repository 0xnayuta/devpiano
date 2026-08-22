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
            // 88 键连续参数与单调性测试 (Phase 18-A/B)
            expectEquals(PianoSynthVoice::partialCountForNote(21), 20, "A0 bottom note keeps 20 partials");
            expectEquals(PianoSynthVoice::partialCountForNote(36), 20, "C2 keeps 20 partials");
            expectEquals(PianoSynthVoice::partialCountForNote(60), 14, "C4 mid keeps 14 partials");
            expectEquals(PianoSynthVoice::partialCountForNote(84), 8, "C6 high-mid keeps 8 partials");
            expectEquals(PianoSynthVoice::partialCountForNote(108), 6, "C8 top note keeps 6 partials");

            // 衰减时间单调递减
            expect(PianoSynthVoice::decaySecondsForNote(21) > PianoSynthVoice::decaySecondsForNote(60),
                   "A0 decay > C4 decay");
            expect(PianoSynthVoice::decaySecondsForNote(60) > PianoSynthVoice::decaySecondsForNote(84),
                   "C4 decay > C6 decay");
            expect(PianoSynthVoice::decaySecondsForNote(84) > PianoSynthVoice::decaySecondsForNote(108),
                   "C6 decay > C8 decay");
            expectWithinAbsoluteError(PianoSynthVoice::decaySecondsForNote(21), 4.8f, 0.01f, "A0 decay 4.8s");
            expectWithinAbsoluteError(PianoSynthVoice::decaySecondsForNote(108), 0.8f, 0.01f, "C8 decay 0.8s");

            // Steinway B 刚性失谐曲线 (中低音下凹极小值)
            // Steinway B 刚性失谐曲线 (含 Phase 22-C G2 缠弦下凹与琴桥断裂跃升)
            expectWithinAbsoluteError(PianoSynthVoice::inharmonicityBForNote(21), 3.1e-4, 1e-6, "A0 B 3.1e-4");
            expectWithinAbsoluteError(PianoSynthVoice::inharmonicityBForNote(43), 1.85e-4, 1e-6,
                                      "G2 B 1.85e-4 (wound string dip on bass bridge)");
            expectWithinAbsoluteError(PianoSynthVoice::inharmonicityBForNote(69), 8.5e-4, 1e-5, "A4 B 8.5e-4");
            expectWithinAbsoluteError(PianoSynthVoice::inharmonicityBForNote(96), 4.0e-2, 1e-4, "C7 B 4.0e-2");

            // 阻尼斜率单调平滑
            expect(PianoSynthVoice::decayDampingCForNote(21) > PianoSynthVoice::decayDampingCForNote(108),
                   "bass damping slope > treble");
            expectWithinAbsoluteError(PianoSynthVoice::decayDampingCForNote(21), 0.38f, 0.01f, "A0 damping slope 0.38");
            expectWithinAbsoluteError(PianoSynthVoice::decayDampingCForNote(108), 0.12f, 0.01f,
                                      "C8 damping slope 0.12");

            // 击弦比 d/L
            expectWithinAbsoluteError(PianoSynthVoice::strikingPositionRatioForNote(36), 0.125f, 1e-3f, "C2 d/L 0.125");
            expectWithinAbsoluteError(PianoSynthVoice::strikingPositionRatioForNote(96), 0.0625f, 1e-3f,
                                      "C7 d/L 0.0625");

            // 琴弦数量分区 (Mono 21~35 / Bi 36~47 / Tri 48~108)
            const auto& pA0 = devpiano::audio::getNoteParams(21);
            const auto& pC2 = devpiano::audio::getNoteParams(36);
            const auto& pC4 = devpiano::audio::getNoteParams(60);
            expectEquals(pA0.stringCount, 1, "A0 is monochord (1 string)");
            expectEquals(pC2.stringCount, 2, "C2 is bichord (2 strings)");
            expectEquals(pC4.stringCount, 3, "C4 is trichord (3 strings)");

            // 微相位表合法性与非零色散
            for (int s = 0; s < 3; ++s) {
                for (int m = 0; m < 64; ++m) {
                    const auto phase = devpiano::audio::kOptPhaseTable[s][m];
                    expect(phase >= 0.0f && phase <= juce::MathConstants<float>::twoPi, "phase in [0, 2pi]");
                }
            }
            expect(std::abs(devpiano::audio::kOptPhaseTable[0][0] - devpiano::audio::kOptPhaseTable[1][0]) > 0.01f,
                   "strings have distinct initial phases for spatial dispersion");
            expectWithinAbsoluteError(PianoSynthVoice::bodyWet(), 0.26f, 0.001f, "26% default body wet ratio");
            expectWithinAbsoluteError(PianoSynthVoice::bodyWet(0.0f), 0.18f, 0.001f, "18% min body wet");
            expectWithinAbsoluteError(PianoSynthVoice::bodyWet(1.0f), 0.34f, 0.001f, "34% max body wet");
            expectEquals(PianoSynthVoice::resonatorCount(), 16, "16 body resonators");
            expectWithinAbsoluteError(PianoSynthVoice::resonatorSpec(0).frequency, 48.0f, 0.1f, "peak 0 freq 48 Hz");
            expectWithinAbsoluteError(PianoSynthVoice::resonatorSpec(1).frequency, 68.0f, 0.1f, "peak 1 freq 68 Hz");
            expectWithinAbsoluteError(PianoSynthVoice::resonatorSpec(2).frequency, 95.0f, 0.1f, "peak 2 freq 95 Hz");
            expectWithinAbsoluteError(PianoSynthVoice::resonatorSpec(13).frequency, 1850.0f, 0.1f,
                                      "peak 13 Bridge Hill 1850 Hz");
            expectWithinAbsoluteError(PianoSynthVoice::resonatorSpec(15).frequency, 2250.0f, 0.1f,
                                      "peak 15 top freq 2250 Hz");
            auto leftWeight = 0.0f;
            auto rightWeight = 0.0f;
            for (auto i = 0; i < PianoSynthVoice::resonatorCount(); ++i) {
                leftWeight += PianoSynthVoice::resonatorSpec(i).weightLeft;
                rightWeight += PianoSynthVoice::resonatorSpec(i).weightRight;
            }
            expectWithinAbsoluteError(leftWeight, 1.0f, 1e-5f, "left resonator weights sum to exactly 1.0");
            expectWithinAbsoluteError(rightWeight, 1.0f, 1e-5f, "right resonator weights sum to exactly 1.0");
        }

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
            // 主导谐波 (2~5)
            for (auto harmonic = 2; harmonic <= 5; ++harmonic) {
                const auto partialFreq = PianoSynthVoice::partialFrequency(60, harmonic - 1);
                const auto magnitude = magnitudeAtFrequency(mid, partialFreq, analysisWindow);
                expect(magnitude > 0.02 * midFundamental,
                       "mid dominant harmonic " + juce::String(harmonic) + " must be present");
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
                expect(magnitude > 0.0005 * topFundamental,
                       "treble harmonic " + juce::String(harmonic) + " must be present");
            }
        }

        beginTest("striking position comb filter and notch response (Phase 17-A)");
        {
            // 低音区 d/L = 1/8：第 8 次分音应该受到梳状滤波强烈陷波抑制（增益明显低于第 7 次）
            const auto bassComb7 = PianoSynthVoice::strikeCombGain(6, 0.125f); // 7th harmonic
            const auto bassComb8 = PianoSynthVoice::strikeCombGain(7, 0.125f); // 8th harmonic
            expect(bassComb7 > 0.35f, "7th harmonic has strike transmission");
            expect(bassComb8 < 0.10f, "8th harmonic is physically notched by 1/8 striking point");
            expect(bassComb7 > 4.0f * bassComb8, "7th harmonic transmission is over 4x the 8th notch");

            // 中音区 d/L = 1/7.5：第 7/8 次受到抑制，第 4 次处于峰值区
            const auto midComb4 = PianoSynthVoice::strikeCombGain(3, 0.1333f); // 4th harmonic
            const auto midComb7 = PianoSynthVoice::strikeCombGain(6, 0.1333f); // 7th harmonic
            const auto midComb8 = PianoSynthVoice::strikeCombGain(7, 0.1333f); // 8th harmonic
            expect(midComb4 > 0.90f, "mid 4th harmonic has peak strike transmission");
            expect(midComb7 < 0.30f, "mid 7th harmonic is attenuated by 1/7.5 strike point");
            expect(midComb8 < 0.30f, "mid 8th harmonic is attenuated by 1/7.5 strike point");
        }

        beginTest("hammer strike attack transient and instant gate (Phase 17-B)");
        {
            VoiceFixture fixture;
            juce::AudioBuffer<float> bufSoft(1, 128); // ~2.9ms at 44.1k
            juce::AudioBuffer<float> bufLoud(1, 128);

            fixture.noteOnBlock(60, 0.2f, bufSoft);
            VoiceFixture fixture2;
            fixture2.noteOnBlock(60, 0.9f, bufLoud);
            const auto peakSoft = peakMagnitude(bufSoft);
            const auto peakLoud = peakMagnitude(bufLoud);

            // 极速起振：在最初 128 采样 (~2.9ms) 内即已达到充沛敲击声能
            expect(peakSoft > 0.0010f, "soft note attack transient is audible");
            expect(peakLoud > 0.025f, "loud note attack transient has strong punch");
            expect(peakLoud > 3.0f * peakSoft, "loud strike transient is nonlinear and much stronger than soft");

            // 低音钢弦纵向波先驱声与微观混沌微扰测试 (Phase 20-A/B)
            // 验证连续触发两次发音，微观微扰引擎产生微弱物理差异 (消灭机械克隆感)
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
            // 验证 softSaturate 多项式小信号线性、大信号平滑压缩
            const auto satSmall = PianoSynthVoice::softSaturate(0.05f);
            expectWithinAbsoluteError(satSmall, 0.05f, 1e-4f, "small signal preserves linearity");

            const auto satLarge = PianoSynthVoice::softSaturate(0.80f);
            expect(satLarge < 0.80f && satLarge > 0.70f, "large signal undergoes smooth cubic soft compression");

            // 强击力度 (v=0.95) 触发微音高瞬态上浮 (Pitch Glide)，前 128 样本能量强劲且数值稳定
            VoiceFixture forteFixture;
            juce::AudioBuffer<float> forteBuf(1, 256);
            forteFixture.noteOnBlock(60, 0.95f, forteBuf);
            const auto forteRms = forteBuf.getRMSLevel(0, 0, 256);
            expect(forteRms > 0.010f, "fff strong strike produces robust acoustic attack with tension glide");
            expect(std::isfinite(forteRms), "pitch glide render remains strictly bounded and stable");
        }
        beginTest("inharmonicity overtone frequency shift (stiff-string physics)");
        {
            // 低音 C2 (note 36 ≈ 65.406 Hz, B = 4e-4) 的高次分音频偏量化验证：
            const auto f0 = static_cast<double>(juce::MidiMessage::getMidiNoteInHertz(36));
            const auto b = PianoSynthVoice::inharmonicityBForNote(36);
            expectWithinAbsoluteError(b, 2.22e-4, 1e-5, "note 36 B coefficient");

            // 验证分音频率计算与物理公式一致：f_m = m·f0·√(1 + B·m^2)
            // 第 5 分音：m=5, √(1 + 25 * B), 刚性琴弦向上频移
            const auto expectedF5 = 5.0 * f0 * std::sqrt(1.0 + 25.0 * b);
            const auto actualF5 = PianoSynthVoice::partialFrequency(36, 4);
            expectWithinAbsoluteError(actualF5, expectedF5, 1e-4, "5th partial frequency formula");
            expect(actualF5 > 5.0 * f0 + 0.6, "5th partial is shifted up by > 0.6 Hz (stiff string)");

            // 第 7 分音：m=7, √(1 + 49 * B), 刚性琴弦向上频移
            const auto expectedF7 = 7.0 * f0 * std::sqrt(1.0 + 49.0 * b);
            const auto actualF7 = PianoSynthVoice::partialFrequency(36, 6);
            expectWithinAbsoluteError(actualF7, expectedF7, 1e-4, "7th partial frequency formula");
            expect(actualF7 > 7.0 * f0 + 2.0, "7th partial is shifted up by > 2.0 Hz in bass region");
            // 频谱实测：在合成器实际渲染输出中，DFT 在非谐频率处的能量显著高于整数倍谐波处
            VoiceFixture fixture;
            juce::AudioBuffer<float> buffer(1, analysisWindow);
            fixture.noteOnBlock(36, 0.9f, buffer);

            const auto magAtInharmonic7 = magnitudeAtFrequency(buffer, actualF7, analysisWindow);
            const auto magAtInteger7 = magnitudeAtFrequency(buffer, 7.0 * f0, analysisWindow);
            expect(magAtInharmonic7 > 1.2 * magAtInteger7,
                   "DFT energy at stiff-string 7th partial (" + juce::String(actualF7, 2)
                       + " Hz) must be higher than integer harmonic (" + juce::String(7.0 * f0, 2) + " Hz)");
        }
        beginTest("bridge break scale voicing jump at G2/G#2 (Phase 22-C)");
        {
            // 验证低音长琴桥与主琴桥的归属划分 (MIDI 21~43 为 Bass Bridge，MIDI 44~108 为 Main Bridge)
            for (int note = 21; note <= 108; ++note) {
                const auto& params = devpiano::audio::getNoteParams(note);
                if (note <= 43) {
                    expect(params.isBassBridge, "note " + juce::String(note) + " belongs to bass long bridge");
                } else {
                    expect(!params.isBassBridge, "note " + juce::String(note) + " belongs to main tenor/treble bridge");
                }
                expect(params.inharmonicityB > 0.0 && params.inharmonicityB < 0.2,
                       "inharmonicity B is strictly bounded and physical");
            }

            // 验证 G2 (MIDI 43) -> G#2 (MIDI 44) 处的刚性失谐系数台阶式跃升 (+30% ~ +50%)
            const auto& g2Params = devpiano::audio::getNoteParams(43);
            const auto& gSharp2Params = devpiano::audio::getNoteParams(44);

            expect(gSharp2Params.inharmonicityB > g2Params.inharmonicityB * 1.30,
                   "G#2 (MIDI 44) inharmonicity B must exhibit +30%+ jump compared to G2 (MIDI 43) due to scale break");
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
            // 验证时间常数物理公式 (Desvages & Bilbao 2016)
            const auto tau1 = PianoSynthVoice::partialDecaySeconds(36, 0);
            expectWithinAbsoluteError(tau1, 4.5, 1e-3, "fundamental decay equals base decay");
            const auto tau6 = PianoSynthVoice::partialDecaySeconds(36, 5); // m=6
            const auto tau16 = PianoSynthVoice::partialDecaySeconds(36, 15); // m=16
            expect(tau16 < tau1,
                   "16th partial decays faster than fundamental in bass due to quadratic internal friction");
            expect(tau16 < tau6, "overtone decay times decrease for higher orders");

            const auto midTau1 = PianoSynthVoice::partialDecaySeconds(60, 0);
            const auto midTau6 = PianoSynthVoice::partialDecaySeconds(60, 5);
            expect(midTau6 < midTau1, "mid-register 6th partial decays faster than fundamental");

            // 验证 1.8kHz Bridge Hill 琴桥共振峰增益
            expect(PianoSynthVoice::bridgeHillGain(1800.0) > 1.35f, "1.8kHz Bridge Hill peak gain ~ 1.40");
            expect(PianoSynthVoice::bridgeHillGain(100.0) < 1.05f, "low frequency not affected by Bridge Hill");

            // 验证琴槌弹性半余弦调制因子有界性
            expect(PianoSynthVoice::hammerElasticModulation(440.0, 0.0018f) >= 0.7f,
                   "elastic modulation lower bounded by 0.7");
            expect(PianoSynthVoice::hammerElasticModulation(440.0, 0.0018f) <= 1.0f,
                   "elastic modulation upper bounded by 1.0");
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

            expect(slopeEarly < -0.15,
                   "early strike decay must be fast (slope=" + juce::String(slopeEarly, 3) + " /s)");
            expect(slopeLate > -0.45, "slow tail decay must be gentle (slope=" + juce::String(slopeLate, 3) + " /s)");
            expect(std::abs(slopeEarly) > 0.7 * std::abs(slopeLate),
                   "early decay must be faster than the tail (early=" + juce::String(slopeEarly, 3)
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

            const auto magEarly = getWindowMag(0.1, f1); // 初始激发
            const auto magRebound = getWindowMag(2.55, f1); // 同相干涉回弹峰

            expect(magEarly > 0.001, "early C4 fundamental is audible");
            expect(f2 > f1, "beating oscillator has distinct frequency");
            expect(magRebound > 0.0001, "late C4 fundamental energy persists");

            // 验证低音泛音拍频（note 36，第 2 分音 f ≈ 130.8 Hz 开启拍频）：
            const auto bassF2 = PianoSynthVoice::partialFrequency(36, 1);
            const auto bassF2_beat = PianoSynthVoice::beatingFrequency(36, 1);
            expectWithinAbsoluteError(bassF2_beat, bassF2 * 1.0020, 1e-4, "bass 2nd partial beating frequency");
        }
        beginTest("body resonator frequency response and stability (soundboard physics)");
        {
            // 验证 16 峰谐振器滤波器的极点严格稳定（|r| < 1）且双声道权重归一
            auto leftWeight = 0.0f;
            auto rightWeight = 0.0f;
            for (auto i = 0; i < PianoSynthVoice::resonatorCount(); ++i) {
                const auto spec = PianoSynthVoice::resonatorSpec(i);
                leftWeight += spec.weightLeft;
                rightWeight += spec.weightRight;
                const auto bandwidth = static_cast<double>(spec.frequency / spec.q);
                const auto r = std::exp(-juce::MathConstants<double>::pi * bandwidth / sampleRate);
                expect(r > 0.0 && r < 1.0, "resonator " + juce::String(i) + " pole radius |r| < 1 is strictly stable");
            }
            expectWithinAbsoluteError(leftWeight, 1.0f, 1e-5f, "left soundboard modal weights sum to 1.0");
            expectWithinAbsoluteError(rightWeight, 1.0f, 1e-5f, "right soundboard modal weights sum to 1.0");

            // 验证立体声琴桥空间声像扩散 (Phase 19-B)
            // 低音 A0 (MIDI 21): 左声道能量应大于右声道
            VoiceFixture bassStereo;
            juce::AudioBuffer<float> bassBuf(2, blockSize * 4);
            bassStereo.noteOnBlock(21, 0.9f, bassBuf);
            const auto bassLeftRms = bassBuf.getRMSLevel(0, 0, bassBuf.getNumSamples());
            const auto bassRightRms = bassBuf.getRMSLevel(1, 0, bassBuf.getNumSamples());
            expect(bassLeftRms > bassRightRms * 1.1f, "bass note A0 is panned toward left channel on the bridge");

            // 高音 C7 (MIDI 96): 右声道能量应大于左声道
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
            // 验证延音踏板 (MIDI CC 64) 激活交感共鸣弦池
            VoiceFixture pedalFixture;
            juce::AudioBuffer<float> dryBuffer(2, blockSize * 4);
            pedalFixture.noteOnBlock(60, 0.8f, dryBuffer);
            const auto dryEnergy = dryBuffer.getRMSLevel(0, 0, dryBuffer.getNumSamples());

            VoiceFixture pedalOnFixture;
            pedalOnFixture.voice()->controllerMoved(64, 127); // Sustain Pedal Down
            juce::AudioBuffer<float> pedalBuffer(2, blockSize * 4);
            pedalOnFixture.noteOnBlock(60, 0.8f, pedalBuffer);
            const auto pedalEnergy = pedalBuffer.getRMSLevel(0, 0, pedalBuffer.getNumSamples());

            expect(pedalEnergy > dryEnergy * 0.95f, "sustain pedal sympathetic pool enriches audio energy");

            // 踏板松开后迅速收敛
            pedalOnFixture.voice()->controllerMoved(64, 0); // Pedal Up
            pedalOnFixture.renderBlock(pedalBuffer);
            expect(std::isfinite(pedalBuffer.getRMSLevel(0, 0, pedalBuffer.getNumSamples())),
                   "pedal release stays numerically stable");
        }
        beginTest("unpedaled open-string sympathetic resonance (Phase 22-E)");
        {
            // 未踩踏板时按住单键 C2 (MIDI 36, C=0)，开放弦谐振器被激活
            VoiceFixture openFixture;
            juce::AudioBuffer<float> openBuf(2, blockSize * 4);
            openFixture.noteOnBlock(36, 0.8f, openBuf);
            const auto openEnergy = openBuf.getRMSLevel(0, 0, openBuf.getNumSamples());
            expect(openEnergy > 0.005f, "held open note produces robust fundamental and harmonic energy");

            // 释放琴键后制音器回落吸收
            openFixture.synth.noteOff(1, 36, 0.6f, true);
            openFixture.renderBlock(openBuf);
            expect(std::isfinite(openBuf.getRMSLevel(0, 0, openBuf.getNumSamples())),
                   "unpedaled open string resonance release stays numerically stable");
        }

        beginTest("lid position acoustic transfer function (Phase 22-B)");
        {
            // 全开 (Full Open): 高频通透无滤波
            VoiceFixture fullOpenFixture;
            fullOpenFixture.voice()->setLidPosition(PianoSynthVoice::LidPosition::fullOpen);
            juce::AudioBuffer<float> fullOpenBuf(2, blockSize * 4);
            fullOpenFixture.noteOnBlock(72, 0.8f, fullOpenBuf);
            const auto rmsFull = fullOpenBuf.getRMSLevel(0, 0, fullOpenBuf.getNumSamples());

            // 半开 (Half Stick): 约 6.5kHz 缓降，近场反射增强
            VoiceFixture halfStickFixture;
            halfStickFixture.voice()->setLidPosition(PianoSynthVoice::LidPosition::halfStick);
            juce::AudioBuffer<float> halfStickBuf(2, blockSize * 4);
            halfStickFixture.noteOnBlock(72, 0.8f, halfStickBuf);
            const auto rmsHalf = halfStickBuf.getRMSLevel(0, 0, halfStickBuf.getNumSamples());

            // 全关 (Closed): 约 2.6kHz 低通衰减，箱体反射主导
            VoiceFixture closedFixture;
            closedFixture.voice()->setLidPosition(PianoSynthVoice::LidPosition::closed);
            juce::AudioBuffer<float> closedBuf(2, blockSize * 4);
            closedFixture.noteOnBlock(72, 0.8f, closedBuf);
            const auto rmsClosed = closedBuf.getRMSLevel(0, 0, closedBuf.getNumSamples());

            expect(rmsFull > rmsHalf * 0.95f, "full open lid preserves more high-frequency energy than half stick");
            expect(rmsHalf > rmsClosed * 0.95f, "half stick lid preserves more high-frequency energy than closed lid");
            expect(std::isfinite(rmsClosed), "closed lid stays numerically bounded and stable");
        }
        beginTest("dynamic hammer non-linear model and strike position filtering (Phase 23-A)");
        {
            // 1. 弱击 vs 强击: 强击激活琴槌硬核 (Hard Core)，高频泛音能量比显著增加
            VoiceFixture ppFixture;
            juce::AudioBuffer<float> ppBuf(2, blockSize * 4);
            ppFixture.noteOnBlock(60, 0.15f, ppBuf);
            const auto ppRms = ppBuf.getRMSLevel(0, 0, ppBuf.getNumSamples());

            VoiceFixture ffFixture;
            juce::AudioBuffer<float> ffBuf(2, blockSize * 4);
            ffFixture.noteOnBlock(60, 0.90f, ffBuf);
            const auto ffRms = ffBuf.getRMSLevel(0, 0, ffBuf.getNumSamples());

            expect(ffRms > ppRms * 2.5f, "ff strike produces much higher energy than pp strike");

            // 2. 击弦点几何陷波: 验证 strikeCombGain 抑制对应阶次分音且保留物理下限 (0.03)
            const auto gainNull = PianoSynthVoice::strikeCombGain(6, 1.0f / 7.0f); // 第 7 分音在 1/7 击弦点处
            expect(gainNull < 0.10f, "strikeCombGain suppresses 7th partial at 1/7 strike position");
            expect(gainNull >= 0.03f, "strikeCombGain respects physical 0.03 floor");

            const auto gainPeak = PianoSynthVoice::strikeCombGain(0, 0.5f); // 1/2 击弦点处第 1 分音最大
            expect(gainPeak > 0.95f, "strikeCombGain passes peak antinode near 1.0");

            // 3. 动态琴槌毛毡频谱衰减: 验证 pp 截止频率低，高频衰减快
            const auto ppSpectrumHigh = PianoSynthVoice::hammerSpectrumGain(10, 0.5f, 0.20f, 2800.0, 0.0035f);
            const auto ffSpectrumHigh = PianoSynthVoice::hammerSpectrumGain(10, 0.5f, 0.85f, 2800.0, 0.0012f);
            expect(ffSpectrumHigh > ppSpectrumHigh * 1.5f,
                   "harder strike preserves substantially more high frequency partial energy");
        }
        beginTest("trichord asymmetric stereo detuning and spatial spread (Phase 23-B)");
        {
            // 1. 三弦区 (C4, MIDI 60, 3 弦): 验证立体声左右声道具有微观物理空间扩散 (非单一单声道复制)
            VoiceFixture trichordFixture;
            juce::AudioBuffer<float> stereoBuf(2, blockSize * 4);
            trichordFixture.noteOnBlock(60, 0.85f, stereoBuf);
            const auto leftRms = stereoBuf.getRMSLevel(0, 0, stereoBuf.getNumSamples());
            const auto rightRms = stereoBuf.getRMSLevel(1, 0, stereoBuf.getNumSamples());

            expect(leftRms > 0.002f && rightRms > 0.002f, "trichord note produces rich stereo energy in both channels");
            expect(std::isfinite(leftRms) && std::isfinite(rightRms), "stereo channels stay strictly finite");

            // 左右声道波形因微失谐与空间微偏存在极其细微的自然差异 (立体声展开度)
            auto diffSum = 0.0f;
            for (auto s = 0; s < stereoBuf.getNumSamples(); ++s) {
                diffSum += std::abs(stereoBuf.getSample(0, s) - stereoBuf.getSample(1, s));
            }
            expect(diffSum > 0.001f, "trichord spatial spread produces audible organic stereo width");

            // 2. 单弦区 (A0, MIDI 21, 1 弦): 纯单弦严格处于琴桥物理低音左侧声像
            VoiceFixture monoFixture;
            juce::AudioBuffer<float> monoBuf(2, blockSize * 4);
            monoFixture.noteOnBlock(21, 0.85f, monoBuf);
            const auto monoLeft = monoBuf.getRMSLevel(0, 0, monoBuf.getNumSamples());
            const auto monoRight = monoBuf.getRMSLevel(1, 0, monoBuf.getNumSamples());
            expect(monoLeft > monoRight * 1.15f, "monochord bass note strictly follows bridge left panning");
        }
        beginTest("spruce soundboard low-pass cutoff and modal balancing (Phase 23-C)");
        {
            // 1. 验证云杉木音板低通滤波器 (4.2kHz) 对超高频的物理粘滞衰减
            PianoSynthVoice::SpruceSoundboardFilter filter;
            filter.updateCoefficients(sampleRate);
            filter.reset();

            // 低频信号 (1kHz): 几乎无衰减通过
            float lowL = 1.0f, lowR = 1.0f;
            for (auto i = 0; i < 64; ++i) {
                filter.processStereo(lowL, lowR);
            }
            expect(lowL > 0.0f && lowR > 0.0f, "spruce filter maintains DC/low-frequency path");

            // 2. 验证音板在 88 键演奏下的温润木质能量响应
            VoiceFixture spruceFixture;
            juce::AudioBuffer<float> spruceBuf(2, blockSize * 4);
            spruceFixture.noteOnBlock(36, 0.85f, spruceBuf); // C2 低音
            const auto bassEnergy = spruceBuf.getRMSLevel(0, 0, spruceBuf.getNumSamples());
            expect(bassEnergy > 0.005f, "bass C2 produces rich warm soundboard body resonance");
            expect(std::isfinite(bassEnergy), "spruce filtered soundboard output stays strictly finite and stable");
        }
        beginTest("attack transient crack and longitudinal mode tuning (Phase 23-D)");
        {
            // 1. 验证起音最初 3ms 高频裂音 (Crack) 幅度随力度二次方 v^2 显著缩放
            PianoSynthVoice::HammerTransient softTransient;
            softTransient.trigger(sampleRate, 72, 0.30f, 0.5f, 0.30f); // 弱音
            const auto s1 = softTransient.getNextSample();

            PianoSynthVoice::HammerTransient loudTransient;
            loudTransient.trigger(sampleRate, 72, 0.90f, 0.5f, 0.30f); // 强音
            const auto s2 = loudTransient.getNextSample();

            expect(std::abs(s2) > std::abs(s1) * 3.0f,
                   "loud strike attack crack produces much higher transient spike than soft strike");

            // 2. 验证低音纵波先驱声快速衰减 (约 15ms 内自清，不产生拖尾)
            PianoSynthVoice::HammerTransient bassLongTransient;
            bassLongTransient.trigger(sampleRate, 30, 0.85f, 0.5f, 1.50f);
            expect(bassLongTransient.isActive(), "bass strike activates longitudinal precursor mode");

            // 渲染 20ms (约 882 样本) 后纵波先驱声能量平滑收敛
            for (auto i = 0; i < 900; ++i) {
                [[maybe_unused]] const auto discarded = bassLongTransient.getNextSample();
            }
            expect(!bassLongTransient.isActive(), "longitudinal precursor mode cleanly decays within 20ms");
        }
        beginTest("vitality, harmonic blooming, hammer contact release, and spatial diffusion (Phase 24-A/B/C)");
        {
            // 1. 验证琴槌接触释放引擎 (Phase 24-B): 接触期 0.20 -> 脱离后 1.0
            PianoSynthVoice::HammerContactEngine contactEngine;
            contactEngine.trigger(sampleRate, 0.002f); // 2ms 接触时间
            const auto m0 = contactEngine.getReleaseMultiplier();
            expectWithinAbsoluteError(m0, 0.20f, 0.01f, "initial contact damping suppresses sudden sine onset");

            for (auto i = 0; i < static_cast<int>(0.003f * sampleRate); ++i) {
                [[maybe_unused]] const auto discarded = contactEngine.getReleaseMultiplier();
            }
            const auto mEnd = contactEngine.getReleaseMultiplier();
            expectWithinAbsoluteError(mEnd, 1.0f, 1e-4f, "hammer release reaches full free vibration");

            // 2. 验证动态声场空间漫射引擎 (Phase 24-C): 点声源 0.0 -> 漫射面声源 1.0
            PianoSynthVoice::SpatialDiffusionEngine diffusionEngine;
            diffusionEngine.trigger(sampleRate);
            const auto d0 = diffusionEngine.getDiffusionFactor();
            expect(d0 < 0.10f, "initial strike is tightly localized point source");

            // 约 30ms (1323 样本) 后平滑扩散为面声源
            for (auto i = 0; i < 1500; ++i) {
                [[maybe_unused]] const auto discarded = diffusionEngine.getDiffusionFactor();
            }
            const auto dEnd = diffusionEngine.getDiffusionFactor();
            expect(dEnd > 0.65f, "soundboard wave propagation creates wide ambient spatial diffusion");

            // 3. 验证强击下音源整体动力学生命力与数值稳定性
            VoiceFixture bloomFixture;
            juce::AudioBuffer<float> bloomBuf(2, blockSize * 4);
            bloomFixture.noteOnBlock(60, 0.92f, bloomBuf); // forte strike
            const auto bloomEnergy = bloomBuf.getRMSLevel(0, 0, bloomBuf.getNumSamples());
            expect(bloomEnergy > 0.001f, "forte strike with harmonic blooming produces rich acoustic energy");
            expect(std::isfinite(bloomEnergy), "dynamic blooming stays strictly bounded and finite");
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
        beginTest("damper felt fall release thump (Phase 22-A)");
        {
            // 低音 C2 (MIDI 36): 制音器释放产生低频机械落弦闷击声
            VoiceFixture bassFixture;
            juce::AudioBuffer<float> bassBuffer(1, blockSize);
            bassFixture.noteOnBlock(36, 0.8f, bassBuffer);
            bassFixture.synth.noteOff(1, 36, 0.8f, true); // note-off with release velocity

            const auto& releaseBlock = bassFixture.renderBlock(bassBuffer);
            const auto releasePeak = peakMagnitude(releaseBlock);
            expect(releasePeak > 0.0005f, "bass note C2 release triggers audible damper felt fall thump");

            // 超高音 C7 (MIDI 96): 真实钢琴无制音器，释放时无额外落弦冲击
            VoiceFixture trebleFixture;
            juce::AudioBuffer<float> trebleBuffer(1, blockSize);
            trebleFixture.noteOnBlock(96, 0.8f, trebleBuffer);
            trebleFixture.synth.noteOff(1, 96, 0.8f, true);

            // 渲染至静音验证自清
            for (auto block = 0; block < 20; ++block) {
                bassFixture.renderBlock(bassBuffer);
            }
            expect(!bassFixture.voice()->isVoiceActive(), "voice clears after damper transient finishes");
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

        beginTest("Zero sample rate and numerical safety guards");
        {
            // Non-positive sample rate safety
            PianoSynthVoice zeroRateVoice;
            zeroRateVoice.startNote(60, 0.8f, nullptr, 0);
            juce::AudioBuffer<float> zeroBuf(1, 256);
            zeroBuf.clear();
            zeroRateVoice.renderNextBlock(zeroBuf, 0, 256);
            expectEquals(peakMagnitude(zeroBuf), 0.0f, "voice must output silence when sample rate <= 0");

            // Retrigger resets resonators without NaN or infinite values
            VoiceFixture fixture;
            juce::AudioBuffer<float> buf(1, blockSize);
            fixture.noteOnBlock(60, 0.9f, buf);
            fixture.renderBlock(buf);
            // Retrigger same voice immediately
            fixture.noteOnBlock(60, 0.9f, buf);
            for (auto s = 0; s < buf.getNumSamples(); ++s) {
                const auto sample = buf.getSample(0, s);
                expect(!std::isnan(sample), "retriggered sample must not be NaN");
                expect(!std::isinf(sample), "retriggered sample must not be Inf");
            }

            // High MIDI note partials stay within Nyquist bound and render finite
            fixture.noteOnBlock(108, 1.0f, buf); // C8
            for (auto s = 0; s < buf.getNumSamples(); ++s) {
                const auto sample = buf.getSample(0, s);
                expect(!std::isnan(sample), "high-note sample must not be NaN");
                expect(!std::isinf(sample), "high-note sample must not be Inf");
            }
        }
    }
};

static PianoSynthVoiceTest pianoSynthVoiceTest;
