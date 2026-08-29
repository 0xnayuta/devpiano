#include <JuceHeader.h>

#include "Export/ExportFlowSupport.h"
#include "Recording/RecordingFlowSupport.h"
#include "Recording/RecordingSessionController.h"
#include "Settings/SettingsModel.h"
#include "TestHelpers.h"

using namespace devpiano::recording;
// devpiano::recording 亦含 RecordingState 枚举（RecordingEngine），故 ui 侧用别名区分
using RecordingUiState = devpiano::ui::RecordingState;

// =============================================================================
// Tests for the recording-session state semantics and flow orchestration
// (AUDIT TEST-001):
//   - RecordingSession::isRecording/isPlaying paused semantics
//   - ui::RecordingState ↔ RecordingFlowState mapping (toRecordingFlowState /
//     toRecordingControlsState / makeRecordingFlowStatus)
//   - chooseRecordingFlowCommand / getStateAfterCommand / shouldRestoreKeyboardFocus
//     combination matrix covering the paused states
//   - last-MIDI export/import directory resolution
//
// replaceTakeAndStartPlayback itself needs a MainComponent (GUI) and is
// covered indirectly: its state transitions are exactly the
// chooseRecordingFlowCommand + getStateAfterCommand composition tested here.
// =============================================================================

namespace {

// 构造一个包含单个 note-on 事件的 take（hasTake() == true）。
devpiano::recording::RecordingTake makeTakeWithEvent() {
    devpiano::recording::RecordingTake take;
    take.sampleRate = 44100.0;
    take.lengthSamples = 44100;
    take.events.push_back({ 0, devpiano::recording::PerformanceEventType::midi, 0,
                            devpiano::recording::RecordingEventSource::computerKeyboard,
                            juce::MidiMessage::noteOn(1, 60, 0.8f) });
    return take;
}

} // namespace

// -----------------------------------------------------------------------------

class RecordingSessionSemanticsTest final : public juce::UnitTest {
public:
    RecordingSessionSemanticsTest()
        : juce::UnitTest("RecordingSession: paused state semantics", "DevPiano/Recording") {
    }

    void runTest() override {
        using Session = RecordingSessionController::RecordingSession;

        testCase("idle state is neither recording nor playing", [&] {
            Session s;
            expect(s.isIdle());
            expect(!s.isRecording());
            expect(!s.isPlaying());
        });

        testCase("recording state is recording flow active", [&] {
            Session s;
            s.state = RecordingUiState::recording;
            expect(s.isRecording());
            expect(!s.isPlaying());
            expect(!s.isIdle());
        });

        testCase("paused recording still counts as recording flow active", [&] {
            Session s;
            s.state = RecordingUiState::recordingPaused;
            expect(s.isRecording(), "paused recording must stay in the recording flow");
            expect(!s.isPlaying());
            expect(!s.isIdle());
        });

        testCase("playing state is playback flow active", [&] {
            Session s;
            s.state = RecordingUiState::playing;
            expect(s.isPlaying());
            expect(!s.isRecording());
            expect(!s.isIdle());
        });

        testCase("paused playback still counts as playback flow active", [&] {
            Session s;
            s.state = RecordingUiState::playingPaused;
            expect(s.isPlaying(), "paused playback must stay in the playback flow");
            expect(!s.isRecording());
            expect(!s.isIdle());
        });

        testCase("hasTake reflects an empty take", [&] {
            Session s;
            expect(!s.hasTake(), "default take must be empty");
            s.take = makeTakeWithEvent();
            expect(s.hasTake(), "take with events must be non-empty");
        });
    }
};

static RecordingSessionSemanticsTest recordingSessionSemanticsTest;

// -----------------------------------------------------------------------------

class RecordingFlowStateMachineTest final : public juce::UnitTest {
public:
    RecordingFlowStateMachineTest()
        : juce::UnitTest("RecordingFlow: command selection state machine", "DevPiano/Recording") {
    }

    void runTest() override {
        testUiToFlowMapping();
        testFlowToUiMapping();
        testRecordIntent();
        testPlayPauseIntent();
        testStopIntent();
        testStateAfterCommand();
        testKeyboardFocusRestore();
    }

private:
    void testUiToFlowMapping() {
        testCase("ui::RecordingState maps onto RecordingFlowState", [&] {
            expect(toRecordingFlowState(RecordingUiState::idle) == RecordingFlowState::idle);
            expect(toRecordingFlowState(RecordingUiState::recording) == RecordingFlowState::recording);
            expect(toRecordingFlowState(RecordingUiState::recordingPaused) == RecordingFlowState::recordingPaused);
            expect(toRecordingFlowState(RecordingUiState::playing) == RecordingFlowState::playing);
            expect(toRecordingFlowState(RecordingUiState::playingPaused) == RecordingFlowState::playingPaused);
        });
    }

