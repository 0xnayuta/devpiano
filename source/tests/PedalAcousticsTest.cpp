#include <JuceHeader.h>

#include "Audio/AudioEngine.h"
#include "Audio/PianoSynthVoice.h"

// ==============================================================================
// Phase 32-A Unit Tests: Pedal Whoosh & Resonance Shock
// ==============================================================================
class PedalAcousticsTest final : public juce::UnitTest {
public:
    PedalAcousticsTest()
        : juce::UnitTest("PedalAcoustics", "Audio") {
    }

    void runTest() override {
        testPedalPressGeneratesWhooshAndShockWhenSilent();
        testPedalReleaseGeneratesReturnThump();
        testPedalVelocityModulation();
        testPedalNoiseLevelBypass();
        testMultiVoiceExclusivity();
        testPedalNoiseWithActiveNotes();
        testAudioEnginePedalIntegration();
        testPedalNoiseInMonoBufferActiveNote();
        testIdlePedalSympatheticResonanceShock();
    }

private:
    static float calculateRms(const juce::AudioBuffer<float>& buffer, int channel = 0) {
        if (buffer.getNumSamples() == 0) {
            return 0.0f;
        }
        return buffer.getRMSLevel(channel, 0, buffer.getNumSamples());
    }

    static float calculatePeak(const juce::AudioBuffer<float>& buffer, int channel = 0) {
        if (buffer.getNumSamples() == 0) {
            return 0.0f;
        }
        return buffer.getMagnitude(channel, 0, buffer.getNumSamples());
    }

    void testPedalPressGeneratesWhooshAndShockWhenSilent() {
        beginTest("Pedal press generates air whoosh and resonance shock when silent");

        PianoSynthVoice voice;
        voice.setCurrentPlaybackSampleRate(48000.0);
        voice.setVoiceIndex(0);
        voice.setPedalNoiseLevel(0.8f);

        // Before pedal: completely silent
        juce::AudioBuffer<float> buffer(2, 512);
        buffer.clear();
        voice.renderNextBlock(buffer, 0, 512);
        expectEquals(calculatePeak(buffer, 0), 0.0f);
        expectEquals(calculatePeak(buffer, 1), 0.0f);

        // Press pedal fully (CC 64 = 127)
        voice.controllerMoved(64, 127);
        expect(voice.isPedalTransientActive());

        // Render the whoosh and shock block
        buffer.clear();
        voice.renderNextBlock(buffer, 0, 512);
        const auto peakL = calculatePeak(buffer, 0);
        const auto peakR = calculatePeak(buffer, 1);
        expectGreaterThan(peakL, 0.001f);
        expectGreaterThan(peakR, 0.001f);
        // Peak should be reasonable, non-clipping (< 0.5)
        expectLessThan(peakL, 0.5f);
        expectLessThan(peakR, 0.5f);

        // Render until tail decay completes (~400 ms)
        int samplesRemaining = static_cast<int>(48000 * 0.45);
        while (samplesRemaining > 0 && (voice.isPedalTransientActive() || voice.isSympatheticShockActive())) {
            buffer.clear();
            const auto block = std::min(512, samplesRemaining);
            voice.renderNextBlock(buffer, 0, block);
            samplesRemaining -= block;
        }

        // Must decay to completely inactive
        expect(!voice.isPedalTransientActive());
        expect(!voice.isSympatheticShockActive());
        buffer.clear();
        voice.renderNextBlock(buffer, 0, 512);
        expectEquals(calculatePeak(buffer, 0), 0.0f);
        expectEquals(calculatePeak(buffer, 1), 0.0f);
    }

    void testPedalReleaseGeneratesReturnThump() {
        beginTest("Pedal release generates softer return thump transient");

        PianoSynthVoice voice;
        voice.setCurrentPlaybackSampleRate(48000.0);
        voice.setVoiceIndex(0);
        voice.setPedalNoiseLevel(0.8f);

        // 1. Press pedal
        voice.controllerMoved(64, 127);
        juce::AudioBuffer<float> buffer(2, 512);
        buffer.clear();
        voice.renderNextBlock(buffer, 0, 512);
        const auto pressPeak = calculatePeak(buffer, 0);

        // Consume press decay
        for (int i = 0; i < 20; ++i) {
            buffer.clear();
            voice.renderNextBlock(buffer, 0, 512);
        }
        expect(!voice.isPedalTransientActive());

        // 2. Release pedal (CC 64 = 0)
        voice.controllerMoved(64, 0);
        expect(voice.isPedalTransientActive());

        buffer.clear();
        voice.renderNextBlock(buffer, 0, 512);
        const auto releasePeak = calculatePeak(buffer, 0);
        expectGreaterThan(releasePeak, 0.0005f);
        // Release thump is softer than press whoosh+shock
        expectLessThan(releasePeak, pressPeak);

        // Consume release decay
        for (int i = 0; i < 15; ++i) {
            buffer.clear();
            voice.renderNextBlock(buffer, 0, 512);
        }
        expect(!voice.isPedalTransientActive());
    }

