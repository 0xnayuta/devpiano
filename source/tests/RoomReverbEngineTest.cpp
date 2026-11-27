#include <JuceHeader.h>

#include "Audio/AudioEngine.h"
#include "Audio/RoomReverbEngine.h"

#include <numeric>

// ============================================================================
/// RoomReverbEngineTest (Phase 31-B / 46-3)
///
/// Validates mathematical algorithmic room reverberation:
/// 1. 8-line Householder topology, prime delays and orthogonal branch energy.
/// 2. Preset Schroeder T30 scaling: Studio < Chamber < Concert Hall.
/// 3. Wet level scaling and zero-wet transparency bypass.
/// 4. Numerical safety: denormal flushing to absolute zero, no NaN/Inf.
/// 5. Sample rate invariance (44.1 kHz, 48 kHz, 96 kHz, 192 kHz).
/// 6. AudioEngine atomic parameter state and block processing integration.
/// 7. Identifier serialization round-trip.
// ============================================================================
class RoomReverbEngineTest final : public juce::UnitTest {
public:
    RoomReverbEngineTest()
        : juce::UnitTest("RoomReverbEngine", "DevPiano/Audio") {
    }

    void runTest() override {
        testEightLineHouseholderTopology();
        testPresetsDecayCharacteristics();
        testWetLevelBypassAndScaling();
        testNumericalSafetyAndDenormalFlushing();
        testSampleRateInvariance();
        testStereoReflectionsAndCrossRateDecay();
        testAudioEngineIntegration();
        testMonoKeepsStereoRoomWet();
        testIdentifierRoundTrip();
    }

private:
    [[nodiscard]] static double measureRT30(double sampleRate, devpiano::audio::ReverbSpace space) {
        using namespace devpiano::audio;
        RoomReverbEngine reverb;
        reverb.prepare(sampleRate);
        reverb.setSpace(space);
        reverb.setWetLevel(1.0f, true);

        const auto totalSamples = static_cast<int>(std::lround(10.0 * sampleRate));
        std::vector<float> left(static_cast<std::size_t>(totalSamples), 0.0f);
        std::vector<float> right(static_cast<std::size_t>(totalSamples), 0.0f);
        left[0] = 1.0f;
        reverb.processStereo(left.data(), right.data(), totalSamples);

        std::vector<double> cumulative(static_cast<std::size_t>(totalSamples) + 1, 0.0);
        for (auto sample = totalSamples - 1; sample >= 0; --sample) {
            const auto index = static_cast<std::size_t>(sample);
            const auto energy = 0.5
                * (static_cast<double>(left[index]) * left[index] + static_cast<double>(right[index]) * right[index]);
            cumulative[index] = cumulative[index + 1] + energy;
        }
        const auto referenceEnergy = cumulative[0];
        if (referenceEnergy <= 0.0) {
            return -1.0;
        }

        auto timeAtMinus5 = -1.0;
        auto timeAtMinus35 = -1.0;
        for (auto sample = 0; sample < totalSamples; ++sample) {
            const auto index = static_cast<std::size_t>(sample);
            const auto decibels = 10.0 * std::log10(std::max(cumulative[index] / referenceEnergy, 1.0e-300));
            if (timeAtMinus5 < 0.0 && decibels <= -5.0) {
                timeAtMinus5 = static_cast<double>(sample) / sampleRate;
            }
            if (timeAtMinus35 < 0.0 && decibels <= -35.0) {
                timeAtMinus35 = static_cast<double>(sample) / sampleRate;
            }
        }
        if (timeAtMinus5 < 0.0 || timeAtMinus35 < 0.0) {
            return -1.0;
        }
        return 2.0 * (timeAtMinus35 - timeAtMinus5);
    }

