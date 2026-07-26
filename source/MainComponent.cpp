#include "MainComponent.h"

#include "Diagnostics/Log.h"
#include "Plugin/PluginFlowSupport.h"
#include "Settings/SettingsSerialization.h"
#include "UI/CustomKeyboard.h"
#include "UI/KeyBindingEditDialog.h"
#include "UI/PluginPanelStateBuilder.h"
#include "UI/jive/AppView.h"
#include "UI/jive/CallbackWiring.h"
#include "UI/jive/ComponentFactory.h"
#include "UI/jive/DesignTokens.h"
#include "UI/native/StatusBarMidiDot.h"


namespace {
juce::String makeSafeUiText(juce::String text) {
    text = text.replaceCharacters("\r\n\t", "   ");
    constexpr auto maxLen = 1024;
    if (text.length() > maxLen)
        text = text.substring(0, maxLen);
    return text;
}
} // namespace
MainComponent::MainComponent() {
    devPianoLogger = std::make_unique<devpiano::diagnostics::DevPianoLogger>();
    juce::Logger::setCurrentLogger(devPianoLogger.get());
    settingsStore.load(appSettings);
    audioEngine.setPluginHost(&pluginHost);
    audioEngine.setRecordingEngine(&recordingEngine);

    midiChannelMapper = std::make_unique<devpiano::midi::MidiChannelMapper>(
        appSettings.channelMatrix, appSettings.midiTranspose, appSettings.keySignature);
    keyboardMidiMapper.setChannelMapper(midiChannelMapper.get());
    presetFlowSupport = std::make_unique<devpiano::layout::PresetFlowSupport>(*this);
    recordingSessionController = std::make_unique<devpiano::recording::RecordingSessionController>(
        *this, recordingEngine, audioEngine, appSettings);
    pluginOperationController
        = std::make_unique<devpiano::plugin::PluginOperationController>(*this, pluginHost, appSettings);
    settingsWindowManager = std::make_unique<devpiano::settings::SettingsWindowManager>();
    keyboardInputHandler = std::make_unique<devpiano::input::KeyboardInputHandler>(*this);
#if DEBUG
    inspector = std::make_unique<melatonin::Inspector>(*this);
#endif

    initialiseUi();
    initialiseFromPreset();

    syncUiFromSettings();
    applyUiStateToAudioEngine();

    initialiseAudioDevice();
    keyboardInputHandler->suppressTextInputMethods();

    pluginOperationController->restorePluginStateOnStartup();
    refreshReadOnlyUiStateFromCurrentSnapshot();

    startTimerHz(30);
    keyboardInputHandler->restoreKeyboardFocus();
    applyLanguage(appSettings.languageCode);
}

MainComponent::~MainComponent() {
    setLookAndFeel(nullptr);
    stopTimer();

    juce::Logger::setCurrentLogger(nullptr);
    appSettings.keyboardScrollOffsetX = getKeyboardViewPositionX();
    saveSettingsNow();

    pluginOperationController.reset();
    shutdownAudio();
    if (recordingSessionController != nullptr)
        recordingSessionController->onFileOpened = {};
    audioEngine.setRecordingEngine(nullptr);
    pluginHost.unloadPlugin();
    settingsWindowManager.reset();
}

void MainComponent::initialiseFromPreset() {
    // Load the last active preset, or fall back to built-in default
    if (appSettings.lastActivePresetId.isNotEmpty()) {
        auto file = devpiano::layout::getPresetDirectory().getChildFile(
            devpiano::layout::sanitisePresetFileName(appSettings.lastActivePresetId) + ".devpiano.preset");
        auto loaded = devpiano::layout::loadPreset(file);
        if (loaded.has_value()) {
            presetFlowSupport->applyPresetData(*loaded);
            return;
        }
    }

    // Fallback: built-in default
    presetFlowSupport->applyPresetData(devpiano::layout::makeDefaultPreset());
}

void MainComponent::reconfigureChannelMapper() {
    midiChannelMapper = std::make_unique<devpiano::midi::MidiChannelMapper>(
        appSettings.channelMatrix, appSettings.midiTranspose, appSettings.keySignature);
    keyboardMidiMapper.setChannelMapper(midiChannelMapper.get());
}

