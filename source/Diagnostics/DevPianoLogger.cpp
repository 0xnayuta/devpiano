#include "DevPianoLogger.h"

namespace devpiano::diagnostics {

DevPianoLogger::DevPianoLogger() = default;
DevPianoLogger::~DevPianoLogger() = default;

void DevPianoLogger::logMessage(const juce::String& message) {
    // Platform-appropriate output:
    //   Windows -> OutputDebugString (debugger output window)
    //   Linux   -> stderr
    juce::Logger::outputDebugString(message);
}

} // namespace devpiano::diagnostics
