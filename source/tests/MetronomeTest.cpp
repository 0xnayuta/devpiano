#include <JuceHeader.h>

#include "Audio/AudioEngine.h"
#include "Audio/MetronomeProcessor.h"
#include "Core/MetronomeModel.h"

namespace {

class MetronomeTest final : public juce::UnitTest {
public:
    MetronomeTest()
        : juce::UnitTest("Metronome: Sample-Accurate Timing & Sound Engine", "DevPiano/Audio") {
    }

    void runTest() override {
        testTimeSignatureCalculations();
        testTapTempoCalculator();
        testMetronomeProcessorBeatTiming();
        testMetronomeProcessorSixEightMeter();
        testMetronomeAudioMixingAndVolume();
        testMetronomeBlockBoundaryArbitraryOffset();
        testMetronomeDynamicBpmChange();
        testMetronomeAudioEngineIntegration();
        testCountInModel();
    }

private:
    void testTimeSignatureCalculations() {
        beginTest("Time signature model numerator, denominator, and naming");

        using devpiano::core::TimeSignature;
        expectEquals(devpiano::core::getTimeSignatureNumerator(TimeSignature::twoFour), 2);
        expectEquals(devpiano::core::getTimeSignatureDenominator(TimeSignature::twoFour), 4);
        expect(juce::String(devpiano::core::getTimeSignatureName(TimeSignature::twoFour)) == "2/4");

        expectEquals(devpiano::core::getTimeSignatureNumerator(TimeSignature::threeFour), 3);
        expectEquals(devpiano::core::getTimeSignatureDenominator(TimeSignature::threeFour), 4);
        expect(juce::String(devpiano::core::getTimeSignatureName(TimeSignature::threeFour)) == "3/4");

        expectEquals(devpiano::core::getTimeSignatureNumerator(TimeSignature::fourFour), 4);
        expectEquals(devpiano::core::getTimeSignatureDenominator(TimeSignature::fourFour), 4);
        expect(juce::String(devpiano::core::getTimeSignatureName(TimeSignature::fourFour)) == "4/4");

        expectEquals(devpiano::core::getTimeSignatureNumerator(TimeSignature::sixEight), 6);
        expectEquals(devpiano::core::getTimeSignatureDenominator(TimeSignature::sixEight), 8);
        expect(juce::String(devpiano::core::getTimeSignatureName(TimeSignature::sixEight)) == "6/8");
    }

    void testTapTempoCalculator() {
        beginTest("Tap tempo uses a three-interval rolling average, clamping, and timeout");

        devpiano::core::TapTempoCalculator tap;
        expect(!tap.calculateBpm().has_value());

        tap.recordTap(10.0);
        expect(!tap.calculateBpm().has_value());
        tap.recordTap(10.5);
        auto bpm = tap.calculateBpm();
        expect(bpm.has_value());
        expectWithinAbsoluteError(*bpm, 120.0, 0.1);

        tap.recordTap(11.3);
        bpm = tap.calculateBpm();
        expect(bpm.has_value());
        expectWithinAbsoluteError(*bpm, 60.0 / 0.65, 0.001);

        tap.recordTap(12.4);
        bpm = tap.calculateBpm();
        expect(bpm.has_value());
        expectWithinAbsoluteError(*bpm, 75.0, 0.001);

        tap.recordTap(12.6);
        bpm = tap.calculateBpm();
        expect(bpm.has_value());
        expectWithinAbsoluteError(*bpm, 60.0 / 0.7, 0.001);
        expectEquals(tap.getTapCount(), 4);

        tap.reset();
        tap.recordTap(1.0);
        tap.recordTap(1.1);
        bpm = tap.calculateBpm();
        expect(bpm.has_value());
        expectWithinAbsoluteError(*bpm, 280.0, 0.001);

        tap.reset();
        tap.recordTap(1.0);
        tap.recordTap(3.0);
        bpm = tap.calculateBpm();
        expect(bpm.has_value());
        expectWithinAbsoluteError(*bpm, 40.0, 0.001);

        tap.reset();
        tap.recordTap(1.0);
        tap.recordTap(1.5);
        expect(tap.calculateBpm().has_value());
        tap.recordTap(4.0);
        expect(!tap.calculateBpm().has_value());
        expectEquals(tap.getTapCount(), 1);
    }

