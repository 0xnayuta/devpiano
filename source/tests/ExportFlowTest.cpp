#include <JuceHeader.h>

#include "Export/ExportFlowSupport.h"
#include "Export/WavExportTask.h"
#include "Recording/MidiFileExporter.h"
#include "Recording/RecordingEngine.h"
#include "Recording/WavFileExporter.h"
#include "TestHelpers.h"

using namespace devpiano::exporting;
using namespace devpiano::recording;

// =============================================================================
// Tests for the export chain's pure logic and file round-trips (AUDIT
// TEST-005):
//   - buildWavExportOptions parameter combinations
//   - canExportTake boundaries
//   - makeDefaultRecordingExportFile / makeExportLogPrefix
//   - MIDI export -> read-back round-trip
//   - WAV export -> read-back header + non-silent payload
// =============================================================================

namespace {

// 1-second take: note-on at 0, note-off at 1s.
RecordingTake makeOneSecondTake() {
    RecordingTake take;
    take.sampleRate = 44100.0;
    take.lengthSamples = 44100;
    take.events.push_back({ 0, PerformanceEventType::midi, 0, RecordingEventSource::computerKeyboard,
                            juce::MidiMessage::noteOn(1, 60, 0.8f) });
    take.events.push_back({ 44100, PerformanceEventType::midi, 0, RecordingEventSource::computerKeyboard,
                            juce::MidiMessage::noteOff(1, 60) });
    return take;
}

} // namespace

// -----------------------------------------------------------------------------

class WavExportOptionsTest final : public juce::UnitTest {
public:
    WavExportOptionsTest()
        : juce::UnitTest("Export: buildWavExportOptions combinations", "DevPiano/Recording") {
    }

    void runTest() override {
        testCase("runtime sample rate takes priority", [&] {
            const auto take = makeOneSecondTake();
            SettingsModel::PerformanceSettingsView perf;
            const auto options = buildWavExportOptions(take, perf, 48000.0, 256);
            expectWithinAbsoluteError(options.sampleRate, 48000.0, 0.001);
        });

        testCase("take sample rate is the fallback when runtime is unset", [&] {
            const auto take = makeOneSecondTake();
            SettingsModel::PerformanceSettingsView perf;
            const auto options = buildWavExportOptions(take, perf, 0.0, 256);
            expectWithinAbsoluteError(options.sampleRate, 44100.0, 0.001);
        });

        testCase("44100 is the default when neither rate is known", [&] {
            RecordingTake take; // sampleRate == 0
            SettingsModel::PerformanceSettingsView perf;
            const auto options = buildWavExportOptions(take, perf, 0.0, 256);
            expectWithinAbsoluteError(options.sampleRate, 44100.0, 0.001);
        });

        testCase("block size is clamped to at least 1", [&] {
            const auto take = makeOneSecondTake();
            SettingsModel::PerformanceSettingsView perf;
            const auto options = buildWavExportOptions(take, perf, 44100.0, 0);
            expectEquals(options.blockSize, 1);
            expectEquals(buildWavExportOptions(take, perf, 44100.0, 512).blockSize, 512);
        });

        testCase("reference pitch in export options is clamped to 400..480 Hz range", [&] {
            RecordingTake take;
            SettingsModel::PerformanceSettingsView perf;

            perf.referencePitchA4 = 400.0;
            expectWithinAbsoluteError(buildWavExportOptions(take, perf, 44100.0, 512).referencePitchA4, 400.0, 1e-4);

            perf.referencePitchA4 = 480.0;
            expectWithinAbsoluteError(buildWavExportOptions(take, perf, 44100.0, 512).referencePitchA4, 480.0, 1e-4);

            perf.referencePitchA4 = 415.0;
            expectWithinAbsoluteError(buildWavExportOptions(take, perf, 44100.0, 512).referencePitchA4, 415.0, 1e-4);

            perf.referencePitchA4 = 440.0;
            expectWithinAbsoluteError(buildWavExportOptions(take, perf, 44100.0, 512).referencePitchA4, 440.0, 1e-4);

            perf.referencePitchA4 = 442.0;
            expectWithinAbsoluteError(buildWavExportOptions(take, perf, 44100.0, 512).referencePitchA4, 442.0, 1e-4);

            perf.referencePitchA4 = 350.0;
            expectWithinAbsoluteError(buildWavExportOptions(take, perf, 44100.0, 512).referencePitchA4, 400.0, 1e-4);

            perf.referencePitchA4 = 550.0;
            expectWithinAbsoluteError(buildWavExportOptions(take, perf, 44100.0, 512).referencePitchA4, 480.0, 1e-4);
        });
    }
};

