#include "Export/WavExportTask.h"

#include "Diagnostics/Log.h"
#include "Recording/PluginOfflineRenderer.h"
#include "Recording/WavFileExporter.h"
#include "UI/ViewHost.h"
#include "UI/jive/DesignTokens.h"
#include "UI/jive/JiveModalDialog.h"

namespace {

struct ProgressContentWrapper final : public juce::Component {
    ProgressContentWrapper(devpiano::ui::ViewHost vh, std::function<void()> onCancelFn)
        : viewHost(std::move(vh))
        , onCancel(std::move(onCancelFn)) {
        if (auto* comp = viewHost.getRootComponent()) {
            addAndMakeVisible(*comp);
        }
        setSize(380, 140);
        setWantsKeyboardFocus(true);
    }

    ~ProgressContentWrapper() override {
        if (!completed && onCancel) {
            onCancel();
        }
        viewHost.reset();
    }

    void markCompleted() noexcept {
        completed = true;
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(devpiano::jive::DesignTokens::get().mainBg());
    }

    void resized() override {
        viewHost.setBounds(getLocalBounds());
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if (key.isKeyCode(juce::KeyPress::escapeKey)) {
            if (onCancel) {
                onCancel();
            }
            return true;
        }
        return false;
    }
    devpiano::ui::ViewHost viewHost;
    std::function<void()> onCancel;
    bool completed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProgressContentWrapper)
};

} // namespace

// ============================================================================
// WavExportTask Implementation
// ============================================================================

WavExportTask::WavExportTask(devpiano::recording::RecordingTake take_, juce::File destinationFile_,
                             const devpiano::exporting::WavExportOptions& options_,
                             std::unique_ptr<juce::AudioPluginInstance> offlinePlugin_,
                             juce::Component* parentToCentreAround)
    : juce::Thread("WAV Export Thread")
    , take(std::move(take_))
    , destinationFile(std::move(destinationFile_))
    , options(options_)
    , offlinePlugin(std::move(offlinePlugin_))
    , parentComponent(parentToCentreAround) {
}

WavExportTask::~WavExportTask() {
    stopTimer();
    cancelRequested = true;
    signalThreadShouldExit();
    stopThread(3000);
    if (activeDialog != nullptr) {
        activeDialog->exitModalState(0);
    }
}

void WavExportTask::setProgress(double newProgress) {
    currentProgress.store(juce::jlimit(0.0, 1.0, newProgress));
}

void WavExportTask::setStatusMessage(const juce::String& newStatusMessage) {
    const juce::ScopedLock sl(messageLock);
    currentStatusMessage = newStatusMessage;
}

bool WavExportTask::runThread(bool showProgressDialog) {
    JUCE_ASSERT_MESSAGE_THREAD

    success.store(false);
    cancelRequested.store(false);
    finished.store(false);
    currentProgress.store(0.0);
    {
        const juce::ScopedLock sl(messageLock);
        currentStatusMessage = TRANS("Exporting...");
        errorMessage.clear();
    }

    if (!showProgressDialog) {
        // Headless execution: start background audio rendering without creating OS windows
        startThread(juce::Thread::Priority::normal);

        while (isThreadRunning()) {
            juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
        }

        stopThread(3000);
        return success.load() && !cancelRequested.load();
    }
    // Build JIVE progress dialog layout
    auto layout = devpiano::ui::jive::JiveModalDialog::makeProgressLayout(TRANS("Exporting..."), 380, 140);
    devpiano::ui::ViewHost viewHost;
    viewHost.loadLayout(layout, true);

    if (auto* cancelBtn = viewHost.find<juce::Button>("dialog-cancel-btn")) {
        cancelBtn->onClick = [this] {
            cancelRequested.store(true);
            signalThreadShouldExit();
        };
    }

    // Start background audio rendering thread
    startThread(juce::Thread::Priority::normal);

    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = TRANS("Export WAV");
    opts.dialogBackgroundColour = devpiano::jive::DesignTokens::get().mainBg();
    opts.componentToCentreAround = parentComponent;
    opts.resizable = false;
    opts.escapeKeyTriggersCloseButton = false; // Cancellation handled gracefully via cancelRequested flag

    auto contentWrapper = std::make_unique<ProgressContentWrapper>(std::move(viewHost), [this] {
        cancelRequested.store(true);
        signalThreadShouldExit();
    });

    if (parentComponent != nullptr) {
        contentWrapper->setLookAndFeel(&parentComponent->getLookAndFeel());
    }
    opts.content.setOwned(contentWrapper.release());

    auto* dialog = opts.launchAsync();
    activeDialog = dialog;

    startTimerHz(30);

    // Run nested message loop until thread finishes or cancel occurs
#if JUCE_MODAL_LOOPS_PERMITTED
    while (isTimerRunning()) {
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    }
#else
    // DevPiano is a desktop application where JUCE_MODAL_LOOPS_PERMITTED is required
    // for nested progress dialog dispatch loop.
    jassertfalse;
    DP_LOG_ERROR("[Export] WAV export requires JUCE_MODAL_LOOPS_PERMITTED=1");
    while (isThreadRunning()) {
        juce::Thread::sleep(10);
    }
#endif
    stopTimer();
    if (activeDialog != nullptr) {
        if (auto* wrapper = dynamic_cast<ProgressContentWrapper*>(activeDialog->getContentComponent())) {
            wrapper->markCompleted();
        }
        activeDialog->exitModalState(0);
        activeDialog = nullptr;
    }

    stopThread(3000);
    return success.load() && !cancelRequested.load();
}

