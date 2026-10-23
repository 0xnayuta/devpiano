#pragma once

#include "PianoDspMath.h"
#include "PianoSineLookupTable.h"
#include "PianoSourceLevels.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <juce_core/juce_core.h>

// Mechanical transient components extracted verbatim from PianoSynthVoice.h
// for R3. They remain three independent value types; PianoSynthVoice keeps the
// same members, declaration order, and prepare/trigger/process/reset call order.
namespace devpiano::audio {

struct HammerTransient {
    static constexpr int longitudinalFadeStartMidi = 48;
    static constexpr int longitudinalEndMidi = 56;

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

    // The precursor remains a low-bass cue and fades smoothly instead of
    // switching off between adjacent semitones. The fade is an engineering
    // voicing boundary, not a measured longitudinal-mode cutoff.
    [[nodiscard]] static float longitudinalPrecursorGain(int midiNoteNumber) noexcept {
        if (midiNoteNumber >= longitudinalEndMidi) {
            return 0.0f;
        }
        if (midiNoteNumber <= longitudinalFadeStartMidi) {
            return 1.0f;
        }
        const auto span = static_cast<float>(longitudinalEndMidi - longitudinalFadeStartMidi);
        const auto t = static_cast<float>(longitudinalEndMidi - midiNoteNumber) / span;
        return t * t * (3.0f - 2.0f * t);
    }