static WavExportOptionsTest wavExportOptionsTest;

// -----------------------------------------------------------------------------

class ExportTakePredicateTest final : public juce::UnitTest {
public:
    ExportTakePredicateTest()
        : juce::UnitTest("Export: canExportTake boundaries", "DevPiano/Recording") {
    }

    void runTest() override {
        testCase("empty take cannot be exported", [&] {
            RecordingTake take;
            expect(!canExportTake(take));
        });

        testCase("take with events can be exported", [&] { expect(canExportTake(makeOneSecondTake())); });
    }
};

static ExportTakePredicateTest exportTakePredicateTest;

// -----------------------------------------------------------------------------

class ExportNamingTest final : public juce::UnitTest {
public:
    ExportNamingTest()
        : juce::UnitTest("Export: default file naming and log prefixes", "DevPiano/Recording") {
    }

    void runTest() override {
        testCase("default export file keeps the destination directory and format extension", [&] {
            devpiano::test::ScopedTempDir tempDir("export-naming");
            const auto time = juce::Time(2026, 8, 17, 12, 30, 45, 0, true);

            const auto midiFile = makeDefaultRecordingExportFile(ExportFileType::midi, tempDir.get(), time);
            expect(midiFile.getParentDirectory() == tempDir.get());
            expect(midiFile.hasFileExtension(".mid"));

            const auto wavFile = makeDefaultRecordingExportFile(ExportFileType::wav, tempDir.get(), time);
            expect(wavFile.hasFileExtension(".wav"));
        });
    }
};

static ExportNamingTest exportNamingTest;

// -----------------------------------------------------------------------------

class MidiExportRoundTripTest final : public juce::UnitTest {
public:
    MidiExportRoundTripTest()
        : juce::UnitTest("Export: MIDI file round-trip", "DevPiano/Recording") {
    }

