#include "MainComponent.h"
#include "UI/HeaderPanelStateBuilder.h"
#include "UI/PluginPanelStateBuilder.h"

#if JUCE_WINDOWS
struct HWND__;
using HWND = HWND__*;
struct HIMC__;
using HIMC = HIMC__*;
extern "C" HIMC __stdcall ImmAssociateContext(HWND, HIMC);
#endif

namespace
{
const auto backgroundColour = juce::Colour(0xff202225);
constexpr int preferredMainContentWidth = 1120;
constexpr int preferredMainContentHeight = 760;
constexpr int minimumMainContentWidth = 980;
constexpr int minimumMainContentHeight = 700;
constexpr int maximumMainContentWidth = 3840;
constexpr int maximumMainContentHeight = 2160;

juce::String makeSafeUiText(juce::String text)
{
    text = text.replaceCharacters("\r\n\t", "   ");

    constexpr auto maxLen = 1024;
    if (text.length() > maxLen)
        text = text.substring(0, maxLen);

    return text;
}

#if JUCE_WINDOWS
void suppressImeForPeer(juce::ComponentPeer* peer)
{
    if (peer == nullptr)
        return;

    if (auto hwnd = static_cast<HWND>(peer->getNativeHandle()))
        ImmAssociateContext(hwnd, nullptr);
}
#endif
}

MainComponent::MainComponent()
    : keyboardPanel(audioEngine.getKeyboardState())
{
    settingsStore.load(appSettings);
    initialiseInputMappingFromSettings();
    audioEngine.setPluginHost(&pluginHost);
    audioEngine.setRecordingEngine(&recordingEngine);

    layoutFlowSupport = std::make_unique<devpiano::layout::LayoutFlowSupport>(*this);
    recordingSessionController = std::make_unique<devpiano::recording::RecordingSessionController>(
        *this, recordingEngine, audioEngine, appSettings, controlsPanel);

    initialiseUi();
    syncUiFromSettings();
    applyUiStateToAudioEngine();

    initialiseAudioDevice();
    suppressTextInputMethods();

    initialiseMidiRouting();
    restorePluginStateOnStartup();
    refreshReadOnlyUiStateFromCurrentSnapshot();

    startTimerHz(30);
    restoreKeyboardFocus();
}

MainComponent::~MainComponent()
{
    stopTimer();

    controlsPanel.onValuesChanged = {};
    controlsPanel.onLayoutChanged = {};
    controlsPanel.onSaveLayoutRequested = {};
    controlsPanel.onResetLayoutRequested = {};
    controlsPanel.onImportLayoutRequested = {};
    controlsPanel.onRenameLayoutRequested = {};
    controlsPanel.onDeleteLayoutRequested = {};
    controlsPanel.onRecordClicked = {};
    controlsPanel.onPlayClicked = {};
    controlsPanel.onStopClicked = {};
    controlsPanel.onBackToStartClicked = {};
    controlsPanel.onExportMidiClicked = {};
    controlsPanel.onExportWavClicked = {};
    controlsPanel.onImportMidiClicked = {};
    headerPanel.onSettingsRequested = {};
    pluginPanel.onScanRequested = {};
    pluginPanel.onLoadRequested = {};
    pluginPanel.onUnloadRequested = {};
    pluginPanel.onToggleEditorRequested = {};

    saveSettingsNow();

    midiRouter.setMessageCallback({});
    midiRouter.setCollector(nullptr);
    midiRouter.closeInputs();

    closePluginEditorWindow();
    shutdownAudio();
    audioEngine.setRecordingEngine(nullptr);
    pluginHost.unloadPlugin();
    settingsWindow.reset();
}

void MainComponent::initialiseInputMappingFromSettings()
{
    const auto inputMapping = appSettings.getInputMappingSettingsView();
    keyboardMidiMapper.setLayout(SettingsModel::keyMapToLayout(inputMapping.keyMap, inputMapping.layoutId));
    appSettings.applyInputMappingSettingsView({ .layoutId = keyboardMidiMapper.getLayout().id,
                                                .keyMap = SettingsModel::layoutToKeyMap(keyboardMidiMapper.getLayout()) });
}

