#include <JuceHeader.h>

#include "Recording/MidiFileImporter.h"
#include "Recording/RecordingEngine.h"

// =============================================================================
// MIDI 文件导入测试
//
// 这些测试使用 tests/fixtures/midi/ 下的真实 MIDI 文件。
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

            MidiImportOptions opts;
            opts.ignoreOtherTracks = true;
            auto resultWithPreference
                = importMidiFile(juce::File(getFixturePath("multitrack-basic.mid")), 48000.0, opts);
            expect(resultWithPreference.has_value());
            expect(!resultWithPreference->isEmpty());

            MidiImportOptions allTracksOpts;
            allTracksOpts.ignoreOtherTracks = false;
            auto resultAllTracks
                = importMidiFile(juce::File(getFixturePath("multitrack-basic.mid")), 48000.0, allTracksOpts);
            expect(resultAllTracks.has_value());
            expect(!resultAllTracks->isEmpty());
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
