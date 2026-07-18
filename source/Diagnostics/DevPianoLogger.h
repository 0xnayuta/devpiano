#pragma once

#include <JuceHeader.h>

namespace devpiano::diagnostics {

//! Custom juce::Logger subclass that routes all log output through
//! outputDebugString for platform-appropriate visibility:
//!   - Windows:  debugger output window (OutputDebugString)
//!   - Linux:    stderr
//! Set via juce::Logger::setCurrentLogger() at application startup.
class DevPianoLogger : public juce::Logger {
public:
    DevPianoLogger();
    ~DevPianoLogger() override;

protected:
    void logMessage(const juce::String& message) override;
};

} // namespace devpiano::diagnostics
