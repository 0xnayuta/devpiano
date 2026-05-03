#pragma once

#include <JuceHeader.h>

namespace devpiano::diagnostics
{

void logInfo(const char* message);
void logWarn(const char* message);
void logError(const char* message);

} // namespace devpiano::diagnostics

// =============================================================================
// Public logging macros
// =============================================================================

#if defined(JUCE_DEBUG) || defined(DEBUG)
    //! General debug output — only enabled in Debug builds.
    #define DP_DEBUG_LOG(message) \
        do { devpiano::diagnostics::logInfo((message)); } while (false)
#else
    #define DP_DEBUG_LOG(message) \
        do { } while (false)
#endif

//! Info-level log — enabled in both Debug and Release builds.
#define DP_LOG_INFO(message) \
    do { devpiano::diagnostics::logInfo((message)); } while (false)

//! Warning-level log — enabled in both Debug and Release builds.
#define DP_LOG_WARN(message) \
    do { devpiano::diagnostics::logWarn((message)); } while (false)

//! Error-level log — enabled in both Debug and Release builds.
#define DP_LOG_ERROR(message) \
    do { devpiano::diagnostics::logError((message)); } while (false)

//! MIDI trace — only enabled in Debug builds.
//! Captures stage label plus a formatted MIDI message description.
#define DP_TRACE_MIDI(message, stage) \
    do { devpiano::diagnostics::traceMidi((message), (stage)); } while (false)

namespace devpiano::diagnostics
{

//! Formats and traces a MIDI message with a user-provided stage label.
//! Only active in Debug builds; a no-op in Release.
void traceMidi(const char* message, const char* stage);

} // namespace devpiano::diagnostics