    void testFlowToUiMapping() {
        testCase("RecordingFlowState maps back onto ui::RecordingState (round-trip)", [&] {
            const RecordingUiState states[]
                = { RecordingUiState::idle, RecordingUiState::recording, RecordingUiState::recordingPaused,
                    RecordingUiState::playing, RecordingUiState::playingPaused };
            for (const auto state : states) {
                expectEquals(static_cast<int>(toRecordingControlsState(toRecordingFlowState(state))),
                             static_cast<int>(state));
            }
        });
    }

    void testRecordIntent() {
        testCase("record starts only from idle", [&] {
            RecordingFlowStatus status;
            status.hasTake = false;
            status.currentState = RecordingFlowState::idle;
            expect(chooseRecordingFlowCommand(RecordingFlowIntent::record, status)
                   == RecordingFlowCommand::startRecording);

            const RecordingFlowState nonIdleStates[]
                = { RecordingFlowState::recording, RecordingFlowState::recordingPaused, RecordingFlowState::playing,
                    RecordingFlowState::playingPaused };
            for (const auto s : nonIdleStates) {
                status.currentState = s;
                expect(chooseRecordingFlowCommand(RecordingFlowIntent::record, status) == RecordingFlowCommand::none);
            }
        });
    }

    void testPlayPauseIntent() {
        testCase("playPause from idle needs a take", [&] {
            RecordingFlowStatus status;
            status.currentState = RecordingFlowState::idle;
            status.hasTake = false;
            expect(chooseRecordingFlowCommand(RecordingFlowIntent::playPause, status) == RecordingFlowCommand::none);
            status.hasTake = true;
            expect(chooseRecordingFlowCommand(RecordingFlowIntent::playPause, status)
                   == RecordingFlowCommand::startPlayback);
        });

        testCase("playPause toggles playback and recording pause states", [&] {
            RecordingFlowStatus status;
            status.hasTake = true;

            status.currentState = RecordingFlowState::playing;
            expect(chooseRecordingFlowCommand(RecordingFlowIntent::playPause, status)
                   == RecordingFlowCommand::pausePlayback);
            status.currentState = RecordingFlowState::playingPaused;
            expect(chooseRecordingFlowCommand(RecordingFlowIntent::playPause, status)
                   == RecordingFlowCommand::resumePlayback);
            status.currentState = RecordingFlowState::recording;
            expect(chooseRecordingFlowCommand(RecordingFlowIntent::playPause, status)
                   == RecordingFlowCommand::pauseRecording);
            status.currentState = RecordingFlowState::recordingPaused;
            expect(chooseRecordingFlowCommand(RecordingFlowIntent::playPause, status)
                   == RecordingFlowCommand::resumeRecording);
        });
    }

    void testStopIntent() {
        testCase("stop maps recording and playback (paused included) to stop commands", [&] {
            RecordingFlowStatus status;
            status.hasTake = true;

            status.currentState = RecordingFlowState::recording;
            expect(chooseRecordingFlowCommand(RecordingFlowIntent::stop, status)
                   == RecordingFlowCommand::stopRecording);
            status.currentState = RecordingFlowState::recordingPaused;
            expect(chooseRecordingFlowCommand(RecordingFlowIntent::stop, status) == RecordingFlowCommand::stopRecording,
                   "stopping a paused recording must stop it");
            status.currentState = RecordingFlowState::playing;
            expect(chooseRecordingFlowCommand(RecordingFlowIntent::stop, status) == RecordingFlowCommand::stopPlayback);
            status.currentState = RecordingFlowState::playingPaused;
            expect(chooseRecordingFlowCommand(RecordingFlowIntent::stop, status) == RecordingFlowCommand::stopPlayback,
                   "stopping a paused playback must stop it");
            status.currentState = RecordingFlowState::idle;
            expect(chooseRecordingFlowCommand(RecordingFlowIntent::stop, status) == RecordingFlowCommand::none);
        });
    }