    void trigger(double sr, int midiNoteNumber, float velocity, float hardness, float stringLength,
                 float hammerExcitationGain) noexcept {
        if (sr <= 0.0) {
            return;
        }
        const auto sampleRate = static_cast<float>(sr);
        const auto dur = juce::jlimit(0.0012f, 0.0030f, 0.0030f - static_cast<float>(midiNoteNumber) * 0.000015f);
        totalSamples = juce::jmax(1, static_cast<int>(dur * sampleRate));
        samplesRemaining = totalSamples;

        const auto nyquist = sampleRate * 0.480f;
        const auto rawF1 = 1100.0f + static_cast<float>(midiNoteNumber) * 12.0f;
        const auto rawF2 = 2600.0f + static_cast<float>(midiNoteNumber) * 18.0f;
        const auto f1 = std::min(juce::jlimit(900.0f, 2200.0f, rawF1), nyquist);
        const auto f2 = std::min(juce::jlimit(2200.0f, 4800.0f, rawF2), nyquist);
        phaseInc1 = f1 / sampleRate;
        phaseInc2 = f2 / sampleRate;
        oscPhase1 = 0.0f;
        oscPhase2 = 0.0f;

        const auto v = juce::jlimit(0.0f, 1.0f, velocity);
        const auto vLevel = (v * v) * (0.6f + 0.8f * hardness);
        amplitude = peakLevelAtFullVelocity * hammerExcitationGain * 0.12f * vLevel;
        decayPerSample = std::exp(-4.5f / static_cast<float>(totalSamples));

        // 起音 3ms 高频裂音 (Phase 23-D, Attack HF Crack, 消除起振延迟)
        const auto keyPos = std::clamp((static_cast<float>(midiNoteNumber) - 21.0f) / 87.0f, 0.0f, 1.0f);
        constexpr auto crackDur = 0.003f;
        const auto crackTotal = juce::jmax(1, static_cast<int>(crackDur * sampleRate));
        crackSamplesRemaining = crackTotal;
        crackAmplitude = peakLevelAtFullVelocity * hammerExcitationGain * 0.02f * (v * v) * (0.20f + 0.80f * keyPos);
        crackDecayPerSample = std::exp(-6.0f / static_cast<float>(crackTotal));
        crackRng = static_cast<std::uint32_t>(midiNoteNumber) * 1664525u + 1013904223u;

        // Hammer excitation is phase-compensated with the strings; this
        // independently generated precursor is not part of that coherent
        // sum and must not inherit its boost.
        const auto longGain = longitudinalPrecursorGain(midiNoteNumber);
        if (longGain > 0.0f) {
            constexpr auto vLongitudinal = 5100.0f;
            const auto fL1 = std::min(vLongitudinal / (2.0f * juce::jmax(0.10f, stringLength)), nyquist);
            const auto fL2 = std::min(2.0f * fL1, nyquist);
            const auto fL3 = std::min(3.0f * fL1, nyquist);
            longPhaseInc1 = fL1 / sampleRate;
            longPhaseInc2 = fL2 / sampleRate;
            longPhaseInc3 = fL3 / sampleRate;
            longPhase1 = 0.0f;
            longPhase2 = 0.0f;
            longPhase3 = 0.0f;

            const auto longTotalSamples = juce::jmax(1, static_cast<int>(0.015f * sampleRate));
            longSamplesRemaining = longTotalSamples;
            longAmplitude = peakLevelAtFullVelocity * 0.04f * vLevel * longGain;
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
            const auto s1 = devpiano::audio::dsp::gSineLookupTable.lookupNorm(oscPhase1);
            const auto s2 = devpiano::audio::dsp::gSineLookupTable.lookupNorm(oscPhase2);
            oscPhase1 += phaseInc1;
            if (oscPhase1 >= 1.0f) {
                oscPhase1 -= 1.0f;
            }
            oscPhase2 += phaseInc2;
            if (oscPhase2 >= 1.0f) {
                oscPhase2 -= 1.0f;
            }
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
            const auto l1 = devpiano::audio::dsp::gSineLookupTable.lookupNorm(longPhase1);
            const auto l2 = devpiano::audio::dsp::gSineLookupTable.lookupNorm(longPhase2);
            const auto l3 = devpiano::audio::dsp::gSineLookupTable.lookupNorm(longPhase3);
            longPhase1 += longPhaseInc1;
            if (longPhase1 >= 1.0f) {
                longPhase1 -= 1.0f;
            }
            longPhase2 += longPhaseInc2;
            if (longPhase2 >= 1.0f) {
                longPhase2 -= 1.0f;
            }
            longPhase3 += longPhaseInc3;
            if (longPhase3 >= 1.0f) {
                longPhase3 -= 1.0f;
            }
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
        oscPhase1 = 0.0f;
        oscPhase2 = 0.0f;
        longPhase1 = 0.0f;
        longPhase2 = 0.0f;
        longPhase3 = 0.0f;
    }

    [[nodiscard]] bool isActive() const noexcept {
        return samplesRemaining > 0 || crackSamplesRemaining > 0 || longSamplesRemaining > 0;
    }
};

// 制音器落弦与琴键释放机械瞬态深化 (Phase 22-A / Phase 32-B, Damper Felt Fall & Key Release Thump)
struct DamperTransient {
    struct BpCoefficients {
        float b0 = 0.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
    };
    BpCoefficients bpCoeffs;
    double lastSampleRate = 0.0;

    void updateSampleRate(double sr) noexcept {
        if (sr <= 0.0 || std::abs(sr - lastSampleRate) < 1e-6) {
            return;
        }
        lastSampleRate = sr;
        const auto f0 = 2800.0f;
        const auto q = 1.8f;
        const auto w0 = juce::MathConstants<double>::twoPi * static_cast<double>(f0) / sr;
        const auto sc = devpiano::audio::dsp::boundedSinCos(w0);
        const auto cosW0 = static_cast<float>(sc.cosVal);
        const auto sinW0 = static_cast<float>(sc.sinVal);
        const auto alpha = sinW0 / (2.0f * q);
        const auto a0 = 1.0f + alpha;
        bpCoeffs.b0 = alpha / a0;
        bpCoeffs.b1 = 0.0f;
        bpCoeffs.b2 = -alpha / a0;
        bpCoeffs.a1 = (-2.0f * cosW0) / a0;
        bpCoeffs.a2 = (1.0f - alpha) / a0;
    }
    // 1. 毛毡触弦低频接触瞬态 (Felt Contact Transient)
    int samplesRemaining = 0;
    int totalSamples = 0;
    float amplitude = 0.0f;
    float decayPerSample = 0.0f;
    float oscPhase1 = 0.0f;
    float phaseInc1 = 0.0f;
    float oscPhase2 = 0.0f;
    float phaseInc2 = 0.0f;

