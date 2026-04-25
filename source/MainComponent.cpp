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

    initialiseUi();
    syncUiFromSettings();
    applyUiStateToAudioEngine();

    initialiseAudioDevice();
    suppressTextInputMethods();

    initialiseMidiRouting();
    restorePluginStateOnStartup();
    refreshReadOnlyUiState();

    restoreKeyboardFocus();
}

MainComponent::~MainComponent()
{
    controlsPanel.onValuesChanged = {};
    controlsPanel.onLayoutChanged = {};
    controlsPanel.onSaveLayoutRequested = {};
    controlsPanel.onResetLayoutRequested = {};
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
    setSize(980, 620);
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
    controlsPanel.onLayoutChanged = [this](const juce::String& newId) { handleLayoutChanged(newId); };
    controlsPanel.onSaveLayoutRequested = [this] { handleSaveLayoutRequested(); };
    controlsPanel.onResetLayoutRequested = [this] { handleResetLayoutToDefaultRequested(); };

    addAndMakeVisible(keyboardPanel);
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

            safe->updateMidiStatusLabel();
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

    controlsPanel.setBounds(area.removeFromTop(200));
    area.removeFromTop(8);

    keyboardPanel.setBounds(area.removeFromBottom(110));
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

void MainComponent::handleLayoutChanged(const juce::String& newLayoutId)
{
    const auto nextLayoutId = newLayoutId.trim();
    if (nextLayoutId.isEmpty() || nextLayoutId == keyboardMidiMapper.getLayout().id)
    {
        restoreKeyboardFocus();
        return;
    }

    audioEngine.getKeyboardState().allNotesOff(1);
    keyboardMidiMapper.setLayout(SettingsModel::keyMapToLayout({}, nextLayoutId));
    appSettings.applyInputMappingSettingsView({ .layoutId = keyboardMidiMapper.getLayout().id,
                                                .keyMap = SettingsModel::layoutToKeyMap(keyboardMidiMapper.getLayout()) });
    syncUiFromSettings();
    saveSettingsSoon();
    restoreKeyboardFocus();
}

void MainComponent::handleSaveLayoutRequested()
{
    appSettings.applyInputMappingSettingsView({ .layoutId = keyboardMidiMapper.getLayout().id,
                                                .keyMap = SettingsModel::layoutToKeyMap(keyboardMidiMapper.getLayout()) });
    syncUiFromSettings();
    saveSettingsNow();
    restoreKeyboardFocus();
}

void MainComponent::handleResetLayoutToDefaultRequested()
{
    const auto currentLayoutId = keyboardMidiMapper.getLayout().id.trim();
    const auto layoutId = currentLayoutId.isNotEmpty() ? currentLayoutId
                                                       : appSettings.getInputMappingSettingsView().layoutId;

    audioEngine.getKeyboardState().allNotesOff(1);
    keyboardMidiMapper.setLayout(SettingsModel::keyMapToLayout({}, layoutId));
    appSettings.applyInputMappingSettingsView({ .layoutId = keyboardMidiMapper.getLayout().id,
                                                .keyMap = SettingsModel::layoutToKeyMap(keyboardMidiMapper.getLayout()) });
    syncUiFromSettings();
    saveSettingsSoon();
    restoreKeyboardFocus();
}

void MainComponent::applyUiStateToAudioEngine()
{
    applyPerformanceSettingsToAudioEngine(getPerformanceSettingsFromUi());
}

void MainComponent::syncUiFromSettings()
{
    applyPerformanceSettingsToUi(appSettings.getPerformanceSettingsView());
    
    juce::StringArray layoutIds = { "default.freepiano.minimal", "default.freepiano.full" };
    controlsPanel.setLayouts(layoutIds, appSettings.getInputMappingSettingsView().layoutId);
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
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<MainComponent>(this)]
    {
        if (safe != nullptr && safe->isShowing() && ! safe->hasKeyboardFocus(true))
            safe->grabKeyboardFocus();

        if (safe != nullptr)
            safe->suppressTextInputMethods();
    });
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
}

