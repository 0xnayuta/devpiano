#include <JuceHeader.h>

#include "Recording/RecordingEngine.h"

using namespace devpiano::recording;

// =============================================================================
// Tests for RecordingEngine: recording, playback, capacity, preset changes
// =============================================================================

namespace {
/// Helper: build a RecordingTake populated with the given note-on/off events.
/// Each tuple = (timestampSamples, noteNumber, isNoteOn, channel, velocity).
RecordingTake buildTake(double sampleRate, std::int64_t lengthSamples,
                        const std::vector<std::tuple<std::int64_t, int, bool, int, float>>& events) {
    RecordingTake take;
    take.sampleRate = sampleRate;
    take.lengthSamples = lengthSamples;
    take.events.reserve(events.size());
    for (const auto& [ts, note, isOn, ch, vel] : events) {
        juce::MidiMessage msg = isOn ? juce::MidiMessage::noteOn(ch, note, vel) : juce::MidiMessage::noteOff(ch, note);
        take.events.push_back({ ts, PerformanceEventType::midi, 0, RecordingEventSource::computerKeyboard, msg });
    }
    return take;
}
} // namespace

// =============================================================================

class RecordingLifecycleTest : public juce::UnitTest {
public:
    RecordingLifecycleTest()
        : juce::UnitTest("RecordingEngine: recording lifecycle") {
    }

    void runTest() override {
        beginTest("start/stop recording produces correct take");
        {
            RecordingEngine engine;
            engine.reserveEvents(128);
            engine.startRecording(44100.0);

            expect(engine.isRecording(), "should be recording after start");
            expect(engine.getState() == RecordingState::recording, "state should be recording");

            // Record three note-on events at known timestamps.
            engine.recordEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), RecordingEventSource::computerKeyboard, 0);
            engine.recordEvent(juce::MidiMessage::noteOn(1, 64, 0.8f), RecordingEventSource::computerKeyboard, 4410);
            engine.recordEvent(juce::MidiMessage::noteOn(1, 67, 0.8f), RecordingEventSource::computerKeyboard, 8820);

            // Advance position beyond last event.
            engine.advanceRecordingPosition(10000);

            auto take = engine.stopRecording();
            expect(!engine.isRecording(), "should no longer be recording after stop");
            expect(engine.getState() == RecordingState::stopped, "state should be stopped");

            expectEquals(3, static_cast<int>(take.events.size()));
            expectEquals(44100.0, take.sampleRate);
            expect(take.lengthSamples >= 8820, "lengthSamples should cover last event");

            // Verify timestamp ordering.
            expectEquals(static_cast<std::int64_t>(0), take.events[0].timestampSamples);
            expectEquals(static_cast<std::int64_t>(4410), take.events[1].timestampSamples);
            expectEquals(static_cast<std::int64_t>(8820), take.events[2].timestampSamples);
        }

        beginTest("snapshot and current take agree");
        {
            RecordingEngine engine;
            engine.reserveEvents(64);
            engine.startRecording(48000.0);
            engine.recordEvent(juce::MidiMessage::noteOn(1, 72, 1.0f), RecordingEventSource::computerKeyboard, 100);
            engine.stopRecording();

            auto snapshot = engine.createTakeSnapshot();
            const auto& current = engine.getCurrentTake();
            expectEquals(current.events.size(), snapshot.events.size());
            expectEquals(current.sampleRate, snapshot.sampleRate);
            expectEquals(current.lengthSamples, snapshot.lengthSamples);
            // Modify snapshot, verify original unchanged (independent copy).
            snapshot.events.clear();
            expect(!current.events.empty(), "original should be unaffected by snapshot mutation");
        }

        beginTest("clear restores idle state");
        {
            RecordingEngine engine;
            engine.reserveEvents(64);
            engine.startRecording(44100.0);
            engine.recordEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), RecordingEventSource::computerKeyboard, 0);
            engine.stopRecording();
            expect(engine.hasTake(), "should have take after recording");

            engine.clear();
            expect(engine.getState() == RecordingState::idle, "state should be idle after clear");
            expect(!engine.hasTake(), "should not have take after clear");
        }

        beginTest("hasTake jasserts during recording (Debug) — verify no crash in Release path");
        {
            RecordingEngine engine;
            engine.reserveEvents(64);
            engine.startRecording(44100.0);
            // hasTake() asserts !isRecording in Debug; in Release it reads empty vector.
            // Just verify it doesn't crash.
            [[maybe_unused]] auto has = engine.hasTake();
            engine.stopRecording();
            expect(true); // survived
        }
    }
};

