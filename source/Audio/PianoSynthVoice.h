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
    static constexpr auto numResonators = 8;
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

        const auto& params = devpiano::audio::getNoteParams(midiNoteNumber);
        const auto baseFrequency = static_cast<double>(juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));

        numActivePartials = params.partialCount;

        // velocity 响度：v^1.5（弱奏更敏感），用 sqrt 避免 std::pow。
        const auto clampedVelocity = juce::jlimit(0.0f, 1.0f, velocity);
        const auto velocityLevel = clampedVelocity * std::sqrt(clampedVelocity);
        // velocity 亮度：随力度线性提升；pianoBrightness 调节亮度基准
        // （b=0.5 时 factor = velocity，与 12-2 一致）。
        const auto brightnessFactor = clampedVelocity * (0.5f + pianoBrightness);
        // 共鸣：衰减时间缩放（r=0.5 中性，r=1 → ×1.3，r=0 → ×0.7）。
        const auto decayScale = 1.0f + (pianoResonance - 0.5f) * 0.6f;
        const auto baseDecaySeconds = static_cast<double>(params.decaySeconds * decayScale);
        // 高频衰减阻尼：pianoBrightness 微调（b=0.5 时为 1.0，暗时阻尼更大衰减更快，亮时阻尼略小）
        const auto dampingSlope = params.decayDampingC * (1.5f - pianoBrightness);

        // 归一化：按当前击弦位置与非线性谱形求和，使 v=1 时峰值恒为 peakLevelAtFullVelocity。
        auto normSum = 0.0f;
        for (auto n = 0; n < numActivePartials; ++n) {
            normSum += amplitudeFor(n, params.strikePosRatio, brightnessFactor) * hammerGain(n, numActivePartials);
        }
        const auto scale = peakLevelAtFullVelocity / juce::jmax(1e-6f, normSum);

        const auto nyquistLimit = sampleRate * 0.495;

        for (auto n = 0; n < numActivePartials; ++n) {
            auto& partial = partials[static_cast<std::size_t>(n)];
            const auto phase1 = devpiano::audio::kOptPhaseTable[0][static_cast<std::size_t>(n % 64)];
            partial.cosState = std::cos(static_cast<double>(phase1));
            partial.sinState = std::sin(static_cast<double>(phase1));
            const auto partialNumber = static_cast<double>(n + 1);
            const auto inharmonicFactor = std::sqrt(1.0 + params.inharmonicityB * partialNumber * partialNumber);
            const auto partialFrequency = baseFrequency * partialNumber * inharmonicFactor;

            if (partialFrequency >= nyquistLimit) {
                partial.level = 0.0f;
                partial.levelFast = 0.0f;
                partial.levelSlow = 0.0f;
                partial.epsilon = 0.0;
                partial.hasBeating = false;
                partial.epsilon2 = 0.0;
                partial.decayFastPerSample = 0.0f;
                partial.decaySlowPerSample = 0.0f;
                continue;
            }

            // Magic Circle 步进系数：ε = 2·sin(π·f/fs) 使 coupled form 精确振荡于
            partial.epsilon = 2.0 * std::sin(juce::MathConstants<double>::pi * partialFrequency / sampleRate);

            // 同音双振荡器微失谐（Phase 14-C）：对中高音分音及低音泛音设置第二根弦的频率
            // （低音基频保持单振荡器锁定音高，泛音与中高音呈现自然拍频）
            const auto isBassFundamental = (midiNoteNumber < 48 && n == 0);
            if (!isBassFundamental && n < params.beatingPartials && params.beatingDetuneRatio > 0.0f) {
                const auto detunedFreq = partialFrequency * (1.0 + static_cast<double>(params.beatingDetuneRatio));
                if (detunedFreq < nyquistLimit) {
                    partial.hasBeating = true;
                    const auto phase2 = devpiano::audio::kOptPhaseTable[1][static_cast<std::size_t>(n % 64)];
                    partial.cosState2 = std::cos(static_cast<double>(phase2));
                    partial.sinState2 = std::sin(static_cast<double>(phase2));
                    partial.epsilon2 = 2.0 * std::sin(juce::MathConstants<double>::pi * detunedFreq / sampleRate);
                } else {
                    partial.hasBeating = false;
                    partial.cosState2 = 1.0;
                    partial.sinState2 = 0.0;
                    partial.epsilon2 = 0.0;
                }
            } else {
                partial.hasBeating = false;
                partial.cosState2 = 1.0;
                partial.sinState2 = 0.0;
                partial.epsilon2 = 0.0;
            }

            partial.level = amplitudeFor(n, params.strikePosRatio, brightnessFactor) * hammerGain(n, numActivePartials)
                * scale * velocityLevel;
            // 模态能量耗散模型：τ_m = τ_base / (1.0 + c_eff * (m - 1))；双阶段衰减
            // （Phase 14-B）：τ_fast,m = τ_m × fastDecayRatio，慢分量权重 slowWeight。
            const auto tau_m = baseDecaySeconds / (1.0 + static_cast<double>(dampingSlope * static_cast<float>(n)));
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
                if (partial.hasBeating) {
                    osc = 0.5f * (osc + static_cast<float>(partial.sinState2));
                    const auto nextCos2 = partial.cosState2 - partial.epsilon2 * partial.sinState2;
                    partial.sinState2 += partial.epsilon2 * nextCos2;
                    partial.cosState2 = nextCos2;
                }
                value += (partial.levelFast + partial.levelSlow) * osc;
                // Magic Circle 步进（coupled form）：
                // u[n] = u[n-1] - ε·v[n-1]；v[n] = v[n-1] + ε·u[n]。
                const auto nextCos = partial.cosState - partial.epsilon * partial.sinState;
                partial.sinState += partial.epsilon * nextCos;
                partial.cosState = nextCos;
                partial.levelFast *= partial.decayFastPerSample;
                partial.levelSlow *= partial.decaySlowPerSample;
            }
            const auto click = hammerTransient.getNextSample();
            const auto sampleIndex = startSample + sample;
            const auto rawOutput = value * envelope + click;
            // 琴体共鸣滤波（Phase 13-3）：并联谐振器组与 Wet/Dry 混合
            auto resonatorSum = 0.0f;
            for (auto& resonator : bodyResonators) {
                resonatorSum += resonator.weight * resonator.process(rawOutput);
            }
            const auto wet = 0.18f + pianoResonance * 0.16f;
            const auto output = (1.0f - wet) * rawOutput + wet * resonatorSum;

            for (auto channel = 0; channel < outputBuffer.getNumChannels(); ++channel) {
                outputBuffer.addSample(channel, sampleIndex, output);
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
    // 慢分量（尾音）时间常数：τ_slow,m = τ_base / (1 + c_eff·(m-1))（Phase 14-B 后 decaySeconds 的语义）。
    [[nodiscard]] static double partialDecaySeconds(int midiNoteNumber, int partialIndex, float brightness = 0.5f,
                                                    float resonance = 0.5f) noexcept {
        const auto& params = devpiano::audio::getNoteParams(midiNoteNumber);
        const auto decayScale = 1.0f + (juce::jlimit(0.0f, 1.0f, resonance) - 0.5f) * 0.6f;
        const auto baseDecay = static_cast<double>(params.decaySeconds * decayScale);
        const auto dampingSlope = params.decayDampingC * (1.5f - juce::jlimit(0.0f, 1.0f, brightness));
        return baseDecay / (1.0 + static_cast<double>(dampingSlope * static_cast<float>(partialIndex)));
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
        float weight;
    };
    [[nodiscard]] static ResonatorSpec resonatorSpec(int index) noexcept {
        constexpr ResonatorSpec specs[] = {
            { 68.0f, 5.5f, 0.16f },  { 112.0f, 5.5f, 0.17f }, { 175.0f, 5.0f, 0.15f }, { 245.0f, 4.5f, 0.14f },
            { 350.0f, 4.0f, 0.12f }, { 490.0f, 3.8f, 0.10f }, { 720.0f, 3.2f, 0.09f }, { 1050.0f, 2.8f, 0.07f },
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

    // 分音相对初始幅度（击弦梳状滤波 × 非线性琴槌谱形）。
    [[nodiscard]] static float amplitudeFor(int partialIndex, float strikingRatio = 0.1333f,
                                            float brightnessFactor = 0.5f) noexcept {
        return strikeCombGain(partialIndex, strikingRatio) * hammerSpectrumGain(partialIndex, brightnessFactor);
    }
    // 亮度：高次谐波增益随亮度因子提升（基频不变，最高次 +60%）。
    [[nodiscard]] static float brightnessBoost(int partialIndex, float brightness, int partialCount) noexcept {
        return 1.0f + brightness * (static_cast<float>(partialIndex) / static_cast<float>(partialCount)) * 0.6f;
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
        double cosState = 1.0; // Magic Circle 余弦状态 u[n]（初值 cos(0) = 1）
        double sinState = 0.0; // Magic Circle 正弦状态 v[n]（初值 sin(0) = 0，渲染取此值）
        double epsilon = 0.0; // 步进系数 ε = 2·sin(π·f/fs)，startNote 计算
        double cosState2 = 1.0; // 第二振荡器余弦状态 u2[n]（初值 1）
        double sinState2 = 0.0; // 第二振荡器正弦状态 v2[n]（初值 0）
        double epsilon2 = 0.0; // 第二振荡器步进系数 ε2 = 2·sin(π·f2/fs)
        bool hasBeating = false; // 是否启用同音双振荡器拍频
        float level = 0.0f; // 初始总幅度（归一化后，仅 startNote 用）
        float levelFast = 0.0f; // 快衰减分量（击弦辐射期）
        float levelSlow = 0.0f; // 慢衰减分量（弱耦合尾音）
        float decayFastPerSample = 0.0f;
        float decaySlowPerSample = 0.0f;
    };
    std::array<Partial, maxPartials> partials;
    int numActivePartials = 0;
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

    std::array<BodyResonator, numResonators> bodyResonators = {
        BodyResonator { 75.0f, 6.0f, 0.18f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        BodyResonator { 110.0f, 6.0f, 0.18f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        BodyResonator { 160.0f, 5.5f, 0.15f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        BodyResonator { 220.0f, 5.0f, 0.14f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        BodyResonator { 320.0f, 4.5f, 0.11f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        BodyResonator { 460.0f, 4.0f, 0.09f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        BodyResonator { 680.0f, 3.5f, 0.08f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        BodyResonator { 950.0f, 3.0f, 0.07f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    };
};
