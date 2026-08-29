#include <JuceHeader.h>

#include "Recording/MidiFileImporter.h"
#include "Recording/MidiTrackMergeEngine.h"
#include "Recording/RecordingEngine.h"

// =============================================================================
// MIDI 文件导入与多轨时间线合并测试
//
// 这些测试使用 tests/fixtures/midi/ 下的真实 MIDI 文件与合成多轨测试用例。
// fixture 目录相对 __FILE__（source/tests/）定位（TEST-014），与 CWD 无关；
// CTest 的 WORKING_DIRECTORY 只作为兼容回退。
// =============================================================================

static juce::File getFixtureDir() {
    return juce::File(__FILE__).getParentDirectory().getChildFile("../../tests/fixtures/midi");
}

static juce::String getFixturePath(const juce::String& filename) {
    return getFixtureDir().getChildFile(filename).getFullPathName();
}

/// 导入指定 fixture 文件并返回 importMidiFile 结果（默认采样率 48kHz）。
static auto importFixture(const juce::String& name, double sampleRate = 48000.0) {
    return devpiano::recording::importMidiFile(juce::File(getFixturePath(name)), sampleRate);
}

// =============================================================================

class MidiFileImportSmokeTest : public juce::UnitTest {
public:
    MidiFileImportSmokeTest()
        : juce::UnitTest("MidiFileImport", "DevPiano/Recording") {
    }

    void runTest() override {
        using devpiano::recording::importMidiFile;

        testCase("non-existent file returns nullopt", [&] {
            auto result = importMidiFile(juce::File("/nonexistent/path.mid"), 48000.0);
            expect(!result.has_value());
        });

        testCase("empty file returns nullopt", [&] {
            auto result = importFixture("empty.mid");
            expect(!result.has_value());
        });

        testCase("invalid file returns nullopt", [&] {
            // invalid.mid 包含非 MIDI 的垃圾数据，应解析失败
            auto result = importFixture("invalid.mid");
            expect(!result.has_value());
        });
    }
};

static MidiFileImportSmokeTest midiFileImportSmokeTest;

// =============================================================================

class MidiFileImportDetail : public juce::UnitTest {
public:
    MidiFileImportDetail()
        : juce::UnitTest("MidiFileImportDetail", "DevPiano/Recording") {
    }

    void runTest() override {
        using devpiano::recording::importMidiFile;
        using devpiano::recording::MidiImportOptions;

        testCase("simple-notes.mid imports with events, sample rate, timestamps and note events", [&] {
            auto result = importFixture("simple-notes.mid");
            expect(result.has_value());
            expect(!result->isEmpty());
            expectGreaterThan(result->events.size(), size_t(0));
            expectEquals(result->sampleRate, 48000.0);
            for (const auto& ev : result->events) {
                expect(ev.timestampSamples >= 0);
                // 所有事件都应处于合理范围内（文件很短）
                expectLessThan(ev.timestampSamples, std::int64_t(48000 * 10));
            }

            int noteOnCount = 0;
            for (const auto& ev : result->events) {
                if (ev.message.isNoteOn()) {
                    ++noteOnCount;
                }
            }
            expectGreaterThan(noteOnCount, 0);
        });

        testCase("multitrack-basic.mid imports with default and track options", [&] {
            auto result = importFixture("multitrack-basic.mid");
            expect(result.has_value());
            expect(!result->isEmpty());

            MidiImportOptions singleTrackOpts;
            singleTrackOpts.ignoreOtherTracks = true;
            auto resultSingleTrack
                = importMidiFile(juce::File(getFixturePath("multitrack-basic.mid")), 48000.0, singleTrackOpts);
            expect(resultSingleTrack.has_value());
            expect(!resultSingleTrack->isEmpty());

            MidiImportOptions allTracksOpts;
            allTracksOpts.mergeAllTracks = true;
            allTracksOpts.ignoreOtherTracks = false;
            auto resultAllTracks
                = importMidiFile(juce::File(getFixturePath("multitrack-basic.mid")), 48000.0, allTracksOpts);
            expect(resultAllTracks.has_value());
            expect(!resultAllTracks->isEmpty());

            // Default import should be multi-track merge mode
            expectGreaterThan(resultAllTracks->events.size(), size_t(0));
        });

        testCase("sustain-pedal.mid imports successfully with controller events", [&] {
            auto result = importFixture("sustain-pedal.mid");
            expect(result.has_value());
            expect(!result->isEmpty());

            int ccCount = 0;
            for (const auto& ev : result->events) {
                if (ev.message.isController()) {
                    ++ccCount;
                }
            }
            expectGreaterThan(ccCount, 0);
        });

        testCase("tempo-change-basic.mid imports successfully", [&] {
            auto result = importFixture("tempo-change-basic.mid");
            expect(result.has_value());
            expect(!result->isEmpty());
        });

        testCase("velocity-channel.mid has varying velocity and non-default channel", [&] {
            auto result = importFixture("velocity-channel.mid");
            expect(result.has_value());
            expect(!result->isEmpty());

            bool foundVaryingVelocity = false;
            bool foundNonDefaultChannel = false;

            for (const auto& ev : result->events) {
                if (ev.message.isNoteOn()) {
                    if (ev.message.getVelocity() != 127) {
                        foundVaryingVelocity = true;
                    }
                    if (ev.message.getChannel() != 0) {
                        foundNonDefaultChannel = true;
                    }
                }
            }

            expect(foundVaryingVelocity, "fixture should contain non-127 velocities");
            expect(foundNonDefaultChannel, "fixture should contain non-channel-1 events");
        });
    }
};