void MainComponent::initialiseUi()
{
    setBounds(getInitialMainContentBounds());
    setWantsKeyboardFocus(true);

    addAndMakeVisible(headerPanel);
    headerPanel.setHintText("VST3 scan/load is ready: scan, select a plugin, then click Load.");
    headerPanel.onSettingsRequested = [this] { showSettingsDialog(); };

    addAndMakeVisible(pluginPanel);
    pluginPanel.onScanRequested = [this] { scanPlugins(); };
    pluginPanel.onLoadRequested = [this] { loadSelectedPlugin(); };
    pluginPanel.onUnloadRequested = [this] { unloadCurrentPlugin(); };
    pluginPanel.onToggleEditorRequested = [this] { togglePluginEditor(); };

    const auto pluginRecovery = getPluginRecoverySettingsWithFallback();
    pluginPanel.setPluginPathText(makeSafeUiText(pluginRecovery.pluginSearchPath));

    addAndMakeVisible(controlsPanel);
    controlsPanel.onValuesChanged = [this] { handlePerformanceUiChanged(); };
    controlsPanel.onLayoutChanged = [this](const juce::String& newId) { layoutFlowSupport->handleLayoutChanged(newId); };
    controlsPanel.onSaveLayoutRequested = [this] { layoutFlowSupport->handleSaveLayoutRequested(); };
    controlsPanel.onResetLayoutRequested = [this] { layoutFlowSupport->handleResetLayoutToDefaultRequested(); };
    controlsPanel.onImportLayoutRequested = [this] { layoutFlowSupport->handleImportLayoutRequested(); };
    controlsPanel.onRenameLayoutRequested = [this] { layoutFlowSupport->handleRenameLayoutRequested(); };
    controlsPanel.onDeleteLayoutRequested = [this] { layoutFlowSupport->handleDeleteLayoutRequested(); };
    controlsPanel.onRecordClicked = [this] { recordingSessionController->handleRecordClicked(); };
    controlsPanel.onPlayClicked = [this] { recordingSessionController->handlePlayClicked(); };
    controlsPanel.onStopClicked = [this] { recordingSessionController->handleStopClicked(); };
    controlsPanel.onBackToStartClicked = [this] { recordingSessionController->handleBackToStartClicked(); };
    controlsPanel.onExportMidiClicked = [this] { recordingSessionController->handleExportMidiClicked(); };
    controlsPanel.onExportWavClicked = [this] { recordingSessionController->handleExportWavClicked(); };
    controlsPanel.onImportMidiClicked = [this] { recordingSessionController->handleImportMidiClicked(); };

    addAndMakeVisible(keyboardPanel);
}

juce::Rectangle<int> MainComponent::getMainContentResizeLimits()
{
    return { minimumMainContentWidth,
             minimumMainContentHeight,
             maximumMainContentWidth,
             maximumMainContentHeight };
}

juce::Rectangle<int> MainComponent::getInitialMainContentBounds() const
{
    const auto limits = getMainContentResizeLimits();
    const auto savedWidth = appSettings.mainWindowWidth;
    const auto savedHeight = appSettings.mainWindowHeight;

    const auto width = juce::jlimit(limits.getX(),
                                   limits.getWidth(),
                                   savedWidth > 0 ? savedWidth : preferredMainContentWidth);
    const auto height = juce::jlimit(limits.getY(),
                                    limits.getHeight(),
                                    savedHeight > 0 ? savedHeight : preferredMainContentHeight);

    return { 0, 0, width, height };
}

void MainComponent::persistMainContentSize(int width, int height)
{
    const auto limits = getMainContentResizeLimits();
    const auto clampedWidth = juce::jlimit(limits.getX(), limits.getWidth(), width);
    const auto clampedHeight = juce::jlimit(limits.getY(), limits.getHeight(), height);

    if (appSettings.mainWindowWidth == clampedWidth && appSettings.mainWindowHeight == clampedHeight)
        return;

    appSettings.mainWindowWidth = clampedWidth;
    appSettings.mainWindowHeight = clampedHeight;
    saveSettingsSoon();
}