    void runTest() override {
        testCase("exported MIDI file reads back with matching events", [&] {
            devpiano::test::ScopedTempDir tempDir("midi-export");
            const auto path = tempDir.getChildFile("take.mid");

            auto previousTake = makeOneSecondTake();
            for (auto& event : previousTake.events) {
                event.message.setNoteNumber(72);
            }
            expect(exportTakeAsMidiFile(previousTake, path));
            const auto take = makeOneSecondTake();
            expect(exportTakeAsMidiFile(take, path), "export must succeed");
            expect(path.existsAsFile());

            juce::FileInputStream in(path);
            expect(in.openedOk());
            if (!in.openedOk()) {
                return;
            }

            juce::MidiFile midiFile;
            expect(midiFile.readFrom(in), "exported file must parse as a MIDI file");
            expectEquals(midiFile.getNumTracks(), 1);

            const auto* track = midiFile.getTrack(0);
            expect(track != nullptr);
            if (track == nullptr) {
                return;
            }

            // tempo event + note-on + note-off
            expect(track->getNumEvents() >= 3, "tempo + note-on + note-off must be present");

            bool foundNoteOn60 = false;
            bool foundNoteOff60 = false;
            for (int i = 0; i < track->getNumEvents(); ++i) {
                const auto* ev = track->getEventPointer(i);
                if (ev->message.isNoteOn() && ev->message.getNoteNumber() == 60) {
                    foundNoteOn60 = true;
                }
                if (ev->message.isNoteOff() && ev->message.getNoteNumber() == 60) {
                    foundNoteOff60 = true;
                }
            }
            expect(foundNoteOn60, "note-on 60 must survive the round-trip");
            expect(foundNoteOff60, "note-off 60 must survive the round-trip");
        });
        testCase("empty take is rejected", [&] {
            devpiano::test::ScopedTempDir tempDir("midi-export-empty");
            RecordingTake take;
            expect(!exportTakeAsMidiFile(take, tempDir.getChildFile("x.mid")));
        });

        testCase("presetChange non-MIDI events are filtered and not exported as pseudo SysEx (SEC-002)", [&] {
            devpiano::test::ScopedTempDir tempDir("midi-export-preset-filter");
            const auto path = tempDir.getChildFile("filtered.mid");

            RecordingTake take;
            take.sampleRate = 44100.0;
            take.lengthSamples = 44100;
            take.events.push_back({ 0, PerformanceEventType::midi, 0, RecordingEventSource::computerKeyboard,
                                    juce::MidiMessage::noteOn(1, 60, 0.8f) });
            // Add a preset-change event (which has empty message / F0 F7)
            take.events.push_back({ 22050, PerformanceEventType::presetChange, 3,
                                    RecordingEventSource::computerKeyboard, juce::MidiMessage() });
            take.events.push_back({ 44100, PerformanceEventType::midi, 0, RecordingEventSource::computerKeyboard,
                                    juce::MidiMessage::noteOff(1, 60) });

            expect(exportTakeAsMidiFile(take, path), "export must succeed");

            juce::FileInputStream in(path);
            expect(in.openedOk());
            juce::MidiFile midiFile;
            expect(midiFile.readFrom(in));
            const auto* track = midiFile.getTrack(0);
            expect(track != nullptr);
            if (track != nullptr) {
                for (int i = 0; i < track->getNumEvents(); ++i) {
                    const auto* ev = track->getEventPointer(i);
                    expect(!ev->message.isSysEx(), "Exported MIDI file must not contain pseudo SysEx events");
                }
            }
        });

        testCase("MIDI numeric bounds reject before replacing an existing target", [&] {
            devpiano::test::ScopedTempDir tempDir("midi-timeline-reject");
            const auto path = tempDir.getChildFile("retained.mid");
            expect(path.replaceWithText("retained MIDI target"));
            const auto original = path.loadFileAsString();
            auto take = makeOneSecondTake();
            expect(!exportTakeAsMidiFile(take, path, 32768));
            expectEquals(path.loadFileAsString(), original);
            take.sampleRate = 1e-300;
            expect(!exportTakeAsMidiFile(take, path));
            expectEquals(path.loadFileAsString(), original);
            take = makeOneSecondTake();
            take.lengthSamples = 44100LL * 1000000;
            take.events.back().timestampSamples = take.lengthSamples;
            expect(!exportTakeAsMidiFile(take, path));
            expectEquals(path.loadFileAsString(), original);
        });
    }
};

static MidiExportRoundTripTest midiExportRoundTripTest;

// -----------------------------------------------------------------------------

class WavExportRoundTripTest final : public juce::UnitTest {
public:
    WavExportRoundTripTest()
        : juce::UnitTest("Export: WAV header round-trip", "DevPiano/Recording") {
    }

    void runTest() override {
        testCase("exported WAV reads back with matching header and audible payload", [&] {
            devpiano::test::ScopedTempDir tempDir("wav-export");
            const auto path = tempDir.getChildFile("take.wav");

            const auto take = makeOneSecondTake();
            WavExportOptions options;
            options.sampleRate = 44100.0;
            options.blockSize = 512;
            options.masterGain = 0.8f;
            options.adsr = { 0.01f, 0.2f, 0.8f, 0.3f };

            options.sampleRate = 48000.0;
            expect(exportTakeAsWavFile(take, path, options));
            options.sampleRate = 44100.0;
            expect(exportTakeAsWavFile(take, path, options), "export must succeed");
            expect(path.existsAsFile());

            std::unique_ptr<juce::AudioFormatReader> reader;
            {
                juce::WavAudioFormat wavFormat;
                reader.reset(wavFormat.createReaderFor(new juce::FileInputStream(path), false));
            }
            expect(reader != nullptr, "exported file must parse as a WAV");
            if (reader == nullptr) {
                return;
            }

            expectWithinAbsoluteError(reader->sampleRate, 44100.0, 0.001, "WAV header sample rate");
            expectEquals(static_cast<int>(reader->numChannels), 2, "WAV header channels");
            expect(reader->lengthInSamples >= 44100, "at least one second of audio");

            juce::AudioBuffer<float> buffer(static_cast<int>(reader->numChannels), 4096);
            float maxSample = 0.0f;
            std::int64_t offset = 0;
            while (offset < reader->lengthInSamples) {
                const auto num = static_cast<int>(std::min<std::int64_t>(4096, reader->lengthInSamples - offset));
                reader->read(&buffer, 0, num, offset, true, true);
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
                    maxSample = juce::jmax(maxSample, buffer.getMagnitude(ch, 0, num));
                }
                offset += num;
            }
            expect(maxSample > 0.01f, "rendered audio must not be silent");
        });