static RecordingLifecycleTest recordingLifecycleTest;

// =============================================================================

class PlaybackBasicTest : public juce::UnitTest {
public:
    PlaybackBasicTest()
        : juce::UnitTest("RecordingEngine: playback basic") {
    }

    void runTest() override {
        beginTest("playback renders events at correct sample offsets");
        {
            // Build a take with three events at sample positions 0, 2205, 4410.
            auto take = buildTake(44100.0, 5000,
                                  {
                                      { 0, 60, true, 1, 1.0f },
                                      { 2205, 64, true, 1, 1.0f },
                                      { 4410, 60, false, 1, 0.0f },
                                  });

            RecordingEngine engine;
            engine.startPlayback(take, 44100.0);
            expect(engine.isPlaying(), "should be playing after start");
            expectEquals(static_cast<std::int64_t>(0), engine.getPlaybackPositionSamples());

            // Render the first block [0, 512).
            juce::MidiBuffer buf;
            engine.renderPlaybackBlock(buf, 0, 512);
            int blockEvents = 0;
            for (auto m : buf) {
                juce::ignoreUnused(m);
                ++blockEvents;
            }
            expectEquals(1, blockEvents, "first block should contain the event at sample 0");

            // Render second block [512, 1024) — no events.
            buf.clear();
            engine.renderPlaybackBlock(buf, 512, 512);
            blockEvents = 0;
            for (auto m : buf) {
                juce::ignoreUnused(m);
                ++blockEvents;
            }
            expectEquals(0, blockEvents);

            // Render block containing sample 2205.
            buf.clear();
            engine.renderPlaybackBlock(buf, 2048, 256);
            blockEvents = 0;
            for (auto m : buf) {
                juce::ignoreUnused(m);
                ++blockEvents;
            }
            expectEquals(1, blockEvents, "block containing sample 2205 should have one event");

            // Render block containing sample 4410.
            buf.clear();
            engine.renderPlaybackBlock(buf, 4096, 512);
            blockEvents = 0;
            for (auto m : buf) {
                juce::ignoreUnused(m);
                ++blockEvents;
            }
            expectEquals(1, blockEvents, "block containing sample 4410 should have one event (note-off)");

            engine.stopPlayback();
            expect(!engine.isPlaying(), "should no longer be playing after stop");
        }

        beginTest("advancePlaybackPosition tracks progress and detects end");
        {
            auto take = buildTake(44100.0, 1000,
                                  {
                                      { 0, 60, true, 1, 1.0f },
                                  });

            RecordingEngine engine;
            engine.startPlayback(take, 44100.0);
            expectEquals(static_cast<std::int64_t>(0), engine.getPlaybackPositionSamples());

            engine.advancePlaybackPosition(500);
            expectEquals(static_cast<std::int64_t>(500), engine.getPlaybackPositionSamples());
            expect(engine.isPlaying());

            // Advance past the end.
            engine.advancePlaybackPosition(600);
            expect(!engine.isPlaying(), "should stop when position >= length");
            expect(engine.consumePlaybackEndedFlag(), "playback ended flag should be set");

            // Consume again → false.
            expect(!engine.consumePlaybackEndedFlag(), "flag should be cleared after consume");
        }

        beginTest("isPlaying returns false in idle state");
        {
            RecordingEngine engine;
            expect(!engine.isPlaying());
            expect(!engine.isRecording());
        }
    }
};

static PlaybackBasicTest playbackBasicTest;

// =============================================================================

class PlaybackSpeedTest : public juce::UnitTest {
public:
    PlaybackSpeedTest()
        : juce::UnitTest("RecordingEngine: playback speed") {
    }