    void testMetronomeProcessorBeatTiming() {
        beginTest("Sample-accurate beat progression in 4/4 time");

        devpiano::audio::MetronomeProcessor processor;
        processor.prepareToPlay(48000.0);
        processor.setBpm(120.0);
        processor.setTimeSignature(devpiano::core::TimeSignature::fourFour);
        processor.setVolume(1.0f);

        // In 48000 Hz at 120 BPM, 4/4 time has exactly 48000 * 60 / 120 = 24000 samples per beat.
        processor.setEnabled(true);
        expectEquals(processor.getCurrentBeatNumber(), 0);
        expect(processor.getIsDownbeat());
        const auto initialSeq = processor.getBeatSequence();
        expect(initialSeq > 0);

        juce::AudioBuffer<float> buffer(2, 24000);
        buffer.clear();

        // Process exactly one beat (24000 samples)
        processor.processAndMix(&buffer, 0, 24000);
        // After 24000 samples, beat 1 should have been triggered
        expectEquals(processor.getCurrentBeatNumber(), 1);
        expect(!processor.getIsDownbeat());
        expect(processor.getBeatSequence() == initialSeq + 1);

        // Process another beat
        buffer.clear();
        processor.processAndMix(&buffer, 0, 24000);
        expectEquals(processor.getCurrentBeatNumber(), 2);
        expect(!processor.getIsDownbeat());
        expect(processor.getBeatSequence() == initialSeq + 2);

        // Process another beat
        buffer.clear();
        processor.processAndMix(&buffer, 0, 24000);
        expectEquals(processor.getCurrentBeatNumber(), 3);
        expect(!processor.getIsDownbeat());
        expect(processor.getBeatSequence() == initialSeq + 3);

        // Next beat wraps around to 0 (downbeat)
        buffer.clear();
        processor.processAndMix(&buffer, 0, 24000);
        expectEquals(processor.getCurrentBeatNumber(), 0);
        expect(processor.getIsDownbeat());
        expect(processor.getBeatSequence() == initialSeq + 4);
    }

    void testMetronomeProcessorSixEightMeter() {
        beginTest("6/8 compound meter beat cycle and secondary accent");

        devpiano::audio::MetronomeProcessor processor;
        processor.prepareToPlay(48000.0);
        processor.setBpm(120.0);
        processor.setTimeSignature(devpiano::core::TimeSignature::sixEight);
        processor.setVolume(1.0f);

        // In 6/8 meter: denominator is 8, so samplesPerBeat = (48000 * 60 / 120) * (4 / 8) = 12000 samples.
        processor.setEnabled(true);
        expectEquals(processor.getCurrentBeatNumber(), 0);
        expect(processor.getIsDownbeat());

        juce::AudioBuffer<float> buffer(2, 12000);

        // 6 beats in cycle: 0 -> 1 -> 2 -> 3 (secondary accent) -> 4 -> 5 -> 0
        for (int expectedBeat = 1; expectedBeat < 6; ++expectedBeat) {
            buffer.clear();
            processor.processAndMix(&buffer, 0, 12000);
            expectEquals(processor.getCurrentBeatNumber(), expectedBeat);
            expect(!processor.getIsDownbeat());
        }

        // 6th step wraps to 0
        buffer.clear();
        processor.processAndMix(&buffer, 0, 12000);
        expectEquals(processor.getCurrentBeatNumber(), 0);
        expect(processor.getIsDownbeat());
    }

    void testMetronomeAudioMixingAndVolume() {
        beginTest("Audio output energy and silent advancement");

        devpiano::audio::MetronomeProcessor processor;
        processor.prepareToPlay(48000.0);
        processor.setBpm(120.0);
        processor.setTimeSignature(devpiano::core::TimeSignature::fourFour);

        juce::AudioBuffer<float> buffer(2, 512);

        // 1. When disabled, buffer remains clean
        processor.setEnabled(false);
        buffer.clear();
        processor.processAndMix(&buffer, 0, 512);
        expectEquals(buffer.getMagnitude(0, 512), 0.0f);

        // 2. When enabled with volume > 0, buffer has acoustic click pulse
        processor.setEnabled(true);
        processor.setVolume(0.8f);
        buffer.clear();
        processor.processAndMix(&buffer, 0, 512);
        const float mag = buffer.getMagnitude(0, 512);
        expect(mag > 0.05f);

        // 3. When volume is 0, audio is clean but sequence advances
        processor.setVolume(0.0f);
        const auto seqBefore = processor.getBeatSequence();
        for (int i = 0; i < 200; ++i) { // 200 * 512 = 102400 samples > 4 beats (96000)
            buffer.clear();
            processor.processAndMix(&buffer, 0, 512);
            expectEquals(buffer.getMagnitude(0, 512), 0.0f);
        }
        expect(processor.getBeatSequence() > seqBefore);
    }