static MidiFileImportDetail midiFileImportDetailTest;

// =============================================================================

class MidiTrackMergeEngineTest : public juce::UnitTest {
public:
    MidiTrackMergeEngineTest()
        : juce::UnitTest("MidiTrackMergeEngine", "DevPiano/Recording") {
    }

    void runTest() override {
        using devpiano::recording::MidiChannelMappingStrategy;
        using devpiano::recording::MidiTrackMergeEngine;
        using devpiano::recording::MidiTrackMergeOptions;

        testCase("empty or invalid input returns nullopt", [&] {
            juce::MidiFile emptyFile;
            auto res1 = MidiTrackMergeEngine::mergeTracks(emptyFile, 48000.0);
            expect(!res1.has_value());

            juce::MidiFile validFile;
            juce::MidiMessageSequence track;
            track.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0.0);
            validFile.addTrack(track);

            auto res2 = MidiTrackMergeEngine::mergeTracks(validFile, 0.0);
            expect(!res2.has_value());

            auto res3 = MidiTrackMergeEngine::mergeTracks(validFile, -44100.0);
            expect(!res3.has_value());
        });

        testCase("simultaneous events priority sorting order", [&] {
            juce::MidiFile file;
            juce::MidiMessageSequence track;
            // Add events at the exact same timestamp (0.5s) in reverse priority order
            track.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0.5);
            track.addEvent(juce::MidiMessage::noteOff(1, 59, (juce::uint8)64), 0.5);
            track.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0.5);
            track.addEvent(juce::MidiMessage::programChange(1, 10), 0.5);

            file.addTrack(track);

            auto result = MidiTrackMergeEngine::mergeTracks(file, 48000.0);
            expect(result.has_value());
            expectEquals(result->take.events.size(), size_t(4));