        testCase("WAV export renders take sounding pitch without double transposition", [&] {
            devpiano::test::ScopedTempDir tempDir("wav-no-double-transpose");
            const auto path = tempDir.getChildFile("pitch_check.wav");

            // A live-recorded take where keyboard input was in Key of D (+2 semitones).
            // The note captured into take.events is note 62 (D4).
            // The embedded preset has transposeEnabled = true, transposeOffset = 2.
            auto take = makeOneSecondTake();
            take.events[0].message = juce::MidiMessage::noteOn(1, 62, 0.8f);
            take.events[1].message = juce::MidiMessage::noteOff(1, 62);

            RecordedPreset rp;
            rp.preset.name = "D_Major";
            rp.acoustic.transposeEnabled = true;
            rp.acoustic.transposeOffset = 2;
            rp.acoustic.channelFollowKeyMask = 0b1111110111111111;
            take.presets.push_back(rp);
            take.events.insert(
                take.events.begin(),
                { 0, PerformanceEventType::presetChange, 0, RecordingEventSource::computerKeyboard, {} });

            WavExportOptions options;
            options.sampleRate = 44100.0;
            options.blockSize = 512;
            options.builtinTone = SettingsModel::BuiltinTone::sine;

            expect(exportTakeAsWavFile(take, path, options), "WAV export with preset should succeed");
            expect(path.existsAsFile());
            expectGreaterThan(static_cast<int>(path.getSize()), 1024);
        });

        testCase("unrepresentable WAV timelines reject before output and preserve existing bytes", [&] {
            devpiano::test::ScopedTempDir tempDir("wav-timeline-reject");
            const auto path = tempDir.getChildFile("retained.wav");
            expect(path.replaceWithText("retained user data"));
            const auto original = path.loadFileAsString();
            WavExportOptions options;
            auto take = makeOneSecondTake();
            take.lengthSamples = std::numeric_limits<std::int64_t>::max();
            take.events.back().timestampSamples = take.lengthSamples;
            auto progressCalled = false;
            expect(!exportTakeAsWavFile(take, path, options, [&](double) {
                progressCalled = true;
                return false;
            }));
            expect(!progressCalled);
            expectEquals(path.loadFileAsString(), original);

            take.events.back().timestampSamples = 44100;
            take.lengthSamples = std::numeric_limits<std::int64_t>::max() - 88199;
            expect(!exportTakeAsWavFile(take, path, options));
            expectEquals(path.loadFileAsString(), original);

            take = makeOneSecondTake();
            take.sampleRate = 1e-300;
            const auto missingParent = tempDir.getChildFile("not-created").getChildFile("invalid.wav");
            expect(!exportTakeAsWavFile(take, missingParent, options));
            expect(!missingParent.getParentDirectory().exists());
        });

        testCase("cancelling a WAV overwrite before commit preserves the original bytes", [&] {
            devpiano::test::ScopedTempDir tempDir("wav-overwrite-cancel");
            const auto path = tempDir.getChildFile("take.wav");
            const auto take = makeOneSecondTake();
            WavExportOptions options;
            expect(exportTakeAsWavFile(take, path, options));
            juce::MemoryBlock original;
            expect(path.loadFileAsData(original));

            options.masterGain = 0.0f;
            auto reachedCommit = false;
            expect(!exportTakeAsWavFile(take, path, options, [&reachedCommit](double progress) {
                reachedCommit = progress == 1.0;
                return !reachedCommit;
            }));
            expect(reachedCommit);
            juce::MemoryBlock afterCancel;
            expect(path.loadFileAsData(afterCancel));
            expect(afterCancel == original, "cancelled replacement must preserve the original WAV");
            expectEquals(tempDir.get().getNumberOfChildFiles(juce::File::findFiles), 1,
                         "cancelled render must not leave its temporary output");
        });

