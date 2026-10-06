#include "PluginOperationController.h"

#include "Diagnostics/Log.h"
#include "MainComponent.h"
#include "Plugin/PluginFlowSupport.h"
#include "Plugin/PluginHost.h"
#include "UI/PluginEditorWindow.h"
#include "UI/WindowIconUtils.h"

namespace devpiano::plugin {

PluginOperationController::PluginOperationController(MainComponent& ownerIn, PluginHost& pluginHostIn,
                                                     SettingsModel& appSettingsIn)
    : owner(ownerIn)
    , pluginHost(pluginHostIn)
    , appSettings(appSettingsIn) {
    // Crash-safe scan persistence: every plugin discovered mid-scan is written
    // to the settings file straight away.  A third-party plugin that crashes
    // the scanner further down the list then costs at most the entries scanned
    // after the last persisted one, instead of discarding the entire scan.
    pluginHost.setScanIncrementalCallback([this](const PluginHost& host) {
        appSettings.knownPluginListState = host.createKnownPluginListXml();
        owner.persistSettingsModelSnapshot();
    });
}

PluginOperationController::~PluginOperationController() {
    // Clear the incremental callback first: cancelVst3ScanSession() below may
    // fire it (when knownPluginList already contains entries).  Without this
    // clear, the host would keep a std::function that captures a freed
    // `this`, and any later advanceStep/addVst3File/cancel call would
    // dereference stale state through owner/appSettings.
    pluginHost.setScanIncrementalCallback({});
    pluginHost.cancelVst3ScanSession();
}

void PluginOperationController::restorePluginStateOnStartup() {
    const auto plan = buildStartupPluginRestorePlan(appSettings.getPluginRecoverySettingsView(),
                                                    pluginHost.getDefaultVst3SearchPath());

    if (tryRestoreCachedPluginList(pluginHost, appSettings, plan)) {
        owner.refreshReadOnlyUiStateFromCurrentSnapshot();

        if (plan.shouldLoadLastPlugin) {
            restoreLastPluginOnStartup(plan);
        }

        return;
    }

    if (!plan.shouldScan) {
        return;
    }

    restorePluginScanPathOnStartup(plan);

    if (plan.shouldLoadLastPlugin) {
        restoreLastPluginOnStartup(plan);
    }
}

void PluginOperationController::loadSelectedPlugin() {
    if (pluginHost.isCurrentlyScanning()) {
        return;
    }

    const auto identifier = getSelectedPluginIdentifierForLoad();
    if (identifier.isEmpty()) {
        owner.finishPluginUiAction(false);
        return;
    }

    loadPluginByIdentifierAndCommitState(identifier);
}

void PluginOperationController::handleImportVst3File(const juce::File& vst3File) {
    if (pluginHost.isCurrentlyScanning()) {
        DP_LOG_WARN("[Plugin] dropped VST3 file ignored while scanning");
        return;
    }

    if (!vst3File.exists() || vst3File.getFileExtension().toLowerCase() != ".vst3") {
        DP_LOG_ERROR("[Plugin] invalid VST3 file: " + vst3File.getFullPathName());
        return;
    }

    juce::Array<juce::PluginDescription> descriptions;
    owner.runPluginActionWithAudioDeviceRebuild(
        [this, &vst3File, &descriptions] { descriptions = pluginHost.addVst3FileToKnownList(vst3File); });

    if (descriptions.isEmpty()) {
        DP_LOG_ERROR("[Plugin] no plugin types found in: " + vst3File.getFullPathName());
        return;
    }

    loadPluginByIdentifierAndCommitState(descriptions[0].createIdentifierString());
    DP_LOG_INFO("[Plugin] loaded from dropped file: " + vst3File.getFullPathName());
}

void PluginOperationController::unloadCurrentPlugin() {
    if (pluginHost.isCurrentlyScanning()) {
        return;
    }

    unloadPluginAndCommitState();
}

void PluginOperationController::togglePluginEditor() {
    if (pluginHost.isCurrentlyScanning()) {
        return;
    }
    if (pluginEditorWindow != nullptr) {
        closePluginEditorWindow();
        owner.finishPluginUiAction(false);
        return;
    }

    auto editor = tryCreatePluginEditor();
    if (editor == nullptr) {
        owner.finishPluginUiAction(false);
        return;
    }

    openPluginEditorWindow(std::move(editor));
}

void PluginOperationController::scanPlugins() {
    const auto path = resolvePluginScanPath();
    if (!isUsablePluginScanPath(path)) {
        pluginHost.markPluginScanSkipped("No usable VST3 scan directories. Check the path field.");
        owner.finishPluginUiAction(false);
        return;
    }

    if (pluginHost.isCurrentlyScanning()) {
        return;
    }

    pendingScanPath = path;
    pendingScanLastPluginIdentifier = appSettings.getPluginRecoverySettingsView().lastPluginIdentifier;

    bool beganScan = false;
    owner.runPluginActionWithAudioDeviceRebuild(
        [this, &path, &beganScan] { beganScan = pluginHost.beginVst3ScanSession(path, true); });
    if (!beganScan) {
        owner.finishPluginUiAction(false);
        return;
    }

    owner.setPluginPathText(path.toString());

    // Record the scan target before the first plugin is probed: if the scanner
    // crashes, the next launch still knows which directories were being scanned.
    appSettings.applyPluginRecoverySettingsView(
        makePluginRecoverySettings(path.toString(), pendingScanLastPluginIdentifier));
    owner.persistSettingsModelSnapshot();

    owner.refreshReadOnlyUiStateFromCurrentSnapshot();

    pendingScanLastPluginIdentifier = appSettings.getPluginRecoverySettingsView().lastPluginIdentifier;
    triggerAsyncUpdate();
}

bool PluginOperationController::hasEditorWindowOpen() const noexcept {
    return pluginEditorWindow != nullptr;
}

void PluginOperationController::restorePluginScanPathOnStartup(const StartupPluginRestorePlan& plan) {
    const auto path = juce::FileSearchPath(plan.recovery.pluginSearchPath);
    if (!isUsablePluginScanPath(path)) {
        return;
    }

    scanPluginsAtPathAndApplyRecoveryState(path, plan.recovery.lastPluginIdentifier);
}

void PluginOperationController::restoreLastPluginOnStartup(const StartupPluginRestorePlan& plan) {
    const auto& identifier = plan.recovery.lastPluginIdentifier;
    if (identifier.isEmpty()) {
        return;
    }

    restorePluginByIdentifierOnStartup(identifier);
}

void PluginOperationController::restorePluginByIdentifierOnStartup(const juce::String& identifier) {
    owner.runPluginActionWithAudioDeviceRebuild([this, identifier](const MainComponent::RuntimeAudioConfig& config) {
        if (!pluginHost.loadPluginByIdentifier(identifier, config.sampleRate, config.blockSize)) {
            DP_LOG_ERROR("[Plugin] startup restore failed: " + identifier + " - " + pluginHost.getLastLoadError());
        }
    });
}

juce::FileSearchPath PluginOperationController::resolvePluginScanPath() const {
    return normalisePluginScanPath(juce::FileSearchPath(owner.getPluginPathText().trim()),
                                   pluginHost.getDefaultVst3SearchPath());
}

juce::String PluginOperationController::getSelectedPluginIdentifierForLoad() const {
    return owner.getSelectedPluginIdentifier();
}

void PluginOperationController::loadPluginByIdentifierAndCommitState(const juce::String& identifier) {
    bool loaded = false;
    juce::String loadError;
    owner.runPluginActionWithAudioDeviceRebuild(
        [this, identifier, &loaded, &loadError](const MainComponent::RuntimeAudioConfig& config) {
            loaded = pluginHost.loadPluginByIdentifier(identifier, config.sampleRate, config.blockSize);
            loadError = pluginHost.getLastLoadError();
        });

    if (loaded) {
        commitPluginRecoveryStateAndFinishUi(
            makePluginRecoverySettings(appSettings.pluginSearchPath, pluginHost.getCurrentPluginIdentifier()), true);
        return;
    }

    DP_LOG_ERROR("[Plugin] failed to load: " + identifier + " - " + loadError);
    owner.finishPluginUiAction(false);
}

void PluginOperationController::unloadPluginAndCommitState() {
    owner.runPluginActionWithAudioDeviceRebuild([this] { pluginHost.unloadPlugin(); });

    commitPluginRecoveryStateAndFinishUi(appSettings.getPluginRecoverySettingsView(), true);
}

std::unique_ptr<juce::AudioProcessorEditor> PluginOperationController::tryCreatePluginEditor() const {
    auto* instance = pluginHost.getInstance();
    if (instance == nullptr || !instance->hasEditor()) {
        return nullptr;
    }

    return std::unique_ptr<juce::AudioProcessorEditor>(instance->createEditorAndMakeActive());
}

void PluginOperationController::handlePluginEditorWindowClosedAsync() {
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<MainComponent>(&owner)] {
        if (safe == nullptr) {
            return;
        }

        safe->pluginOperationController->closePluginEditorWindow();
        safe->finishPluginUiAction(false);
    });
}

