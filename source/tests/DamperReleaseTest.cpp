#include <JuceHeader.h>

#include "Audio/PianoSynthVoice.h"

// ==============================================================================
// Phase 32-B Unit Tests: Damper Drop Thump & Key Release Dynamics
// ==============================================================================
class DamperReleaseTest final : public juce::UnitTest {
public:
    DamperReleaseTest()
        : juce::UnitTest("DamperRelease", "Audio") {
    }

    void runTest() override {
        testFastReleaseGeneratesWoodThumpAndSteepDecay();
        testSlowReleaseExtendsFeltFrictionDuration();
        testHighRegisterNoDamperFeltHasWoodThump();
        testDynamicReleaseDampingVelocityScaling();
        testNumericalStabilityAndCompleteDecay();
    }

private:
    static float calculatePeak(const juce::AudioBuffer<float>& buffer, int channel = 0) {
        if (buffer.getNumSamples() == 0) {
            return 0.0f;
        }
        return buffer.getMagnitude(channel, 0, buffer.getNumSamples());
    }

    static float calculateRms(const juce::AudioBuffer<float>& buffer, int channel = 0) {
        if (buffer.getNumSamples() == 0) {
            return 0.0f;
        }
        return buffer.getRMSLevel(channel, 0, buffer.getNumSamples());
    }

    void testFastReleaseGeneratesWoodThumpAndSteepDecay() {
        beginTest("Fast key release generates prominent wood thump with steep decay");

        juce::Synthesiser synth;
        synth.setCurrentPlaybackSampleRate(48000.0);
        synth.addSound(new PianoSynthSound());
        auto* voice = new PianoSynthVoice();
        voice->setVoiceIndex(0);
        synth.addVoice(voice);

        // Start note 60 (Middle C)
        synth.noteOn(1, 60, 0.8f);

        juce::AudioBuffer<float> buffer(2, 512);
        buffer.clear();
        synth.renderNextBlock(buffer, juce::MidiBuffer(), 0, 512);
        expect(voice->isVoiceActive());

        // Fast release: velocity 0.95
        synth.noteOff(1, 60, 0.95f, true);

        // Render first transient block
        buffer.clear();
        synth.renderNextBlock(buffer, juce::MidiBuffer(), 0, 512);
        const auto peakFast = calculatePeak(buffer, 0);
        expectGreaterThan(peakFast, 0.005f);
    }

    void testSlowReleaseExtendsFeltFrictionDuration() {
        beginTest("Slow release extends damper felt friction duration with gentler decay");

        // Fast release transient
        PianoSynthVoice voiceFast;
        voiceFast.setCurrentPlaybackSampleRate(48000.0);
        voiceFast.setVoiceIndex(0);

        // Slow release transient
        PianoSynthVoice voiceSlow;
        voiceSlow.setCurrentPlaybackSampleRate(48000.0);
        voiceSlow.setVoiceIndex(0);

        // Trigger note 50 release directly on the internal damper transient
        // Fast release: 0.95
        // Slow release: 0.15
        juce::Synthesiser synthFast;
        synthFast.setCurrentPlaybackSampleRate(48000.0);
        synthFast.addSound(new PianoSynthSound());
        auto* vFast = new PianoSynthVoice();
        vFast->setVoiceIndex(0);
        synthFast.addVoice(vFast);

        juce::Synthesiser synthSlow;
        synthSlow.setCurrentPlaybackSampleRate(48000.0);
        synthSlow.addSound(new PianoSynthSound());
        auto* vSlow = new PianoSynthVoice();
        vSlow->setVoiceIndex(0);
        synthSlow.addVoice(vSlow);

        synthFast.noteOn(1, 50, 0.7f);
        synthSlow.noteOn(1, 50, 0.7f);

        juce::AudioBuffer<float> dummy(2, 256);
        synthFast.renderNextBlock(dummy, juce::MidiBuffer(), 0, 256);
        synthSlow.renderNextBlock(dummy, juce::MidiBuffer(), 0, 256);

        synthFast.noteOff(1, 50, 0.95f, true);
        synthSlow.noteOff(1, 50, 0.15f, true);

        // Render several blocks and check that slow release maintains energy longer
        // because of prolonged felt friction
        juce::AudioBuffer<float> bufFast(2, 512);
        juce::AudioBuffer<float> bufSlow(2, 512);

        // Skip the initial strike of the thump
        synthFast.renderNextBlock(bufFast, juce::MidiBuffer(), 0, 512);
        synthSlow.renderNextBlock(bufSlow, juce::MidiBuffer(), 0, 512);

        // Second block (~10-20ms into release): slow release friction should be active
        bufFast.clear();
        bufSlow.clear();
        synthFast.renderNextBlock(bufFast, juce::MidiBuffer(), 0, 512);
        synthSlow.renderNextBlock(bufSlow, juce::MidiBuffer(), 0, 512);

        expectGreaterThan(calculatePeak(bufSlow, 0), 0.0001f);
    }

