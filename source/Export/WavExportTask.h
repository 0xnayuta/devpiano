#pragma once

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

#include "Export/WavExportOptions.h"
#include "Recording/RecordingEngine.h"

// ============================================================================
// WavExportTask — WAV export with JIVE-driven progress dialog and cancel support.
//
// Refactored in Phase 15-D to replace legacy AlertWindow with JiveModalDialog
// declarative progress layout, providing a theme-consistent dark ProgressBar.
//
// Usage (message thread):
//   task.startAsync([](bool ok, const juce::String& error) { ... });
// ============================================================================
class WavExportTask : private juce::Thread, private juce::Timer {
public:
    using CompletionCallback = std::function<void(bool success, const juce::String& errorMessage)>;

    WavExportTask(devpiano::recording::RecordingTake take, juce::File destinationFile,
                  const devpiano::exporting::WavExportOptions& options,
                  std::unique_ptr<juce::AudioPluginInstance> offlinePlugin = nullptr,
                  juce::Component* parentToCentreAround = nullptr);

    ~WavExportTask() override;

    /// Starts asynchronous export on a background thread while displaying the JIVE progress dialog.
    /// Non-blocking: returns immediately; invokes onComplete on the message thread when finished.
    void startAsync(CompletionCallback onComplete);

    /// Runs synchronously without UI dialogs (for headless/testing environments).
    /// Returns true if completed successfully, false if cancelled or failed.
    bool runSync();
    [[nodiscard]] bool wasSuccessful() const noexcept {
        return success.load();
    }

    [[nodiscard]] juce::String getErrorMessage() const {
        const juce::ScopedLock sl(messageLock);
        return errorMessage;
    }

private:
    void run() override;
    void timerCallback() override;

    void setProgress(double newProgress);
    void setStatusMessage(const juce::String& newStatusMessage);
    void failExport(const juce::String& errorMsg, bool isCancellation = false);

    devpiano::recording::RecordingTake take;
    const juce::File destinationFile;
    devpiano::exporting::WavExportOptions options;
    std::unique_ptr<juce::AudioPluginInstance> offlinePlugin;
    juce::Component* parentComponent = nullptr;

    std::atomic<bool> success { false };
    std::atomic<bool> cancelRequested { false };
    std::atomic<bool> finished { false };
    std::atomic<double> currentProgress { 0.0 };

    juce::CriticalSection messageLock;
    juce::String currentStatusMessage;
    juce::String errorMessage;
    CompletionCallback completionCallback;

    // Active progress dialog reference (message thread only)
    juce::Component::SafePointer<juce::DialogWindow> activeDialog;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WavExportTask)
};
