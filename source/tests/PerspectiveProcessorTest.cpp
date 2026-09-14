#include <JuceHeader.h>

#include "Audio/AudioEngine.h"
#include "Audio/PerspectiveProcessor.h"
#include "Audio/PianoSynthVoice.h"

// ============================================================================
/// PerspectiveProcessorTest (Phase 31-A)
///
/// Validates Player vs Audience dual-perspective stereo field processing:
/// 1. Player perspective transparency (100% width, bass left, treble right).
/// 2. Audience perspective soundstage flipping (treble left, bass right).
/// 3. Acoustic stereo width convergence (65% width) in audience mode.
/// 4. High-frequency air absorption damping over distance.
/// 5. Smooth crossfade interpolation during live switching (click-free / no phase cancellation).
/// 6. PianoSynthVoice & AudioEngine end-to-end integration and acoustic energy distribution.
/// 7. Identifier serialization round-trip.
// ============================================================================
class PerspectiveProcessorTest final : public juce::UnitTest {
public:
    PerspectiveProcessorTest()
        : juce::UnitTest("PerspectiveProcessor", "DevPiano/Audio") {
    }

    void runTest() override {
        testPlayerPerspectiveTransparency();
        testAudiencePerspectiveStereoFlippingAndConvergence();
        testAudienceAirAbsorptionFrequencyResponse();
        testSmoothCrossfadeTransitionWithoutClicks();
        testPianoSynthVoiceAndAudioEngineIntegration();
        testIdentifierRoundTrip();
    }

private:
    void testPlayerPerspectiveTransparency() {
        beginTest("PerspectiveProcessor: Player perspective transparency");

        using namespace devpiano::audio;

        PerspectiveProcessor processor;
        processor.prepare(44100.0);
        processor.setPerspective(SoundPerspective::player);

        // In player mode, processor must be completely transparent to stereo signal
        for (int i = 0; i < 200; ++i) {
            float left = 0.7f;
            float right = -0.3f;
            processor.processStereo(left, right);
            expectWithinAbsoluteError(left, 0.7f, 1e-4f);
            expectWithinAbsoluteError(right, -0.3f, 1e-4f);
        }
    }

    void testAudiencePerspectiveStereoFlippingAndConvergence() {
        beginTest("PerspectiveProcessor: Audience perspective flipping and convergence");

        using namespace devpiano::audio;

        PerspectiveProcessor processor;
        processor.prepare(44100.0);
        processor.setPerspective(SoundPerspective::audience, true); // Snap immediately

        // 1. Extreme left-panned input (L=1.0, R=0.0): run a train of samples so filter settles
        float lastL = 0.0f;
        float lastR = 0.0f;
        for (int i = 0; i < 200; ++i) {
            float l = 1.0f;
            float r = 0.0f;
            processor.processStereo(l, r);
            lastL = l;
            lastR = r;
        }

        // Right channel must dominate significantly in audience mode for left input
        expect(lastR > lastL, "In audience perspective, left input must flip so right channel dominates");
        expect(lastR > 0.60f, "Right channel should carry the majority of energy");
        expect(lastL < 0.40f, "Left channel should carry reduced energy");

        // 2. Pure side signal (L = 1.0, R = -1.0), Mid = 0.0
        // Expected output magnitude should be reduced to ~65% width
        float sideL = 0.0f;
        float sideR = 0.0f;
        for (int i = 0; i < 200; ++i) {
            float l = 1.0f;
            float r = -1.0f;
            processor.processStereo(l, r);
            sideL = l;
            sideR = r;
        }

        // Left should become negative (flipped), right positive
        expect(sideL < 0.0f, "Left channel must invert sign");
        expect(sideR > 0.0f, "Right channel must invert sign");
        expectWithinAbsoluteError(std::abs(sideL), 0.65f, 0.05f);
        expectWithinAbsoluteError(std::abs(sideR), 0.65f, 0.05f);
    }

    void testAudienceAirAbsorptionFrequencyResponse() {
        beginTest("PerspectiveProcessor: Air absorption high-frequency damping");

        using namespace devpiano::audio;

        const double sampleRate = 48000.0;
        PerspectiveProcessor processor;
        processor.prepare(sampleRate);
        processor.setPerspective(SoundPerspective::audience);

        // Settle smoother
        for (int i = 0; i < 4000; ++i) {
            float dummyL = 0.0f;
            float dummyR = 0.0f;
            processor.processStereo(dummyL, dummyR);
        }

        // Measure transmission gain at 100 Hz (low frequency) vs 12 kHz (high frequency)
        auto measureGain = [&](double freqHz) {
            double maxOut = 0.0;
            const auto omega = 2.0 * std::numbers::pi * freqHz / sampleRate;
            for (int n = 0; n < 2000; ++n) {
                auto in = static_cast<float>(std::sin(omega * n));
                float l = in;
                float r = in;
                processor.processStereo(l, r);
                if (n > 1000) {
                    maxOut = std::max(maxOut, static_cast<double>(std::abs(l)));
                }
            }
            return maxOut;
        };

        const auto lowFreqGain = measureGain(100.0);
        const auto highFreqGain = measureGain(12000.0);

        // Low frequencies (100 Hz) should pass through with near-unity gain (~1.0)
        expectWithinAbsoluteError(lowFreqGain, 1.0, 0.05);

        // High frequencies (12 kHz) should be attenuated by air absorption in audience mode
        expect(highFreqGain < 0.90, "12 kHz tone must experience air absorption damping in audience perspective");
        expect(highFreqGain < lowFreqGain, "High frequency gain must be lower than low frequency gain");
    }

