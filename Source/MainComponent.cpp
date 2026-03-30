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
    saveSettingsNow();

    midiRouter.closeInputs();

    shutdownAudio();
    pluginEditorWindow.reset();
    pluginHost.unloadPlugin();
    settingsWindow.reset();
}

void MainComponent::initialiseInputMappingFromSettings()
{
    const auto inputMapping = appSettings.getInputMappingSettingsView();
    keyboardMidiMapper.setLayout(SettingsModel::keyMapToLayout(inputMapping.keyMap));
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

    const auto pluginRecovery = appSettings.getPluginRecoverySettingsView();
    pluginPanel.setPluginPathText(pluginRecovery.pluginSearchPath.isNotEmpty()
                                      ? pluginRecovery.pluginSearchPath
                                      : pluginHost.getDefaultVst3SearchPath().toString());

    addAndMakeVisible(controlsPanel);
    controlsPanel.onValuesChanged = [this] { handlePerformanceUiChanged(); };

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

    headerPanel.setBounds(area.removeFromTop(76));
    area.removeFromTop(10);

    pluginPanel.setBounds(area.removeFromTop(210));
    area.removeFromTop(12);

    controlsPanel.setBounds(area.removeFromTop(180));
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
}

void MainComponent::syncSettingsFromUi()
{
    appSettings.applyPerformanceSettingsView(getPerformanceSettingsFromUi());

    appSettings.applyPluginRecoverySettingsView({ .pluginSearchPath = pluginPanel.getPluginPathText().trim(),
                                                  .lastPluginName = pluginHost.hasLoadedPlugin() ? pluginHost.getCurrentPluginName()
                                                                                                 : pluginPanel.getSelectedPluginName().trim() });

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
        deviceManager.initialise(0, 2, appSettings.audioDeviceState.get(), true);

    captureAudioDeviceState();
}

void MainComponent::captureAudioDeviceState()
{
    if (auto xml = deviceManager.createStateXml())
        appSettings.setSerializedAudioDeviceState(std::move(xml));
}

void MainComponent::prepareForAudioDeviceRebuild()
{
    captureAudioDeviceState();
    shutdownAudio();
    pluginEditorWindow.reset();
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

    auto content = std::make_unique<SettingsComponent>(deviceManager);
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
    settingsWindow->centreAroundComponent(this, 620, 420);
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

void MainComponent::refreshPluginPanel()
{
    pluginPanel.updateState(buildPluginPanelState(createAppStateSnapshot()));
}

void MainComponent::finishPluginUiAction(bool shouldSaveSettings)
{
    if (shouldSaveSettings)
        saveSettingsSoon();

    refreshReadOnlyUiState();
    restoreKeyboardFocus();
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
                                         createRuntimePluginStateSnapshot(),
                                         createRuntimeInputStateSnapshot());
}

void MainComponent::restorePluginStateOnStartup()
{
    restorePluginScanAndLoadState();
}

void MainComponent::restorePluginScanAndLoadState()
{
    restorePluginScanPathOnStartup();
    restoreLastPluginOnStartup();
}

void MainComponent::restorePluginScanPathOnStartup()
{
    const auto pluginRecovery = appSettings.getPluginRecoverySettingsView();
    const auto path = resolvePluginScanPath();

    if (path.toString().trim().isEmpty())
        return;

    pluginHost.scanVst3Plugins(path, true);
    appSettings.applyPluginRecoverySettingsView({ .pluginSearchPath = path.toString(),
                                                  .lastPluginName = pluginRecovery.lastPluginName });
}

