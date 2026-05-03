#include "DebugLog.h"

namespace
{

[[nodiscard]] juce::String fromUtf8(const char* text)
{
    return juce::String(juce::CharPointer_UTF8(text != nullptr ? text : ""));
}

void outputLine(const juce::String& prefix, const juce::String& message)
{
    // Use JUCE's Logger for Release-capable output; DBG additionally writes
    // to the debugger output window in Debug builds.
#if defined(JUCE_DEBUG) || defined(DEBUG)
    DBG(prefix + message);
#endif
    juce::Logger::writeToLog(prefix + message);
}

} // anonymous namespace

namespace devpiano::diagnostics
{

void logInfo(const juce::String& message)
{
    outputLine("[DP INFO] ", message);
}

void logWarn(const juce::String& message)
{
    outputLine("[DP WARN] ", message);
}

void logError(const juce::String& message)
{
    outputLine("[DP ERROR] ", message);
}

void logInfo(const char* utf8Message)
{
    logInfo(fromUtf8(utf8Message));
}

void logWarn(const char* utf8Message)
{
    logWarn(fromUtf8(utf8Message));
}

void logError(const char* utf8Message)
{
    logError(fromUtf8(utf8Message));
}

void traceMidi(const juce::String& message, const juce::String& stage)
{
#if defined(JUCE_DEBUG) || defined(DEBUG)
    juce::String output = "[DP MIDI:";
    output += stage;
    output += "] ";
    output += message;
    DBG(output);
#endif
    // No-op in Release: traceMidi is debug-only and must have zero
    // side effects when JUCE_DEBUG / DEBUG is not defined.
    juce::ignoreUnused(message, stage);
}

void traceMidi(const char* utf8Message, const char* utf8Stage)
{
    traceMidi(fromUtf8(utf8Message), fromUtf8(utf8Stage));
}

} // namespace devpiano::diagnostics