    void testPedalVelocityModulation() {
        beginTest("Fast pedal press produces higher energy than slow pedal press");

        PianoSynthVoice voiceFast;
        voiceFast.setCurrentPlaybackSampleRate(48000.0);
        voiceFast.setVoiceIndex(0);
        voiceFast.setPedalNoiseLevel(0.8f);

        // Fast press: 0 -> 127 (jump = 127)
        voiceFast.controllerMoved(64, 127);
        juce::AudioBuffer<float> bufFast(2, 1024);
        bufFast.clear();
        voiceFast.renderNextBlock(bufFast, 0, 1024);
        const auto rmsFast = calculateRms(bufFast, 0);

        PianoSynthVoice voiceSlow;
        voiceSlow.setCurrentPlaybackSampleRate(48000.0);
        voiceSlow.setVoiceIndex(0);
        voiceSlow.setPedalNoiseLevel(0.8f);

        // Set previous value to 40, jump to 65 (small jump = 25)
        voiceSlow.controllerMoved(64, 40);
        voiceSlow.controllerMoved(64, 65);
        juce::AudioBuffer<float> bufSlow(2, 1024);
        bufSlow.clear();
        voiceSlow.renderNextBlock(bufSlow, 0, 1024);
        const auto rmsSlow = calculateRms(bufSlow, 0);

        expectGreaterThan(rmsFast, rmsSlow);
    }

    void testPedalNoiseLevelBypass() {
        beginTest("Setting pedalNoiseLevel to 0.0 completely silences pedal transients");

        PianoSynthVoice voice;
        voice.setCurrentPlaybackSampleRate(48000.0);
        voice.setVoiceIndex(0);
        voice.setPedalNoiseLevel(0.0f);

        voice.controllerMoved(64, 127);
        // Level is 0, so transient should not be active or output must be 0
        juce::AudioBuffer<float> buffer(2, 512);
        buffer.clear();
        voice.renderNextBlock(buffer, 0, 512);
        expectEquals(calculatePeak(buffer, 0), 0.0f);
        expectEquals(calculatePeak(buffer, 1), 0.0f);
    }

    void testMultiVoiceExclusivity() {
        beginTest("Only voice index 0 generates pedal noise when idle");

        PianoSynthVoice voice0;
        voice0.setCurrentPlaybackSampleRate(48000.0);
        voice0.setVoiceIndex(0);
        voice0.setPedalNoiseLevel(0.8f);

        PianoSynthVoice voice1;
        voice1.setCurrentPlaybackSampleRate(48000.0);
        voice1.setVoiceIndex(1);
        voice1.setPedalNoiseLevel(0.8f);

        voice0.controllerMoved(64, 127);
        voice1.controllerMoved(64, 127);

        juce::AudioBuffer<float> buf0(2, 512);
        juce::AudioBuffer<float> buf1(2, 512);
        buf0.clear();
        buf1.clear();

        voice0.renderNextBlock(buf0, 0, 512);
        voice1.renderNextBlock(buf1, 0, 512);

        expectGreaterThan(calculatePeak(buf0, 0), 0.001f);
        expectEquals(calculatePeak(buf1, 0), 0.0f);
        expectEquals(calculatePeak(buf1, 1), 0.0f);
    }

    void testPedalNoiseWithActiveNotes() {
        beginTest("Pedal transients mix cleanly without interrupting active note rendering");

        juce::Synthesiser synth;
        synth.setCurrentPlaybackSampleRate(48000.0);
        synth.addSound(new PianoSynthSound());
        auto* voice = new PianoSynthVoice();
        voice->setVoiceIndex(0);
        voice->setPedalNoiseLevel(0.6f);
        voice->setAdsrParameters({ 0.001f, 0.2f, 0.8f, 0.3f });
        synth.addVoice(voice);

        synth.noteOn(1, 60, 0.7f);

        juce::AudioBuffer<float> bufNoteOnly(2, 512);
        bufNoteOnly.clear();
        synth.renderNextBlock(bufNoteOnly, juce::MidiBuffer(), 0, 512);
        const auto peakNoteOnly = calculatePeak(bufNoteOnly, 0);
        expectGreaterThan(peakNoteOnly, 0.01f);

        // Press pedal while note is singing
        synth.handleController(1, 64, 127);
        juce::AudioBuffer<float> bufWithPedal(2, 512);
        bufWithPedal.clear();
        synth.renderNextBlock(bufWithPedal, juce::MidiBuffer(), 0, 512);
        const auto peakWithPedal = calculatePeak(bufWithPedal, 0);

        expectGreaterThan(peakWithPedal, 0.01f);
        expect(voice->isVoiceActive());
    }