    void testMetronomeBlockBoundaryArbitraryOffset() {
        beginTest("Small audio blocks with arbitrary sample offsets maintain exact rhythm");

        devpiano::audio::MetronomeProcessor processor;
        processor.prepareToPlay(48000.0);
        processor.setBpm(120.0); // 24000 samples per beat
        processor.setTimeSignature(devpiano::core::TimeSignature::fourFour);
        processor.setEnabled(true);

        const int blockSize = 128;
        juce::AudioBuffer<float> buffer(2, blockSize);

        const auto startSeq = processor.getBeatSequence();
        // Run exactly 24000 samples in 128-sample chunks: 24000 / 128 = 187.5 blocks
        // Run 188 blocks: 188 * 128 = 24064 samples (slightly past 24000)
        for (int block = 0; block < 188; ++block) {
            buffer.clear();
            processor.processAndMix(&buffer, 0, blockSize);
        }

        // Exactly one new beat has been fired
        expect(processor.getBeatSequence() == startSeq + 1);
        expectEquals(processor.getCurrentBeatNumber(), 1);
    }

    void testMetronomeDynamicBpmChange() {
        beginTest("Dynamic BPM change scales beat interval smoothly without glitching");

        devpiano::audio::MetronomeProcessor processor;
        processor.prepareToPlay(48000.0);
        processor.setBpm(60.0); // 48000 samples per beat
        processor.setEnabled(true);

        juce::AudioBuffer<float> buffer(2, 512);
        for (int i = 0; i < 10; ++i) {
            buffer.clear();
            processor.processAndMix(&buffer, 0, 512);
        }

        // Change BPM mid-stream to 240.0 (12000 samples per beat)
        processor.setBpm(240.0);
        expectWithinAbsoluteError(processor.getBpm(), 240.0, 0.01);

        for (int i = 0; i < 30; ++i) {
            buffer.clear();
            processor.processAndMix(&buffer, 0, 512);
        }
        // At 240 BPM, beat progression moves 4x faster
        expect(processor.getBeatSequence() > 1);
    }

    void testMetronomeAudioEngineIntegration() {
        beginTest("AudioEngine metronome public interface and rendering integration");

        AudioEngine engine;
        engine.prepareToPlay(256, 48000.0);
        juce::AudioBuffer<float> buffer(2, 256);
        juce::AudioSourceChannelInfo info(&buffer, 0, 256);

        // Consume startup warmup blocks (25ms = ~5 blocks of 256 samples)
        for (int i = 0; i < 8; ++i) {
            buffer.clear();
            engine.getNextAudioBlock(info);
        }

        expect(!engine.isMetronomeEnabled());
        engine.setMetronomeEnabled(true);
        expect(engine.isMetronomeEnabled());

        engine.setMetronomeBpm(130.0);
        expectWithinAbsoluteError(engine.getMetronomeBpm(), 130.0, 0.01);

        engine.setMetronomeTimeSignature(devpiano::core::TimeSignature::threeFour);
        expect(engine.getMetronomeTimeSignature() == devpiano::core::TimeSignature::threeFour);

        engine.setMetronomeVolume(0.85f);
        expectWithinAbsoluteError(static_cast<double>(engine.getMetronomeVolume()), 0.85, 0.01);

        buffer.clear();
        engine.getNextAudioBlock(info);

        // Click transient mixed into buffer
        expect(buffer.getMagnitude(0, 256) > 0.01f);
    }

    void testCountInModel() {
        beginTest("Count-in bars model helper");

        using devpiano::core::CountInBars;
        expectEquals(devpiano::core::getCountInBarCount(CountInBars::none), 0);
        expectEquals(devpiano::core::getCountInBarCount(CountInBars::oneBar), 1);
        expectEquals(devpiano::core::getCountInBarCount(CountInBars::twoBars), 2);
    }
};

static MetronomeTest metronomeTest;

} // namespace