void WavExportTask::timerCallback() {
    const bool isRunning = isThreadRunning();

    if (!isRunning || finished.load() || activeDialog == nullptr || cancelRequested.load()) {
        finished.store(true);
        stopTimer();
        if (activeDialog != nullptr) {
            if (auto* wrapper = dynamic_cast<ProgressContentWrapper*>(activeDialog->getContentComponent())) {
                wrapper->markCompleted();
            }
            activeDialog->exitModalState(0);
            activeDialog = nullptr;
        }
        return;
    }

    if (activeDialog != nullptr) {
        if (auto* wrapper = dynamic_cast<ProgressContentWrapper*>(activeDialog->getContentComponent())) {
            if (wrapper->viewHost.isValid()) {
                juce::String msg;
                {
                    const juce::ScopedLock sl(messageLock);
                    msg = currentStatusMessage;
                }
                wrapper->viewHost.setText("progress-status-message", msg);
                wrapper->viewHost.setProperty("dialog-progress-bar", "value", currentProgress.load());
            }
        }
    }
}

void WavExportTask::failExport(const juce::String& errorMsg, bool isCancellation) {
    success.store(false);
    {
        const juce::ScopedLock sl(messageLock);
        errorMessage = isCancellation ? TRANS("Export cancelled.") : errorMsg;
    }
    if (destinationFile.existsAsFile() && !destinationFile.deleteFile()) {
        DP_LOG_WARN(
            juce::String(isCancellation ? "Failed to clean up cancelled WAV: " : "Failed to clean up failed WAV: ")
            + destinationFile.getFullPathName());
    }
}

void WavExportTask::run() {
    using namespace devpiano::exporting;

    setProgress(0.0);
    setStatusMessage(TRANS("Exporting..."));

    auto isCancelled = [this]() noexcept { return threadShouldExit() || cancelRequested.load(); };

    auto progressCallback = [this, &isCancelled](double p) -> bool {
        setProgress(p);
        const auto percent = static_cast<int>(p * 100.0);
        if (percent % 10 == 0 || p >= 1.0) {
            setStatusMessage(TRANS("Exporting...") + " " + juce::String(percent) + "%");
        }
        return !isCancelled();
    };

    // ERR-015: Render path may throw; catch all exceptions, report failure and clean up destination file.
    try {
        if (isCancelled()) {
            failExport({}, true);
        } else {
            const bool renderOk = (offlinePlugin != nullptr)
                ? renderTakeWithOfflinePlugin(take, destinationFile, options, *offlinePlugin, progressCallback)
                : exportTakeAsWavFile(take, destinationFile, options, progressCallback);

            if (renderOk) {
                success.store(true);
            } else {
                const auto defaultErr = (offlinePlugin != nullptr)
                    ? TRANS("Export failed during plugin rendering.")
                    : TRANS("Export failed during built-in synth rendering.");
                failExport(defaultErr, isCancelled());
            }
        }
    } catch (const std::exception& e) {
        DP_LOG_ERROR("[Export] WAV export threw: " + juce::String(e.what()));
        failExport(TRANS("Export failed unexpectedly."));
    } catch (...) {
        DP_LOG_ERROR("[Export] WAV export threw an unknown exception");
        failExport(TRANS("Export failed unexpectedly."));
    }

    if (success.load()) {
        setProgress(1.0);
        setStatusMessage(TRANS("Export complete."));
        DP_LOG_INFO("[Export] WAV exported: " + destinationFile.getFullPathName());
    } else {
        const juce::ScopedLock sl(messageLock);
        DP_LOG_WARN("[Export] WAV export " + errorMessage);
    }

    finished.store(true);
}