    void testAudioEnginePedalIntegration() {
        beginTest("AudioEngine: pedalNoiseLevel atomic parameter distribution and voice lifecycle");

        AudioEngine engine;
        engine.prepareToPlay(512, 48000.0);
        engine.setMasterGain(1.0f);
        engine.setBuiltinSynthTone(AudioEngine::BuiltinSynthTone::piano);
        engine.setPedalNoiseLevel(0.75f);

        // Atomic getter verification
        expectWithinAbsoluteError(engine.getPedalNoiseLevel(), 0.75f, 1e-4f);

        // Consume warmup blocks
        const auto warmupBlocks = AudioEngine::calculateWarmupBlockCount(48000.0, 512);
        juce::AudioBuffer<float> buffer(2, 512);
        juce::AudioSourceChannelInfo info(&buffer, 0, 512);
        for (int i = 0; i < warmupBlocks; ++i) {
            buffer.clear();
            engine.getNextAudioBlock(info);
        }

        // Verify clamp boundaries
        engine.setPedalNoiseLevel(-0.5f);
        expectWithinAbsoluteError(engine.getPedalNoiseLevel(), 0.0f, 1e-4f);
        engine.setPedalNoiseLevel(1.8f);
        expectWithinAbsoluteError(engine.getPedalNoiseLevel(), 1.0f, 1e-4f);

        // Rebuild synth (switch tone and switch back)
        engine.setBuiltinSynthTone(AudioEngine::BuiltinSynthTone::sine);
        engine.setBuiltinSynthTone(AudioEngine::BuiltinSynthTone::piano);
        engine.setPedalNoiseLevel(0.65f);
        buffer.clear();
        engine.getNextAudioBlock(info);
        expectWithinAbsoluteError(engine.getPedalNoiseLevel(), 0.65f, 1e-4f);
    }

    void testPedalNoiseInMonoBufferActiveNote() {
        beginTest("Pedal noise is included in single-channel (mono) buffers during active note playback");

        juce::Synthesiser synth;
        synth.setCurrentPlaybackSampleRate(48000.0);
        synth.addSound(new PianoSynthSound());
        auto* voice = new PianoSynthVoice();
        voice->setVoiceIndex(0);
        voice->setPedalNoiseLevel(0.8f);
        voice->setAdsrParameters({ 0.001f, 0.2f, 0.8f, 0.3f });
        synth.addVoice(voice);

        synth.noteOn(1, 60, 0.7f);

        // Render one mono block without pedal
        juce::AudioBuffer<float> monoBufWithoutPedal(1, 512);
        monoBufWithoutPedal.clear();
        synth.renderNextBlock(monoBufWithoutPedal, juce::MidiBuffer(), 0, 512);
        const auto peakWithoutPedal = calculatePeak(monoBufWithoutPedal, 0);
        expectGreaterThan(peakWithoutPedal, 0.01f);

        // Now press sustain pedal during note playback
        synth.handleController(1, 64, 127);
        juce::AudioBuffer<float> monoBufWithPedal(1, 512);
        monoBufWithPedal.clear();
        synth.renderNextBlock(monoBufWithPedal, juce::MidiBuffer(), 0, 512);
        const auto peakWithPedal = calculatePeak(monoBufWithPedal, 0);

        expectGreaterThan(peakWithPedal, 0.01f);
        expectLessThan(peakWithPedal, 1.5f);

        // Ensure samples are finite and bounded
        for (int i = 0; i < 512; ++i) {
            const auto sample = monoBufWithPedal.getSample(0, i);
            expect(!std::isnan(sample));
            expect(!std::isinf(sample));
        }

        synth.noteOff(1, 60, 0.5f, false);
    }

    void testIdlePedalSympatheticResonanceShock() {
        beginTest("Idle sustain pedal press renders and decays sympathetic resonance shock beyond transient duration");

        PianoSynthVoice voice;
        voice.setCurrentPlaybackSampleRate(48000.0);
        voice.setVoiceIndex(0);
        voice.setPedalNoiseLevel(1.0f);

        // Idle state: no note active
        expect(!voice.isVoiceActive());

        // Press sustain pedal
        voice.controllerMoved(64, 127);

        // Render first 80ms (pedalTransient whoosh + shock)
        juce::AudioBuffer<float> buffer(2, 512);
        int initialSamples = static_cast<int>(48000 * 0.08);
        while (initialSamples > 0) {
            buffer.clear();
            const auto block = std::min(512, initialSamples);
            voice.renderNextBlock(buffer, 0, block);
            initialSamples -= block;
        }

        // At this point (80ms), pedalTransient whoosh (~65ms) has finished
        expect(!voice.isPedalTransientActive());

        // Render next block (sympathetic resonance shock decay)
        // With the fix, sympathetic resonance continues to render and decay
        buffer.clear();
        voice.renderNextBlock(buffer, 0, 512);
        const auto postTransientPeak = calculatePeak(buffer, 0);
        expectGreaterThan(postTransientPeak, 0.0f);

        // Render until tail decay completes (~400 ms total)
        int tailSamples = static_cast<int>(48000 * 0.35);
        while (tailSamples > 0) {
            buffer.clear();
            const auto block = std::min(512, tailSamples);
            voice.renderNextBlock(buffer, 0, block);
            tailSamples -= block;
        }

        // After 400+ ms, output should be completely silent and finite
        buffer.clear();
        voice.renderNextBlock(buffer, 0, 512);
        expectEquals(calculatePeak(buffer, 0), 0.0f);
        expectEquals(calculatePeak(buffer, 1), 0.0f);
    }
};

static PedalAcousticsTest pedalAcousticsTest;