    void runTest() override {
        beginTest("speed multiplier scales event timestamps");
        {
            // Event at sample 4410. At 1.0x → offset 4410; at 2.0x → offset 2205.
            auto take = buildTake(44100.0, 10000,
                                  {
                                      { 4410, 64, true, 1, 1.0f },
                                  });

            RecordingEngine engine;

            // 1.0x speed
            engine.startPlayback(take, 44100.0);
            expectEquals(1.0, engine.getPlaybackSpeedMultiplier());
            {
                juce::MidiBuffer buf;
                engine.renderPlaybackBlock(buf, 0, 10000);
                int count = 0;
                int sampleOff = -1;
                for (auto m : buf) {
                    ++count;
                    sampleOff = m.samplePosition;
                }
                expectEquals(1, count);
                expectEquals(4410, sampleOff, "at 1.0x event should be at original offset");
            }
            engine.stopPlayback();

            // 2.0x speed
            engine.setPlaybackSpeedMultiplier(2.0);
            engine.startPlayback(take, 44100.0);
            expectEquals(2.0, engine.getPlaybackSpeedMultiplier());
            {
                juce::MidiBuffer buf;
                engine.renderPlaybackBlock(buf, 0, 10000);
                int count = 0;
                int sampleOff = -1;
                for (auto m : buf) {
                    ++count;
                    sampleOff = m.samplePosition;
                }
                expectEquals(1, count);
                expectEquals(2205, sampleOff, "at 2.0x event should be at half the offset");
            }
            engine.stopPlayback();
        }

        beginTest("speed clamping");
        {
            RecordingEngine engine;
            engine.setPlaybackSpeedMultiplier(3.0);
            expectEquals(2.0, engine.getPlaybackSpeedMultiplier(), "should clamp to 2.0");

            engine.setPlaybackSpeedMultiplier(0.1);
            expectEquals(0.5, engine.getPlaybackSpeedMultiplier(), "should clamp to 0.5");
        }
    }
};

static PlaybackSpeedTest playbackSpeedTest;

// =============================================================================

class PlaybackEndDetectionTest : public juce::UnitTest {
public:
    PlaybackEndDetectionTest()
        : juce::UnitTest("RecordingEngine: playback end detection") {
    }

    void runTest() override {
        beginTest("exact boundary advance triggers end");
        {
            auto take = buildTake(44100.0, 1024,
                                  {
                                      { 0, 60, true, 1, 1.0f },
                                  });

            RecordingEngine engine;
            engine.startPlayback(take, 44100.0);
            // Advance exactly to the scaled length.
            engine.advancePlaybackPosition(1024);
            expect(!engine.isPlaying(), "should end when position reaches length");
            expect(engine.consumePlaybackEndedFlag());

            // Position should be clamped to length, not exceed.
            expectEquals(static_cast<std::int64_t>(1024), engine.getPlaybackPositionSamples());
        }

        beginTest("zero-length take ends immediately");
        {
            RecordingTake empty;
            empty.sampleRate = 44100.0;
            empty.lengthSamples = 0;

            RecordingEngine engine;
            engine.startPlayback(empty, 44100.0);
            expect(engine.isPlaying());

            engine.advancePlaybackPosition(1);
            expect(!engine.isPlaying());
            expect(engine.consumePlaybackEndedFlag());
        }
    }
};

static PlaybackEndDetectionTest playbackEndDetectionTest;

// =============================================================================

class RecordingCapacityTest : public juce::UnitTest {
public:
    RecordingCapacityTest()
        : juce::UnitTest("RecordingEngine: capacity / dropped events") {
    }

    void runTest() override {
        beginTest("events dropped when capacity exhausted");
        {
            RecordingEngine engine;
            // Reserve tiny capacity.
            engine.reserveEvents(2);
            engine.startRecording(44100.0);

            engine.recordEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), RecordingEventSource::computerKeyboard, 0);
            engine.recordEvent(juce::MidiMessage::noteOn(1, 62, 1.0f), RecordingEventSource::computerKeyboard, 100);
            expect(!engine.hasDroppedEvents(), "should not have dropped within capacity");

            // Third event exceeds capacity.
            engine.recordEvent(juce::MidiMessage::noteOn(1, 64, 1.0f), RecordingEventSource::computerKeyboard, 200);
            expect(engine.hasDroppedEvents(), "should have dropped events beyond capacity");
            expectEquals(static_cast<std::size_t>(1), engine.getDroppedEventCount());

            auto take = engine.stopRecording();
            expectEquals(2, static_cast<int>(take.events.size()), "only 2 events should be retained");
        }

        beginTest("no drops with sufficient capacity");
        {
            RecordingEngine engine;
            engine.reserveEvents(100);
            engine.startRecording(48000.0);

            for (int i = 0; i < 50; ++i)
                engine.recordEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), RecordingEventSource::computerKeyboard,
                                   static_cast<std::int64_t>(i * 100));

            expect(!engine.hasDroppedEvents());
            auto take = engine.stopRecording();
            expectEquals(50, static_cast<int>(take.events.size()));
        }

        beginTest("reserved capacity is queryable");
        {
            RecordingEngine engine;
            engine.reserveEvents(96000);
            expectEquals(static_cast<std::size_t>(96000), engine.getReservedEventCapacity());
        }
    }
};