    // 2. 木质键体落回键床底部轻撞声 (Key Release Thump / Key Bed Return)
    int woodSamplesRemaining = 0;
    float woodAmplitude = 0.0f;
    float woodDecay = 0.0f;
    float woodPhase1 = 0.0f;
    float woodPhaseInc1 = 0.0f;
    float woodPhase2 = 0.0f;
    float woodPhaseInc2 = 0.0f;

    // 3. 毛毡纤维贴弦高频摩擦微脉冲 (Felt Friction Noise)
    int frictionSamplesRemaining = 0;
    float frictionAmplitude = 0.0f;
    float frictionDecay = 0.0f;
    float bpW1 = 0.0f, bpW2 = 0.0f;
    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    std::uint32_t rngState = 0x87654321;

    void trigger(double sr, int midiNoteNumber, float releaseVelocity) noexcept {
        if (sr <= 0.0) {
            return;
        }
        const auto sampleRate = static_cast<float>(sr);
        const auto rv = juce::jlimit(0.05f, 1.0f, releaseVelocity);
        const auto clampedNote = std::clamp(midiNoteNumber, 21, 108);
        const auto noteRatio = static_cast<float>(clampedNote - 21) / 87.0f;

        // 只有有阻尼器的音区 (一般 <= 88 键，即 F6 左右以下) 才有毛毡接触弦声
        const auto hasDamper = (midiNoteNumber <= 88);

        if (hasDamper) {
            // A. 速度自适应毛毡落弦摩擦与持续时间:
            // 慢离键摩擦接触时间长，快离键干脆截断
            const auto baseDur = juce::jlimit(0.008f, 0.026f, 0.026f - noteRatio * 0.016f);
            const auto velocityDurationScale = 1.6f - 0.9f * rv;
            const auto dur = baseDur * velocityDurationScale;
            totalSamples = juce::jmax(1, static_cast<int>(dur * sampleRate));
            samplesRemaining = totalSamples;

            const auto f1 = juce::jlimit(75.0f, 160.0f, 85.0f + noteRatio * 60.0f);
            const auto f2 = juce::jlimit(180.0f, 420.0f, 220.0f + noteRatio * 180.0f);
            phaseInc1 = f1 / sampleRate;
            phaseInc2 = f2 / sampleRate;
            oscPhase1 = 0.0f;
            oscPhase2 = 0.0f;

            const auto zoneGain = juce::jlimit(0.05f, 1.0f, 1.0f - noteRatio * 0.85f);
            amplitude = peakLevelAtFullVelocity * 0.075f * std::sqrt(rv) * zoneGain;

            const auto decayExponent = -(3.2f + 3.2f * rv);
            decayPerSample = std::exp(decayExponent / static_cast<float>(totalSamples));

            // B. 毛毡微摩擦高频噪声 (Felt Friction):
            // 慢离键摩擦显著，持续 15~30ms，中心 ~2800 Hz, Q ~ 1.8
            const auto frictionDur = juce::jlimit(0.012f, 0.032f, 0.032f * (1.3f - 0.6f * rv));
            frictionSamplesRemaining = juce::jmax(1, static_cast<int>(frictionDur * sampleRate));
            frictionAmplitude = peakLevelAtFullVelocity * 0.015f * (0.4f + 0.6f * (1.0f - rv)) * zoneGain;
            frictionDecay = std::exp(-5.0f / static_cast<float>(frictionSamplesRemaining));

            if (std::abs(sampleRate - lastSampleRate) >= 1e-6) {
                updateSampleRate(sampleRate);
            }
            b0 = bpCoeffs.b0;
            b1 = bpCoeffs.b1;
            b2 = bpCoeffs.b2;
            a1 = bpCoeffs.a1;
            a2 = bpCoeffs.a2;
            bpW1 = 0.0f;
            bpW2 = 0.0f;
        } else {
            samplesRemaining = 0;
            amplitude = 0.0f;
            frictionSamplesRemaining = 0;
            frictionAmplitude = 0.0f;
        }

        // C. 木质键体落回键床撞击声 (Key Release Thump): 全 88 键通用
        const auto woodDur = 0.018f;
        woodSamplesRemaining = juce::jmax(1, static_cast<int>(woodDur * sampleRate));
        const auto woodF1 = 140.0f - noteRatio * 25.0f;
        const auto woodF2 = 270.0f - noteRatio * 40.0f;
        woodPhaseInc1 = woodF1 / sampleRate;
        woodPhaseInc2 = woodF2 / sampleRate;
        woodPhase1 = 0.0f;
        woodPhase2 = 0.0f;
        woodAmplitude = peakLevelAtFullVelocity * 0.032f * (rv * rv);
        woodDecay = std::exp(-6.0f / static_cast<float>(woodSamplesRemaining));
    }

