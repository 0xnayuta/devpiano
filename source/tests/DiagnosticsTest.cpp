#include <JuceHeader.h>

#include "Diagnostics/DevPianoLogger.h"
#include "Diagnostics/MidiTrace.h"
#include "TestHelpers.h"

namespace {

class DiagnosticsTest : public juce::UnitTest {
public:
    DiagnosticsTest()
        : juce::UnitTest("Diagnostics", "DevPiano/Diagnostics") {
    }

    void runTest() override {
        testCase("DevPianoLogger safe destruction unsets current logger", [&] {
            devpiano::test::ScopedTempDir tempDir("logger-cleanup");
            const auto testLogFile = tempDir.getChildFile("cleanup.log");

            {
                auto logger = std::make_unique<devpiano::diagnostics::DevPianoLogger>(testLogFile);
                juce::Logger::setCurrentLogger(logger.get());
                expect(devpiano::diagnostics::DevPianoLogger::getCurrentDevPianoLogger() == logger.get());
                // logger is destroyed at end of block without explicit setCurrentLogger(nullptr)
            }

            // Destructor must have unset current logger safely
            expect(juce::Logger::getCurrentLogger() == nullptr);
            expect(devpiano::diagnostics::DevPianoLogger::getCurrentDevPianoLogger() == nullptr);
        });

        testCase("describeMidiMessage NoteOn NoteOff Controller and fallback", [&] {
            using devpiano::diagnostics::describeMidiMessage;

            // Audible intermediate velocity 64 (OBS-001: raw integer 0..127, not scaled 8128)
            const auto noteOn64 = juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(64));
            expect(noteOn64.isNoteOn());
            const auto desc64 = describeMidiMessage(noteOn64);
            expect(desc64.startsWith("NoteOn"));
            expect(desc64.contains("ch=1"));
            expect(desc64.contains("note=60"));
            expectEquals(desc64.fromFirstOccurrenceOf("vel=", false, false).getIntValue(), 64);

            // Audible lower boundary velocity 1
            const auto noteOn1 = juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(1));
            expect(noteOn1.isNoteOn());
            const auto desc1 = describeMidiMessage(noteOn1);
            expect(desc1.startsWith("NoteOn"));
            expectEquals(desc1.fromFirstOccurrenceOf("vel=", false, false).getIntValue(), 1);

            // Audible upper boundary velocity 127
            const auto noteOn127 = juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(127));
            expect(noteOn127.isNoteOn());
            const auto desc127 = describeMidiMessage(noteOn127);
            expect(desc127.startsWith("NoteOn"));
            expectEquals(desc127.fromFirstOccurrenceOf("vel=", false, false).getIntValue(), 127);

            // MIDI velocity 0 boundary: JUCE semantics classify noteOn with vel=0 as noteOff
            const auto noteOn0 = juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(0));
            expect(noteOn0.isNoteOff());
            expect(!noteOn0.isNoteOn());
            const auto desc0 = describeMidiMessage(noteOn0);
            expect(desc0.startsWith("NoteOff"));
            expect(desc0.contains("vel="));
            expectEquals(desc0.fromFirstOccurrenceOf("vel=", false, false).getIntValue(), 0);

            // Explicit NoteOff message with release velocity 64
            const auto noteOff64 = juce::MidiMessage::noteOff(2, 69, static_cast<juce::uint8>(64));
            expect(noteOff64.isNoteOff());
            const auto descOff64 = describeMidiMessage(noteOff64);
            expect(descOff64.startsWith("NoteOff"));
            expect(descOff64.contains("ch=2"));
            expect(descOff64.contains("note=69"));
            expectEquals(descOff64.fromFirstOccurrenceOf("vel=", false, false).getIntValue(), 64);

            const auto ccPedal = juce::MidiMessage::controllerEvent(1, 64, 127);
            const auto ccDesc = describeMidiMessage(ccPedal);
            expect(ccDesc.startsWith("CC"));
            expect(ccDesc.contains("cc=64"));
            expect(ccDesc.contains("val=127"));

            const auto pitchBend = juce::MidiMessage::pitchWheel(1, 8192);
            const auto pitchDesc = describeMidiMessage(pitchBend);
            expect(pitchDesc.startsWith("PitchBend"));
            expect(pitchDesc.contains("val=8192"));