    void testEightLineHouseholderTopology() {
        beginTest("RoomReverbEngine: 8 prime delays and orthogonal Householder feedback");
        using namespace devpiano::audio;
        expectEquals(RoomReverbEngine::kFdnSize, 8);
        expectEquals(RoomReverbEngine::kEarlyTaps, 4);

        RoomReverbEngine reverb;
        reverb.prepare(48000.0);
        for (std::size_t i = 0; i < RoomReverbEngine::kFdnSize; ++i) {
            expect(reverb.delaySamples[i] >= 32, "FDN delay must be at least one render block");
            for (std::size_t j = i + 1; j < RoomReverbEngine::kFdnSize; ++j) {
                expect(std::gcd(reverb.delaySamples[i], reverb.delaySamples[j]) == 1,
                       "FDN delay lengths must be pairwise coprime at 48 kHz");
            }
        }

        const std::array<float, RoomReverbEngine::kFdnSize> input { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f };
        const auto output = RoomReverbEngine::householderFeedback(input);
        auto inputEnergy = 0.0f;
        auto outputEnergy = 0.0f;
        auto inputSum = 0.0f;
        auto outputSum = 0.0f;
        for (std::size_t i = 0; i < RoomReverbEngine::kFdnSize; ++i) {
            inputEnergy += input[i] * input[i];
            outputEnergy += output[i] * output[i];
            inputSum += input[i];
            outputSum += output[i];
        }
        expectWithinAbsoluteError(outputEnergy, inputEnergy, 1.0e-4f,
                                  "Householder transform must preserve branch energy");
        expectWithinAbsoluteError(outputSum, -inputSum, 1.0e-4f,
                                  "Householder transform must invert the common-mode sum");
    }

    void testPresetsDecayCharacteristics() {
        beginTest("RoomReverbEngine: Preset decay scaling (Studio < Chamber < Concert Hall)");
        using namespace devpiano::audio;
        constexpr double sampleRate = 44100.0;

        const auto decayStudio = measureRT30(sampleRate, ReverbSpace::studio);
        const auto decayChamber = measureRT30(sampleRate, ReverbSpace::chamber);
        const auto decayConcert = measureRT30(sampleRate, ReverbSpace::concertHall);

        expect(decayStudio > 0.0, "Studio T30 must exist");
        expect(decayChamber > decayStudio, "Chamber T30 must be longer than Studio");
        expect(decayConcert > decayChamber, "Concert Hall T30 must be longer than Chamber");
    }

    void testWetLevelBypassAndScaling() {
        beginTest("RoomReverbEngine: Wet level bypass and scaling");

        using namespace devpiano::audio;

        RoomReverbEngine reverb;
        reverb.prepare(44100.0);
        reverb.setSpace(ReverbSpace::chamber);

        // 1. Zero wet level -> exact transparent bypass
        reverb.setWetLevel(0.0f, true);

        std::vector<float> left = { 0.5f, -0.2f, 0.8f, -0.9f, 0.1f };
        std::vector<float> right = { -0.4f, 0.3f, -0.7f, 0.6f, -0.2f };
        const auto origLeft = left;
        const auto origRight = right;

        reverb.processStereo(left.data(), right.data(), static_cast<int>(left.size()));

        for (std::size_t i = 0; i < left.size(); ++i) {
            expectEquals(left[i], origLeft[i]);
            expectEquals(right[i], origRight[i]);
        }

        // 2. Nullptr or zero length handling safely
        reverb.processStereo(nullptr, right.data(), 10);
        reverb.processStereo(left.data(), nullptr, 10);
        reverb.processStereo(left.data(), right.data(), 0);
        reverb.processStereo(left.data(), right.data(), -5);

        // 3. Clamping of wet levels
        reverb.setWetLevel(-0.5f);
        expectEquals(reverb.getWetLevel(), 0.0f);
        reverb.setWetLevel(1.5f);
        expectEquals(reverb.getWetLevel(), 1.0f);
        reverb.setWetLevel(0.25f);
        expectEquals(reverb.getWetLevel(), 0.25f);
    }

