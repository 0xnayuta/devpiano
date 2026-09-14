#pragma once

#include <juce_core/juce_core.h>
#include <memory>

namespace devpiano::diagnostics {

//! Custom juce::Logger implementation providing a dual-sink logging architecture:
//!   - File sink: persistent log file with rolling size limit in the OS-appropriate
//!     application data directory (%APPDATA%/devpiano on Windows, ~/.config/devpiano on Linux)
//!   - Debugger sink: platform debug output (OutputDebugString on Windows, stderr on Linux)
//! Installed via juce::Logger::setCurrentLogger() at application startup.
class DevPianoLogger : public juce::Logger {
public:
    /// Default constructor: creates a FileLogger in the standard platform app log directory
    /// with a 512 KB rolling size limit.
    DevPianoLogger();

    /// Custom file constructor: allows specifying a custom log file and initial size limit (e.g. for testing).
    explicit DevPianoLogger(const juce::File& customLogFile, juce::int64 maxInitialFileSizeBytes = 512LL * 1024);

    ~DevPianoLogger() override;

    /// Returns the active log file (may be a default or nonexistent file if file logging failed).
    [[nodiscard]] juce::File getLogFile() const;

    /// Returns the directory where the log file is stored.
    [[nodiscard]] juce::File getLogDirectory() const;

    /// Returns true if the file logger sink was successfully opened.
    [[nodiscard]] bool hasActiveFileLogger() const noexcept;

    /// Returns the current active DevPianoLogger instance if installed, or nullptr.
    [[nodiscard]] static DevPianoLogger* getCurrentDevPianoLogger() noexcept;

protected:
    void logMessage(const juce::String& message) override;

private:
    std::unique_ptr<juce::FileLogger> fileLogger;
    juce::File logFilePath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DevPianoLogger)
};

} // namespace devpiano::diagnostics
