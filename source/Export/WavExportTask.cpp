#include "Export/WavExportTask.h"

#include "Diagnostics/Log.h"
#include "Recording/PluginOfflineRenderer.h"
#include "Recording/WavFileExporter.h"
#include "UI/jive/DesignTokens.h"
#include "UI/jive/JiveModalDialog.h"
#include "UI/jive/JiveUtils.h"
#include "UI/jive/StyleCatalog.h"

namespace {

struct ProgressContentWrapper final : public juce::Component {
    ProgressContentWrapper(std::unique_ptr<::jive::GuiItem> item, std::unique_ptr<::jive::Interpreter> interp,
                           std::function<void()> onCancelFn)
        : rootItem(std::move(item))
        , interpreter(std::move(interp))
        , onCancel(std::move(onCancelFn)) {
        if (rootItem != nullptr) {
            if (auto comp = rootItem->getComponent()) {
                addAndMakeVisible(*comp);
            }
        }
        setSize(380, 140);
        setWantsKeyboardFocus(true);
    }

    ~ProgressContentWrapper() override {
        devpiano::ui::jive::safeCleanupJiveTree(rootItem);
        interpreter.reset();
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(devpiano::jive::DesignTokens::get().mainBg());
    }

    void resized() override {
        if (rootItem != nullptr) {
            if (auto comp = rootItem->getComponent()) {
                comp->setBounds(getLocalBounds());
            }
        }
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

    std::unique_ptr<::jive::GuiItem> rootItem;
    std::unique_ptr<::jive::Interpreter> interpreter;
    std::function<void()> onCancel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProgressContentWrapper)
};

} // namespace

// ============================================================================
// WavExportTask Implementation
// ============================================================================

WavExportTask::WavExportTask(devpiano::recording::RecordingTake take_, const juce::File& destinationFile_,
                             const devpiano::exporting::WavExportOptions& options_,
                             std::unique_ptr<juce::AudioPluginInstance> offlinePlugin_,
                             juce::Component* parentToCentreAround)
    : juce::Thread("WAV Export Thread")
    , take(std::move(take_))
    , destinationFile(destinationFile_)
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

