#pragma once

#include "Piano88KeyTable.h"
#include "PianoDspMath.h"
#include "PianoSourceLevels.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <juce_core/juce_core.h>

class PianoSynthVoiceTest;

// Main-string/partial core extracted from PianoSynthVoice for R9.
// This file owns only partial admission/installation, oscillator recursion and
// the pitch-glide/spatial-diffusion state needed by that recursion. It has no
// Voice pointer and no dependency on PianoSynthVoice.h. All expressions are
// moved from the R7 implementation; do not change their order or types here.
namespace devpiano::audio::dsp {

struct UnisonExcitation {
    std::array<float, 3> phase {};
};

inline const auto gUnisonExcitation = [] {
    std::array<std::array<UnisonExcitation, 64>, 88> result {};
    // Follow the existing MIDI 80..100 detune taper when approaching single-string equivalence.
    const auto fullSpreadDetune = devpiano::audio::getNoteParams(80).beatingDetuneRatio;
    for (std::size_t key = 0; key < result.size(); ++key) {
        const auto& params = devpiano::audio::getNoteParams(21 + static_cast<int>(key));
        const auto spread = std::clamp(params.beatingDetuneRatio / fullSpreadDetune, 0.0f, 1.0f);
        for (std::size_t partial = 0; partial < result[key].size(); ++partial) {
            auto& entry = result[key][partial];
            const auto base = devpiano::audio::kOptPhaseTable[0][partial];
            for (std::size_t string = 0; string < entry.phase.size(); ++string) {
                const auto original = devpiano::audio::kOptPhaseTable[string][partial];
                const auto difference = std::remainder(static_cast<double>(original - base), kTwoPi);
                entry.phase[string] = spread >= 1.0f ? original : base + spread * static_cast<float>(difference);
            }
        }
    }
    return result;
}();

struct UnisonWindowPower {
    double reference = 1.0;
    double actual = 1.0;
};

[[nodiscard]] inline UnisonWindowPower unisonWindowPower(const std::array<float, 3>& phase, int strings,
                                                         double frequency, float detune) noexcept {
    if (strings <= 1) {
        return {};
    }
    // The registered early-body RMS window defines the amplitude reference.
    // This is one note-wide calibration scalar, not physical energy conservation.
    constexpr auto duration = 0.2;
    constexpr auto midpoint = 0.2;
    auto reference = static_cast<double>(strings);
    auto actual = reference;
    for (auto first = 0; first < strings; ++first) {
        for (auto second = first + 1; second < strings; ++second) {
            const auto difference = frequency * static_cast<double>(detune) * static_cast<double>(second - first);
            const auto halfWindow = kPi * difference * duration;
            const auto sinc = halfWindow == 0.0 ? 1.0 : boundedSin(halfWindow) / halfWindow;
            const auto meanPhase = kTwoPi * difference * midpoint;
            const auto phaseDifference
                = static_cast<double>(phase[static_cast<std::size_t>(second)] - phase[static_cast<std::size_t>(first)]);
            reference += 2.0 * sinc * boundedCos(meanPhase);
            actual += 2.0 * sinc * boundedCos(meanPhase + phaseDifference);
        }
    }
    const auto divisor = static_cast<double>(strings * strings);
    return { std::max(0.0, reference / divisor), std::max(0.0, actual / divisor) };
}

} // namespace devpiano::audio::dsp

namespace devpiano::audio {

// Exact subset of PianoNoteParams/PianoNoteSetup consumed by the old inline
// install. It is a value command, not a retained parameter snapshot.
struct MainStringStart {
    int requestedPartials = 0;
    int stringCount = 1;
    int beatingPartials = 0;
    float beatingDetuneRatio = 0.0f;
    int unisonKeyIndex = 0;

