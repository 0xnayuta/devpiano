#include "Export/WavExportTask.h"

#include "Recording/PluginOfflineRenderer.h"
#include "Recording/WavFileExporter.h"

#include "Diagnostics/Log.h"
// ============================================================================
WavExportTask::WavExportTask(
    // NOLINTNEXTLINE(modernize-pass-by-value) - RecordingTake/juce::File 均非重型类型，按值 + move 与 const& 开销相同
    devpiano::recording::RecordingTake take_, const juce::File& destinationFile_,
    // NOLINTNEXTLINE(modernize-pass-by-value) - WavExportOptions 为 POD：move 被 move-const-arg 否决，const& 最优
    const devpiano::exporting::WavExportOptions& options_, std::unique_ptr<juce::AudioPluginInstance> offlinePlugin_,
    juce::Component* parentToCentreAround)
    : ThreadWithProgressWindow(TRANS("Export WAV"), true, true, 10000, {}, parentToCentreAround)
    , take(std::move(take_))
    , destinationFile(destinationFile_)
    , options(options_)
    , offlinePlugin(std::move(offlinePlugin_)) {
    if (auto* w = getAlertWindow()) {
        w->centreWithSize(400, 120);
    }
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
        if (percent % 10 == 0 || p >= 1.0) {
            setStatusMessage(TRANS("Exporting...") + " " + juce::String(percent) + "%");
        }
        return !threadShouldExit();
    };

    // ERR-015：渲染路径（插件离线渲染 / 文件写出）可能抛异常，统一捕获为
    // 失败结果并清理残留目标文件，避免 run() 异常逸出导致线程悬挂。
    try {
        if (offlinePlugin != nullptr) {
            // Plugin offline-render path
            if (threadShouldExit()) {
                success = false;
                errorMessage = TRANS("Export cancelled.");
                destinationFile.deleteFile(); // best-effort; log failure below
            }

            if (renderTakeWithOfflinePlugin(take, destinationFile, options, *offlinePlugin, progressCallback)) {
                success = true;
            } else {
                if (threadShouldExit()) {
                    errorMessage = TRANS("Export cancelled.");
                    if (!destinationFile.deleteFile()) {
                        DP_LOG_WARN("Failed to clean up cancelled WAV: " + destinationFile.getFullPathName());
                    }
                } else {
                    errorMessage = TRANS("Export failed during plugin rendering.");
                    // ERR-009：非取消失败也清理残留的部分文件。
                    if (!destinationFile.deleteFile()) {
                        DP_LOG_WARN("Failed to clean up failed WAV: " + destinationFile.getFullPathName());
                    }
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
                    if (!destinationFile.deleteFile()) {
                        DP_LOG_WARN("Failed to clean up cancelled WAV: " + destinationFile.getFullPathName());
                    }
                } else {
                    errorMessage = TRANS("Export failed during sine synth rendering.");
                    // ERR-009：非取消失败也清理残留的部分文件。
                    if (!destinationFile.deleteFile()) {
                        DP_LOG_WARN("Failed to clean up failed WAV: " + destinationFile.getFullPathName());
                    }
                }
                success = false;
            }
        }
    } catch (const std::exception& e) {
        success = false;
        errorMessage = TRANS("Export failed unexpectedly.");
        DP_LOG_ERROR("[Export] WAV export threw: " + juce::String(e.what()));
        if (!destinationFile.deleteFile()) {
            DP_LOG_WARN("Failed to clean up failed WAV: " + destinationFile.getFullPathName());
        }
    } catch (...) {
        success = false;
        errorMessage = TRANS("Export failed unexpectedly.");
        DP_LOG_ERROR("[Export] WAV export threw an unknown exception");
        if (!destinationFile.deleteFile()) {
            DP_LOG_WARN("Failed to clean up failed WAV: " + destinationFile.getFullPathName());
        }
    }

    if (success) {
        setProgress(1.0);
        setStatusMessage(TRANS("Export complete."));
        // ERR-012：run() 内补结果日志（此前只有调用方日志，线程内无观测点）。
        DP_LOG_INFO("[Export] WAV exported: " + destinationFile.getFullPathName());
    } else {
        DP_LOG_WARN("[Export] WAV export " + errorMessage);
    }
}