void MainComponent::initialiseMidiRouting()
{
    midiRouter.setCollector(&audioEngine.getMidiCollector());
    midiRouter.setMessageCallback([safe = juce::Component::SafePointer<MainComponent>(this)](const juce::MidiMessage& message)
    {
        juce::MessageManager::callAsync([safe, message]
        {
            if (safe == nullptr)
                return;

            ++safe->externalMidiMessageCount;

            if (message.isNoteOn())
                safe->lastExternalMidiMessage = "Note On " + juce::String(message.getNoteNumber());
            else if (message.isNoteOff())
                safe->lastExternalMidiMessage = "Note Off " + juce::String(message.getNoteNumber());
            else if (message.isController())
                safe->lastExternalMidiMessage = "CC " + juce::String(message.getControllerNumber());
            else if (message.isProgramChange())
                safe->lastExternalMidiMessage = "Program " + juce::String(message.getProgramChangeNumber());
            else if (message.isPitchWheel())
                safe->lastExternalMidiMessage = "Pitch Wheel";
            else
                safe->lastExternalMidiMessage = message.getDescription();

            safe->refreshMidiStatusFromCurrentSnapshot();
        });
    });
    midiRouter.openAllInputs();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    audioEngine.prepareToPlay(samplesPerBlockExpected, sampleRate);
    appSettings.applyAudioSettingsView({ .sampleRate = sampleRate,
                                         .bufferSize = samplesPerBlockExpected });
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    audioEngine.getNextAudioBlock(bufferToFill);
}

void MainComponent::releaseResources()
{
    audioEngine.releaseResources();
}

void MainComponent::timerCallback()
{
    recordingSessionController->checkPlaybackEnded();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(backgroundColour);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(16);

    headerPanel.setBounds(area.removeFromTop(98));
    area.removeFromTop(10);

    pluginPanel.setBounds(area.removeFromTop(188));
    area.removeFromTop(12);

    controlsPanel.setBounds(area.removeFromTop(260));
    area.removeFromTop(8);

    keyboardPanel.setBounds(area.removeFromBottom(128));
}

void MainComponent::visibilityChanged()
{
    if (isShowing())
    {
        suppressTextInputMethods();
        restoreKeyboardFocus();
    }
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    const auto handled = keyboardMidiMapper.handleKeyPressed(key, audioEngine.getKeyboardState());

    if (handled)
        suppressTextInputMethods();

    return handled;
}

bool MainComponent::keyStateChanged(bool isKeyDown)
{
    juce::ignoreUnused(isKeyDown);
    const auto handled = keyboardMidiMapper.handleKeyStateChanged(audioEngine.getKeyboardState());

    if (handled)
        suppressTextInputMethods();

    return handled;
}

void MainComponent::focusGained(juce::Component::FocusChangeType cause)
{
    juce::AudioAppComponent::focusGained(cause);

    if (isSettingsWindowOpen())
        return;

    // Windows may have already given us focus via WM_SETFOCUS before grabKeyboardFocus ran,
    // causing takeKeyboardFocus's early-return check to fire. Call grabKeyboardFocus() to
    // synchronize the global state. The early-return in takeKeyboardFocus will safely fire
    // (because currentlyFocusedComponent will already be set after the first call).
    if (juce::Component::getCurrentlyFocusedComponent() != this)
        grabKeyboardFocus();
}

void MainComponent::focusLost(juce::Component::FocusChangeType cause)
{
    juce::AudioAppComponent::focusLost(cause);
}

SettingsModel::PerformanceSettingsView MainComponent::getPerformanceSettingsFromUi() const
{
    return { .masterGain = controlsPanel.getMasterGain(),
             .adsrAttack = controlsPanel.getAttack(),
             .adsrDecay = controlsPanel.getDecay(),
             .adsrSustain = controlsPanel.getSustain(),
             .adsrRelease = controlsPanel.getRelease() };
}