bool WavExportTask::runThread() {
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

    // Build JIVE progress dialog layout
    auto layout = devpiano::ui::jive::JiveModalDialog::makeProgressLayout(TRANS("Exporting..."), 380, 140);
    devpiano::ui::jive::StyleCatalog::get().applyToTree(layout);

    auto interpreter = std::make_unique<::jive::Interpreter>();
    auto rootItem = interpreter->interpret(layout);
    jassert(rootItem != nullptr);

    if (rootItem != nullptr) {
        if (auto* cancelBtn = devpiano::ui::jive::JiveModalDialog::findButtonById(*rootItem, "dialog-cancel-btn")) {
            cancelBtn->onClick = [this] {
                cancelRequested.store(true);
                signalThreadShouldExit();
            };
        }
    }

    // Start background audio rendering thread
    startThread(juce::Thread::Priority::normal);

    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = TRANS("Export WAV");
    opts.dialogBackgroundColour = devpiano::jive::DesignTokens::get().mainBg();
    opts.componentToCentreAround = parentComponent;
    opts.resizable = false;
    opts.escapeKeyTriggersCloseButton = false; // Cancellation handled gracefully via cancelRequested flag

    auto contentWrapper = std::make_unique<ProgressContentWrapper>(std::move(rootItem), std::move(interpreter), [this] {
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
    if (activeDialog != nullptr) {
        activeDialog->exitModalState(0);
        activeDialog = nullptr;
    }

    stopThread(3000);
    return success.load() && !cancelRequested.load();
}

void WavExportTask::timerCallback() {
    const bool isRunning = isThreadRunning();

    if (!isRunning || finished.load() || activeDialog == nullptr) {
        stopTimer();
        return;
    }

    // Update status text and progress bar on message thread
    if (activeDialog != nullptr) {
        if (auto* wrapper = dynamic_cast<ProgressContentWrapper*>(activeDialog->getContentComponent())) {
            if (wrapper->rootItem != nullptr) {
                juce::String msg;
                {
                    const juce::ScopedLock sl(messageLock);
                    msg = currentStatusMessage;
                }
                if (auto* msgItem = devpiano::ui::jive::JiveModalDialog::findGuiItemById(*wrapper->rootItem,
                                                                                         "progress-status-message")) {
                    msgItem->state.setProperty("text", msg, nullptr);
                }

                if (auto* barItem
                    = devpiano::ui::jive::JiveModalDialog::findGuiItemById(*wrapper->rootItem, "dialog-progress-bar")) {
                    barItem->state.setProperty("value", currentProgress.load(), nullptr);
                }
            }
        }
    }
}

void WavExportTask::run() {
    using namespace devpiano::exporting;

    setProgress(0.0);
    setStatusMessage(TRANS("Exporting..."));

    auto progressCallback = [this](double p) -> bool {
        setProgress(p);
        const auto percent = static_cast<int>(p * 100.0);
        if (percent % 10 == 0 || p >= 1.0) {
            setStatusMessage(TRANS("Exporting...") + " " + juce::String(percent) + "%");
        }
        return !threadShouldExit() && !cancelRequested.load();
    };

    // ERR-015: Render path may throw; catch all exceptions, report failure and clean up destination file.
    try {
        if (threadShouldExit() || cancelRequested.load()) {
            success.store(false);
            {
                const juce::ScopedLock sl(messageLock);
                errorMessage = TRANS("Export cancelled.");
            }
            destinationFile.deleteFile();
            finished.store(true);
            return;
        }

        if (offlinePlugin != nullptr) {
            // Plugin offline-render path
            if (renderTakeWithOfflinePlugin(take, destinationFile, options, *offlinePlugin, progressCallback)) {
                success.store(true);
            } else {
                if (threadShouldExit() || cancelRequested.load()) {
                    const juce::ScopedLock sl(messageLock);
                    errorMessage = TRANS("Export cancelled.");
                    if (!destinationFile.deleteFile()) {
                        DP_LOG_WARN("Failed to clean up cancelled WAV: " + destinationFile.getFullPathName());
                    }
                } else {
                    const juce::ScopedLock sl(messageLock);
                    errorMessage = TRANS("Export failed during plugin rendering.");
                    if (!destinationFile.deleteFile()) {
                        DP_LOG_WARN("Failed to clean up failed WAV: " + destinationFile.getFullPathName());
                    }
                }
                success.store(false);
            }
        } else {
            // Built-in synth fallback path
            if (exportTakeAsWavFile(take, destinationFile, options, progressCallback)) {
                success.store(true);
            } else {
                if (threadShouldExit() || cancelRequested.load()) {
                    const juce::ScopedLock sl(messageLock);
                    errorMessage = TRANS("Export cancelled.");
                    if (!destinationFile.deleteFile()) {
                        DP_LOG_WARN("Failed to clean up cancelled WAV: " + destinationFile.getFullPathName());
                    }
                } else {
                    const juce::ScopedLock sl(messageLock);
                    errorMessage = TRANS("Export failed during built-in synth rendering.");
                    if (!destinationFile.deleteFile()) {
                        DP_LOG_WARN("Failed to clean up failed WAV: " + destinationFile.getFullPathName());
                    }
                }
                success.store(false);
            }
        }
    } catch (const std::exception& e) {
        success.store(false);
        {
            const juce::ScopedLock sl(messageLock);
            errorMessage = TRANS("Export failed unexpectedly.");
        }
        DP_LOG_ERROR("[Export] WAV export threw: " + juce::String(e.what()));
        if (!destinationFile.deleteFile()) {
            DP_LOG_WARN("Failed to clean up failed WAV: " + destinationFile.getFullPathName());
        }
    } catch (...) {
        success.store(false);
        {
            const juce::ScopedLock sl(messageLock);
            errorMessage = TRANS("Export failed unexpectedly.");
        }
        DP_LOG_ERROR("[Export] WAV export threw an unknown exception");
        if (!destinationFile.deleteFile()) {
            DP_LOG_WARN("Failed to clean up failed WAV: " + destinationFile.getFullPathName());
        }
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