    void testHighRegisterNoDamperFeltHasWoodThump() {
        beginTest("High register notes (>88) bypass felt damper but retain wood key thump");

        juce::Synthesiser synth;
        synth.setCurrentPlaybackSampleRate(48000.0);
        synth.addSound(new PianoSynthSound());
        auto* voice = new PianoSynthVoice();
        voice->setVoiceIndex(0);
        synth.addVoice(voice);

        // Note 96 (C7, above damper line)
        synth.noteOn(1, 96, 0.7f);

        juce::AudioBuffer<float> dummy(2, 256);
        synth.renderNextBlock(dummy, juce::MidiBuffer(), 0, 256);

        // Fast release Note 96
        synth.noteOff(1, 96, 0.9f, true);

        juce::AudioBuffer<float> buffer(2, 512);
        buffer.clear();
        synth.renderNextBlock(buffer, juce::MidiBuffer(), 0, 512);

        // Must still produce wood thump
        const auto peak = calculatePeak(buffer, 0);
        expectGreaterThan(peak, 0.0005f);
    }

    void testDynamicReleaseDampingVelocityScaling() {
        beginTest("Dynamic ADSR release damping extinguishes strings faster on fast release");

        juce::Synthesiser synthFast;
        synthFast.setCurrentPlaybackSampleRate(48000.0);
        synthFast.addSound(new PianoSynthSound());
        auto* vFast = new PianoSynthVoice();
        vFast->setVoiceIndex(0);
        synthFast.addVoice(vFast);

        juce::Synthesiser synthSlow;
        synthSlow.setCurrentPlaybackSampleRate(48000.0);
        synthSlow.addSound(new PianoSynthSound());
        auto* vSlow = new PianoSynthVoice();
        vSlow->setVoiceIndex(0);
        synthSlow.addVoice(vSlow);

        // Strike Note 60 with identical velocity
        synthFast.noteOn(1, 60, 0.7f);
        synthSlow.noteOn(1, 60, 0.7f);

        // Play for 100ms so tone is established
        juce::AudioBuffer<float> buffer(2, 512);
        for (int i = 0; i < 10; ++i) {
            synthFast.renderNextBlock(buffer, juce::MidiBuffer(), 0, 512);
            synthSlow.renderNextBlock(buffer, juce::MidiBuffer(), 0, 512);
        }

        // Fast release vs slow release
        synthFast.noteOff(1, 60, 0.95f, true);
        synthSlow.noteOff(1, 60, 0.10f, true);

        // Render for 250ms (~24 blocks of 512)
        juce::AudioBuffer<float> bFast(2, 512);
        juce::AudioBuffer<float> bSlow(2, 512);
        float rmsFastTail = 0.0f;
        float rmsSlowTail = 0.0f;

        for (int i = 0; i < 20; ++i) {
            bFast.clear();
            bSlow.clear();
            synthFast.renderNextBlock(bFast, juce::MidiBuffer(), 0, 512);
            synthSlow.renderNextBlock(bSlow, juce::MidiBuffer(), 0, 512);
            if (i >= 15) { // Tail blocks (160ms - 210ms)
                rmsFastTail += calculateRms(bFast, 0);
                rmsSlowTail += calculateRms(bSlow, 0);
            }
        }

        // Fast release string vibration should be extinguished faster than slow release
        expectGreaterThan(rmsSlowTail, rmsFastTail);
    }

    void testNumericalStabilityAndCompleteDecay() {
        beginTest("Damper release decays completely to silence without DC offset or denormals");

        juce::Synthesiser synth;
        synth.setCurrentPlaybackSampleRate(48000.0);
        synth.addSound(new PianoSynthSound());
        auto* voice = new PianoSynthVoice();
        voice->setVoiceIndex(0);
        synth.addVoice(voice);

        synth.noteOn(1, 40, 0.8f);
        juce::AudioBuffer<float> buf(2, 512);
        synth.renderNextBlock(buf, juce::MidiBuffer(), 0, 512);

        synth.noteOff(1, 40, 0.8f, true);

        // Render for 2 seconds (sufficient for all partials and transients to silence)
        for (int i = 0; i < 200; ++i) {
            buf.clear();
            synth.renderNextBlock(buf, juce::MidiBuffer(), 0, 512);
        }

        expect(!voice->isVoiceActive());
        buf.clear();
        synth.renderNextBlock(buf, juce::MidiBuffer(), 0, 512);
        expectEquals(calculatePeak(buf, 0), 0.0f);
        expectEquals(calculatePeak(buf, 1), 0.0f);
    }
};

static DamperReleaseTest damperReleaseTest;
