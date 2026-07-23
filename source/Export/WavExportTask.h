#pragma once

#include <JuceHeader.h>

#include "Export/WavExportOptions.h"
#include "Recording/RecordingEngine.h"

#include <atomic>
#include <memory>

// ============================================================================
// WavExportTask — WAV export with progress dialog and cancel support.
//
// Wraps renderTakeWithOfflinePlugin (plugin path) or exportTakeAsWavFile (sine
// synth fallback) in a JUCE ThreadWithProgressWindow so the render loop runs
// on a background thread while the user sees a ProgressBar and can cancel.
//
// Usage (message thread):
//   WavExportTask task(takeCopy, file, options, std::move(offlinePlugin));
//   task.runThread();                         // blocks with nested message loop
//   if (task.wasSuccessful()) { /* ok */ }
//
// The offline plugin instance (if any) must be created on the message thread
// before constructing this task — AudioPluginFormatManager::createPluginInstance
// is not thread-safe.
// ============================================================================
class WavExportTask : public juce::ThreadWithProgressWindow {
public:
    WavExportTask(devpiano::recording::RecordingTake take, const juce::File& destinationFile,
                  const devpiano::exporting::WavExportOptions& options,
                  std::unique_ptr<juce::AudioPluginInstance> offlinePlugin,
                  juce::Component* parentToCentreAround = nullptr);

    ~WavExportTask() override;

    [[nodiscard]] bool wasSuccessful() const noexcept {
        return success;
    }
    [[nodiscard]] const juce::String& getErrorMessage() const noexcept {
        return errorMessage;
    }

private:
    void run() override;

    devpiano::recording::RecordingTake take;
    const juce::File destinationFile;
    devpiano::exporting::WavExportOptions options;
    std::unique_ptr<juce::AudioPluginInstance> offlinePlugin;

    std::atomic<bool> success { false };
    juce::String errorMessage;
};