void MainComponent::handlePresetShortcut(int index) {
    if (presetFlowSupport != nullptr)
        presetFlowSupport->applyPresetByIndex(index);
}
void MainComponent::initialiseUi() {
    // Design tokens load
    {
        const auto tokensFile
            = juce::File::getCurrentWorkingDirectory().getChildFile("source/UI/jive/design_tokens.json");
        if (auto stream = tokensFile.createInputStream()) {
            auto json = juce::JSON::parse(*stream);
            devpiano::jive::DesignTokens::get().loadFromJSON(json);
        }
    }

    lookAndFeel = std::make_unique<DevPianoLookAndFeel>();
    setLookAndFeel(lookAndFeel.get());
    setWantsKeyboardFocus(true);

    // JIVE interpreter setup
    jiveInterpreter = std::make_unique<::jive::Interpreter>();
    ::devpiano::ui::jive::registerNativeComponents(*jiveInterpreter, audioEngine.getKeyboardState(), adsrCurve);

    auto state = ::jive::makeView<::devpiano::ui::jive::AppView>();
    jiveRootItem = jiveInterpreter->interpret(state);
    auto jiveRoot = jiveRootItem->getComponent();
    addAndMakeVisible(jiveRoot.get());

    // Wire all UI callbacks through JIVE tree
    ::devpiano::ui::jive::wireAllCallbacks(*jiveRootItem, *this);

    // Restore state from settings
    const auto pluginRecovery = getPluginRecoverySettingsWithFallback();
    setPluginPanelPath(makeSafeUiText(pluginRecovery.pluginSearchPath));
    setPluginPanelExpanded(appSettings.pluginPanelExpanded);
    setControlsPlaybackSpeed(1.0);
    recordingEngine.setPlaybackSpeedMultiplier(1.0);
    recentFiles.restoreFromString(appSettings.recentFilesSerialized);

    setBounds(getInitialMainContentBounds());
}
juce::Rectangle<int> MainComponent::getMainContentResizeLimits() {
    return { kMinWidth, kMinHeight, kMaxWidth, kMaxHeight };
}

juce::Rectangle<int> MainComponent::getInitialMainContentBounds() const {
    const auto limits = getMainContentResizeLimits();
    const auto savedWidth = appSettings.mainWindowWidth;
    const auto savedHeight = appSettings.mainWindowHeight;

    const auto width
        = juce::jlimit(limits.getX(), limits.getWidth(), savedWidth > 0 ? savedWidth : kPreferredWidth);
    const auto height
        = juce::jlimit(limits.getY(), limits.getHeight(), savedHeight > 0 ? savedHeight : kPreferredHeight);

    return { 0, 0, width, height };
}

void MainComponent::persistMainContentSize(int width, int height) {
    const auto limits = getMainContentResizeLimits();
    const auto clampedWidth = juce::jlimit(limits.getX(), limits.getWidth(), width);
    const auto clampedHeight = juce::jlimit(limits.getY(), limits.getHeight(), height);

    if (appSettings.mainWindowWidth == clampedWidth && appSettings.mainWindowHeight == clampedHeight)
        return;

    appSettings.mainWindowWidth = clampedWidth;
    appSettings.mainWindowHeight = clampedHeight;
    saveSettingsSoon();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate) {
    audioEngine.prepareToPlay(samplesPerBlockExpected, sampleRate);
    appSettings.applyAudioSettingsView({ .sampleRate = sampleRate, .bufferSize = samplesPerBlockExpected });
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) {
    audioEngine.getNextAudioBlock(bufferToFill);
}

void MainComponent::releaseResources() {
    audioEngine.releaseResources();
}

void MainComponent::timerCallback() {
    recordingSessionController->checkPlaybackEnded();

    // Drain preset-change notifications from playback
    {
        auto changes = recordingEngine.drainPendingPresetChanges();
        for (const auto& change : changes)
            presetFlowSupport->applyPresetByIndex(change.presetId);
    }

    // Aggressively reclaim keyboard focus from child components that don't
    // consume text input. This ensures piano keys always work immediately
    // after slider/button/combobox interaction without requiring every
    // child component to explicitly release focus.
    if (auto* f = juce::Component::getCurrentlyFocusedComponent()) {
        if (f != this && isParentOf(f) && dynamic_cast<const juce::TextEditor*>(f) == nullptr
            && !(dynamic_cast<const juce::Label*>(f) && static_cast<const juce::Label*>(f)->isBeingEdited())) {
            grabKeyboardFocus();
        }
    }
}

