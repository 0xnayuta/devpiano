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
//   WavExportTask task(takeCopy, file, options, std::move(offlinePlugin), parentComp);
//   task.runThread();                         // blocks with nested message loop
//   if (task.wasSuccessful()) { /* ok */ }
// ============================================================================
class WavExportTask : private juce::Thread, private juce::Timer {
public:
    WavExportTask(devpiano::recording::RecordingTake take, const juce::File& destinationFile,
                  const devpiano::exporting::WavExportOptions& options,
                  std::unique_ptr<juce::AudioPluginInstance> offlinePlugin = nullptr,
                  juce::Component* parentToCentreAround = nullptr);

    ~WavExportTask() override;

    /// Runs the export on a background thread while displaying the JIVE progress dialog.
    /// Returns true if completed successfully, false if cancelled or failed.
    bool runThread();

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

    // Active progress dialog references (message thread only)
    juce::Component::SafePointer<juce::DialogWindow> activeDialog;
    juce::Component::SafePointer<juce::Label> statusLabel;
    juce::Component::SafePointer<juce::ProgressBar> progressBar;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WavExportTask)
};