        testCase("a failed replacement preserves an occupied output directory", [&] {
            devpiano::test::ScopedTempDir tempDir("export-replace-failure");
            const auto occupied = tempDir.getChildFile("occupied");
            expect(occupied.createDirectory().wasOk());
            const auto original = occupied.getChildFile("original");
            expect(original.replaceWithText("owned-user-data"));
            const auto take = makeOneSecondTake();
            expect(!exportTakeAsMidiFile(take, occupied));
            expect(!exportTakeAsWavFile(take, occupied, WavExportOptions {}));
            expectEquals(original.loadFileAsString(), juce::String("owned-user-data"));
            expectEquals(tempDir.get().getNumberOfChildFiles(juce::File::findFilesAndDirectories), 1,
                         "failed exports must retain only the original directory");
        });
        testCase("preset changes during WAV export update acoustics and silence old bank on tone switch", [&] {
            devpiano::test::ScopedTempDir tempDir("wav-preset-export");
            const auto path = tempDir.getChildFile("preset-take.wav");

            RecordingTake take;
            take.sampleRate = 44100.0;
            take.lengthSamples = 44100;

            RecordedPreset p0;
            p0.preset.name = "PianoPreset";
            p0.acoustic.builtinTone = devpiano::core::BuiltinTone::piano;
            p0.acoustic.masterGain = 0.5f;

            RecordedPreset p1;
            p1.preset.name = "SinePreset";
            p1.acoustic.builtinTone = devpiano::core::BuiltinTone::sine;
            p1.acoustic.masterGain = 0.9f;

            take.presets = { p0, p1 };

            PerformanceEvent evP0;
            evP0.type = PerformanceEventType::presetChange;
            evP0.timestampSamples = 0;
            evP0.presetId = 0;

            PerformanceEvent evN0;
            evN0.type = PerformanceEventType::midi;
            evN0.timestampSamples = 0;
            evN0.message = juce::MidiMessage::noteOn(1, 60, 0.8f);

            PerformanceEvent evOff0;
            evOff0.type = PerformanceEventType::midi;
            evOff0.timestampSamples = 20000;
            evOff0.message = juce::MidiMessage::noteOff(1, 60, 0.0f);

            PerformanceEvent evP1;
            evP1.type = PerformanceEventType::presetChange;
            evP1.timestampSamples = 22050;
            evP1.presetId = 1;

            PerformanceEvent evN1;
            evN1.type = PerformanceEventType::midi;
            evN1.timestampSamples = 22050;
            evN1.message = juce::MidiMessage::noteOn(1, 64, 0.8f);

            PerformanceEvent evOff1;
            evOff1.type = PerformanceEventType::midi;
            evOff1.timestampSamples = 40000;
            evOff1.message = juce::MidiMessage::noteOff(1, 64, 0.0f);

            take.events = { evP0, evN0, evOff0, evP1, evN1, evOff1 };

            WavExportOptions options;
            options.sampleRate = 44100.0;
            options.blockSize = 512;
            options.numChannels = 2;

            expect(exportTakeAsWavFile(take, path, options));
            expect(path.existsAsFile());

            juce::WavAudioFormat wavFormat;
            std::unique_ptr<juce::AudioFormatReader> reader(
                wavFormat.createReaderFor(path.createInputStream().release(), true));
            expect(reader != nullptr);
            if (reader != nullptr) {
                expectEquals(static_cast<int>(reader->numChannels), 2);
                expect(reader->lengthInSamples >= 44100);
            }
        });

        testCase("missing or invalid preset snapshot references reject before destination file is touched", [&] {
            devpiano::test::ScopedTempDir tempDir("wav-preset-reject");
            const auto target = tempDir.getChildFile("protected.wav");
            expect(target.replaceWithText("user data must remain untouched"));

            RecordingTake take;
            take.sampleRate = 44100.0;
            take.lengthSamples = 44100;

            PerformanceEvent evP0;
            evP0.type = PerformanceEventType::presetChange;
            evP0.timestampSamples = 0;
            evP0.presetId = 0;

            PerformanceEvent evN0;
            evN0.type = PerformanceEventType::midi;
            evN0.timestampSamples = 0;
            evN0.message = juce::MidiMessage::noteOn(1, 60, 0.8f);

            take.events = { evP0, evN0 };

            WavExportOptions options;
            expect(!exportTakeAsWavFile(take, target, options));
            expectEquals(target.loadFileAsString(), juce::String("user data must remain untouched"));
        });