juce::String MainComponent::getLastPluginNameForRecoveryStateFromUi() const
{
    return pluginHost.hasLoadedPlugin() ? pluginHost.getCurrentPluginName()
                                        : pluginPanel.getSelectedPluginName().trim();
}

juce::String MainComponent::getPersistedPluginSearchPath() const
{
    return appSettings.getPluginRecoverySettingsView().pluginSearchPath;
}

SettingsModel::PluginRecoverySettingsView MainComponent::getPluginRecoverySettingsFromUi() const
{
    return devpiano::plugin::makePluginRecoverySettings(pluginPanel.getPluginPathText().trim(),
                                                        getLastPluginNameForRecoveryStateFromUi());
}

SettingsModel::PluginRecoverySettingsView MainComponent::getPluginRecoverySettingsWithFallback() const
{
    return devpiano::plugin::withPluginRecoveryPathFallback(appSettings.getPluginRecoverySettingsView(),
                                                            pluginHost.getDefaultVst3SearchPath());
}

void MainComponent::applyPerformanceSettingsToUi(const SettingsModel::PerformanceSettingsView& performance)
{
    controlsPanel.setValues(performance.masterGain,
                            performance.adsrAttack,
                            performance.adsrDecay,
                            performance.adsrSustain,
                            performance.adsrRelease);
}

void MainComponent::applyPerformanceSettingsToAudioEngine(const SettingsModel::PerformanceSettingsView& performance)
{
    audioEngine.setMasterGain(performance.masterGain);
    audioEngine.setAdsr(performance.adsrAttack,
                        performance.adsrDecay,
                        performance.adsrSustain,
                        performance.adsrRelease);
}

void MainComponent::applyPluginRecoverySettings(const SettingsModel::PluginRecoverySettingsView& pluginRecovery)
{
    appSettings.applyPluginRecoverySettingsView(pluginRecovery);
}

void MainComponent::commitPluginRecoveryStateAndFinishUi(const SettingsModel::PluginRecoverySettingsView& pluginRecovery,
                                                         bool shouldSaveSettings)
{
    applyPluginRecoverySettings(pluginRecovery);
    finishPluginUiAction(shouldSaveSettings);
}

void MainComponent::handlePerformanceUiChanged()
{
    applyPerformanceSettingsToAudioEngine(getPerformanceSettingsFromUi());
    saveSettingsSoon();
}

void MainComponent::applyUiStateToAudioEngine()
{
    applyPerformanceSettingsToAudioEngine(getPerformanceSettingsFromUi());
}

void MainComponent::syncUiFromSettings()
{
    applyPerformanceSettingsToUi(appSettings.getPerformanceSettingsView());

    juce::StringArray layoutIds = { "default.freepiano.minimal", "default.freepiano.full" };
    juce::StringArray layoutDisplayNames = { "FreePiano Minimal", "FreePiano Full" };

    auto userLayouts = devpiano::layout::scanUserLayoutDirectory();
    for (const auto& layout : userLayouts)
    {
        layoutIds.add(layout.id);
        layoutDisplayNames.add(layout.name.isNotEmpty() ? layout.name : layout.id);
    }

    controlsPanel.setLayouts(layoutIds, appSettings.getInputMappingSettingsView().layoutId, layoutDisplayNames);
}

void MainComponent::syncSettingsFromUi()
{
    appSettings.applyPerformanceSettingsView(getPerformanceSettingsFromUi());

    applyPluginRecoverySettings(getPluginRecoverySettingsFromUi());

    appSettings.applyInputMappingSettingsView({ .layoutId = keyboardMidiMapper.getLayout().id,
                                                .keyMap = SettingsModel::layoutToKeyMap(keyboardMidiMapper.getLayout()) });
}

void MainComponent::suppressTextInputMethods()
{
    if (auto* peer = getPeer())
    {
        peer->refreshTextInputTarget();

       #if JUCE_WINDOWS
        suppressImeForPeer(peer);
       #endif
    }
}

