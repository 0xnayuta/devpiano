#include <JuceHeader.h>

#include "Audio/AudioEngine.h"
#include "Audio/PianoSynthVoice.h"
#include "Audio/SineSynthVoice.h"
#include "Audio/TemperamentEngine.h"

// ============================================================================
/// PianoSynthVoiceTemperamentTest (Phase 30-B)
///
/// Validates temperament and reference pitch decoupling in PianoSynthVoice,
/// SineSynthVoice fallback, and AudioEngine voice parameter distribution.
// ============================================================================
class PianoSynthVoiceTemperamentTest final : public juce::UnitTest {
public:
    PianoSynthVoiceTemperamentTest()
        : juce::UnitTest("PianoSynthVoiceTemperament", "DevPiano/Audio") {
    }

    void runTest() override {
        testVoicePartialFrequencyWithTemperament();
        testVoiceDynamicTuningSwitching();
        testAudioEngineTemperamentAtomicState();
        testSineSynthVoiceTemperament();
    }

private:
    void testVoicePartialFrequencyWithTemperament() {
        beginTest("PianoSynthVoice: Partial frequency computation with temperaments and reference pitch");

        using namespace devpiano::audio;

        // 1. A4 (note 69, partial 0): physical partial frequency = f0 * sqrt(1 + B)
        const auto bA4 = PianoSynthVoice::inharmonicityBForNote(69);
        const auto inharmonicFactorA4 = std::sqrt(1.0 + bA4);
        for (const auto refPitch : { 415.0, 432.0, 440.0, 442.0 }) {
            const auto expectedA4 = refPitch * inharmonicFactorA4;
            const auto fA4Equal = PianoSynthVoice::partialFrequency(69, 0, Temperament::equal, refPitch);
            expectWithinAbsoluteError(fA4Equal, expectedA4, 1e-4);

            const auto fA4Just = PianoSynthVoice::partialFrequency(69, 0, Temperament::just, refPitch);
            expectWithinAbsoluteError(fA4Just, expectedA4, 1e-4);

            const auto fA4Meantone = PianoSynthVoice::partialFrequency(69, 0, Temperament::meantone, refPitch);
            expectWithinAbsoluteError(fA4Meantone, expectedA4, 1e-4);
        }

        // 2. C4 (note 60, partial 0): in Just Intonation with A=5/3, f0 = 440 * (3/5) = 264 Hz,
        // partial 0 frequency = 264 * sqrt(1 + B(60))
        const auto bC4 = PianoSynthVoice::inharmonicityBForNote(60);
        const auto expectedC4 = 264.0 * std::sqrt(1.0 + bC4);
        const auto fC4Just = PianoSynthVoice::partialFrequency(60, 0, Temperament::just, 440.0);
        expectWithinAbsoluteError(fC4Just, expectedC4, 1e-4);

        // 3. Higher partials account for inharmonicity (f_n > n * f0)
        for (int p = 1; p < 10; ++p) {
            const auto fEqual = PianoSynthVoice::partialFrequency(60, p, Temperament::equal, 440.0);
            const auto fNominal = PianoSynthVoice::partialFrequency(60, 0, Temperament::equal, 440.0) * (p + 1);
            expectGreaterThan(fEqual, fNominal);
        }
    }

    void testVoiceDynamicTuningSwitching() {
        beginTest("PianoSynthVoice: Dynamic temperament switching during playback renders cleanly");

        using namespace devpiano::audio;

        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 512;

        juce::Synthesiser synth;
        synth.setCurrentPlaybackSampleRate(sampleRate);
        synth.addSound(new PianoSynthSound());

        auto* voice = new PianoSynthVoice();
        voice->setTemperament(Temperament::equal);
        voice->setReferencePitchA4(440.0);
        synth.addVoice(voice);

        // Trigger middle C
        synth.noteOn(1, 60, 0.8f);

        juce::AudioBuffer<float> buffer(2, blockSize);

        // Render standard block
        buffer.clear();
        synth.renderNextBlock(buffer, juce::MidiBuffer(), 0, blockSize);
        expectGreaterThan(buffer.getMagnitude(0, blockSize), 0.0f);

        // Dynamically switch temperament and reference pitch while note is ringing
        voice->setTemperament(Temperament::meantone);
        voice->setReferencePitchA4(415.0);

        expect(voice->getTemperament() == Temperament::meantone);
        expectEquals(voice->getReferencePitchA4(), 415.0);

        // Render subsequent blocks to ensure numerical stability (no NaN, no Inf)
        for (int b = 0; b < 10; ++b) {
            buffer.clear();
            synth.renderNextBlock(buffer, juce::MidiBuffer(), 0, blockSize);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
                const auto* readPtr = buffer.getReadPointer(ch);
                for (int s = 0; s < blockSize; ++s) {
                    expect(!std::isnan(readPtr[s]));
                    expect(!std::isinf(readPtr[s]));
                    expectLessThan(std::abs(readPtr[s]), 10.0f);
                }
            }
        }

        synth.allNotesOff(1, false);
    }

    void testAudioEngineTemperamentAtomicState() {
        beginTest("AudioEngine: Temperament and reference pitch atomic state and voice distribution");

        using namespace devpiano::audio;

        AudioEngine engine;

        // Default state
        expect(engine.getTemperament() == AudioEngine::Temperament::equal);
        expectEquals(engine.getReferencePitchA4(), 440.0);

        // Mutate temperament
        engine.setTemperament(AudioEngine::Temperament::werckmeister3);
        expect(engine.getTemperament() == AudioEngine::Temperament::werckmeister3);

        // Mutate reference pitch
        engine.setReferencePitchA4(432.0);
        expectEquals(engine.getReferencePitchA4(), 432.0);

        // Clamping protection
        engine.setReferencePitchA4(350.0);
        expectEquals(engine.getReferencePitchA4(), 410.0);

        engine.setReferencePitchA4(500.0);
        expectEquals(engine.getReferencePitchA4(), 450.0);

        // Audio render block consumes pending parameters without race conditions
        engine.prepareToPlay(512, 44100.0);
        juce::AudioBuffer<float> audioBuffer(2, 512);
        audioBuffer.clear();
        juce::AudioSourceChannelInfo channelInfo(&audioBuffer, 0, 512);
        engine.getNextAudioBlock(channelInfo);

        expect(engine.getTemperament() == AudioEngine::Temperament::werckmeister3);
        expectEquals(engine.getReferencePitchA4(), 450.0);
    }

    void testSineSynthVoiceTemperament() {
        beginTest("SineSynthVoice: Fallback sine voice adapts to temperament and reference pitch");

        using namespace devpiano::audio;

        SineSynthVoice sineVoice;
        sineVoice.setCurrentPlaybackSampleRate(44100.0);

        expect(sineVoice.getTemperament() == Temperament::equal);
        expectEquals(sineVoice.getReferencePitchA4(), 440.0);

        sineVoice.setTemperament(Temperament::pythagorean);
        sineVoice.setReferencePitchA4(415.0);

        expect(sineVoice.getTemperament() == Temperament::pythagorean);
        expectEquals(sineVoice.getReferencePitchA4(), 415.0);
    }
};

static PianoSynthVoiceTemperamentTest pianoSynthVoiceTemperamentTest;