void PluginOperationController::closePluginEditorWindow() {
    pluginEditorWindow.reset();
}

void PluginOperationController::openPluginEditorWindow(std::unique_ptr<juce::AudioProcessorEditor> editor) {
    auto closeEditorWindow = [safe = juce::Component::SafePointer<MainComponent>(&owner)] {
        if (safe != nullptr) {
            safe->pluginOperationController->handlePluginEditorWindowClosedAsync();
        }
    };

    pluginEditorWindow
        = std::make_unique<PluginEditorWindow>(pluginHost.getCurrentPluginName(), std::move(editor), closeEditorWindow);
    pluginEditorWindow->centreAroundComponent(&owner, pluginEditorWindow->getContentComponent()->getWidth(),
                                              pluginEditorWindow->getContentComponent()->getHeight());
    pluginEditorWindow->setVisible(true);
    devpiano::ui::applyAppWindowIcon(*pluginEditorWindow);
    owner.refreshReadOnlyUiStateFromCurrentSnapshot();
}

void PluginOperationController::scanPluginsAtPathAndApplyRecoveryState(const juce::FileSearchPath& path,
                                                                       const juce::String& lastPluginIdentifier) {
    owner.runPluginActionWithAudioDeviceRebuild([this, &path, lastPluginIdentifier] {
        scanPluginsAtPathAndUpdateRecovery(pluginHost, appSettings, path, lastPluginIdentifier);
    });
}