void MainComponent::restoreKeyboardFocus()
{
    if (isSettingsWindowOpen())
        return;

    if (isShowing() && juce::Component::getCurrentlyFocusedComponent() != this)
        grabKeyboardFocus();

    suppressTextInputMethods();
}

void MainComponent::initialiseAudioDevice()
{
    setAudioChannels(0, 2);

    const auto audioSettings = appSettings.getAudioSettingsView();
    if (audioSettings.hasSerializedDeviceState && appSettings.audioDeviceState != nullptr)
    {
        const auto result = deviceManager.initialise(0, 2, appSettings.audioDeviceState.get(), true);
        if (result.isNotEmpty())
            juce::Logger::writeToLog("[AudioDevice] initialise restore result: " + result);
    }

    captureAudioDeviceState();
    logCurrentAudioDeviceDiagnostics("initialiseAudioDevice");
}

void MainComponent::captureAudioDeviceState()
{
    if (auto xml = deviceManager.createStateXml())
        appSettings.setSerializedAudioDeviceState(std::move(xml));
}

void MainComponent::prepareForAudioDeviceRebuild()
{
    captureAudioDeviceState();
    closePluginEditorWindow();
    shutdownAudio();
}

void MainComponent::finishAudioDeviceRebuild()
{
    initialiseAudioDevice();
    restoreKeyboardFocus();
}

void MainComponent::collectCurrentSettingsState()
{
    syncSettingsFromUi();
    captureAudioDeviceState();
}

void MainComponent::saveSettingsNow()
{
    collectCurrentSettingsState();
    settingsStore.save(appSettings);
    logCurrentAudioDeviceDiagnostics("saveSettingsNow");
}

void MainComponent::saveSettingsSoon()
{
    collectCurrentSettingsState();
    settingsStore.scheduleSave(appSettings);
}

void MainComponent::showSettingsDialog()
{
    class SettingsDialogWindow final : public juce::DialogWindow
    {
    public:
        SettingsDialogWindow(const juce::String& title,
                             juce::Colour background,
                             std::function<void()> onClose)
            : juce::DialogWindow(title, background, true), closeCallback(std::move(onClose))
        {
        }

        void closeButtonPressed() override
        {
            if (closeCallback)
                closeCallback();
        }

        bool escapeKeyPressed() override
        {
            closeButtonPressed();
            return true;
        }

    private:
        std::function<void()> closeCallback;
    };

    auto content = std::make_unique<SettingsComponent>(deviceManager, appSettings.audioDeviceState.get());
    auto* contentPtr = content.get();

    auto closeWindow = [safe = juce::Component::SafePointer<MainComponent>(this)]
    {
        if (safe != nullptr)
            safe->closeSettingsWindow();
    };

    contentPtr->onSaveRequested = [safe = juce::Component::SafePointer<MainComponent>(this)]
    {
        if (safe != nullptr)
            safe->saveAndCloseSettingsWindow();
    };

    settingsWindow = std::make_unique<SettingsDialogWindow>("Audio Settings", backgroundColour, closeWindow);
    settingsWindow->setUsingNativeTitleBar(true);
    settingsWindow->setContentOwned(content.release(), true);
    settingsWindow->centreAroundComponent(this, 620, 560);
    settingsWindow->setResizable(true, true);
    settingsWindow->setVisible(true);
    settingsWindow->toFront(true);
}

bool MainComponent::isSettingsWindowDirty() const
{
    if (auto* settingsContent = getSettingsContent())
        return settingsContent->isDirty();

    return false;
}

bool MainComponent::isSettingsWindowOpen() const
{
    return settingsWindow != nullptr && settingsWindow->isShowing();
}

void MainComponent::closeSettingsWindowAsync()
{
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<MainComponent>(this)]
    {
        if (safe == nullptr)
            return;

        if (safe->settingsWindow != nullptr)
            safe->settingsWindow->setVisible(false);

        safe->settingsWindow.reset();
        safe->restoreKeyboardFocus();
    });
}