static RecordingCapacityTest recordingCapacityTest;

// =============================================================================

class PresetChangeRecordingTest : public juce::UnitTest {
public:
    PresetChangeRecordingTest()
        : juce::UnitTest("RecordingEngine: preset change recording") {
    }

    void runTest() override {
        beginTest("preset change events merged into final take");
        {
            RecordingEngine engine;
            engine.reserveEvents(64);
            engine.startRecording(44100.0);

            engine.recordEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), RecordingEventSource::computerKeyboard, 0);
            engine.recordPresetChange(3, 2205);
            engine.recordEvent(juce::MidiMessage::noteOn(1, 64, 1.0f), RecordingEventSource::computerKeyboard, 4410);

            auto take = engine.stopRecording();
            // 2 note events + 1 preset change → 3 total.
            expectEquals(3, static_cast<int>(take.events.size()));

            int presetCount = 0;
            for (const auto& ev : take.events)
                if (ev.type == PerformanceEventType::presetChange)
                    ++presetCount;
            expectEquals(1, presetCount);
            // Verify the preset event data.
            const auto* presetEv = [&]() -> const PerformanceEvent* {
                for (const auto& ev : take.events)
                    if (ev.type == PerformanceEventType::presetChange)
                        return &ev;
                return nullptr;
            }();
            expect(presetEv != nullptr);
            expectEquals(static_cast<uint8_t>(3), presetEv->presetId);
            expectEquals(static_cast<std::int64_t>(2205), presetEv->timestampSamples);
        }

        beginTest("preset change ignored when not recording");
        {
            RecordingEngine engine;
            engine.reserveEvents(64);
            engine.recordPresetChange(1, 0);
            expect(!engine.isRecording());
            expect(!engine.hasTake());
        }
    }
};

static PresetChangeRecordingTest presetChangeRecordingTest;

// =============================================================================

class PresetChangePlaybackTest : public juce::UnitTest {
public:
    PresetChangePlaybackTest()
        : juce::UnitTest("RecordingEngine: preset change playback") {
    }

    void runTest() override {
        beginTest("preset changes drained during playback");
        {
            RecordingTake take;
            take.sampleRate = 44100.0;
            take.lengthSamples = 5000;
            // Insert a preset-change event manually.
            {
                PerformanceEvent ev;
                ev.timestampSamples = 1000;
                ev.type = PerformanceEventType::presetChange;
                ev.presetId = 7;
                take.events.push_back(ev);
            }
            {
                PerformanceEvent ev;
                ev.timestampSamples = 3000;
                ev.type = PerformanceEventType::presetChange;
                ev.presetId = 2;
                take.events.push_back(ev);
            }

            RecordingEngine engine;
            engine.startPlayback(take, 44100.0);

            juce::MidiBuffer buf;
            engine.renderPlaybackBlock(buf, 0, 5000); // covers both preset events

            auto drained = engine.drainPendingPresetChanges();
            expectEquals(static_cast<int>(drained.size()), 2);
            expectEquals(drained[0].presetId, static_cast<uint8_t>(7));
            expectEquals(drained[1].presetId, static_cast<uint8_t>(2));

            // Drain again → empty.
            auto drained2 = engine.drainPendingPresetChanges();
            expect(drained2.empty());

            engine.stopPlayback();
        }

        beginTest("no preset changes when none recorded");
        {
            auto take = buildTake(44100.0, 1000,
                                  {
                                      { 0, 60, true, 1, 1.0f },
                                  });

            RecordingEngine engine;
            engine.startPlayback(take, 44100.0);

            juce::MidiBuffer buf;
            engine.renderPlaybackBlock(buf, 0, 1000);
            auto drained = engine.drainPendingPresetChanges();
            expect(drained.empty());

            engine.stopPlayback();
        }
    }
};

static PresetChangePlaybackTest presetChangePlaybackTest;

// =============================================================================

class PitchBendSmoothingTest : public juce::UnitTest {
public:
    PitchBendSmoothingTest()
        : juce::UnitTest("RecordingEngine: pitch bend smoothing") {
    }

