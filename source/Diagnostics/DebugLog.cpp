#include "DebugLog.h"

#include <cstdio>

namespace
{

void outputLine(const char* prefix, const char* message)
{
    // UseJUCE's Logger for Release-capable output; DBG additionally writes
    // to the debugger output window in Debug builds.
#if defined(JUCE_DEBUG) || defined(DEBUG)
    DBG(prefix << message);
#endif
    juce::Logger::writeToLog(prefix + juce::String(message));
}

} // anonymous namespace

namespace devpiano::diagnostics
{

void logInfo(const char* message)
{
    outputLine("[DP INFO] ", message);
}

void logWarn(const char* message)
{
    outputLine("[DP WARN] ", message);
}

void logError(const char* message)
{
    outputLine("[DP ERROR] ", message);
}

void traceMidi(const char* message, const char* stage)
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

} // namespace devpiano::diagnostics