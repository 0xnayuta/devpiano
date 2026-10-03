#include <JuceHeader.h>

#include "Recording/RecordingEngine.h"
#include "Recording/RecordingFlowSupport.h"

using namespace devpiano::recording;

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

/// Helper: 统计 MidiBuffer 中的 MIDI 消息条数。
int countMidiBufferEvents(const juce::MidiBuffer& buf) {
    int n = 0;
    for (auto m : buf) {
        juce::ignoreUnused(m);
        ++n;
    }
    return n;
}
} // namespace

// =============================================================================

class RecordingLifecycleTest : public juce::UnitTest {
public:
    RecordingLifecycleTest()
        : juce::UnitTest("RecordingEngine: recording lifecycle", "DevPiano/Engine") {
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

        beginTest("during-recording queries use safe paths; hasTake valid after stop (TEST-018)");
        {
            RecordingEngine engine;
            engine.reserveEvents(64);
            engine.startRecording(44100.0);
            engine.recordEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), RecordingEventSource::computerKeyboard, 0);

            // 录制中不调用 hasTake()（jassert 契约违反——事件向量正被音频线程
            // 修改）。正确的录制中检查是 RecordingSessionController 的 local-take
            // 副本（recordingSession.hasTake()）；引擎侧的安全查询路径如下：
            expect(engine.getCurrentTake().isEmpty(),
                   "getCurrentTake must return an empty take during recording (safe query path)");
            expect(engine.getReservedEventCapacity() >= 64, "capacity query must stay readable during recording");
            engine.stopRecording();
            expect(engine.hasTake(), "hasTake must be valid after stopRecording");
        }
    }
};

static RecordingLifecycleTest recordingLifecycleTest;

// =============================================================================

