#pragma once

#include <JuceHeader.h>

// =============================================================================
// Public logging macros
// =============================================================================
//
// Replaces the old DebugLog.h macro system.  All output is routed through
// juce::Logger::writeToLog() which dispatches to a DevPianoLogger instance
// (when one is registered) or falls back to outputDebugString().
//
// Three persistent levels (always compiled):
//   DP_LOG_INFO, DP_LOG_WARN, DP_LOG_ERROR
//
// Two debug-only levels (compiled away in Release):
//   DP_DEBUG_LOG, DP_TRACE_MIDI

//! Info-level log — enabled in both Debug and Release builds.
#define DP_LOG_INFO(message)                                                                                           \
    do {                                                                                                               \
        juce::Logger::writeToLog(juce::String("[DP INFO] ") + (message));                                              \
    } while (false)

//! Warning-level log — enabled in both Debug and Release builds.
#define DP_LOG_WARN(message)                                                                                           \
    do {                                                                                                               \
        juce::Logger::writeToLog(juce::String("[DP WARN] ") + (message));                                              \
    } while (false)

//! Error-level log — enabled in both Debug and Release builds.
#define DP_LOG_ERROR(message)                                                                                          \
    do {                                                                                                               \
        juce::Logger::writeToLog(juce::String("[DP ERROR] ") + (message));                                             \
    } while (false)

#if defined(JUCE_DEBUG) || defined(DEBUG)
//! General debug output — only enabled in Debug builds.
#define DP_DEBUG_LOG(message)                                                                                          \
    do {                                                                                                               \
        juce::Logger::writeToLog(juce::String("[DP DEBUG] ") + (message));                                             \
    } while (false)

//! MIDI trace — only enabled in Debug builds.
//! Captures a stage label plus a formatted MIDI message description.
#define DP_TRACE_MIDI(message, stage)                                                                                  \
    do {                                                                                                               \
        juce::Logger::writeToLog(juce::String("[DP MIDI:") + (stage) + "] " + (message));                              \
    } while (false)
#else
#define DP_DEBUG_LOG(message)                                                                                          \
    do {                                                                                                               \
    } while (false)
#define DP_TRACE_MIDI(message, stage)                                                                                  \
    do {                                                                                                               \
    } while (false)
#endif