void MainComponent::closeSettingsWindow()
{
    if (isSettingsWindowDirty())
        saveSettingsNow();

    closeSettingsWindowAsync();
}

void MainComponent::saveAndCloseSettingsWindow()
{
    saveSettingsNow();
    closeSettingsWindowAsync();
}

SettingsComponent* MainComponent::getSettingsContent() const
{
    if (settingsWindow == nullptr)
        return nullptr;

    return dynamic_cast<SettingsComponent*>(settingsWindow->getContentComponent());
}

void MainComponent::renderReadOnlyUiState(const devpiano::core::AppState& appState)
{
    headerPanel.updateMidiStatus(buildHeaderPanelMidiStatus(appState));
    pluginPanel.updateState(buildPluginPanelState(pluginHost,
                                                 appState.plugin.lastPluginName,
                                                 appState.plugin.isEditorOpen));
}

void MainComponent::refreshReadOnlyUiStateFromCurrentSnapshot()
{
    renderReadOnlyUiState(buildCurrentAppStateSnapshot());
}

void MainComponent::refreshMidiStatusFromCurrentSnapshot()
{
    headerPanel.updateMidiStatus(buildHeaderPanelMidiStatus(buildCurrentAppStateSnapshot()));
}

void MainComponent::finishPluginUiAction(bool shouldSaveSettings)
{
    if (shouldSaveSettings)
        saveSettingsSoon();

    refreshReadOnlyUiStateFromCurrentSnapshot();
    restoreKeyboardFocus();
}

void MainComponent::logCurrentAudioDeviceDiagnostics(const juce::String& context) const
{
    const auto diagnostics = devpiano::audio::buildAudioDeviceDiagnostics(appSettings.audioDeviceState.get(), deviceManager);
    juce::Logger::writeToLog("[AudioDevice] " + context + "\n" + diagnostics.detailedSummary);
}

devpiano::core::RuntimeAudioState MainComponent::buildRuntimeAudioStateSnapshot() const
{
    const auto diagnostics = devpiano::audio::buildAudioDeviceDiagnostics(appSettings.audioDeviceState.get(), deviceManager);

    return { .hasLiveDevice = diagnostics.live.hasLiveDevice,
             .sampleRate = diagnostics.live.sampleRate,
             .bufferSize = diagnostics.live.bufferSize,
             .backendName = diagnostics.live.backendName,
             .deviceName = diagnostics.live.deviceName,
             .availableBufferSizesText = devpiano::audio::formatBufferSizes(diagnostics.live.availableBufferSizes),
             .restoreOutcome = diagnostics.restoreOutcome,
             .mismatchReasons = diagnostics.mismatchReasons };
}

devpiano::core::RuntimePluginState MainComponent::buildRuntimePluginStateSnapshot() const
{
    return { .currentPluginName = pluginHost.getCurrentPluginName(),
             .availablePluginNames = pluginHost.getKnownPluginNames(),
             .lastScanSummary = pluginHost.getLastScanSummary(),
             .lastLoadError = pluginHost.getLastLoadError(),
             .preparedSampleRate = pluginHost.getPreparedSampleRate(),
             .preparedBlockSize = pluginHost.getPreparedBlockSize(),
             .supportsVst3 = pluginHost.supportsVst3(),
             .hasLoadedPlugin = pluginHost.hasLoadedPlugin(),
             .isPrepared = pluginHost.isPrepared(),
             .isEditorOpen = pluginEditorWindow != nullptr };
}

devpiano::core::RuntimeInputState MainComponent::buildRuntimeInputStateSnapshot() const
{
    return { .keyboardLayout = keyboardMidiMapper.getLayout(),
             .openMidiInputCount = midiRouter.getOpenInputCount(),
             .midiActivityCount = externalMidiMessageCount,
             .lastMidiMessage = lastExternalMidiMessage };
}

devpiano::core::AppState MainComponent::buildCurrentAppStateSnapshot() const
{
    return devpiano::core::buildAppState(appSettings,
                                         buildRuntimeAudioStateSnapshot(),
                                         buildRuntimePluginStateSnapshot(),
                                         buildRuntimeInputStateSnapshot());
}

