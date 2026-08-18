#pragma once

#include <JuceHeader.h>

#include <array>
#include <cmath>

// 内置谐波钢琴合成器（Phase 12-2）：与 SineSynthVoice 并存的第二套内置音色。
// 与 SineSynthVoice 相同的承载方式（继承 juce::SynthesiserVoice），由
// juce::Synthesiser 管理 voice 生命周期；AudioEngine::setBuiltinSynthTone
// 切换实时路径注册的音色（默认仍为 sine，Piano 可切换，后续阶段切默认）。
//
// 合成模型（纯加法、零采样依赖）：
// - 分音叠加与刚性琴弦非谐性（Phase 13-1）：基频 + 高次分音，各分音频率按 JOS PASP
//   刚性琴弦公式计算 f_m = m·f₀·√(1+B·m²)，刚度系数 B 按音区查表（低音弦粗 B≈4e-4，
//   高音 B≈1e-5），各分音失去公共整数倍周期，产生自然的泛音拍频（Beats）；
// - 模态分音衰减速率建模（Phase 13-2）：基于 Mutable Instruments 模态能量耗散模型，
//   分音时间常数 τ_m = τ_base / (1.0 + c_eff · (m - 1))，高次分音快速耗散，音色由击弦瞬间
//   的丰富泛音自然过渡至基频主导的纯净尾音；pianoResonance 调节 τ_base，pianoBrightness
//   微调高频阻尼斜率；
// - velocity 双映射：响度 level = v^1.5（弱奏更敏感）+ 亮度（高次谐波增益随 v 提升）；
// - attack/release 沿用 AudioEngine::setAdsr 接线（经 setAdsrParameters
//   提取 attack/release 作门控，decay/sustain 由分音衰减替代）。
// 实时约束：renderNextBlock 无堆分配、无锁；每分音一次 std::sin（≤8 次），
// startNote 内每 note 做 O(partials) 次 std::sqrt/std::exp（按键瞬间计算，逐采样
// 渲染 CPU 零新增）。
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
    static constexpr auto maxPartials = 8;
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
        adsrGate.setParameters({ parameters.attack, 0.001f, 1.0f, parameters.release });
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
        const auto& region = regionForNote(midiNoteNumber);
        const auto baseFrequency = static_cast<double>(juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));

        numActivePartials = region.partialCount;

        // velocity 响度：v^1.5（弱奏更敏感），用 sqrt 避免 std::pow。
        const auto velocityLevel = velocity * std::sqrt(velocity);
        // velocity 亮度：随力度线性提升；pianoBrightness 调节亮度基准
        // （b=0.5 时 factor = velocity，与 12-2 一致）。
        const auto brightnessFactor = velocity * (0.5f + pianoBrightness);
        // 共鸣：衰减时间缩放（r=0.5 中性，r=1 → ×1.3，r=0 → ×0.7）。
        const auto decayScale = 1.0f + (pianoResonance - 0.5f) * 0.6f;
        const auto baseDecaySeconds = static_cast<double>(region.decaySeconds * decayScale);
        // 高频衰减阻尼：pianoBrightness 微调（b=0.5 时为 1.0，暗时阻尼更大衰减更快，亮时阻尼略小）
        const auto dampingSlope = region.decayDampingC * (1.5f - pianoBrightness);

        // 归一化：按当前亮度因子求和，使 v=1 时峰值恒为 peakLevelAtFullVelocity。
        auto normSum = 0.0f;
        for (auto n = 0; n < numActivePartials; ++n) {
            normSum += amplitudeFor(n) * brightnessBoost(n, brightnessFactor, numActivePartials)
                * hammerGain(n, numActivePartials);
        }
        const auto scale = peakLevelAtFullVelocity / juce::jmax(1e-6f, normSum);

        for (auto n = 0; n < numActivePartials; ++n) {
            auto& partial = partials[static_cast<std::size_t>(n)];
            partial.phase = 0.0;
            const auto partialNumber = static_cast<double>(n + 1);
            const auto inharmonicFactor = std::sqrt(1.0 + region.inharmonicityB * partialNumber * partialNumber);
            const auto partialFrequency = baseFrequency * partialNumber * inharmonicFactor;
            partial.increment = juce::MathConstants<double>::twoPi * partialFrequency / sampleRate;
            partial.level = amplitudeFor(n) * brightnessBoost(n, brightnessFactor, numActivePartials)
                * hammerGain(n, numActivePartials) * scale * velocityLevel;
            // 模态能量耗散模型：τ_m = τ_base / (1.0 + c_eff * (m - 1))
            const auto tau_m = baseDecaySeconds / (1.0 + static_cast<double>(dampingSlope * static_cast<float>(n)));
            partial.decayPerSample = static_cast<float>(std::exp(-1.0 / (tau_m * sampleRate)));
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
                break;
            }

            auto value = 0.0f;
            for (auto n = 0; n < numActivePartials; ++n) {
                auto& partial = partials[static_cast<std::size_t>(n)];
                value += partial.level * static_cast<float>(std::sin(partial.phase));
                partial.phase += partial.increment;
                if (partial.phase >= juce::MathConstants<double>::twoPi) {
                    partial.phase -= juce::MathConstants<double>::twoPi;
                }
                partial.level *= partial.decayPerSample;
            }

            const auto sampleIndex = startSample + sample;
            const auto output = value * envelope;
            for (auto channel = 0; channel < outputBuffer.getNumChannels(); ++channel) {
                outputBuffer.addSample(channel, sampleIndex, output);
            }
        }

        // 分音全部衰减到阈值以下（-80 dB）即释放 voice，避免低音长尾长期占位。
        if (allPartialsSilent()) {
            clearCurrentNote();
        }
    }

    // 合成参数查询（确定性测试 / 调试用）。
    [[nodiscard]] static int partialCountForNote(int midiNoteNumber) noexcept {
        return regionForNote(midiNoteNumber).partialCount;
    }
    [[nodiscard]] static float decaySecondsForNote(int midiNoteNumber) noexcept {
        return regionForNote(midiNoteNumber).decaySeconds;
    }
    [[nodiscard]] static double inharmonicityBForNote(int midiNoteNumber) noexcept {
        return regionForNote(midiNoteNumber).inharmonicityB;
    }
    [[nodiscard]] static float decayDampingCForNote(int midiNoteNumber) noexcept {
        return regionForNote(midiNoteNumber).decayDampingC;
    }
    [[nodiscard]] static double partialFrequency(int midiNoteNumber, int partialIndex) noexcept {
        const auto baseFrequency = static_cast<double>(juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));
        const auto partialNumber = static_cast<double>(partialIndex + 1);
        const auto b = inharmonicityBForNote(midiNoteNumber);
        return baseFrequency * partialNumber * std::sqrt(1.0 + b * partialNumber * partialNumber);
    }
    [[nodiscard]] static double partialDecaySeconds(int midiNoteNumber, int partialIndex, float brightness = 0.5f,
                                                    float resonance = 0.5f) noexcept {
        const auto& region = regionForNote(midiNoteNumber);
        const auto decayScale = 1.0f + (juce::jlimit(0.0f, 1.0f, resonance) - 0.5f) * 0.6f;
        const auto baseDecay = static_cast<double>(region.decaySeconds * decayScale);
        const auto dampingSlope = region.decayDampingC * (1.5f - juce::jlimit(0.0f, 1.0f, brightness));
        return baseDecay / (1.0 + static_cast<double>(dampingSlope * static_cast<float>(partialIndex)));
    }

