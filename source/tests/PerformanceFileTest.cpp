#include <JuceHeader.h>

#include "Recording/PerformanceFile.h"
#include "Recording/RecordingEngine.h"
#include "TestHelpers.h"

// =============================================================================
// Tests for .devpiano file persistence: save/load round-trip and atomic
// temporary-file writes (AUDIT-SEC-004).
//
// Category "DevPiano/Recording" runs in the default suite (TEST-010): writes
// go to the system temp directory, which is safe under WSL root.
// =============================================================================

namespace {

devpiano::recording::RecordingTake makeTestTake() {
    devpiano::recording::RecordingTake take;
    take.sampleRate = 44100.0;
    take.lengthSamples = 88200;

    devpiano::recording::PerformanceEvent noteOn;
    noteOn.timestampSamples = 0;
    noteOn.type = devpiano::recording::PerformanceEventType::midi;
    noteOn.source = devpiano::recording::RecordingEventSource::computerKeyboard;
    noteOn.message = juce::MidiMessage::noteOn(1, 60, 0.8f);

    devpiano::recording::PerformanceEvent noteOff;
    noteOff.timestampSamples = 44100;
    noteOff.type = devpiano::recording::PerformanceEventType::midi;
    noteOff.source = devpiano::recording::RecordingEventSource::computerKeyboard;
    noteOff.message = juce::MidiMessage::noteOff(1, 60, 0.8f);

    devpiano::recording::PerformanceEvent presetChange;
    presetChange.timestampSamples = 22050;
    presetChange.type = devpiano::recording::PerformanceEventType::presetChange;
    presetChange.presetId = 3;
    presetChange.source = devpiano::recording::RecordingEventSource::computerKeyboard;

    take.events = { noteOn, noteOff, presetChange };
    return take;
}

// TemporaryFile names the scratch file "<base>_temp<hex>.<ext>" in the target
// directory; this helper detects leftover scratch files after save.
bool hasTempResidue(const juce::File& dir, const juce::String& base) {
    return std::ranges::any_of(juce::RangedDirectoryIterator(dir, false, "*", juce::File::findFiles),
                               [&base](const auto& entry) {
                                   return entry.getFile().getFileNameWithoutExtension().startsWith(base + "_temp");
                               });
}
} // namespace

class PerformanceFileSaveLoadTest : public juce::UnitTest {
public:
    PerformanceFileSaveLoadTest()
        : juce::UnitTest("PerformanceFile", "DevPiano/Recording") {
    }

