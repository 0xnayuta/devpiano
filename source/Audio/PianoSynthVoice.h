#pragma once

#include <JuceHeader.h>

#include "Piano88KeyTable.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

// 内置物理建模钢琴合成器（Phase 12~20）：
// - 88 键物理参数化 (Phase 18-A/B)：Steinway B 刚性失谐、Bensa 实测阻尼、STFT 最优微相位矩阵；
// - 空气黏性阻尼与二次方摩擦 (Phase 18-C)：Desvages & Bilbao (2016) 模态耗散模型，呈现中频下凹歌唱性；
// - 16 峰正交云杉木音板模态 (Phase 19-A)：Bank 2010 / Chabassier 2019 实测物理模态分布；
// - 琴桥立体声空间辐射 (Phase 19-B)：根据 88 键物理跨度分配声像，消灭单声道耳膜居中压迫感；
// - 同音三弦独立三振荡器非对称拍频 (Phase 19-C)：3 弦独立微失谐与 STFT 空间初相；
// - 低音钢弦纵波先驱脉冲 (Phase 20-A, Bank 2005/2010)：v_L ≈ 5100 m/s 极短金属撞击先导声；
// - 机械击弦微观混沌微扰 (Phase 20-B, Bank & Chabassier 2019)：消除同音轮指的机械克隆感。

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
    static constexpr auto bodyWetRatio = 0.26f;
    static constexpr auto peakLevelAtFullVelocity = 0.16f;
    static constexpr auto silentLevelThreshold = 1e-4f;

    bool canPlaySound(juce::SynthesiserSound* sound) override {
        return dynamic_cast<PianoSynthSound*>(sound) != nullptr;
    }

    void setAdsrParameters(const juce::ADSR::Parameters& parameters) {
        if (getSampleRate() > 0.0) {
            adsrGate.setSampleRate(getSampleRate());
        }
        const auto attackSec = juce::jmin(0.0002f, parameters.attack);
        adsrGate.setParameters({ attackSec, 0.001f, 1.0f, parameters.release });
    }

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

        const auto clampedVelocity = juce::jlimit(0.0f, 1.0f, velocity);
        const auto velocityLevel = clampedVelocity * std::sqrt(clampedVelocity);
        const auto brightnessFactor = clampedVelocity * (0.5f + pianoBrightness);
        const auto decayScale = 1.0f + (pianoResonance - 0.5f) * 0.6f;
        const auto baseDecaySeconds = static_cast<double>(params.decaySeconds * decayScale);

        const auto effectiveHardness
            = 0.15f + 0.85f * std::pow(clampedVelocity, 1.5f) * (0.5f + 0.5f * pianoHammerHardness);
        const auto tc = params.tcBase * (2.5f - 1.9f * effectiveHardness);

        // 击弦微观混沌微扰引擎 (Phase 20-B, Bank & Chabassier 2019 Sec. 4)
        auto rngState = static_cast<std::uint32_t>(midiNoteNumber) * 1009u + (++triggerCounter) * 1013u
            + static_cast<std::uint32_t>(clampedVelocity * 1000.0f);
        auto hashJitter = [](std::uint32_t& s) noexcept -> float {
            s = s * 1664525u + 1013904223u;
            return static_cast<float>(s >> 16) / 65535.0f * 2.0f - 1.0f;
        };
        const auto jitterStrike = 1.0f + 0.006f * hashJitter(rngState);
        const auto jitterTc = 1.0f + 0.008f * hashJitter(rngState);
        const auto jitterPhase = 0.012f * hashJitter(rngState);

        const auto effectiveStrikePos = params.strikePosRatio * jitterStrike;
        const auto effectiveTc = tc * jitterTc;

        const auto piOverL = juce::MathConstants<double>::pi / static_cast<double>(params.stringLength);
        const auto k1 = piOverL * piOverL;
        const auto alpha1 = static_cast<double>(params.b1) + static_cast<double>(params.b2) * k1;

        auto normSum = 0.0f;
        for (auto n = 0; n < numActivePartials; ++n) {
            const auto partialNumber = static_cast<double>(n + 1);
            const auto inharmonicFactor = std::sqrt(1.0 + params.inharmonicityB * partialNumber * partialNumber);
            const auto partialFrequency = baseFrequency * partialNumber * inharmonicFactor;
            normSum += amplitudeFor(n, effectiveStrikePos, brightnessFactor, partialFrequency, effectiveTc)
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

            partial.stringCount = params.stringCount;
            const auto phase1 = devpiano::audio::kOptPhaseTable[0][static_cast<std::size_t>(n % 64)] + jitterPhase;
            const auto phase2 = devpiano::audio::kOptPhaseTable[1][static_cast<std::size_t>(n % 64)] + jitterPhase;
            const auto phase3 = devpiano::audio::kOptPhaseTable[2][static_cast<std::size_t>(n % 64)] + jitterPhase;

            if (partial.stringCount == 1 || n >= params.beatingPartials || params.beatingDetuneRatio <= 0.0f) {
                partial.stringCount = 1;
                partial.cosState = std::cos(static_cast<double>(phase1));
                partial.sinState = std::sin(static_cast<double>(phase1));
                partial.epsilon = 2.0 * std::sin(juce::MathConstants<double>::pi * partialFrequency / sampleRate);
                partial.epsilon2 = 0.0;
                partial.epsilon3 = 0.0;
            } else if (partial.stringCount == 2) {
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

            partial.level = amplitudeFor(n, effectiveStrikePos, brightnessFactor, partialFrequency, effectiveTc)
                * hammerGain(n, numActivePartials) * brightnessBoost(n, pianoBrightness, numActivePartials) * scale
                * velocityLevel;

            const auto kn = m * m * k1;
            const auto airTerm = std::sqrt(baseFrequency / std::max(partialFrequency, 20.0));
            const auto alpha_n
                = static_cast<double>(params.b1) * (0.80 + 0.20 * airTerm) + static_cast<double>(params.b2) * kn;

            const auto dampingEffect
                = (alpha_n / juce::jmax(1e-9, alpha1)) * static_cast<double>(1.5f - pianoBrightness);
            const auto tau_m = baseDecaySeconds / dampingEffect;
            const auto tauFast_m = tau_m * static_cast<double>(params.fastDecayRatio);
            partial.decayFastPerSample = static_cast<float>(std::exp(-1.0 / (tauFast_m * sampleRate)));
            partial.decaySlowPerSample = static_cast<float>(std::exp(-1.0 / (tau_m * sampleRate)));
            partial.levelFast = partial.level * (1.0f - params.slowWeight);
            partial.levelSlow = partial.level * params.slowWeight;
        }

        hammerTransient.trigger(sampleRate, midiNoteNumber, clampedVelocity, pianoHammerHardness, params.stringLength);
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
                const auto nextCos = partial.cosState - partial.epsilon * partial.sinState;
                partial.sinState += partial.epsilon * nextCos;
                partial.cosState = nextCos;
                partial.levelFast *= partial.decayFastPerSample;
                partial.levelSlow *= partial.decaySlowPerSample;
            }
            const auto click = hammerTransient.getNextSample();
            const auto sampleIndex = startSample + sample;
            const auto rawOutput = value * envelope + click;

            auto resonatorLeftSum = 0.0f;
            auto resonatorRightSum = 0.0f;
            for (std::size_t i = 0; i < numResonators; ++i) {
                const auto spec = resonatorSpec(static_cast<int>(i));
                const auto resOut = bodyResonators[i].process(rawOutput);
                resonatorLeftSum += spec.weightLeft * resOut;
                resonatorRightSum += spec.weightRight * resOut;
            }

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
                const auto outMono = (1.0f - wet) * rawOutput + wet * 0.5f * (resonatorLeftSum + resonatorRightSum);
                outputBuffer.addSample(0, sampleIndex, outMono);
            }
        }

        if (allPartialsSilent()) {
            clearCurrentNote();
            hammerTransient.reset();
            for (auto& resonator : bodyResonators) {
                resonator.reset();
            }
        }
    }

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

    [[nodiscard]] static float strikeCombGain(int partialIndex, float strikingRatio) noexcept {
        const auto m = static_cast<float>(partialIndex + 1);
        const auto raw = std::abs(std::sin(juce::MathConstants<float>::pi * m * strikingRatio));
        return 0.06f + 0.94f * raw;
    }

    [[nodiscard]] static float hammerSpectrumGain(int partialIndex, float brightnessFactor) noexcept {
        const auto m = static_cast<float>(partialIndex + 1);
        const auto powerRollOff = 1.0f / std::pow(m, 1.35f);
        const auto effectiveBrightness = juce::jlimit(0.15f, 2.5f, brightnessFactor);
        const auto cutoffHarmonic = 1.5f + 16.0f * effectiveBrightness;
        const auto feltFilter = std::exp(-m / cutoffHarmonic);
        return powerRollOff * feltFilter;
    }

    [[nodiscard]] static float bridgeHillGain(double frequency) noexcept {
        const auto f = static_cast<float>(frequency);
        const auto diff = (f - 1800.0f) / 800.0f;
        return 1.0f + 0.40f * std::exp(-0.5f * diff * diff);
    }

    [[nodiscard]] static float hammerElasticModulation(double partialFrequency, float tc) noexcept {
        const auto fTc = static_cast<float>(partialFrequency) * tc;
        const auto denom = 1.0f - 4.0f * fTc * fTc;
        if (std::abs(denom) < 1e-4f) {
            return 1.0f;
        }
        const auto cosineMod = std::min(std::abs(std::cos(juce::MathConstants<float>::pi * fTc) / denom), 1.0f);
        return 0.7f + 0.3f * cosineMod;
    }

    [[nodiscard]] static float amplitudeFor(int partialIndex, float strikingRatio = 0.1333f,
                                            float brightnessFactor = 0.5f, double partialFrequency = 440.0,
                                            float tc = 0.0018f) noexcept {
        return strikeCombGain(partialIndex, strikingRatio) * hammerSpectrumGain(partialIndex, brightnessFactor)
            * hammerElasticModulation(partialFrequency, tc) * bridgeHillGain(partialFrequency);
    }

    [[nodiscard]] static float brightnessBoost(int partialIndex, float brightness, int partialCount) noexcept {
        return 1.0f
            + (brightness - 0.5f) * 0.5f * (static_cast<float>(partialIndex) / static_cast<float>(partialCount));
    }

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
        int stringCount = 1;
        float level = 0.0f;
        float levelFast = 0.0f;
        float levelSlow = 0.0f;
        float decayFastPerSample = 0.0f;
        float decaySlowPerSample = 0.0f;
    };
    std::array<Partial, maxPartials> partials;
    int numActivePartials = 0;
    int currentPlayingMidiNote = 60;
    std::uint32_t triggerCounter = 0;
    juce::ADSR adsrGate;
    float pianoBrightness = 0.5f;
    float pianoHammerHardness = 0.5f;
    float pianoResonance = 0.5f;

    struct HammerTransient {
        int samplesRemaining = 0;
        int totalSamples = 0;
        float amplitude = 0.0f;
        float decayPerSample = 0.0f;
        float oscPhase1 = 0.0f;
        float phaseInc1 = 0.0f;
        float oscPhase2 = 0.0f;
        float phaseInc2 = 0.0f;

        int longSamplesRemaining = 0;
        float longAmplitude = 0.0f;
        float longDecayPerSample = 0.0f;
        float longPhase1 = 0.0f;
        float longPhaseInc1 = 0.0f;
        float longPhase2 = 0.0f;
        float longPhaseInc2 = 0.0f;
        float longPhase3 = 0.0f;
        float longPhaseInc3 = 0.0f;

        void trigger(double sr, int midiNoteNumber, float velocity, float hardness,
                     float stringLength = 1.0f) noexcept {
            if (sr <= 0.0) {
                return;
            }
            const auto sampleRate = static_cast<float>(sr);
            const auto dur = juce::jlimit(0.0012f, 0.0030f, 0.0030f - static_cast<float>(midiNoteNumber) * 0.000015f);
            totalSamples = juce::jmax(1, static_cast<int>(dur * sampleRate));
            samplesRemaining = totalSamples;

            const auto f1 = juce::jlimit(900.0f, 2200.0f, 1100.0f + static_cast<float>(midiNoteNumber) * 12.0f);
            const auto f2 = juce::jlimit(2200.0f, 4800.0f, 2600.0f + static_cast<float>(midiNoteNumber) * 18.0f);
            phaseInc1 = juce::MathConstants<float>::twoPi * f1 / sampleRate;
            phaseInc2 = juce::MathConstants<float>::twoPi * f2 / sampleRate;
            oscPhase1 = 0.0f;
            oscPhase2 = 0.0f;

            const auto v = juce::jlimit(0.0f, 1.0f, velocity);
            const auto vLevel = v * std::sqrt(v) * (0.6f + 0.8f * hardness);
            amplitude = peakLevelAtFullVelocity * 0.35f * vLevel;
            decayPerSample = std::exp(-4.5f / static_cast<float>(totalSamples));

            if (midiNoteNumber < 56) {
                constexpr auto vLongitudinal = 5100.0f;
                const auto fL1 = vLongitudinal / (2.0f * juce::jmax(0.10f, stringLength));
                const auto fL2 = 2.0f * fL1;
                const auto fL3 = 3.0f * fL1;
                longPhaseInc1 = juce::MathConstants<float>::twoPi * fL1 / sampleRate;
                longPhaseInc2 = juce::MathConstants<float>::twoPi * fL2 / sampleRate;
                longPhaseInc3 = juce::MathConstants<float>::twoPi * fL3 / sampleRate;
                longPhase1 = 0.0f;
                longPhase2 = 0.0f;
                longPhase3 = 0.0f;

                const auto longTotalSamples = juce::jmax(1, static_cast<int>(0.016f * sampleRate));
                longSamplesRemaining = longTotalSamples;
                longAmplitude = peakLevelAtFullVelocity * 0.10f * vLevel;
                longDecayPerSample = std::exp(-5.0f / static_cast<float>(longTotalSamples));
            } else {
                longSamplesRemaining = 0;
                longAmplitude = 0.0f;
            }
        }

        [[nodiscard]] float getNextSample() noexcept {
            auto out = 0.0f;
            if (samplesRemaining > 0) {
                --samplesRemaining;
                const auto s1 = std::sin(oscPhase1);
                const auto s2 = std::sin(oscPhase2);
                oscPhase1 += phaseInc1;
                oscPhase2 += phaseInc2;
                out += amplitude * (0.6f * s1 + 0.4f * s2);
                amplitude *= decayPerSample;
            }
            if (longSamplesRemaining > 0) {
                --longSamplesRemaining;
                const auto l1 = std::sin(longPhase1);
                const auto l2 = std::sin(longPhase2);
                const auto l3 = std::sin(longPhase3);
                longPhase1 += longPhaseInc1;
                longPhase2 += longPhaseInc2;
                longPhase3 += longPhaseInc3;
                out += longAmplitude * (0.60f * l1 + 0.25f * l2 + 0.15f * l3);
                longAmplitude *= longDecayPerSample;
            }
            return out;
        }

        void reset() noexcept {
            samplesRemaining = 0;
            amplitude = 0.0f;
            longSamplesRemaining = 0;
            longAmplitude = 0.0f;
        }

        [[nodiscard]] bool isActive() const noexcept {
            return samplesRemaining > 0 || longSamplesRemaining > 0;
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
