#include "DevPianoLogger.h"

#include <cstddef>

namespace devpiano::diagnostics {

namespace {

constexpr const char* kSessionBanner = "=== DevPiano Diagnostics Session Started ===";

} // anonymous namespace

DevPianoLogger::DevPianoLogger()
    : DevPianoLogger(juce::FileLogger::getSystemLogFileFolder().getChildFile("DevPiano").getChildFile("devpiano.log"),
                     512LL * 1024) {
}

DevPianoLogger::DevPianoLogger(const juce::File& customLogFile, juce::int64 maxTotalFileSizeBytesIn)
    : logFilePath(customLogFile)
    , maxTotalFileSizeBytes(maxTotalFileSizeBytesIn)
    , maxPerFileSizeBytes(maxTotalFileSizeBytesIn > 0 ? maxTotalFileSizeBytesIn / 2 : 0) {
    initFileSink();
}

DevPianoLogger::~DevPianoLogger() {
    if (juce::Logger::getCurrentLogger() == this) {
        juce::Logger::setCurrentLogger(nullptr);
    }
}

juce::File DevPianoLogger::getLogFile() const {
    const juce::ScopedLock sl(lock);
    return logFilePath;
}

juce::File DevPianoLogger::getBackupLogFile() const {
    const juce::ScopedLock sl(lock);
    return backupFilePath;
}

juce::File DevPianoLogger::getLogDirectory() const {
    return getLogFile().getParentDirectory();
}

bool DevPianoLogger::hasActiveFileLogger() const noexcept {
    const juce::ScopedLock sl(lock);
    return fileSinkActive;
}

bool DevPianoLogger::hasFileError() const noexcept {
    const juce::ScopedLock sl(lock);
    return fileErrorOccurred;
}

juce::String DevPianoLogger::getLastError() const {
    const juce::ScopedLock sl(lock);
    return lastErrorMessage;
}

DevPianoLogger* DevPianoLogger::getCurrentDevPianoLogger() noexcept {
    return dynamic_cast<DevPianoLogger*>(juce::Logger::getCurrentLogger());
}

juce::File DevPianoLogger::deriveBackupFile(const juce::File& activeFile) {
    if (activeFile == juce::File()) {
        return {};
    }
    const auto ext = activeFile.getFileExtension();
    const auto base = activeFile.getFileNameWithoutExtension();
    if (ext.isNotEmpty()) {
        return activeFile.getSiblingFile(base + ".old" + ext);
    }
    return activeFile.getSiblingFile(base + ".old");
}

juce::String DevPianoLogger::clampUtf8(const juce::String& text, size_t maxBytes) {
    if (text.getNumBytesAsUTF8() <= maxBytes) {
        return text;
    }
    if (maxBytes == 0) {
        return {};
    }

    auto p = text.getCharPointer();
    const char* start = p.getAddress();
    const char* lastValid = start;

    while (!p.isEmpty()) {
        auto nextP = p;
        nextP.getAndAdvance();
        const auto bytes = static_cast<size_t>(nextP.getAddress() - start);
        if (bytes > maxBytes) {
            break;
        }
        lastValid = nextP.getAddress();
        p = nextP;
    }

    return { juce::CharPointer_UTF8(start), juce::CharPointer_UTF8(lastValid) };
}

bool DevPianoLogger::trimFileToBudget(const juce::File& file, juce::int64 maxBytes) {
    if (!file.existsAsFile()) {
        return true;
    }
    const auto fileSize = file.getSize();
    if (fileSize <= maxBytes) {
        return true;
    }
    if (maxBytes <= 0) {
        return file.deleteFile();
    }

    juce::MemoryBlock tail;
    {
        juce::FileInputStream in(file);
        if (!in.openedOk()) {
            return false;
        }

        if (!in.setPosition(fileSize - maxBytes)) {
            return false;
        }
        juce::int64 startPos = fileSize - maxBytes;
        bool foundNewline = false;
        while (!in.isExhausted()) {
            const char c = in.readByte();
            if (c == '\n') {
                startPos = in.getPosition();
                foundNewline = true;
                break;
            }
        }

        if (!foundNewline) {
            if (!in.setPosition(fileSize - maxBytes)) {
                return false;
            }
            startPos = fileSize - maxBytes;
            while (!in.isExhausted()) {
                const auto byteVal = static_cast<unsigned char>(in.readByte());
                if ((byteVal & 0xC0) != 0x80) {
                    startPos = in.getPosition() - 1;
                    break;
                }
            }
        }

        const auto remaining = fileSize - startPos;
        if (remaining > 0) {
            if (!in.setPosition(startPos)
                || in.readIntoMemoryBlock(tail, static_cast<std::ptrdiff_t>(remaining))
                    != static_cast<size_t>(remaining)) {
                return false;
            }
        }
    }

    juce::FileOutputStream out(file);
    if (!out.openedOk()) {
        return false;
    }
    if (!out.setPosition(0) || out.truncate().failed()) {
        return false;
    }
    if (tail.getSize() > 0 && !out.write(tail.getData(), tail.getSize())) {
        return false;
    }
    out.flush();
    return out.getStatus().wasOk();
}

void DevPianoLogger::initFileSink() {
    if (logFilePath == juce::File()) {
        fileSinkActive = false;
        return;
    }

    if (maxTotalFileSizeBytes <= 0 || maxPerFileSizeBytes <= 0) {
        failFileSink("Log file budget must be greater than zero");
        return;
    }

    backupFilePath = deriveBackupFile(logFilePath);

    const auto parentDir = logFilePath.getParentDirectory();
    if (!parentDir.exists()) {
        const auto result = parentDir.createDirectory();
        if (result.failed()) {
            failFileSink("Failed to create log directory: " + result.getErrorMessage());
            return;
        }
    } else if (!parentDir.isDirectory()) {
        failFileSink("Log directory path is not a directory: " + parentDir.getFullPathName());
        return;
    }

    // Safe startup clamp for existing pre-session files
    if (!trimFileToBudget(backupFilePath, maxPerFileSizeBytes)) {
        failFileSink("Failed to trim backup log file: " + backupFilePath.getFullPathName());
        return;
    }
    if (!trimFileToBudget(logFilePath, maxPerFileSizeBytes)) {
        failFileSink("Failed to trim active log file: " + logFilePath.getFullPathName());
        return;
    }

    // Verify active file can be opened for append
    {
        juce::FileOutputStream out(logFilePath, 256);
        if (!out.openedOk()) {
            failFileSink("Failed to open log file for writing: " + logFilePath.getFullPathName());
            return;
        }
    }

    fileSinkActive = true;

    // Write initial session banner if budget permits
    if (maxPerFileSizeBytes >= 128) {
        logMessage(kSessionBanner);
    }
}

void DevPianoLogger::failFileSink(const juce::String& errorMessage) {
    fileSinkActive = false;
    fileErrorOccurred = true;
    lastErrorMessage = errorMessage;
    juce::Logger::outputDebugString("[DP LOGGER ERROR] " + errorMessage);
}

bool DevPianoLogger::rotateFiles() {
    if (backupFilePath.existsAsFile()) {
        if (!backupFilePath.deleteFile()) {
            failFileSink("Failed to delete old backup log file: " + backupFilePath.getFullPathName());
            return false;
        }
    }

    if (logFilePath.existsAsFile()) {
        if (!logFilePath.moveFileTo(backupFilePath)) {
            failFileSink("Failed to rotate log file to backup: " + logFilePath.getFullPathName());
            return false;
        }
    }

    return true;
}

void DevPianoLogger::logMessage(const juce::String& message) {
    const juce::ScopedLock sl(lock);

    // Platform debugger sink always receives the complete, un-truncated message
    juce::Logger::outputDebugString(message);

    if (!fileSinkActive) {
        return;
    }

    if (maxPerFileSizeBytes <= 0) {
        failFileSink("Log file budget is zero or negative");
        return;
    }

    // Byte-safe UTF-8 clamping for oversized messages (reserving 1 byte for newline)
    juce::String fileMessage = message;
    const auto maxMsgBytes = static_cast<size_t>(juce::jmax(juce::int64(0), maxPerFileSizeBytes - 1));
    if (fileMessage.getNumBytesAsUTF8() > maxMsgBytes) {
        fileMessage = clampUtf8(message, maxMsgBytes);
    }

    const auto* utf8Data = fileMessage.toRawUTF8();
    const size_t messageBytes = std::strlen(utf8Data);
    const size_t totalLineBytes = messageBytes + 1; // +1 for '\n'

    const auto currentActiveSize = logFilePath.existsAsFile() ? logFilePath.getSize() : 0;
    if (currentActiveSize + static_cast<juce::int64>(totalLineBytes) > maxPerFileSizeBytes) {
        if (!rotateFiles()) {
            // Rotation failed; file sink is disabled, message is NOT appended
            return;
        }
    }

    juce::FileOutputStream out(logFilePath, 256);
    if (!out.openedOk()) {
        failFileSink("Failed to open log file for append: " + logFilePath.getFullPathName());
        return;
    }

    bool writeOk = true;
    if (messageBytes > 0) {
        writeOk = out.write(utf8Data, messageBytes);
    }
    if (writeOk) {
        const char nl = '\n';
        writeOk = out.write(&nl, 1);
    }
    out.flush();

    if (!writeOk || out.getStatus().failed()) {
        failFileSink("Failed to write to log file: " + logFilePath.getFullPathName());
        return;
    }

    const auto activeSize = logFilePath.existsAsFile() ? logFilePath.getSize() : 0;
    const auto backupSize = backupFilePath.existsAsFile() ? backupFilePath.getSize() : 0;
    if (activeSize + backupSize > maxTotalFileSizeBytes) {
        failFileSink("Log size exceeded total budget: active=" + juce::String(activeSize)
                     + " backup=" + juce::String(backupSize) + " total=" + juce::String(maxTotalFileSizeBytes));
    }
}

} // namespace devpiano::diagnostics