bool MainComponent::isSettingsWindowDirty() const
{
    if (settingsWindow == nullptr)
        return false;

    if (auto* settingsContent = dynamic_cast<SettingsComponent*>(settingsWindow->getContentComponent()))
        return settingsContent->isDirty();

    return false;
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

void MainComponent::applyReadOnlyUiState(const devpiano::core::AppState& appState)
{
    headerPanel.updateMidiStatus(buildHeaderPanelMidiStatus(appState));
    headerPanel.updateAudioStatus(buildHeaderPanelAudioStatus(appState));
    pluginPanel.updateState(buildPluginPanelState(appState));
}

void MainComponent::refreshReadOnlyUiState()
{
    applyReadOnlyUiState(createAppStateSnapshot());
}

void MainComponent::updateMidiStatusLabel()
{
    headerPanel.updateMidiStatus(buildHeaderPanelMidiStatus(createAppStateSnapshot()));
}

void MainComponent::finishPluginUiAction(bool shouldSaveSettings)
{
    if (shouldSaveSettings)
        saveSettingsSoon();

    refreshReadOnlyUiState();
    restoreKeyboardFocus();
}

void MainComponent::logCurrentAudioDeviceDiagnostics(const juce::String& context) const
{
    const auto diagnostics = devpiano::audio::buildAudioDeviceDiagnostics(appSettings.audioDeviceState.get(), deviceManager);
    juce::Logger::writeToLog("[AudioDevice] " + context + "\n" + diagnostics.detailedSummary);
}

devpiano::core::RuntimeAudioState MainComponent::createRuntimeAudioStateSnapshot() const
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

devpiano::core::RuntimePluginState MainComponent::createRuntimePluginStateSnapshot() const
{
    return { .currentPluginName = pluginHost.getCurrentPluginName(),
             .availablePluginNames = pluginHost.getKnownPluginNames(),
             .pluginListText = pluginHost.getPluginListDescription(),
             .availableFormatsDescription = pluginHost.getAvailableFormatsDescription(),
             .lastScanSummary = pluginHost.getLastScanSummary(),
             .lastLoadError = pluginHost.getLastLoadError(),
             .preparedSampleRate = pluginHost.getPreparedSampleRate(),
             .preparedBlockSize = pluginHost.getPreparedBlockSize(),
             .supportsVst3 = pluginHost.supportsVst3(),
             .hasLoadedPlugin = pluginHost.hasLoadedPlugin(),
             .isPrepared = pluginHost.isPrepared(),
             .isEditorOpen = pluginEditorWindow != nullptr };
}

devpiano::core::RuntimeInputState MainComponent::createRuntimeInputStateSnapshot() const
{
    return { .keyboardLayout = keyboardMidiMapper.getLayout(),
             .openMidiInputCount = midiRouter.getOpenInputCount(),
             .midiActivityCount = externalMidiMessageCount,
             .lastMidiMessage = lastExternalMidiMessage };
}

devpiano::core::AppState MainComponent::createAppStateSnapshot() const
{
    return devpiano::core::buildAppState(appSettings,
                                         createRuntimeAudioStateSnapshot(),
                                         createRuntimePluginStateSnapshot(),
                                         createRuntimeInputStateSnapshot());
}

void MainComponent::restorePluginStateOnStartup()
{
    const auto plan = devpiano::plugin::buildStartupPluginRestorePlan(appSettings.getPluginRecoverySettingsView(),
                                                                      pluginHost.getDefaultVst3SearchPath());

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

    devpiano::plugin::restorePluginsAtPath(pluginHost,
                                           path,
                                           plan.recovery,
                                           [this](const SettingsModel::PluginRecoverySettingsView& settings)
    {
        applyPluginRecoverySettings(settings);
    });
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
    const auto pathText = pluginPanel.getPluginPathText().trim();
    return pathText.isNotEmpty() ? juce::FileSearchPath(pathText)
                                 : pluginHost.getDefaultVst3SearchPath();
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
    refreshReadOnlyUiState();
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
    const auto settings = devpiano::plugin::makePluginRecoverySettings(path.toString(), lastPluginName);
    runPluginActionWithAudioDeviceRebuild([this, &path, &settings]
    {
        devpiano::plugin::restorePluginsAtPath(pluginHost,
                                               path,
                                               settings,
                                               [this](const SettingsModel::PluginRecoverySettingsView& s)
        {
            applyPluginRecoverySettings(s);
        });
    });
}

void MainComponent::scanPluginsAtPathAndCommitState(const juce::FileSearchPath& path)
{
    const auto lastPluginName = appSettings.getPluginRecoverySettingsView().lastPluginName;
    scanPluginsAtPathAndApplyRecoveryState(path, lastPluginName);
    finishPluginUiAction(true);
}

void MainComponent::scanPlugins()
{
    const auto path = resolvePluginScanPath();
    if (! devpiano::plugin::isUsablePluginScanPath(path))
    {
        finishPluginUiAction(false);
        return;
    }

    scanPluginsAtPathAndCommitState(path);
}