void MainComponent::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    const auto centreX = bounds.getCentreX();
    const auto centreY = bounds.getCentreY() * 0.8f;
    const float radius = juce::jmax(bounds.getWidth(), bounds.getHeight()) * 0.75f;

    juce::ColourGradient bgGrad(juce::Colour(0xff202327), centreX, centreY, juce::Colour(0xff161719), centreX + radius,
                                centreY + radius, true);
    g.setGradientFill(bgGrad);
    g.fillRect(bounds);

    // Panel borders handled by JIVE style sheets
}

void MainComponent::resized() {
    if (jiveRootItem != nullptr)
        jiveRootItem->getComponent()->setBounds(getLocalBounds());
}

void MainComponent::paintOverChildren(juce::Graphics& g) {
    if (dropActive) {
        g.setColour(juce::Colour(0x40aaddff));
        g.drawRect(getLocalBounds(), 3);
    }
}

bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files) {
    for (auto& f : files) {
        auto ext = juce::File(f).getFileExtension().toLowerCase();
        if (ext == ".devpiano" || ext == ".mid" || ext == ".midi" || ext == ".devpiano.preset" || ext == ".vst3")
            return true;
    }
    return false;
}

void MainComponent::fileDragEnter(const juce::StringArray&, int, int) {
    dropActive = true;
    repaint();
}

void MainComponent::fileDragExit(const juce::StringArray&) {
    dropActive = false;
    repaint();
}

void MainComponent::filesDropped(const juce::StringArray& files, int, int) {
    dropActive = false;
    repaint();

    for (auto& f : files) {
        const auto file = juce::File(f);
        const auto ext = file.getFileExtension().toLowerCase();

        if (ext == ".devpiano") {
            if (recordingSessionController != nullptr)
                recordingSessionController->handleOpenPerformanceFile(file);
        } else if (ext == ".mid" || ext == ".midi") {
            if (recordingSessionController != nullptr)
                recordingSessionController->handleImportMidiFile(file);
        } else if (ext == ".devpiano.preset") {
            if (presetFlowSupport != nullptr)
                presetFlowSupport->handleImportPresetFile(file);
        } else if (ext == ".vst3") {
            if (pluginOperationController != nullptr)
                pluginOperationController->handleImportVst3File(file);
        }
    }
}
void MainComponent::visibilityChanged() {
    keyboardInputHandler->visibilityChanged();
}

bool MainComponent::keyPressed(const juce::KeyPress& key) {
    return keyboardInputHandler->keyPressed(key);
}

bool MainComponent::keyStateChanged(bool isKeyDown) {
    return keyboardInputHandler->keyStateChanged(isKeyDown);
}

bool MainComponent::isKeyboardInputSuppressed() const noexcept {
    return keyboardInputHandler->isKeyboardInputSuppressed();
}

bool MainComponent::shouldTakeKeyboardFocus() const noexcept {
    return keyboardInputHandler->shouldTakeKeyboardFocus();
}

void MainComponent::focusGained(juce::Component::FocusChangeType cause) {
    keyboardInputHandler->focusGained(cause);
}

void MainComponent::focusLost(juce::Component::FocusChangeType cause) {
    keyboardInputHandler->focusLost(cause);
}

SettingsModel::PerformanceSettingsView MainComponent::getPerformanceSettingsFromUi() const {
    return { .masterGain = getControlsMasterGain(),
             .adsrAttack = getControlsAttack(),
             .adsrDecay = getControlsDecay(),
             .adsrSustain = getControlsSustain(),
             .adsrRelease = getControlsRelease() };
}

juce::String MainComponent::getLastPluginNameForRecoveryStateFromUi() const {
    if (pluginHost.hasLoadedPlugin())
        return pluginHost.getCurrentPluginName();

    auto selected = getSelectedPluginName().trim();
    if (selected.isNotEmpty())
        return selected;

    // Fallback: during early startup the UI may not be populated yet;
    // preserve the model's persisted value so saveSettingsSoon() doesn't clear it.
    return appSettings.lastPluginName;
}

SettingsModel::PluginRecoverySettingsView MainComponent::getPluginRecoverySettingsFromUi() const {
    return devpiano::plugin::makePluginRecoverySettings(getPluginPanelPath().trim(),
                                                        getLastPluginNameForRecoveryStateFromUi());
}

SettingsModel::PluginRecoverySettingsView MainComponent::getPluginRecoverySettingsWithFallback() const {
    return devpiano::plugin::withPluginRecoveryPathFallback(appSettings.getPluginRecoverySettingsView(),
                                                            pluginHost.getDefaultVst3SearchPath());
}

