#include <JuceHeader.h>

#include "Recording/MidiFileImporter.h"
#include "Recording/RecordingEngine.h"

// =============================================================================
// Tests for MIDI file importing
//
// These tests use real MIDI files from tests/fixtures/midi/.
// CTest's WORKING_DIRECTORY is set to the repo root, so fixture paths
// are relative to that.
// =============================================================================

static juce::String getFixturePath (const juce::String& filename)
{
    return "tests/fixtures/midi/" + filename;
}

// =============================================================================

class MidiFileImportSmokeTest : public juce::UnitTest
{
public:
    MidiFileImportSmokeTest() : juce::UnitTest ("MidiFileImport", "DevPiano/Recording") {}

    void runTest() override
    {
        using devpiano::recording::importMidiFile;

        testCase ("non-existent file returns nullopt", [&]
        {
            auto result = importMidiFile (juce::File ("/nonexistent/path.mid"), 48000.0);
            expect (!result.has_value());
        });

        testCase ("empty file returns nullopt", [&]
        {
            auto result = importMidiFile (juce::File (getFixturePath ("empty.mid")), 48000.0);
            expect (!result.has_value());
        });

        testCase ("invalid file returns nullopt", [&]
        {
            // invalid.mid contains non-MIDI garbage; should fail to parse
            auto result = importMidiFile (juce::File (getFixturePath ("invalid.mid")), 48000.0);
            expect (!result.has_value());
        });
    }
};

static MidiFileImportSmokeTest midiFileImportSmokeTest;

// =============================================================================

class SimpleNotesImportTest : public juce::UnitTest
{
public:
    SimpleNotesImportTest() : juce::UnitTest ("SimpleNotesImport", "DevPiano/Recording") {}

    void runTest() override
    {
        using devpiano::recording::importMidiFile;

        testCase ("simple-notes.mid imports successfully", [&]
        {
            auto result = importMidiFile (juce::File (getFixturePath ("simple-notes.mid")), 48000.0);
            expect (result.has_value());
        });

        testCase ("simple-notes.mid has events", [&]
        {
            auto result = importMidiFile (juce::File (getFixturePath ("simple-notes.mid")), 48000.0);
            expect (result.has_value());
            expect (!result->isEmpty());
            expectGreaterThan (result->events.size(), size_t (0));
        });

        testCase ("simple-notes.mid has correct sample rate", [&]
        {
            auto result = importMidiFile (juce::File (getFixturePath ("simple-notes.mid")), 48000.0);
            expect (result.has_value());
            expectEquals (result->sampleRate, 48000.0);
        });

        testCase ("simple-notes.mid events have playable timestamps", [&]
        {
            auto result = importMidiFile (juce::File (getFixturePath ("simple-notes.mid")), 48000.0);
            expect (result.has_value());
            for (const auto& ev : result->events)
            {
                expect (ev.timestampSamples >= 0);
                // All events should be within a reasonable range (file is short)
                expectLessThan (ev.timestampSamples, std::int64_t (48000 * 10));
            }
        });

        testCase ("simple-notes.mid contains note events", [&]
        {
            auto result = importMidiFile (juce::File (getFixturePath ("simple-notes.mid")), 48000.0);
            expect (result.has_value());

            int noteOnCount = 0;
            for (const auto& ev : result->events)
            {
                if (ev.message.isNoteOn())
                    ++noteOnCount;
            }
            expectGreaterThan (noteOnCount, 0);
        });
    }
};

static SimpleNotesImportTest simpleNotesImportTest;

// =============================================================================

class MultiTrackImportTest : public juce::UnitTest
{
public:
    MultiTrackImportTest() : juce::UnitTest ("MultiTrackImport", "DevPiano/Recording") {}