            // Verify priority: Program Change (1) -> Controller (2) -> Note Off (3) -> Note On (4)
            expect(result->take.events[0].message.isProgramChange(), "First should be ProgramChange");
            expect(result->take.events[1].message.isController(), "Second should be Controller");
            expect(result->take.events[2].message.isNoteOff(true), "Third should be NoteOff");
            expect(result->take.events[3].message.isNoteOn(false), "Fourth should be NoteOn");
        });

        testCase("synthetic multi-track merge with chronological ordering and stats", [&] {
            juce::MidiFile file;

            // Track 0: Conductor (Tempo & Meta only)
            juce::MidiMessageSequence track0;
            track0.addEvent(juce::MidiMessage::textMetaEvent(1, "Track 0 Conductor"), 0.0);
            file.addTrack(track0);

            // Track 1: Right hand notes
            juce::MidiMessageSequence track1;
            track1.addEvent(juce::MidiMessage::programChange(1, 0), 0.0);
            track1.addEvent(juce::MidiMessage::noteOn(1, 72, (juce::uint8)100), 0.1);
            track1.addEvent(juce::MidiMessage::noteOff(1, 72, (juce::uint8)0), 0.5);
            track1.addEvent(juce::MidiMessage::noteOn(1, 74, (juce::uint8)100), 0.6);
            track1.addEvent(juce::MidiMessage::noteOff(1, 74, (juce::uint8)0), 1.0);
            file.addTrack(track1);

            // Track 2: Left hand notes + sustain pedal
            juce::MidiMessageSequence track2;
            track2.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0.05);
            track2.addEvent(juce::MidiMessage::noteOn(1, 48, (juce::uint8)90), 0.1);
            track2.addEvent(juce::MidiMessage::noteOff(1, 48, (juce::uint8)0), 0.8);
            track2.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), 0.9);
            file.addTrack(track2);

            MidiTrackMergeOptions opts;
            opts.channelStrategy = MidiChannelMappingStrategy::passThrough;
            opts.singleTrackOnly = false;

            auto mergeRes = MidiTrackMergeEngine::mergeTracks(file, 48000.0, opts);
            expect(mergeRes.has_value());
            const auto& take = mergeRes->take;
            const auto& stats = mergeRes->stats;

            expectEquals(stats.trackCount, 3);
            expectEquals(stats.noteOnCount, 3);
            expectEquals(stats.noteOffCount, 3);
            expectEquals(stats.ccCount, 2);
            expectEquals(stats.programChangeCount, 1);
            expectEquals(stats.mergedEventCount, 9);

            // Verify chronological order: timestamps strictly non-decreasing
            std::int64_t prevTs = -1;
            for (const auto& ev : take.events) {
                expect(ev.timestampSamples >= prevTs, "Timestamps must be non-decreasing");
                prevTs = ev.timestampSamples;
            }

            // Verify singleTrackOnly mode extracts only track 1 (has 4 note events vs track 2's 2 note events)
            MidiTrackMergeOptions singleOpts;
            singleOpts.singleTrackOnly = true;
            auto singleRes = MidiTrackMergeEngine::mergeTracks(file, 48000.0, singleOpts);
            expect(singleRes.has_value());
            expectEquals(singleRes->stats.noteOnCount, 2);
            expectEquals(singleRes->stats.noteOffCount, 2);
            expectEquals(singleRes->stats.mergedEventCount, 5); // 1 prog + 2 noteOn + 2 noteOff
        });

        testCase("channel mapping strategies verification", [&] {
            juce::MidiFile file;

            // Track 0 has notes on Ch 1
            juce::MidiMessageSequence track0;
            track0.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)80), 0.1);
            track0.addEvent(juce::MidiMessage::noteOff(1, 60, (juce::uint8)0), 0.5);
            file.addTrack(track0);

            // Track 1 has notes on Ch 1 (same channel)
            juce::MidiMessageSequence track1;
            track1.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8)80), 0.2);
            track1.addEvent(juce::MidiMessage::noteOff(1, 64, (juce::uint8)0), 0.6);
            file.addTrack(track1);

            // Strategy 1: passThrough retains Ch 1 for both
            {
                MidiTrackMergeOptions opts;
                opts.channelStrategy = MidiChannelMappingStrategy::passThrough;
                auto res = MidiTrackMergeEngine::mergeTracks(file, 48000.0, opts);
                expect(res.has_value());
                for (const auto& ev : res->take.events) {
                    expectEquals(ev.message.getChannel(), 1);
                }
            }

            // Strategy 2: autoAssignIfSingleChannel assigns Track 0 -> Ch 1, Track 1 -> Ch 2
            {
                MidiTrackMergeOptions opts;
                opts.channelStrategy = MidiChannelMappingStrategy::autoAssignIfSingleChannel;
                auto res = MidiTrackMergeEngine::mergeTracks(file, 48000.0, opts);
                expect(res.has_value());
                expectEquals(res->take.events[0].message.getChannel(), 1); // Track 0 noteOn (ts=0.1)
                expectEquals(res->take.events[1].message.getChannel(), 2); // Track 1 noteOn (ts=0.2)
                expectEquals(res->take.events[2].message.getChannel(), 1); // Track 0 noteOff (ts=0.5)
                expectEquals(res->take.events[3].message.getChannel(), 2); // Track 1 noteOff (ts=0.6)
            }

            // Strategy 3: forceTrackToChannel maps track index % 16 + 1
            {
                MidiTrackMergeOptions opts;
                opts.channelStrategy = MidiChannelMappingStrategy::forceTrackToChannel;
                auto res = MidiTrackMergeEngine::mergeTracks(file, 48000.0, opts);
                expect(res.has_value());
                expectEquals(res->take.events[0].message.getChannel(), 1);
                expectEquals(res->take.events[1].message.getChannel(), 2);
            }
        });
    }
};

static MidiTrackMergeEngineTest midiTrackMergeEngineTest;