void MainComponent::applyPerformanceSettingsToUi(const SettingsModel::PerformanceSettingsView& performance) {
    setControlsValues(performance.masterGain, performance.adsrAttack, performance.adsrDecay, performance.adsrSustain,
                      performance.adsrRelease);
}

void MainComponent::applyPerformanceSettingsToAudioEngine(const SettingsModel::PerformanceSettingsView& performance) {
    audioEngine.setMasterGain(performance.masterGain);
    audioEngine.setAdsr(performance.adsrAttack, performance.adsrDecay, performance.adsrSustain,
                        performance.adsrRelease);
}

void MainComponent::applyPluginRecoverySettings(const SettingsModel::PluginRecoverySettingsView& pluginRecovery) {
    appSettings.applyPluginRecoverySettingsView(pluginRecovery);
}

void MainComponent::handlePerformanceUiChanged() {
    applyPerformanceSettingsToAudioEngine(getPerformanceSettingsFromUi());
    saveSettingsSoon();
}

void MainComponent::applyUiStateToAudioEngine() {
    applyPerformanceSettingsToAudioEngine(getPerformanceSettingsFromUi());
}

void MainComponent::syncUiFromSettings() {
    applyPerformanceSettingsToUi(appSettings.getPerformanceSettingsView());

    if (presetFlowSupport != nullptr) {
        setControlsPresets(presetFlowSupport->getPresetIds(), presetFlowSupport->getCurrentPresetId(),
                           presetFlowSupport->getPresetDisplayNames());
    }

    setCustomKeyboardLayout(keyboardMidiMapper.getLayout());
    {
        auto kbs = appSettings.getKeyboardDisplaySettingsView();
        devpiano::ui::KeyboardSettings ks;
        ks.colourMode = kbs.colourMode;
        ks.noteDisplay = kbs.noteDisplay;
        ks.fadeSpeed = kbs.fadeSpeed;
        ks.keySignature = appSettings.keySignature;
        ks.customKeyLabels = kbs.customKeyLabels;
        ks.customKeyColours = kbs.customKeyColours;
        getCustomKeyboard().setKeyboardSettings(ks);
    }
    // Restore keyboard scroll position (after layout is known); -1 sentinel = unset
    if (appSettings.keyboardScrollOffsetX >= 0)
        setKeyboardViewPosition(-1, appSettings.keyboardScrollOffsetX);
    else
        setKeyboardViewPosition(24); // default: align note 24 (C1) at left edge
}

void MainComponent::syncSettingsFromUi() {
    appSettings.applyPerformanceSettingsView(getPerformanceSettingsFromUi());

    applyPluginRecoverySettings(getPluginRecoverySettingsFromUi());
}

void MainComponent::suppressTextInputMethods() {
    keyboardInputHandler->suppressTextInputMethods();
}

void MainComponent::restoreKeyboardFocus() {
    keyboardInputHandler->restoreKeyboardFocus();
}

void MainComponent::initialiseAudioDevice() {
    const auto audioSettings = appSettings.getAudioSettingsView();
    const auto* savedState = (audioSettings.hasSerializedDeviceState && appSettings.audioDeviceState != nullptr)
        ? appSettings.audioDeviceState.get()
        : nullptr;

    setAudioChannels(0, 2, savedState);

    if (deviceManager.getCurrentAudioDevice() == nullptr)
        DP_LOG_ERROR("[AudioDevice] initialiseAudioDevice: no device available after initialization");

    captureAudioDeviceState();
    logCurrentAudioDeviceDiagnostics("initialiseAudioDevice");
}

void MainComponent::captureAudioDeviceState() {
    if (auto xml = deviceManager.createStateXml())
        appSettings.setSerializedAudioDeviceState(std::move(xml));
}

void MainComponent::prepareForAudioDeviceRebuild() {
    captureAudioDeviceState();
    if (pluginOperationController)
        pluginOperationController->closePluginEditorWindow();
    shutdownAudio();
}

void MainComponent::finishAudioDeviceRebuild() {
    initialiseAudioDevice();
    restoreKeyboardFocus();
}

void MainComponent::collectCurrentSettingsState() {
    syncSettingsFromUi();
    captureAudioDeviceState();
}

