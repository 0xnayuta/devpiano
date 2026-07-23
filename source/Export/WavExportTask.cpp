#include "Export/WavExportTask.h"

#include "Recording/PluginOfflineRenderer.h"
#include "Recording/WavFileExporter.h"

#include "Diagnostics/Log.h"
// ============================================================================
WavExportTask::WavExportTask(devpiano::recording::RecordingTake take_, const juce::File& destinationFile_,
                             const devpiano::exporting::WavExportOptions& options_,
                             std::unique_ptr<juce::AudioPluginInstance> offlinePlugin_,
                             juce::Component* parentToCentreAround)
    : ThreadWithProgressWindow(TRANS("Export WAV"), true, true, 10000, {}, parentToCentreAround)
    , take(std::move(take_))
    , destinationFile(destinationFile_)
    , options(options_)
    , offlinePlugin(std::move(offlinePlugin_)) {
    if (auto* w = getAlertWindow())
        w->centreWithSize(400, 120);
}

WavExportTask::~WavExportTask() = default;

// ============================================================================
void WavExportTask::run() {
    using namespace devpiano::exporting;

    setProgress(0.0);
    setStatusMessage(TRANS("Exporting..."));

    // Build a progress callback that updates the dialog and checks for cancel.
    // Returns true to continue, false to abort.
    auto progressCallback = [this](double p) -> bool {
        setProgress(juce::jlimit(0.0, 1.0, p));
        const auto percent = static_cast<int>(p * 100.0);
        if (percent % 10 == 0 || p >= 1.0)
            setStatusMessage(TRANS("Exporting...") + " " + juce::String(percent) + "%");
        return !threadShouldExit();
    };

    if (offlinePlugin != nullptr) {
        // Plugin offline-render path
        if (threadShouldExit()) {
            success = false;
            errorMessage = TRANS("Export cancelled.");
            destinationFile.deleteFile();  // best-effort; log failure below
        }

        if (renderTakeWithOfflinePlugin(take, destinationFile, options, *offlinePlugin, progressCallback)) {
            success = true;
        } else {
            if (threadShouldExit()) {
                errorMessage = TRANS("Export cancelled.");
            if (!destinationFile.deleteFile())
                DP_LOG_WARN("Failed to clean up cancelled WAV: " + destinationFile.getFullPathName());
            } else {
                errorMessage = TRANS("Export failed during plugin rendering.");
            }
            success = false;
        }
    } else {
        // Sine-synth fallback path
        if (exportTakeAsWavFile(take, destinationFile, options, progressCallback)) {
            success = true;
        } else {
            if (threadShouldExit()) {
                errorMessage = TRANS("Export cancelled.");
            if (!destinationFile.deleteFile())
                DP_LOG_WARN("Failed to clean up cancelled WAV: " + destinationFile.getFullPathName());
            } else {
                errorMessage = TRANS("Export failed during sine synth rendering.");
            }
            success = false;
        }
    }

    if (success) {
        setProgress(1.0);
        setStatusMessage(TRANS("Export complete."));
    }
}
