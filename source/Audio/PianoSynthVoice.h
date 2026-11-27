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
#include "PianoWholeBodyBus.h"
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
using PianoBusInput = devpiano::audio::PianoBusInput;
using PianoWholeBodyBus = devpiano::audio::PianoWholeBodyBus;
// 内置物理建模钢琴合成器（Phase 12~24）：
// - 空气黏性阻尼与二次方摩擦 (Phase 18-C)：Desvages & Bilbao (2016) 模态耗散模型，呈现中频下凹歌唱性；
// - 24 峰正交云杉木音板模态 (Phase 19-A / 46-1)：Bank 2010 / Chabassier 2019 的 16 个锚点加 8 个中高频插值填充；
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
//   8kHz 云杉木粘滞内耗低通截止滤波器与 24 峰物理音板模态配平，赋予真实三角钢琴温润深厚的木质感；
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

    PianoSynthVoice() {
        static_cast<void>(devpiano::audio::getPianoStretchRatios());
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
    }

    void setWholeBodyBus(PianoWholeBodyBus* bus) noexcept {
        wholeBodyBus = bus;
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
    }

    [[nodiscard]] Temperament getTemperament() const noexcept {
        return pianoTemperament;
    }

    void setReferencePitchA4(double pitch) noexcept {
        pianoReferencePitchA4 = TemperamentEngine::clampReferencePitch(pitch);
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
            duplexPool.reset();
            damperTransient.updateSampleRate(newRate);
        }
    }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override {
        const auto sampleRate = getSampleRate();
        if (sampleRate <= 0.0) {
            return;
        }
        if (wholeBodyBus != nullptr && noteKeyActive) {
            wholeBodyBus->noteOffKey(currentPlayingMidiNote);
            noteKeyActive = false;
        }

        const auto& params = devpiano::audio::getNoteParams(midiNoteNumber);
        const auto triggerSequence = ++triggerCounter;
        const auto setup = devpiano::audio::computePianoNoteSetup(
            midiNoteNumber, velocity, params, pianoBrightness, pianoHammerHardness, pianoResonance,
            pianoFeltAgeingAmount, hammerVoicing, softPedalDown, softPedalAmount, pianoTemperament,
            pianoReferencePitchA4, pianoStretchTuningEnabled, triggerSequence);

        configureVoiceForNote(midiNoteNumber, setup);
        duplexPool.prepare(sampleRate, setup.firstPartialFrequency, setup.effectiveInharmonicityB);
        mainString.start(makeMainStringStart(params, setup));
        triggerMechanicalAttack(midiNoteNumber, setup, params);

        if (wholeBodyBus != nullptr) {
            wholeBodyBus->noteOnKey(midiNoteNumber);
            noteKeyActive = true;
        }
    }

    // 45-1: 同音且按键仍按下时复用同一个 voice 重新击弦。保留 mainString
    // 的振荡器相位与残余能量、声像、总线按键所有权和 NoteOff 身份；
    // 不调用 stopVoice/startVoice，也不分配第二个持久 voice。
    void retriggerNote(float velocity) {
        const auto sampleRate = getSampleRate();
        const auto midiNoteNumber = getCurrentlyPlayingNote();
        if (sampleRate <= 0.0 || midiNoteNumber < 0) {
            return;
        }

        const auto& params = devpiano::audio::getNoteParams(midiNoteNumber);
        const auto triggerSequence = ++triggerCounter;
        const auto setup = devpiano::audio::computePianoNoteSetup(
            midiNoteNumber, velocity, params, pianoBrightness, pianoHammerHardness, pianoResonance,
            pianoFeltAgeingAmount, hammerVoicing, softPedalDown, softPedalAmount, pianoTemperament,
            pianoReferencePitchA4, pianoStretchTuningEnabled, triggerSequence);

        configureVoiceForNote(midiNoteNumber, setup);
        // duplexPool 保留现有交感共振状态；重击只更新主弦激励。
        mainString.retrigger(makeMainStringStart(params, setup));
        triggerMechanicalAttack(midiNoteNumber, setup, params);

        if (wholeBodyBus != nullptr && !noteKeyActive) {
            wholeBodyBus->noteOnKey(midiNoteNumber);
            noteKeyActive = true;
        }
    }

    void stopNote(float velocity, bool allowTailOff) override {
        if (wholeBodyBus != nullptr && noteKeyActive) {
            wholeBodyBus->noteOffKey(currentPlayingMidiNote);
            noteKeyActive = false;
        }

        if (allowTailOff) {
            const auto sampleRate = getSampleRate();
            const auto relVel = velocity > 0.0f ? velocity : 0.6f;

            // 高音无制音器区：松键不进入 ADSR 释放门控，让弦自然衰减至
            // allPartialsSilent() 后释放声部。木质回弹仍由 DamperTransient
            // 提供；allowTailOff=false 的 Panic/硬停/偷声部路径保持立即重置。
            if (currentPlayingMidiNote > 88) {
                damperTransient.trigger(sampleRate, currentPlayingMidiNote, relVel);
                return;
            }

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
        duplexPool.reset();
        clearCurrentNote();
    }
    void pitchWheelMoved(int) override {
    }
    void controllerMoved(int controllerNumber, int controllerValue) override {
        // CC64 sustain is aggregated by BuiltinSynthesiser and applied to the
        // shared whole-piano bus; the voice only handles CC67 una corda.
        if (controllerNumber == 67) {
            // MIDI CC 67 弱音/移位踏板 (Una Corda / Soft Pedal, Phase 29-B)
            softPedalDown = (controllerValue >= 64);
            softPedalAmount
                = softPedalDown ? juce::jlimit(0.0f, 1.0f, static_cast<float>(controllerValue) / 127.0f) : 0.0f;
        }
    }
    // Piano voices render into the shared whole-body bus; direct per-voice
    // rendering is unsupported after Phase 43-1. BuiltinSynthesiser is the
    // only production caller.
    void renderNextBlock(juce::AudioBuffer<float>&, int, int) override {
    }

    void renderToBus(PianoBusInput& input, int startSample, int numSamples) noexcept {
        juce::ScopedNoDenormals noDenormals;
        auto isKeySounding = getCurrentlyPlayingNote() >= 0 && (adsrGate.isActive() || damperTransient.isActive());
        if (!isKeySounding && !duplexPool.isActive()) {
            if (getCurrentlyPlayingNote() >= 0) {
                clearCurrentNote();
            }
            return;
        }

        for (auto sample = 0; sample < numSamples; ++sample) {
            const auto sampleIndex = startSample + sample;
            const auto envelope = isKeySounding ? adsrGate.getNextSample() : 0.0f;
            if (isKeySounding && envelope <= 0.0f && !adsrGate.isActive() && !damperTransient.isActive()) {
                adsrGate.reset();
                isKeySounding = false;
            }
            const auto duplexMix = duplexPool.nextMix();

            if (!isKeySounding) {
                const auto duplexOut = duplexMix * duplexPool.process(0.0f) * noteOutputGain;
                if (duplexOut != 0.0f) {
                    input.add(sampleIndex, duplexOut * directLeftGain, duplexOut * directRightGain, 0.0f);
                }
                continue;
            }

            // Pre-scale mechanical transients so the shared string output gain
            // leaves their net level at mechanicalOutputGain.
            const auto woodKnock = hammerTransient.getNextSample() * mechanicalPreGain;
            const auto longitudinal = hammerTransient.getNextLongitudinalSample() * mechanicalPreGain * noteOutputGain
                * kLongitudinalBodyDriveGain;
            const auto damperThump = damperTransient.getNextSample() * mechanicalPreGain;
            const auto stringSample = mainString.nextSample();
            const auto valueLeft = stringSample.left;
            const auto valueRight = stringSample.right;
            const auto contactMult = hammerContactEngine.getReleaseMultiplier();
            const auto preSatLeft = valueLeft * contactMult * envelope + woodKnock + damperThump;
            const auto preSatRight = valueRight * contactMult * envelope + woodKnock + damperThump;
            const auto rawLeft = softSaturate(preSatLeft);
            const auto rawRight = softSaturate(preSatRight);
            const auto rawMono = 0.5f * (rawLeft + rawRight);
            const auto duplexOut = duplexPool.process(rawMono);

            const auto wet = 0.18f + pianoResonance * 0.16f;
            const auto duplexGain = 1.0f - duplexMix;
            const auto dryLeft
                = (duplexGain * (1.0f - wet) * rawLeft * directLeftGain + duplexMix * duplexOut * directLeftGain)
                * noteOutputGain;
            const auto dryRight
                = (duplexGain * (1.0f - wet) * rawRight * directRightGain + duplexMix * duplexOut * directRightGain)
                * noteOutputGain;
            // 46-2: 只缩放进入共享音板/交感/纵波的 drive；直达声、partial
            // 阶数权重、音区输出增益与声像保持原路径。
            const auto drive = (rawMono * noteOutputGain * wet * duplexGain + longitudinal) * bridgeDriveWeight;
            input.add(sampleIndex, dryLeft, dryRight, drive);
        }

        if (isKeySounding && allPartialsSilent()) {
            adsrGate.reset();
            hammerTransient.reset();
            hammerContactEngine.reset();
            damperTransient.reset();
            mainString.reset();
        }
        if (getCurrentlyPlayingNote() >= 0 && !adsrGate.isActive() && !damperTransient.isActive()
            && !duplexPool.isActive()) {
            duplexPool.reset();
            clearCurrentNote();
        }
    }

private:
    [[nodiscard]] static float mechanicalRegisterGain(int midiNoteNumber) noexcept {
        const auto keyPos = std::clamp((static_cast<float>(midiNoteNumber) - 21.0f) / 87.0f, 0.0f, 1.0f);
        return 0.95f + 0.05f * keyPos;
    }

    // 46-2: 低音长桥/中音/高音短桥的三锚点驱动权重。低音略强、高音略弱；
    // 中间线性插值，音区边界没有离散跳变。该权重是工程桥耦合代理，
    // 不是实琴逐键测量，也不改变 partial 阶数权重或输出增益。
    [[nodiscard]] static float bridgeDriveWeightForNote(int midiNoteNumber) noexcept {
        const auto midi = std::clamp(midiNoteNumber, 21, 108);
        if (midi <= 60) {
            const auto position = static_cast<float>(midi - 21) / 39.0f;
            return 1.10f - 0.10f * position;
        }
        const auto position = static_cast<float>(midi - 60) / 48.0f;
        return 1.00f - 0.06f * position;
    }

    [[nodiscard]] static float softSaturate(float x) noexcept {
        const auto x2 = std::clamp(x * x, 0.0f, 1.0f);
        return x * (1.0f - 0.12f * x2);
    }

    [[nodiscard]] bool allPartialsSilent() const noexcept {
        if (hammerTransient.isActive() || damperTransient.isActive()) {
            return false;
        }
        return mainString.isSilent();
    }

    void configureVoiceForNote(int midiNoteNumber, const devpiano::audio::PianoNoteSetup& setup) noexcept {
        currentPlayingMidiNote = midiNoteNumber;
        noteOutputGain = devpiano::audio::pianoRegisterLoudnessGain(midiNoteNumber);
        // Mechanical action noise has its own shallow register curve. It is
        // pre-scaled before the shared string output gain so its net level is
        // mechanicalOutputGain rather than the string register curve.
        mechanicalOutputGain = mechanicalRegisterGain(midiNoteNumber);
        mechanicalPreGain = mechanicalOutputGain / juce::jmax(1e-6f, noteOutputGain);
        bridgeDriveWeight = bridgeDriveWeightForNote(midiNoteNumber);
        const auto directPan = 0.15f + 0.70f * setup.keyPos;
        // 42-4: 等功率声像；中央 pan=0.5 仍为 0.7071，两端不再有 +1.73 dB 翘曲。
        // boundedSinCos 只在起音/重击时计算，实时逐采样路径无库函数三角。
        const auto panAngle = static_cast<double>(directPan) * devpiano::audio::dsp::kHalfPi;
        const auto pan = devpiano::audio::dsp::boundedSinCos(panAngle);
        directLeftGain = static_cast<float>(pan.cosVal);
        directRightGain = static_cast<float>(pan.sinVal);
    }

    [[nodiscard]] devpiano::audio::MainStringStart
    makeMainStringStart(const devpiano::audio::PianoNoteParams& params,
                        const devpiano::audio::PianoNoteSetup& setup) const noexcept {
        devpiano::audio::MainStringStart stringStart;
        stringStart.requestedPartials = params.partialCount;
        stringStart.stringCount = params.stringCount;
        stringStart.beatingPartials = params.beatingPartials;
        stringStart.beatingDetuneRatio = params.beatingDetuneRatio;
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
        stringStart.slowWeight = setup.slowWeight;
        return stringStart;
    }

    void triggerMechanicalAttack(int midiNoteNumber, const devpiano::audio::PianoNoteSetup& setup,
                                 const devpiano::audio::PianoNoteParams& params) {
        const auto sampleRate = getSampleRate();
        const auto hammerTransientVel = setup.clampedVelocity * (1.0f - 0.35f * setup.unaCordaMu);
        // The wood knock and longitudinal precursor are additive mechanical
        // transients, not part of the coherent string sum. The precursor is
        // routed separately into the shared body drive; do not inherit the
        // string unison phase-compensation gain.
        hammerTransient.trigger(sampleRate, midiNoteNumber, hammerTransientVel, setup.hammerHardness,
                                params.stringLength, 1.0f);
        hammerContactEngine.trigger(sampleRate, setup.effectiveTc);
        mainString.triggerSpatialDiffusion();
        damperTransient.reset();
        mainString.triggerPitchGlide(setup.clampedVelocity);
        adsrGate.setSampleRate(sampleRate);
        adsrGate.setParameters(configuredAdsr);
        adsrGate.noteOn();
    }

    int currentPlayingMidiNote = 60;
    std::uint32_t triggerCounter = 0;
    juce::ADSR::Parameters configuredAdsr { 0.0002f, 0.001f, 1.0f, 0.18f };
    juce::ADSR adsrGate;
    float pianoBrightness = 0.5f;
    float pianoHammerHardness = 0.5f;
    float pianoResonance = 0.5f;
    HammerVoicing hammerVoicing = devpiano::audio::kCalibratedHammerVoicing;
    Temperament pianoTemperament = Temperament::equal;
    double pianoReferencePitchA4 = TemperamentEngine::kDefaultReferencePitch;
    bool pianoStretchTuningEnabled = true;
    float directLeftGain = 0.7071f;
    float directRightGain = 0.7071f;
    float noteOutputGain = 1.0f;
    float mechanicalOutputGain = 1.0f;
    float mechanicalPreGain = 1.0f;
    float bridgeDriveWeight = 1.0f;

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

    // Phase 43-1 whole-piano owner. The voice contributes dry/drive samples
    // and note-key events; it does not own soundboard/sympathetic/lid state.
    PianoWholeBodyBus* wholeBodyBus = nullptr;
    bool noteKeyActive = false;

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

    bool softPedalDown = false;
    float softPedalAmount = 0.0f;
    int voiceIndex = 0;
    float pianoFeltAgeingAmount = 0.0f;

private:
    // The longitudinal precursor is an independent bass excitation. Keep it
    // inside the shared body/sympathetic drive instead of adding it to dry
    // stereo; 0.65 keeps the isolated metallic ping subordinate to the board.
    static constexpr float kLongitudinalBodyDriveGain = 0.65f;
    // Explicit test-only observation of private DSP/helper state. Production
    // callers use semantic setters/render and never write these fields.
    friend class ::PianoSynthVoiceTest;
    friend class ::PianoTuningAndDuplexTest;
    friend class ::PedalAcousticsTest;
    devpiano::audio::PianoMainString mainString;
};