    void runTest() override {
        beginTest("pitch wheel events are EMA-smoothed during playback");
        {
            RecordingTake take;
            take.sampleRate = 44100.0;
            take.lengthSamples = 500;
            // Raw pitch wheel at extreme values.
            {
                PerformanceEvent ev;
                ev.timestampSamples = 0;
                ev.type = PerformanceEventType::midi;
                ev.message = juce::MidiMessage::pitchWheel(1, 16383); // max pitch bend
                take.events.push_back(ev);
            }

            RecordingEngine engine;
            engine.startPlayback(take, 44100.0);

            juce::MidiBuffer buf;
            engine.renderPlaybackBlock(buf, 0, 512);

            // The smoothed value should be between center (8192) and target (16383),
            // because EMA factor 0.3 is applied once.
            bool foundPitchWheel = false;
            for (auto m : buf) {
                if (m.getMessage().isPitchWheel()) {
                    foundPitchWheel = true;
                    auto val = m.getMessage().getPitchWheelValue();
                    // EMA: 8192 + 0.3 * (16383 - 8192) = 8192 + 2457.3 = 10649.3
                    // Allow some tolerance for float rounding.
                    expect(val > 8192 && val < 16383, "smoothed value should be between center and target");
                }
            }
            expect(foundPitchWheel, "playback should emit a pitch wheel event");

            engine.stopPlayback();
        }
    }
};

static PitchBendSmoothingTest pitchBendSmoothingTest;

// =============================================================================

class TakeHelpersTest : public juce::UnitTest {
public:
    TakeHelpersTest()
        : juce::UnitTest("RecordingEngine: RecordingTake helpers") {
    }

    void runTest() override {
        beginTest("isEmpty on empty take");
        {
            RecordingTake take;
            expect(take.isEmpty());
            expectEquals(0.0, take.durationSeconds());
        }

        beginTest("isEmpty on populated take");
        {
            auto take = buildTake(44100.0, 44100,
                                  {
                                      { 0, 60, true, 1, 1.0f },
                                  });
            expect(!take.isEmpty());
        }

        beginTest("durationSeconds computes correctly");
        {
            auto take = buildTake(44100.0, 44100, {});
            expectEquals(take.durationSeconds(), 1.0);

            RecordingTake zeroRate;
            zeroRate.sampleRate = 0.0;
            zeroRate.lengthSamples = 44100;
            expectEquals(zeroRate.durationSeconds(), 0.0);

            RecordingTake zeroLen;
            zeroLen.sampleRate = 44100.0;
            zeroLen.lengthSamples = 0;
            expectEquals(zeroLen.durationSeconds(), 0.0);
        }

        beginTest("durationSeconds at 48kHz");
        {
            auto take = buildTake(48000.0, 96000, {});
            expectEquals(take.durationSeconds(), 2.0);
        }
    }
};

static TakeHelpersTest takeHelpersTest;

// =============================================================================

class MidiBufferBlockRecordingTest : public juce::UnitTest {
public:
    MidiBufferBlockRecordingTest()
        : juce::UnitTest("RecordingEngine: MidiBuffer block recording") {
    }

    void runTest() override {
        beginTest("recordMidiBufferBlock captures MIDI with absolute timestamps");
        {
            RecordingEngine engine;
            engine.reserveEvents(64);
            engine.startRecording(44100.0);

            // Create a MidiBuffer with two events at block-relative offsets.
            juce::MidiBuffer buf;
            buf.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
            buf.addEvent(juce::MidiMessage::noteOn(1, 64, 0.8f), 256);

            // Block starts at sample 1024.
            engine.recordMidiBufferBlock(buf, RecordingEventSource::realtimeMidiBuffer, 1024);

            auto take = engine.stopRecording();
            expectEquals(2, static_cast<int>(take.events.size()));

            // First event: 1024 + 0 = 1024.
            expectEquals(static_cast<std::int64_t>(1024), take.events[0].timestampSamples);
            // Second event: 1024 + 256 = 1280.
            expectEquals(static_cast<std::int64_t>(1280), take.events[1].timestampSamples);
            expect(take.events[0].source == RecordingEventSource::realtimeMidiBuffer,
                   "source should be realtimeMidiBuffer");
        }

        beginTest("recordMidiBufferBlock ignores when not recording");
        {
            RecordingEngine engine;
            engine.reserveEvents(64);

            juce::MidiBuffer buf;
            buf.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
            engine.recordMidiBufferBlock(buf, RecordingEventSource::realtimeMidiBuffer, 0);

            expect(!engine.isRecording());
            // No take because we never started recording.
        }
    }
};

static MidiBufferBlockRecordingTest midiBufferBlockRecordingTest;