    void testNumericalSafetyAndDenormalFlushing() {
        beginTest("RoomReverbEngine: Denormal flushing and numerical stability");

        using namespace devpiano::audio;

        RoomReverbEngine reverb;
        reverb.prepare(44100.0);
        reverb.setSpace(ReverbSpace::concertHall);
        reverb.setWetLevel(0.5f);

        // Excite with a sharp pulse
        std::vector<float> left(256, 0.0f);
        std::vector<float> right(256, 0.0f);
        left[0] = 0.95f;
        right[0] = 0.95f;
        reverb.processStereo(left.data(), right.data(), 256);

        constexpr int kSilentBlockCount = 500;
        constexpr int kBlock = 512;
        std::vector<float> bufL(kBlock, 0.0f);
        std::vector<float> bufR(kBlock, 0.0f);

        std::size_t nonFiniteSampleValues = 0;
        for (int b = 0; b < kSilentBlockCount; ++b) {
            std::fill(bufL.begin(), bufL.end(), 0.0f);
            std::fill(bufR.begin(), bufR.end(), 0.0f);
            reverb.processStereo(bufL.data(), bufR.data(), kBlock);

            for (std::size_t i = 0; i < kBlock; ++i) {
                if (!std::isfinite(bufL[i])) {
                    ++nonFiniteSampleValues;
                }
                if (!std::isfinite(bufR[i])) {
                    ++nonFiniteSampleValues;
                }
            }
        }
        expectEquals(nonFiniteSampleValues, static_cast<std::size_t>(0));

        float finalL = 0.0f;
        float finalR = 0.0f;
        for (std::size_t i = 0; i < kBlock; ++i) {
            finalL = std::max(finalL, std::abs(bufL[i]));
            finalR = std::max(finalR, std::abs(bufR[i]));
        }
        expectEquals(finalL, 0.0f);
        expectEquals(finalR, 0.0f);
    }

    void testSampleRateInvariance() {
        beginTest("RoomReverbEngine: Sample rate invariance across common audio rates");

        using namespace devpiano::audio;

        const std::array<double, 4> sampleRates = { 44100.0, 48000.0, 96000.0, 192000.0 };

        for (const auto sr : sampleRates) {
            RoomReverbEngine reverb;
            reverb.prepare(sr);
            reverb.setSpace(ReverbSpace::chamber);
            reverb.setWetLevel(0.3f);

            std::vector<float> l(128, 0.0f);
            std::vector<float> r(128, 0.0f);
            l[0] = 1.0f;
            r[0] = 1.0f;

            reverb.processStereo(l.data(), r.data(), 128);

            expect(l[0] != 0.0f);
            expect(r[0] != 0.0f);
            expect(!std::isnan(l[0]) && !std::isinf(l[0]));
            expect(!std::isnan(r[0]) && !std::isinf(r[0]));
        }
    }

    void testStereoReflectionsAndCrossRateDecay() {
        beginTest("RoomReverbEngine: stereo early reflections and cross-rate decay");

        using namespace devpiano::audio;

        const auto decay44 = measureRT30(44100.0, ReverbSpace::chamber);
        const auto decay48 = measureRT30(48000.0, ReverbSpace::chamber);
        const auto decay96 = measureRT30(96000.0, ReverbSpace::chamber);
        expect(decay44 > 1.0 && decay44 < 8.0, "chamber T30 has a bounded time-domain length");
        // 容差由实测跨率差异决定：延迟取整与模态分布仍带来约 15% 偏差，
        // 这里只拦截数量级错误，不用过紧阈值掩盖或伪造采样率不变性。
        expectWithinAbsoluteError(decay48, decay44, std::max(0.35, 0.30 * decay44));
        expectWithinAbsoluteError(decay96, decay44, std::max(0.35, 0.30 * decay44));

        RoomReverbEngine reverb;
        reverb.prepare(48000.0);
        reverb.setSpace(ReverbSpace::chamber);
        reverb.setWetLevel(1.0f, true);
        std::vector<float> left(4096, 0.0f);
        std::vector<float> right(4096, 0.0f);
        left[0] = 1.0f;
        reverb.processStereo(left.data(), right.data(), 4096);

        auto leftEnergy = 0.0;
        auto rightEnergy = 0.0;
        auto correlation = 0.0;
        for (auto sample = 0; sample < 4096; ++sample) {
            leftEnergy += static_cast<double>(left[sample]) * left[sample];
            rightEnergy += static_cast<double>(right[sample]) * right[sample];
            correlation += static_cast<double>(left[sample]) * right[sample];
        }
        correlation /= std::sqrt(std::max(1.0e-20, leftEnergy * rightEnergy));
        expectGreaterThan(leftEnergy, 1.0e-8, "left input produces room output");
        expectGreaterThan(rightEnergy, 1.0e-10, "stereo room preserves a right-channel reflection path");
        expect(std::abs(correlation) < 0.99, "stereo room does not collapse to a mono wet signal");
    }

