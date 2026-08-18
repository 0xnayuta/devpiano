#pragma once

#include <JuceHeader.h>

#include <array>
#include <cmath>

// 内置谐波钢琴合成器（Phase 12-2）：与 SineSynthVoice 并存的第二套内置音色。
// 与 SineSynthVoice 相同的承载方式（继承 juce::SynthesiserVoice），由
// juce::Synthesiser 管理 voice 生命周期；AudioEngine::setBuiltinSynthTone
// 切换实时路径注册的音色（默认仍为 sine，Piano 可切换，后续阶段切默认）。
//
// 合成模型（v1，纯加法、零采样依赖）：
// - 基频 + 2~7 次谐波叠加，分音数与谐波幅度按音区查表（低音区谐波丰富、
//   高音区收敛），幅度归一避免 clip；
// - velocity 双映射：响度 level = v^1.5（弱奏更敏感）+ 亮度（高次谐波增益
//   随 v 提升）；
// - 分音独立指数衰减：decay 按音区（低音长、高音短），高次分音略快；
//   attack/release 沿用 AudioEngine::setAdsr 接线（经 setAdsrParameters
//   提取 attack/release 作门控，decay/sustain 由分音衰减替代）。
//
// 实时约束：renderNextBlock 无堆分配、无锁；每分音一次 std::sin（≤8 次），
// startNote 内每 note 一次 std::sqrt/std::exp（实时安全，不做逐采样 pow）。

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
    static constexpr auto peakLevelAtFullVelocity = 0.28f;
    static constexpr auto silentLevelThreshold = 1e-4f;

    bool canPlaySound(juce::SynthesiserSound* sound) override {
        return dynamic_cast<PianoSynthSound*>(sound) != nullptr;
    }

    // 只取 attack/release 作门控；decay/sustain 由分音独立衰减替代（Phase 12-2）。
    void setAdsrParameters(const juce::ADSR::Parameters& parameters) {
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
            partial.increment
                = juce::MathConstants<double>::twoPi * baseFrequency * static_cast<double>(n + 1) / sampleRate;
            partial.level = amplitudeFor(n) * brightnessBoost(n, brightnessFactor, numActivePartials)
                * hammerGain(n, numActivePartials) * scale * velocityLevel;
            partial.decayPerSample = static_cast<float>(std::exp(
                -1.0 / (static_cast<double>(region.decaySeconds * decayScale * harmonicDecayFactor(n)) * sampleRate)));
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

private:
    struct VoiceRegion {
        int partialCount;
        float decaySeconds;
    };

    static constexpr VoiceRegion voiceRegions[] = {
        { 8, 4.0f }, // note < 48：7 次谐波，长衰减（低音弦余音长）
        { 6, 2.5f }, // 48–71：5 次谐波
        { 4, 1.5f }, // 72–95：3 次谐波
        { 3, 0.8f }, // ≥ 96：2 次谐波，短衰减（高音收敛）
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

    // 分音独立衰减：高次分音衰减略快（v1 简单因子表；精细曲线建模在 Phase 13）。
    [[nodiscard]] static float harmonicDecayFactor(int partialIndex) noexcept {
        constexpr float factors[] = { 1.0f, 0.85f, 0.72f, 0.62f, 0.55f, 0.50f, 0.46f, 0.43f };
        return factors[partialIndex];
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
