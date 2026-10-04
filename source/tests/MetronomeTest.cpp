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
        testMetronomeBpmPhaseRebase();
        testMutedClickExpiresBeforeUnmute();
        testMetronomeAudioEngineIntegration();
        testCountInModel();
        testMetronomeEnabledBeforePrepare();
        testMetronomeStateTransitionsAndLifecycle();
        testCountInSampleAccurateDownbeatOneAndTwoBars();
        testCountInMultiBeatUiPollLeaps();
        testCountInCancelAndRearm();
        testCountInPrepareSampleRateChangeHalfway();
        testCountInSilentMetronome();
        testCountInTimeSignaturePeriods();
        testZeroTrigBeatPulseStability();
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
        if (bpm.has_value()) {
            expectWithinAbsoluteError(*bpm, 120.0, 0.1);
        }

        tap.recordTap(11.3);
        bpm = tap.calculateBpm();
        expect(bpm.has_value());
        if (bpm.has_value()) {
            expectWithinAbsoluteError(*bpm, 60.0 / 0.65, 0.001);
        }

        tap.recordTap(12.4);
        bpm = tap.calculateBpm();
        expect(bpm.has_value());
        if (bpm.has_value()) {
            expectWithinAbsoluteError(*bpm, 75.0, 0.001);
        }

        tap.recordTap(12.6);
        bpm = tap.calculateBpm();
        expect(bpm.has_value());
        if (bpm.has_value()) {
            expectWithinAbsoluteError(*bpm, 60.0 / 0.7, 0.001);
        }
        expectEquals(tap.getTapCount(), 4);

        tap.reset();
        tap.recordTap(1.0);
        tap.recordTap(1.1);
        bpm = tap.calculateBpm();
        expect(bpm.has_value());
        if (bpm.has_value()) {
            expectWithinAbsoluteError(*bpm, 280.0, 0.001);
        }

        tap.reset();
        tap.recordTap(1.0);
        tap.recordTap(3.0);
        bpm = tap.calculateBpm();
        expect(bpm.has_value());
        if (bpm.has_value()) {
            expectWithinAbsoluteError(*bpm, 40.0, 0.001);
        }

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
        const auto initialSeq = processor.getBeatSequence();
        processor.setEnabled(true);
        expect(processor.getBeatSequence() == initialSeq);
        expect(processor.isEnabled());

        juce::AudioBuffer<float> buffer(2, 24000);
        buffer.clear();

        processor.processAndMix(&buffer, 0, 1);
        expectEquals(processor.getCurrentBeatNumber(), 0);
        expect(processor.getIsDownbeat());
        expect(processor.getBeatSequence() == initialSeq + 1);

        processor.processAndMix(&buffer, 1, 23999);
        expectEquals(processor.getCurrentBeatNumber(), 1);
        expect(!processor.getIsDownbeat());
        expect(processor.getBeatSequence() == initialSeq + 2);

        // Process another beat
        buffer.clear();
        processor.processAndMix(&buffer, 0, 24000);
        expectEquals(processor.getCurrentBeatNumber(), 2);
        expect(!processor.getIsDownbeat());
        expect(processor.getBeatSequence() == initialSeq + 3);

        // Process another beat
        buffer.clear();
        processor.processAndMix(&buffer, 0, 24000);
        expectEquals(processor.getCurrentBeatNumber(), 3);
        expect(!processor.getIsDownbeat());
        expect(processor.getBeatSequence() == initialSeq + 4);

        // Next beat wraps around to 0 (downbeat)
        buffer.clear();
        processor.processAndMix(&buffer, 0, 24000);
        expectEquals(processor.getCurrentBeatNumber(), 0);
        expect(processor.getIsDownbeat());
        expect(processor.getBeatSequence() == initialSeq + 5);
    }

    void testMetronomeProcessorSixEightMeter() {
        beginTest("6/8 compound meter beat cycle and secondary accent");

        devpiano::audio::MetronomeProcessor processor;
        processor.prepareToPlay(48000.0);
        processor.setBpm(120.0);
        processor.setTimeSignature(devpiano::core::TimeSignature::sixEight);
        processor.setVolume(1.0f);

        // In 6/8 meter: denominator is 8, so samplesPerBeat = (48000 * 60 / 120) * (4 / 8) = 12000 samples.
        const auto initialSeq = processor.getBeatSequence();
        processor.setEnabled(true);
        expect(processor.getBeatSequence() == initialSeq);
        expect(processor.isEnabled());

        juce::AudioBuffer<float> buffer(2, 12000);

        buffer.clear();
        processor.processAndMix(&buffer, 0, 1);
        expectEquals(processor.getCurrentBeatNumber(), 0);
        expect(processor.getIsDownbeat());
        expect(processor.getBeatSequence() == initialSeq + 1);

        processor.processAndMix(&buffer, 1, 11999);
        expectEquals(processor.getCurrentBeatNumber(), 1);
        expect(!processor.getIsDownbeat());
        expect(processor.getBeatSequence() == initialSeq + 2);

        // 6 beats in cycle: 1 -> 2 -> 3 (secondary accent) -> 4 -> 5 -> 0
        for (int expectedBeat = 2; expectedBeat < 6; ++expectedBeat) {
            buffer.clear();
            processor.processAndMix(&buffer, 0, 12000);
            expectEquals(processor.getCurrentBeatNumber(), expectedBeat);
            expect(!processor.getIsDownbeat());
            expect(processor.getBeatSequence() == initialSeq + 2 + (expectedBeat - 1));
        }

        // 6th step wraps to 0
        buffer.clear();
        processor.processAndMix(&buffer, 0, 12000);
        expectEquals(processor.getCurrentBeatNumber(), 0);
        expect(processor.getIsDownbeat());
        expect(processor.getBeatSequence() == initialSeq + 7);
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

        const auto startSeq = processor.getBeatSequence();
        processor.setEnabled(true);
        expect(processor.getBeatSequence() == startSeq);

        const int blockSize = 128;
        juce::AudioBuffer<float> buffer(2, blockSize);

        // Run exactly 24000 samples in 128-sample chunks: 24000 / 128 = 187.5 blocks
        // Run 188 blocks: 188 * 128 = 24064 samples (slightly past 24000)
        for (int block = 0; block < 188; ++block) {
            buffer.clear();
            processor.processAndMix(&buffer, 0, blockSize);
        }

        expect(processor.getBeatSequence() == startSeq + 2);
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

    void testMetronomeBpmPhaseRebase() {
        beginTest("BPM changes preserve fractional beat phase without catch-up clicks");

        devpiano::audio::MetronomeProcessor processor;
        processor.prepareToPlay(48000.0);
        processor.setBpm(60.0);
        processor.setEnabled(true);
        juce::AudioBuffer<float> buffer(2, 48000);

        processor.processAndMix(&buffer, 0, 24000);
        const auto sequenceBeforeChange = processor.getBeatSequence();
        processor.setBpm(240.0);

        buffer.clear();
        processor.processAndMix(&buffer, 0, 5999);
        expect(processor.getBeatSequence() == sequenceBeforeChange);

        buffer.clear();
        processor.processAndMix(&buffer, 0, 1);
        expect(processor.getBeatSequence() == sequenceBeforeChange + 1);
        expectEquals(processor.getCurrentBeatNumber(), 1);
    }

    void testMutedClickExpiresBeforeUnmute() {
        beginTest("A click decays while muted and does not reappear after unmuting");

        devpiano::audio::MetronomeProcessor processor;
        processor.prepareToPlay(48000.0);
        processor.setBpm(120.0);
        processor.setVolume(0.0f);
        processor.setEnabled(true);
        juce::AudioBuffer<float> buffer(2, 20000);

        processor.processAndMix(&buffer, 0, 1);
        buffer.clear();
        processor.processAndMix(&buffer, 0, 16000);
        expectEquals(buffer.getMagnitude(0, 16000), 0.0f);

        processor.setVolume(1.0f);
        buffer.clear();
        processor.processAndMix(&buffer, 0, 64);
        expectEquals(buffer.getMagnitude(0, 64), 0.0f);
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

    void testMetronomeStateTransitionsAndLifecycle() {
        beginTest("Metronome atomic run-state transitions, idempotence, and lifecycle reset");

        devpiano::audio::MetronomeProcessor processor;
        processor.prepareToPlay(48000.0);
        processor.setBpm(120.0);
        processor.setTimeSignature(devpiano::core::TimeSignature::fourFour);
        processor.setVolume(1.0f);

        juce::AudioBuffer<float> buffer(2, 512);

        processor.setEnabled(false);
        processor.setEnabled(false);
        expect(!processor.isEnabled());

        const auto initialSeq = processor.getBeatSequence();
        processor.setEnabled(true);
        expect(processor.isEnabled());
        expect(processor.getBeatSequence() == initialSeq);

        processor.setEnabled(true);
        expect(processor.getBeatSequence() == initialSeq);

        buffer.clear();
        processor.processAndMix(&buffer, 0, 512);
        expectEquals(processor.getCurrentBeatNumber(), 0);
        expect(processor.getIsDownbeat());
        expect(processor.getBeatSequence() == initialSeq + 1);

        processor.setEnabled(true);
        expect(processor.getBeatSequence() == initialSeq + 1);

        processor.prepareToPlay(48000.0);
        expect(processor.isEnabled());
        const auto seqBeforePrepare = processor.getBeatSequence();

        buffer.clear();
        processor.processAndMix(&buffer, 0, 512);
        expectEquals(processor.getCurrentBeatNumber(), 0);
        expect(processor.getIsDownbeat());
        expect(processor.getBeatSequence() == seqBeforePrepare + 1);

        processor.setEnabled(false);
        expect(!processor.isEnabled());
        buffer.clear();
        processor.processAndMix(&buffer, 0, 512);
        expectEquals(buffer.getMagnitude(0, 512), 0.0f);

        const auto seqBeforeReEnable = processor.getBeatSequence();
        processor.setEnabled(true);
        expect(processor.isEnabled());
        expect(processor.getBeatSequence() == seqBeforeReEnable);

        buffer.clear();
        processor.processAndMix(&buffer, 0, 512);
        expectEquals(processor.getCurrentBeatNumber(), 0);
        expect(processor.getIsDownbeat());
        expect(processor.getBeatSequence() == seqBeforeReEnable + 1);
        expect(buffer.getMagnitude(0, 512) > 0.01f);

        processor.setEnabled(false);
        processor.setVolume(0.0f);
        const auto seqBeforeSilent = processor.getBeatSequence();
        processor.setEnabled(true);
        expect(processor.getBeatSequence() == seqBeforeSilent);

        buffer.clear();
        processor.processAndMix(&buffer, 0, 512);
        expectEquals(processor.getCurrentBeatNumber(), 0);
        expect(processor.getIsDownbeat());
        expect(processor.getBeatSequence() == seqBeforeSilent + 1);
        expectEquals(buffer.getMagnitude(0, 512), 0.0f);
    }

    void testMetronomeEnabledBeforePrepare() {
        beginTest("Enabling before prepare still schedules the first downbeat");

        devpiano::audio::MetronomeProcessor processor;
        processor.setEnabled(true);
        processor.prepareToPlay(48000.0);
        processor.setVolume(1.0f);

        juce::AudioBuffer<float> buffer(2, 128);
        processor.processAndMix(&buffer, 0, 128);

        expect(processor.isEnabled());
        expect(processor.getBeatSequence() == std::uint32_t { 1 });
        expectEquals(processor.getCurrentBeatNumber(), 0);
        expect(processor.getIsDownbeat());
        expect(buffer.getMagnitude(0, 128) > 0.01f);
    }
    void testCountInSampleAccurateDownbeatOneAndTwoBars() {
        beginTest("Count-in one and two bars reach target downbeat at exact sample");

        // 1 bar at 120 BPM 4/4: 4 beats = 2.0s = 96000 samples at 48kHz
        {
            devpiano::audio::MetronomeProcessor processor;
            processor.prepareToPlay(48000.0);
            processor.setBpm(120.0);
            processor.setTimeSignature(devpiano::core::TimeSignature::fourFour);
            processor.setVolume(1.0f);
            processor.armCountIn(4);

            expect(processor.isCountInArmed());
            expectEquals(processor.getCountInRemainingBeats(), 4);

            juce::AudioBuffer<float> buffer(2, 512);
            std::int64_t totalSamples = 0;
            int startOffset = -1;
            std::int64_t targetSample = -1;

            while (totalSamples < 120000) {
                buffer.clear();
                const int offset = processor.consumeCountInStartOffset(512);
                if (offset >= 0 && startOffset < 0) {
                    startOffset = offset;
                    targetSample = totalSamples + offset;
                }
                processor.processAndMix(&buffer, 0, 512);
                totalSamples += 512;
                if (startOffset >= 0) {
                    break;
                }
            }

            expect(startOffset >= 0);
            expectEquals(targetSample, std::int64_t { 96000 });
            expect(!processor.isCountInArmed());
            expectEquals(processor.getCountInRemainingBeats(), 0);
            expectEquals(processor.getCurrentBeatNumber(), 0);
            expect(processor.getIsDownbeat());

            // Subsequent block must not trigger count-in start again
            expectEquals(processor.consumeCountInStartOffset(512), -1);
        }

        // 2 bars at 120 BPM 4/4: 8 beats = 4.0s = 192000 samples at 48kHz
        {
            devpiano::audio::MetronomeProcessor processor;
            processor.prepareToPlay(48000.0);
            processor.setBpm(120.0);
            processor.setTimeSignature(devpiano::core::TimeSignature::fourFour);
            processor.setVolume(1.0f);
            processor.armCountIn(8);

            expect(processor.isCountInArmed());
            expectEquals(processor.getCountInRemainingBeats(), 8);

            juce::AudioBuffer<float> buffer(2, 512);
            std::int64_t totalSamples = 0;
            int startOffset = -1;
            std::int64_t targetSample = -1;

            while (totalSamples < 240000) {
                buffer.clear();
                const int offset = processor.consumeCountInStartOffset(512);
                if (offset >= 0 && startOffset < 0) {
                    startOffset = offset;
                    targetSample = totalSamples + offset;
                }
                processor.processAndMix(&buffer, 0, 512);
                totalSamples += 512;
                if (startOffset >= 0) {
                    break;
                }
            }

            expect(startOffset >= 0);
            expectEquals(targetSample, std::int64_t { 192000 });
            expect(!processor.isCountInArmed());
            expectEquals(processor.getCountInRemainingBeats(), 0);
            expectEquals(processor.getCurrentBeatNumber(), 0);
            expect(processor.getIsDownbeat());
        }
    }

    void testCountInMultiBeatUiPollLeaps() {
        beginTest("Multi-beat UI poll delays do not alter audio count-in timing");

        devpiano::audio::MetronomeProcessor processor;
        processor.prepareToPlay(48000.0);
        processor.setBpm(120.0);
        processor.setTimeSignature(devpiano::core::TimeSignature::fourFour);
        processor.setVolume(1.0f);
        processor.armCountIn(4);

        juce::AudioBuffer<float> buffer(2, 512);
        std::int64_t totalSamples = 0;

        // Process 60000 samples (2.5 beats) without checking
        while (totalSamples < 60000) {
            buffer.clear();
            expectEquals(processor.consumeCountInStartOffset(512), -1);
            processor.processAndMix(&buffer, 0, 512);
            totalSamples += 512;
        }

        // UI wakes up late and checks remaining beats: 2 beats remaining
        expectEquals(processor.getCountInRemainingBeats(), 2);

        int startOffset = -1;
        std::int64_t targetSample = -1;
        while (totalSamples < 120000) {
            buffer.clear();
            const int offset = processor.consumeCountInStartOffset(512);
            if (offset >= 0 && startOffset < 0) {
                startOffset = offset;
                targetSample = totalSamples + offset;
            }
            processor.processAndMix(&buffer, 0, 512);
            totalSamples += 512;
            if (startOffset >= 0) {
                break;
            }
        }

        expect(startOffset >= 0);
        expectEquals(targetSample, std::int64_t { 96000 });
    }

    void testCountInCancelAndRearm() {
        beginTest("Count-in cancel stops counting and re-arming anchors fresh period");

        devpiano::audio::MetronomeProcessor processor;
        processor.prepareToPlay(48000.0);
        processor.setBpm(120.0);
        processor.setTimeSignature(devpiano::core::TimeSignature::fourFour);
        processor.setVolume(1.0f);
        processor.armCountIn(4);

        juce::AudioBuffer<float> buffer(2, 512);
        for (int i = 0; i < 20; ++i) {
            buffer.clear();
            expectEquals(processor.consumeCountInStartOffset(512), -1);
            processor.processAndMix(&buffer, 0, 512);
        }

        processor.cancelCountIn();
        expect(!processor.isCountInArmed());
        expectEquals(processor.getCountInRemainingBeats(), 0);
        expectEquals(processor.consumeCountInStartOffset(512), -1);

        // Re-arm count-in for 4 beats: must count fresh 96000 samples
        processor.armCountIn(4);
        expect(processor.isCountInArmed());
        expectEquals(processor.getCountInRemainingBeats(), 4);

        std::int64_t totalSamples = 0;
        int startOffset = -1;
        std::int64_t targetSample = -1;
        while (totalSamples < 120000) {
            buffer.clear();
            const int offset = processor.consumeCountInStartOffset(512);
            if (offset >= 0 && startOffset < 0) {
                startOffset = offset;
                targetSample = totalSamples + offset;
            }
            processor.processAndMix(&buffer, 0, 512);
            totalSamples += 512;
            if (startOffset >= 0) {
                break;
            }
        }

        expect(startOffset >= 0);
        expectEquals(targetSample, std::int64_t { 96000 });
    }

    void testCountInPrepareSampleRateChangeHalfway() {
        beginTest("Device rate prepare halfway preserves count-in duration and monotonic sequence");

        devpiano::audio::MetronomeProcessor processor;
        processor.prepareToPlay(48000.0);
        processor.setBpm(120.0);
        processor.setTimeSignature(devpiano::core::TimeSignature::fourFour);
        processor.setVolume(1.0f);
        processor.armCountIn(4);

        juce::AudioBuffer<float> buffer(2, 512);
        std::int64_t totalSamplesAt48k = 0;

        // Process 48 blocks = 24576 samples at 48k (beat 0 finished at 24000, 576 samples into beat 1)
        for (int i = 0; i < 48; ++i) {
            buffer.clear();
            expectEquals(processor.consumeCountInStartOffset(512), -1);
            processor.processAndMix(&buffer, 0, 512);
            totalSamplesAt48k += 512;
        }

        expect(processor.isCountInArmed());
        expectEquals(processor.getCountInRemainingBeats(), 3);
        const auto seqAtSwitch = processor.getBeatSequence();

        // Audio device switches to 44.1kHz
        processor.prepareToPlay(44100.0);
        expect(processor.isCountInArmed());
        expectEquals(processor.getCountInRemainingBeats(), 3);
        expectEquals(processor.getBeatSequence(), seqAtSwitch);

        std::int64_t totalSamplesAt44k = 0;
        int startOffset = -1;
        std::int64_t targetSampleAt44k = -1;

        while (totalSamplesAt44k < 100000) {
            buffer.clear();
            const int offset = processor.consumeCountInStartOffset(512);
            if (offset >= 0 && startOffset < 0) {
                startOffset = offset;
                targetSampleAt44k = totalSamplesAt44k + offset;
            }
            processor.processAndMix(&buffer, 0, 512);
            totalSamplesAt44k += 512;
            if (startOffset >= 0) {
                break;
            }
        }

        expect(startOffset >= 0);
        // Total duration: 24576/48000 + targetSampleAt44k/44100 == 2.0s
        const double totalSeconds
            = (static_cast<double>(totalSamplesAt48k) / 48000.0) + (static_cast<double>(targetSampleAt44k) / 44100.0);
        expectWithinAbsoluteError(totalSeconds, 2.0, 0.001);
        expect(processor.getBeatSequence() > seqAtSwitch);
    }

    void testCountInSilentMetronome() {
        beginTest("Silent metronome completes count-in sample-accurately");

        devpiano::audio::MetronomeProcessor processor;
        processor.prepareToPlay(48000.0);
        processor.setBpm(120.0);
        processor.setTimeSignature(devpiano::core::TimeSignature::fourFour);
        processor.setVolume(0.0f);
        processor.armCountIn(4);

        juce::AudioBuffer<float> buffer(2, 512);
        std::int64_t totalSamples = 0;
        int startOffset = -1;
        std::int64_t targetSample = -1;

        while (totalSamples < 120000) {
            buffer.clear();
            const int offset = processor.consumeCountInStartOffset(512);
            if (offset >= 0 && startOffset < 0) {
                startOffset = offset;
                targetSample = totalSamples + offset;
            }
            processor.processAndMix(&buffer, 0, 512);
            expectEquals(buffer.getMagnitude(0, 512), 0.0f);
            totalSamples += 512;
            if (startOffset >= 0) {
                break;
            }
        }

        expect(startOffset >= 0);
        expectEquals(targetSample, std::int64_t { 96000 });
    }

    void testCountInTimeSignaturePeriods() {
        beginTest("Time signature periods: 3/4 and 6/8 count-in downbeat alignment");

        // 3/4: 1 bar = 3 beats = 1.5s = 72000 samples at 48kHz
        {
            devpiano::audio::MetronomeProcessor processor;
            processor.prepareToPlay(48000.0);
            processor.setBpm(120.0);
            processor.setTimeSignature(devpiano::core::TimeSignature::threeFour);
            processor.setVolume(1.0f);
            processor.armCountIn(3);

            juce::AudioBuffer<float> buffer(2, 512);
            std::int64_t totalSamples = 0;
            int startOffset = -1;
            std::int64_t targetSample = -1;

            while (totalSamples < 100000) {
                buffer.clear();
                const int offset = processor.consumeCountInStartOffset(512);
                if (offset >= 0 && startOffset < 0) {
                    startOffset = offset;
                    targetSample = totalSamples + offset;
                }
                processor.processAndMix(&buffer, 0, 512);
                totalSamples += 512;
                if (startOffset >= 0) {
                    break;
                }
            }

            expect(startOffset >= 0);
            expectEquals(targetSample, std::int64_t { 72000 });
        }

        // 6/8: 1 bar = 6 beats (division 8) = 6 * 12000 = 72000 samples at 48kHz
        {
            devpiano::audio::MetronomeProcessor processor;
            processor.prepareToPlay(48000.0);
            processor.setBpm(120.0);
            processor.setTimeSignature(devpiano::core::TimeSignature::sixEight);
            processor.setVolume(1.0f);
            processor.armCountIn(6);

            juce::AudioBuffer<float> buffer(2, 512);
            std::int64_t totalSamples = 0;
            int startOffset = -1;
            std::int64_t targetSample = -1;

            while (totalSamples < 100000) {
                buffer.clear();
                const int offset = processor.consumeCountInStartOffset(512);
                if (offset >= 0 && startOffset < 0) {
                    startOffset = offset;
                    targetSample = totalSamples + offset;
                }
                processor.processAndMix(&buffer, 0, 512);
                totalSamples += 512;
                if (startOffset >= 0) {
                    break;
                }
            }

            expect(startOffset >= 0);
            expectEquals(targetSample, std::int64_t { 72000 });
        }
    }

    void testZeroTrigBeatPulseStability() {
        beginTest("ZeroTrig precomputed beat coefficients and pulse stability");

        devpiano::audio::MetronomeProcessor processor;
        constexpr double sr = 48000.0;
        processor.prepareToPlay(sr);
        processor.setBpm(120.0);
        processor.setTimeSignature(devpiano::core::TimeSignature::fourFour);
        processor.setVolume(1.0f);
        processor.setEnabled(true);

        juce::AudioBuffer<float> buffer(2, 512);
        buffer.clear();
        processor.processAndMix(&buffer, 0, 512);

        expect(processor.getIsDownbeat(), "first beat must be downbeat");
        const auto peak = buffer.getMagnitude(0, 512);
        expect(peak > 0.1f, "downbeat click must produce audible output");

        auto maxAmp = 0.0f;
        for (int i = 0; i < 20; ++i) {
            buffer.clear();
            processor.processAndMix(&buffer, 0, 512);
            for (int s = 0; s < 512; ++s) {
                const auto val = buffer.getSample(0, s);
                expect(!std::isnan(val) && !std::isinf(val), "metronome click must be finite");
                maxAmp = std::max(maxAmp, std::abs(val));
            }
        }
        expect(maxAmp < 2.0f, "metronome oscillator must remain strictly bounded");
    }
};

MetronomeTest metronomeTest;

} // namespace
