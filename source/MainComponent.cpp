#include "MainComponent.h"
#include "Export/ExportFlowSupport.h"
#include "Recording/MidiFileExporter.h"
#include "Recording/MidiFileImporter.h"
#include "Recording/RecordingFlowSupport.h"
#include "Recording/WavFileExporter.h"
#include "UI/HeaderPanelStateBuilder.h"
#include "UI/PluginPanelStateBuilder.h"
#include "Layout/LayoutPreset.h"

#if JUCE_WINDOWS
struct HWND__;
using HWND = HWND__*;
struct HIMC__;
using HIMC = HIMC__*;
extern "C" HIMC __stdcall ImmAssociateContext(HWND, HIMC);
#endif

namespace
{
[[nodiscard]] devpiano::recording::RecordingFlowState toRecordingFlowState(ControlsPanel::RecordingState state) noexcept
{
    switch (state)
    {
        case ControlsPanel::RecordingState::idle: return devpiano::recording::RecordingFlowState::idle;
        case ControlsPanel::RecordingState::recording: return devpiano::recording::RecordingFlowState::recording;
        case ControlsPanel::RecordingState::playing: return devpiano::recording::RecordingFlowState::playing;
    }

    return devpiano::recording::RecordingFlowState::idle;
}

[[nodiscard]] ControlsPanel::RecordingState toControlsPanelRecordingState(devpiano::recording::RecordingFlowState state) noexcept
{
    switch (state)
    {
        case devpiano::recording::RecordingFlowState::idle: return ControlsPanel::RecordingState::idle;
        case devpiano::recording::RecordingFlowState::recording: return ControlsPanel::RecordingState::recording;
        case devpiano::recording::RecordingFlowState::playing: return ControlsPanel::RecordingState::playing;
    }

    return ControlsPanel::RecordingState::idle;
}

[[nodiscard]] devpiano::recording::RecordingFlowStatus makeRecordingFlowStatus(ControlsPanel::RecordingState state,
                                                                               bool hasTake) noexcept
{
    return { .currentState = toRecordingFlowState(state),
             .hasTake = hasTake };
}
} // namespace

