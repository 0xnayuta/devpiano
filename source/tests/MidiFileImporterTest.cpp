#include <JuceHeader.h>

#include "Recording/MidiFileImporter.h"
#include "Recording/MidiTrackMergeEngine.h"
#include "Recording/PerformanceFile.h"
#include "Recording/RecordingEngine.h"
#include "TestHelpers.h"

// =============================================================================
// MIDI 文件导入与多轨时间线合并测试
//
// 这些测试使用 tests/fixtures/midi/ 下的真实 MIDI 文件与合成多轨测试用例。
// fixture 目录相对 __FILE__（source/tests/）定位（TEST-014），与 CWD 无关；
// CTest 的 WORKING_DIRECTORY 只作为兼容回退。
// =============================================================================

static juce::File getFixtureDir() {
    return devpiano::test::getFixturesDir().getChildFile("midi");
}

static juce::String getFixturePath(const juce::String& filename) {
    return getFixtureDir().getChildFile(filename).getFullPathName();
}

/// 导入指定 fixture 文件并返回 importMidiFile 结果（默认采样率 48kHz）。
static auto importFixture(const juce::String& name, double sampleRate = 48000.0) {
    return devpiano::recording::importMidiFile(juce::File(getFixturePath(name)), sampleRate);
}
static void writeBinaryFile(const juce::File& file, const std::vector<uint8_t>& bytes) {
    juce::FileOutputStream out(file);
    if (out.openedOk()) {
        out.write(bytes.data(), bytes.size());
    }
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

        testCase("trailing bytes after the last chunk do not abort the import", [&] {
            // 真实文件常见形态：最后一个 MTrk 之后残留换行等杂散字节。
            // juce::MidiFile::readFrom 对此整体返回 false，但轨道内容已解析完成，应继续导入。
            devpiano::test::ScopedTempDir tempDir("midi-trailing-bytes");
            const auto file = tempDir.getChildFile("trailing-bytes.mid");

            juce::MidiFile midi;
            juce::MidiMessageSequence track;
            track.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0.1);
            track.addEvent(juce::MidiMessage::noteOff(1, 60, (juce::uint8)0), 0.5);
            midi.addTrack(track);

            {
                juce::FileOutputStream out(file);
                expect(out.openedOk());
                if (!out.openedOk()) {
                    return;
                }
                expect(midi.writeTo(out));
                out.write("\r\n", 2);
            }
            expect(file.existsAsFile());

            auto result = importMidiFile(file, 48000.0);
            expect(result.has_value(), "trailing bytes must not discard a fully parsed file");
            if (result.has_value()) {
                expectGreaterThan(result->events.size(), size_t(0));
            }
        });

        testCase("missing second declared MTrk returns nullopt (ERR-004)", [&] {
            devpiano::test::ScopedTempDir tempDir("midi-missing-mtrk");
            const auto file = tempDir.getChildFile("missing-second-track.mid");

            // Header: format 1, 2 tracks, division 480 (14 bytes)
            // Track 0: MTrk (13 bytes body)
            // Missing Track 1 chunk entirely
            const std::vector<uint8_t> payload
                = { 'M',  'T',  'h',  'd',  0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x02,
                    0x01, 0xE0, 'M',  'T',  'r',  'k',  0x00, 0x00, 0x00, 0x0D, 0x00, 0x90,
                    0x3C, 0x64, 0x83, 0x60, 0x80, 0x3C, 0x00, 0x00, 0xFF, 0x2F, 0x00 };
            writeBinaryFile(file, payload);
            auto result = importMidiFile(file, 48000.0);
            expect(!result.has_value(), "file missing declared second MTrk must be rejected");
        });

        testCase("truncated chunk body returns nullopt (ERR-004)", [&] {
            devpiano::test::ScopedTempDir tempDir("midi-truncated-chunk");
            const auto file = tempDir.getChildFile("short-chunk.mid");

            // Header: format 0, 1 track, division 480
            // Track 0: declares 64 bytes chunkSize, but file supplies only 10 bytes
            const std::vector<uint8_t> payload
                = { 'M', 'T', 'h',  'd',  0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x01, 0xE0, 'M',  'T',
                    'r', 'k', 0x00, 0x00, 0x00, 0x40, 0x00, 0x90, 0x3C, 0x64, 0x00, 0x80, 0x3C, 0x00, 0x00, 0xFF };
            writeBinaryFile(file, payload);
            auto result = importMidiFile(file, 48000.0);
            expect(!result.has_value(), "file with truncated chunk body must be rejected");
        });

        testCase("truncated track event returns nullopt (ERR-004)", [&] {
            devpiano::test::ScopedTempDir tempDir("midi-truncated-event");
            const auto file = tempDir.getChildFile("truncated-event.mid");

            // Header: format 0, 1 track, division 480
            // Track 0: declares 3 bytes, body has dt=0 and NoteOn with note but missing velocity
            const std::vector<uint8_t> payload
                = { 'M',  'T', 'h', 'd', 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x01,
                    0xE0, 'M', 'T', 'r', 'k',  0x00, 0x00, 0x00, 0x03, 0x00, 0x90, 0x3C };
            writeBinaryFile(file, payload);
            auto result = importMidiFile(file, 48000.0);
            expect(!result.has_value(), "file with truncated event in track must be rejected");
        });

        testCase("invalid 0x58 fixed length returns nullopt (SEC-006)", [&] {
            devpiano::test::ScopedTempDir tempDir("midi-invalid-meta-len");
            const auto file = tempDir.getChildFile("invalid-0x58-length.mid");

            // Time signature 0x58 with length 2 instead of 4
            const std::vector<uint8_t> payload
                = { 'M', 'T', 'h',  'd',  0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x01, 0xE0, 'M',  'T',
                    'r', 'k', 0x00, 0x00, 0x00, 0x0A, 0x00, 0xFF, 0x58, 0x02, 0x04, 0x02, 0x00, 0xFF, 0x2F, 0x00 };
            writeBinaryFile(file, payload);
            auto result = importMidiFile(file, 48000.0);
            expect(!result.has_value(), "invalid 0x58 fixed length must be rejected");
        });

        testCase("invalid 0x58 denominator exponent returns nullopt (SEC-006)", [&] {
            devpiano::test::ScopedTempDir tempDir("midi-invalid-meta-exp");
            const auto file = tempDir.getChildFile("invalid-0x58-exp.mid");

            // Time signature 0x58 with exponent 32 (0x20)
            const std::vector<uint8_t> payload = { 'M',  'T',  'h',  'd',  0x00, 0x00, 0x00, 0x06, 0x00,
                                                   0x00, 0x00, 0x01, 0x01, 0xE0, 'M',  'T',  'r',  'k',
                                                   0x00, 0x00, 0x00, 0x0C, 0x00, 0xFF, 0x58, 0x04, 0x04,
                                                   0x20, 0x18, 0x08, 0x00, 0xFF, 0x2F, 0x00 };
            writeBinaryFile(file, payload);
            auto result = importMidiFile(file, 48000.0);
            expect(!result.has_value(), "invalid 0x58 denominator exponent must be rejected");
        });

        testCase("legal time signature imports successfully (SEC-006)", [&] {
            devpiano::test::ScopedTempDir tempDir("midi-legal-timesig");
            const auto file = tempDir.getChildFile("legal-3-4.mid");

            // Time signature 3/4 (num 3, exp 2 -> denom 4)
            const std::vector<uint8_t> payload
                = { 'M',  'T',  'h',  'd',  0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x01, 0xE0, 'M',
                    'T',  'r',  'k',  0x00, 0x00, 0x00, 0x15, 0x00, 0xFF, 0x58, 0x04, 0x03, 0x02, 0x18, 0x08,
                    0x00, 0x90, 0x3C, 0x64, 0x83, 0x60, 0x80, 0x3C, 0x00, 0x00, 0xFF, 0x2F, 0x00 };
            writeBinaryFile(file, payload);
            auto result = devpiano::recording::importMidiFileWithMetadata(file, 48000.0);
            expect(result.has_value(), "legal 3/4 time signature file must import successfully");
            if (result.has_value()) {
                expect(result->metadata.initialTimeSignature.has_value());
                if (result->metadata.initialTimeSignature.has_value()) {
                    expectEquals(result->metadata.initialTimeSignature->numerator, 3);
                    expectEquals(result->metadata.initialTimeSignature->denominator, 4);
                }
            }
        });

        testCase("valid complete multi-track with trailing CRLF succeeds (ERR-004)", [&] {
            devpiano::test::ScopedTempDir tempDir("midi-complete-crlf");
            const auto file = tempDir.getChildFile("complete-crlf.mid");

            // Header: format 1, 2 tracks, division 480
            // Track 0: Conductor (19 bytes)
            // Track 1: Notes (13 bytes)
            // Trailing: 0x0D, 0x0A (\r\n)
            const std::vector<uint8_t> payload
                = { 'M',  'T',  'h',  'd',  0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x02, 0x01, 0xE0, 'M',  'T',
                    'r',  'k',  0x00, 0x00, 0x00, 0x13, 0x00, 0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08, 0x00, 0xFF,
                    0x51, 0x03, 0x07, 0xA1, 0x20, 0x00, 0xFF, 0x2F, 0x00, 'M',  'T',  'r',  'k',  0x00, 0x00, 0x00,
                    0x0D, 0x00, 0x90, 0x3C, 0x64, 0x83, 0x60, 0x80, 0x3C, 0x00, 0x00, 0xFF, 0x2F, 0x00, 0x0D, 0x0A };
            writeBinaryFile(file, payload);
            auto result = devpiano::recording::importMidiFileWithMetadata(file, 48000.0);
            expect(result.has_value(), "valid complete multi-track with CRLF must succeed");
            if (result.has_value()) {
                expectEquals(result->stats.trackCount, 2);
                expectGreaterThan(result->take.events.size(), size_t(0));
            }
        });

        testCase("invalid time division zero is rejected", [&] {
            devpiano::test::ScopedTempDir tempDir("midi-zero-division");
            const auto file = tempDir.getChildFile("zero-division.mid");

            // Header: division 0
            const std::vector<uint8_t> payload
                = { 'M',  'T', 'h', 'd', 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x00,
                    0x00, 'M', 'T', 'r', 'k',  0x00, 0x00, 0x00, 0x04, 0x00, 0xFF, 0x2F, 0x00 };
            writeBinaryFile(file, payload);
            auto result = importMidiFile(file, 48000.0);
            expect(!result.has_value(), "MIDI file with time division 0 must be rejected");
        });

        testCase("unsupported target sample rate returns nullopt (SEC-004)", [&] {
            devpiano::test::ScopedTempDir tempDir("midi-sample-rate");
            const auto file = tempDir.getChildFile("valid-test.mid");

            const std::vector<uint8_t> payload
                = { 'M',  'T',  'h',  'd',  0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01,
                    0x01, 0xE0, 'M',  'T',  'r',  'k',  0x00, 0x00, 0x00, 0x0D, 0x00, 0x90,
                    0x3C, 0x64, 0x83, 0x60, 0x80, 0x3C, 0x00, 0x00, 0xFF, 0x2F, 0x00 };
            writeBinaryFile(file, payload);
            expect(!importMidiFile(file, 0.0).has_value());
            expect(!importMidiFile(file, -44100.0).has_value());
            expect(!importMidiFile(file, 4000.0).has_value());
            expect(!importMidiFile(file, 500000.0).has_value());
            expect(!importMidiFile(file, std::numeric_limits<double>::quiet_NaN()).has_value());
            expect(importMidiFile(file, 48000.0).has_value());
        });

        testCase("missing terminal End of Track returns nullopt", [&] {
            devpiano::test::ScopedTempDir tempDir("midi-missing-eot");
            const auto file = tempDir.getChildFile("missing-eot.mid");

            const std::vector<uint8_t> payload
                = { 'M', 'T', 'h',  'd',  0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x01, 0xE0, 'M', 'T',
                    'r', 'k', 0x00, 0x00, 0x00, 0x09, 0x00, 0x90, 0x3C, 0x64, 0x83, 0x60, 0x80, 0x3C, 0x00 };
            writeBinaryFile(file, payload);
            auto result = importMidiFile(file, 48000.0);
            expect(!result.has_value(), "track missing terminal EOT must be rejected");
        });

        testCase("events after terminal End of Track returns nullopt", [&] {
            devpiano::test::ScopedTempDir tempDir("midi-events-after-eot");
            const auto file = tempDir.getChildFile("events-after-eot.mid");

            const std::vector<uint8_t> payload
                = { 'M',  'T',  'h',  'd',  0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x01,
                    0xE0, 'M',  'T',  'r',  'k',  0x00, 0x00, 0x00, 0x11, 0x00, 0x90, 0x3C, 0x64,
                    0x00, 0xFF, 0x2F, 0x00, 0x00, 0x90, 0x3E, 0x64, 0x83, 0x60, 0x80, 0x3E, 0x00 };
            writeBinaryFile(file, payload);
            auto result = importMidiFile(file, 48000.0);
            expect(!result.has_value(), "events after terminal EOT must be rejected");
        });

        testCase("complete unknown extension chunk is skipped and preserves MTrk import", [&] {
            devpiano::test::ScopedTempDir tempDir("midi-unknown-chunk");
            const auto file = tempDir.getChildFile("unknown-chunk.mid");

            const std::vector<uint8_t> payload
                = { 'M',  'T',  'h',  'd',  0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x01, 0x01, 0xE0, 'U',  'N',
                    'K',  'N',  0x00, 0x00, 0x00, 0x04, 0x01, 0x02, 0x03, 0x04, 'M',  'T',  'r',  'k',  0x00, 0x00,
                    0x00, 0x0D, 0x00, 0x90, 0x3C, 0x64, 0x83, 0x60, 0x80, 0x3C, 0x00, 0x00, 0xFF, 0x2F, 0x00 };
            writeBinaryFile(file, payload);
            auto result = importMidiFile(file, 48000.0);
            expect(result.has_value(), "complete unknown extension chunk must be skipped");
            if (result.has_value()) {
                expectEquals(result->events.size(), size_t(2));
            }
        });

        testCase("missing MTrk when unknown chunk is present returns nullopt", [&] {
            devpiano::test::ScopedTempDir tempDir("midi-unknown-missing-mtrk");
            const auto file = tempDir.getChildFile("unknown-missing-mtrk.mid");

            // Header declares 2 tracks, but file only has 1 unknown chunk and 1 MTrk chunk
            const std::vector<uint8_t> payload
                = { 'M',  'T',  'h',  'd',  0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x02, 0x01, 0xE0, 'U',  'N',
                    'K',  'N',  0x00, 0x00, 0x00, 0x04, 0x01, 0x02, 0x03, 0x04, 'M',  'T',  'r',  'k',  0x00, 0x00,
                    0x00, 0x0D, 0x00, 0x90, 0x3C, 0x64, 0x83, 0x60, 0x80, 0x3C, 0x00, 0x00, 0xFF, 0x2F, 0x00 };
            writeBinaryFile(file, payload);
            auto result = importMidiFile(file, 48000.0);
            expect(!result.has_value(), "missing second MTrk even with unknown chunk must be rejected");
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
            if (!result.has_value()) {
                return;
            }
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

        testCase("multitrack-basic.mid imports with default multi-track options", [&] {
            auto result = importFixture("multitrack-basic.mid");
            expect(result.has_value());
            if (!result.has_value()) {
                return;
            }
            expect(!result->isEmpty());
            expectGreaterThan(result->events.size(), size_t(0));
        });

        testCase("sustain-pedal.mid imports successfully with controller events", [&] {
            auto result = importFixture("sustain-pedal.mid");
            expect(result.has_value());
            if (!result.has_value()) {
                return;
            }
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
            if (!result.has_value()) {
                return;
            }
            expect(!result->isEmpty());
        });

        testCase("velocity-channel.mid has varying velocity and non-default channel", [&] {
            auto result = importFixture("velocity-channel.mid");
            expect(result.has_value());
            if (!result.has_value()) {
                return;
            }
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

            expect(!MidiTrackMergeEngine::mergeTracks(validFile, 0.0).has_value());
            expect(!MidiTrackMergeEngine::mergeTracks(validFile, -44100.0).has_value());
            expect(!MidiTrackMergeEngine::mergeTracks(validFile, 4000.0).has_value());
            expect(!MidiTrackMergeEngine::mergeTracks(validFile, 500000.0).has_value());
            expect(!MidiTrackMergeEngine::mergeTracks(validFile, std::numeric_limits<double>::quiet_NaN()).has_value());
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
            track0.addEvent(juce::MidiMessage::tempoMetaEvent(juce::roundToInt(60000000.0 / 140.0)), 0.0);
            track0.addEvent(juce::MidiMessage::timeSignatureMetaEvent(3, 4), 0.0);
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
                if (res.has_value() && res->take.events.size() >= 4) {
                    // Events from track 0 (mapped to Ch 1)
                    expectEquals(res->take.events[0].message.getChannel(), 1); // noteOn
                    expectEquals(res->take.events[2].message.getChannel(), 1); // noteOff
                    // Events from track 1 (mapped to Ch 2)
                    expectEquals(res->take.events[1].message.getChannel(), 2); // noteOn
                    expectEquals(res->take.events[3].message.getChannel(), 2); // noteOff
                }
            }
        });

        testCase("meta parsing: time signature, key signature, tempo map, song title, and track names", [&] {
            juce::MidiFile file;

            // Track 0: Song title text, tempo changes, time signature, key signature
            juce::MidiMessageSequence track0;
            track0.addEvent(juce::MidiMessage::textMetaEvent(1, "Sonata in C Major"), 0.0);
            // 140 BPM = 60 / 140 = 0.428571s per quarter -> 428571 microseconds
            track0.addEvent(juce::MidiMessage::tempoMetaEvent(428571), 0.0);
            // 160 BPM = 60 / 160 = 0.375s per quarter -> 375000 microseconds at 1.0s
            track0.addEvent(juce::MidiMessage::tempoMetaEvent(375000), 1.0);
            track0.addEvent(juce::MidiMessage::timeSignatureMetaEvent(3, 4), 0.0);
            track0.addEvent(juce::MidiMessage::keySignatureMetaEvent(1, false), 0.0); // 1 sharp = G major
            file.addTrack(track0);

            // Track 1: Right hand with track name
            juce::MidiMessageSequence track1;
            track1.addEvent(juce::MidiMessage::textMetaEvent(3, "Piano Right Hand"), 0.0); // Meta 3 = track name
            track1.addEvent(juce::MidiMessage::noteOn(1, 72, (juce::uint8)100), 0.1);
            track1.addEvent(juce::MidiMessage::noteOff(1, 72, (juce::uint8)0), 0.5);
            file.addTrack(track1);

            // Track 2: Left hand with track name
            juce::MidiMessageSequence track2;
            track2.addEvent(juce::MidiMessage::textMetaEvent(3, "Piano Left Hand"), 0.0);
            track2.addEvent(juce::MidiMessage::noteOn(1, 48, (juce::uint8)90), 0.1);
            track2.addEvent(juce::MidiMessage::noteOff(1, 48, (juce::uint8)0), 0.5);
            file.addTrack(track2);

            auto res = MidiTrackMergeEngine::mergeTracks(file, 48000.0);
            expect(res.has_value());
            const auto& meta = res->metadata;

            expectEquals(meta.songTitle, juce::String("Sonata in C Major"));
            expect(meta.initialTimeSignature.has_value());
            expectEquals(meta.initialTimeSignature->numerator, 3);
            expectEquals(meta.initialTimeSignature->denominator, 4);
            expectEquals(meta.initialTimeSignature->toString(), juce::String("3/4"));

            expect(meta.initialKeySignature.has_value());
            expectEquals(meta.initialKeySignature->sharpsOrFlats, 1);
            expect(!meta.initialKeySignature->isMinor);
            expectEquals(meta.initialKeySignature->toString(), juce::String("G major"));

            expectEquals(meta.tempoMap.size(), size_t(2));
            expectWithinAbsoluteError(meta.initialBpm, 140.0, 0.5);
            expectWithinAbsoluteError(meta.minBpm, 140.0, 0.5);
            expectWithinAbsoluteError(meta.maxBpm, 160.0, 0.5);

            expectEquals(meta.tracks.size(), size_t(3));
            expectEquals(meta.tracks[1].trackName, juce::String("Piano Right Hand"));
            expectEquals(meta.tracks[2].trackName, juce::String("Piano Left Hand"));

            const auto summary = meta.formatSummary();
            expect(summary.contains("Sonata in C Major"), "Summary should contain song title");
            expect(summary.contains("3/4"), "Summary should contain time signature");
            expect(summary.contains("G major"), "Summary should contain key signature");
        });

        testCase("key signature string formatting for major and minor keys", [&] {
            using devpiano::recording::MidiKeySignature;

            expectEquals(MidiKeySignature { 0, false }.toString(), juce::String("C major"));
            expectEquals(MidiKeySignature { 1, false }.toString(), juce::String("G major"));
            expectEquals(MidiKeySignature { 7, false }.toString(), juce::String("C# major"));
            expectEquals(MidiKeySignature { -1, false }.toString(), juce::String("F major"));
            expectEquals(MidiKeySignature { -7, false }.toString(), juce::String("Cb major"));

            expectEquals(MidiKeySignature { 0, true }.toString(), juce::String("A minor"));
            expectEquals(MidiKeySignature { 1, true }.toString(), juce::String("E minor"));
            expectEquals(MidiKeySignature { 7, true }.toString(), juce::String("A# minor"));
            expectEquals(MidiKeySignature { -1, true }.toString(), juce::String("D minor"));
            expectEquals(MidiKeySignature { -7, true }.toString(), juce::String("Ab minor"));
        });

        testCase("copyright meta event (Meta 2) is ignored and does not contaminate song title", [&] {
            juce::MidiFile file;

            // Track 0: Copyright (Meta 2) appears first, followed by Song Title (Meta 3)
            juce::MidiMessageSequence track0;
            track0.addEvent(juce::MidiMessage::textMetaEvent(2, "Copyright (C) 2026 DevPiano Authors"), 0.0);
            track0.addEvent(juce::MidiMessage::textMetaEvent(3, "Clair de Lune"), 0.0);
            file.addTrack(track0);

            // Track 1: Notes
            juce::MidiMessageSequence track1;
            track1.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0.1);
            track1.addEvent(juce::MidiMessage::noteOff(1, 60, (juce::uint8)0), 0.5);
            file.addTrack(track1);

            auto res = MidiTrackMergeEngine::mergeTracks(file, 48000.0);
            expect(res.has_value());
            if (res.has_value()) {
                // Song title must be "Clair de Lune", NOT the copyright string!
                expectEquals(res->metadata.songTitle, juce::String("Clair de Lune"));
                expect(!res->metadata.songTitle.contains("Copyright"));
            }
        });

        testCase("direct merge API rejects invalid meta events and out-of-bounds timestamps (SEC-004/005/006)", [&] {
            // Invalid 0x58 length != 4
            {
                juce::MidiFile file;
                juce::MidiMessageSequence track;
                const uint8_t raw[] = { 0xFF, 0x58, 0x02, 0x04, 0x02 };
                track.addEvent(juce::MidiMessage(raw, sizeof(raw)), 0.0);
                track.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)80), 0.1);
                file.addTrack(track);
                expect(!MidiTrackMergeEngine::mergeTracks(file, 48000.0).has_value(),
                       "0x58 length != 4 must be rejected");
            }

            // Invalid 0x58 denominator exponent >= 31
            {
                juce::MidiFile file;
                juce::MidiMessageSequence track;
                const uint8_t raw[] = { 0xFF, 0x58, 0x04, 0x04, 0x20, 0x18, 0x08 };
                track.addEvent(juce::MidiMessage(raw, sizeof(raw)), 0.0);
                track.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)80), 0.1);
                file.addTrack(track);
                expect(!MidiTrackMergeEngine::mergeTracks(file, 48000.0).has_value(),
                       "0x58 exponent >= 31 must be rejected");
            }

            // Invalid 0x58 numerator == 0
            {
                juce::MidiFile file;
                juce::MidiMessageSequence track;
                const uint8_t raw[] = { 0xFF, 0x58, 0x04, 0x00, 0x02, 0x18, 0x08 };
                track.addEvent(juce::MidiMessage(raw, sizeof(raw)), 0.0);
                track.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)80), 0.1);
                file.addTrack(track);
                expect(!MidiTrackMergeEngine::mergeTracks(file, 48000.0).has_value(),
                       "0x58 numerator 0 must be rejected");
            }
            // Unterminated raw VLQ length in meta event
            {
                juce::MidiFile file;
                juce::MidiMessageSequence track;
                const uint8_t raw[] = { 0xFF, 0x58, 0x81 };
                track.addEvent(juce::MidiMessage(raw, sizeof(raw)), 0.0);
                track.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)80), 0.1);
                file.addTrack(track);
                expect(!MidiTrackMergeEngine::mergeTracks(file, 48000.0).has_value(),
                       "unterminated raw VLQ length in meta event must be rejected");
            }

            // Truncated raw meta payload
            {
                juce::MidiFile file;
                juce::MidiMessageSequence track;
                const uint8_t raw[] = { 0xFF, 0x58, 0x04, 0x01 };
                track.addEvent(juce::MidiMessage(raw, sizeof(raw)), 0.0);
                track.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)80), 0.1);
                file.addTrack(track);
                expect(!MidiTrackMergeEngine::mergeTracks(file, 48000.0).has_value(),
                       "truncated raw meta payload must be rejected");
            }

            // Invalid 0x51 tempo == 0 us
            {
                juce::MidiFile file;
                juce::MidiMessageSequence track;
                const uint8_t raw[] = { 0xFF, 0x51, 0x03, 0x00, 0x00, 0x00 };
                track.addEvent(juce::MidiMessage(raw, sizeof(raw)), 0.0);
                track.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)80), 0.1);
                file.addTrack(track);
                expect(!MidiTrackMergeEngine::mergeTracks(file, 48000.0).has_value(), "0x51 tempo 0 must be rejected");
            }

            // Negative timestamp
            {
                juce::MidiFile file;
                juce::MidiMessageSequence track;
                track.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)80), -0.5);
                track.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)80), 0.1);
                file.addTrack(track);
                expect(!MidiTrackMergeEngine::mergeTracks(file, 48000.0).has_value(),
                       "negative timestamp must be rejected");
            }

            // Nonfinite NaN timestamp
            {
                juce::MidiFile file;
                juce::MidiMessageSequence track;
                track.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)80),
                               std::numeric_limits<double>::quiet_NaN());
                file.addTrack(track);
                expect(!MidiTrackMergeEngine::mergeTracks(file, 48000.0).has_value(), "NaN timestamp must be rejected");
            }

            // Unrepresentable timestamp
            {
                juce::MidiFile file;
                juce::MidiMessageSequence track;
                track.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)80), 1e18);
                file.addTrack(track);
                expect(!MidiTrackMergeEngine::mergeTracks(file, 48000.0).has_value(),
                       "unrepresentable timestamp must be rejected");
            }

            // Take with only t=0 events has lengthSamples >= 1
            {
                juce::MidiFile file;
                juce::MidiMessageSequence track;
                track.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)80), 0.0);
                track.addEvent(juce::MidiMessage::noteOff(1, 60, (juce::uint8)0), 0.0);
                file.addTrack(track);
                auto res = MidiTrackMergeEngine::mergeTracks(file, 48000.0);
                expect(res.has_value(), "t=0 take must merge successfully");
                if (res.has_value()) {
                    expectEquals(res->stats.noteOnCount, 1);
                    expectEquals(res->stats.noteOffCount, 1);
                    expectEquals(res->stats.mergedEventCount, 2);
                    expect(res->take.lengthSamples >= 1, "t=0 take must have lengthSamples >= 1");
                }
            }
        });
        testCase("autoAssignIfSingleChannel preserves existing distinct channels", [&] {
            juce::MidiFile file;

            // Track 0 has notes on Ch 1
            juce::MidiMessageSequence track0;
            track0.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)80), 0.1);
            file.addTrack(track0);

            // Track 1 has notes on Ch 2 (already distinct)
            juce::MidiMessageSequence track1;
            track1.addEvent(juce::MidiMessage::noteOn(2, 64, (juce::uint8)80), 0.2);
            file.addTrack(track1);

            MidiTrackMergeOptions opts;
            opts.channelStrategy = MidiChannelMappingStrategy::autoAssignIfSingleChannel;
            auto res = MidiTrackMergeEngine::mergeTracks(file, 48000.0, opts);
            expect(res.has_value());
            expectEquals(res->take.events[0].message.getChannel(), 1);
            expectEquals(res->take.events[1].message.getChannel(), 2);
        });
    }
};