    [[nodiscard]] float getNextSample() noexcept {
        if (!isActive()) {
            return 0.0f;
        }
        auto out = 0.0f;

        // 1. 毛毡触弦声
        if (samplesRemaining > 0) {
            --samplesRemaining;
            const auto s1 = devpiano::audio::dsp::gSineLookupTable.lookupNorm(oscPhase1);
            const auto s2 = devpiano::audio::dsp::gSineLookupTable.lookupNorm(oscPhase2);
            oscPhase1 += phaseInc1;
            if (oscPhase1 >= 1.0f) {
                oscPhase1 -= 1.0f;
            }
            oscPhase2 += phaseInc2;
            if (oscPhase2 >= 1.0f) {
                oscPhase2 -= 1.0f;
            }
            out += amplitude * (0.75f * s1 + 0.25f * s2);
            amplitude *= decayPerSample;
        }

        // 2. 毛毡高频摩擦声
        if (frictionSamplesRemaining > 0) {
            --frictionSamplesRemaining;
            rngState ^= (rngState << 13);
            rngState ^= (rngState >> 17);
            rngState ^= (rngState << 5);
            const auto rawNoise = static_cast<float>(static_cast<std::int32_t>(rngState)) * (1.0f / 2147483648.0f);
            const auto w = rawNoise - a1 * bpW1 - a2 * bpW2;
            const auto filtered = b0 * w + b1 * bpW1 + b2 * bpW2;
            bpW2 = bpW1;
            bpW1 = w;
            out += filtered * frictionAmplitude;
            frictionAmplitude *= frictionDecay;
        }

        // 3. 木质键床撞击声
        if (woodSamplesRemaining > 0) {
            --woodSamplesRemaining;
            const auto w1 = devpiano::audio::dsp::gSineLookupTable.lookupNorm(woodPhase1);
            const auto w2 = devpiano::audio::dsp::gSineLookupTable.lookupNorm(woodPhase2);
            woodPhase1 += woodPhaseInc1;
            if (woodPhase1 >= 1.0f) {
                woodPhase1 -= 1.0f;
            }
            woodPhase2 += woodPhaseInc2;
            if (woodPhase2 >= 1.0f) {
                woodPhase2 -= 1.0f;
            }
            out += woodAmplitude * (0.70f * w1 + 0.30f * w2);
            woodAmplitude *= woodDecay;
        }

        return out;
    }

    void reset() noexcept {
        samplesRemaining = 0;
        amplitude = 0.0f;
        woodSamplesRemaining = 0;
        woodAmplitude = 0.0f;
        frictionSamplesRemaining = 0;
        frictionAmplitude = 0.0f;
        bpW1 = 0.0f;
        bpW2 = 0.0f;
        oscPhase1 = 0.0f;
        oscPhase2 = 0.0f;
        woodPhase1 = 0.0f;
        woodPhase2 = 0.0f;
    }