void MainComponent::restorePluginStateOnStartup()
{
    const auto plan = devpiano::plugin::buildStartupPluginRestorePlan(appSettings.getPluginRecoverySettingsView(),
                                                                      pluginHost.getDefaultVst3SearchPath());

    if (devpiano::plugin::tryRestoreCachedPluginList(pluginHost, appSettings, plan))
    {
        refreshReadOnlyUiStateFromCurrentSnapshot();

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

void MainComponent::restorePluginScanPathOnStartup(const devpiano::plugin::StartupPluginRestorePlan& plan)
{
    const auto path = juce::FileSearchPath(plan.recovery.pluginSearchPath);
    if (! devpiano::plugin::isUsablePluginScanPath(path))
        return;

    devpiano::plugin::scanPluginsAtPathAndUpdateRecovery(pluginHost,
                                                         appSettings,
                                                         path,
                                                         plan.recovery.lastPluginName);
}

void MainComponent::restoreLastPluginOnStartup(const devpiano::plugin::StartupPluginRestorePlan& plan)
{
    const auto& pluginName = plan.recovery.lastPluginName;
    if (pluginName.isEmpty())
        return;

    restorePluginByNameOnStartup(pluginName);
}

void MainComponent::restorePluginByNameOnStartup(const juce::String& pluginName)
{
    runPluginActionWithAudioDeviceRebuild([this, pluginName](const RuntimeAudioConfig& config)
    {
        const auto loaded = pluginHost.loadPluginByName(pluginName,
                                                        config.sampleRate,
                                                        config.blockSize);
        juce::ignoreUnused(loaded);
    });
}

juce::FileSearchPath MainComponent::resolvePluginScanPath() const
{
    return devpiano::plugin::normalisePluginScanPath(juce::FileSearchPath(pluginPanel.getPluginPathText().trim()),
                                                     pluginHost.getDefaultVst3SearchPath());
}

double MainComponent::getCurrentRuntimeSampleRate() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const auto rate = device->getCurrentSampleRate();
        if (rate > 0.0)
            return rate;
    }

    return appSettings.getAudioSettingsView().sampleRate;
}

int MainComponent::getCurrentRuntimeBlockSize() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const auto size = device->getCurrentBufferSizeSamples();
        if (size > 0)
            return size;
    }

    return appSettings.getAudioSettingsView().bufferSize;
}

MainComponent::RuntimeAudioConfig MainComponent::getCurrentRuntimeAudioConfig() const
{
    return { .sampleRate = getCurrentRuntimeSampleRate(),
             .blockSize = getCurrentRuntimeBlockSize() };
}

void MainComponent::runPluginActionWithAudioDeviceRebuild(const std::function<void(const RuntimeAudioConfig&)>& action)
{
    struct AudioDeviceRebuildGuard final
    {
        explicit AudioDeviceRebuildGuard(MainComponent& ownerIn) : owner(ownerIn) {}
        ~AudioDeviceRebuildGuard() { owner.finishAudioDeviceRebuild(); }

        MainComponent& owner;
    };

    const auto runtimeAudioConfig = getCurrentRuntimeAudioConfig();

    prepareForAudioDeviceRebuild();
    const AudioDeviceRebuildGuard rebuildGuard(*this);
    action(runtimeAudioConfig);
}

void MainComponent::runPluginActionWithAudioDeviceRebuild(const std::function<void()>& action)
{
    runPluginActionWithAudioDeviceRebuild([&action](const RuntimeAudioConfig&)
    {
        action();
    });
}

juce::String MainComponent::getSelectedPluginNameForLoad() const
{
    return pluginPanel.getSelectedPluginName().trim();
}

void MainComponent::loadPluginByNameAndCommitState(const juce::String& pluginName)
{
    runPluginActionWithAudioDeviceRebuild([this, pluginName](const RuntimeAudioConfig& config)
    {
        const auto success = pluginHost.loadPluginByName(pluginName,
                                                         config.sampleRate,
                                                         config.blockSize);
        juce::ignoreUnused(success);
    });

    commitPluginRecoveryStateAndFinishUi(devpiano::plugin::makePluginRecoverySettings(getPersistedPluginSearchPath(), pluginName),
                                          true);
}