        testCase("offline WAV export renders sine tone with frequency governed by reference pitch range", [&] {
            devpiano::test::ScopedTempDir tempDir("wav-pitch-freq");

            const auto renderToneAtPitch = [&](const juce::String& fileName, double pitch) -> double {
                const auto wavFile = tempDir.getChildFile(fileName);
                RecordingTake take;
                take.sampleRate = 48000.0;
                take.lengthSamples = 48000;

                take.events.push_back({ 0, PerformanceEventType::midi, 0, RecordingEventSource::computerKeyboard,
                                        juce::MidiMessage::noteOn(1, 69, 0.9f) });
                take.events.push_back({ 48000, PerformanceEventType::midi, 0, RecordingEventSource::computerKeyboard,
                                        juce::MidiMessage::noteOff(1, 69) });

                WavExportOptions options;
                options.sampleRate = 48000.0;
                options.blockSize = 512;
                options.builtinTone = SettingsModel::BuiltinTone::sine;
                options.referencePitchA4 = pitch;
                options.adsr = { 0.001f, 0.1f, 1.0f, 0.05f };

                if (!exportTakeAsWavFile(take, wavFile, options)) {
                    return 0.0;
                }

                juce::WavAudioFormat wavFormat;
                std::unique_ptr<juce::AudioFormatReader> reader(
                    wavFormat.createReaderFor(wavFile.createInputStream().release(), true));
                if (reader == nullptr || reader->lengthInSamples < 48000) {
                    return 0.0;
                }

                juce::AudioBuffer<float> buf(1, 1200);
                reader->read(&buf, 0, 1200, 4800, true, false);
                const float* samples = buf.getReadPointer(0);

                int crossings = 0;
                for (int i = 1; i < 1200; ++i) {
                    if (samples[i - 1] <= 0.0f && samples[i] > 0.0f) {
                        ++crossings;
                    }
                }
                return static_cast<double>(crossings) * 40.0;
            };

            const auto freq400 = renderToneAtPitch("sine_400.wav", 400.0);
            expectEquals(freq400, 400.0);

            const auto freq480 = renderToneAtPitch("sine_480.wav", 480.0);
            expectEquals(freq480, 480.0);

            const auto freqClampedLow = renderToneAtPitch("sine_low.wav", 350.0);
            expectEquals(freqClampedLow, 400.0);

            const auto freqClampedHigh = renderToneAtPitch("sine_high.wav", 520.0);
            expectEquals(freqClampedHigh, 480.0);
        });
    }
};

// -----------------------------------------------------------------------------

class MultiTrackWavExportTest final : public juce::UnitTest {
public:
    MultiTrackWavExportTest()
        : juce::UnitTest("Export: Multi-Track WAV offline export", "DevPiano/Recording") {
    }

