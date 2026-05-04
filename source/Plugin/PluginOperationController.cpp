#include "PluginOperationController.h"

#include "MainComponent.h"
#include "Plugin/PluginFlowSupport.h"
#include "Plugin/PluginHost.h"
#include "UI/PluginEditorWindow.h"
#include "UI/PluginPanel.h"

namespace devpiano::plugin
{

PluginOperationController::PluginOperationController(MainComponent& ownerIn,
                                                     PluginHost& pluginHostIn,
                                                     SettingsModel& appSettingsIn,
                                                     PluginPanel& pluginPanelIn)
    : owner(ownerIn)
    , pluginHost(pluginHostIn)
    , appSettings(appSettingsIn)
    , pluginPanel(pluginPanelIn)
{
}

PluginOperationController::~PluginOperationController()
{
    pluginHost.cancelVst3ScanSession();
}

void PluginOperationController::restorePluginStateOnStartup()
{
    const auto plan = buildStartupPluginRestorePlan(appSettings.getPluginRecoverySettingsView(),
                                                    pluginHost.getDefaultVst3SearchPath());

    if (tryRestoreCachedPluginList(pluginHost, appSettings, plan))
    {
        owner.refreshReadOnlyUiStateFromCurrentSnapshot();

        if (plan.shouldLoadLastPlugin)
            restoreLastPluginOnStartup(plan);

        return;
    }

    if (! plan.shouldScan)
        return;

    restorePluginScanPathOnStartup(plan);

    if (plan.shouldLoadLastPlugin)
        restoreLastPluginOnStartup(plan);
}

void PluginOperationController::loadSelectedPlugin()
{
    if (pluginHost.isCurrentlyScanning())
        return;

    const auto pluginName = getSelectedPluginNameForLoad();
    if (pluginName.isEmpty())
    {
        owner.finishPluginUiAction(false);
        return;
    }

    loadPluginByNameAndCommitState(pluginName);
}

void PluginOperationController::unloadCurrentPlugin()
{
    if (pluginHost.isCurrentlyScanning())
        return;

    unloadPluginAndCommitState();
}

void PluginOperationController::togglePluginEditor()
{
    if (pluginHost.isCurrentlyScanning())
        return;
    if (pluginEditorWindow != nullptr)
    {
        closePluginEditorWindow();
        owner.finishPluginUiAction(false);
        return;
    }

    auto editor = tryCreatePluginEditor();
    if (editor == nullptr)
    {
        owner.finishPluginUiAction(false);
        return;
    }

    openPluginEditorWindow(std::move(editor));
}

void PluginOperationController::scanPlugins()
{
    const auto path = resolvePluginScanPath();
    if (! isUsablePluginScanPath(path))
    {
        pluginHost.markPluginScanSkipped("No usable VST3 scan directories. Check the path field.");
        owner.finishPluginUiAction(false);
        return;
    }

    if (pluginHost.isCurrentlyScanning())
        return;

    pendingScanPath = path;
    pendingScanLastPluginName = appSettings.getPluginRecoverySettingsView().lastPluginName;

    if (! pluginHost.beginVst3ScanSession(path, true))
    {
        owner.finishPluginUiAction(false);
        return;
    }

    pluginPanel.setPluginPathText(path.toString());

    owner.refreshReadOnlyUiStateFromCurrentSnapshot();

    pendingScanLastPluginName = appSettings.getPluginRecoverySettingsView().lastPluginName;
    triggerAsyncUpdate();
}

bool PluginOperationController::hasEditorWindowOpen() const noexcept
{
    return pluginEditorWindow != nullptr;
}

void PluginOperationController::restorePluginScanPathOnStartup(const StartupPluginRestorePlan& plan)
{
    const auto path = juce::FileSearchPath(plan.recovery.pluginSearchPath);
    if (! isUsablePluginScanPath(path))
        return;

    scanPluginsAtPathAndUpdateRecovery(pluginHost,
                                       appSettings,
                                       path,
                                       plan.recovery.lastPluginName);
}

void PluginOperationController::restoreLastPluginOnStartup(const StartupPluginRestorePlan& plan)
{
    const auto& pluginName = plan.recovery.lastPluginName;
    if (pluginName.isEmpty())
        return;

    restorePluginByNameOnStartup(pluginName);
}

void PluginOperationController::restorePluginByNameOnStartup(const juce::String& pluginName)
{
    owner.runPluginActionWithAudioDeviceRebuild([this, pluginName](const MainComponent::RuntimeAudioConfig& config)
    {
        const auto loaded = pluginHost.loadPluginByName(pluginName,
                                                        config.sampleRate,
                                                        config.blockSize);
        juce::ignoreUnused(loaded);
    });
}

juce::FileSearchPath PluginOperationController::resolvePluginScanPath() const
{
    return normalisePluginScanPath(juce::FileSearchPath(pluginPanel.getPluginPathText().trim()),
                                   pluginHost.getDefaultVst3SearchPath());
}

juce::String PluginOperationController::getSelectedPluginNameForLoad() const
{
    return pluginPanel.getSelectedPluginName().trim();
}

void PluginOperationController::loadPluginByNameAndCommitState(const juce::String& pluginName)
{
    owner.runPluginActionWithAudioDeviceRebuild([this, pluginName](const MainComponent::RuntimeAudioConfig& config)
    {
        const auto success = pluginHost.loadPluginByName(pluginName,
                                                         config.sampleRate,
                                                         config.blockSize);
        juce::ignoreUnused(success);
    });

    commitPluginRecoveryStateAndFinishUi(makePluginRecoverySettings(appSettings.pluginSearchPath, pluginName),
                                         true);
}

void PluginOperationController::unloadPluginAndCommitState()
{
    owner.runPluginActionWithAudioDeviceRebuild([this]
    {
        pluginHost.unloadPlugin();
    });

    commitPluginRecoveryStateAndFinishUi(appSettings.getPluginRecoverySettingsView(), true);
}

std::unique_ptr<juce::AudioProcessorEditor> PluginOperationController::tryCreatePluginEditor() const
{
    auto* instance = pluginHost.getInstance();
    if (instance == nullptr || ! instance->hasEditor())
        return nullptr;

    return std::unique_ptr<juce::AudioProcessorEditor>(instance->createEditorAndMakeActive());
}

void PluginOperationController::handlePluginEditorWindowClosedAsync()
{
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<MainComponent>(&owner)]
    {
        if (safe == nullptr)
            return;

        safe->pluginOperationController->closePluginEditorWindow();
        safe->finishPluginUiAction(false);
    });
}

