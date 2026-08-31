#include <JuceHeader.h>

#include "Audio/SineSynthVoice.h"

// =============================================================================
// Unit tests for SineSynthVoice: deterministic rendering, ADSR lifecycle,
// sample rate safety guards, note stop modes, and multi-channel rendering.
// =============================================================================

class SineSynthVoiceTest final : public juce::UnitTest {
public:
    SineSynthVoiceTest()
        : juce::UnitTest("SineSynthVoice", "DevPiano/Audio") {
    }

    void runTest() override {
        testSoundAndVoiceAssociation();
        testSampleRateSafetyGuard();
        testDeterministicRenderingAndFrequency();
        testAdsrTailOffAndAutoClear();
        testInstantStopNote();
        testMultiChannelConsistency();
    }

private:
    void testSoundAndVoiceAssociation() {
        beginTest("Sound and voice association");

        SineSynthSound sound;
        expect(sound.appliesToNote(60));
        expect(sound.appliesToChannel(1));
        expect(sound.appliesToChannel(16));

        SineSynthVoice voice;
        expect(voice.canPlaySound(&sound));

        // Unknown sound type must be rejected
        struct DummySound final : public juce::SynthesiserSound {
            bool appliesToNote(int) override {
                return false;
            }
            bool appliesToChannel(int) override {
                return false;
            }
        } dummySound;
        expect(!voice.canPlaySound(&dummySound));
    }

    void testSampleRateSafetyGuard() {
        beginTest("Zero or negative sample rate guard");

        SineSynthSound sound;
        SineSynthVoice voice;

        voice.setCurrentPlaybackSampleRate(0.0);
        voice.startNote(60, 0.8f, &sound, 0);
        expect(!voice.isVoiceActive(), "Voice should remain inactive when sample rate is 0");

        voice.setCurrentPlaybackSampleRate(-44100.0);
        voice.startNote(60, 0.8f, &sound, 0);
        expect(!voice.isVoiceActive(), "Voice should remain inactive when sample rate is negative");
    }

    void testDeterministicRenderingAndFrequency() {
        beginTest("Deterministic rendering and non-zero output");

        SineSynthSound sound;
        SineSynthVoice voice;
        constexpr double sampleRate = 44100.0;
        voice.setCurrentPlaybackSampleRate(sampleRate);

        // Standard ADSR: fast attack, full sustain
        juce::ADSR::Parameters adsrParams;
        adsrParams.attack = 0.001f;
        adsrParams.decay = 0.01f;
        adsrParams.sustain = 1.0f;
        adsrParams.release = 0.05f;
        voice.setAdsrParameters(adsrParams);

        // Start note A4 (MIDI 69 = 440Hz)
        voice.startNote(69, 1.0f, &sound, 0);
        expect(voice.isVoiceActive());

        juce::AudioBuffer<float> buffer(2, 512);
        buffer.clear();
        voice.renderNextBlock(buffer, 0, 512);

        const auto mag = buffer.getMagnitude(0, 0, 512);
        expect(mag > 0.1f, "Rendered block must contain audible signal");
        expect(mag <= 0.71f, "Magnitude should not exceed nominal velocity level (1.0 * 0.70)");
    }

    void testAdsrTailOffAndAutoClear() {
        beginTest("ADSR tail off and voice self-clearing");

        SineSynthSound sound;
        SineSynthVoice voice;
        constexpr double sampleRate = 44100.0;
        voice.setCurrentPlaybackSampleRate(sampleRate);

        juce::ADSR::Parameters adsrParams;
        adsrParams.attack = 0.001f;
        adsrParams.decay = 0.005f;
        adsrParams.sustain = 0.8f;
        adsrParams.release = 0.01f; // 10ms release = 441 samples
        voice.setAdsrParameters(adsrParams);

        voice.startNote(60, 0.8f, &sound, 0);
        expect(voice.isVoiceActive());

        juce::AudioBuffer<float> buffer(1, 256);
        buffer.clear();
        voice.renderNextBlock(buffer, 0, 256);
        expect(voice.isVoiceActive());

        // Stop note with tail-off
        voice.stopNote(0.0f, true);
        expect(voice.isVoiceActive(), "Voice should stay active while releasing");

        // Render until tail-off finishes (release = 441 samples, render 1024 samples)
        juce::AudioBuffer<float> releaseBuffer(1, 1024);
        releaseBuffer.clear();
        voice.renderNextBlock(releaseBuffer, 0, 1024);

        expect(!voice.isVoiceActive(), "Voice must become inactive once release envelope reaches 0");
    }

    void testInstantStopNote() {
        beginTest("Instant stop note (allowTailOff=false)");

        SineSynthSound sound;
        SineSynthVoice voice;
        constexpr double sampleRate = 44100.0;
        voice.setCurrentPlaybackSampleRate(sampleRate);

        voice.startNote(60, 0.8f, &sound, 0);
        expect(voice.isVoiceActive());

        voice.stopNote(0.0f, false);
        expect(!voice.isVoiceActive(), "Voice must be cleared immediately when allowTailOff is false");

        juce::AudioBuffer<float> buffer(1, 256);
        buffer.clear();
        voice.renderNextBlock(buffer, 0, 256);
        expect(buffer.getMagnitude(0, 0, 256) == 0.0f, "Render block after instant stop must be completely silent");
    }

    void testMultiChannelConsistency() {
        beginTest("Multi-channel consistency");

        SineSynthSound sound;
        SineSynthVoice voice;
        constexpr double sampleRate = 44100.0;
        voice.setCurrentPlaybackSampleRate(sampleRate);

        voice.startNote(64, 0.9f, &sound, 0);

        juce::AudioBuffer<float> buffer(2, 256);
        buffer.clear();
        voice.renderNextBlock(buffer, 0, 256);

        const auto* left = buffer.getReadPointer(0);
        const auto* right = buffer.getReadPointer(1);
        for (int i = 0; i < 256; ++i) {
            expectEquals(left[i], right[i], "Left and right channels must match identically");
        }
    }
};

static SineSynthVoiceTest sineSynthVoiceTest;