    void runTest() override {
        using namespace devpiano::recording;

        testCase("save then load round-trips take and metadata", [&] {
            devpiano::test::ScopedTempDir tempDir("perf-roundtrip");
            const auto target = tempDir.getChildFile("roundtrip.devpiano");
            const auto take = makeTestTake();
            PerformanceFileMetadata metadata;
            metadata.title = "test title";
            metadata.notes = "some notes";
            metadata.createdAt = "2026-08-16T00:00:00Z";

            expect(savePerformanceFile(take, target, metadata));
            expect(target.existsAsFile());

            const auto loaded = loadPerformanceFile(target);
            expect(loaded.has_value());
            if (loaded.has_value()) {
                expectEquals(loaded->sampleRate, 44100.0);
                expectEquals(loaded->lengthSamples, take.lengthSamples);
                expectEquals(static_cast<int>(loaded->events.size()), 3);
            }

            const auto loadedMeta = loadPerformanceFileMetadata(target);
            expect(loadedMeta.has_value());
            if (loadedMeta.has_value()) {
                expectEquals(loadedMeta->title, metadata.title);
                expectEquals(loadedMeta->notes, metadata.notes);
            }
        });

        testCase("chronological stabilization on deserialization (QUAL-005)", [&] {
            RecordingTake take;
            take.sampleRate = 44100.0;
            take.lengthSamples = 88200;

            PerformanceEvent evLate;
            evLate.timestampSamples = 44100;
            evLate.type = PerformanceEventType::midi;
            evLate.source = RecordingEventSource::computerKeyboard;
            evLate.message = juce::MidiMessage::noteOff(1, 60, 0.8f);

            PerformanceEvent evEarly;
            evEarly.timestampSamples = 0;
            evEarly.type = PerformanceEventType::midi;
            evEarly.source = RecordingEventSource::computerKeyboard;
            evEarly.message = juce::MidiMessage::noteOn(1, 60, 0.8f);

            PerformanceEvent evMid;
            evMid.timestampSamples = 22050;
            evMid.type = PerformanceEventType::presetChange;
            evMid.presetId = 3;

            take.events = { evLate, evEarly, evMid };

            const auto json = serialiseTakeToJson(take);
            const auto loaded = deserialiseTakeFromJson(json);
            expect(loaded.has_value());
            if (loaded.has_value()) {
                expectEquals(static_cast<int>(loaded->events.size()), 3);
                expectEquals(loaded->events[0].timestampSamples, static_cast<std::int64_t>(0));
                expect(loaded->events[0].message.isNoteOn());
                expectEquals(loaded->events[1].timestampSamples, static_cast<std::int64_t>(22050));
                expect(loaded->events[1].type == PerformanceEventType::presetChange);
                expectEquals(loaded->events[2].timestampSamples, static_cast<std::int64_t>(44100));
                expect(loaded->events[2].message.isNoteOff());
            }
        });

        testCase("equal-sample events preserve stable insertion order (QUAL-005)", [&] {
            RecordingTake take;
            take.sampleRate = 48000.0;
            take.lengthSamples = 96000;

            PerformanceEvent ev1;
            ev1.timestampSamples = 1000;
            ev1.type = PerformanceEventType::midi;
            ev1.source = RecordingEventSource::computerKeyboard;
            ev1.message = juce::MidiMessage::noteOn(1, 60, 0.8f);

            PerformanceEvent ev2;
            ev2.timestampSamples = 1000;
            ev2.type = PerformanceEventType::midi;
            ev2.source = RecordingEventSource::computerKeyboard;
            ev2.message = juce::MidiMessage::noteOn(1, 64, 0.8f);

            PerformanceEvent ev3;
            ev3.timestampSamples = 1000;
            ev3.type = PerformanceEventType::midi;
            ev3.source = RecordingEventSource::computerKeyboard;
            ev3.message = juce::MidiMessage::noteOn(1, 67, 0.8f);

            take.events = { ev1, ev2, ev3 };

            const auto json = serialiseTakeToJson(take);
            const auto loaded = deserialiseTakeFromJson(json);
            expect(loaded.has_value());
            if (loaded.has_value()) {
                expectEquals(static_cast<int>(loaded->events.size()), 3);
                expectEquals(loaded->events[0].timestampSamples, static_cast<std::int64_t>(1000));
                expectEquals(loaded->events[0].message.getNoteNumber(), 60);
                expectEquals(loaded->events[1].timestampSamples, static_cast<std::int64_t>(1000));
                expectEquals(loaded->events[1].message.getNoteNumber(), 64);
                expectEquals(loaded->events[2].timestampSamples, static_cast<std::int64_t>(1000));
                expectEquals(loaded->events[2].message.getNoteNumber(), 67);
            }
        });

        testCase("no temporary file residue after successful save", [&] {
            devpiano::test::ScopedTempDir tempDir("perf-clean");
            const auto target = tempDir.getChildFile("clean.devpiano");
            expect(savePerformanceFile(makeTestTake(), target));
            expect(target.existsAsFile());
            expect(!hasTempResidue(tempDir.get(), "clean"));
        });

        testCase("overwriting an existing file replaces its content", [&] {
            devpiano::test::ScopedTempDir tempDir("perf-overwrite");
            const auto target = tempDir.getChildFile("overwrite.devpiano");
            expect(savePerformanceFile(makeTestTake(), target));

            auto secondTake = makeTestTake();
            secondTake.lengthSamples = 44100;
            secondTake.events.clear();
            PerformanceEvent otherNote;
            otherNote.timestampSamples = 10;
            otherNote.type = PerformanceEventType::midi;
            otherNote.source = RecordingEventSource::computerKeyboard;
            otherNote.message = juce::MidiMessage::noteOn(2, 72, 1.0f);
            secondTake.events.push_back(otherNote);

            expect(savePerformanceFile(secondTake, target));

            const auto loaded = loadPerformanceFile(target);
            expect(loaded.has_value());
            if (loaded.has_value()) {
                expect(loaded->lengthSamples == 44100);
                expectEquals(static_cast<int>(loaded->events.size()), 1);
                expectEquals(loaded->events[0].message.getNoteNumber(), 72);
                expectEquals(loaded->events[0].message.getChannel(), 2);
            }
            expect(!hasTempResidue(tempDir.get(), "overwrite"));
        });

        testCase("invalid take is rejected without touching the file system", [&] {
            devpiano::test::ScopedTempDir tempDir("perf-invalid");
            const auto target = tempDir.getChildFile("invalid.devpiano");
            RecordingTake empty;
            expect(!savePerformanceFile(empty, target));
            expect(!target.existsAsFile());
            expect(!hasTempResidue(tempDir.get(), "invalid"));

            RecordingTake badRate = makeTestTake();
            badRate.sampleRate = 4000.0;
            expect(!savePerformanceFile(badRate, target));
            expect(!target.existsAsFile());

            RecordingTake badTimestamp = makeTestTake();
            badTimestamp.events[0].timestampSamples = badTimestamp.lengthSamples + 1;
            expect(!savePerformanceFile(badTimestamp, target));
            expect(!target.existsAsFile());
        });

        testCase("SEC-003: reject malicious base64 length prefix", [&] {
            const auto helper = [](const juce::String& b64) {
                const auto json
                    = R"({"version":2,"format":"devpiano-performance","sampleRate":44100.0,"lengthSamples":88200,"events":[{"timestampSamples":0,"type":"midi","source":"computerKeyboard","midiData":")"
                    + b64 + R"("}]})";
                return deserialiseTakeFromJson(json);
            };

            expect(!helper("-1.AAAA").has_value(), "negative prefix rejected");
            expect(!helper("abc.AAAA").has_value(), "non-numeric prefix rejected");
            expect(!helper("20000000.AAAA").has_value(), "huge prefix exceeding budget rejected");
            expect(!helper("1234").has_value(), "missing dot separator rejected");
            expect(!helper("03.AAAA").has_value(), "leading zero prefix rejected");
            expect(!helper("0.").has_value(), "zero length prefix rejected");
            expect(!helper(".AAAA").has_value(), "empty prefix rejected");
        });

        testCase("SEC-003: reject exact encoded size mismatch, alphabet violations, and invalid padding bits", [&] {
            const auto helper = [](const juce::String& b64) {
                const auto json
                    = R"({"version":2,"format":"devpiano-performance","sampleRate":44100.0,"lengthSamples":88200,"events":[{"timestampSamples":0,"type":"midi","source":"computerKeyboard","midiData":")"
                    + b64 + R"("}]})";
                return deserialiseTakeFromJson(json);
            };

            expect(!helper("3.AAA").has_value(), "truncated payload rejected");
            expect(!helper("3.AAAAA").has_value(), "extra trailing payload rejected");
            expect(!helper("3.AA=A").has_value(), "invalid alphabet '=' rejected");
            expect(!helper("3.AA,A").has_value(), "invalid alphabet ',' rejected");
            expect(!helper("3.AA/A").has_value(), "invalid alphabet '/' rejected");
            expect(!helper("3.AA A").has_value(), "whitespace in payload rejected");

            // 1 byte -> 2 base64 chars (rem = 8 % 6 = 2). Last char must have upper 4 bits zero (val < 4).
            // 'D' has value 4, so upper bits are non-zero (4 >> 2 != 0).
            expect(!helper("1.AD").has_value(), "dirty padding bits rejected");
        });

        testCase("SEC-003: reject malformed or truncated MIDI frames before MidiMessage construction", [&] {
            const auto helper = [](const uint8_t* bytes, size_t size) {
                juce::MemoryBlock mb(bytes, size);
                const auto json
                    = R"({"version":2,"format":"devpiano-performance","sampleRate":44100.0,"lengthSamples":88200,"events":[{"timestampSamples":0,"type":"midi","source":"computerKeyboard","midiData":")"
                    + mb.toBase64Encoding() + R"("}]})";
                return deserialiseTakeFromJson(json);
            };

            // Truncated NoteOn (2 bytes instead of 3)
            const uint8_t truncatedNoteOn[] = { 0x90, 0x3C };
            expect(!helper(truncatedNoteOn, sizeof(truncatedNoteOn)).has_value(), "truncated noteOn rejected");

            // Channel voice message with high bit set in data byte
            const uint8_t badDataByte[] = { 0x90, 0x85, 0x40 };
            expect(!helper(badDataByte, sizeof(badDataByte)).has_value(), "high bit in data byte rejected");

            // Valid raw SysEx F0...F7 accepted
            const uint8_t validSysEx[] = { 0xF0, 0x7E, 0x01, 0xF7 };
            expect(helper(validSysEx, sizeof(validSysEx)).has_value(), "valid raw SysEx accepted");

            // Truncated SysEx (no trailing 0xF7)
            const uint8_t truncatedSysEx[] = { 0xF0, 0x7E, 0x01 };
            expect(!helper(truncatedSysEx, sizeof(truncatedSysEx)).has_value(), "truncated SysEx rejected");

            // Legal 1-byte 0xFF System Reset accepted
            const uint8_t systemReset[] = { 0xFF };
            expect(helper(systemReset, sizeof(systemReset)).has_value(), "1-byte 0xFF system reset accepted");

            // Truncated Meta event (0xFF with no VLQ length)
            const uint8_t truncatedMeta[] = { 0xFF, 0x58 };
            expect(!helper(truncatedMeta, sizeof(truncatedMeta)).has_value(), "truncated meta event rejected");

            // Time signature meta-event with denominator exponent 31 (shift overflow / UB)
            const uint8_t badTimeSigExp[] = { 0xFF, 0x58, 0x04, 0x04, 31, 0x18, 0x08 };
            expect(!helper(badTimeSigExp, sizeof(badTimeSigExp)).has_value(), "time-sig exponent 31 rejected");

            // Time signature meta-event with invalid fixed length 5 instead of 4
            const uint8_t badTimeSigLen[] = { 0xFF, 0x58, 0x05, 0x04, 0x02, 0x18, 0x08, 0x00 };
            expect(!helper(badTimeSigLen, sizeof(badTimeSigLen)).has_value(), "time-sig invalid length rejected");

            // Tempo meta-event with invalid fixed length 2 instead of 3
            const uint8_t badTempoLen[] = { 0xFF, 0x51, 0x02, 0x07, 0xA1 };
            expect(!helper(badTempoLen, sizeof(badTempoLen)).has_value(), "tempo invalid length rejected");
        });

        testCase("SEC-004: reject unsupported sample rates, unrepresentable length, and invalid timestamps", [&] {
            const auto helper = [](const juce::String& sr, const juce::String& len, const juce::String& ts) {
                const auto noteB64
                    = juce::MemoryBlock(juce::MidiMessage::noteOn(1, 60, 0.8f).getRawData(), 3).toBase64Encoding();
                const auto json = R"({"version":2,"format":"devpiano-performance","sampleRate":)" + sr
                    + R"(,"lengthSamples":)" + len + R"(,"events":[{"timestampSamples":)" + ts
                    + R"(,"type":"midi","source":"computerKeyboard","midiData":")" + noteB64 + R"("}]})";
                return deserialiseTakeFromJson(json);
            };

            expect(!helper("0.0", "88200", "0").has_value(), "zero sample rate rejected");
            expect(!helper("-44100.0", "88200", "0").has_value(), "negative sample rate rejected");
            expect(!helper("1000.0", "88200", "0").has_value(), "sample rate < 8000 rejected");
            expect(!helper("500000.0", "88200", "0").has_value(), "sample rate > 384000 rejected");
            expect(!helper("\"44100\"", "88200", "0").has_value(), "string sample rate rejected");

            expect(!helper("44100.0", "-10", "0").has_value(), "negative length rejected");
            expect(!helper("44100.0", "88200.5", "0").has_value(), "fractional length rejected");
            expect(!helper("44100.0", "\"88200\"", "0").has_value(), "string length rejected");

            expect(!helper("44100.0", "88200", "-1").has_value(), "negative timestamp rejected");
            expect(!helper("44100.0", "88200", "10.5").has_value(), "fractional timestamp rejected");
            expect(!helper("44100.0", "88200", "\"10\"").has_value(), "string timestamp rejected");
            expect(!helper("44100.0", "88200", "88201").has_value(), "timestamp > length rejected");
            expect(helper("44100.0", "88200", "88200").has_value(), "timestamp == length accepted (legacy boundary)");
        });

        testCase("legacy v1 missing type and v2 preset event support", [&] {
            const auto noteB64
                = juce::MemoryBlock(juce::MidiMessage::noteOn(1, 60, 0.8f).getRawData(), 3).toBase64Encoding();

            // Legacy v1 JSON: missing "type" field in events, missing metadata block
            const auto v1Json
                = R"({"version":1,"format":"devpiano-performance","sampleRate":44100.0,"lengthSamples":88200,"events":[{"timestampSamples":0,"source":"computerKeyboard","midiData":")"
                + noteB64 + R"("}]})";
            const auto v1Loaded = deserialiseTakeFromJson(v1Json);
            expect(v1Loaded.has_value());
            if (v1Loaded.has_value()) {
                expectEquals(static_cast<int>(v1Loaded->events.size()), 1);
                expect(v1Loaded->events[0].type == PerformanceEventType::midi);
                expect(v1Loaded->events[0].message.isNoteOn());
            }

            // v2 presetChange event: valid presetId in [0, 255]
            const auto v2Json
                = R"({"version":2,"format":"devpiano-performance","sampleRate":44100.0,"lengthSamples":88200,"events":[{"timestampSamples":100,"type":"presetChange","presetId":5}]})";
            const auto v2Loaded = deserialiseTakeFromJson(v2Json);
            expect(v2Loaded.has_value());
            if (v2Loaded.has_value()) {
                expectEquals(static_cast<int>(v2Loaded->events.size()), 1);
                expect(v2Loaded->events[0].type == PerformanceEventType::presetChange);
                expectEquals(static_cast<int>(v2Loaded->events[0].presetId), 5);
            }

            // v2 presetChange with invalid presetId (> 255 or string) rejected
            const auto v2BadId
                = R"({"version":2,"format":"devpiano-performance","sampleRate":44100.0,"lengthSamples":88200,"events":[{"timestampSamples":100,"type":"presetChange","presetId":300}]})";
            expect(!deserialiseTakeFromJson(v2BadId).has_value());
        });

        testCase("file read errors and size budget bounds", [&] {
            devpiano::test::ScopedTempDir tempDir("perf-fileguards");

            // Non-existent file
            const auto missing = tempDir.getChildFile("nonexistent.devpiano");
            expect(!loadPerformanceFile(missing).has_value());
            expect(!loadPerformanceFileMetadata(missing).has_value());

            // Empty file (0 bytes)
            const auto emptyFile = tempDir.getChildFile("empty.devpiano");
            emptyFile.create();
            expect(!loadPerformanceFile(emptyFile).has_value());
            expect(!loadPerformanceFileMetadata(emptyFile).has_value());
        });
    }
};

static PerformanceFileSaveLoadTest performanceFileSaveLoadTest;