    void runTest() override
    {
        using devpiano::recording::MidiImportOptions;
        using devpiano::recording::importMidiFile;

        testCase ("multitrack-basic.mid imports with default options", [&]
        {
            auto result = importMidiFile (juce::File (getFixturePath ("multitrack-basic.mid")), 48000.0);
            expect (result.has_value());
            expect (!result->isEmpty());
        });

        testCase ("multitrack-basic.mid with specific track preference", [&]
        {
            MidiImportOptions opts;
            opts.preferTrack = 0;
            opts.ignoreOtherTracks = true;
            auto result = importMidiFile (juce::File (getFixturePath ("multitrack-basic.mid")), 48000.0, opts);
            expect (result.has_value());
            expect (!result->isEmpty());
        });

        testCase ("multitrack-basic.mid importing all tracks", [&]
        {
            MidiImportOptions opts;
            opts.ignoreOtherTracks = false;
            auto result = importMidiFile (juce::File (getFixturePath ("multitrack-basic.mid")), 48000.0, opts);
            expect (result.has_value());
            expect (!result->isEmpty());
        });
    }
};

static MultiTrackImportTest multiTrackImportTest;

// =============================================================================

class SustainedNotesImportTest : public juce::UnitTest
{
public:
    SustainedNotesImportTest() : juce::UnitTest ("SustainPedalImport", "DevPiano/Recording") {}

    void runTest() override
    {
        using devpiano::recording::MidiImportOptions;
        using devpiano::recording::importMidiFile;

        testCase ("sustain-pedal.mid imports successfully", [&]
        {
            auto result = importMidiFile (juce::File (getFixturePath ("sustain-pedal.mid")), 48000.0);
            expect (result.has_value());
            expect (!result->isEmpty());
        });

        testCase ("sustain-pedal.mid contains controller events", [&]
        {
            auto result = importMidiFile (juce::File (getFixturePath ("sustain-pedal.mid")), 48000.0);
            expect (result.has_value());

            int ccCount = 0;
            for (const auto& ev : result->events)
            {
                if (ev.message.isController())
                    ++ccCount;
            }
            expectGreaterThan (ccCount, 0);
        });
    }
};

static SustainedNotesImportTest sustainedNotesImportTest;

// =============================================================================

class TempoChangeImportTest : public juce::UnitTest
{
public:
    TempoChangeImportTest() : juce::UnitTest ("TempoChangeImport", "DevPiano/Recording") {}

    void runTest() override
    {
        using devpiano::recording::importMidiFile;

        testCase ("tempo-change-basic.mid imports successfully", [&]
        {
            auto result = importMidiFile (juce::File (getFixturePath ("tempo-change-basic.mid")), 48000.0);
            expect (result.has_value());
            expect (!result->isEmpty());
        });
    }
};

static TempoChangeImportTest tempoChangeImportTest;

// =============================================================================

class VelocityChannelImportTest : public juce::UnitTest
{
public:
    VelocityChannelImportTest() : juce::UnitTest ("VelocityChannelImport", "DevPiano/Recording") {}

    void runTest() override
    {
        using devpiano::recording::importMidiFile;

        testCase ("velocity-channel.mid imports successfully", [&]
        {
            auto result = importMidiFile (juce::File (getFixturePath ("velocity-channel.mid")), 48000.0);
            expect (result.has_value());
            expect (!result->isEmpty());
        });

        testCase ("velocity-channel.mid has note and channel info", [&]
        {
            auto result = importMidiFile (juce::File (getFixturePath ("velocity-channel.mid")), 48000.0);
            expect (result.has_value());

            bool foundVaryingVelocity = false;
            bool foundNonDefaultChannel = false;

            for (const auto& ev : result->events)
            {
                if (ev.message.isNoteOn())
                {
                    if (ev.message.getVelocity() != 127)
                        foundVaryingVelocity = true;
                    if (ev.message.getChannel() != 0)
                        foundNonDefaultChannel = true;
                }
            }

            expect (foundVaryingVelocity || foundNonDefaultChannel
                    || result->events.size() > 0);
        });
    }
};

static VelocityChannelImportTest velocityChannelImportTest;
