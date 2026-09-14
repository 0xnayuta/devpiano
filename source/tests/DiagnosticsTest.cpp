#include <JuceHeader.h>

#include "Diagnostics/DevPianoLogger.h"
#include "Diagnostics/Log.h"
#include "Diagnostics/MidiTrace.h"
#include "TestHelpers.h"

namespace {

class DiagnosticsTest : public juce::UnitTest {
public:
    DiagnosticsTest()
        : juce::UnitTest("Diagnostics", "DevPiano/Diagnostics") {
    }

    void runTest() override {
        testCase("DevPianoLogger default path resolution", [&] {
            devpiano::diagnostics::DevPianoLogger logger;
            const auto logFile = logger.getLogFile();
            expect(logFile.getFileName() == "devpiano.log");
            expect(logger.getLogDirectory().getFileName() == "devpiano");
        });

        testCase("DevPianoLogger custom file writing and dual sink", [&] {
            devpiano::test::ScopedTempDir tempDir("diagnostics");
            const auto testLogFile = tempDir.getChildFile("test_output.log");

            {
                devpiano::diagnostics::DevPianoLogger logger(testLogFile, 64LL * 1024);
                expect(logger.hasActiveFileLogger());
                expect(logger.getLogFile() == testLogFile);
                expect(logger.getLogDirectory() == tempDir.get());

                juce::Logger::setCurrentLogger(&logger);
                expect(devpiano::diagnostics::DevPianoLogger::getCurrentDevPianoLogger() == &logger);

                DP_LOG_INFO("Sample test info message");
                DP_LOG_WARN("Sample test warning message");
                DP_LOG_ERROR("Sample test error message");

                juce::Logger::setCurrentLogger(nullptr);
            }

            expect(testLogFile.existsAsFile());
            const auto content = testLogFile.loadFileAsString();
            expect(content.contains("=== DevPiano Diagnostics Session Started ==="));
            expect(content.contains("[DP INFO] Sample test info message"));
            expect(content.contains("[DP WARN] Sample test warning message"));
            expect(content.contains("[DP ERROR] Sample test error message"));
        });

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

            const auto noteOn = juce::MidiMessage::noteOn(1, 60, 0.75f);
            const auto noteOnDesc = describeMidiMessage(noteOn);
            expect(noteOnDesc.startsWith("NoteOn"));
            expect(noteOnDesc.contains("ch=1"));
            expect(noteOnDesc.contains("note=60(C4)"));

            const auto noteOff = juce::MidiMessage::noteOff(2, 69, 0.0f);
            const auto noteOffDesc = describeMidiMessage(noteOff);
            expect(noteOffDesc.startsWith("NoteOff"));
            expect(noteOffDesc.contains("ch=2"));
            expect(noteOffDesc.contains("note=69(A4)"));

            const auto ccPedal = juce::MidiMessage::controllerEvent(1, 64, 127);
            const auto ccDesc = describeMidiMessage(ccPedal);
            expect(ccDesc.startsWith("CC"));
            expect(ccDesc.contains("cc=64"));
            expect(ccDesc.contains("val=127"));

            const auto pitchBend = juce::MidiMessage::pitchWheel(1, 8192);
            const auto pitchDesc = describeMidiMessage(pitchBend);
            expect(pitchDesc.startsWith("PitchBend"));

            const auto progChange = juce::MidiMessage::programChange(1, 5);
            const auto progDesc = describeMidiMessage(progChange);
            expect(progDesc.startsWith("ProgramChange"));
            expect(progDesc.contains("prog=5"));
        });
    }
};

DiagnosticsTest diagnosticsTest;

} // namespace