    [[nodiscard]] bool isActive() const noexcept {
        return samplesRemaining > 0 || woodSamplesRemaining > 0 || frictionSamplesRemaining > 0;
    }
};

// 延音踏板机械气流与箱体共鸣冲击 (Phase 32-A, Pedal Whoosh & Resonance Shock)
struct PedalTransient {
    struct BpCoefficients {
        float b0 = 0.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
    };
    BpCoefficients bpDown;
    BpCoefficients bpUp;
    double lastSampleRate = 0.0;

    void updateSampleRate(double sr) noexcept {
        if (sr <= 0.0 || std::abs(sr - lastSampleRate) < 1e-6) {
            return;
        }
        lastSampleRate = sr;

        auto computeBp = [sr](float f0, float q) noexcept -> BpCoefficients {
            BpCoefficients c;
            const auto w0 = juce::MathConstants<double>::twoPi * static_cast<double>(f0) / sr;
            const auto sc = devpiano::audio::dsp::boundedSinCos(w0);
            const auto cosW0 = static_cast<float>(sc.cosVal);
            const auto sinW0 = static_cast<float>(sc.sinVal);
            const auto alpha = sinW0 / (2.0f * q);
            const auto a0 = 1.0f + alpha;
            c.b0 = alpha / a0;
            c.b1 = 0.0f;
            c.b2 = -alpha / a0;
            c.a1 = (-2.0f * cosW0) / a0;
            c.a2 = (1.0f - alpha) / a0;
            return c;
        };

        bpDown = computeBp(1350.0f, 1.25f);
        bpUp = computeBp(950.0f, 1.1f);
    }
    int whooshSamplesRemaining = 0;
    int whooshTotalSamples = 0;
    float whooshAmplitude = 0.0f;
    float whooshDecay = 0.0f;
    float whooshRise = 0.0f;
    float whooshEnv = 0.0f;

    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float bpW1 = 0.0f, bpW2 = 0.0f;

    std::uint32_t rngState = 0x12345678;

    int shockSamplesRemaining = 0;
    float shockAmplitude = 0.0f;
    float shockDecay = 0.0f;
    float shockPhase1 = 0.0f;
    float shockPhaseInc1 = 0.0f;
    float shockPhase2 = 0.0f;
    float shockPhaseInc2 = 0.0f;

    // Pedal control truth and raw CC history live in BuiltinSynthesiser; this
    // mechanical value type intentionally owns no channel state.
    void trigger(double sr, bool isDown, float pedalVelocity, float noiseLevel) noexcept {
        if (sr <= 0.0 || noiseLevel <= 1e-4f) {
            return;
        }
        const auto sampleRate = static_cast<float>(sr);
        const auto vel = juce::jlimit(0.1f, 1.0f, pedalVelocity);
        const auto level = juce::jlimit(0.0f, 1.0f, noiseLevel);

        if (isDown) {
            // A. Whoosh 气流声: 持续 ~60ms
            const auto durSec = juce::jlimit(0.045f, 0.075f, 0.075f - vel * 0.020f);
            whooshTotalSamples = juce::jmax(1, static_cast<int>(durSec * sampleRate));
            whooshSamplesRemaining = whooshTotalSamples;
            whooshEnv = 0.0f;
            whooshRise = 1.0f / static_cast<float>(juce::jmax(1, static_cast<int>(0.012f * sampleRate)));
            whooshDecay = std::exp(-4.5f / static_cast<float>(whooshTotalSamples));
            whooshAmplitude = 0.038f * vel * level;

            if (std::abs(sampleRate - lastSampleRate) >= 1e-6) {
                updateSampleRate(sampleRate);
            }
            b0 = bpDown.b0;
            b1 = bpDown.b1;
            b2 = bpDown.b2;
            a1 = bpDown.a1;
            a2 = bpDown.a2;
            bpW1 = 0.0f;
            bpW2 = 0.0f;
            // B. Resonance Shock 低频瞬态冲击 (58 Hz & 116 Hz)
            const auto shockDurSec = 0.065f;
            shockSamplesRemaining = juce::jmax(1, static_cast<int>(shockDurSec * sampleRate));
            shockPhaseInc1 = 58.0f / sampleRate;
            shockPhaseInc2 = 116.0f / sampleRate;
            shockPhase1 = 0.0f;
            shockPhase2 = 0.0f;
            shockAmplitude = 0.025f * vel * level;
            shockDecay = std::exp(-5.0f / static_cast<float>(shockSamplesRemaining));
        } else {
            // 踏板抬起: 制音器下落回位与微弱阻尼触弦声
            const auto durSec = 0.035f;
            whooshTotalSamples = juce::jmax(1, static_cast<int>(durSec * sampleRate));
            whooshSamplesRemaining = whooshTotalSamples;
            whooshEnv = 0.0f;
            whooshRise = 1.0f / static_cast<float>(juce::jmax(1, static_cast<int>(0.008f * sampleRate)));
            whooshDecay = std::exp(-5.5f / static_cast<float>(whooshTotalSamples));
            whooshAmplitude = 0.018f * vel * level;

            if (std::abs(sampleRate - lastSampleRate) >= 1e-6) {
                updateSampleRate(sampleRate);
            }
            b0 = bpUp.b0;
            b1 = bpUp.b1;
            b2 = bpUp.b2;
            a1 = bpUp.a1;
            a2 = bpUp.a2;
            bpW1 = 0.0f;
            bpW2 = 0.0f;
            const auto shockDurSec = 0.040f;
            shockSamplesRemaining = juce::jmax(1, static_cast<int>(shockDurSec * sampleRate));
            shockPhaseInc1 = 65.0f / sampleRate;
            shockPhaseInc2 = 130.0f / sampleRate;
            shockPhase1 = 0.0f;
            shockPhase2 = 0.0f;
            shockAmplitude = 0.012f * vel * level;
            shockDecay = std::exp(-6.0f / static_cast<float>(shockSamplesRemaining));
        }
    }

