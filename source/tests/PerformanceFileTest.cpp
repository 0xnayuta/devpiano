#include <JuceHeader.h>

#include "Recording/PerformanceFile.h"
#include "Recording/RecordingEngine.h"

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

juce::File makeScratchFile(const juce::String& name) {
    return juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("devpiano_perf_test_" + name);
}

// TemporaryFile names the scratch file "<base>_temp<hex>.<ext>" in the target
// directory; this helper detects leftover scratch files after save.
bool hasTempResidue(const juce::File& targetFile) {
    const auto dir = targetFile.getParentDirectory();
    const auto base = targetFile.getFileNameWithoutExtension();
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
            const auto target = makeScratchFile("roundtrip.devpiano");
            target.deleteFile();

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
                expect(loaded->events[0].message.isNoteOn());
                expect(loaded->events[0].timestampSamples == 0);
                expect(loaded->events[1].timestampSamples == 44100);
                expect(loaded->events[2].type == PerformanceEventType::presetChange);
                expectEquals(static_cast<int>(loaded->events[2].presetId), 3);
            }

            const auto loadedMeta = loadPerformanceFileMetadata(target);
            expect(loadedMeta.has_value());
            if (loadedMeta.has_value()) {
                expectEquals(loadedMeta->title, metadata.title);
                expectEquals(loadedMeta->notes, metadata.notes);
            }

            target.deleteFile();
        });

        testCase("no temporary file residue after successful save", [&] {
            const auto target = makeScratchFile("clean.devpiano");
            target.deleteFile();

            expect(savePerformanceFile(makeTestTake(), target));
            expect(target.existsAsFile());
            expect(!hasTempResidue(target));

            target.deleteFile();
        });

        testCase("overwriting an existing file replaces its content", [&] {
            const auto target = makeScratchFile("overwrite.devpiano");
            target.deleteFile();

            expect(savePerformanceFile(makeTestTake(), target));

            // Second take with a distinct length and a single different event.
            auto secondTake = makeTestTake();
            secondTake.lengthSamples = 44100;
            secondTake.events.clear();
            devpiano::recording::PerformanceEvent otherNote;
            otherNote.timestampSamples = 10;
            otherNote.type = devpiano::recording::PerformanceEventType::midi;
            otherNote.source = devpiano::recording::RecordingEventSource::computerKeyboard;
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
            expect(!hasTempResidue(target));

            target.deleteFile();
        });

        testCase("invalid take is rejected without touching the file system", [&] {
            const auto target = makeScratchFile("invalid.devpiano");
            target.deleteFile();

            devpiano::recording::RecordingTake empty;
            expect(!savePerformanceFile(empty, target));
            expect(!target.existsAsFile());
            expect(!hasTempResidue(target));
        });
    }
};

static PerformanceFileSaveLoadTest performanceFileSaveLoadTest;