namespace
{
const auto backgroundColour = juce::Colour(0xff202225);
constexpr std::size_t defaultRecordingEventsPerSecond = 100;
constexpr std::size_t defaultRecordingCapacitySeconds = 30 * 60;
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

[[nodiscard]] std::optional<std::pair<juce::File, devpiano::core::KeyboardLayout>> findUserLayoutFileById(const juce::String& layoutId)
{
    if (layoutId.isEmpty() || layoutId.startsWith("default."))
        return std::nullopt;

    auto dir = devpiano::layout::getUserLayoutDirectory();
    for (const auto& entry : dir.findChildFiles(juce::File::TypesOfFileToFind::findFiles, false, "*.freepiano.layout"))
    {
        auto loaded = devpiano::layout::loadLayoutPreset(entry);
        if (!loaded.has_value())
            continue;

        loaded->id = devpiano::layout::getUserLayoutIdForFile(entry);
        if (loaded->name.trim().isEmpty())
            loaded->name = devpiano::layout::getLayoutPresetDisplayNameForFile(entry);

        if (loaded->id == layoutId)
            return std::make_pair(entry, *loaded);
    }

    return std::nullopt;
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
    controlsPanel.onLayoutChanged = [this](const juce::String& newId) { handleLayoutChanged(newId); };
    controlsPanel.onSaveLayoutRequested = [this] { handleSaveLayoutRequested(); };
    controlsPanel.onResetLayoutRequested = [this] { handleResetLayoutToDefaultRequested(); };
    controlsPanel.onImportLayoutRequested = [this] { handleImportLayoutRequested(); };
    controlsPanel.onRenameLayoutRequested = [this] { handleRenameLayoutRequested(); };
    controlsPanel.onDeleteLayoutRequested = [this] { handleDeleteLayoutRequested(); };
    controlsPanel.onRecordClicked = [this] { handleRecordClicked(); };
    controlsPanel.onPlayClicked = [this] { handlePlayClicked(); };
    controlsPanel.onStopClicked = [this] { handleStopClicked(); };
    controlsPanel.onExportMidiClicked = [this] { handleExportMidiClicked(); };
    controlsPanel.onExportWavClicked = [this] { handleExportWavClicked(); };
    controlsPanel.onImportMidiClicked = [this] { handleImportMidiClicked(); };

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
    if (! recordingEngine.consumePlaybackEndedFlag())
        return;

    if (currentRecordingState != ControlsPanel::RecordingState::playing)
        return;

    const auto stoppedTake = stopInternalPlayback();
    juce::ignoreUnused(stoppedTake);

    currentRecordingState = ControlsPanel::RecordingState::idle;
    controlsPanel.setRecordingState(currentRecordingState);
    restoreKeyboardFocus();
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
    saveLayoutChooser = std::make_unique<juce::FileChooser>("Save Layout", devpiano::layout::getUserLayoutDirectory(), "*.freepiano.layout");
    saveLayoutChooser->launchAsync(juce::FileBrowserComponent::saveMode, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        auto path = file.getFullPathName();
        if (path.isEmpty())
            return;

        auto currentLayout = keyboardMidiMapper.getLayout();
        const auto targetLayoutId = devpiano::layout::getUserLayoutIdForFile(file);
        currentLayout.id = targetLayoutId;
        if (currentLayout.name.trim().isEmpty())
            currentLayout.name = devpiano::layout::getLayoutPresetDisplayNameForFile(file);

        auto saved = devpiano::layout::saveLayoutPreset(currentLayout, file);
        juce::Logger::writeToLog(saved ? "Layout saved: " + file.getFullPathName() : "Layout save FAILED: " + file.getFullPathName());
        if (saved)
        {
            audioEngine.getKeyboardState().allNotesOff(1);
            keyboardMidiMapper.setLayout(currentLayout);
            appSettings.applyInputMappingSettingsView({ .layoutId = keyboardMidiMapper.getLayout().id,
                                                        .keyMap = SettingsModel::layoutToKeyMap(keyboardMidiMapper.getLayout()) });
            syncUiFromSettings();
            saveSettingsSoon();
            restoreKeyboardFocus();
        }
    });
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

void MainComponent::handleImportLayoutRequested()
{
    importLayoutChooser = std::make_unique<juce::FileChooser>("Import Layout", juce::File{}, "*.freepiano.layout");
    importLayoutChooser->launchAsync(juce::FileBrowserComponent::openMode, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (!file.exists())
            return;

        auto optLayout = devpiano::layout::loadLayoutPreset(file);
        if (!optLayout.has_value())
            return;

        auto layout = *optLayout;
        auto userDir = devpiano::layout::getUserLayoutDirectory();

        auto originalName = file.getFileName();
        auto destFile = userDir.getChildFile(originalName);
        layout.id = devpiano::layout::getUserLayoutIdForFile(destFile);
        if (layout.name.trim().isEmpty())
            layout.name = devpiano::layout::getLayoutPresetDisplayNameForFile(destFile);

        if (!devpiano::layout::saveLayoutPreset(layout, destFile))
            return;

        audioEngine.getKeyboardState().allNotesOff(1);
        keyboardMidiMapper.setLayout(layout);
        appSettings.applyInputMappingSettingsView({ .layoutId = keyboardMidiMapper.getLayout().id,
                                                    .keyMap = SettingsModel::layoutToKeyMap(keyboardMidiMapper.getLayout()) });
        syncUiFromSettings();
        saveSettingsSoon();
        restoreKeyboardFocus();
    });
}