    void testAudioEngineIntegration() {
        beginTest("RoomReverbEngine: AudioEngine parameter and block rendering integration");

        AudioEngine engine;
        expect(engine.getReverbSpace() == AudioEngine::ReverbSpace::chamber);
        expectWithinAbsoluteError(engine.getReverbWet(), 0.0f, 1e-4f);

        engine.setReverbSpace(AudioEngine::ReverbSpace::concertHall);
        expect(engine.getReverbSpace() == AudioEngine::ReverbSpace::concertHall);

        engine.setReverbSpace(AudioEngine::ReverbSpace::studio);
        expect(engine.getReverbSpace() == AudioEngine::ReverbSpace::studio);

        engine.setReverbWet(0.42f);
        expectWithinAbsoluteError(engine.getReverbWet(), 0.42f, 1e-4f);

        // Audio block processing
        engine.prepareToPlay(512, 44100.0);
        juce::AudioBuffer<float> buffer(2, 512);
        buffer.clear();
        juce::AudioSourceChannelInfo info(&buffer, 0, 512);
        engine.getNextAudioBlock(info);
    }
    void testMonoKeepsStereoRoomWet() {
        beginTest("RoomReverbEngine: mono callback keeps the stereo Room wet effect");
        const auto renderMonoTake = [](float wet) {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            engine.setMasterGain(1.0f);
            engine.setReverbSpace(AudioEngine::ReverbSpace::concertHall);
            engine.setReverbWet(wet);
            engine.setLidPosition(AudioEngine::LidPosition::fullOpen);
            for (int i = 0; i < AudioEngine::calculateWarmupBlockCount(44100.0, 512); ++i) {
                juce::AudioBuffer<float> warm(1, 512);
                warm.clear();
                const juce::AudioSourceChannelInfo warmInfo(&warm, 0, 512);
                engine.getNextAudioBlock(warmInfo);
            }
            engine.getKeyboardState().noteOn(1, 60, 0.8f);
            juce::AudioBuffer<float> buffer(1, 512 * 16);
            buffer.clear();
            for (int start = 0; start < buffer.getNumSamples(); start += 512) {
                const juce::AudioSourceChannelInfo info(&buffer, start, 512);
                engine.getNextAudioBlock(info);
            }
            engine.getKeyboardState().noteOff(1, 60, 0.0f);
            return buffer;
        };
        const auto dry = renderMonoTake(0.0f);
        const auto wet = renderMonoTake(0.6f);
        expect(dry.getMagnitude(0, 0, dry.getNumSamples()) > 0.01f, "mono dry take must contain performed audio");
        expect(wet.getMagnitude(0, 0, wet.getNumSamples()) > 0.01f, "mono wet take must contain performed audio");
        double difference = 0.0;
        for (int s = 0; s < dry.getNumSamples(); ++s) {
            difference = std::max(
                difference,
                std::abs(static_cast<double>(wet.getSample(0, s)) - static_cast<double>(dry.getSample(0, s))));
        }
        expect(difference > 1e-4, "mono Room wet must change the performed output");
    }

    void testIdentifierRoundTrip() {
        beginTest("RoomReverbEngine: Identifier round-trip");

        using namespace devpiano::audio;

        expect(RoomReverbEngine::toIdentifier(ReverbSpace::studio) == "studio");
        expect(RoomReverbEngine::toIdentifier(ReverbSpace::chamber) == "chamber");
        expect(RoomReverbEngine::toIdentifier(ReverbSpace::concertHall) == "concert_hall");

        expect(RoomReverbEngine::fromIdentifier("studio") == ReverbSpace::studio);
        expect(RoomReverbEngine::fromIdentifier("chamber") == ReverbSpace::chamber);
        expect(RoomReverbEngine::fromIdentifier("concert_hall") == ReverbSpace::concertHall);
        expect(RoomReverbEngine::fromIdentifier("unknown") == ReverbSpace::chamber);
    }
};

static RoomReverbEngineTest roomReverbEngineTest;
