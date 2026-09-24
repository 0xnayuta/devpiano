#include <JuceHeader.h>

#include "Audio/AudioEngine.h"
#include "Audio/RoomReverbEngine.h"

// ============================================================================
/// RoomReverbEngineTest (Phase 31-B)
///
/// Validates mathematical algorithmic room reverberation:
/// 1. Preset decay scaling: Studio (0.6s) < Chamber (1.5s) < Concert Hall (2.4s).
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
        testAudioEngineIntegration();
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

    void testIdentifierRoundTrip() {
        beginTest("RoomReverbEngine: Identifier round-trip");

        using namespace devpiano::audio;

        expect(RoomReverbEngine::toIdentifier(ReverbSpace::studio) == "studio");
        expect(RoomReverbEngine::toIdentifier(ReverbSpace::chamber) == "chamber");
        expect(RoomReverbEngine::toIdentifier(ReverbSpace::concertHall) == "concert_hall");

        expect(RoomReverbEngine::fromIdentifier("studio") == ReverbSpace::studio);
        expect(RoomReverbEngine::fromIdentifier("chamber") == ReverbSpace::chamber);
        expect(RoomReverbEngine::fromIdentifier("concert_hall") == ReverbSpace::concertHall);
        expect(RoomReverbEngine::fromIdentifier("hall") == ReverbSpace::concertHall);
        expect(RoomReverbEngine::fromIdentifier("unknown") == ReverbSpace::chamber);
    }
};

static RoomReverbEngineTest roomReverbEngineTest;
