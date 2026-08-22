#pragma once

#include <JuceHeader.h>

#include "Piano88KeyTable.h"

#include <array>
#include <cmath>

// 内置物理建模钢琴合成器（Phase 12~13）：与 SineSynthVoice 并存的内置音色。
// 与 SineSynthVoice 相同的承载方式（继承 juce::SynthesiserVoice），由
// juce::Synthesiser 管理 voice 生命周期；AudioEngine::setBuiltinSynthTone
// 切换实时路径注册的音色（默认内置音色为 Piano，Sine 可切换回退）。
//
// - 分音叠加与刚性琴弦非谐性（Phase 13-1）：基频 + 高次分音，各分音频率按 JOS PASP
//   刚性琴弦公式计算 f_m = m·f₀·√(1+B·m²)，刚度系数 B 按音区查表（低音弦粗 B≈4e-4，
//   高音 B≈1e-5），各分音失去公共整数倍周期，产生自然的泛音拍频（Beats）；
// - 递归振荡器与分音数扩展（Phase 14-A）：分音上限 8 → 20/14/8/6（按音区），每分音
//   用 Magic Circle coupled-form 递归振荡器（零 std::sin、幅度严格有界、频率由
//   ε = 2·sin(π·f/fs) 精确决定），幅度衰减以每采样增益乘法维持；
// - 双阶段衰减（Phase 14-B）：每分音双指数分量 A(t) = A·[(1-w)·e^{-t/τ_fast} + w·e^{-t/τ_slow}]，
//   快分量对应击弦后弦-音板快速能量辐射（τ_fast ≈ 0.15~0.2 × τ_slow），慢分量对应
//   弱耦合偏振模态的绵长尾音（权重 w 按音区 0.15~0.30）；两套 decayPerSample 在
//   startNote 预计算，逐采样各一次乘法；
// - 同音三弦拍频（Phase 14-C）：对低中音区核心分音（前 4~6 分音）引入微失谐双振荡器对
//   （beating doublet，失谐率 0.10%~0.20%），两根弦同相激发后自然产生周期性相长/相消干涉
//   （Beating，周期 ~1.5~4 s），重现真实钢琴同音多弦调律带来的"厚实颤动"感；高音区关闭。
// - 模态分音衰减速率建模（Phase 13-2）：基于 Mutable Instruments 模态能量耗散模型，
//   分音时间常数 τ_m = τ_base / (1.0 + c_eff · (m - 1))，高次分音快速耗散，音色由击弦瞬间
//   的丰富泛音自然过渡至基频主导的纯净尾音；pianoResonance 调节 τ_base，pianoBrightness
// - 琴体音板共鸣模态组（Phase 14-D 扩展至 8 峰）：借鉴 Bank 2010 / Pianoteq 音板
//   实测模态分布，在 voice 输出端挂载 8 个音板主共鸣峰（75~950 Hz，含箱体呼吸模态、
//   琴桥耦合模态与板面共鸣峰），归一化权重和为 1.0，经 Wet/Dry 混合（wet = 0.25）
//   注入饱满温暖的木质共鸣箱体感；极点严格在单位圆内，渐近绝对稳定；
// - velocity 双映射：响度 level = v^1.5（弱奏更敏感）+ 亮度（高次谐波增益随 v 提升）；
// - attack/release 沿用 AudioEngine::setAdsr 接线（经 setAdsrParameters
//   提取 attack/release 作门控，decay/sustain 由分音衰减替代）。
//
// 实时约束（Phase 14-A 起）：renderNextBlock 无堆分配、无锁；每分音 Magic Circle
// 递归振荡器（coupled form，3 乘 2 加双精度，零 std::sin）+ 每采样增益衰减，≤20 分音
// + 3 个二阶谐振器 ≈ 每 sample ≤ 184 次运算；startNote 内每 note 做 O(partials) 次
// std::sqrt/std::sin/std::exp（按键瞬间计算，逐采样渲染 CPU 极轻量）。

class PianoSynthSound final : public juce::SynthesiserSound {
public:
    bool appliesToNote(int) override {
        return true;
    }
    bool appliesToChannel(int) override {
        return true;
    }
};

class PianoSynthVoice final : public juce::SynthesiserVoice {
public:
    static constexpr auto maxPartials = 20;
    static constexpr auto numResonators = 16;
    static constexpr auto bodyWetRatio = 0.26f; // 26% default wet soundboard, 74% dry string
    // 每 voice 峰值上限（v=1）：0.16 为 8 voice 齐奏 + masterGain 0.8 预留
    // 复音余量（实际峰值约为该值的 0.6~0.8，最坏 8 voice 周期对齐 ≈0.9）。
    static constexpr auto peakLevelAtFullVelocity = 0.16f;
    static constexpr auto silentLevelThreshold = 1e-4f;