void PluginOperationController::closePluginEditorWindow()
{
    pluginEditorWindow.reset();
}

void PluginOperationController::openPluginEditorWindow(std::unique_ptr<juce::AudioProcessorEditor> editor)
{
    auto closeEditorWindow = [safe = juce::Component::SafePointer<MainComponent>(&owner)]
    {
        if (safe != nullptr)
            safe->pluginOperationController->handlePluginEditorWindowClosedAsync();
    };

    pluginEditorWindow = std::make_unique<PluginEditorWindow>(pluginHost.getCurrentPluginName(),
                                                                std::move(editor),
                                                                closeEditorWindow);
    pluginEditorWindow->centreAroundComponent(&owner,
                                               pluginEditorWindow->getContentComponent()->getWidth(),
                                               pluginEditorWindow->getContentComponent()->getHeight());
    pluginEditorWindow->setVisible(true);
    owner.refreshReadOnlyUiStateFromCurrentSnapshot();
}

void PluginOperationController::scanPluginsAtPathAndApplyRecoveryState(const juce::FileSearchPath& path,
                                                                       const juce::String& lastPluginName)
{
    owner.runPluginActionWithAudioDeviceRebuild([this, &path, lastPluginName]
    {
        scanPluginsAtPathAndUpdateRecovery(pluginHost,
                                           appSettings,
                                           path,
                                           lastPluginName);
    });
}

void PluginOperationController::scanPluginsAtPathAndCommitState(const juce::FileSearchPath& path)
{
    const auto lastPluginName = appSettings.getPluginRecoverySettingsView().lastPluginName;
    scanPluginsAtPathAndApplyRecoveryState(path, lastPluginName);
    pluginPanel.setPluginPathText(path.toString());
    owner.finishPluginUiAction(true);
}

void PluginOperationController::handleAsyncUpdate()
{
    if (! pluginHost.isCurrentlyScanning())
    {
        finishScanSessionAndCommitState();
        return;
    }

    if (scanStepInProgress)
        return;

    scanStepInProgress = true;

    const bool hasMore = pluginHost.advanceVst3ScanStep();

    scanStepInProgress = false;

    owner.refreshReadOnlyUiStateFromCurrentSnapshot();

    if (! hasMore)
    {
        finishScanSessionAndCommitState();
        return;
    }

    triggerAsyncUpdate();
}

void PluginOperationController::finishScanSessionAndCommitState()
{
    const auto recovery = makePluginRecoverySettings(pendingScanPath.toString(),
                                                    pendingScanLastPluginName);

    appSettings.applyPluginRecoverySettingsView(recovery);
    appSettings.knownPluginListState = pluginHost.createKnownPluginListXml();

    owner.finishPluginUiAction(true);
}

void PluginOperationController::commitPluginRecoveryStateAndFinishUi(const SettingsModel::PluginRecoverySettingsView& pluginRecovery,
                                                                      bool shouldSaveSettings)
{
    appSettings.applyPluginRecoverySettingsView(pluginRecovery);
    owner.finishPluginUiAction(shouldSaveSettings);
}

} // namespace devpiano::plugin