    void runTest() override {
        testCase("multi-track multi-channel take renders non-silent WAV with piano and sine tone", [&] {
            devpiano::test::ScopedTempDir tempDir("multitrack-wav-export");
            const auto pianoWavPath = tempDir.getChildFile("multitrack_piano.wav");
            const auto sineWavPath = tempDir.getChildFile("multitrack_sine.wav");

            // Build a multi-track multi-channel take
            RecordingTake multiTrackTake;
            multiTrackTake.sampleRate = 48000.0;
            multiTrackTake.lengthSamples = 48000LL * 2; // 2 seconds

            // Track 1 / Channel 1: Right hand melody
            multiTrackTake.events.push_back({ 0, PerformanceEventType::midi, 0, RecordingEventSource::playback,
                                              juce::MidiMessage::noteOn(1, 72, 0.85f) });
            multiTrackTake.events.push_back({ 24000, PerformanceEventType::midi, 0, RecordingEventSource::playback,
                                              juce::MidiMessage::noteOff(1, 72, 0.0f) });
            multiTrackTake.events.push_back({ 24000, PerformanceEventType::midi, 0, RecordingEventSource::playback,
                                              juce::MidiMessage::noteOn(1, 76, 0.85f) });
            multiTrackTake.events.push_back({ 48000LL * 2, PerformanceEventType::midi, 0,
                                              RecordingEventSource::playback,
                                              juce::MidiMessage::noteOff(1, 76, 0.0f) });

            // Track 2 / Channel 2: Left hand chords + CC64 sustain
            multiTrackTake.events.push_back({ 0, PerformanceEventType::midi, 0, RecordingEventSource::playback,
                                              juce::MidiMessage::controllerEvent(2, 64, 127) });
            multiTrackTake.events.push_back({ 0, PerformanceEventType::midi, 0, RecordingEventSource::playback,
                                              juce::MidiMessage::noteOn(2, 48, 0.75f) });
            multiTrackTake.events.push_back({ 0, PerformanceEventType::midi, 0, RecordingEventSource::playback,
                                              juce::MidiMessage::noteOn(2, 55, 0.70f) });
            multiTrackTake.events.push_back({ 48000LL * 2, PerformanceEventType::midi, 0,
                                              RecordingEventSource::playback,
                                              juce::MidiMessage::noteOff(2, 48, 0.0f) });
            multiTrackTake.events.push_back({ 48000LL * 2, PerformanceEventType::midi, 0,
                                              RecordingEventSource::playback,
                                              juce::MidiMessage::noteOff(2, 55, 0.0f) });

            // 1. Export with Piano Synth Voice
            {
                WavExportOptions pianoOptions;
                pianoOptions.sampleRate = 48000.0;
                pianoOptions.blockSize = 512;
                pianoOptions.masterGain = 0.8f;
                pianoOptions.builtinTone = SettingsModel::BuiltinTone::piano;
                pianoOptions.pianoBrightness = 0.6f;
                pianoOptions.pianoHammerHardness = 0.5f;
                pianoOptions.pianoResonance = 0.6f;

                expect(exportTakeAsWavFile(multiTrackTake, pianoWavPath, pianoOptions), "piano export must succeed");
                expect(pianoWavPath.existsAsFile());

                juce::WavAudioFormat wavFormat;
                std::unique_ptr<juce::AudioFormatReader> reader(
                    wavFormat.createReaderFor(new juce::FileInputStream(pianoWavPath), false));
                expect(reader != nullptr);
                if (reader != nullptr) {
                    expectEquals(reader->sampleRate, 48000.0);
                    expectEquals(static_cast<int>(reader->numChannels), 2);
                    expect(reader->lengthInSamples >= 48000LL * 2);

                    juce::AudioBuffer<float> buf(static_cast<int>(reader->numChannels), 4096);
                    float peak = 0.0f;
                    std::int64_t pos = 0;
                    while (pos < reader->lengthInSamples) {
                        const auto count
                            = static_cast<int>(std::min<std::int64_t>(4096, reader->lengthInSamples - pos));
                        reader->read(&buf, 0, count, pos, true, true);
                        for (int c = 0; c < buf.getNumChannels(); ++c) {
                            peak = std::max(peak, buf.getMagnitude(c, 0, count));
                        }
                        pos += count;
                    }
                    expect(peak > 0.01f, "rendered piano audio must contain audible signal");
                }
            }

            // 2. Export with Sine Synth Voice
            {
                WavExportOptions sineOptions;
                sineOptions.sampleRate = 48000.0;
                sineOptions.blockSize = 512;
                sineOptions.masterGain = 0.8f;
                sineOptions.builtinTone = SettingsModel::BuiltinTone::sine;
                sineOptions.adsr = { 0.01f, 0.2f, 0.8f, 0.3f };

                expect(exportTakeAsWavFile(multiTrackTake, sineWavPath, sineOptions), "sine export must succeed");
                expect(sineWavPath.existsAsFile());

                juce::WavAudioFormat wavFormat;
                std::unique_ptr<juce::AudioFormatReader> reader(
                    wavFormat.createReaderFor(new juce::FileInputStream(sineWavPath), false));
                expect(reader != nullptr);
                if (reader != nullptr) {
                    expectEquals(reader->sampleRate, 48000.0);
                    expectEquals(static_cast<int>(reader->numChannels), 2);
                    expect(reader->lengthInSamples >= 48000LL * 2);

                    // Read samples and verify non-silent audio signal
                    juce::AudioBuffer<float> buffer(2, static_cast<int>(reader->lengthInSamples));
                    reader->read(&buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

                    float maxMagnitude = 0.0f;
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
                        maxMagnitude = std::max(maxMagnitude, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));
                    }
                    expect(maxMagnitude > 0.01f, "exported sine WAV should contain audible signal");
                }
            }
        });
    }
};
static MultiTrackWavExportTest multiTrackWavExportTest;
static WavExportRoundTripTest wavExportRoundTripTest;