    bool canPlaySound(juce::SynthesiserSound* sound) override {
        return dynamic_cast<PianoSynthSound*>(sound) != nullptr;
    }

    // 只取 attack/release 作门控；decay/sustain 由分音独立衰减替代（Phase 12-2）。
    // 先设采样率再设参数：juce::ADSR 的 rate 仅在 setParameters 时重算，
    // 未先 setSampleRate 会按内部默认 44100 计算（48 kHz 设备 attack 偏快）。
    // 注意：addVoice 会用 synth 的当前 sampleRate 覆盖 voice（构造期为 0），
    // 采样率无效时跳过，ADSR 内部默认 44100 无断言。
    void setAdsrParameters(const juce::ADSR::Parameters& parameters) {
        if (getSampleRate() > 0.0) {
            adsrGate.setSampleRate(getSampleRate());
        }
        // 极速起振门控 (Phase 17-B)：强制 attack <= 0.0002s (0.2ms) 瞬时爆发消除拉弓感
        const auto attackSec = juce::jmin(0.0002f, parameters.attack);
        adsrGate.setParameters({ attackSec, 0.001f, 1.0f, parameters.release });
    }

    // 音色参数（Phase 12-3，0..1，默认 0.5 与 v1 基准行为一致）：
    // - brightness：亮度基准，作用于 velocity 亮度映射（b=0.5 时与 12-2 相同）；
    // - hammerHardness：击弦硬度，高次谐波起始增益（0.5 中性）；
    // - resonance：共鸣/余韵，衰减时间缩放（0.5 中性，高 → 衰减更慢）。
    void setPianoParameters(float brightness, float hammerHardness, float resonance) noexcept {
        pianoBrightness = juce::jlimit(0.0f, 1.0f, brightness);
        pianoHammerHardness = juce::jlimit(0.0f, 1.0f, hammerHardness);
        pianoResonance = juce::jlimit(0.0f, 1.0f, resonance);
    }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override {
        const auto sampleRate = getSampleRate();
        if (sampleRate <= 0.0) {
            return;
        }

        currentPlayingMidiNote = midiNoteNumber;
        const auto& params = devpiano::audio::getNoteParams(midiNoteNumber);
        const auto baseFrequency = static_cast<double>(juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));

        numActivePartials = params.partialCount;

        // velocity 响度：v^1.5（弱奏更敏感），用 sqrt 避免 std::pow。
        const auto clampedVelocity = juce::jlimit(0.0f, 1.0f, velocity);
        const auto velocityLevel = clampedVelocity * std::sqrt(clampedVelocity);
        // velocity 亮度：随力度线性提升；pianoBrightness 调节亮度基准
        const auto brightnessFactor = clampedVelocity * (0.5f + pianoBrightness);
        // 共鸣：衰减时间缩放（r=0.5 中性，r=1 → ×1.3，r=0 → ×0.7）。
        const auto decayScale = 1.0f + (pianoResonance - 0.5f) * 0.6f;
        const auto baseDecaySeconds = static_cast<double>(params.decaySeconds * decayScale);

        // 琴槌动态接触时间 (Phase 18-C): 随力度与硬度收缩 (pp 柔和接触长 -> ff 极短冲击)
        const auto effectiveHardness
            = 0.15f + 0.85f * std::pow(clampedVelocity, 1.5f) * (0.5f + 0.5f * pianoHammerHardness);
        const auto tc = params.tcBase * (2.5f - 1.9f * effectiveHardness);

        // 琴弦空间频率常数 (k1 = (π/L)²) 与基频耗散率 alpha1
        const auto piOverL = juce::MathConstants<double>::pi / static_cast<double>(params.stringLength);
        const auto k1 = piOverL * piOverL;
        const auto alpha1 = static_cast<double>(params.b1) + static_cast<double>(params.b2) * k1;

        // 归一化：按当前击弦位置、半余弦调制与琴桥共振峰求和，使 v=1 时峰值恒为 peakLevelAtFullVelocity。
        auto normSum = 0.0f;
        for (auto n = 0; n < numActivePartials; ++n) {
            const auto partialNumber = static_cast<double>(n + 1);
            const auto inharmonicFactor = std::sqrt(1.0 + params.inharmonicityB * partialNumber * partialNumber);
            const auto partialFrequency = baseFrequency * partialNumber * inharmonicFactor;
            normSum += amplitudeFor(n, params.strikePosRatio, brightnessFactor, partialFrequency, tc)
                * hammerGain(n, numActivePartials) * brightnessBoost(n, pianoBrightness, numActivePartials);
        }
        const auto scale = peakLevelAtFullVelocity / juce::jmax(1e-6f, normSum);

