#pragma once

#include <juce_core/juce_core.h>
#include <memory>

namespace devpiano::diagnostics {

//! Custom juce::Logger implementation providing a dual-sink logging architecture:
//!   - File sink: bounded rolling file log with active file and single backup file,
//!     enforcing a combined size budget and UTF-8 safe clamping.
//!   - Debugger sink: platform debug output (OutputDebugString on Windows, stderr on Linux)
//!     which receives full un-truncated messages.
//! Installed via juce::Logger::setCurrentLogger() at application startup.
class DevPianoLogger : public juce::Logger {
public:
    /// Default constructor: creates a logger in the standard platform app log directory
    /// (DevPiano/devpiano.log) with a 512 KiB total combined budget (256 KiB per file).
    DevPianoLogger();

    /// Custom file constructor: allows specifying a custom log file and total combined budget.
    explicit DevPianoLogger(const juce::File& customLogFile, juce::int64 maxTotalFileSizeBytes = 512LL * 1024);

    ~DevPianoLogger() override;

    /// Returns the active log file path.
    [[nodiscard]] juce::File getLogFile() const;

    /// Returns the single backup log file path.
    [[nodiscard]] juce::File getBackupLogFile() const;

    /// Returns the directory where log files are stored.
    [[nodiscard]] juce::File getLogDirectory() const;

    /// Returns true if the file sink is currently active and accepting writes.
    [[nodiscard]] bool hasActiveFileLogger() const noexcept;

    /// Returns true if a file open, write, or rotation error occurred.
    [[nodiscard]] bool hasFileError() const noexcept;

    /// Returns the last observable file error message.
    [[nodiscard]] juce::String getLastError() const;

    /// Returns the current active DevPianoLogger instance if installed, or nullptr.
    [[nodiscard]] static DevPianoLogger* getCurrentDevPianoLogger() noexcept;

protected:
    void logMessage(const juce::String& message) override;

private:
    static juce::File deriveBackupFile(const juce::File& activeFile);
    static juce::String clampUtf8(const juce::String& text, size_t maxBytes);
    static bool trimFileToBudget(const juce::File& file, juce::int64 maxBytes);

    void initFileSink();
    void failFileSink(const juce::String& errorMessage);
    bool rotateFiles();

    juce::File logFilePath;
    juce::File backupFilePath;
    juce::int64 maxTotalFileSizeBytes { 512LL * 1024 };
    juce::int64 maxPerFileSizeBytes { 256LL * 1024 };
    bool fileSinkActive { false };
    bool fileErrorOccurred { false };
    juce::String lastErrorMessage;
    mutable juce::CriticalSection lock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DevPianoLogger)
};

} // namespace devpiano::diagnostics