    [[nodiscard]] float getNextSample() noexcept {
        if (!isActive()) {
            return 0.0f;
        }
        auto out = 0.0f;

        if (whooshSamplesRemaining > 0) {
            --whooshSamplesRemaining;
            rngState ^= (rngState << 13);
            rngState ^= (rngState >> 17);
            rngState ^= (rngState << 5);
            const auto rawNoise = static_cast<float>(static_cast<std::int32_t>(rngState)) * (1.0f / 2147483648.0f);

            const auto w = rawNoise - a1 * bpW1 - a2 * bpW2;
            const auto filtered = b0 * w + b1 * bpW1 + b2 * bpW2;
            bpW2 = bpW1;
            bpW1 = w;

            if (whooshEnv < 1.0f) {
                whooshEnv = std::min(1.0f, whooshEnv + whooshRise);
            } else {
                whooshAmplitude *= whooshDecay;
            }
            out += filtered * whooshAmplitude * whooshEnv;
        }

        if (shockSamplesRemaining > 0) {
            --shockSamplesRemaining;
            const auto s1 = devpiano::audio::dsp::gSineLookupTable.lookupNorm(shockPhase1);
            const auto s2 = devpiano::audio::dsp::gSineLookupTable.lookupNorm(shockPhase2);
            shockPhase1 += shockPhaseInc1;
            if (shockPhase1 >= 1.0f) {
                shockPhase1 -= 1.0f;
            }
            shockPhase2 += shockPhaseInc2;
            if (shockPhase2 >= 1.0f) {
                shockPhase2 -= 1.0f;
            }
            out += shockAmplitude * (0.65f * s1 + 0.35f * s2);
            shockAmplitude *= shockDecay;
        }

        return out;
    }

    void reset() noexcept {
        whooshSamplesRemaining = 0;
        whooshAmplitude = 0.0f;
        whooshEnv = 0.0f;
        bpW1 = 0.0f;
        bpW2 = 0.0f;
        shockSamplesRemaining = 0;
        shockAmplitude = 0.0f;
        shockPhase1 = 0.0f;
        shockPhase2 = 0.0f;
    }
    [[nodiscard]] bool isActive() const noexcept {
        return whooshSamplesRemaining > 0 || shockSamplesRemaining > 0;
    }
};

} // namespace devpiano::audio