        const auto nyquistLimit = sampleRate * 0.495;

        for (auto n = 0; n < numActivePartials; ++n) {
            auto& partial = partials[static_cast<std::size_t>(n)];
            const auto m = static_cast<double>(n + 1);
            const auto inharmonicFactor = std::sqrt(1.0 + params.inharmonicityB * m * m);
            const auto partialFrequency = baseFrequency * m * inharmonicFactor;

            if (partialFrequency >= nyquistLimit) {
                partial.level = 0.0f;
                partial.levelFast = 0.0f;
                partial.levelSlow = 0.0f;
                partial.epsilon = 0.0;
                partial.epsilon2 = 0.0;
                partial.epsilon3 = 0.0;
                partial.stringCount = 1;
                partial.decayFastPerSample = 0.0f;
                partial.decaySlowPerSample = 0.0f;
                continue;
            }

            // 同音多弦独立振荡网络 (Phase 19-C, Monochord / Bichord / Trichord)
            partial.stringCount = params.stringCount;
            const auto phase1 = devpiano::audio::kOptPhaseTable[0][static_cast<std::size_t>(n % 64)];
            const auto phase2 = devpiano::audio::kOptPhaseTable[1][static_cast<std::size_t>(n % 64)];
            const auto phase3 = devpiano::audio::kOptPhaseTable[2][static_cast<std::size_t>(n % 64)];

            if (partial.stringCount == 1 || n >= params.beatingPartials || params.beatingDetuneRatio <= 0.0f) {
                // 单弦 (Monochord) 或高次无拍频分音
                partial.stringCount = 1;
                partial.cosState = std::cos(static_cast<double>(phase1));
                partial.sinState = std::sin(static_cast<double>(phase1));
                partial.epsilon = 2.0 * std::sin(juce::MathConstants<double>::pi * partialFrequency / sampleRate);
                partial.epsilon2 = 0.0;
                partial.epsilon3 = 0.0;
            } else if (partial.stringCount == 2) {
                // 双弦 (Bichord): 对称微失谐 (-Δc/2, +Δc/2)
                const auto detuneHalf = static_cast<double>(params.beatingDetuneRatio * 0.5f);
                const auto f1 = partialFrequency * (1.0 - detuneHalf);
                const auto f2 = partialFrequency * (1.0 + detuneHalf);

                partial.cosState = std::cos(static_cast<double>(phase1));
                partial.sinState = std::sin(static_cast<double>(phase1));
                partial.epsilon = 2.0 * std::sin(juce::MathConstants<double>::pi * f1 / sampleRate);

                partial.cosState2 = std::cos(static_cast<double>(phase2));
                partial.sinState2 = std::sin(static_cast<double>(phase2));
                partial.epsilon2 = 2.0 * std::sin(juce::MathConstants<double>::pi * f2 / sampleRate);
                partial.epsilon3 = 0.0;
            } else {
                // 三弦 (Trichord): 非对称合唱微失谐 (-Δc, 0, +Δc)
                const auto detune = static_cast<double>(params.beatingDetuneRatio);
                const auto f1 = partialFrequency * (1.0 - detune);
                const auto f2 = partialFrequency;
                const auto f3 = partialFrequency * (1.0 + detune);

                partial.cosState = std::cos(static_cast<double>(phase1));
                partial.sinState = std::sin(static_cast<double>(phase1));
                partial.epsilon = 2.0 * std::sin(juce::MathConstants<double>::pi * f1 / sampleRate);

                partial.cosState2 = std::cos(static_cast<double>(phase2));
                partial.sinState2 = std::sin(static_cast<double>(phase2));
                partial.epsilon2 = 2.0 * std::sin(juce::MathConstants<double>::pi * f2 / sampleRate);

                partial.cosState3 = std::cos(static_cast<double>(phase3));
                partial.sinState3 = std::sin(static_cast<double>(phase3));
                partial.epsilon3 = 2.0 * std::sin(juce::MathConstants<double>::pi * f3 / sampleRate);
            }

            partial.level = amplitudeFor(n, params.strikePosRatio, brightnessFactor, partialFrequency, tc)
                * hammerGain(n, numActivePartials) * brightnessBoost(n, pianoBrightness, numActivePartials) * scale
                * velocityLevel;

            // 空气黏性阻尼与二次方内部摩擦耗散 (Phase 18-C, Desvages & Bilbao 2016)
            // α_n = b1·0.80 + b1·0.20·√(f0/fn) + b2·(nπ/L)²
            const auto kn = m * m * k1;
            const auto airTerm = std::sqrt(baseFrequency / std::max(partialFrequency, 20.0));
            const auto alpha_n
                = static_cast<double>(params.b1) * (0.80 + 0.20 * airTerm) + static_cast<double>(params.b2) * kn;

            // 慢衰减时间常数 τ_slow,n 与快衰减常数 τ_fast,n
            const auto dampingEffect
                = (alpha_n / juce::jmax(1e-9, alpha1)) * static_cast<double>(1.5f - pianoBrightness);
            const auto tau_m = baseDecaySeconds / dampingEffect;
            const auto tauFast_m = tau_m * static_cast<double>(params.fastDecayRatio);
            partial.decayFastPerSample = static_cast<float>(std::exp(-1.0 / (tauFast_m * sampleRate)));
            partial.decaySlowPerSample = static_cast<float>(std::exp(-1.0 / (tau_m * sampleRate)));
            partial.levelFast = partial.level * (1.0f - params.slowWeight);
            partial.levelSlow = partial.level * params.slowWeight;
        }