    double baseFrequency = 0.0;
    double effectiveInharmonicityB = 0.0;
    float keyPos = 0.0f;
    float contactHardness = 0.0f;
    float effectiveStrikePos = 0.0f;
    float effectiveTc = 0.0f;
    float effectiveBrightness = 0.0f;
    float pianoBrightness = 0.0f;
    float hammerVoicingRolloffDelta = 0.0f;
    float velocityLevel = 0.0f;
    float clampedVelocity = 0.0f;
    float unaCordaMu = 0.0f;
    float jitterPhase = 0.0f;
    double baseDecaySeconds = 0.0;
    double k1 = 0.0;
    double alpha1 = 0.0;

    float b1 = 0.0f;
    float b2 = 0.0f;
    float fastDecayRatio = 0.0f;
    float slowWeight = 0.0f;
};

struct MainStringSample {
    float left = 0.0f;
    float right = 0.0f;
};

class PianoMainString final {
public:
    // Sets only the sample rate used by start and modulation triggers. It does
    // not clear partials or engines; the old playback-rate path did not clear
    // partial state either.
    void prepare(double newSampleRate) noexcept {
        sampleRate = newSampleRate;
    }

    // Installs the admitted partial prefix and returns the existing
    // unisonScale. Pitch glide and diffusion are triggered separately at their
    // original start-note call positions.
    [[nodiscard]] float start(const MainStringStart& start) noexcept {
        if (sampleRate <= 0.0) {
            return 1.0f;
        }

        const auto sr = sampleRate;
        activePartials = start.requestedPartials;

        // 39-A admission and budget: the highest detuned string of each
        // partial decides admission against the coherent ceiling, so every
        // active oscillator stays within the epsilon band. The L1 budget
        // denominator sums nominal weights over the full requested set, so
        // pruned weights are not redistributed into survivors (rendered
        // total simply drops when band-limited). Both passes use the
        // requested count for the brightness tilt; the fallback first
        // partial uses the ceiling frequency in numerator and denominator
        // consistently.
        const auto coherentCeiling = sr * kCoherentBandRatio;
        std::array<double, maxPartials> noteFrequencies {};
        const auto requestedPartials = activePartials;
        auto admittedCount = 0;
        for (auto n = 0; n < requestedPartials; ++n) {
            const auto partialNumber = static_cast<double>(n + 1);
            const auto inharmonicFactor
                = std::sqrt(1.0 + start.effectiveInharmonicityB * partialNumber * partialNumber);
            const auto partialFrequency = start.baseFrequency * partialNumber * inharmonicFactor;
            noteFrequencies[static_cast<std::size_t>(n)] = partialFrequency;
            if (n == admittedCount && highestDetunedFrequency(n, start, partialFrequency) < coherentCeiling) {
                admittedCount = n + 1;
            }
        }
        auto fallbackActive = false;
        if (admittedCount <= 0) {
            // Low-rate sounding fallback: keep the first partial only, at the
            // coherent ceiling.
            noteFrequencies[0] = coherentCeiling;
            admittedCount = 1;
            fallbackActive = true;
        }
        activePartials = admittedCount;
        const auto& unisonExcitation
            = devpiano::audio::dsp::gUnisonExcitation[static_cast<std::size_t>(start.unisonKeyIndex)];
        auto normSum = 0.0f;
        auto nominalPower = 0.0f;
        auto coherentPower = 0.0f;
        for (auto n = 0; n < requestedPartials; ++n) {
            const auto frequency
                = (fallbackActive && n == 0) ? coherentCeiling : noteFrequencies[static_cast<std::size_t>(n)];
            const auto weight = retainedPartialWeight(n, start.keyPos, start.contactHardness, start.effectiveStrikePos,
                                                      frequency, start.effectiveTc, requestedPartials,
                                                      start.effectiveBrightness, start.hammerVoicingRolloffDelta);
            const auto stringCount
                = n < start.beatingPartials && start.beatingDetuneRatio > 0.0f ? start.stringCount : 1;
            // One whole-voice scalar keeps the relative modal weights intact.
            // No per-partial boost or per-key correction table is applied.
            const auto power
                = devpiano::audio::dsp::unisonWindowPower(unisonExcitation[static_cast<std::size_t>(n % 64)].phase,
                                                          stringCount, frequency, start.beatingDetuneRatio);
            normSum += weight;
            nominalPower += weight * weight * static_cast<float>(power.reference);
            coherentPower += weight * weight * static_cast<float>(power.actual);
        }
        const auto unisonScale = std::sqrt(nominalPower / juce::jmax(1e-12f, coherentPower));
        const auto scale = devpiano::audio::peakLevelAtFullVelocity * unisonScale / juce::jmax(1e-6f, normSum);

        for (auto k = admittedCount; k < maxPartials; ++k) {
            auto& p = partials[static_cast<std::size_t>(k)];
            p.level = 0.0f;
            p.levelFast = 0.0f;
            p.levelSlow = 0.0f;
            p.epsilon = 0.0;
            p.epsilon2 = 0.0;
            p.epsilon3 = 0.0;
            p.stringCount = 1;
            p.decayFastPerSample = 0.0f;
            p.decaySlowPerSample = 0.0f;
            p.bloomGain = 1.0f;
            p.bloomRisePerSample = 0.0f;
        }
        for (auto n = 0; n < activePartials; ++n) {
            auto& partial = partials[static_cast<std::size_t>(n)];
            const auto m = static_cast<double>(n + 1);
            const auto partialFrequency = noteFrequencies[static_cast<std::size_t>(n)];
            partial.stringCount = start.stringCount;
            // Original phase signature below the existing treble detune taper;
            // phase differences converge with detune instead of jumping at zero.
            const auto& excitation = unisonExcitation[static_cast<std::size_t>(n % 64)];
            const auto phase1 = excitation.phase[0] + start.jitterPhase;
            const auto phase2 = excitation.phase[1] + start.jitterPhase;
            const auto phase3 = excitation.phase[2] + start.jitterPhase;

            // 39-A unit-y Magic Circle init: every active string starts at
            // its output phase with y = sin(phi) and
            // x = sqrt(1 - e^2/4) * cos(phi) + e * sin(phi) / 2. The stored
            // level includes the note-wide unison reference scale and una-corda
            // attenuation; pruned weights are not redistributed.
            if (partial.stringCount == 1 || n >= start.beatingPartials || start.beatingDetuneRatio <= 0.0f) {
                partial.stringCount = 1;
                initMagicCircleState(partial.cosState, partial.sinState, partial.epsilon, partialFrequency, sr, phase1);
                partial.epsilon2 = 0.0;
                partial.epsilon3 = 0.0;
            } else if (partial.stringCount == 2) {
                const auto detuneHalf = static_cast<double>(start.beatingDetuneRatio * 0.5f);
                const auto f1 = partialFrequency * (1.0 - detuneHalf);
                const auto f2 = partialFrequency * (1.0 + detuneHalf);
                initMagicCircleState(partial.cosState, partial.sinState, partial.epsilon, f1, sr, phase1);
                initMagicCircleState(partial.cosState2, partial.sinState2, partial.epsilon2, f2, sr, phase2);
                partial.epsilon3 = 0.0;
            } else {
                const auto detune = static_cast<double>(start.beatingDetuneRatio);
                const auto f1 = partialFrequency * (1.0 - detune);
                const auto f2 = partialFrequency;
                const auto f3 = partialFrequency * (1.0 + detune);
                initMagicCircleState(partial.cosState, partial.sinState, partial.epsilon, f1, sr, phase1);
                initMagicCircleState(partial.cosState2, partial.sinState2, partial.epsilon2, f2, sr, phase2);
                initMagicCircleState(partial.cosState3, partial.sinState3, partial.epsilon3, f3, sr, phase3);
            }
            // Una-corda trichord energy (Phase 29-B, unchanged mechanism).
            const auto trichordAtten
                = (start.stringCount == 3) ? (1.0f - 0.30f * start.unaCordaMu) : (1.0f - 0.15f * start.unaCordaMu);
            partial.level = retainedPartialWeight(n, start.keyPos, start.contactHardness, start.effectiveStrikePos,
                                                  partialFrequency, start.effectiveTc, requestedPartials,
                                                  start.effectiveBrightness, start.hammerVoicingRolloffDelta)
                * scale * start.velocityLevel * trichordAtten;
            if (n >= 2 && start.clampedVelocity > 0.40f) {
                const auto bloomDepth = 0.35f * (start.clampedVelocity - 0.40f) / 0.60f;
                partial.bloomGain = 1.0f - bloomDepth;
                const auto tauBloom = static_cast<double>(
                    0.008f + 0.016f * (1.0f - static_cast<float>(n) / static_cast<float>(requestedPartials)));
                partial.bloomRisePerSample = static_cast<float>(1.0 / (tauBloom * sr));
            } else {
                partial.bloomGain = 1.0f;
                partial.bloomRisePerSample = 0.0f;
            }

            const auto kn = m * m * start.k1;
            const auto airTerm = std::sqrt(start.baseFrequency / std::max(partialFrequency, 20.0));
            const auto alpha_n
                = static_cast<double>(start.b1) * (0.80 + 0.20 * airTerm) + static_cast<double>(start.b2) * kn;

            const auto dampingEffect
                = (alpha_n / juce::jmax(1e-9, start.alpha1)) * static_cast<double>(1.5f - start.pianoBrightness);
            const auto unaCordaDamping = 1.0
                + 0.15 * static_cast<double>(start.unaCordaMu)
                    * (static_cast<double>(n) / static_cast<double>(requestedPartials));
            const auto tau_m = (start.baseDecaySeconds / dampingEffect) / unaCordaDamping;
            const auto tauFast_m = tau_m * static_cast<double>(start.fastDecayRatio);
            partial.decayFastPerSample = static_cast<float>(std::exp(-1.0 / (tauFast_m * sr)));
            partial.decaySlowPerSample = static_cast<float>(std::exp(-1.0 / (tau_m * sr)));
            partial.levelFast = partial.level * (1.0f - start.slowWeight);
            partial.levelSlow = partial.level * start.slowWeight;
        }

        return unisonScale;
    }