class PlaybackBasicTest : public juce::UnitTest {
public:
    PlaybackBasicTest()
        : juce::UnitTest("RecordingEngine: playback basic", "DevPiano/Engine") {
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
            int blockEvents = countMidiBufferEvents(buf);
            expectEquals(1, blockEvents, "first block should contain the event at sample 0");

            // Render second block [512, 1024) — no events.
            buf.clear();
            engine.renderPlaybackBlock(buf, 512, 512);
            blockEvents = countMidiBufferEvents(buf);
            expectEquals(0, blockEvents);

            // Render block containing sample 2205.
            buf.clear();
            engine.renderPlaybackBlock(buf, 2048, 256);
            blockEvents = countMidiBufferEvents(buf);
            expectEquals(1, blockEvents, "block containing sample 2205 should have one event");

            // Render block containing sample 4410.
            buf.clear();
            engine.renderPlaybackBlock(buf, 4096, 512);
            blockEvents = countMidiBufferEvents(buf);
            expectEquals(1, blockEvents, "block containing sample 4410 should have one event (note-off)");

            engine.stopPlaybackQuiescent();
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

        beginTest("rejected numeric timelines leave the playing take intact");
        {
            const auto original = buildTake(48000.0, 48000, { { 480, 65, true, 1, 1.0f } });
            for (const auto invalidRate :
                 { 1e-300, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() }) {
                RecordingEngine engine;
                engine.startPlayback(original, 44100.0);
                auto invalid = original;
                invalid.sampleRate = invalidRate;
                engine.startPlayback(invalid, 44100.0);
                expect(engine.isPlaying());
                expectEquals(engine.getPlaybackTakeLengthSamples(), std::int64_t { 48000 });
                juce::MidiBuffer buffer;
                engine.renderPlaybackBlock(buffer, 0, 512);
                expectEquals(buffer.getNumEvents(), 1);
                for (const auto event : buffer) {
                    expectEquals(event.samplePosition, 441);
                    expectEquals(event.getMessage().getNoteNumber(), 65);
                }
            }
            RecordingEngine engine;
            auto oversized = original;
            oversized.lengthSamples = std::numeric_limits<std::int64_t>::max();
            engine.startPlayback(oversized, 44100.0);
            expect(!engine.isPlaying());
            engine.startPlayback(original, 1e-300);
            expect(!engine.isPlaying());
        }

        beginTest("valid sample-rate conversion and speed remain deterministic after seek");
        {
            const auto take = buildTake(48000.0, 48000, { { 24000, 72, true, 1, 1.0f } });
            RecordingEngine engine;
            engine.setPlaybackSpeedMultiplier(2.0);
            engine.startPlaybackAtTakeSample(take, 44100.0, 24000);
            expectEquals(engine.getPlaybackPositionSamples(), std::int64_t { 11025 });
            engine.setPlaybackSpeedMultiplier(std::numeric_limits<double>::quiet_NaN());
            expectEquals(engine.getPlaybackSpeedMultiplier(), 2.0);
            juce::MidiBuffer buffer;
            engine.renderPlaybackBlock(buffer, 11025, 128);
            expectEquals(buffer.getNumEvents(), 1);
            for (const auto event : buffer) {
                expectEquals(event.samplePosition, 0);
                expectEquals(event.getMessage().getNoteNumber(), 72);
            }
            engine.advancePlaybackPosition(11025);
            expect(engine.consumePlaybackEndedFlag());
            expectEquals(engine.getPlaybackPositionSamples(), std::int64_t { 22050 });
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
        : juce::UnitTest("RecordingEngine: playback speed", "DevPiano/Engine") {
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
            engine.stopPlaybackQuiescent();

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
            engine.stopPlaybackQuiescent();
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
        : juce::UnitTest("RecordingEngine: playback end detection", "DevPiano/Engine") {
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
        : juce::UnitTest("RecordingEngine: capacity / dropped events", "DevPiano/Engine") {
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
            expectEquals(static_cast<int>(engine.getDroppedEventCount()), 0, "should not have dropped within capacity");

            // Third event exceeds capacity.
            engine.recordEvent(juce::MidiMessage::noteOn(1, 64, 1.0f), RecordingEventSource::computerKeyboard, 200);
            expect(engine.getDroppedEventCount() > 0, "should have dropped events beyond capacity");
            expectEquals(static_cast<std::size_t>(1), engine.getDroppedEventCount());

            auto take = engine.stopRecording();
            expectEquals(2, static_cast<int>(take.events.size()), "only 2 events should be retained");
        }

        beginTest("no drops with sufficient capacity");
        {
            RecordingEngine engine;
            engine.reserveEvents(100);
            engine.startRecording(48000.0);

            for (int i = 0; i < 50; ++i) {
                engine.recordEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), RecordingEventSource::computerKeyboard,
                                   static_cast<std::int64_t>(i) * 100);
            }

            expectEquals(static_cast<int>(engine.getDroppedEventCount()), 0);
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
        : juce::UnitTest("RecordingEngine: preset change recording", "DevPiano/Engine") {
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
            for (const auto& ev : take.events) {
                if (ev.type == PerformanceEventType::presetChange) {
                    ++presetCount;
                }
            }
            expectEquals(1, presetCount);
            // Verify the preset event data.
            const auto* presetEv = [&]() -> const PerformanceEvent* {
                for (const auto& ev : take.events) {
                    if (ev.type == PerformanceEventType::presetChange) {
                        return &ev;
                    }
                }
                return nullptr;
            }();
            expect(presetEv != nullptr);
            // JUCE expect() 失败时仅记录不中断，继续解引用会在断言失败时崩
            // 溃——用 if 守卫满足 clang-analyzer 的 null 检查。
            if (presetEv != nullptr) {
                expectEquals(static_cast<uint8_t>(3), presetEv->presetId);
                expectEquals(static_cast<std::int64_t>(2205), presetEv->timestampSamples);
            }

            // Verify timestamp monotonic ordering (SEC-002)
            for (size_t i = 1; i < take.events.size(); ++i) {
                expect(take.events[i].timestampSamples >= take.events[i - 1].timestampSamples,
                       "Events in take must be monotonically ordered by timestamp");
            }
            expectEquals(static_cast<std::int64_t>(0), take.events[0].timestampSamples);
            expectEquals(static_cast<std::int64_t>(2205), take.events[1].timestampSamples);
            expectEquals(static_cast<std::int64_t>(4410), take.events[2].timestampSamples);
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
        : juce::UnitTest("RecordingEngine: preset change playback", "DevPiano/Engine") {
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

            engine.stopPlaybackQuiescent();
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

            engine.stopPlaybackQuiescent();
        }
    }
};

static PresetChangePlaybackTest presetChangePlaybackTest;

// =============================================================================

class PitchBendSmoothingTest : public juce::UnitTest {
public:
    PitchBendSmoothingTest()
        : juce::UnitTest("RecordingEngine: pitch bend smoothing", "DevPiano/Engine") {
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

            engine.stopPlaybackQuiescent();
        }
    }
};

static PitchBendSmoothingTest pitchBendSmoothingTest;

// =============================================================================

class TakeHelpersTest : public juce::UnitTest {
public:
    TakeHelpersTest()
        : juce::UnitTest("RecordingEngine: RecordingTake helpers", "DevPiano/Engine") {
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
        : juce::UnitTest("RecordingEngine: MidiBuffer block recording", "DevPiano/Engine") {
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

// =============================================================================

class PlaybackPauseResumeTest : public juce::UnitTest {
public:
    PlaybackPauseResumeTest()
        : juce::UnitTest("RecordingEngine: playback pause/resume", "DevPiano/Engine") {
    }

    void runTest() override {
        beginTest("startPlayback with resume position continues from there");
        {
            auto take = buildTake(44100.0, 10000, { { 5000, 60, true, 1, 1.0f } });
            RecordingEngine engine;

            engine.startPlayback(take, 44100.0, 5000);
            expectEquals(static_cast<std::int64_t>(5000), engine.getPlaybackPositionSamples(),
                         "resume position should be honoured");
            expect(engine.isPlaying());

            // Event at 5000 belongs to the resumed block starting at 5000.
            juce::MidiBuffer buf;
            engine.renderPlaybackBlock(buf, 5000, 500);
            int count = 0;
            for (auto m : buf) {
                ++count;
                expectEquals(0, m.samplePosition, "event should land at block start");
            }
            expectEquals(1, count, "resumed playback should render the event at the resume point");
        }

        beginTest("resume position beyond end is clamped to the end");
        {
            auto take = buildTake(44100.0, 1000, {});
            RecordingEngine engine;
            engine.startPlayback(take, 44100.0, 999999);
            expectEquals(static_cast<std::int64_t>(1000), engine.getPlaybackPositionSamples(),
                         "resume past the end should clamp to the scaled length");
        }

        beginTest("pausePlayback freezes position and resume continues");
        {
            auto take = buildTake(44100.0, 20000, { { 4410, 60, true, 1, 1.0f } });
            RecordingEngine engine;

            engine.startPlayback(take, 44100.0);
            engine.advancePlaybackPosition(4410);
            expect(engine.isPlaying());

            engine.pausePlayback();
            expect(!engine.isPlaying(), "paused playback is not playing");
            expect(engine.getState() == RecordingState::playingPaused, "state should be playingPaused");
            expectEquals(static_cast<std::int64_t>(4410), engine.getPlaybackPositionSamples(),
                         "pause must retain the position");

            // Paused: no rendering, no advancement.
            juce::MidiBuffer buf;
            engine.renderPlaybackBlock(buf, 4410, 1000);
            int count = 0;
            for ([[maybe_unused]] auto m : buf) {
                ++count;
            }
            expectEquals(0, count, "no events rendered while paused");
            engine.advancePlaybackPosition(500);
            expectEquals(static_cast<std::int64_t>(4410), engine.getPlaybackPositionSamples(),
                         "position must not advance while paused");

            // Resume from the retained position.
            engine.startPlayback(take, 44100.0, engine.getPlaybackPositionSamples());
            expect(engine.isPlaying());
            engine.advancePlaybackPosition(500);
            expectEquals(static_cast<std::int64_t>(4910), engine.getPlaybackPositionSamples(),
                         "resume should continue from the pause point");
        }

        beginTest("non-integral sample-rate playback cursor survives pause and resume");
        {
            auto take = buildTake(44100.0, 44100, {});
            RecordingEngine engine;
            engine.startPlaybackAtTakeSample(take, 48000.0, 1);
            const auto scaledCursor = engine.getPlaybackPositionSamples();
            expectEquals(static_cast<std::int64_t>(1), scaledCursor);

            for (int restart = 0; restart < 4; ++restart) {
                engine.pausePlayback();
                engine.startPlayback(take, 48000.0, scaledCursor);
                expectEquals(scaledCursor, engine.getPlaybackPositionSamples());
            }
        }

        beginTest("pausePlayback outside playing state is a no-op");
        {
            RecordingEngine engine;
            engine.pausePlayback();
            expect(engine.getState() == RecordingState::idle, "pause in idle must not change state");
        }
    }
};

static PlaybackPauseResumeTest playbackPauseResumeTest;

// =============================================================================

class RecordingPauseTest : public juce::UnitTest {
public:
    RecordingPauseTest()
        : juce::UnitTest("RecordingEngine: recording pause/resume", "DevPiano/Engine") {
    }

    void runTest() override {
        beginTest("pauseRecording freezes capture and position");
        {
            RecordingEngine engine;
            engine.reserveEvents(128);
            engine.startRecording(44100.0);
            engine.advanceRecordingPosition(4410);

            engine.pauseRecording();
            expect(!engine.isRecording(), "paused recording is not recording");
            expect(engine.getState() == RecordingState::recordingPaused, "state should be recordingPaused");

            // Paused: events ignored, position frozen.
            engine.recordEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), RecordingEventSource::computerKeyboard, 99999);
            engine.advanceRecordingPosition(500);
            expectEquals(static_cast<std::int64_t>(4410), engine.getCurrentPositionSamples(),
                         "position must not advance while paused");

            engine.resumeRecording();
            expect(engine.isRecording(), "resumed recording is recording again");
            expect(engine.getState() == RecordingState::recording, "state should be recording after resume");

            engine.advanceRecordingPosition(500);
            expectEquals(static_cast<std::int64_t>(4910), engine.getCurrentPositionSamples(),
                         "position should continue after resume");

            auto take = engine.stopRecording();
            expectEquals(static_cast<std::int64_t>(4910), take.lengthSamples, "length covers the resumed position");
        }

        beginTest("stopRecording from paused state finalises take");
        {
            RecordingEngine engine;
            engine.reserveEvents(128);
            engine.startRecording(44100.0);
            engine.recordEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), RecordingEventSource::computerKeyboard, 0);
            engine.advanceRecordingPosition(4410);

            engine.pauseRecording();
            auto take = engine.stopRecording();

            expect(engine.getState() == RecordingState::stopped, "state should be stopped after stopRecording");
            expectEquals(1, static_cast<int>(take.events.size()), "events recorded before pausing must be kept");
            expectEquals(static_cast<std::int64_t>(4410), take.lengthSamples,
                         "length must be finalised even when stopping from paused");
        }
    }
};

static RecordingPauseTest recordingPauseTest;
class AbLoopTest final : public juce::UnitTest {
public:
    AbLoopTest()
        : juce::UnitTest("RecordingEngine: A-B loop and seek", "DevPiano/Engine") {
    }