void MainComponent::handleRenameLayoutRequested()
{
    const auto layoutId = controlsPanel.getSelectedLayoutId().trim();
    if (layoutId.isEmpty() || layoutId.startsWith("default."))
        return;

    auto layoutFile = findUserLayoutFileById(layoutId);
    if (!layoutFile.has_value())
        return;

    const auto& [fileToRename, loadedLayout] = *layoutFile;
    auto currentDisplayName = loadedLayout.name.trim();
    if (currentDisplayName.isEmpty())
        currentDisplayName = devpiano::layout::getLayoutPresetDisplayNameForFile(fileToRename);

    auto* renameWindow = new juce::AlertWindow("Rename Layout",
                                               "Set the display name shown in the layout dropdown.",
                                               juce::AlertWindow::NoIcon);
    renameWindow->addTextEditor("displayName", currentDisplayName, "Display Name:");
    renameWindow->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
    renameWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    juce::Component::SafePointer<juce::AlertWindow> safeWindow(renameWindow);
    renameWindow->enterModalState(true,
                                  juce::ModalCallbackFunction::create([safeThis, safeWindow, layoutId](int result)
                                  {
                                      if (result != 1 || safeThis == nullptr || safeWindow == nullptr)
                                          return;

                                      auto updatedDisplayName = safeWindow->getTextEditorContents("displayName").trim();
                                      if (updatedDisplayName.isEmpty())
                                          return;

                                      auto layoutFileToUpdate = findUserLayoutFileById(layoutId);
                                      if (!layoutFileToUpdate.has_value())
                                          return;

                                      auto [resolvedFile, layout] = *layoutFileToUpdate;
                                      layout.name = updatedDisplayName;
                                      if (!devpiano::layout::saveLayoutPreset(layout, resolvedFile))
                                          return;

                                      if (safeThis->keyboardMidiMapper.getLayout().id == layoutId)
                                      {
                                          safeThis->keyboardMidiMapper.setLayoutDisplayName(updatedDisplayName);
                                          safeThis->appSettings.applyInputMappingSettingsView({ .layoutId = layoutId,
                                                                                               .keyMap = SettingsModel::layoutToKeyMap(safeThis->keyboardMidiMapper.getLayout()) });
                                      }

                                      safeThis->syncUiFromSettings();
                                      safeThis->saveSettingsSoon();
                                      safeThis->restoreKeyboardFocus();
                                  }),
                                  true);
}

void MainComponent::handleDeleteLayoutRequested()
{
    auto layoutId = controlsPanel.getSelectedLayoutId();
    if (layoutId.isEmpty() || layoutId.startsWith("default."))
        return;
    auto layoutName = layoutId;
    juce::File fileToDelete;
    if (auto layoutFile = findUserLayoutFileById(layoutId); layoutFile.has_value())
    {
        layoutName = layoutFile->second.name.isNotEmpty() ? layoutFile->second.name : layoutId;
        fileToDelete = layoutFile->first;
    }

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    auto capturedLayoutId = layoutId;
    auto capturedFileToDelete = fileToDelete;

    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions::makeOptionsOkCancel(
            juce::AlertWindow::WarningIcon,
            "Delete Layout",
            "Are you sure you want to delete \"" + layoutName + "\"?\nThis action cannot be undone.",
            "Delete",
            "Cancel"),
        [safeThis, capturedLayoutId, capturedFileToDelete](int result)
        {
            if (result != 1 || safeThis == nullptr)
                return;

            if (capturedFileToDelete.exists())
                capturedFileToDelete.deleteFile();

            if (safeThis->keyboardMidiMapper.getLayout().id == capturedLayoutId)
            {
                safeThis->audioEngine.getKeyboardState().allNotesOff(1);
                safeThis->keyboardMidiMapper.setLayout(devpiano::core::makeDefaultKeyboardLayout());
                safeThis->appSettings.applyInputMappingSettingsView({ .layoutId = safeThis->keyboardMidiMapper.getLayout().id,
                                                            .keyMap = SettingsModel::layoutToKeyMap(safeThis->keyboardMidiMapper.getLayout()) });
            }
            safeThis->syncUiFromSettings();
            safeThis->saveSettingsSoon();
            safeThis->restoreKeyboardFocus();
        });
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
    if (settingsWindow == nullptr)
        return false;

    if (auto* settingsContent = dynamic_cast<SettingsComponent*>(settingsWindow->getContentComponent()))
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