    void testStateAfterCommand() {
        testCase("every command maps to the expected target state", [&] {
            expect(getStateAfterCommand(RecordingFlowCommand::startRecording, RecordingFlowState::idle)
                   == RecordingFlowState::recording);
            expect(getStateAfterCommand(RecordingFlowCommand::startPlayback, RecordingFlowState::idle)
                   == RecordingFlowState::playing);
            expect(getStateAfterCommand(RecordingFlowCommand::resumePlayback, RecordingFlowState::playingPaused)
                   == RecordingFlowState::playing);
            expect(getStateAfterCommand(RecordingFlowCommand::pausePlayback, RecordingFlowState::playing)
                   == RecordingFlowState::playingPaused);
            expect(getStateAfterCommand(RecordingFlowCommand::pauseRecording, RecordingFlowState::recording)
                   == RecordingFlowState::recordingPaused);
            expect(getStateAfterCommand(RecordingFlowCommand::resumeRecording, RecordingFlowState::recordingPaused)
                   == RecordingFlowState::recording);
            expect(getStateAfterCommand(RecordingFlowCommand::stopRecording, RecordingFlowState::recording)
                   == RecordingFlowState::idle);
            expect(getStateAfterCommand(RecordingFlowCommand::stopPlayback, RecordingFlowState::playing)
                   == RecordingFlowState::idle);
        });

        testCase("none keeps the fallback state", [&] {
            expect(getStateAfterCommand(RecordingFlowCommand::none, RecordingFlowState::playingPaused)
                   == RecordingFlowState::playingPaused);
        });
    }

    void testKeyboardFocusRestore() {
        testCase("keyboard focus is restored for every command except none", [&] {
            expect(!shouldRestoreKeyboardFocus(RecordingFlowCommand::none));
            expect(shouldRestoreKeyboardFocus(RecordingFlowCommand::startRecording));
            expect(shouldRestoreKeyboardFocus(RecordingFlowCommand::startPlayback));
            expect(shouldRestoreKeyboardFocus(RecordingFlowCommand::pausePlayback));
            expect(shouldRestoreKeyboardFocus(RecordingFlowCommand::stopPlayback));
        });
    }
};

static RecordingFlowStateMachineTest recordingFlowStateMachineTest;

// -----------------------------------------------------------------------------

class MidiExportDirectoryTest final : public juce::UnitTest {
public:
    MidiExportDirectoryTest()
        : juce::UnitTest("RecordingSession: last MIDI directory resolution", "DevPiano/Recording") {
    }

    void runTest() override {
        using devpiano::exporting::getLastMidiExportDirectory;
        using devpiano::exporting::getLastMidiImportDirectory;

        testCase("export: existing file yields its parent directory", [&] {
            devpiano::test::ScopedTempDir tempDir("export-file");
            auto file = tempDir.getChildFile("out.mid");
            file.replaceWithText("x");

            SettingsModel settings;
            settings.lastMidiExportPath = file.getFullPathName();
            expectEquals(getLastMidiExportDirectory(settings).getFullPathName(), tempDir.get().getFullPathName());
        });

        testCase("export: existing directory yields itself", [&] {
            devpiano::test::ScopedTempDir tempDir("export-dir");

            SettingsModel settings;
            settings.lastMidiExportPath = tempDir.get().getFullPathName();
            expectEquals(getLastMidiExportDirectory(settings).getFullPathName(), tempDir.get().getFullPathName());
        });

        testCase("export: stale path falls back to its parent directory", [&] {
            devpiano::test::ScopedTempDir tempDir("export-stale");
            auto missing = tempDir.getChildFile("gone.mid"); // 不存在

            SettingsModel settings;
            settings.lastMidiExportPath = missing.getFullPathName();
            expectEquals(getLastMidiExportDirectory(settings).getFullPathName(), tempDir.get().getFullPathName());
        });

        testCase("export: empty setting falls back to CWD", [&] {
            SettingsModel settings;
            settings.lastMidiExportPath = {};
            expectEquals(getLastMidiExportDirectory(settings).getFullPathName(),
                         juce::File::getCurrentWorkingDirectory().getFullPathName());
        });

        testCase("import: existing file yields its parent directory", [&] {
            devpiano::test::ScopedTempDir tempDir("import-file");
            auto file = tempDir.getChildFile("in.mid");
            file.replaceWithText("x");

            SettingsModel settings;
            settings.lastMidiImportPath = file.getFullPathName();
            expectEquals(getLastMidiImportDirectory(settings).getFullPathName(), tempDir.get().getFullPathName());
        });

        testCase("import: missing path yields an empty file", [&] {
            devpiano::test::ScopedTempDir tempDir("import-stale");

            SettingsModel settings;
            settings.lastMidiImportPath = tempDir.getChildFile("gone.mid").getFullPathName();
            expect(getLastMidiImportDirectory(settings) == juce::File());
        });

        testCase("import: empty setting yields an empty file", [&] {
            SettingsModel settings;
            settings.lastMidiImportPath = {};
            expect(getLastMidiImportDirectory(settings) == juce::File());
        });
    }
};

static MidiExportDirectoryTest midiExportDirectoryTest;