    void runTest() override {
        beginTest("seek uses take samples, clamps boundaries, and emits all-channel cleanup");
        {
            auto take = buildTake(1000.0, 200, { { 0, 60, true, 1, 1.0f } });
            RecordingEngine engine;
            engine.setPlaybackSpeedMultiplier(1.0);
            engine.startPlaybackAtTakeSample(take, 2000.0, 40);
            expectEquals(static_cast<std::int64_t>(80), engine.getPlaybackPositionSamples());
            expectEquals(static_cast<std::int64_t>(40), engine.getPlaybackPositionInTakeSamples());

            engine.requestPlaybackSeek(75);
            std::int64_t pendingSample = 0;
            expect(engine.getPendingPlaybackSeekSample(pendingSample));
            expectEquals(static_cast<std::int64_t>(75), pendingSample);

            juce::MidiBuffer cleanup;
            expect(engine.applyPendingTransportCommands(cleanup).seekApplied);
            expectEquals(static_cast<std::int64_t>(150), engine.getPlaybackPositionSamples());
            expectEquals(48, countMidiBufferEvents(cleanup));

            engine.requestPlaybackSeek(-1);
            cleanup.clear();
            expect(engine.applyPendingTransportCommands(cleanup).seekApplied);
            expectEquals(static_cast<std::int64_t>(0), engine.getPlaybackPositionSamples());

            engine.requestPlaybackSeek(500);
            cleanup.clear();
            expect(engine.applyPendingTransportCommands(cleanup).seekApplied);
            expectEquals(static_cast<std::int64_t>(400), engine.getPlaybackPositionSamples());
            expectEquals(static_cast<std::int64_t>(200), engine.getPlaybackPositionInTakeSamples());
        }

        beginTest("seek resumes a merged multichannel timeline at the exact take sample");
        {
            auto take = buildTake(1000.0, 100,
                                  {
                                      { 10, 60, true, 1, 1.0f },
                                      { 20, 67, true, 2, 1.0f },
                                      { 30, 60, false, 1, 0.0f },
                                      { 40, 72, true, 3, 1.0f },
                                  });
            RecordingEngine engine;
            engine.startPlaybackAtTakeSample(take, 2000.0, 20);

            juce::MidiBuffer buffer;
            engine.renderPlaybackBlock(buffer, 40, 41);
            expectEquals(3, countMidiBufferEvents(buffer));

            int eventIndex = 0;
            for (const auto metadata : buffer) {
                const auto message = metadata.getMessage();
                if (eventIndex == 0) {
                    expectEquals(0, metadata.samplePosition);
                    expect(message.isNoteOn());
                    expectEquals(2, message.getChannel());
                    expectEquals(67, message.getNoteNumber());
                } else if (eventIndex == 1) {
                    expectEquals(20, metadata.samplePosition);
                    expect(message.isNoteOff());
                    expectEquals(1, message.getChannel());
                    expectEquals(60, message.getNoteNumber());
                } else {
                    expectEquals(40, metadata.samplePosition);
                    expect(message.isNoteOn());
                    expectEquals(3, message.getChannel());
                    expectEquals(72, message.getNoteNumber());
                }
                ++eventIndex;
            }
        }

        beginTest("rendering after a seek beyond B advances from normalized A");
        {
            auto take = buildTake(1000.0, 100, { { 10, 60, true, 1, 1.0f } });
            RecordingEngine engine;
            engine.setPlaybackLoopStartSample(10);
            engine.setPlaybackLoopEndSample(20);
            engine.startPlaybackAtTakeSample(take, 1000.0, 0);

            engine.requestPlaybackSeek(25);
            juce::MidiBuffer seekCleanup;
            expect(engine.applyPendingTransportCommands(seekCleanup).seekApplied);

            juce::MidiBuffer buffer;
            engine.renderPlaybackBlock(buffer, 25, 4);
            bool playedLoopStartEvent = false;
            for (const auto metadata : buffer) {
                const auto message = metadata.getMessage();
                playedLoopStartEvent
                    |= message.isNoteOn() && message.getChannel() == 1 && message.getNoteNumber() == 60;
            }
            expect(playedLoopStartEvent);
            engine.advancePlaybackPosition(4);

            expectEquals(static_cast<std::int64_t>(14), engine.getPlaybackPositionSamples());
            expect(engine.isPlaying());
        }
        beginTest("loop wraps at B, cleans all channels before replaying multichannel A events");
        {
            auto take = buildTake(1000.0, 100,
                                  {
                                      { 10, 60, true, 1, 1.0f },
                                      { 10, 36, true, 10, 1.0f },
                                      { 40, 67, true, 2, 1.0f },
                                  });
            RecordingEngine engine;
            engine.setPlaybackLoopStartSample(10);
            engine.setPlaybackLoopEndSample(40);
            engine.startPlaybackAtTakeSample(take, 1000.0, 20);

            juce::MidiBuffer buffer;
            engine.renderPlaybackBlock(buffer, 20, 25);
            int cleanupAtWrap = 0;
            int replayedNotesAtWrap = 0;
            bool endMarkerEventWasPlayed = false;
            bool firstWrapEventWasCleanup = false;
            bool sawWrapOffset = false;
            for (const auto metadata : buffer) {
                const auto message = metadata.getMessage();
                if (metadata.samplePosition != 20) {
                    continue;
                }

                if (!sawWrapOffset) {
                    firstWrapEventWasCleanup = message.isController() && message.getControllerNumber() == 64
                        && message.getControllerValue() == 0;
                    sawWrapOffset = true;
                }
                if (message.isNoteOn()) {
                    expectEquals(48, cleanupAtWrap, "panic must precede A events at the same sample");
                    ++replayedNotesAtWrap;
                    endMarkerEventWasPlayed |= message.getChannel() == 2 && message.getNoteNumber() == 67;
                } else if ((message.isController()
                            && (message.getControllerNumber() == 64 || message.getControllerNumber() == 120))
                           || message.isAllNotesOff()) {
                    ++cleanupAtWrap;
                }
            }

            expect(sawWrapOffset, "B boundary should occur inside this block");
            expect(firstWrapEventWasCleanup, "the first event at B must release sustain");
            expectEquals(48, cleanupAtWrap);
            expectEquals(2, replayedNotesAtWrap, "both playback channels should restart at A");
            expect(!endMarkerEventWasPlayed, "events at B are outside the half-open loop interval");

            engine.advancePlaybackPosition(25);
            expect(engine.isPlaying(), "an active loop must suppress normal completion");
            expect(!engine.consumePlaybackEndedFlag());
            expectEquals(static_cast<std::int64_t>(15), engine.getPlaybackPositionSamples());
        }

        beginTest("loop markers remain take-relative at faster playback speed");
        {
            auto take = buildTake(1000.0, 100,
                                  {
                                      { 20, 60, true, 1, 1.0f },
                                      { 50, 67, true, 2, 1.0f },
                                  });
            RecordingEngine engine;
            engine.setPlaybackLoopStartSample(20);
            engine.setPlaybackLoopEndSample(50);
            engine.setPlaybackSpeedMultiplier(2.0);
            engine.startPlaybackAtTakeSample(take, 1000.0, 30);

            juce::MidiBuffer buffer;
            engine.renderPlaybackBlock(buffer, 15, 12);
            int cleanupAtWrap = 0;
            int replayedNotesAtWrap = 0;
            for (const auto metadata : buffer) {
                if (metadata.samplePosition != 10) {
                    continue;
                }
                const auto message = metadata.getMessage();
                if (message.isNoteOn() && message.getChannel() == 1 && message.getNoteNumber() == 60) {
                    ++replayedNotesAtWrap;
                } else if ((message.isController()
                            && (message.getControllerNumber() == 64 || message.getControllerNumber() == 120))
                           || message.isAllNotesOff()) {
                    ++cleanupAtWrap;
                }
            }
            expectEquals(48, cleanupAtWrap);
            expectEquals(1, replayedNotesAtWrap);

            engine.advancePlaybackPosition(12);
            expectEquals(static_cast<std::int64_t>(12), engine.getPlaybackPositionSamples());
        }

        beginTest("speed changes preserve take position while playback is paused or stopped");
        {
            auto take = buildTake(1000.0, 500, { { 0, 60, true, 1, 1.0f } });
            RecordingEngine engine;
            engine.startPlaybackAtTakeSample(take, 1000.0, 100);
            engine.advancePlaybackPosition(40);
            expectEquals(static_cast<std::int64_t>(140), engine.getPlaybackPositionInTakeSamples());

            engine.pausePlayback();
            engine.setPlaybackSpeedMultiplier(0.5);
            engine.applyPendingTransportCommandsQuiescent();
            expectEquals(static_cast<std::int64_t>(140), engine.getPlaybackPositionInTakeSamples());
            expectEquals(static_cast<std::int64_t>(280), engine.getPlaybackPositionSamples());

            engine.stopPlaybackQuiescent();
            engine.setPlaybackSpeedMultiplier(2.0);
            engine.applyPendingTransportCommandsQuiescent();
            expectEquals(static_cast<std::int64_t>(140), engine.getPlaybackPositionInTakeSamples());
            expectEquals(static_cast<std::int64_t>(70), engine.getPlaybackPositionSamples());
        }

        beginTest("B at block end cleans up at offset zero of the next block");
        {
            auto take = buildTake(1000.0, 100, { { 10, 60, true, 1, 1.0f } });
            RecordingEngine engine;
            engine.setPlaybackLoopStartSample(10);
            engine.setPlaybackLoopEndSample(30);
            engine.startPlaybackAtTakeSample(take, 1000.0, 10);

            juce::MidiBuffer buffer;
            engine.renderPlaybackBlock(buffer, 10, 20);
            expectEquals(1, countMidiBufferEvents(buffer),
                         "the first block contains only the note at A, with no early panic");
            engine.advancePlaybackPosition(20);
            expectEquals(static_cast<std::int64_t>(10), engine.getPlaybackPositionSamples());

            buffer.clear();
            engine.renderPlaybackBlock(buffer, 10, 8);
            int cleanupAtStart = 0;
            int firstOffsetEvent = -1;
            for (const auto metadata : buffer) {
                if (metadata.samplePosition != 0) {
                    continue;
                }
                const auto message = metadata.getMessage();
                if (firstOffsetEvent < 0) {
                    firstOffsetEvent = message.isController() && message.getControllerNumber() == 64 ? 1 : 0;
                }
                if ((message.isController()
                     && (message.getControllerNumber() == 64 || message.getControllerNumber() == 120))
                    || message.isAllNotesOff()) {
                    ++cleanupAtStart;
                }
            }
            expectEquals(1, firstOffsetEvent, "cleanup must be first at the loop-start sample");
            expectEquals(48, cleanupAtStart);
        }

        beginTest("reversed and empty loop ranges do not alter playback");
        {
            auto take = buildTake(1000.0, 100, { { 0, 60, true, 1, 1.0f } });
            RecordingEngine engine;
            engine.setPlaybackLoopStartSample(20);
            engine.setPlaybackLoopEndSample(10);
            expect(!engine.getPlaybackLoopRange().isValid());
            engine.startPlayback(take, 1000.0);

            juce::MidiBuffer buffer;
            engine.renderPlaybackBlock(buffer, 0, 25);
            expectEquals(1, countMidiBufferEvents(buffer));
            engine.advancePlaybackPosition(25);
            expectEquals(static_cast<std::int64_t>(25), engine.getPlaybackPositionSamples());
            expect(!engine.consumePlaybackEndedFlag());

            engine.clearPlaybackLoop();
            engine.setPlaybackLoopStartSample(10);
            engine.setPlaybackLoopEndSample(10);
            expect(!engine.getPlaybackLoopRange().isValid());
        }

        beginTest("tiny loop shorter than block size remains configured but inactive during playback");
        {
            auto take = buildTake(1000.0, 1000, { { 10, 60, true, 1, 1.0f } });
            RecordingEngine engine;
            engine.setPlaybackBlockSize(512);
            engine.setPlaybackLoopStartSample(10);
            engine.setPlaybackLoopEndSample(11);

            const auto rawLoop = engine.getPlaybackLoopRange();
            expect(rawLoop.isValid());
            expectEquals(static_cast<std::int64_t>(10), rawLoop.startSamples);
            expectEquals(static_cast<std::int64_t>(11), rawLoop.endSamples);

            engine.startPlayback(take, 1000.0);

            juce::MidiBuffer buffer;
            engine.renderPlaybackBlock(buffer, 0, 512);

            const auto configuredLoopAfterRender = engine.getPlaybackLoopRange();
            expect(configuredLoopAfterRender.isValid());
            expectEquals(static_cast<std::int64_t>(10), configuredLoopAfterRender.startSamples);
            expectEquals(static_cast<std::int64_t>(11), configuredLoopAfterRender.endSamples);

            expectEquals(1, countMidiBufferEvents(buffer),
                         "rendered MIDI event count stays bounded without repeated cleanup batches");

            engine.advancePlaybackPosition(512);
            expectEquals(static_cast<std::int64_t>(512), engine.getPlaybackPositionSamples(),
                         "playback advances linearly rather than repeatedly wrapping");
        }
    }
};

static AbLoopTest abLoopTest;

// =============================================================================

class PlaybackTransportConcurrencyTest final : public juce::UnitTest {
public:
    PlaybackTransportConcurrencyTest()
        : juce::UnitTest("RecordingEngine: transport concurrency", "DevPiano/Engine") {
    }