        hammerTransient.trigger(sampleRate, midiNoteNumber, clampedVelocity, pianoHammerHardness);
        for (auto& resonator : bodyResonators) {
            resonator.reset();
            resonator.updateCoefficients(sampleRate);
        }

        adsrGate.setSampleRate(sampleRate);
        adsrGate.noteOn();
    }

    void stopNote(float, bool allowTailOff) override {
        if (allowTailOff) {
            adsrGate.noteOff();
            return;
        }

        adsrGate.reset();
        hammerTransient.reset();
        for (auto& resonator : bodyResonators) {
            resonator.reset();
        }
        clearCurrentNote();
    }
    void pitchWheelMoved(int) override {
    }
    void controllerMoved(int, int) override {
    }

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override {
        if (!isVoiceActive()) {
            return;
        }

        for (auto sample = 0; sample < numSamples; ++sample) {
            const auto envelope = adsrGate.getNextSample();
            if (envelope <= 0.0f && !adsrGate.isActive()) {
                clearCurrentNote();
                for (auto& resonator : bodyResonators) {
                    resonator.reset();
                }
                break;
            }

            auto value = 0.0f;
            for (auto n = 0; n < numActivePartials; ++n) {
                auto& partial = partials[static_cast<std::size_t>(n)];
                auto osc = static_cast<float>(partial.sinState);
                if (partial.stringCount == 2) {
                    osc = 0.5f * (osc + static_cast<float>(partial.sinState2));
                    const auto nextCos2 = partial.cosState2 - partial.epsilon2 * partial.sinState2;
                    partial.sinState2 += partial.epsilon2 * nextCos2;
                    partial.cosState2 = nextCos2;
                } else if (partial.stringCount == 3) {
                    osc = (1.0f / 3.0f)
                        * (osc + static_cast<float>(partial.sinState2) + static_cast<float>(partial.sinState3));
                    const auto nextCos2 = partial.cosState2 - partial.epsilon2 * partial.sinState2;
                    partial.sinState2 += partial.epsilon2 * nextCos2;
                    partial.cosState2 = nextCos2;
                    const auto nextCos3 = partial.cosState3 - partial.epsilon3 * partial.sinState3;
                    partial.sinState3 += partial.epsilon3 * nextCos3;
                    partial.cosState3 = nextCos3;
                }
                value += (partial.levelFast + partial.levelSlow) * osc;
                // Magic Circle 步进（coupled form）：
                const auto nextCos = partial.cosState - partial.epsilon * partial.sinState;
                partial.sinState += partial.epsilon * nextCos;
                partial.cosState = nextCos;
                partial.levelFast *= partial.decayFastPerSample;
                partial.levelSlow *= partial.decaySlowPerSample;
            }
            const auto click = hammerTransient.getNextSample();
            const auto sampleIndex = startSample + sample;
            const auto rawOutput = value * envelope + click;

            // 16 峰物理云杉木音板模态与立体声空间辐射 (Phase 19-A/B, Bank 2010 / Chabassier 2019)
            auto resonatorLeftSum = 0.0f;
            auto resonatorRightSum = 0.0f;
            for (std::size_t i = 0; i < numResonators; ++i) {
                const auto spec = resonatorSpec(static_cast<int>(i));
                const auto resOut = bodyResonators[i].process(rawOutput);
                resonatorLeftSum += spec.weightLeft * resOut;
                resonatorRightSum += spec.weightRight * resOut;
            }
            // 琴桥立体声声像定位与非对称空间投影 (低音在左 0.20 -> 高音在右 0.80)
            const auto midi = std::clamp(static_cast<float>(currentPlayingMidiNote), 21.0f, 108.0f);
            const auto keyPos = (midi - 21.0f) / 87.0f;
            const auto directPan = 0.20f + 0.60f * keyPos;
            const auto directLeft = (1.0f - directPan) * 1.414f;
            const auto directRight = directPan * 1.414f;

            const auto wet = 0.18f + pianoResonance * 0.16f;
            const auto outLeft = (1.0f - wet) * rawOutput * directLeft + wet * resonatorLeftSum;
            const auto outRight = (1.0f - wet) * rawOutput * directRight + wet * resonatorRightSum;

            if (outputBuffer.getNumChannels() >= 2) {
                outputBuffer.addSample(0, sampleIndex, outLeft);
                outputBuffer.addSample(1, sampleIndex, outRight);
            } else if (outputBuffer.getNumChannels() == 1) {
                // 单声道设备：保持全额能量直通输出
                const auto outMono = (1.0f - wet) * rawOutput + wet * 0.5f * (resonatorLeftSum + resonatorRightSum);
                outputBuffer.addSample(0, sampleIndex, outMono);
            }
        }
        // 分音全部衰减到阈值以下（-80 dB）且瞬态击弦冲击结束即释放 voice。
        if (allPartialsSilent()) {
            clearCurrentNote();
            hammerTransient.reset();
            for (auto& resonator : bodyResonators) {
                resonator.reset();
            }
        }
    }

    // 合成参数查询（确定性测试 / 调试用）。
    [[nodiscard]] static int partialCountForNote(int midiNoteNumber) noexcept {
        return devpiano::audio::getNoteParams(midiNoteNumber).partialCount;
    }
    [[nodiscard]] static float decaySecondsForNote(int midiNoteNumber) noexcept {
        return devpiano::audio::getNoteParams(midiNoteNumber).decaySeconds;
    }
    [[nodiscard]] static float decayDampingCForNote(int midiNoteNumber) noexcept {
        return devpiano::audio::getNoteParams(midiNoteNumber).decayDampingC;
    }
    [[nodiscard]] static double inharmonicityBForNote(int midiNoteNumber) noexcept {
        return devpiano::audio::getNoteParams(midiNoteNumber).inharmonicityB;
    }
    [[nodiscard]] static float fastDecayRatioForNote(int midiNoteNumber) noexcept {
        return devpiano::audio::getNoteParams(midiNoteNumber).fastDecayRatio;
    }
    [[nodiscard]] static float slowWeightForNote(int midiNoteNumber) noexcept {
        return devpiano::audio::getNoteParams(midiNoteNumber).slowWeight;
    }
    // 慢分量（尾音）时间常数：包含空气阻尼中频下凹与高阶二次方内部摩擦。
    [[nodiscard]] static double partialDecaySeconds(int midiNoteNumber, int partialIndex, float brightness = 0.5f,
                                                    float resonance = 0.5f) noexcept {
        const auto& params = devpiano::audio::getNoteParams(midiNoteNumber);
        const auto decayScale = 1.0f + (juce::jlimit(0.0f, 1.0f, resonance) - 0.5f) * 0.6f;
        const auto baseDecay = static_cast<double>(params.decaySeconds * decayScale);
        const auto f0 = static_cast<double>(juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));
        const auto fn = partialFrequency(midiNoteNumber, partialIndex);
        const auto piOverL = juce::MathConstants<double>::pi / static_cast<double>(params.stringLength);
        const auto k1 = piOverL * piOverL;
        const auto alpha1 = static_cast<double>(params.b1) + static_cast<double>(params.b2) * k1;
        const auto m = static_cast<double>(partialIndex + 1);
        const auto kn = m * m * k1;
        const auto airTerm = std::sqrt(f0 / std::max(fn, 20.0));
        const auto alpha_n
            = static_cast<double>(params.b1) * (0.80 + 0.20 * airTerm) + static_cast<double>(params.b2) * kn;
        const auto dampingEffect
            = (alpha_n / juce::jmax(1e-9, alpha1)) * static_cast<double>(1.5f - juce::jlimit(0.0f, 1.0f, brightness));
        return baseDecay / dampingEffect;
    }
    // 快分量（击弦辐射期）时间常数：τ_fast,m = τ_slow,m × fastDecayRatio。
    [[nodiscard]] static double partialFastDecaySeconds(int midiNoteNumber, int partialIndex, float brightness = 0.5f,
                                                        float resonance = 0.5f) noexcept {
        const auto& params = devpiano::audio::getNoteParams(midiNoteNumber);
        return partialDecaySeconds(midiNoteNumber, partialIndex, brightness, resonance)
            * static_cast<double>(params.fastDecayRatio);
    }
    [[nodiscard]] static float bodyWet(float resonance = 0.5f) noexcept {
        return 0.18f + juce::jlimit(0.0f, 1.0f, resonance) * 0.16f;
    }
    [[nodiscard]] static double partialFrequency(int midiNoteNumber, int partialIndex) noexcept {
        const auto baseFrequency = static_cast<double>(juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));
        const auto partialNumber = static_cast<double>(partialIndex + 1);
        const auto b = inharmonicityBForNote(midiNoteNumber);
        return baseFrequency * partialNumber * std::sqrt(1.0 + b * partialNumber * partialNumber);
    }
    [[nodiscard]] static float beatingDetuneRatioForNote(int midiNoteNumber) noexcept {
        return devpiano::audio::getNoteParams(midiNoteNumber).beatingDetuneRatio;
    }
    [[nodiscard]] static int beatingPartialCountForNote(int midiNoteNumber) noexcept {
        return devpiano::audio::getNoteParams(midiNoteNumber).beatingPartials;
    }
    [[nodiscard]] static double beatingFrequency(int midiNoteNumber, int partialIndex) noexcept {
        const auto f = partialFrequency(midiNoteNumber, partialIndex);
        const auto& params = devpiano::audio::getNoteParams(midiNoteNumber);
        const auto isBassFundamental = (midiNoteNumber < 48 && partialIndex == 0);
        if (!isBassFundamental && partialIndex < params.beatingPartials && params.beatingDetuneRatio > 0.0f) {
            return f * (1.0 + static_cast<double>(params.beatingDetuneRatio));
        }
        return f;
    }

    [[nodiscard]] static constexpr int resonatorCount() noexcept {
        return numResonators;
    }
    struct ResonatorSpec {
        float frequency;
        float q;
        float weightLeft;
        float weightRight;
    };
    [[nodiscard]] static ResonatorSpec resonatorSpec(int index) noexcept {
        // 16 峰物理云杉木音板模态 (Bank 2010 Table II & Chabassier 2019 Sec. 3.3, 严格双声道 1.0 归一化)
        constexpr ResonatorSpec specs[numResonators] = {
            { 48.0f, 6.0f, 0.12f, 0.04f },   { 68.0f, 6.0f, 0.12f, 0.04f },   { 95.0f, 5.5f, 0.11f, 0.05f },
            { 135.0f, 5.5f, 0.10f, 0.05f },  { 185.0f, 5.0f, 0.09f, 0.06f },  { 250.0f, 4.8f, 0.08f, 0.07f },
            { 340.0f, 4.5f, 0.08f, 0.08f },  { 460.0f, 4.2f, 0.07f, 0.08f },  { 620.0f, 3.8f, 0.06f, 0.09f },
            { 820.0f, 3.5f, 0.05f, 0.09f },  { 1080.0f, 3.2f, 0.04f, 0.09f }, { 1380.0f, 3.0f, 0.03f, 0.08f },
            { 1680.0f, 2.8f, 0.02f, 0.07f }, { 1850.0f, 2.5f, 0.01f, 0.05f }, { 2050.0f, 2.4f, 0.01f, 0.03f },
            { 2250.0f, 2.2f, 0.01f, 0.03f },
        };
        const auto clamped = std::clamp(index, 0, numResonators - 1);
        return specs[clamped];
    }

    using VoiceRegion = devpiano::audio::PianoNoteParams;

    [[nodiscard]] static const VoiceRegion& regionForNote(int midiNoteNumber) noexcept {
        return devpiano::audio::getNoteParams(midiNoteNumber);
    }
    [[nodiscard]] static float strikingPositionRatioForNote(int midiNoteNumber) noexcept {
        return devpiano::audio::getNoteParams(midiNoteNumber).strikePosRatio;
    }

    // 击弦点梳状滤波增益 (Phase 17-A)：S(m) = |sin(m·π·d/L)|，底噪泄漏 0.06 防彻底无声。
    [[nodiscard]] static float strikeCombGain(int partialIndex, float strikingRatio) noexcept {
        const auto m = static_cast<float>(partialIndex + 1);
        const auto raw = std::abs(std::sin(juce::MathConstants<float>::pi * m * strikingRatio));
        return 0.06f + 0.94f * raw;
    }

    // 非线性琴槌毛毡谱形 (Phase 17-A)：消灭 1/n 锯齿波，高次幂律滚降 (1/m^1.35) + 力度指数截止。
    [[nodiscard]] static float hammerSpectrumGain(int partialIndex, float brightnessFactor) noexcept {
        const auto m = static_cast<float>(partialIndex + 1);
        const auto powerRollOff = 1.0f / std::pow(m, 1.35f);
        const auto effectiveBrightness = juce::jlimit(0.15f, 2.5f, brightnessFactor);
        const auto cutoffHarmonic = 1.5f + 16.0f * effectiveBrightness;
        const auto feltFilter = std::exp(-m / cutoffHarmonic);
        return powerRollOff * feltFilter;
    }

    // 1.8kHz Bridge Hill 琴桥共振峰增益 (Phase 18-C)
    [[nodiscard]] static float bridgeHillGain(double frequency) noexcept {
        const auto f = static_cast<float>(frequency);
        const auto diff = (f - 1800.0f) / 800.0f;
        return 1.0f + 0.40f * std::exp(-0.5f * diff * diff);
    }

    // 琴槌弹性半余弦接触调制 (Phase 18-C)
    [[nodiscard]] static float hammerElasticModulation(double partialFrequency, float tc) noexcept {
        const auto fTc = static_cast<float>(partialFrequency) * tc;
        const auto denom = 1.0f - 4.0f * fTc * fTc;
        if (std::abs(denom) < 1e-4f) {
            return 1.0f;
        }
        const auto cosineMod = std::min(std::abs(std::cos(juce::MathConstants<float>::pi * fTc) / denom), 1.0f);
        return 0.7f + 0.3f * cosineMod;
    }

    // 分音相对初始幅度（击弦梳状滤波 × 非线性琴槌谱形 × 弹性接触调制 × 琴桥共振峰）。
    [[nodiscard]] static float amplitudeFor(int partialIndex, float strikingRatio = 0.1333f,
                                            float brightnessFactor = 0.5f, double partialFrequency = 440.0,
                                            float tc = 0.0018f) noexcept {
        return strikeCombGain(partialIndex, strikingRatio) * hammerSpectrumGain(partialIndex, brightnessFactor)
            * hammerElasticModulation(partialFrequency, tc) * bridgeHillGain(partialFrequency);
    }
    // 亮度：高次谐波增益随亮度旋钮提升（基频不变，最高次 ±25%）。
    [[nodiscard]] static float brightnessBoost(int partialIndex, float brightness, int partialCount) noexcept {
        return 1.0f
            + (brightness - 0.5f) * 0.5f * (static_cast<float>(partialIndex) / static_cast<float>(partialCount));
    }

    // 击弦硬度：高次谐波起始增益（h=0.5 中性，基频不受影响，最高次 ±20%）。
    [[nodiscard]] float hammerGain(int partialIndex, int partialCount) const noexcept {
        return 1.0f
            + (pianoHammerHardness - 0.5f) * 0.4f
            * (static_cast<float>(partialIndex) / static_cast<float>(partialCount));
    }

    [[nodiscard]] bool allPartialsSilent() const noexcept {
        if (hammerTransient.isActive()) {
            return false;
        }
        for (auto n = 0; n < numActivePartials; ++n) {
            const auto& partial = partials[static_cast<std::size_t>(n)];
            if (partial.levelFast + partial.levelSlow > silentLevelThreshold) {
                return false;
            }
        }
        return true;
    }

    struct Partial {
        double cosState = 1.0;
        double sinState = 0.0;
        double epsilon = 0.0;
        double cosState2 = 1.0;
        double sinState2 = 0.0;
        double epsilon2 = 0.0;
        double cosState3 = 1.0;
        double sinState3 = 0.0;
        double epsilon3 = 0.0;
        int stringCount = 1; // 1 (Mono), 2 (Bi), 3 (Tri)
        float level = 0.0f;
        float levelFast = 0.0f;
        float levelSlow = 0.0f;
        float decayFastPerSample = 0.0f;
        float decaySlowPerSample = 0.0f;
    };
    std::array<Partial, maxPartials> partials;
    int numActivePartials = 0;
    int currentPlayingMidiNote = 60;
    juce::ADSR adsrGate;
    float pianoBrightness = 0.5f;
    float pianoHammerHardness = 0.5f;
    float pianoResonance = 0.5f;
    // 琴槌撞击瞬态冲击核 (Phase 17-B)：2~3ms 木质/毛毡物理撞击脉冲 (Click/Thump)
    struct HammerTransient {
        int samplesRemaining = 0;
        int totalSamples = 0;
        float amplitude = 0.0f;
        float decayPerSample = 0.0f;
        float oscPhase1 = 0.0f;
        float phaseInc1 = 0.0f;
        float oscPhase2 = 0.0f;
        float phaseInc2 = 0.0f;

        void trigger(double sr, int midiNoteNumber, float velocity, float hardness) noexcept {
            if (sr <= 0.0) {
                return;
            }
            const auto sampleRate = static_cast<float>(sr);
            // 冲击持续时间 1.5ms ~ 3.0ms（高音短脆，低音深沉）
            const auto dur = juce::jlimit(0.0012f, 0.0030f, 0.0030f - static_cast<float>(midiNoteNumber) * 0.000015f);
            totalSamples = juce::jmax(1, static_cast<int>(dur * sampleRate));
            samplesRemaining = totalSamples;

            // 双共振峰打击频率 (毛毡冲击 1.1~2.0 kHz + 钢丝初始震荡 2.5~4.5 kHz)
            const auto f1 = juce::jlimit(900.0f, 2200.0f, 1100.0f + static_cast<float>(midiNoteNumber) * 12.0f);
            const auto f2 = juce::jlimit(2200.0f, 4800.0f, 2600.0f + static_cast<float>(midiNoteNumber) * 18.0f);
            phaseInc1 = juce::MathConstants<float>::twoPi * f1 / sampleRate;
            phaseInc2 = juce::MathConstants<float>::twoPi * f2 / sampleRate;
            oscPhase1 = 0.0f;
            oscPhase2 = 0.0f;

            // 力度非线性响应 (v^1.6) + 击弦硬度缩放
            const auto v = juce::jlimit(0.0f, 1.0f, velocity);
            const auto vLevel = v * std::sqrt(v) * (0.6f + 0.8f * hardness);
            amplitude = peakLevelAtFullVelocity * 0.35f * vLevel;
            decayPerSample = std::exp(-4.5f / static_cast<float>(totalSamples));
        }

        [[nodiscard]] float getNextSample() noexcept {
            if (samplesRemaining <= 0) {
                return 0.0f;
            }
            --samplesRemaining;
            const auto s1 = std::sin(oscPhase1);
            const auto s2 = std::sin(oscPhase2);
            oscPhase1 += phaseInc1;
            oscPhase2 += phaseInc2;
            const auto out = amplitude * (0.6f * s1 + 0.4f * s2);
            amplitude *= decayPerSample;
            return out;
        }

        void reset() noexcept {
            samplesRemaining = 0;
            amplitude = 0.0f;
        }

        [[nodiscard]] bool isActive() const noexcept {
            return samplesRemaining > 0;
        }
    };
    HammerTransient hammerTransient;
    struct BodyResonator {
        float frequency = 110.0f;
        float q = 6.0f;
        float weight = 0.40f;
        float c1 = 0.0f;
        float c2 = 0.0f;
        float g = 0.0f;
        float s1 = 0.0f;
        float s2 = 0.0f;

        void updateCoefficients(double sampleRate) noexcept {
            if (sampleRate <= 0.0) {
                return;
            }
            const auto theta = juce::MathConstants<double>::twoPi * static_cast<double>(frequency) / sampleRate;
            const auto bandwidth = static_cast<double>(frequency / q);
            const auto r = std::exp(-juce::MathConstants<double>::pi * bandwidth / sampleRate);
            c1 = static_cast<float>(2.0 * r * std::cos(theta));
            c2 = static_cast<float>(-r * r);
            g = static_cast<float>((1.0 - r * r) * 0.5);
        }

        void reset() noexcept {
            s1 = 0.0f;
            s2 = 0.0f;
        }

        [[nodiscard]] float process(float in) noexcept {
            const auto w = in + c1 * s1 + c2 * s2;
            const auto y = g * (w - s2);
            s2 = s1;
            s1 = w;
            return y;
        }
    };
    std::array<BodyResonator, numResonators> bodyResonators = [] {
        std::array<BodyResonator, numResonators> array {};
        for (std::size_t i = 0; i < numResonators; ++i) {
            const auto spec = resonatorSpec(static_cast<int>(i));
            array[i].frequency = spec.frequency;
            array[i].q = spec.q;
            array[i].weight = spec.weightLeft;
        }
        return array;
    }();
};