void MainComponent::loadSelectedPlugin()
{
    const auto pluginName = getSelectedPluginNameForLoad();
    if (pluginName.isEmpty())
    {
        finishPluginUiAction(false);
        return;
    }

    loadPluginByNameAndCommitState(pluginName);
}

void MainComponent::unloadPluginAndCommitState()
{
    runPluginActionWithAudioDeviceRebuild([this]
    {
        pluginHost.unloadPlugin();
    });

    commitPluginRecoveryStateAndFinishUi(appSettings.getPluginRecoverySettingsView(), true);
}

void MainComponent::unloadCurrentPlugin()
{
    unloadPluginAndCommitState();
}

std::unique_ptr<juce::AudioProcessorEditor> MainComponent::tryCreatePluginEditor() const
{
    auto* instance = pluginHost.getInstance();
    if (instance == nullptr || ! instance->hasEditor())
        return nullptr;

    return std::unique_ptr<juce::AudioProcessorEditor>(instance->createEditorAndMakeActive());
}

void MainComponent::handlePluginEditorWindowClosedAsync()
{
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<MainComponent>(this)]
    {
        if (safe == nullptr)
            return;

        safe->closePluginEditorWindow();
        safe->finishPluginUiAction(false);
    });
}

void MainComponent::closePluginEditorWindow()
{
    pluginEditorWindow.reset();
}

void MainComponent::openPluginEditorWindow(std::unique_ptr<juce::AudioProcessorEditor> editor)
{
    auto closeEditorWindow = [safe = juce::Component::SafePointer<MainComponent>(this)]
    {
        if (safe != nullptr)
            safe->handlePluginEditorWindowClosedAsync();
    };

    pluginEditorWindow = std::make_unique<PluginEditorWindow>(pluginHost.getCurrentPluginName(),
                                                               std::move(editor),
                                                               closeEditorWindow);
    pluginEditorWindow->centreAroundComponent(this,
                                              pluginEditorWindow->getContentComponent()->getWidth(),
                                              pluginEditorWindow->getContentComponent()->getHeight());
    pluginEditorWindow->setVisible(true);
    refreshReadOnlyUiStateFromCurrentSnapshot();
}

void MainComponent::togglePluginEditor()
{
    if (pluginEditorWindow != nullptr)
    {
        closePluginEditorWindow();
        finishPluginUiAction(false);
        return;
    }

    auto editor = tryCreatePluginEditor();
    if (editor == nullptr)
    {
        finishPluginUiAction(false);
        return;
    }

    openPluginEditorWindow(std::move(editor));
}

void MainComponent::scanPluginsAtPathAndApplyRecoveryState(const juce::FileSearchPath& path,
                                                           const juce::String& lastPluginName)
{
    runPluginActionWithAudioDeviceRebuild([this, &path, lastPluginName]
    {
        devpiano::plugin::scanPluginsAtPathAndUpdateRecovery(pluginHost,
                                                             appSettings,
                                                             path,
                                                             lastPluginName);
    });
}

void MainComponent::scanPluginsAtPathAndCommitState(const juce::FileSearchPath& path)
{
    const auto lastPluginName = appSettings.getPluginRecoverySettingsView().lastPluginName;
    scanPluginsAtPathAndApplyRecoveryState(path, lastPluginName);
    pluginPanel.setPluginPathText(makeSafeUiText(path.toString()));
    finishPluginUiAction(true);
}

void MainComponent::scanPlugins()
{
    const auto path = resolvePluginScanPath();
    if (! devpiano::plugin::isUsablePluginScanPath(path))
    {
        pluginHost.markPluginScanSkipped("No usable VST3 scan directories. Check the path field.");
        finishPluginUiAction(false);
        return;
    }

    scanPluginsAtPathAndCommitState(path);
}
