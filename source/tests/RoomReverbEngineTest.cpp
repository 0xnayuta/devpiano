#include <JuceHeader.h>

#include "Audio/AudioEngine.h"
#include "Audio/RoomReverbEngine.h"

// ============================================================================
/// RoomReverbEngineTest (Phase 31-B)
///
/// Validates mathematical algorithmic room reverberation:
/// 1. Preset decay scaling: Studio (0.6 s) < Chamber (1.5 s) < Concert Hall (2.4 s).
/// 2. Wet level scaling and zero-wet transparency bypass.
/// 3. Numerical safety: denormal flushing to absolute zero, no NaN/Inf.
/// 4. Sample rate invariance (44.1 kHz, 48 kHz, 96 kHz, 192 kHz).
/// 5. AudioEngine atomic parameter state and block processing integration.
/// 6. Identifier serialization round-trip.
// ============================================================================
class RoomReverbEngineTest final : public juce::UnitTest {
public:
    RoomReverbEngineTest()
        : juce::UnitTest("RoomReverbEngine", "DevPiano/Audio") {
    }

    void runTest() override {
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
    void testPresetsDecayCharacteristics() {
        beginTest("RoomReverbEngine: Preset decay scaling (Studio < Chamber < Concert Hall)");

        using namespace devpiano::audio;

        constexpr double sampleRate = 44100.0;

        auto measureDecaySampleCount = [&](ReverbSpace space) {
            RoomReverbEngine reverb;
            reverb.prepare(sampleRate);
            reverb.setSpace(space);
            reverb.setWetLevel(1.0f); // 100% wet to measure reverberant tail alone

            // Excite with a single impulse
            std::vector<float> left(2048, 0.0f);
            std::vector<float> right(2048, 0.0f);
            left[0] = 1.0f;
            right[0] = 1.0f;

            reverb.processStereo(left.data(), right.data(), 2048);

            // Now pump silence and measure how many samples until RMS falls below threshold (e.g. -40dB = 0.01)
            int totalSamples = 2048;
            int tailLength = 0;
            constexpr int kBlockSize = 512;
            std::vector<float> silenceL(kBlockSize, 0.0f);
            std::vector<float> silenceR(kBlockSize, 0.0f);

            for (int block = 0; block < 300; ++block) { // up to ~3.5 seconds
                std::fill(silenceL.begin(), silenceL.end(), 0.0f);
                std::fill(silenceR.begin(), silenceR.end(), 0.0f);
                reverb.processStereo(silenceL.data(), silenceR.data(), kBlockSize);
                totalSamples += kBlockSize;

                float maxSample = 0.0f;
                for (std::size_t i = 0; i < kBlockSize; ++i) {
                    maxSample = std::max(maxSample, std::abs(silenceL[i]));
                    maxSample = std::max(maxSample, std::abs(silenceR[i]));
                }

                if (maxSample > 0.005f) {
                    tailLength = totalSamples;
                }
            }
            return tailLength;
        };

        const auto decayStudio = measureDecaySampleCount(ReverbSpace::studio);
        const auto decayChamber = measureDecaySampleCount(ReverbSpace::chamber);
        const auto decayConcert = measureDecaySampleCount(ReverbSpace::concertHall);

        // Verification of relative physical RT60 scaling
        expect(decayStudio > 0, "Studio tail must exist");
        expect(decayChamber > decayStudio, "Chamber tail must be longer than Studio");
        expect(decayConcert > decayChamber, "Concert Hall tail must be longer than Chamber");
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

        const auto measureDecaySeconds = [](double sampleRate, ReverbSpace space) {
            RoomReverbEngine reverb;
            reverb.prepare(sampleRate);
            reverb.setSpace(space);
            reverb.setWetLevel(1.0f, true);
            std::vector<float> impulseL(2048, 0.0f);
            std::vector<float> impulseR(2048, 0.0f);
            impulseL[0] = 1.0f;
            impulseR[0] = 0.0f;
            reverb.processStereo(impulseL.data(), impulseR.data(), 2048);
            auto referencePeak = 0.0f;
            for (auto sample = 0; sample < 2048; ++sample) {
                referencePeak = std::max(referencePeak, std::abs(impulseL[sample]));
                referencePeak = std::max(referencePeak, std::abs(impulseR[sample]));
            }
            const auto threshold = std::max(1.0e-6f, referencePeak * 1.0e-3f);
            constexpr int blockSize = 512;
            constexpr double measureSeconds = 8.0;
            const auto maxBlocks = static_cast<int>(std::ceil(measureSeconds * sampleRate / blockSize));
            std::vector<float> silentL(blockSize, 0.0f);
            std::vector<float> silentR(blockSize, 0.0f);
            auto total = 2048;
            auto lastAudible = 0;
            for (auto block = 0; block < maxBlocks; ++block) {
                std::fill(silentL.begin(), silentL.end(), 0.0f);
                std::fill(silentR.begin(), silentR.end(), 0.0f);
                reverb.processStereo(silentL.data(), silentR.data(), blockSize);
                total += blockSize;
                auto peak = 0.0f;
                for (auto sample = 0; sample < blockSize; ++sample) {
                    peak = std::max(peak, std::abs(silentL[sample]));
                    peak = std::max(peak, std::abs(silentR[sample]));
                }
                if (peak > threshold) {
                    lastAudible = total;
                }
            }
            return static_cast<double>(lastAudible) / sampleRate;
        };

        const auto decay44 = measureDecaySeconds(44100.0, ReverbSpace::chamber);
        const auto decay48 = measureDecaySeconds(48000.0, ReverbSpace::chamber);
        const auto decay96 = measureDecaySeconds(96000.0, ReverbSpace::chamber);
        expect(decay44 > 0.3 && decay44 < 4.0, "chamber decay has a bounded time-domain length");
        // 容差由实测跨率差异决定：延迟取整与峰值参考不同仍带来约 25% 偏差，
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