            const auto progChange = juce::MidiMessage::programChange(1, 5);
            const auto progDesc = describeMidiMessage(progChange);
            expect(progDesc.startsWith("ProgramChange"));
            expect(progDesc.contains("prog=5"));
        });

        testCase("DevPianoLogger in-session bounded rotation within budget", [&] {
            devpiano::test::ScopedTempDir tempDir("logger-rotation");
            const auto testLogFile = tempDir.getChildFile("bounded.log");
            constexpr juce::int64 budget = 1024; // 1 KiB combined budget (512 B per file)

            {
                devpiano::diagnostics::DevPianoLogger logger(testLogFile, budget);
                expect(logger.hasActiveFileLogger());
                const auto backupFile = logger.getBackupLogFile();

                juce::Logger::setCurrentLogger(&logger);
                // Write multiple messages in a long session to trigger multiple rotations
                for (int i = 0; i < 40; ++i) {
                    juce::Logger::writeToLog("Session message sequence entry #" + juce::String(i));
                }
                juce::Logger::setCurrentLogger(nullptr);

                expect(!logger.hasFileError());
                expect(logger.hasActiveFileLogger());

                // Both active and backup files must exist
                expect(testLogFile.existsAsFile());
                expect(backupFile.existsAsFile());

                const auto activeSize = testLogFile.getSize();
                const auto backupSize = backupFile.getSize();

                // Per-file invariant
                expect(activeSize <= budget / 2);
                expect(backupSize <= budget / 2);

                // Combined total budget invariant
                expect(activeSize + backupSize <= budget);

                // Latest legal message must be retained in the active file
                const auto latestContent = testLogFile.loadFileAsString();
                expect(latestContent.contains("#39"));

                // No unbounded temp files or extra archive files created
                const auto allFiles = tempDir.get().findChildFiles(juce::File::findFiles, false);
                expect(allFiles.size() == 2);
            }
        });

        testCase("DevPianoLogger oversized UTF-8 clamping preserves valid code points", [&] {
            devpiano::test::ScopedTempDir tempDir("logger-oversize");
            const auto testLogFile = tempDir.getChildFile("oversize.log");
            constexpr juce::int64 budget = 512; // 256 bytes per file

            {
                devpiano::diagnostics::DevPianoLogger logger(testLogFile, budget);
                expect(logger.hasActiveFileLogger());

                juce::String oversized;
                const auto chunk = "Log" + juce::String::charToString(0x266C) + juce::String::charToString(0x1F3B9);
                for (int i = 0; i < 40; ++i) {
                    oversized += chunk;
                }

                juce::Logger::setCurrentLogger(&logger);
                juce::Logger::writeToLog(oversized);
                juce::Logger::setCurrentLogger(nullptr);

                expect(!logger.hasFileError());
                expect(testLogFile.existsAsFile());
                expect(testLogFile.getSize() <= budget / 2);

                juce::MemoryBlock bytes;
                expect(testLogFile.loadFileAsData(bytes));
                const auto* data = static_cast<const char*>(bytes.getData());
                const auto size = static_cast<int>(bytes.getSize());
                expect(juce::CharPointer_UTF8::isValidString(data, size));
                const auto content = juce::String::fromUTF8(data, size);
                expect(content.endsWithChar('\n'));
                expect(oversized.startsWith(content.dropLastCharacters(1)));
            }
        });

        testCase("DevPianoLogger filesystem failure does not exceed budget and records error", [&] {
            devpiano::test::ScopedTempDir tempDir("logger-failure");
            const auto testLogFile = tempDir.getChildFile("fail.log");
            constexpr juce::int64 budget = 256; // 128 bytes per file

            // Scenario A: target parent is a regular file, so directory creation / file open fails
            const auto blockerFile = tempDir.getChildFile("regular_file_blocker");
            blockerFile.create();
            const auto invalidFile = blockerFile.getChildFile("cannot_create_inside_file.log");
            {
                devpiano::diagnostics::DevPianoLogger failLogger(invalidFile, budget);
                expect(!failLogger.hasActiveFileLogger());
                expect(failLogger.hasFileError());
                expect(failLogger.getLastError().isNotEmpty());

                juce::Logger::setCurrentLogger(&failLogger);
                juce::Logger::writeToLog("Message while logger in failure state");
                juce::Logger::setCurrentLogger(nullptr);
            }

            // Scenario B: rotation failure when backup path is blocked by a non-empty directory
            {
                devpiano::diagnostics::DevPianoLogger logger(testLogFile, budget);
                expect(logger.hasActiveFileLogger());

                juce::Logger::setCurrentLogger(&logger);
                juce::Logger::writeToLog("Initial message within budget");
                expect(testLogFile.existsAsFile());
                const auto initialSize = testLogFile.getSize();
                expect(initialSize <= budget / 2);

                // Block the backup file path by creating it as a non-empty directory
                const auto backupPath = logger.getBackupLogFile();
                backupPath.createDirectory();
                backupPath.getChildFile("blocking_child.txt").create();

                // Now write large messages that would force a rotation
                for (int i = 0; i < 10; ++i) {
                    juce::Logger::writeToLog("Attempting write that triggers blocked rotation #" + juce::String(i));
                }
                juce::Logger::setCurrentLogger(nullptr);

                // Rotation failure must deactivate file sink and record error
                expect(logger.hasFileError());
                expect(!logger.hasActiveFileLogger());
                expect(logger.getLastError().isNotEmpty());

                // Active file must NOT continue growing beyond budget
                expect(testLogFile.getSize() <= budget / 2);
            }
        });
    }
};

DiagnosticsTest diagnosticsTest;

} // namespace
