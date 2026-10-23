#pragma once

#include "HammerVoicing.h"
#include "PerspectiveProcessor.h"
#include "Piano88KeyTable.h"
#include "PianoDspMath.h"
#include "PianoLidAcoustics.h"
#include "PianoMainString.h"
#include "PianoMechanicalTransients.h"
#include "PianoNoteSetup.h"
#include "PianoRegisterVoicing.h"
#include "PianoSourceLevels.h"
#include "PianoTuning.h"
#include "PianoVoiceResonance.h"
#include "TemperamentEngine.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
class PianoSynthVoiceTest;
class PianoTuningAndDuplexTest;
class PedalAcousticsTest;
// 内置物理建模钢琴合成器（Phase 12~24）：
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
// - 长短琴桥交界 (Phase 22-C 原 G2/G#2 B 阶跃)：Task 39-2 官方参考实测否定阶跃假设，
//   B 表已改为 17 锚点实测表（26/52/57 由 interp-training 扩充，见 Piano88KeyTable.h 第 4
//   段），桥归属仅作弦数/描述语义；
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

using Temperament = devpiano::audio::Temperament;
using TemperamentEngine = devpiano::audio::TemperamentEngine;

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
    using SoundPerspective = devpiano::audio::SoundPerspective;
    using PerspectiveProcessor = devpiano::audio::PerspectiveProcessor;
    using Temperament = devpiano::audio::Temperament;
    using HammerVoicing = devpiano::audio::HammerVoicing;
    using LidPosition = devpiano::audio::LidPosition;
    static constexpr auto numResonators = 16;
    static constexpr auto bodyWetRatio = 0.26f;

    PianoSynthVoice() {
        static_cast<void>(devpiano::audio::getPianoStretchRatios());
        sympatheticPool.updateCoefficients(48000.0);
        for (auto& resonator : bodyResonators) {
            resonator.updateCoefficients(48000.0);
        }
        damperTransient.updateSampleRate(48000.0);
    }

    bool canPlaySound(juce::SynthesiserSound* sound) override {
        return dynamic_cast<PianoSynthSound*>(sound) != nullptr;
    }

    void setAdsrParameters(const juce::ADSR::Parameters& parameters) {
        configuredAdsr = parameters;
        configuredAdsr.attack = juce::jmin(0.0002f, parameters.attack);
        configuredAdsr.decay = 0.001f;
        configuredAdsr.sustain = 1.0f;
        configuredAdsr.release = juce::jmax(0.01f, parameters.release);
        if (getSampleRate() > 0.0) {
            adsrGate.setSampleRate(getSampleRate());
        }
        adsrGate.setParameters(configuredAdsr);
    }

    [[nodiscard]] const juce::ADSR::Parameters& getAdsrParameters() const noexcept {
        return configuredAdsr;
    }

    void setPianoParameters(float brightness, float hammerHardness, float resonance) noexcept {
        pianoBrightness = juce::jlimit(0.0f, 1.0f, brightness);
        pianoHammerHardness = juce::jlimit(0.0f, 1.0f, hammerHardness);
        pianoResonance = juce::jlimit(0.0f, 1.0f, resonance);
    }

    /// 三力度整音锚点。非法配置保持原配置并返回 false；默认配置为零偏移。
    [[nodiscard]] bool setHammerVoicing(const HammerVoicing& voicing) noexcept {
        if (!devpiano::audio::isValidHammerVoicing(voicing)) {
            return false;
        }
        hammerVoicing = voicing;
        return true;
    }

    [[nodiscard]] const HammerVoicing& getHammerVoicing() const noexcept {
        return hammerVoicing;
    }

    void setPianoTuning(bool stretchTuningEnabled, float duplexResonance) noexcept {
        pianoStretchTuningEnabled = stretchTuningEnabled;
        duplexPool.setAmount(juce::jlimit(0.0f, 1.0f, duplexResonance));
        sympatheticPool.updateCoefficients(getSampleRate(), pianoTemperament, pianoReferencePitchA4,
                                           pianoStretchTuningEnabled);
    }

    void setLidPosition(LidPosition position) noexcept {
        pianoLidPosition = position;
        lidAcoustics.setPosition(position, getSampleRate());
    }

    [[nodiscard]] LidPosition getLidPosition() const noexcept {
        return pianoLidPosition;
    }

    void setSoundPerspective(SoundPerspective perspective) noexcept {
        pianoSoundPerspective = perspective;
        perspectiveProcessor.setPerspective(perspective, !isVoiceActive());
    }

    [[nodiscard]] SoundPerspective getSoundPerspective() const noexcept {
        return pianoSoundPerspective;
    }
    void setSoftPedalDown(bool down, float amount = 1.0f) noexcept {
        softPedalDown = down;
        softPedalAmount = down ? juce::jlimit(0.0f, 1.0f, amount) : 0.0f;
    }

    [[nodiscard]] bool isSoftPedalDown() const noexcept {
        return softPedalDown;
    }

    [[nodiscard]] float getSoftPedalAmount() const noexcept {
        return softPedalAmount;
    }
    void setTemperament(Temperament temperament) noexcept {
        pianoTemperament = temperament;
        sympatheticPool.updateCoefficients(getSampleRate(), pianoTemperament, pianoReferencePitchA4,
                                           pianoStretchTuningEnabled);
    }

    [[nodiscard]] Temperament getTemperament() const noexcept {
        return pianoTemperament;
    }

    void setReferencePitchA4(double pitch) noexcept {
        pianoReferencePitchA4 = TemperamentEngine::clampReferencePitch(pitch);
        sympatheticPool.updateCoefficients(getSampleRate(), pianoTemperament, pianoReferencePitchA4,
                                           pianoStretchTuningEnabled);
    }

    [[nodiscard]] double getReferencePitchA4() const noexcept {
        return pianoReferencePitchA4;
    }
    void setVoiceIndex(int index) noexcept {
        voiceIndex = index;
    }

    [[nodiscard]] int getVoiceIndex() const noexcept {
        return voiceIndex;
    }

    void setFeltAgeingAmount(float amount) noexcept {
        pianoFeltAgeingAmount = juce::jlimit(0.0f, 1.0f, amount);
    }

    [[nodiscard]] float getFeltAgeingAmount() const noexcept {
        return pianoFeltAgeingAmount;
    }

    [[nodiscard]] bool isVoiceActive() const override {
        return getCurrentlyPlayingNote() >= 0;
    }
    [[nodiscard]] bool isPlayingChannel(int midiChannel) const override {
        return juce::SynthesiserVoice::isPlayingChannel(midiChannel);
    }
    void setCurrentPlaybackSampleRate(double newRate) override {
        juce::SynthesiserVoice::setCurrentPlaybackSampleRate(newRate);
        if (newRate > 0.0) {
            mainString.prepare(newRate);
            sympatheticPool.updateCoefficients(newRate, pianoTemperament, pianoReferencePitchA4,
                                               pianoStretchTuningEnabled);
            duplexPool.reset();
            lidAcoustics.setPosition(pianoLidPosition, newRate);
            perspectiveProcessor.prepare(newRate);
            for (auto& resonator : bodyResonators) {
                resonator.updateCoefficients(newRate);
            }
            spruceSoundboardFilter.updateCoefficients(newRate);
            damperTransient.updateSampleRate(newRate);
        }
    }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override {
        const auto sampleRate = getSampleRate();
        if (sampleRate <= 0.0) {
            return;
        }
        perspectiveProcessor.prepare(sampleRate);

        currentPlayingMidiNote = midiNoteNumber;
        noteOutputGain = devpiano::audio::pianoRegisterLoudnessGain(midiNoteNumber);
        const auto& params = devpiano::audio::getNoteParams(midiNoteNumber);
        const auto triggerSequence = ++triggerCounter;
        const auto setup = devpiano::audio::computePianoNoteSetup(
            midiNoteNumber, velocity, params, pianoBrightness, pianoHammerHardness, pianoResonance,
            pianoFeltAgeingAmount, hammerVoicing, softPedalDown, softPedalAmount, pianoTemperament,
            pianoReferencePitchA4, pianoStretchTuningEnabled, triggerSequence);

        duplexPool.prepare(sampleRate, setup.firstPartialFrequency, setup.effectiveInharmonicityB);

        const auto keyPos = setup.keyPos;
        const auto directPan = 0.15f + 0.70f * keyPos;
        directLeftGain = (1.0f - directPan) * std::numbers::sqrt2_v<float>;
        directRightGain = directPan * std::numbers::sqrt2_v<float>;

        const auto unaCordaMu = setup.unaCordaMu;
        const auto hammerHardness = setup.hammerHardness;
        const auto effectiveTc = setup.effectiveTc;
        const auto clampedVelocity = setup.clampedVelocity;

        devpiano::audio::MainStringStart stringStart;
        stringStart.requestedPartials = params.partialCount;
        stringStart.stringCount = params.stringCount;
        stringStart.beatingPartials = params.beatingPartials;
        stringStart.beatingDetuneRatio = params.beatingDetuneRatio;
        stringStart.unisonKeyIndex = std::clamp(midiNoteNumber, 21, 108) - 21;
        stringStart.baseFrequency = setup.baseFrequency;
        stringStart.effectiveInharmonicityB = setup.effectiveInharmonicityB;
        stringStart.keyPos = setup.keyPos;
        stringStart.contactHardness = setup.contactHardness;
        stringStart.effectiveStrikePos = setup.effectiveStrikePos;
        stringStart.effectiveTc = setup.effectiveTc;
        stringStart.effectiveBrightness = setup.effectiveBrightness;
        stringStart.pianoBrightness = pianoBrightness;
        stringStart.hammerVoicingRolloffDelta = setup.hammerVoicingRolloffDelta;
        stringStart.velocityLevel = setup.velocityLevel;
        stringStart.clampedVelocity = setup.clampedVelocity;
        stringStart.unaCordaMu = setup.unaCordaMu;
        stringStart.jitterPhase = setup.jitterPhase;
        stringStart.baseDecaySeconds = setup.baseDecaySeconds;
        stringStart.k1 = setup.k1;
        stringStart.alpha1 = setup.alpha1;
        stringStart.b1 = params.b1;
        stringStart.b2 = params.b2;
        stringStart.fastDecayRatio = params.fastDecayRatio;
        stringStart.slowWeight = params.slowWeight;
        const auto unisonScale = mainString.start(stringStart);

        const auto hammerTransientVel = clampedVelocity * (1.0f - 0.35f * unaCordaMu);
        hammerTransient.trigger(sampleRate, midiNoteNumber, hammerTransientVel, hammerHardness, params.stringLength,
                                unisonScale);
        hammerContactEngine.trigger(sampleRate, effectiveTc);
        mainString.triggerSpatialDiffusion();
        damperTransient.reset();
        mainString.triggerPitchGlide(clampedVelocity);

        for (auto& resonator : bodyResonators) {
            resonator.reset();
            resonator.updateCoefficients(sampleRate);
        }
        spruceSoundboardFilter.updateCoefficients(sampleRate);
        spruceSoundboardFilter.reset();

        sympatheticPool.updateCoefficients(sampleRate, pianoTemperament, pianoReferencePitchA4,
                                           pianoStretchTuningEnabled);
        sympatheticPool.noteOnKey(midiNoteNumber);
        lidAcoustics.setPosition(pianoLidPosition, sampleRate);
        lidAcoustics.reset();

        adsrGate.setSampleRate(sampleRate);
        adsrGate.setParameters(configuredAdsr);
        adsrGate.noteOn();
    }

    void stopNote(float velocity, bool allowTailOff) override {
        sympatheticPool.noteOffKey(currentPlayingMidiNote);

        if (allowTailOff) {
            const auto sampleRate = getSampleRate();
            const auto relVel = velocity > 0.0f ? velocity : 0.6f;

            // 离键速度动态调节琴弦 ADSR 释放时间 (Phase 32-B, Dynamic Key Release Damping):
            // 慢离键毛毡缓冲贴弦缓慢，保留微弱余音延展；快离键瞬间强力消音
            if (sampleRate > 0.0) {
                const auto baseRelease = configuredAdsr.release;
                const auto dynamicRelease = baseRelease * (1.5f - 0.75f * juce::jlimit(0.1f, 1.0f, relVel));
                auto releaseParams = configuredAdsr;
                releaseParams.release = dynamicRelease;
                adsrGate.setParameters(releaseParams);
            }
            adsrGate.noteOff();
            damperTransient.trigger(sampleRate, currentPlayingMidiNote, relVel);
            return;
        }

        adsrGate.reset();
        hammerTransient.reset();
        hammerContactEngine.reset();
        damperTransient.reset();
        mainString.reset();
        for (auto& resonator : bodyResonators) {
            resonator.reset();
        }
        spruceSoundboardFilter.reset();
        sympatheticPool.reset();
        duplexPool.reset();
        lidAcoustics.reset();
        perspectiveProcessor.reset();
        clearCurrentNote();
    }
    void pitchWheelMoved(int) override {
    }
    // Synchronizes the note-driven sympathetic gate from BuiltinSynthesiser's
    // channel truth at note start. This is deliberately not a MIDI/controller
    // event: it does not trigger the whole-piano pedal owner.
    void setSympatheticPedalDown(bool isDown) noexcept {
        sympatheticPool.setPedalDown(isDown);
    }

    void controllerMoved(int controllerNumber, int controllerValue) override {
        // MIDI CC 64 sustain state is applied by BuiltinSynthesiser's single
        // pedal-control entry point. The Voice only updates its note-driven
        // sympathetic gate here; the whole-piano pedal owner owns the
        // mechanical transient and shock.
        if (controllerNumber == 64) {
            setSympatheticPedalDown(controllerValue >= 64);
        } else if (controllerNumber == 67) {
            // MIDI CC 67 弱音/移位踏板 (Una Corda / Soft Pedal, Phase 29-B)
            softPedalDown = (controllerValue >= 64);
            softPedalAmount
                = softPedalDown ? juce::jlimit(0.0f, 1.0f, static_cast<float>(controllerValue) / 127.0f) : 0.0f;
        }
    }
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override {
        juce::ScopedNoDenormals noDenormals;
        auto isKeySounding = getCurrentlyPlayingNote() >= 0 && (adsrGate.isActive() || damperTransient.isActive());
        if (!isKeySounding && !duplexPool.isActive()) {
            if (getCurrentlyPlayingNote() >= 0) {
                lidAcoustics.reset();
                perspectiveProcessor.reset();
                clearCurrentNote();
            }
            return;
        }

        for (auto sample = 0; sample < numSamples; ++sample) {
            const auto sampleIndex = startSample + sample;
            const auto envelope = isKeySounding ? adsrGate.getNextSample() : 0.0f;
            if (isKeySounding && envelope <= 0.0f && !adsrGate.isActive() && !damperTransient.isActive()) {
                adsrGate.reset();
                for (auto& resonator : bodyResonators) {
                    resonator.reset();
                }
                spruceSoundboardFilter.reset();
                sympatheticPool.reset();
                isKeySounding = false;
            }
            const auto duplexMix = duplexPool.nextMix();

            if (!isKeySounding) {
                const auto duplexOut = duplexMix * duplexPool.process(0.0f) * noteOutputGain;
                if (duplexOut != 0.0f) {
                    auto outL = duplexOut * directLeftGain;
                    auto outR = duplexOut * directRightGain;
                    lidAcoustics.processStereo(outL, outR);
                    perspectiveProcessor.processStereo(outL, outR);

                    if (outputBuffer.getNumChannels() >= 2) {
                        outputBuffer.addSample(0, sampleIndex, outL);
                        outputBuffer.addSample(1, sampleIndex, outR);
                    } else if (outputBuffer.getNumChannels() == 1) {
                        // 39-A voice mono: fully processed stereo downmix,
                        // including lid/perspective tail. No extra copy.
                        outputBuffer.addSample(0, sampleIndex, 0.5f * (outL + outR));
                    }
                }
                continue;
            }

            const auto click = hammerTransient.getNextSample();
            const auto damperThump = damperTransient.getNextSample();
            const auto stringSample = mainString.nextSample();
            const auto valueLeft = stringSample.left;
            const auto valueRight = stringSample.right;
            // Hammer contact micro-damping and release (Phase 24-B, 39-A):
            // consume the existing HammerContactEngine exactly once per
            // sounding sample and apply its multiplier only to the string sum,
            // ahead of ADSR and pre-saturation. Hammer click and damper thump
            // bypass contact; duration/state are unchanged.
            const auto contactMult = hammerContactEngine.getReleaseMultiplier();
            const auto preSatLeft = valueLeft * contactMult * envelope + click + damperThump;
            const auto preSatRight = valueRight * contactMult * envelope + click + damperThump;
            // 音板与琴桥大动态软饱和 (Phase 22-D, Bilbao 2009)
            const auto rawLeft = softSaturate(preSatLeft);
            const auto rawRight = softSaturate(preSatRight);
            const auto rawMono = 0.5f * (rawLeft + rawRight);
            const auto duplexOut = duplexPool.process(rawMono);

            // 1. 16 峰物理云杉木音板模态 (Phase 19-A/B)
            auto resonatorLeftSum = 0.0f;
            auto resonatorRightSum = 0.0f;
            for (std::size_t i = 0; i < numResonators; ++i) {
                const auto& res = bodyResonators[i];
                const auto resOut = bodyResonators[i].process(rawMono);
                resonatorLeftSum += res.weightLeft * resOut;
                resonatorRightSum += res.weightRight * resOut;
            }

            // 2. 云杉木音板高频粘滞吸收低通滤波 (Phase 23-C, Boutillon & Ege 2013)
            spruceSoundboardFilter.processStereo(resonatorLeftSum, resonatorRightSum);

            // 3. 延音踏板与单键和弦开放弦交感共鸣 (Phase 21-A / Phase 22-E, Bank 2010 Sec. VI)
            const auto sympatheticOut = sympatheticPool.process(rawMono * noteOutputGain);

            // 4. 琴桥立体声声像定位与非对称空间投影 (低音在左 0.15 -> 高音在右 0.85)

            const auto wet = 0.18f + pianoResonance * 0.16f;
            auto outLeft = (1.0f - wet) * rawLeft * directLeftGain + wet * resonatorLeftSum;
            auto outRight = (1.0f - wet) * rawRight * directRightGain + wet * resonatorRightSum;
            outLeft = (1.0f - duplexMix) * outLeft + duplexMix * duplexOut * directLeftGain;
            outRight = (1.0f - duplexMix) * outRight + duplexMix * duplexOut * directRightGain;
            outLeft *= noteOutputGain;
            outRight *= noteOutputGain;
            const auto sympatheticMix = (1.0f - duplexMix) * wet * sympatheticOut;
            outLeft += 0.40f * sympatheticMix;
            outRight += 0.60f * sympatheticMix;

            // 5. 三角钢琴琴盖反射与近场木质微反射 (Phase 21-B / Phase 22-B, Chabassier 2013/2019)
            lidAcoustics.processStereo(outLeft, outRight);
            // 6. 双视角立体声场与声像转换 (Phase 31-A, PerspectiveProcessor: Player vs Audience)
            perspectiveProcessor.processStereo(outLeft, outRight);

            if (outputBuffer.getNumChannels() >= 2) {
                outputBuffer.addSample(0, sampleIndex, outLeft);
                outputBuffer.addSample(1, sampleIndex, outRight);
            } else if (outputBuffer.getNumChannels() == 1) {
                // Mono is the final lid/perspective-processed stereo downmix.
                outputBuffer.addSample(0, sampleIndex, 0.5f * (outLeft + outRight));
            }
        }

        if (isKeySounding && allPartialsSilent()) {
            adsrGate.reset();
            hammerTransient.reset();
            hammerContactEngine.reset();
            damperTransient.reset();
            mainString.reset();
            for (auto& resonator : bodyResonators) {
                resonator.reset();
            }
            spruceSoundboardFilter.reset();
            if (!sympatheticPool.pedalDown) {
                sympatheticPool.reset();
            }
        }
        if (getCurrentlyPlayingNote() >= 0 && !adsrGate.isActive() && !damperTransient.isActive()
            && !duplexPool.isActive()) {
            duplexPool.reset();
            lidAcoustics.reset();
            perspectiveProcessor.reset();
            clearCurrentNote();
        }
    }