void MainComponent::saveSettingsNow() {
    collectCurrentSettingsState();
    settingsStore.save(appSettings);
    logCurrentAudioDeviceDiagnostics("saveSettingsNow");
}

void MainComponent::saveSettingsSoon() {
    collectCurrentSettingsState();
    settingsStore.scheduleSave(appSettings);
}

void MainComponent::showSettingsDialog() {
    settingsWindowManager->showFor(*this);
}

bool MainComponent::isSettingsWindowOpen() const {
    return settingsWindowManager != nullptr && settingsWindowManager->isOpen();
}

void MainComponent::renderReadOnlyUiState(const devpiano::core::AppState& appState) {
    updatePluginPanelState(
        buildPluginPanelState(pluginHost, appState.plugin.lastPluginName, appState.plugin.isEditorOpen));
}

void MainComponent::refreshReadOnlyUiStateFromCurrentSnapshot() {
    renderReadOnlyUiState(buildCurrentAppStateSnapshot());
}

void MainComponent::refreshPluginUiState() {
    renderReadOnlyUiState(buildCurrentAppStateSnapshot());
}

void MainComponent::finishPluginUiAction(bool shouldSaveSettings) {
    if (shouldSaveSettings)
        saveSettingsSoon();

    refreshReadOnlyUiStateFromCurrentSnapshot();
    restoreKeyboardFocus();
}

void MainComponent::logCurrentAudioDeviceDiagnostics(const juce::String& context) const {
    const auto diagnostics
        = devpiano::audio::buildAudioDeviceDiagnostics(appSettings.audioDeviceState.get(), deviceManager);
    DP_LOG_INFO("[AudioDevice] " + context + "\n" + diagnostics.detailedSummary);
}

devpiano::core::AppState MainComponent::buildCurrentAppStateSnapshot() const {
    return devpiano::core::buildCurrentAppStateSnapshot(
        appSettings, deviceManager, pluginHost,
        pluginOperationController != nullptr && pluginOperationController->hasEditorWindowOpen(), keyboardMidiMapper);
}

void MainComponent::applyLanguage(const juce::String& code) {
    devpiano::locale::activate(devpiano::locale::codeToLanguage(code));
    refreshAllTexts();
}

void MainComponent::refreshAllTexts() {
    // Refresh all JIVE-managed label texts for locale changes
    setStatusPluginName(getSelectedPluginName());
    // TODO: add refresh for remaining labels when locale system is integrated with JIVE
}

double MainComponent::getCurrentRuntimeSampleRate() const {
    if (auto* device = deviceManager.getCurrentAudioDevice()) {
        const auto rate = device->getCurrentSampleRate();
        if (rate > 0.0)
            return rate;
    }

    return appSettings.getAudioSettingsView().sampleRate;
}

int MainComponent::getCurrentRuntimeBlockSize() const {
    if (auto* device = deviceManager.getCurrentAudioDevice()) {
        const auto size = device->getCurrentBufferSizeSamples();
        if (size > 0)
            return size;
    }

    return appSettings.getAudioSettingsView().bufferSize;
}

MainComponent::RuntimeAudioConfig MainComponent::getCurrentRuntimeAudioConfig() const {
    return { .sampleRate = getCurrentRuntimeSampleRate(), .blockSize = getCurrentRuntimeBlockSize() };
}

void MainComponent::runPluginActionWithAudioDeviceRebuild(
    const std::function<void(const RuntimeAudioConfig&)>& action) {
    struct AudioDeviceRebuildGuard final {
        explicit AudioDeviceRebuildGuard(MainComponent& ownerIn)
            : owner(ownerIn) {
        }
        ~AudioDeviceRebuildGuard() {
            owner.finishAudioDeviceRebuild();
        }

        MainComponent& owner;
    };

    const auto runtimeAudioConfig = getCurrentRuntimeAudioConfig();

    prepareForAudioDeviceRebuild();
    const AudioDeviceRebuildGuard rebuildGuard(*this);
    action(runtimeAudioConfig);
}

void MainComponent::runPluginActionWithAudioDeviceRebuild(const std::function<void()>& action) {
    runPluginActionWithAudioDeviceRebuild([&action](const RuntimeAudioConfig&) { action(); });
}

void MainComponent::saveRecentFiles() {
    appSettings.recentFilesSerialized = recentFiles.toString();
    saveSettingsSoon();
}

#include "RecentFilesMenuHelper.cpp"

#include "MainComponentJiveAccessors.cpp"