// =============================================================================

class MultiTrackPerformanceFileRoundTripTest : public juce::UnitTest {
public:
    MultiTrackPerformanceFileRoundTripTest()
        : juce::UnitTest("MultiTrackPerformanceFileRoundTrip", "DevPiano/Recording") {
    }

    void runTest() override {
        using devpiano::recording::importMidiFileWithMetadata;
        using devpiano::recording::loadPerformanceFile;
        using devpiano::recording::loadPerformanceFileMetadata;
        using devpiano::recording::PerformanceFileMetadata;
        using devpiano::recording::savePerformanceFile;

        testCase("multi-track imported take and metadata round-trips via .devpiano file", [&] {
            devpiano::test::ScopedTempDir tempDir("multitrack-test");
            const auto performanceFile = tempDir.getChildFile("multitrack.devpiano");

            auto importRes = importMidiFileWithMetadata(juce::File(getFixturePath("multitrack-basic.mid")), 48000.0);
            expect(importRes.has_value());
            if (!importRes.has_value()) {
                return;
            }
            expect(!importRes->take.isEmpty());

            PerformanceFileMetadata meta;
            meta.title = importRes->metadata.songTitle.isNotEmpty() ? importRes->metadata.songTitle : "Multitrack Song";
            meta.notes = "Multi-track imported take test note";

            expect(savePerformanceFile(importRes->take, performanceFile, meta), "save must succeed");
            expect(performanceFile.existsAsFile());

            auto loadedTake = loadPerformanceFile(performanceFile);
            expect(loadedTake.has_value());
            if (loadedTake.has_value()) {
                expectEquals(loadedTake->sampleRate, importRes->take.sampleRate);
                expectEquals(loadedTake->lengthSamples, importRes->take.lengthSamples);
                expectEquals(loadedTake->events.size(), importRes->take.events.size());

                for (size_t i = 0; i < loadedTake->events.size(); ++i) {
                    expectEquals(loadedTake->events[i].timestampSamples, importRes->take.events[i].timestampSamples);
                    expectEquals(loadedTake->events[i].message.getChannel(),
                                 importRes->take.events[i].message.getChannel());
                    expectEquals(loadedTake->events[i].message.getNoteNumber(),
                                 importRes->take.events[i].message.getNoteNumber());
                }
            }

            auto loadedMeta = loadPerformanceFileMetadata(performanceFile);
            expect(loadedMeta.has_value());
            if (loadedMeta.has_value()) {
                expectEquals(loadedMeta->title, meta.title);
                expectEquals(loadedMeta->notes, meta.notes);
            }
        });
    }
};

static MultiTrackPerformanceFileRoundTripTest multiTrackPerformanceFileRoundTripTest;
static MidiTrackMergeEngineTest midiTrackMergeEngineTest;