    void triggerSpatialDiffusion() noexcept {
        spatialDiffusion.trigger(sampleRate);
    }

    void triggerPitchGlide(float velocity) noexcept {
        pitchGlide.trigger(sampleRate, velocity);
    }

    // Advances every active partial once and returns the pre-contact stereo
    // string sum. No block processing and no allocation.
    [[nodiscard]] MainStringSample nextSample() noexcept {
        const auto glideMult = static_cast<double>(pitchGlide.getGlideMultiplier());
        const auto diffusionFactor = spatialDiffusion.getDiffusionFactor();

        // 同音多弦立体声非对称空间展开 (Phase 23-B / Phase 24-C, Dynamic Spatial Diffusion)
        auto valueLeft = 0.0f;
        auto valueRight = 0.0f;
        for (auto n = 0; n < activePartials; ++n) {
            auto& partial = partials[static_cast<std::size_t>(n)];
            auto oscL = static_cast<float>(partial.sinState);
            auto oscR = oscL;
            const auto effEps
                = std::clamp(partial.epsilon * glideMult, -kMagicCircleEpsilonMax, kMagicCircleEpsilonMax);

            if (partial.stringCount == 2) {
                const auto s1 = static_cast<float>(partial.sinState);
                const auto s2 = static_cast<float>(partial.sinState2);
                const auto sSum = 0.5f * (s1 + s2);
                const auto sDiff = 0.5f * (s2 - s1);
                const auto effSpread = 0.20f * (0.60f + 0.40f * diffusionFactor);
                oscL = sSum - effSpread * sDiff;
                oscR = sSum + effSpread * sDiff;
                const auto effEps2
                    = std::clamp(partial.epsilon2 * glideMult, -kMagicCircleEpsilonMax, kMagicCircleEpsilonMax);
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

                const auto effEps2
                    = std::clamp(partial.epsilon2 * glideMult, -kMagicCircleEpsilonMax, kMagicCircleEpsilonMax);
                const auto nextCos2 = partial.cosState2 - effEps2 * partial.sinState2;
                partial.sinState2 += effEps2 * nextCos2;
                partial.cosState2 = nextCos2;
                const auto effEps3
                    = std::clamp(partial.epsilon3 * glideMult, -kMagicCircleEpsilonMax, kMagicCircleEpsilonMax);
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
            if (partialAmp > 0.0f) {
                valueLeft += partialAmp * oscL;
                valueRight += partialAmp * oscR;
            }

            const auto nextCos = partial.cosState - effEps * partial.sinState;
            partial.sinState += effEps * nextCos;
            partial.cosState = nextCos;
            partial.levelFast = (partial.levelFast > 1e-7f) ? (partial.levelFast * partial.decayFastPerSample) : 0.0f;
            partial.levelSlow = (partial.levelSlow > 1e-7f) ? (partial.levelSlow * partial.decaySlowPerSample) : 0.0f;
        }
        return { valueLeft, valueRight };
    }

    // Partial-only silence. Voice adds hammer/damper/ADSR/Duplex conditions.
    [[nodiscard]] bool isSilent() const noexcept {
        for (auto n = 0; n < activePartials; ++n) {
            const auto& partial = partials[static_cast<std::size_t>(n)];
            if (partial.levelFast + partial.levelSlow > silentLevelThreshold) {
                return false;
            }
        }
        return true;
    }

    // Hard-stop main-string reset. Partial slots are kept fixed and overwritten
    // by the next start, matching the old hard-stop path's string state.
    void reset() noexcept {
        activePartials = 0;
        pitchGlide.reset();
        spatialDiffusion.reset();
    }

private:
    static constexpr int maxPartials = 20;
    static constexpr double kMagicCircleEpsilonMax = 1.995;
    static constexpr double kCoherentBandRatio = 0.477485;
    static constexpr float silentLevelThreshold = 1e-4f;

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
            const auto srFloat = static_cast<float>(sr);
            const auto totalSamples = static_cast<int>(0.012f * srFloat);
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

    [[nodiscard]] static double highestDetunedFrequency(int partialIndex, const MainStringStart& start,
                                                        double partialFrequency) noexcept {
        if (start.stringCount == 1 || partialIndex >= start.beatingPartials || start.beatingDetuneRatio <= 0.0f) {
            return partialFrequency;
        }
        if (start.stringCount == 2) {
            return partialFrequency * (1.0 + static_cast<double>(start.beatingDetuneRatio * 0.5f));
        }
        return partialFrequency * (1.0 + static_cast<double>(start.beatingDetuneRatio));
    }

    [[nodiscard]] float retainedPartialWeight(int partialIndex, float keyPos, float contactHardness,
                                              float effectiveStrikePos, double partialFrequency, float effectiveTc,
                                              int partialCount, float effectiveBrightness,
                                              float rolloffDelta = 0.0f) const noexcept {
        return amplitudeFor(partialIndex, keyPos, contactHardness, effectiveStrikePos, partialFrequency, effectiveTc,
                            rolloffDelta)
            * brightnessBoost(partialIndex, effectiveBrightness, partialCount);
    }

    static void initMagicCircleState(double& cosState, double& sinState, double& epsilon, double frequency, double sr,
                                     float phase) noexcept {
        // Fixed-epsilon Magic Circle: with y(0) = sin(phi) the ellipse
        // invariant gives x(0) = sqrt(1 - e^2/4) * cos(phi) + e * sin(phi)/2.
        epsilon = 2.0 * devpiano::audio::dsp::boundedSin(juce::MathConstants<double>::pi * frequency / sr);
        const auto clampedEps = std::clamp(epsilon, -kMagicCircleEpsilonMax, kMagicCircleEpsilonMax);
        epsilon = clampedEps;
        const auto sc = devpiano::audio::dsp::boundedSinCos(static_cast<double>(phase));
        sinState = sc.sinVal;
        const auto halfEps = clampedEps * 0.5;
        const auto root = std::sqrt(std::max(0.0, 1.0 - halfEps * halfEps));
        cosState = root * sc.cosVal + halfEps * sc.sinVal;
    }

    [[nodiscard]] static float strikeCombGain(int partialIndex, float strikingRatio) noexcept {
        const auto m = static_cast<double>(partialIndex + 1);
        // 击弦点几何梳状陷波 (Chaigne & Askenfelt 1994, 消除 7~9 阶非协和刺耳分音)
        const auto angle = juce::MathConstants<double>::pi * m * static_cast<double>(strikingRatio);
        const auto raw = static_cast<float>(std::abs(devpiano::audio::dsp::boundedSin(angle)));
        return std::max(raw, 0.03f);
    }

    [[nodiscard]] static float hammerSpectrumGain(int partialIndex, float keyPos, float contactHardness,
                                                  double partialFrequency, float tc,
                                                  float rolloffDelta = 0.0f) noexcept {
        const auto m = static_cast<float>(partialIndex + 1);
        // 88 键音区基底幂次滚降: 低音 1.25 -> 高音 2.10 (保证低音丰富，高音突出基频)
        const auto rolloff = 1.20f + 0.30f * keyPos + 1.20f * (keyPos * keyPos);
        const auto basePowerRollOff = 1.0f / std::pow(m, rolloff);

        // 琴槌毛毡非线性低通滤波: 截止频率 fc = 2.5 / Tc (Chaigne & Askenfelt 1994)
        const auto hammerCutoff = 2.5f / std::max(tc, 0.0001f);
        // 滚降陡度随力度动态变化: pp 弱音更陡 (滤除高频), ff 强音更缓 (保留金属光泽)。
        // rolloffDelta 是三力度整音的可校准残差，仍作用于同一毛毡低通，不另加高频增益。
        const auto hammerRolloffExp = std::clamp(2.0f - 0.80f * contactHardness + rolloffDelta, 0.80f, 6.0f);

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
        const auto angle = juce::MathConstants<double>::pi * static_cast<double>(fTc);
        const auto cosineMod
            = std::min(static_cast<float>(std::abs(devpiano::audio::dsp::boundedCos(angle))) / std::abs(denom), 1.0f);
        return 0.7f + 0.3f * cosineMod;
    }

    [[nodiscard]] static float amplitudeFor(int partialIndex, float keyPos, float contactHardness, float strikingRatio,
                                            double partialFrequency, float tc, float rolloffDelta) noexcept {
        return strikeCombGain(partialIndex, strikingRatio)
            * hammerSpectrumGain(partialIndex, keyPos, contactHardness, partialFrequency, tc, rolloffDelta)
            * hammerElasticModulation(partialFrequency, tc) * bridgeHillGain(partialFrequency);
    }

    [[nodiscard]] static float brightnessBoost(int partialIndex, float brightness, int partialCount) noexcept {
        return 1.0f
            + (brightness - 0.5f) * 0.5f * (static_cast<float>(partialIndex) / static_cast<float>(partialCount));
    }

    std::array<Partial, maxPartials> partials;
    int activePartials = 0;
    double sampleRate = 0.0;
    PitchGlideEngine pitchGlide;
    SpatialDiffusionEngine spatialDiffusion;

    // Test-only access for generated-component invariants. No production
    // accessor or writable span is exposed through PianoSynthVoice.
    friend class ::PianoSynthVoiceTest;
};

} // namespace devpiano::audio