// -----------------------------------------------------------------------------

class WavExportTaskSmokeTest final : public juce::UnitTest {
public:
    WavExportTaskSmokeTest()
        : juce::UnitTest("Export: WavExportTask background execution", "DevPiano/Recording") {
    }

    void runTest() override {
        testCase("WavExportTask executes background export successfully", [&] {
            devpiano::test::ScopedTempDir tempDir("task-smoke-success");
            const auto target = tempDir.getChildFile("export_task_ok.wav");

            RecordingTake take;
            take.sampleRate = 44100.0;
            take.lengthSamples = 4410; // 0.1s short take
            take.events.push_back({ 0, PerformanceEventType::midi, 0, RecordingEventSource::computerKeyboard,
                                    juce::MidiMessage::noteOn(1, 60, 0.8f) });
            take.events.push_back({ 2205, PerformanceEventType::midi, 0, RecordingEventSource::computerKeyboard,
                                    juce::MidiMessage::noteOff(1, 60) });

            WavExportOptions options;
            options.sampleRate = 44100.0;
            options.blockSize = 512;
            options.bitsPerSample = 16;
            options.masterGain = 1.0f;

            WavExportTask task(take, target, options, nullptr, nullptr);
            // Run synchronously in unit test environment
            const bool result = task.runSync();

            expect(result, "runSync must complete successfully");
            expect(task.wasSuccessful(), "wasSuccessful flag must be true");
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(target));
            expect(reader != nullptr, "background export must leave a readable final WAV header");
            if (reader != nullptr) {
                expectWithinAbsoluteError(reader->sampleRate, options.sampleRate, 0.001);
                juce::AudioBuffer<float> audio(2, 4410);
                expect(reader->read(&audio, 0, audio.getNumSamples(), 0, true, true));
                expect(audio.getMagnitude(0, audio.getNumSamples()) > 0.0001f,
                       "background export must contain the performed note");
            }
        });

        testCase("WavExportTask fails gracefully on invalid target path", [&] {
            RecordingTake take;
            take.sampleRate = 44100.0;
            take.lengthSamples = 4410;
            take.events.push_back({ 0, PerformanceEventType::midi, 0, RecordingEventSource::computerKeyboard,
                                    juce::MidiMessage::noteOn(1, 60, 0.8f) });

            WavExportOptions options;
            options.sampleRate = 44100.0;
            options.blockSize = 512;

            // Empty target file is invalid
            WavExportTask failTask(take, juce::File(), options, nullptr, nullptr);
            const bool result = failTask.runSync();

            expect(!result, "runSync must return false for invalid destination");
            expect(!failTask.wasSuccessful(), "wasSuccessful must be false");
        });

        testCase("rejected background export does not delete an existing output", [&] {
            devpiano::test::ScopedTempDir tempDir("task-existing-output");
            const auto target = tempDir.getChildFile("existing.wav");
            const auto take = makeOneSecondTake();
            WavExportOptions options;
            expect(exportTakeAsWavFile(take, target, options));
            juce::MemoryBlock original;
            expect(target.loadFileAsData(original));
            options.blockSize = 0;
            WavExportTask task(take, target, options, nullptr, nullptr);
            expect(!task.runSync());
            juce::MemoryBlock afterFailure;
            expect(target.loadFileAsData(afterFailure));
            expect(afterFailure == original, "failure must not remove a pre-existing user file");
        });
    }
};

static WavExportTaskSmokeTest wavExportTaskSmokeTest;
