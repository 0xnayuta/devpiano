#include "DevPianoLogger.h"

namespace devpiano::diagnostics {

DevPianoLogger::DevPianoLogger()
    : DevPianoLogger(juce::FileLogger::getSystemLogFileFolder().getChildFile("DevPiano").getChildFile("devpiano.log"),
                     512LL * 1024) {
}

DevPianoLogger::DevPianoLogger(const juce::File& customLogFile, juce::int64 maxInitialFileSizeBytes)
    : logFilePath(customLogFile) {
    if (customLogFile != juce::File()) {
        try {
            fileLogger = std::make_unique<juce::FileLogger>(
                customLogFile, "=== DevPiano Diagnostics Session Started ===", maxInitialFileSizeBytes);
        } catch (...) {
            fileLogger.reset();
        }
    }
}

DevPianoLogger::~DevPianoLogger() {
    if (juce::Logger::getCurrentLogger() == this) {
        juce::Logger::setCurrentLogger(nullptr);
    }
}

juce::File DevPianoLogger::getLogFile() const {
    if (fileLogger != nullptr) {
        return fileLogger->getLogFile();
    }
    return logFilePath;
}

juce::File DevPianoLogger::getLogDirectory() const {
    return getLogFile().getParentDirectory();
}

bool DevPianoLogger::hasActiveFileLogger() const noexcept {
    return fileLogger != nullptr;
}

DevPianoLogger* DevPianoLogger::getCurrentDevPianoLogger() noexcept {
    return dynamic_cast<DevPianoLogger*>(juce::Logger::getCurrentLogger());
}

void DevPianoLogger::logMessage(const juce::String& message) {
    if (fileLogger != nullptr) {
        fileLogger->logMessage(message);
#if !(defined(JUCE_DEBUG) || defined(DEBUG))
        // In Release builds, FileLogger::logMessage skips DBG(), so we explicitly
        // forward to outputDebugString to guarantee dual-sink output.
        juce::Logger::outputDebugString(message);
#endif
    } else {
        // Fallback when file logger is unavailable
        juce::Logger::outputDebugString(message);
    }
}

} // namespace devpiano::diagnostics
