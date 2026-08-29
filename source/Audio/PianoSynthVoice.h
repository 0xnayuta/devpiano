#pragma once

#include "Piano88KeyTable.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

// 内置物理建模钢琴合成器（Phase 12~24）：
// - 88 键物理参数化 (Phase 18-A/B)：Steinway B 刚性失谐、Bensa 实测阻尼、STFT 最优微相位矩阵；
// - 空气黏性阻尼与二次方摩擦 (Phase 18-C)：Desvages & Bilbao (2016) 模态耗散模型，呈现中频下凹歌唱性；
// - 16 峰正交云杉木音板模态 (Phase 19-A)：Bank 2010 / Chabassier 2019 实测物理模态分布；
// - 琴桥立体声空间辐射 (Phase 19-B)：根据 88 键物理跨度分配声像，消灭单声道耳膜居中压迫感；
// - 同音三弦独立三振荡器非对称拍频 (Phase 19-C)：3 弦独立微失谐与 STFT 空间初相；
// - 低音钢弦纵波先驱脉冲 (Phase 20-A, Bank 2005/2010)：v_L ≈ 5100 m/s 极短金属撞击先导声；
// - 机械击弦微观混沌微扰 (Phase 20-B, Bank & Chabassier 2019)：消除同音轮指的机械克隆感；
// - 延音踏板全局交感共鸣弦池 (Phase 21-A, Bank 2010 Sec. VI)：CC64 踏板驱动 12 半音基底交感共振；
// - 三角钢琴琴盖反射与近场木质微反射 (Phase 21-B, Chabassier 2019 Sec. 3.4)：琴盖空气深度与早期反射；
// - 制音器落弦与琴键释放机械瞬态 (Phase 22-A, Damper Felt Fall & Release Thump)：88 键音区分级落弦低频闷击声；
// - 琴盖开合度声学传递函数 (Phase 22-B, Lid Position: Full / Half / Closed)：Chabassier 2013 JASA
// 开合高频滚降与箱体近场反射矩阵；
// - 长短琴桥断裂交界音色补偿 (Phase 22-C, Bridge Break Voicing Jump)：Fletcher & Rossing 1998 G2/G#2 阶跃跳变；
// - 强击非线性张力音高微漂移与软饱和 (Phase 22-D, Pitch Glide & Soft Saturation)：Bank & Sujbert 2005 JASA 张力瞬态与
// Bilbao 2009 软饱和；
// - 未踩踏板单键和弦开放弦交感共鸣 (Phase 22-E, Duplex & Unpedaled Sympathetic Resonance)：Bank 2010 IEEE TASLP Sec. VI
// 开放制音弦交感振荡；
// - 动态琴槌非线性刚度与击弦点几何陷波 (Phase 23-A, Chaigne & Askenfelt 1994, Russell & Rossing 1998)：
//   三层毛毡动力学压实模型、连续变化接触时间 Tc、动态截止滚降指数与精确击弦位置陷波；
// - 同音三弦立体声非对称微失谐与声相展开 (Phase 23-B, Weinreich 1977 JASA)：
//   将同音多弦振动在物理声相空间微观展开，彻底消除中高音单声道聚焦感，营造开阔立体的空气环绕感；
// - 云杉木音板高频截止与木质腔体共鸣峰配平 (Phase 23-C, Boutillon & Ege 2013, Giordano 1998)：
//   4.2kHz 云杉木粘滞内耗低通截止滤波器与 16 峰物理音板模态配平，赋予真实三角钢琴温润深厚的木质感；
// - 起音瞬态裂音与低音纵波微调 (Phase 23-D, Attack Transient Crack & Longitudinal Tuning)：
//   前 3ms 高频冲击裂音 (HF Crack) 与紧凑型低音纵波先导声，对齐真琴 27~30ms 极速起振特性；
// - 泛音时间滞后膨胀与绽放 (Phase 24-A, Harmonic Blooming)：中高力度高阶分音非线性能量泵浦与上升绽放；
// - 琴槌接触微阻尼与脱离物理释放 (Phase 24-B, Hammer Contact-Release Dynamics)：消灭 t=0 正弦波机械突兀感；
// - 动态声场空间漫射 (Phase 24-C, Dynamic Spatial Diffusion)：从击打点声源平滑漫射为音板面声源包围场。

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
    static constexpr auto peakLevelAtFullVelocity = 0.45f;
    static constexpr auto silentLevelThreshold = 1e-4f;

    enum class LidPosition : std::uint8_t {
        fullOpen = 0,
        halfStick = 1,
        closed = 2,
    };

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

    void setLidPosition(LidPosition position) noexcept {
        pianoLidPosition = position;
        lidAcoustics.setPosition(position, getSampleRate());
    }

    [[nodiscard]] LidPosition getLidPosition() const noexcept {
        return pianoLidPosition;
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
        const auto decayScale = 1.0f + (pianoResonance - 0.5f) * 0.6f;
        const auto baseDecaySeconds = static_cast<double>(params.decaySeconds * decayScale);

        const auto keyPos = std::clamp((static_cast<float>(midiNoteNumber) - 21.0f) / 87.0f, 0.0f, 1.0f);

        // 动态琴槌毛毡动力学 (Phase 23-A, Chaigne & Askenfelt 1994, Russell & Rossing 1998)
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
            normSum += amplitudeFor(n, keyPos, effectiveHardness, effectiveStrikePos, partialFrequency, effectiveTc)
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
                partial.bloomGain = 1.0f;
                partial.bloomRisePerSample = 0.0f;
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

            partial.level
                = amplitudeFor(n, keyPos, effectiveHardness, effectiveStrikePos, partialFrequency, effectiveTc)
                * hammerGain(n, numActivePartials) * brightnessBoost(n, pianoBrightness, numActivePartials) * scale
                * velocityLevel;

            // 泛音时间滞后膨胀与绽放 (Phase 24-A, Harmonic Blooming)
            if (n >= 2 && clampedVelocity > 0.40f) {
                const auto bloomDepth = 0.35f * (clampedVelocity - 0.40f) / 0.60f;
                partial.bloomGain = 1.0f - bloomDepth;
                const auto tauBloom = static_cast<double>(
                    0.008f + 0.016f * (1.0f - static_cast<float>(n) / static_cast<float>(numActivePartials)));
                partial.bloomRisePerSample = static_cast<float>(1.0 / (tauBloom * sampleRate));
            } else {
                partial.bloomGain = 1.0f;
                partial.bloomRisePerSample = 0.0f;
            }

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
        hammerContactEngine.trigger(sampleRate, effectiveTc);
        spatialDiffusionEngine.trigger(sampleRate);
        damperTransient.reset();
        pitchGlideEngine.trigger(sampleRate, clampedVelocity);

        for (auto& resonator : bodyResonators) {
            resonator.reset();
            resonator.updateCoefficients(sampleRate);
        }
        spruceSoundboardFilter.updateCoefficients(sampleRate);
        spruceSoundboardFilter.reset();

        sympatheticPool.updateCoefficients(sampleRate);
        sympatheticPool.noteOnKey(midiNoteNumber);
        lidAcoustics.setPosition(pianoLidPosition, sampleRate);
        lidAcoustics.reset();

        adsrGate.setSampleRate(sampleRate);
        adsrGate.noteOn();
    }

    void stopNote(float velocity, bool allowTailOff) override {
        sympatheticPool.noteOffKey(currentPlayingMidiNote);

        if (allowTailOff) {
            adsrGate.noteOff();
            const auto sampleRate = getSampleRate();
            damperTransient.trigger(sampleRate, currentPlayingMidiNote, velocity > 0.0f ? velocity : 0.6f);
            return;
        }

        adsrGate.reset();
        hammerTransient.reset();
        hammerContactEngine.reset();
        spatialDiffusionEngine.reset();
        damperTransient.reset();
        pitchGlideEngine.reset();
        for (auto& resonator : bodyResonators) {
            resonator.reset();
        }
        spruceSoundboardFilter.reset();
        sympatheticPool.reset();
        lidAcoustics.reset();
        clearCurrentNote();
    }
    void pitchWheelMoved(int) override {
    }
    void controllerMoved(int controllerNumber, int controllerValue) override {
        // MIDI CC 64 延音踏板 (Sustain Pedal, Phase 21-A)
        if (controllerNumber == 64) {
            sympatheticPool.setPedalDown(controllerValue >= 64);
        }
    }

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override {
        if (!isVoiceActive()) {
            return;
        }

        for (auto sample = 0; sample < numSamples; ++sample) {
            const auto envelope = adsrGate.getNextSample();
            const auto click = hammerTransient.getNextSample();
            const auto damperThump = damperTransient.getNextSample();
            const auto glideMult = static_cast<double>(pitchGlideEngine.getGlideMultiplier());
            const auto diffusionFactor = spatialDiffusionEngine.getDiffusionFactor();

            if (envelope <= 0.0f && !adsrGate.isActive() && !damperTransient.isActive()) {
                clearCurrentNote();
                for (auto& resonator : bodyResonators) {
                    resonator.reset();
                }
                spruceSoundboardFilter.reset();
                sympatheticPool.reset();
                lidAcoustics.reset();
                break;
            }

            // 同音多弦立体声非对称空间展开 (Phase 23-B / Phase 24-C, Dynamic Spatial Diffusion)
            auto valueLeft = 0.0f;
            auto valueRight = 0.0f;
            for (auto n = 0; n < numActivePartials; ++n) {
                auto& partial = partials[static_cast<std::size_t>(n)];
                auto oscL = static_cast<float>(partial.sinState);
                auto oscR = oscL;
                const auto effEps = partial.epsilon * glideMult;

                if (partial.stringCount == 2) {
                    const auto s1 = static_cast<float>(partial.sinState);
                    const auto s2 = static_cast<float>(partial.sinState2);
                    const auto sSum = 0.5f * (s1 + s2);
                    const auto sDiff = 0.5f * (s2 - s1);
                    const auto effSpread = 0.20f * (0.60f + 0.40f * diffusionFactor);
                    oscL = sSum - effSpread * sDiff;
                    oscR = sSum + effSpread * sDiff;
                    const auto effEps2 = partial.epsilon2 * glideMult;
                    const auto nextCos2 = partial.cosState2 - effEps2 * partial.sinState2;
                    partial.sinState2 += effEps2 * nextCos2;
                    partial.cosState2 = nextCos2;
                } else if (partial.stringCount == 3) {
                    const auto s1 = static_cast<float>(partial.sinState);
                    const auto s2 = static_cast<float>(partial.sinState2);
                    const auto s3 = static_cast<float>(partial.sinState3);
                    // 物理对称同音三弦 Mid-Side 动态空间扩散 (Phase 23-B / Phase 24-C)
                    const auto sSum = (1.0f / 3.0f) * (s1 + s2 + s3);
                    const auto sDiff = (1.0f / 3.0f) * (s3 - s1);
                    const auto effSpread = 0.25f * (0.60f + 0.40f * diffusionFactor);
                    oscL = sSum - effSpread * sDiff;
                    oscR = sSum + effSpread * sDiff;

                    const auto effEps2 = partial.epsilon2 * glideMult;
                    const auto nextCos2 = partial.cosState2 - effEps2 * partial.sinState2;
                    partial.sinState2 += effEps2 * nextCos2;
                    partial.cosState2 = nextCos2;
                    const auto effEps3 = partial.epsilon3 * glideMult;
                    const auto nextCos3 = partial.cosState3 - effEps3 * partial.sinState3;
                    partial.sinState3 += effEps3 * nextCos3;
                    partial.cosState3 = nextCos3;
                }

                // 泛音时间滞后膨胀与绽放 (Phase 24-A)
                if (partial.bloomGain < 1.0f) {
                    partial.bloomGain
                        = std::min(1.0f, partial.bloomGain + partial.bloomRisePerSample * (1.0f - partial.bloomGain));
                }

                const auto partialAmp = (partial.levelFast + partial.levelSlow) * partial.bloomGain;
                valueLeft += partialAmp * oscL;
                valueRight += partialAmp * oscR;

                const auto nextCos = partial.cosState - effEps * partial.sinState;
                partial.sinState += effEps * nextCos;
                partial.cosState = nextCos;
                partial.levelFast *= partial.decayFastPerSample;
                partial.levelSlow *= partial.decaySlowPerSample;
            }
            const auto sampleIndex = startSample + sample;
            // 琴槌接触微阻尼与脱离物理释放 (Phase 24-B)
            const auto preSatLeft = valueLeft * envelope + click + damperThump;
            const auto preSatRight = valueRight * envelope + click + damperThump;
            // 音板与琴桥大动态软饱和 (Phase 22-D, Bilbao 2009)
            const auto rawLeft = softSaturate(preSatLeft);
            const auto rawRight = softSaturate(preSatRight);
            const auto rawMono = 0.5f * (rawLeft + rawRight);

            // 1. 16 峰物理云杉木音板模态 (Phase 19-A/B)
            auto resonatorLeftSum = 0.0f;
            auto resonatorRightSum = 0.0f;
            for (std::size_t i = 0; i < numResonators; ++i) {
                const auto spec = resonatorSpec(static_cast<int>(i));
                const auto resOut = bodyResonators[i].process(rawMono);
                resonatorLeftSum += spec.weightLeft * resOut;
                resonatorRightSum += spec.weightRight * resOut;
            }

            // 2. 云杉木音板高频粘滞吸收低通滤波 (Phase 23-C, Boutillon & Ege 2013)
            spruceSoundboardFilter.processStereo(resonatorLeftSum, resonatorRightSum);

            // 3. 延音踏板与单键和弦开放弦交感共鸣 (Phase 21-A / Phase 22-E, Bank 2010 Sec. VI)
            const auto sympatheticOut = sympatheticPool.process(rawMono);

            // 4. 琴桥立体声声像定位与非对称空间投影 (低音在左 0.15 -> 高音在右 0.85)
            const auto midi = std::clamp(static_cast<float>(currentPlayingMidiNote), 21.0f, 108.0f);
            const auto keyPos = (midi - 21.0f) / 87.0f;
            const auto directPan = 0.15f + 0.70f * keyPos;
            const auto directLeft = (1.0f - directPan) * 1.414f;
            const auto directRight = directPan * 1.414f;

            const auto wet = 0.18f + pianoResonance * 0.16f;
            auto outLeft = (1.0f - wet) * rawLeft * directLeft + wet * (resonatorLeftSum + 0.40f * sympatheticOut);
            auto outRight = (1.0f - wet) * rawRight * directRight + wet * (resonatorRightSum + 0.60f * sympatheticOut);

            // 5. 三角钢琴琴盖反射与近场木质微反射 (Phase 21-B / Phase 22-B, Chabassier 2013/2019)
            lidAcoustics.processStereo(outLeft, outRight);

            if (outputBuffer.getNumChannels() >= 2) {
                outputBuffer.addSample(0, sampleIndex, outLeft);
                outputBuffer.addSample(1, sampleIndex, outRight);
            } else if (outputBuffer.getNumChannels() == 1) {
                const auto outMono
                    = (1.0f - wet) * rawMono + wet * 0.5f * (resonatorLeftSum + resonatorRightSum + sympatheticOut);
                outputBuffer.addSample(0, sampleIndex, outMono);
            }
        }

        if (allPartialsSilent()) {
            clearCurrentNote();
            hammerTransient.reset();
            hammerContactEngine.reset();
            spatialDiffusionEngine.reset();
            damperTransient.reset();
            pitchGlideEngine.reset();
            for (auto& resonator : bodyResonators) {
                resonator.reset();
            }
            spruceSoundboardFilter.reset();
            sympatheticPool.reset();
            lidAcoustics.reset();
        }
    }

    [[nodiscard]] static float softSaturate(float x) noexcept {
        const auto x2 = std::clamp(x * x, 0.0f, 1.0f);
        return x * (1.0f - 0.12f * x2);
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
        // 击弦点几何梳状陷波 (Chaigne & Askenfelt 1994, 消除 7~9 阶非协和刺耳分音)
        const auto raw = std::abs(std::sin(juce::MathConstants<float>::pi * m * strikingRatio));
        return std::max(raw, 0.03f);
    }

    [[nodiscard]] static float hammerSpectrumGain(int partialIndex, float keyPos, float effectiveHardness,
                                                  double partialFrequency, float tc) noexcept {
        const auto m = static_cast<float>(partialIndex + 1);
        // 88 键音区基底幂次滚降: 低音 1.25 -> 高音 2.10 (保证低音丰富，高音突出基频)
        const auto rolloff = 1.20f + 0.30f * keyPos + 1.20f * (keyPos * keyPos);
        const auto basePowerRollOff = 1.0f / std::pow(m, rolloff);

        // 琴槌毛毡非线性低通滤波: 截止频率 fc = 2.5 / Tc (Chaigne & Askenfelt 1994)
        const auto hammerCutoff = 2.5f / std::max(tc, 0.0001f);
        // 滚降陡度随力度动态变化: pp 弱音 1.84 (陡峭滤除高频), ff 强音 1.20 (平缓保留金属光泽)
        const auto hammerRolloffExp = 2.0f - 0.80f * effectiveHardness;

        const auto freqRatio = static_cast<float>(partialFrequency) / hammerCutoff;
        const auto feltFilter = 1.0f / (1.0f + std::pow(freqRatio, hammerRolloffExp));

        return basePowerRollOff * feltFilter;
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

    [[nodiscard]] static float amplitudeFor(int partialIndex, float keyPos = 0.5f, float effectiveHardness = 0.5f,
                                            float strikingRatio = 0.1333f, double partialFrequency = 440.0,
                                            float tc = 0.0018f) noexcept {
        return strikeCombGain(partialIndex, strikingRatio)
            * hammerSpectrumGain(partialIndex, keyPos, effectiveHardness, partialFrequency, tc)
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
        if (hammerTransient.isActive() || damperTransient.isActive()) {
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

        // 泛音时间滞后膨胀与绽放 (Phase 24-A, Harmonic Blooming)
        float bloomGain = 1.0f;
        float bloomRisePerSample = 0.0f;
    };
    std::array<Partial, maxPartials> partials;
    int numActivePartials = 0;
    int currentPlayingMidiNote = 60;
    std::uint32_t triggerCounter = 0;
    juce::ADSR adsrGate;
    float pianoBrightness = 0.5f;
    float pianoHammerHardness = 0.5f;
    float pianoResonance = 0.5f;
    LidPosition pianoLidPosition = LidPosition::fullOpen;

    // 强击非线性张力音高微漂移引擎 (Phase 22-D, Bank & Sujbert 2005 JASA)
    struct PitchGlideEngine {
        int samplesRemaining = 0;
        float glideFactor = 0.0f;
        float decayPerSample = 0.0f;

        void trigger(double sr, float velocity) noexcept {
            if (sr <= 0.0) {
                reset();
                return;
            }
            const auto clampedV = juce::jlimit(0.0f, 1.0f, velocity);
            if (clampedV < 0.40f) {
                reset();
                return;
            }
            const auto sampleRate = static_cast<float>(sr);
            const auto totalSamples = static_cast<int>(0.012f * sampleRate);
            samplesRemaining = totalSamples;
            const auto maxGlide = 0.0025f * (clampedV - 0.40f) / 0.60f;
            glideFactor = maxGlide;
            decayPerSample = std::exp(-4.5f / static_cast<float>(totalSamples));
        }

        [[nodiscard]] float getGlideMultiplier() noexcept {
            if (samplesRemaining <= 0) {
                return 1.0f;
            }
            --samplesRemaining;
            const auto mult = 1.0f + glideFactor;
            glideFactor *= decayPerSample;
            return mult;
        }

        void reset() noexcept {
            samplesRemaining = 0;
            glideFactor = 0.0f;
        }
    };
    PitchGlideEngine pitchGlideEngine;

    // 琴槌接触微阻尼与脱离物理释放引擎 (Phase 24-B, Hammer Contact-Release Dynamics)
    struct HammerContactEngine {
        int samplesRemaining = 0;
        float releaseProgress = 1.0f;
        float progressInc = 0.0f;

        void trigger(double sr, float tc) noexcept {
            if (sr <= 0.0 || tc <= 0.0f) {
                reset();
                return;
            }
            const auto total = std::max(1, static_cast<int>(tc * static_cast<float>(sr)));
            samplesRemaining = total;
            releaseProgress = 0.20f;
            progressInc = (1.0f - 0.20f) / static_cast<float>(total);
        }

        [[nodiscard]] float getReleaseMultiplier() noexcept {
            if (samplesRemaining <= 0) {
                return 1.0f;
            }
            --samplesRemaining;
            const auto current = releaseProgress;
            releaseProgress = std::min(1.0f, releaseProgress + progressInc);
            return current;
        }

        void reset() noexcept {
            samplesRemaining = 0;
            releaseProgress = 1.0f;
            progressInc = 0.0f;
        }
    };
    HammerContactEngine hammerContactEngine;

    // 动态声场空间漫射引擎 (Phase 24-C, Dynamic Spatial Diffusion)
    struct SpatialDiffusionEngine {
        float diffusionProgress = 1.0f;
        float decayFactor = 0.0f;

        void trigger(double sr) noexcept {
            if (sr <= 0.0) {
                reset();
                return;
            }
            diffusionProgress = 0.0f;
            decayFactor = std::exp(-1.0f / (0.025f * static_cast<float>(sr)));
        }

        [[nodiscard]] float getDiffusionFactor() noexcept {
            if (diffusionProgress >= 0.999f) {
                return 1.0f;
            }
            diffusionProgress = 1.0f - (1.0f - diffusionProgress) * decayFactor;
            return diffusionProgress;
        }

        void reset() noexcept {
            diffusionProgress = 1.0f;
        }
    };
    SpatialDiffusionEngine spatialDiffusionEngine;

    struct HammerTransient {
        int samplesRemaining = 0;
        int totalSamples = 0;
        float amplitude = 0.0f;
        float decayPerSample = 0.0f;
        float oscPhase1 = 0.0f;
        float phaseInc1 = 0.0f;
        float oscPhase2 = 0.0f;
        float phaseInc2 = 0.0f;

        // 起音高频摩擦裂音 (Phase 23-D, Attack HF Crack, 0~3ms)
        int crackSamplesRemaining = 0;
        float crackAmplitude = 0.0f;
        float crackDecayPerSample = 0.0f;
        std::uint32_t crackRng = 0x5a5a5a5au;

        // 低音纵波先驱声 (Phase 20-A / Phase 23-D, Longitudinal Precursor Ping)
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

            // 起音 3ms 高频裂音 (Phase 23-D, Attack HF Crack, 消除起振延迟)
            const auto keyPos = std::clamp((static_cast<float>(midiNoteNumber) - 21.0f) / 87.0f, 0.0f, 1.0f);
            constexpr auto crackDur = 0.003f;
            const auto crackTotal = juce::jmax(1, static_cast<int>(crackDur * sampleRate));
            crackSamplesRemaining = crackTotal;
            crackAmplitude = peakLevelAtFullVelocity * 0.08f * (v * v) * (0.20f + 0.80f * keyPos);
            crackDecayPerSample = std::exp(-6.0f / static_cast<float>(crackTotal));
            crackRng = static_cast<std::uint32_t>(midiNoteNumber) * 1664525u + 1013904223u;

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

                const auto longTotalSamples = juce::jmax(1, static_cast<int>(0.015f * sampleRate));
                longSamplesRemaining = longTotalSamples;
                longAmplitude = peakLevelAtFullVelocity * 0.10f * vLevel;
                longDecayPerSample = std::exp(-5.5f / static_cast<float>(longTotalSamples));
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
            if (crackSamplesRemaining > 0) {
                --crackSamplesRemaining;
                crackRng = crackRng * 1664525u + 1013904223u;
                const auto noiseVal = static_cast<float>(crackRng >> 16) / 65535.0f * 2.0f - 1.0f;
                out += crackAmplitude * noiseVal;
                crackAmplitude *= crackDecayPerSample;
            }
            if (longSamplesRemaining > 0) {
                --longSamplesRemaining;
                const auto l1 = std::sin(longPhase1);
                const auto l2 = std::sin(longPhase2);
                const auto l3 = std::sin(longPhase3);
                longPhase1 += longPhaseInc1;
                longPhase2 += longPhaseInc2;
                longPhase3 += longPhaseInc3;
                out += longAmplitude * (0.55f * l1 + 0.30f * l2 + 0.15f * l3);
                longAmplitude *= longDecayPerSample;
            }
            return out;
        }

        void reset() noexcept {
            samplesRemaining = 0;
            amplitude = 0.0f;
            crackSamplesRemaining = 0;
            crackAmplitude = 0.0f;
            longSamplesRemaining = 0;
            longAmplitude = 0.0f;
        }

        [[nodiscard]] bool isActive() const noexcept {
            return samplesRemaining > 0 || crackSamplesRemaining > 0 || longSamplesRemaining > 0;
        }
    };
    HammerTransient hammerTransient;

    // 制音器落弦与琴键释放机械瞬态 (Phase 22-A, Damper Felt Fall & Release Thump)
    struct DamperTransient {
        int samplesRemaining = 0;
        int totalSamples = 0;
        float amplitude = 0.0f;
        float decayPerSample = 0.0f;
        float oscPhase1 = 0.0f;
        float phaseInc1 = 0.0f;
        float oscPhase2 = 0.0f;
        float phaseInc2 = 0.0f;

        void trigger(double sr, int midiNoteNumber, float releaseVelocity) noexcept {
            if (sr <= 0.0) {
                return;
            }
            if (midiNoteNumber > 88) {
                reset();
                return;
            }

            const auto sampleRate = static_cast<float>(sr);
            const auto noteRatio = static_cast<float>(midiNoteNumber - 21) / 67.0f;
            const auto dur = juce::jlimit(0.006f, 0.024f, 0.024f - noteRatio * 0.018f);
            totalSamples = juce::jmax(1, static_cast<int>(dur * sampleRate));
            samplesRemaining = totalSamples;

            const auto f1 = juce::jlimit(75.0f, 160.0f, 85.0f + noteRatio * 60.0f);
            const auto f2 = juce::jlimit(180.0f, 420.0f, 220.0f + noteRatio * 180.0f);
            phaseInc1 = juce::MathConstants<float>::twoPi * f1 / sampleRate;
            phaseInc2 = juce::MathConstants<float>::twoPi * f2 / sampleRate;
            oscPhase1 = 0.0f;
            oscPhase2 = 0.0f;

            const auto rv = juce::jlimit(0.0f, 1.0f, releaseVelocity);
            const auto zoneGain = juce::jlimit(0.05f, 1.0f, 1.0f - noteRatio * 0.85f);
            amplitude = peakLevelAtFullVelocity * 0.08f * rv * zoneGain;
            decayPerSample = std::exp(-5.0f / static_cast<float>(totalSamples));
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
            const auto out = amplitude * (0.75f * s1 + 0.25f * s2);
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
    DamperTransient damperTransient;

    // 延音踏板全局交感共鸣弦池与单键开放弦交感 (Phase 21-A / Phase 22-E, Bank 2010 Sec. VI)
    struct SympatheticResonancePool {
        static constexpr auto numPoolResonators = 12;
        bool pedalDown = false;
        std::array<int, numPoolResonators> openNoteCount {};
        float c1[numPoolResonators] {};
        float c2[numPoolResonators] {};
        float g[numPoolResonators] {};
        float s1[numPoolResonators] {};
        float s2[numPoolResonators] {};

        void setPedalDown(bool isDown) noexcept {
            pedalDown = isDown;
        }

        void noteOnKey(int midiNoteNumber) noexcept {
            const auto pc = (midiNoteNumber % 12 + 12) % 12;
            ++openNoteCount[static_cast<std::size_t>(pc)];
        }

        void noteOffKey(int midiNoteNumber) noexcept {
            const auto pc = (midiNoteNumber % 12 + 12) % 12;
            auto& count = openNoteCount[static_cast<std::size_t>(pc)];
            if (count > 0) {
                --count;
            }
        }

        void updateCoefficients(double sampleRate) noexcept {
            if (sampleRate <= 0.0) {
                return;
            }
            constexpr float freqs[numPoolResonators] = { 65.41f, 69.30f, 73.42f,  77.78f,  82.41f,  87.31f,
                                                         92.50f, 98.00f, 103.83f, 110.00f, 116.54f, 123.47f };
            for (int i = 0; i < numPoolResonators; ++i) {
                const auto theta = juce::MathConstants<double>::twoPi * static_cast<double>(freqs[i]) / sampleRate;
                const auto bandwidth = static_cast<double>(freqs[i] / 12.0f);
                const auto r = std::exp(-juce::MathConstants<double>::pi * bandwidth / sampleRate);
                c1[i] = static_cast<float>(2.0 * r * std::cos(theta));
                c2[i] = static_cast<float>(-r * r);
                g[i] = static_cast<float>((1.0 - r * r) * 0.5);
            }
        }

        void reset() noexcept {
            openNoteCount.fill(0);
            for (int i = 0; i < numPoolResonators; ++i) {
                s1[i] = 0.0f;
                s2[i] = 0.0f;
            }
        }

        [[nodiscard]] float process(float in) noexcept {
            auto sum = 0.0f;
            for (int i = 0; i < numPoolResonators; ++i) {
                const auto isOpen = pedalDown || (openNoteCount[static_cast<std::size_t>(i)] > 0);
                if (!isOpen) {
                    s1[i] *= 0.90f;
                    s2[i] *= 0.90f;
                    continue;
                }
                const auto drive = in * (pedalDown ? 0.08f : 0.04f);
                const auto w = drive + c1[i] * s1[i] + c2[i] * s2[i];
                const auto y = g[i] * (w - s2[i]);
                s2[i] = s1[i];
                s1[i] = w;
                sum += y;
            }
            return sum;
        }
    };
    SympatheticResonancePool sympatheticPool;

    // 三角钢琴琴盖反射与近场木质微反射 (Phase 21-B / Phase 22-B, Chabassier 2013/2019)
    struct LidAcoustics {
        static constexpr std::size_t delaySize = 1024;
        std::array<float, delaySize> leftBuffer {};
        std::array<float, delaySize> rightBuffer {};
        std::size_t writePos = 0;
        LidPosition position = LidPosition::fullOpen;
        float lpLeft = 0.0f;
        float lpRight = 0.0f;
        float lpCoeff = 0.0f;

        void updateCoefficients(double sampleRate) noexcept {
            if (sampleRate <= 0.0) {
                return;
            }
            const auto sr = static_cast<float>(sampleRate);
            if (position == LidPosition::fullOpen) {
                lpCoeff = 0.0f;
            } else if (position == LidPosition::halfStick) {
                constexpr float fc = 6500.0f;
                lpCoeff = std::exp(-juce::MathConstants<float>::twoPi * fc / sr);
            } else {
                constexpr float fc = 2600.0f;
                lpCoeff = std::exp(-juce::MathConstants<float>::twoPi * fc / sr);
            }
        }

        void setPosition(LidPosition newPosition, double sampleRate) noexcept {
            position = newPosition;
            updateCoefficients(sampleRate);
        }

        void reset() noexcept {
            leftBuffer.fill(0.0f);
            rightBuffer.fill(0.0f);
            writePos = 0;
            lpLeft = 0.0f;
            lpRight = 0.0f;
        }

        void processStereo(float& left, float& right) noexcept {
            if (lpCoeff > 0.0f) {
                lpLeft = (1.0f - lpCoeff) * left + lpCoeff * lpLeft;
                lpRight = (1.0f - lpCoeff) * right + lpCoeff * lpRight;
                left = lpLeft;
                right = lpRight;
            }

            leftBuffer[writePos] = left;
            rightBuffer[writePos] = right;

            const auto tap1Pos = (writePos + delaySize - 141) % delaySize;
            const auto tap2Pos = (writePos + delaySize - 344) % delaySize;
            const auto tap3Pos = (writePos + delaySize - 507) % delaySize;

            float gDirect = 0.82f;
            float g1 = 0.10f;
            float g2 = 0.06f;
            float g3 = 0.04f;

            if (position == LidPosition::halfStick) {
                gDirect = 0.75f;
                g1 = 0.14f;
                g2 = 0.09f;
                g3 = 0.06f;
            } else if (position == LidPosition::closed) {
                gDirect = 0.68f;
                g1 = 0.18f;
                g2 = 0.12f;
                g3 = 0.08f;
            }

            const auto earlyL = g1 * leftBuffer[tap1Pos] + g2 * rightBuffer[tap2Pos] + g3 * leftBuffer[tap3Pos];
            const auto earlyR = g1 * rightBuffer[tap1Pos] + g2 * leftBuffer[tap2Pos] + g3 * rightBuffer[tap3Pos];

            left = left * gDirect + earlyL;
            right = right * gDirect + earlyR;

            writePos = (writePos + 1) % delaySize;
        }
    };
    LidAcoustics lidAcoustics;

    // 云杉木音板高频粘滞吸收低通滤波器 (Phase 23-C, Boutillon & Ege 2013)
    struct SpruceSoundboardFilter {
        float lpLeft = 0.0f;
        float lpRight = 0.0f;
        float lpCoeff = 0.0f;

        void updateCoefficients(double sampleRate) noexcept {
            if (sampleRate <= 0.0) {
                return;
            }
            constexpr float fc = 4200.0f;
            lpCoeff = std::exp(-juce::MathConstants<float>::twoPi * fc / static_cast<float>(sampleRate));
        }

        void reset() noexcept {
            lpLeft = 0.0f;
            lpRight = 0.0f;
        }

        void processStereo(float& left, float& right) noexcept {
            if (lpCoeff > 0.0f) {
                lpLeft = (1.0f - lpCoeff) * left + lpCoeff * lpLeft;
                lpRight = (1.0f - lpCoeff) * right + lpCoeff * lpRight;
                left = lpLeft;
                right = lpRight;
            }
        }
    };
    SpruceSoundboardFilter spruceSoundboardFilter;

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