void MainComponent::renderReadOnlyUiState(const devpiano::core::AppState& appState)
{
    headerPanel.updateMidiStatus(buildHeaderPanelMidiStatus(appState));
    pluginPanel.updateState(buildPluginPanelState(appState));
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

void MainComponent::startInternalRecording(std::size_t expectedEventCapacity)
{
    const auto capacity = expectedEventCapacity > 0
                              ? expectedEventCapacity
                              : defaultRecordingEventsPerSecond * defaultRecordingCapacitySeconds;

    runPluginActionWithAudioDeviceRebuild([this, capacity](const RuntimeAudioConfig& config)
    {
        recordingEngine.reserveEvents(capacity);
        recordingEngine.startRecording(config.sampleRate);
    });

    juce::Logger::writeToLog("[Recording] Internal recording started; reserved events=" + juce::String(static_cast<int>(recordingEngine.getReservedEventCapacity())));
}

devpiano::recording::RecordingTake MainComponent::stopInternalRecording()
{
    devpiano::recording::RecordingTake take;

    runPluginActionWithAudioDeviceRebuild([this, &take](const RuntimeAudioConfig&)
    {
        take = recordingEngine.stopRecording();
    });

    juce::Logger::writeToLog("[Recording] Internal recording stopped; events="
                             + juce::String(static_cast<int>(take.events.size()))
                             + ", dropped="
                             + juce::String(static_cast<int>(recordingEngine.getDroppedEventCount())));
    return take;
}

void MainComponent::startInternalPlayback(const devpiano::recording::RecordingTake& take)
{
    if (take.isEmpty())
    {
        juce::Logger::writeToLog("[Playback] startInternalPlayback called with empty take - ignoring");
        return;
    }

    // Ensure any notes left active by a previous playback pass are released before
    // the next take starts. The AudioEngine injects these messages before rendering
    // playback events in the next audio block.
    audioEngine.requestAllNotesOff();

    runPluginActionWithAudioDeviceRebuild([this, &take](const RuntimeAudioConfig& config)
    {
        recordingEngine.startPlayback(take, config.sampleRate);
    });

    juce::Logger::writeToLog("[Playback] Internal playback started; take events="
                             + juce::String(static_cast<int>(take.events.size()))
                             + ", sampleRate="
                             + juce::String(take.sampleRate));
}

devpiano::recording::RecordingTake MainComponent::stopInternalPlayback()
{
    auto take = recordingEngine.getCurrentTake();
    recordingEngine.stopPlayback();
    audioEngine.requestAllNotesOff();

    juce::Logger::writeToLog("[Playback] Internal playback stopped");
    return take;
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

void MainComponent::handleRecordClicked()
{
    using namespace devpiano::recording;

    const auto command = chooseRecordingFlowCommand(RecordingFlowIntent::record,
                                                   makeRecordingFlowStatus(currentRecordingState, !currentTake.isEmpty()));
    if (command != RecordingFlowCommand::startRecording)
        return;

    recordingEngine.clear();
    currentTake = {};
    currentTakeCanBeExported = false;
    controlsPanel.setCanExportTake(false);
    controlsPanel.setHasTake(false);
    startInternalRecording(0);
    currentRecordingState = toControlsPanelRecordingState(getStateAfterCommand(command,
                                                                               toRecordingFlowState(currentRecordingState)));
    controlsPanel.setRecordingState(currentRecordingState);
    if (shouldRestoreKeyboardFocus(command))
        restoreKeyboardFocus();
}

void MainComponent::handlePlayClicked()
{
    using namespace devpiano::recording;

    const auto command = chooseRecordingFlowCommand(RecordingFlowIntent::play,
                                                   makeRecordingFlowStatus(currentRecordingState, !currentTake.isEmpty()));
    if (command != RecordingFlowCommand::startPlayback)
        return;

    startInternalPlayback(currentTake);
    currentRecordingState = toControlsPanelRecordingState(getStateAfterCommand(command,
                                                                               toRecordingFlowState(currentRecordingState)));
    controlsPanel.setRecordingState(currentRecordingState);
    if (shouldRestoreKeyboardFocus(command))
        restoreKeyboardFocus();
}

void MainComponent::handleStopClicked()
{
    using namespace devpiano::recording;

    const auto command = chooseRecordingFlowCommand(RecordingFlowIntent::stop,
                                                   makeRecordingFlowStatus(currentRecordingState, !currentTake.isEmpty()));

    if (command == RecordingFlowCommand::stopRecording)
    {
        currentTake = stopInternalRecording();
        currentTakeCanBeExported = !currentTake.isEmpty();
        controlsPanel.setCanExportTake(currentTakeCanBeExported);
        controlsPanel.setHasTake(!currentTake.isEmpty());
    }
    else if (command == RecordingFlowCommand::stopPlayback)
    {
        const auto stoppedTake = stopInternalPlayback();
        juce::ignoreUnused(stoppedTake);
    }
    else
    {
        return;
    }

    currentRecordingState = toControlsPanelRecordingState(getStateAfterCommand(command,
                                                                               toRecordingFlowState(currentRecordingState)));
    controlsPanel.setRecordingState(currentRecordingState);
    if (shouldRestoreKeyboardFocus(command))
        restoreKeyboardFocus();
}

void MainComponent::handleExportMidiClicked()
{
    using devpiano::exporting::ExportFileType;

    if (! currentTakeCanBeExported || ! devpiano::exporting::canExportTake(currentTake))
    {
        juce::Logger::writeToLog(devpiano::exporting::makeExportLogPrefix(ExportFileType::midi)
                                 + " export skipped: currentTake is empty or not exportable");
        return;
    }

    const auto defaultFile = devpiano::exporting::makeDefaultRecordingExportFile(ExportFileType::midi);

    exportMidiChooser = std::make_unique<juce::FileChooser>("Export MIDI Recording", defaultFile, "*.mid");
    exportMidiChooser->launchAsync(juce::FileBrowserComponent::saveMode
                                       | juce::FileBrowserComponent::canSelectFiles
                                       | juce::FileBrowserComponent::warnAboutOverwriting,
                                   [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();

        if (file == juce::File())
        {
            juce::Logger::writeToLog(devpiano::exporting::makeExportLogPrefix(ExportFileType::midi)
                                     + " export cancelled by user");
            return;
        }

        if (devpiano::exporting::exportTakeAsMidiFile(currentTake, file))
            juce::Logger::writeToLog(devpiano::exporting::makeExportLogPrefix(ExportFileType::midi)
                                     + " exported: " + file.getFullPathName());
        else
            juce::Logger::writeToLog(devpiano::exporting::makeExportLogPrefix(ExportFileType::midi)
                                     + " export FAILED: " + file.getFullPathName());

        exportMidiChooser.reset();
    });
}

void MainComponent::handleExportWavClicked()
{
    using devpiano::exporting::ExportFileType;

    if (! currentTakeCanBeExported || ! devpiano::exporting::canExportTake(currentTake))
    {
        juce::Logger::writeToLog(devpiano::exporting::makeExportLogPrefix(ExportFileType::wav)
                                 + " export skipped: currentTake is empty or not exportable");
        return;
    }

    const auto defaultFile = devpiano::exporting::makeDefaultRecordingExportFile(ExportFileType::wav);

    exportWavChooser = std::make_unique<juce::FileChooser>("Export WAV Recording", defaultFile, "*.wav");
    exportWavChooser->launchAsync(juce::FileBrowserComponent::saveMode
                                      | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();

        if (file == juce::File())
        {
            juce::Logger::writeToLog(devpiano::exporting::makeExportLogPrefix(ExportFileType::wav)
                                     + " export cancelled by user");
            exportWavChooser.reset();
            return;
        }

        const auto options = devpiano::exporting::buildWavExportOptions(currentTake,
                                                                        getPerformanceSettingsFromUi(),
                                                                        getCurrentRuntimeSampleRate(),
                                                                        getCurrentRuntimeBlockSize());

        if (devpiano::exporting::exportTakeAsWavFile(currentTake, file, options))
            juce::Logger::writeToLog(devpiano::exporting::makeExportLogPrefix(ExportFileType::wav)
                                     + " exported: " + file.getFullPathName());
        else
            juce::Logger::writeToLog(devpiano::exporting::makeExportLogPrefix(ExportFileType::wav)
                                     + " export FAILED: " + file.getFullPathName());

        exportWavChooser.reset();
    });
}

void MainComponent::handleImportMidiClicked()
{
    if (currentRecordingState == ControlsPanel::RecordingState::recording)
    {
        juce::Logger::writeToLog("[MIDI Import] import skipped while recording");
        restoreKeyboardFocus();
        return;
    }

    if (currentRecordingState == ControlsPanel::RecordingState::playing)
    {
        const auto stoppedTake = stopInternalPlayback();
        juce::ignoreUnused(stoppedTake);
        currentRecordingState = ControlsPanel::RecordingState::idle;
        controlsPanel.setRecordingState(currentRecordingState);
        juce::Logger::writeToLog("[MIDI Import] stopped current playback before opening importer");
    }

    const auto startDir = [this]() -> juce::File
    {
        if (appSettings.lastMidiImportPath.isNotEmpty())
        {
            auto f = juce::File(appSettings.lastMidiImportPath);
            if (f.exists())
                return f.getParentDirectory();
        }
        return juce::File {};
    }();

    importMidiChooser = std::make_unique<juce::FileChooser>("Import MIDI File", startDir, "*.mid;*.midi");
    importMidiChooser->launchAsync(juce::FileBrowserComponent::openMode
                                       | juce::FileBrowserComponent::canSelectFiles,
                                   [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (!file.exists())
        {
            importMidiChooser.reset();
            return;
        }

        appSettings.lastMidiImportPath = file.getFullPathName();
        saveSettingsSoon();

        const auto sampleRate = getCurrentRuntimeSampleRate();
        auto take = devpiano::recording::importMidiFile(file, sampleRate);

        if (!take.has_value() || take->isEmpty())
        {
            juce::Logger::writeToLog("[MIDI Import] import failed or produced empty take: " + file.getFullPathName());
            importMidiChooser.reset();
            return;
        }

        if (currentRecordingState == ControlsPanel::RecordingState::playing)
        {
            const auto stoppedTake = stopInternalPlayback();
            juce::ignoreUnused(stoppedTake);
            currentRecordingState = ControlsPanel::RecordingState::idle;
            controlsPanel.setRecordingState(currentRecordingState);
            juce::Logger::writeToLog("[MIDI Import] stopped current playback before replacing take");
        }

        currentTake = std::move(*take);
        currentTakeCanBeExported = false;
        controlsPanel.setCanExportTake(false);
        controlsPanel.setHasTake(!currentTake.isEmpty());
        currentRecordingState = ControlsPanel::RecordingState::idle;
        controlsPanel.setRecordingState(currentRecordingState);

        // Immediately start playback so user can hear the imported MIDI
        startInternalPlayback(currentTake);
        currentRecordingState = ControlsPanel::RecordingState::playing;
        controlsPanel.setRecordingState(currentRecordingState);

        juce::Logger::writeToLog("[MIDI Import] imported: " + file.getFullPathName()
                                 + ", events=" + juce::String(static_cast<int>(currentTake.events.size())));

        importMidiChooser.reset();
        restoreKeyboardFocus();
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