void MainComponent::restoreLastPluginOnStartup()
{
    const auto pluginRecovery = appSettings.getPluginRecoverySettingsView();
    if (pluginRecovery.lastPluginName.isEmpty())
        return;

    runPluginActionWithAudioDeviceRebuild([this, pluginRecovery](double sampleRate, int blockSize)
    {
        const auto loaded = pluginHost.loadPluginByName(pluginRecovery.lastPluginName, sampleRate, blockSize);
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

void MainComponent::runPluginActionWithAudioDeviceRebuild(const std::function<void(double, int)>& action)
{
    const auto sampleRate = getCurrentRuntimeSampleRate();
    const auto blockSize = getCurrentRuntimeBlockSize();

    prepareForAudioDeviceRebuild();
    action(sampleRate, blockSize);
    finishAudioDeviceRebuild();
}

void MainComponent::runPluginActionWithAudioDeviceRebuild(const std::function<void()>& action)
{
    prepareForAudioDeviceRebuild();
    action();
    finishAudioDeviceRebuild();
}

void MainComponent::loadSelectedPlugin()
{
    const auto pluginName = pluginPanel.getSelectedPluginName().trim();
    if (pluginName.isEmpty())
    {
        finishPluginUiAction(false);
        return;
    }

    runPluginActionWithAudioDeviceRebuild([this, pluginName](double sampleRate, int blockSize)
    {
        const auto success = pluginHost.loadPluginByName(pluginName, sampleRate, blockSize);
        juce::ignoreUnused(success);
    });

    appSettings.applyPluginRecoverySettingsView({ .pluginSearchPath = appSettings.getPluginRecoverySettingsView().pluginSearchPath,
                                                  .lastPluginName = pluginName });
    finishPluginUiAction(true);
}

void MainComponent::unloadCurrentPlugin()
{
    runPluginActionWithAudioDeviceRebuild([this]
    {
        pluginHost.unloadPlugin();
    });
    finishPluginUiAction(true);
}

void MainComponent::togglePluginEditor()
{
    if (pluginEditorWindow != nullptr)
    {
        pluginEditorWindow.reset();
        finishPluginUiAction(false);
        return;
    }

    auto* instance = pluginHost.getInstance();
    if (instance == nullptr || ! instance->hasEditor())
    {
        finishPluginUiAction(false);
        return;
    }

    auto editor = std::unique_ptr<juce::AudioProcessorEditor>(instance->createEditorAndMakeActive());
    if (editor == nullptr)
    {
        finishPluginUiAction(false);
        return;
    }

    class PluginEditorWindow final : public juce::DocumentWindow
    {
    public:
        PluginEditorWindow(const juce::String& title,
                           std::function<void()> onClose)
            : juce::DocumentWindow(title,
                                   juce::Desktop::getInstance().getDefaultLookAndFeel()
                                       .findColour(juce::ResizableWindow::backgroundColourId),
                                   juce::DocumentWindow::closeButton),
              closeCallback(std::move(onClose))
        {
        }

        void closeButtonPressed() override
        {
            if (closeCallback)
                closeCallback();
        }

    private:
        std::function<void()> closeCallback;
    };

    auto closeEditorWindow = [safe = juce::Component::SafePointer<MainComponent>(this)]
    {
        juce::MessageManager::callAsync([safe]
        {
            if (safe == nullptr)
                return;

            safe->pluginEditorWindow.reset();
            safe->finishPluginUiAction(false);
        });
    };

    auto title = pluginHost.getCurrentPluginName();
    if (title.isEmpty())
        title = "Plugin Editor";
    else
        title << " Editor";

    pluginEditorWindow = std::make_unique<PluginEditorWindow>(title, closeEditorWindow);
    pluginEditorWindow->setUsingNativeTitleBar(true);
    pluginEditorWindow->setResizable(editor->isResizable(), true);
    pluginEditorWindow->setContentOwned(editor.release(), true);
    pluginEditorWindow->centreAroundComponent(this,
                                              pluginEditorWindow->getContentComponent()->getWidth(),
                                              pluginEditorWindow->getContentComponent()->getHeight());
    pluginEditorWindow->setVisible(true);
    refreshPluginPanel();
}

void MainComponent::scanPlugins()
{
    const auto path = resolvePluginScanPath();

    pluginHost.scanVst3Plugins(path, true);
    appSettings.applyPluginRecoverySettingsView({ .pluginSearchPath = path.toString(),
                                                  .lastPluginName = appSettings.getPluginRecoverySettingsView().lastPluginName });
    finishPluginUiAction(true);
}