void PluginOperationController::scanPluginsAtPathAndCommitState(const juce::FileSearchPath& path) {
    const auto lastPluginIdentifier = appSettings.getPluginRecoverySettingsView().lastPluginIdentifier;
    scanPluginsAtPathAndApplyRecoveryState(path, lastPluginIdentifier);
    owner.setPluginPathText(path.toString());
    owner.finishPluginUiAction(true);
}

void PluginOperationController::handleAsyncUpdate() {
    if (!pluginHost.isCurrentlyScanning()) {
        finishScanSessionAndCommitState();
        return;
    }

    if (scanStepInProgress) {
        return;
    }

    scanStepInProgress = true;

    const bool hasMore = pluginHost.advanceVst3ScanStep();

    scanStepInProgress = false;

    owner.refreshReadOnlyUiStateFromCurrentSnapshot();

    if (!hasMore) {
        finishScanSessionAndCommitState();
        return;
    }

    triggerAsyncUpdate();
}

void PluginOperationController::finishScanSessionAndCommitState() {
    const auto recovery = makePluginRecoverySettings(pendingScanPath.toString(), pendingScanLastPluginIdentifier);

    appSettings.applyPluginRecoverySettingsView(recovery);
    appSettings.knownPluginListState = pluginHost.createKnownPluginListXml();

    owner.finishPluginUiAction(true);

    // Auto-load the first available plugin when user-initiated scan completes
    // and no plugin is currently loaded.
    if (!pluginHost.hasLoadedPlugin() && !pluginHost.getKnownPluginDescriptions().isEmpty()) {
        loadSelectedPlugin();
    }
}

void PluginOperationController::commitPluginRecoveryStateAndFinishUi(
    const SettingsModel::PluginRecoverySettingsView& pluginRecovery, bool shouldSaveSettings) {
    appSettings.applyPluginRecoverySettingsView(pluginRecovery);
    owner.finishPluginUiAction(shouldSaveSettings);
}

} // namespace devpiano::plugin
