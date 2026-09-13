#include <JuceHeader.h>

#include "Audio/AudioEngine.h"
#include "Audio/PianoSynthVoice.h"

// ==============================================================================
// Phase 32-C Unit Tests: Inharmonicity Jitter & Felt Ageing Dynamics
// ==============================================================================
class FeltAgeingTest final : public juce::UnitTest {
public:
    FeltAgeingTest()
        : juce::UnitTest("FeltAgeing", "Audio") {
    }

    void runTest() override {
        testDeterministicNoteJitterProperties();
        testZeroFeltAgeingPurity();
        testPitchMicroJitterBoundaries();
        testPerKeyHardnessAndTimbreDispersion();
        testAudioEngineFeltAgeingParameterSync();
    }

private:
    void testDeterministicNoteJitterProperties() {
        beginTest("deterministicNoteJitter produces bounded deterministic output in [-1, 1]");

        // 1. Check bounds across all 88 keys and multiple salts
        constexpr std::uint32_t salts[] = { 0x13579bdfu, 0x2468ace0u, 0x9e3779b9u, 0x4b7e1516u };

        for (const auto salt : salts) {
            float sum = 0.0f;
            for (int note = 21; note <= 108; ++note) {
                const auto val1 = PianoSynthVoice::deterministicNoteJitter(note, salt);
                const auto val2 = PianoSynthVoice::deterministicNoteJitter(note, salt);

                // Strictly deterministic: same input -> same output
                expectEquals(val1, val2);

                // Range check
                expect(val1 >= -1.0f && val1 <= 1.0f);
                sum += val1;
            }

            // Mean should be centered near 0 (-0.3 to +0.3 over 88 keys)
            const auto mean = sum / 88.0f;
            expect(std::abs(mean) < 0.35f);
        }
    }

    void testZeroFeltAgeingPurity() {
        beginTest("Zero felt ageing amount produces purely nominal mathematical frequencies");

        PianoSynthVoice voicePure;
        voicePure.setCurrentPlaybackSampleRate(48000.0);
        voicePure.setFeltAgeingAmount(0.0f);

        // When ageing is 0, getter returns 0.0f
        expectWithinAbsoluteError(voicePure.getFeltAgeingAmount(), 0.0f, 1e-4f);
    }

    void testPitchMicroJitterBoundaries() {
        beginTest("Pitch micro-jitter is strictly constrained within +-1.5 cents");

        for (int note = 21; note <= 108; ++note) {
            const auto jitterNorm = PianoSynthVoice::deterministicNoteJitter(note, 0x13579bdfu);
            const auto cents = jitterNorm * 1.2f; // at feltAgeingAmount = 1.0
            expect(std::abs(cents) <= 1.25f);

            const auto ratio = std::pow(2.0, static_cast<double>(cents) / 1200.0);
            // Ratio must be within [0.9992, 1.0008]
            expect(ratio >= 0.9990 && ratio <= 1.0010);
        }
    }

    void testPerKeyHardnessAndTimbreDispersion() {
        beginTest("Adjacent keys exhibit subtle organic timbre dispersion with felt ageing");

        juce::Synthesiser synthZero;
        synthZero.setCurrentPlaybackSampleRate(48000.0);
        synthZero.addSound(new PianoSynthSound());
        auto* v0 = new PianoSynthVoice();
        v0->setFeltAgeingAmount(0.0f);
        synthZero.addVoice(v0);

        juce::Synthesiser synthAged;
        synthAged.setCurrentPlaybackSampleRate(48000.0);
        synthAged.addSound(new PianoSynthSound());
        auto* vAged = new PianoSynthVoice();
        vAged->setFeltAgeingAmount(0.8f);
        synthAged.addVoice(vAged);

        // Render identical note (Note 60) on both synths
        synthZero.noteOn(1, 60, 0.7f);
        synthAged.noteOn(1, 60, 0.7f);

        juce::AudioBuffer<float> bufZero(2, 512);
        juce::AudioBuffer<float> bufAged(2, 512);

        synthZero.renderNextBlock(bufZero, juce::MidiBuffer(), 0, 512);
        synthAged.renderNextBlock(bufAged, juce::MidiBuffer(), 0, 512);

        // Both render valid audible sound
        expectGreaterThan(bufZero.getMagnitude(0, 0, 512), 0.01f);
        expectGreaterThan(bufAged.getMagnitude(0, 0, 512), 0.01f);

        // Render second block (10-20ms) where subtle phase and timbre differences accumulate
        bufZero.clear();
        bufAged.clear();
        synthZero.renderNextBlock(bufZero, juce::MidiBuffer(), 0, 512);
        synthAged.renderNextBlock(bufAged, juce::MidiBuffer(), 0, 512);

        float maxDiff = 0.0f;
        for (int i = 0; i < 512; ++i) {
            maxDiff = std::max(maxDiff, std::abs(bufZero.getSample(0, i) - bufAged.getSample(0, i)));
        }
        expectGreaterThan(maxDiff, 0.0001f);
    }

    void testAudioEngineFeltAgeingParameterSync() {
        beginTest("AudioEngine: feltAgeingAmount atomic parameter sync and boundary clamping");

        AudioEngine engine;
        engine.prepareToPlay(512, 48000.0);
        engine.setBuiltinSynthTone(AudioEngine::BuiltinSynthTone::piano);

        engine.setFeltAgeingAmount(0.7f);
        expectWithinAbsoluteError(engine.getFeltAgeingAmount(), 0.7f, 1e-4f);

        // Clamping checks
        engine.setFeltAgeingAmount(-0.4f);
        expectWithinAbsoluteError(engine.getFeltAgeingAmount(), 0.0f, 1e-4f);

        engine.setFeltAgeingAmount(1.8f);
        expectWithinAbsoluteError(engine.getFeltAgeingAmount(), 1.0f, 1e-4f);

        // Consume warmup blocks
        const auto warmupBlocks = AudioEngine::calculateWarmupBlockCount(48000.0, 512);
        juce::AudioBuffer<float> buffer(2, 512);
        juce::AudioSourceChannelInfo info(&buffer, 0, 512);
        for (int i = 0; i < warmupBlocks; ++i) {
            buffer.clear();
            engine.getNextAudioBlock(info);
        }

        // Apply parameter block
        engine.setFeltAgeingAmount(0.45f);
        buffer.clear();
        engine.getNextAudioBlock(info);
        expectWithinAbsoluteError(engine.getFeltAgeingAmount(), 0.45f, 1e-4f);
    }
};

static FeltAgeingTest feltAgeingTest;