private:
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

    [[nodiscard]] static double
    partialDecaySeconds(int midiNoteNumber, int partialIndex, float brightness = 0.5f, float resonance = 0.5f,
                        Temperament temperament = Temperament::equal,
                        double referencePitchA4 = TemperamentEngine::kDefaultReferencePitch) noexcept {
        const auto& params = devpiano::audio::getNoteParams(midiNoteNumber);
        const auto decayScale = 1.0f + (juce::jlimit(0.0f, 1.0f, resonance) - 0.5f) * 0.6f;
        const auto baseDecay = static_cast<double>(params.decaySeconds * decayScale);
        const auto f0 = devpiano::audio::pianoFirstPartialFrequency(midiNoteNumber, temperament, referencePitchA4)
            / std::sqrt(1.0 + params.inharmonicityB);
        const auto fn = partialFrequency(midiNoteNumber, partialIndex, temperament, referencePitchA4);
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
    [[nodiscard]] static double partialFrequency(int midiNoteNumber, int partialIndex,
                                                 Temperament temperament = Temperament::equal,
                                                 double referencePitchA4 = TemperamentEngine::kDefaultReferencePitch,
                                                 bool stretchTuningEnabled = true) noexcept {
        return devpiano::audio::pianoPartialFrequency(midiNoteNumber, partialIndex, temperament, referencePitchA4,
                                                      stretchTuningEnabled);
    }
    [[nodiscard]] static float beatingDetuneRatioForNote(int midiNoteNumber) noexcept {
        return devpiano::audio::getNoteParams(midiNoteNumber).beatingDetuneRatio;
    }
    [[nodiscard]] static int beatingPartialCountForNote(int midiNoteNumber) noexcept {
        return devpiano::audio::getNoteParams(midiNoteNumber).beatingPartials;
    }
    [[nodiscard]] static double
    beatingFrequency(int midiNoteNumber, int partialIndex, Temperament temperament = Temperament::equal,
                     double referencePitchA4 = TemperamentEngine::kDefaultReferencePitch) noexcept {
        const auto f = partialFrequency(midiNoteNumber, partialIndex, temperament, referencePitchA4);
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

    [[nodiscard]] bool allPartialsSilent() const noexcept {
        if (hammerTransient.isActive() || damperTransient.isActive()) {
            return false;
        }
        return mainString.isSilent();
    }

    int currentPlayingMidiNote = 60;
    std::uint32_t triggerCounter = 0;
    juce::ADSR::Parameters configuredAdsr { 0.0002f, 0.001f, 1.0f, 0.18f };
    juce::ADSR adsrGate;
    float pianoBrightness = 0.5f;
    float pianoHammerHardness = 0.5f;
    float pianoResonance = 0.5f;
    HammerVoicing hammerVoicing = devpiano::audio::kCalibratedHammerVoicing;
    LidPosition pianoLidPosition = LidPosition::fullOpen;
    Temperament pianoTemperament = Temperament::equal;
    double pianoReferencePitchA4 = TemperamentEngine::kDefaultReferencePitch;
    bool pianoStretchTuningEnabled = true;
    float directLeftGain = 0.7071f;
    float directRightGain = 0.7071f;
    float noteOutputGain = 1.0f;

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
            releaseProgress = 0.55f;
            progressInc = (1.0f - 0.55f) / static_cast<float>(total);
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

    devpiano::audio::HammerTransient hammerTransient;

    devpiano::audio::DamperTransient damperTransient;

    // 延音踏板全局交感共鸣弦池与单键开放弦交感 (Phase 21-A / Phase 22-E, Bank 2010 Sec. VI)
    devpiano::audio::SympatheticResonancePool sympatheticPool;

    struct DuplexResonance {
        struct Mode {
            double cosine = 0.0;
            double sine = 0.0;
            double drive = 0.0;
            double x = 0.0;
            double y = 0.0;
            bool enabled = false;

            void reset() noexcept {
                x = y = 0.0;
            }

            [[nodiscard]] float process(float input) noexcept {
                if (!enabled || (input == 0.0f && x == 0.0 && y == 0.0)) {
                    return 0.0f;
                }
                const auto nextX = cosine * x - sine * y + drive * static_cast<double>(input);
                y = sine * x + cosine * y;
                x = nextX;
                if (input == 0.0f && std::abs(x) + std::abs(y) < 1e-10) {
                    reset();
                }
                return static_cast<float>(x);
            }
        };

        std::array<Mode, 2> modes;
        float target = 0.15f;
        float amount = 0.15f;
        float mix = 0.018f;
        float smoothing = 0.0f;
        bool enabled = false;

        void setAmount(float value) noexcept {
            target = value;
        }

        void reset() noexcept {
            for (auto& mode : modes) {
                mode.reset();
            }
            amount = target;
            mix = 0.12f * amount;
        }

        void prepare(double sampleRate, double firstPartialFrequency, double stiffness) noexcept {
            reset();
            enabled = false;
            smoothing = static_cast<float>(std::exp(-1.0 / (0.005 * sampleRate)));
            constexpr std::array<double, 2> lengthRatios { 5.0, 3.02 };
            constexpr std::array<double, 2> decaySeconds { 0.09, 0.16 };
            for (std::size_t i = 0; i < modes.size(); ++i) {
                auto& mode = modes[i];
                const auto ratio = lengthRatios[i];
                const auto frequency
                    = firstPartialFrequency * ratio * std::sqrt((1.0 + stiffness * ratio * ratio) / (1.0 + stiffness));
                mode.enabled = frequency < sampleRate * 0.480;
                if (!mode.enabled) {
                    continue;
                }
                enabled = true;
                const auto radius = std::exp(-1.0 / (decaySeconds[i] * sampleRate));
                const auto angle = juce::MathConstants<double>::twoPi * frequency / sampleRate;
                const auto rotation = devpiano::audio::dsp::boundedSinCos(angle);
                mode.cosine = radius * rotation.cosVal;
                mode.sine = radius * rotation.sinVal;
                mode.drive = 1.0 - radius;
            }
        }

        [[nodiscard]] float nextMix() noexcept {
            if (!enabled) {
                return 0.0f;
            }
            if (!juce::exactlyEqual(amount, target)) {
                amount = target + smoothing * (amount - target);
                if (std::abs(amount - target) < 1e-5f) {
                    amount = target;
                    if (target == 0.0f) {
                        reset();
                    }
                }
                mix = 0.12f * amount;
            }
            return mix;
        }

        [[nodiscard]] bool isActive() const noexcept {
            if (mix <= 0.0f) {
                return false;
            }
            auto bound = 0.0;
            for (const auto& mode : modes) {
                if (mode.enabled) {
                    bound += std::abs(mode.x) + std::abs(mode.y);
                }
            }
            return 0.5 * static_cast<double>(mix) * bound > 1e-8;
        }

        [[nodiscard]] float process(float input) noexcept {
            if (!enabled || mix <= 0.0f) {
                return 0.0f;
            }
            return 0.5f * (modes[0].process(input) + modes[1].process(input));
        }
    };
    DuplexResonance duplexPool;

    devpiano::audio::LidAcoustics lidAcoustics;
    PerspectiveProcessor perspectiveProcessor;
    SoundPerspective pianoSoundPerspective = SoundPerspective::player;

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
            const auto safeFc = std::min(fc, static_cast<float>(sampleRate * 0.480));
            lpCoeff = std::exp(-juce::MathConstants<float>::twoPi * safeFc / static_cast<float>(sampleRate));
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
        float weightLeft = 0.40f;
        float weightRight = 0.40f;

        float c1 = 0.0f;
        float c2 = 0.0f;
        float g = 0.0f;
        float s1 = 0.0f;
        float s2 = 0.0f;

        double lastSampleRate = 0.0;

        void updateCoefficients(double sampleRate) noexcept {
            if (sampleRate <= 0.0 || std::abs(sampleRate - lastSampleRate) < 1e-6) {
                return;
            }
            lastSampleRate = sampleRate;
            const auto theta = juce::MathConstants<double>::twoPi * static_cast<double>(frequency) / sampleRate;
            const auto bandwidth = static_cast<double>(frequency / q);
            const auto r = std::exp(-juce::MathConstants<double>::pi * bandwidth / sampleRate);
            c1 = static_cast<float>(2.0 * r * devpiano::audio::dsp::boundedCos(theta));
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
            array[i].weightLeft = spec.weightLeft;
            array[i].weightRight = spec.weightRight;
        }
        return array;
    }();
    bool softPedalDown = false;
    float softPedalAmount = 0.0f;
    int voiceIndex = 0;
    float pianoFeltAgeingAmount = 0.0f;

private:
    // Explicit test-only observation of private DSP/helper state. Production
    // callers use semantic setters/render and never write these fields.
    friend class ::PianoSynthVoiceTest;
    friend class ::PianoTuningAndDuplexTest;
    friend class ::PedalAcousticsTest;
    devpiano::audio::PianoMainString mainString;
};