    void testSmoothCrossfadeTransitionWithoutClicks() {
        beginTest("PerspectiveProcessor: Click-free smooth parameter crossfade");

        using namespace devpiano::audio;

        const double sampleRate = 44100.0;
        PerspectiveProcessor processor;
        processor.prepare(sampleRate);

        float prevL = 0.0f;
        float prevR = 0.0f;
        float maxDeltaL = 0.0f;
        float maxDeltaR = 0.0f;

        // Process a continuous 440 Hz tone while toggling perspective multiple times
        for (int block = 0; block < 10; ++block) {
            if (block % 2 == 0) {
                processor.setPerspective(SoundPerspective::audience);
            } else {
                processor.setPerspective(SoundPerspective::player);
            }

            for (int i = 0; i < 512; ++i) {
                const auto t = (block * 512 + i);
                const auto phase = 2.0 * std::numbers::pi * 440.0 * t / sampleRate;
                auto l = static_cast<float>(std::sin(phase));
                auto r = static_cast<float>(std::cos(phase));

                processor.processStereo(l, r);

                if (t > 0) {
                    maxDeltaL = std::max(maxDeltaL, std::abs(l - prevL));
                    maxDeltaR = std::max(maxDeltaR, std::abs(r - prevR));
                }

                prevL = l;
                prevR = r;

                // Numerical sanity: no NaN or Inf
                expect(!std::isnan(l) && !std::isinf(l));
                expect(!std::isnan(r) && !std::isinf(r));
            }
        }

        // For a 440 Hz tone with max amplitude 1.0 at 44.1 kHz, max derivative is omega/sr ~ 0.063
        // Transitions must not introduce discontinuous step jumps
        expect(maxDeltaL < 0.20f, "Max per-sample delta on left channel must be bounded (no clicks)");
        expect(maxDeltaR < 0.20f, "Max per-sample delta on right channel must be bounded (no clicks)");
    }

    void testPianoSynthVoiceAndAudioEngineIntegration() {
        beginTest("PerspectiveProcessor: Voice and AudioEngine acoustic integration");

        using namespace devpiano::audio;

        // 1. AudioEngine atomic state and getter/setter verification
        AudioEngine audioEngine;
        expect(audioEngine.getSoundPerspective() == AudioEngine::SoundPerspective::player);

        audioEngine.setSoundPerspective(AudioEngine::SoundPerspective::audience);
        expect(audioEngine.getSoundPerspective() == AudioEngine::SoundPerspective::audience);

        audioEngine.setSoundPerspective(AudioEngine::SoundPerspective::player);
        expect(audioEngine.getSoundPerspective() == AudioEngine::SoundPerspective::player);

        // 2. Direct PianoSynthVoice acoustic energy rendering
        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 1024;

        auto renderVoiceEnergy = [&](SoundPerspective perspective, int midiNote, float& outL, float& outR) {
            juce::Synthesiser synth;
            synth.setCurrentPlaybackSampleRate(sampleRate);
            synth.addSound(new PianoSynthSound());
            auto* voice = new PianoSynthVoice();
            voice->setAdsrParameters({ 0.001f, 0.2f, 0.8f, 0.3f });
            voice->setSoundPerspective(perspective);
            synth.addVoice(voice);
            expect(voice->getSoundPerspective() == perspective);

            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, midiNote, 0.8f), 0);

            juce::AudioBuffer<float> buffer(2, blockSize);
            buffer.clear();
            synth.renderNextBlock(buffer, midi, 0, blockSize);

            outL = buffer.getRMSLevel(0, 0, blockSize);
            outR = buffer.getRMSLevel(1, 0, blockSize);
        };

        // Bass note A0 (MIDI 21):
        // Player mode: low strings on left side of the piano -> Left > Right
        float playerBassL = 0.0f;
        float playerBassR = 0.0f;
        renderVoiceEnergy(SoundPerspective::player, 21, playerBassL, playerBassR);
        expect(playerBassL > playerBassR, "In player perspective, bass notes must have higher energy on the left");

        // Audience mode: perspective flipped -> Right > Left
        float audBassL = 0.0f;
        float audBassR = 0.0f;
        renderVoiceEnergy(SoundPerspective::audience, 21, audBassL, audBassR);
        expect(audBassR > audBassL, "In audience perspective, bass notes must have higher energy on the right");

        // Treble note C8 (MIDI 108):
        // Player mode: high strings on right side -> Right > Left
        float playerTrebleL = 0.0f;
        float playerTrebleR = 0.0f;
        renderVoiceEnergy(SoundPerspective::player, 108, playerTrebleL, playerTrebleR);
        expect(playerTrebleR > playerTrebleL,
               "In player perspective, treble notes must have higher energy on the right");

        // Audience mode: perspective flipped -> Left > Right
        float audTrebleL = 0.0f;
        float audTrebleR = 0.0f;
        renderVoiceEnergy(SoundPerspective::audience, 108, audTrebleL, audTrebleR);
        expect(audTrebleL > audTrebleR, "In audience perspective, treble notes must have higher energy on the left");
    }

    void testIdentifierRoundTrip() {
        beginTest("PerspectiveProcessor: Identifier round-trip");

        using namespace devpiano::audio;

        expect(PerspectiveProcessor::toIdentifier(SoundPerspective::player) == "player");
        expect(PerspectiveProcessor::toIdentifier(SoundPerspective::audience) == "audience");

        expect(PerspectiveProcessor::fromIdentifier("player") == SoundPerspective::player);
        expect(PerspectiveProcessor::fromIdentifier("audience") == SoundPerspective::audience);
        expect(PerspectiveProcessor::fromIdentifier("unknown") == SoundPerspective::player);
    }
};

static PerspectiveProcessorTest perspectiveProcessorTest;