    void runTest() override {
        beginTest("speed multiplier published during active rendering applies at block boundary");
        {
            auto take = buildTake(1000.0, 2000,
                                  {
                                      { 0, 60, true, 1, 1.0f },
                                      { 200, 60, false, 1, 0.0f },
                                      { 400, 62, true, 1, 1.0f },
                                      { 600, 62, false, 1, 0.0f },
                                  });

            RecordingEngine engine;
            engine.setPlaybackSpeedMultiplier(1.0);
            engine.startPlayback(take, 1000.0, 0);

            juce::WaitableEvent audioInsideBlock0;
            juce::WaitableEvent uiPublishedSpeed;
            juce::WaitableEvent audioFinishedBlock0;
            juce::WaitableEvent audioFinishedBlock1;

            juce::MidiBuffer block0Buffer;
            juce::MidiBuffer block1Buffer;
            RecordingEngine::TransportCommandResult block1Result;
            std::int64_t posAfterBlock0 = 0;
            std::int64_t posAfterBlock1 = 0;

            struct AudioThread final : public juce::Thread {
                AudioThread(RecordingEngine& e, juce::WaitableEvent& insideB0, juce::WaitableEvent& uiPub,
                            juce::WaitableEvent& finB0, juce::WaitableEvent& finB1, juce::MidiBuffer& b0Buf,
                            juce::MidiBuffer& b1Buf, RecordingEngine::TransportCommandResult& b1Res, std::int64_t& p0,
                            std::int64_t& p1)
                    : juce::Thread("TestAudioThread")
                    , engine(e)
                    , audioInsideBlock0(insideB0)
                    , uiPublishedSpeed(uiPub)
                    , audioFinishedBlock0(finB0)
                    , audioFinishedBlock1(finB1)
                    , block0Buffer(b0Buf)
                    , block1Buffer(b1Buf)
                    , block1Result(b1Res)
                    , posAfterBlock0(p0)
                    , posAfterBlock1(p1) {
                }

                ~AudioThread() override {
                    uiPublishedSpeed.signal();
                    waitForThreadToExit(-1);
                }

                void run() override {
                    engine.renderPlaybackBlock(block0Buffer, 0, 300);
                    audioInsideBlock0.signal();
                    uiPublishedSpeed.wait(5000);

                    engine.advancePlaybackPosition(300);
                    posAfterBlock0 = engine.getPlaybackPositionSamples();
                    audioFinishedBlock0.signal();

                    block1Result = engine.applyPendingTransportCommands(block1Buffer);
                    const auto startPos = engine.getPlaybackPositionSamples();
                    engine.renderPlaybackBlock(block1Buffer, startPos, 400);
                    engine.advancePlaybackPosition(400);
                    posAfterBlock1 = engine.getPlaybackPositionSamples();
                    audioFinishedBlock1.signal();
                }

                RecordingEngine& engine;
                juce::WaitableEvent& audioInsideBlock0;
                juce::WaitableEvent& uiPublishedSpeed;
                juce::WaitableEvent& audioFinishedBlock0;
                juce::WaitableEvent& audioFinishedBlock1;
                juce::MidiBuffer& block0Buffer;
                juce::MidiBuffer& block1Buffer;
                RecordingEngine::TransportCommandResult& block1Result;
                std::int64_t& posAfterBlock0;
                std::int64_t& posAfterBlock1;
            };

            AudioThread audioThread(engine, audioInsideBlock0, uiPublishedSpeed, audioFinishedBlock0,
                                    audioFinishedBlock1, block0Buffer, block1Buffer, block1Result, posAfterBlock0,
                                    posAfterBlock1);

            audioThread.startThread();

            expect(audioInsideBlock0.wait(5000), "audio thread should enter block 0");
            engine.setPlaybackSpeedMultiplier(2.0);

            expectEquals(2.0, engine.getPlaybackSpeedMultiplier());
            expectEquals(1.0, engine.getEffectivePlaybackSpeedMultiplier(),
                         "effective speed on audio timeline must remain 1.0x until block boundary");

            uiPublishedSpeed.signal();

            expect(audioFinishedBlock0.wait(5000), "audio thread should finish block 0");
            expectEquals(static_cast<std::int64_t>(300), posAfterBlock0, "position advanced at 1.0x to 300 samples");

            expect(audioFinishedBlock1.wait(5000), "audio thread should finish block 1");
            audioThread.waitForThreadToExit(-1);

            expect(block1Result.speedChanged, "block 1 boundary must apply speed change");
            expectEquals(2.0, engine.getEffectivePlaybackSpeedMultiplier(),
                         "effective speed must be 2.0x after block boundary");

            int noteOn62Count = 0;
            int noteOff62Count = 0;
            for (const auto meta : block1Buffer) {
                const auto msg = meta.getMessage();
                if (msg.isNoteOn() && msg.getNoteNumber() == 62) {
                    ++noteOn62Count;
                    expectEquals(50, meta.samplePosition, "NoteOn 62 offset at 2.0x");
                }
                if (msg.isNoteOff() && msg.getNoteNumber() == 62) {
                    ++noteOff62Count;
                    expectEquals(150, meta.samplePosition, "NoteOff 62 offset at 2.0x");
                }
            }
            expectEquals(1, noteOn62Count);
            expectEquals(1, noteOff62Count);

            engine.stopPlaybackQuiescent();
        }

        beginTest("active stop published during active rendering dominates and delivers panic cleanup at boundary");
        {
            auto take = buildTake(1000.0, 2000,
                                  {
                                      { 0, 60, true, 1, 1.0f },
                                      { 1500, 60, false, 1, 0.0f },
                                  });

            RecordingEngine engine;
            engine.startPlayback(take, 1000.0, 0);

            juce::WaitableEvent noteSoundingEvent;
            juce::WaitableEvent stopPublishedEvent;
            juce::WaitableEvent stopAppliedEvent;

            juce::MidiBuffer block0Buf;
            juce::MidiBuffer block1Buf;
            RecordingEngine::TransportCommandResult block1Result;
            std::int64_t stopPosSamples = 0;

            struct StopAudioThread final : public juce::Thread {
                StopAudioThread(RecordingEngine& e, juce::WaitableEvent& sounding, juce::WaitableEvent& stopPub,
                                juce::WaitableEvent& stopApp, juce::MidiBuffer& b0, juce::MidiBuffer& b1,
                                RecordingEngine::TransportCommandResult& res, std::int64_t& stopPos)
                    : juce::Thread("TestStopAudioThread")
                    , engine(e)
                    , noteSoundingEvent(sounding)
                    , stopPublishedEvent(stopPub)
                    , stopAppliedEvent(stopApp)
                    , block0Buf(b0)
                    , block1Buf(b1)
                    , block1Result(res)
                    , stopPosSamples(stopPos) {
                }

                ~StopAudioThread() override {
                    stopPublishedEvent.signal();
                    waitForThreadToExit(-1);
                }

                void run() override {
                    engine.renderPlaybackBlock(block0Buf, 0, 200);
                    engine.advancePlaybackPosition(200);
                    noteSoundingEvent.signal();

                    stopPublishedEvent.wait(5000);

                    block1Result = engine.applyPendingTransportCommands(block1Buf);
                    stopPosSamples = engine.getPlaybackPositionSamples();
                    stopAppliedEvent.signal();
                }

                RecordingEngine& engine;
                juce::WaitableEvent& noteSoundingEvent;
                juce::WaitableEvent& stopPublishedEvent;
                juce::WaitableEvent& stopAppliedEvent;
                juce::MidiBuffer& block0Buf;
                juce::MidiBuffer& block1Buf;
                RecordingEngine::TransportCommandResult& block1Result;
                std::int64_t& stopPosSamples;
            };

            StopAudioThread stopThread(engine, noteSoundingEvent, stopPublishedEvent, stopAppliedEvent, block0Buf,
                                       block1Buf, block1Result, stopPosSamples);
            stopThread.startThread();

            expect(noteSoundingEvent.wait(5000), "audio thread should sound note in block 0");
            expect(engine.isPlaying(), "engine must still be playing while audio is rendering block 0");

            engine.requestPlaybackStop();
            expect(engine.isPlaying(), "engine must remain playing until audio block boundary consumes stop");

            stopPublishedEvent.signal();

            expect(stopAppliedEvent.wait(5000), "audio thread should apply stop at block boundary");
            stopThread.waitForThreadToExit(-1);

            expect(block1Result.stopApplied, "block 1 boundary must apply stop command");
            expect(!engine.isPlaying(), "engine must be stopped after block boundary application");
            expectEquals(static_cast<std::int64_t>(200), stopPosSamples,
                         "playback position must retain user-visible sample position");

            int allNotesOffCount = 0;
            int cc64Count = 0;
            int cc120Count = 0;
            for (const auto meta : block1Buf) {
                const auto msg = meta.getMessage();
                if (msg.isAllNotesOff()) {
                    ++allNotesOffCount;
                }
                if (msg.isController() && msg.getControllerNumber() == 64 && msg.getControllerValue() == 0) {
                    ++cc64Count;
                }
                if (msg.isController() && msg.getControllerNumber() == 120 && msg.getControllerValue() == 0) {
                    ++cc120Count;
                }
            }
            expectEquals(16, allNotesOffCount, "All Notes Off on 16 channels");
            expectEquals(16, cc64Count, "Sustain pedal release on 16 channels");
            expectEquals(16, cc120Count, "All Sound Off on 16 channels");
        }

        beginTest("stop dominates concurrent speed and seek at audio block boundary");
        {
            auto take = buildTake(1000.0, 2000,
                                  {
                                      { 0, 60, true, 1, 1.0f },
                                      { 1000, 60, false, 1, 0.0f },
                                  });

            RecordingEngine engine;
            engine.startPlayback(take, 1000.0, 0);

            engine.requestPlaybackSeek(500);
            engine.setPlaybackSpeedMultiplier(2.0);
            engine.requestPlaybackStop();

            juce::MidiBuffer boundaryBuffer;
            const auto result = engine.applyPendingTransportCommands(boundaryBuffer);

            expect(result.stopApplied, "stop must dominate at the boundary");
            expect(!result.seekApplied, "seek must be superseded by stop");
            expect(!result.speedChanged, "speed must be superseded by stop");
            expect(!engine.isPlaying(), "engine must be stopped");
            expectEquals(48, countMidiBufferEvents(boundaryBuffer), "panic cleanup emitted");
        }

        beginTest("speed change at floor-rounding boundary preserves cursor and delivers NoteOff without replay");
        {
            auto take = buildTake(1000.0, 1000,
                                  {
                                      { 100, 60, true, 1, 1.0f },
                                      { 300, 60, false, 1, 0.0f },
                                  });

            RecordingEngine engine;
            engine.setPlaybackSpeedMultiplier(1.0);
            engine.startPlayback(take, 1000.0, 0);

            juce::MidiBuffer block0Buf;
            engine.renderPlaybackBlock(block0Buf, 0, 101);
            expectEquals(1, countMidiBufferEvents(block0Buf), "block 0 should contain NoteOn 60 at sample 100");
            engine.advancePlaybackPosition(101);
            expectEquals(static_cast<std::int64_t>(101), engine.getPlaybackPositionSamples());

            engine.setPlaybackSpeedMultiplier(2.0);

            juce::MidiBuffer block1CmdBuf;
            const auto cmdResult = engine.applyPendingTransportCommands(block1CmdBuf);
            expect(cmdResult.speedChanged);
            expectEquals(2.0, engine.getEffectivePlaybackSpeedMultiplier());

            const auto block1Start = engine.getPlaybackPositionSamples();
            expectEquals(static_cast<std::int64_t>(50), block1Start);

            juce::MidiBuffer block1Buf;
            engine.renderPlaybackBlock(block1Buf, block1Start, 70);
            int block1NoteOnCount = 0;
            int block1NoteOffCount = 0;
            for (const auto meta : block1Buf) {
                if (meta.getMessage().isNoteOn()) {
                    ++block1NoteOnCount;
                }
                if (meta.getMessage().isNoteOff()) {
                    ++block1NoteOffCount;
                }
            }
            expectEquals(0, block1NoteOnCount, "NoteOn 60 must NOT replay at the rounding boundary");
            expectEquals(0, block1NoteOffCount, "NoteOff 60 is at 300 (scaled to 150), not in [50, 120)");
            engine.advancePlaybackPosition(70);
            expectEquals(static_cast<std::int64_t>(120), engine.getPlaybackPositionSamples());

            juce::MidiBuffer block2Buf;
            engine.renderPlaybackBlock(block2Buf, 120, 50);
            int block2NoteOnCount = 0;
            int block2NoteOffCount = 0;
            int noteOffOffset = -1;
            for (const auto meta : block2Buf) {
                if (meta.getMessage().isNoteOn()) {
                    ++block2NoteOnCount;
                }
                if (meta.getMessage().isNoteOff() && meta.getMessage().getNoteNumber() == 60) {
                    ++block2NoteOffCount;
                    noteOffOffset = meta.samplePosition;
                }
            }
            expectEquals(0, block2NoteOnCount);
            expectEquals(1, block2NoteOffCount, "NoteOff 60 must be delivered cleanly");
            expectEquals(30, noteOffOffset, "NoteOff 60 offset = 150 - 120 = 30");

            engine.advancePlaybackPosition(50);
            engine.stopPlaybackQuiescent();
        }

        beginTest("clear retains selected playback speed multiplier");
        {
            RecordingEngine engine;
            engine.setPlaybackSpeedMultiplier(1.5);
            expectEquals(1.5, engine.getPlaybackSpeedMultiplier());

            engine.clear();
            expectEquals(1.5, engine.getPlaybackSpeedMultiplier(),
                         "clear must not silently reset user-selected playback speed");
        }
    }
};

static PlaybackTransportConcurrencyTest playbackTransportConcurrencyTest;
