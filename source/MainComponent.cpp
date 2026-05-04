#include "MainComponent.h"

#include "Diagnostics/DebugLog.h"
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
    pluginOperationController = std::make_unique<devpiano::plugin::PluginOperationController>(
        *this, pluginHost, appSettings, pluginPanel);
    settingsWindowManager = std::make_unique<devpiano::settings::SettingsWindowManager>();

    initialiseUi();
    syncUiFromSettings();
    applyUiStateToAudioEngine();

    initialiseAudioDevice();
    suppressTextInputMethods();

    initialiseMidiRouting();
    pluginOperationController->restorePluginStateOnStartup();
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

    saveSettingsNow();

    midiRouter.setMessageCallback({});
    midiRouter.setCollector(nullptr);
    midiRouter.closeInputs();

    pluginOperationController.reset();
    shutdownAudio();
    audioEngine.setRecordingEngine(nullptr);
    pluginHost.unloadPlugin();
    settingsWindowManager.reset();
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
    pluginPanel.onScanRequested = [this] { pluginOperationController->scanPlugins(); };
    pluginPanel.onLoadRequested = [this] { pluginOperationController->loadSelectedPlugin(); };
    pluginPanel.onUnloadRequested = [this] { pluginOperationController->unloadCurrentPlugin(); };
    pluginPanel.onToggleEditorRequested = [this] { pluginOperationController->togglePluginEditor(); };

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
    controlsPanel.onSavePerformanceClicked = [this] { recordingSessionController->handleSavePerformanceClicked(); };
    controlsPanel.onOpenPerformanceClicked = [this] { recordingSessionController->handleOpenPerformanceClicked(); };
    controlsPanel.onPlaybackSpeedChange = [this](double speed) { recordingSessionController->handlePlaybackSpeedChange(speed); };

    // Initialize playback speed to 1.0x (never persisted — default on every launch).
    controlsPanel.setPlaybackSpeed(1.0);
    recordingEngine.setPlaybackSpeedMultiplier(1.0);

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
    // If focus is on a child component (e.g. TextEditor), don't intercept piano keys
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
    {
        if (focused != this && isParentOf(focused))
            return false;
    }

    const auto handled = keyboardMidiMapper.handleKeyPressed(key, audioEngine.getKeyboardState());

    if (handled)
        suppressTextInputMethods();

    return handled;
}

bool MainComponent::keyStateChanged(bool isKeyDown)
{
    juce::ignoreUnused(isKeyDown);

    // If focus is on a child component (e.g. TextEditor), don't intercept piano keys
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
    {
        if (focused != this && isParentOf(focused))
            return false;
    }

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

    if (pluginOperationController != nullptr && pluginOperationController->hasEditorWindowOpen())
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

    if (pluginOperationController != nullptr && pluginOperationController->hasEditorWindowOpen())
        return;

    if (isShowing() && juce::Component::getCurrentlyFocusedComponent() != this)
        grabKeyboardFocus();

    suppressTextInputMethods();
}

void MainComponent::initialiseAudioDevice()
{
    const auto audioSettings = appSettings.getAudioSettingsView();
    const auto* savedState = (audioSettings.hasSerializedDeviceState && appSettings.audioDeviceState != nullptr)
                               ? appSettings.audioDeviceState.get()
                               : nullptr;

    setAudioChannels(0, 2, savedState);

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
    if (pluginOperationController)
        pluginOperationController->closePluginEditorWindow();
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
    settingsWindowManager->show({ .parent = *this,
                                  .deviceManager = deviceManager,
                                  .savedAudioDeviceState = appSettings.audioDeviceState.get(),
                                  .onSaveRequested = [safe = juce::Component::SafePointer<MainComponent>(this)]
                                  {
                                      if (safe != nullptr)
                                          safe->saveSettingsNow();
                                  },
                                  .onClosed = [safe = juce::Component::SafePointer<MainComponent>(this)]
                                  {
                                      if (safe != nullptr)
                                          safe->restoreKeyboardFocus();
                                  } });
}

bool MainComponent::isSettingsWindowOpen() const
{
    return settingsWindowManager != nullptr && settingsWindowManager->isOpen();
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

void MainComponent::refreshPluginUiState()
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
    DP_LOG_INFO("[AudioDevice] " + context + "\n" + diagnostics.detailedSummary);
}

devpiano::core::AppState MainComponent::buildCurrentAppStateSnapshot() const
{
    return devpiano::core::buildCurrentAppStateSnapshot(appSettings,
                                                        deviceManager,
                                                        pluginHost,
                                                        pluginOperationController != nullptr
                                                            && pluginOperationController->hasEditorWindowOpen(),
                                                        keyboardMidiMapper,
                                                        midiRouter,
                                                        externalMidiMessageCount,
                                                        lastExternalMidiMessage);
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
