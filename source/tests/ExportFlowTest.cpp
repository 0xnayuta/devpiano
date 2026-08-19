#include <JuceHeader.h>

#include "Export/ExportFlowSupport.h"
#include "Recording/MidiFileExporter.h"
#include "Recording/RecordingEngine.h"
#include "Recording/WavFileExporter.h"

using namespace devpiano::exporting;
using namespace devpiano::recording;

// =============================================================================
// Tests for the export chain's pure logic and file round-trips (AUDIT
// TEST-005):
//   - buildWavExportOptions parameter combinations
//   - canExportTake boundaries
//   - makeDefaultRecordingExportFile / makeExportLogPrefix
//   - MIDI export → read-back round-trip
//   - WAV export → read-back header + non-silent payload
// =============================================================================

namespace {

[[nodiscard]] juce::File makeScratchDir(const juce::String& tag) {
    auto dir
        = juce::File::getSpecialLocation(juce::File::tempDirectory)
              .getChildFile("devpiano-test-" + tag + "-" + juce::String(juce::Random::getSystemRandom().nextInt64()));
    dir.createDirectory();
    return dir;
}

// 1 秒 take：note-on at 0、note-off at 1s。
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

        testCase("performance settings flow into the options", [&] {
            const auto take = makeOneSecondTake();
            SettingsModel::PerformanceSettingsView perf;
            perf.masterGain = 0.33f;
            perf.adsrAttack = 0.05f;
            perf.adsrDecay = 0.60f;
            perf.adsrSustain = 0.40f;
            perf.adsrRelease = 0.80f;
            perf.builtinTone = SettingsModel::BuiltinTone::piano;
            perf.pianoBrightness = 0.70f;
            perf.pianoHammerHardness = 0.35f;
            perf.pianoResonance = 0.90f;
            const auto options = buildWavExportOptions(take, perf, 44100.0, 512);
            expectWithinAbsoluteError(options.masterGain, 0.33f, 0.0001f);
            expectWithinAbsoluteError(options.adsr.attack, 0.05f, 0.0001f);
            expectWithinAbsoluteError(options.adsr.decay, 0.60f, 0.0001f);
            expectWithinAbsoluteError(options.adsr.sustain, 0.40f, 0.0001f);
            expectWithinAbsoluteError(options.adsr.release, 0.80f, 0.0001f);
            expectEquals(static_cast<int>(options.builtinTone), static_cast<int>(SettingsModel::BuiltinTone::piano),
                         "tone must flow into options");
            expectWithinAbsoluteError(options.pianoBrightness, 0.70f, 0.0001f);
            expectWithinAbsoluteError(options.pianoHammerHardness, 0.35f, 0.0001f);
            expectWithinAbsoluteError(options.pianoResonance, 0.90f, 0.0001f);
            expectEquals(options.numChannels, 2);
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
        testCase("default export file has the timestamped naming scheme", [&] {
            auto dir = makeScratchDir("export-naming");
            const auto time = juce::Time(2026, 8, 17, 12, 30, 45, 0, true);

            const auto midiFile = makeDefaultRecordingExportFile(ExportFileType::midi, dir, time);
            expect(midiFile.getParentDirectory() == dir);
            expect(midiFile.getFileName().startsWith("recording_"));
            expect(midiFile.hasFileExtension(".mid"));

            const auto wavFile = makeDefaultRecordingExportFile(ExportFileType::wav, dir, time);
            expect(wavFile.getFileName().startsWith("recording_"));
            expect(wavFile.hasFileExtension(".wav"));
        });

        testCase("log prefixes identify the export type", [&] {
            expectEquals(makeExportLogPrefix(ExportFileType::midi), juce::String("[Export] MIDI"));
            expectEquals(makeExportLogPrefix(ExportFileType::wav), juce::String("[Export] WAV"));
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
            auto dir = makeScratchDir("midi-export");
            const auto path = dir.getChildFile("take.mid");

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
            auto dir = makeScratchDir("midi-export-empty");
            RecordingTake take;
            expect(!exportTakeAsMidiFile(take, dir.getChildFile("x.mid")));
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
            auto dir = makeScratchDir("wav-export");
            const auto path = dir.getChildFile("take.wav");

            const auto take = makeOneSecondTake();
            WavExportOptions options;
            options.sampleRate = 44100.0;
            options.blockSize = 512;
            options.masterGain = 0.8f;
            options.adsr = { 0.01f, 0.2f, 0.8f, 0.3f };

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
    }
};

static WavExportRoundTripTest wavExportRoundTripTest;