private:
    struct VoiceRegion {
        int partialCount;
        float decaySeconds;
        double inharmonicityB;
        float decayDampingC;
    };

    static constexpr VoiceRegion voiceRegions[] = {
        { 8, 4.0f, 4.0e-4, 0.35f }, // note < 48：低音弦长，高次衰减阻尼斜率 c = 0.35
        { 6, 2.5f, 1.0e-4, 0.25f }, // 48–71：中音弦 c = 0.25
        { 4, 1.5f, 3.0e-5, 0.18f }, // 72–95：高音弦 c = 0.18
        { 3, 0.8f, 1.0e-5, 0.12f }, // ≥ 96：极高音弦 c = 0.12
    };
    [[nodiscard]] static const VoiceRegion& regionForNote(int midiNoteNumber) noexcept {
        if (midiNoteNumber < 48) {
            return voiceRegions[0];
        }
        if (midiNoteNumber < 72) {
            return voiceRegions[1];
        }
        if (midiNoteNumber < 96) {
            return voiceRegions[2];
        }
        return voiceRegions[3];
    }

    // 分音相对幅度：基频 1，第 n 次谐波 1/(n+1)。
    [[nodiscard]] static float amplitudeFor(int partialIndex) noexcept {
        return 1.0f / static_cast<float>(partialIndex + 1);
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
        for (auto n = 0; n < numActivePartials; ++n) {
            if (partials[static_cast<std::size_t>(n)].level > silentLevelThreshold) {
                return false;
            }
        }
        return true;
    }

    struct Partial {
        double phase = 0.0;
        double increment = 0.0;
        float level = 0.0f;
        float decayPerSample = 0.0f;
    };

    std::array<Partial, maxPartials> partials;
    int numActivePartials = 0;
    juce::ADSR adsrGate;
    float pianoBrightness = 0.5f;
    float pianoHammerHardness = 0.5f;
    float pianoResonance = 0.5f;
};
