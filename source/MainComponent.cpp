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

#if JUCE_WINDOWS
struct HWND__;
using HWND = HWND__*;
struct HIMC__;
using HIMC = HIMC__*;
extern "C" HIMC __stdcall ImmAssociateContext(HWND, HIMC);
#endif

namespace {
constexpr int preferredMainContentWidth = 1120;
constexpr int preferredMainContentHeight = 760;
constexpr int minimumMainContentWidth = 980;
constexpr int minimumMainContentHeight = 700;
constexpr int maximumMainContentWidth = 3840;
constexpr int maximumMainContentHeight = 2160;

juce::String makeSafeUiText(juce::String text) {
    text = text.replaceCharacters("\r\n\t", "   ");

    constexpr auto maxLen = 1024;
    if (text.length() > maxLen)
        text = text.substring(0, maxLen);

    return text;
}

#if JUCE_WINDOWS
void suppressImeForPeer(juce::ComponentPeer* peer) {
    if (peer == nullptr)
        return;

    if (auto hwnd = static_cast<HWND>(peer->getNativeHandle()))
        ImmAssociateContext(hwnd, nullptr);
}
#endif
}

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
#if DEBUG
    inspector = std::make_unique<melatonin::Inspector>(*this);
#endif

    initialiseUi();
    initialiseFromPreset();

    syncUiFromSettings();
    applyUiStateToAudioEngine();

    initialiseAudioDevice();
    suppressTextInputMethods();

    pluginOperationController->restorePluginStateOnStartup();
    refreshReadOnlyUiStateFromCurrentSnapshot();

    startTimerHz(30);
    restoreKeyboardFocus();
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
    return { minimumMainContentWidth, minimumMainContentHeight, maximumMainContentWidth, maximumMainContentHeight };
}

juce::Rectangle<int> MainComponent::getInitialMainContentBounds() const {
    const auto limits = getMainContentResizeLimits();
    const auto savedWidth = appSettings.mainWindowWidth;
    const auto savedHeight = appSettings.mainWindowHeight;

    const auto width
        = juce::jlimit(limits.getX(), limits.getWidth(), savedWidth > 0 ? savedWidth : preferredMainContentWidth);
    const auto height
        = juce::jlimit(limits.getY(), limits.getHeight(), savedHeight > 0 ? savedHeight : preferredMainContentHeight);

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
    if (isShowing()) {
        suppressTextInputMethods();
        restoreKeyboardFocus();
    }
}

bool MainComponent::keyPressed(const juce::KeyPress& key) {
    if (isKeyboardInputSuppressed())
        return false;

    // F1-F12 preset shortcuts (no modifiers)
    if (!key.getModifiers().isAnyModifierKeyDown()) {
        for (int i = 0; i < 12; ++i) {
            if (key == juce::KeyPress(static_cast<int>(juce::KeyPress::F1Key) + i)) {
                handlePresetShortcut(i);
                return true;
            }
        }
    }

    const auto handled = keyboardMidiMapper.handleKeyPressed(key, audioEngine.getKeyboardState());

    if (handled) {
        getCustomKeyboard().notifyNoteActivity();
        suppressTextInputMethods();
    }

    return handled;
}

bool MainComponent::keyStateChanged(bool isKeyDown) {
    juce::ignoreUnused(isKeyDown);

    if (isKeyboardInputSuppressed())
        return false;

    const auto handled = keyboardMidiMapper.handleKeyStateChanged(audioEngine.getKeyboardState());

    if (handled) {
        getCustomKeyboard().notifyNoteActivity();
        suppressTextInputMethods();
    }

    return handled;
}

bool MainComponent::isKeyboardInputSuppressed() const noexcept {
    // Only suppress keyboard input when focus is on an actively-edited
    // text component. Sliders, buttons, comboboxes and other child
    // components that don't consume text keys should not block piano input.
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent()) {
        if (focused == this || !isParentOf(focused))
            return false;
        if (dynamic_cast<const juce::TextEditor*>(focused) != nullptr)
            return true;
        if (auto* label = dynamic_cast<const juce::Label*>(focused))
            return label->isBeingEdited();
        return false;
    }
    return false;
}

bool MainComponent::shouldTakeKeyboardFocus() const noexcept {
    if (auto* mcm = juce::ModalComponentManager::getInstanceWithoutCreating();
        mcm != nullptr && mcm->getNumModalComponents() > 0)
        return false;

    if (isSettingsWindowOpen())
        return false;

    if (pluginOperationController != nullptr && pluginOperationController->hasEditorWindowOpen())
        return false;

    return true;
}

void MainComponent::focusGained(juce::Component::FocusChangeType cause) {
    juce::AudioAppComponent::focusGained(cause);

    if (!shouldTakeKeyboardFocus())
        return;

    // Windows may have already given us focus via WM_SETFOCUS before grabKeyboardFocus ran,
    // causing takeKeyboardFocus's early-return check to fire. Call grabKeyboardFocus() to
    // synchronize the global state. The early-return in takeKeyboardFocus will safely fire
    // (because currentlyFocusedComponent will already be set after the first call).
    if (juce::Component::getCurrentlyFocusedComponent() != this)
        grabKeyboardFocus();
}

void MainComponent::focusLost(juce::Component::FocusChangeType cause) {
    juce::AudioAppComponent::focusLost(cause);
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
    if (auto* peer = getPeer()) {
        peer->refreshTextInputTarget();

#if JUCE_WINDOWS
        suppressImeForPeer(peer);
#endif
    }
}

void MainComponent::restoreKeyboardFocus() {
    if (!shouldTakeKeyboardFocus())
        return;

    if (isShowing() && juce::Component::getCurrentlyFocusedComponent() != this)
        grabKeyboardFocus();

    suppressTextInputMethods();
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

void MainComponent::showRecentFilesMenu() {
    juce::PopupMenu menu;
    recentFiles.removeNonExistentFiles();

    const auto numFiles = recentFiles.getNumFiles();
    int itemId = 1;

    if (numFiles == 0) {
        menu.addItem(0, TRANS("(no recent files)"), false, false);
    } else {
        for (int i = 0; i < numFiles; ++i) {
            auto file = recentFiles.getFile(i);
            auto name = file.getFileName();
            auto ext = file.getFileExtension().toLowerCase();
            juce::String prefix;
            if (ext == ".devpiano")
                prefix = juce::String::fromUTF8("\xe2\x99\xaa "); // ♪
            else if (ext == ".mid" || ext == ".midi")
                prefix = juce::String::fromUTF8("\xe2\x99\xab "); // ♫
            else
                prefix = "? ";

            menu.addItem(itemId, prefix + name);
            ++itemId;
        }
    }

    int clearId = itemId;
    if (numFiles > 0) {
        menu.addSeparator();
        clearId = itemId;
        menu.addItem(clearId, TRANS("Clear Recent Files"));
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(getRecentFilesButtonScreenBounds()),
                       [safe = juce::Component::SafePointer<MainComponent>(this), numFiles, clearId](int result) {
                           if (safe == nullptr)
                               return;

                           if (result == 0)
                               return;

                           if (result == clearId) {
                               safe->recentFiles.clear();
                               safe->saveRecentFiles();
                               return;
                           }

                           const auto index = result - 1;
                           if (!juce::isPositiveAndBelow(index, numFiles))
                               return;

                           auto file = safe->recentFiles.getFile(index);
                           if (!file.exists())
                               return;

                           auto ext = file.getFileExtension().toLowerCase();
                           if (ext == ".devpiano")
                               safe->recordingSessionController->handleOpenPerformanceFile(file);
                           else if (ext == ".mid" || ext == ".midi")
                               safe->recordingSessionController->handleImportMidiFile(file);
                       });
}

// ═══════════════════════════════════════════════════════════════════════════
// JIVE component accessors (Phase 11d)
// ═══════════════════════════════════════════════════════════════════════════

namespace {
template <typename T> T* jc(jive::GuiItem& root, const juce::String& id) {
    return dynamic_cast<T*>(root.getComponent()->findChildWithID(id));
}
} // namespace

CustomKeyboard& MainComponent::getCustomKeyboard() {
    if (auto* ck = jc<CustomKeyboard>(*jiveRootItem, "custom-keyboard"))
        return *ck;
    auto* vp = jc<juce::Viewport>(*jiveRootItem, "custom-keyboard");
    if (vp != nullptr)
        if (auto* ck = dynamic_cast<CustomKeyboard*>(vp->getViewedComponent()))
            return *ck;
    jassertfalse;
    static CustomKeyboard dummy(audioEngine.getKeyboardState());
    return dummy;
}

void MainComponent::setCustomKeyboardLayout(const devpiano::core::KeyboardLayout& layout) {
    getCustomKeyboard().setKeyboardLayout(layout);
}

void MainComponent::setCustomKeyboardSettings(const devpiano::ui::KeyboardSettings& ks) {
    getCustomKeyboard().setKeyboardSettings(ks);
}

void MainComponent::setKeyboardViewPosition(int midiNote, int pixelOffset) {
    auto* vp = jc<juce::Viewport>(*jiveRootItem, "custom-keyboard");
    if (vp == nullptr)
        return;
    auto* ck = dynamic_cast<CustomKeyboard*>(vp->getViewedComponent());
    if (ck == nullptr)
        return;
    if (pixelOffset >= 0) {
        vp->setViewPosition(pixelOffset, 0);
    } else if (midiNote >= 0 && midiNote <= 127) {
        int whiteCount = 0;
        for (int n = 0; n < midiNote; ++n)
            if (devpiano::ui::isWhiteKey(n))
                ++whiteCount;
        auto x = static_cast<int>(whiteCount * ck->getKeyboardSettings().keyWidth);
        vp->setViewPosition(x, 0);
    }
}

int MainComponent::getKeyboardViewPositionX() const {
    auto* vp = dynamic_cast<juce::Viewport*>(jiveRootItem->getComponent()->findChildWithID("custom-keyboard"));
    return vp != nullptr ? vp->getViewPositionX() : 0;
}

juce::String MainComponent::getPluginPanelPath() const {
    if (auto* ed = jc<juce::TextEditor>(*jiveRootItem, "plugin-path-editor"))
        return ed->getText();
    return {};
}

void MainComponent::setPluginPanelPath(const juce::String& path) {
    if (auto* ed = jc<juce::TextEditor>(*jiveRootItem, "plugin-path-editor"))
        ed->setText(path);
}

juce::String MainComponent::getSelectedPluginName() const {
    if (auto* cb = jc<juce::ComboBox>(*jiveRootItem, "plugin-selector"))
        return cb->getText();
    return {};
}

void MainComponent::setInstrumentFilterVisible(bool visible) {
    // JIVE layout does not currently have an instrument filter ComboBox;
    // when added, wire it here via findChildWithID("instrument-filter").
    juce::ignoreUnused(visible);
}

void MainComponent::updatePluginPanelState(const PluginPanelState& state) {
    if (auto* cb = jc<juce::ComboBox>(*jiveRootItem, "plugin-selector")) {
        cb->clear();
        int id = 1;
        for (const auto& name : state.availablePluginNames)
            cb->addItem(name, id++);
        if (state.preferredSelection.isNotEmpty())
            cb->setText(state.preferredSelection);
        cb->setEnabled(!state.isCurrentlyScanning);
    }
}

void MainComponent::setPluginPanelExpanded(bool expanded) {
    if (auto* area = jc<juce::Component>(*jiveRootItem, "plugin-expanded-area"))
        area->setVisible(expanded);
}

bool MainComponent::isPluginPanelExpanded() const {
    if (auto* area = jc<juce::Component>(*jiveRootItem, "plugin-expanded-area"))
        return area->isVisible();
    return false;
}

void MainComponent::setControlsValues(float gain, float a, float d, float s, float r) {
    auto sk = [&](const juce::String& id, float v) {
        if (auto* sl = jc<juce::Slider>(*jiveRootItem, id))
            sl->setValue(v, juce::dontSendNotification);
    };
    sk("volume-knob", gain);
    sk("attack-knob", a);
    sk("decay-knob", d);
    sk("sustain-knob", s);
    sk("release-knob", r);
    adsrCurve.setParameters(a, d, s, r);
}

float MainComponent::getControlsMasterGain() const {
    auto* s = jc<juce::Slider>(*jiveRootItem, "volume-knob");
    return s ? (float)s->getValue() : 0.8f;
}
float MainComponent::getControlsAttack() const {
    auto* s = jc<juce::Slider>(*jiveRootItem, "attack-knob");
    return s ? (float)s->getValue() : 0.1f;
}
float MainComponent::getControlsDecay() const {
    auto* s = jc<juce::Slider>(*jiveRootItem, "decay-knob");
    return s ? (float)s->getValue() : 0.3f;
}
float MainComponent::getControlsSustain() const {
    auto* s = jc<juce::Slider>(*jiveRootItem, "sustain-knob");
    return s ? (float)s->getValue() : 0.7f;
}
float MainComponent::getControlsRelease() const {
    auto* s = jc<juce::Slider>(*jiveRootItem, "release-knob");
    return s ? (float)s->getValue() : 0.5f;
}

void MainComponent::setControlsPresets(const juce::StringArray& ids, const juce::String& current,
                                       const juce::StringArray& names) {
    if (auto* cb = jc<juce::ComboBox>(*jiveRootItem, "preset-combo")) {
        cb->clear();
        for (int i = 0; i < ids.size() && i < names.size(); ++i)
            cb->addItem(names[i], i + 1);
        if (current.isNotEmpty()) {
            auto idx = ids.indexOf(current);
            if (idx >= 0)
                cb->setSelectedItemIndex(idx, juce::dontSendNotification);
        }
    }
}

juce::String MainComponent::getControlsSelectedPresetId() const {
    return {};
}

void MainComponent::setControlsRecordingState(RecordingControlsState state) {
    auto se = [&](const juce::String& id, bool en) {
        if (auto* btn = jc<juce::Button>(*jiveRootItem, id))
            btn->setEnabled(en);
    };
    se("record-btn", state.state != RecordingState::playing);
    se("play-btn", state.hasTake && state.state != RecordingState::recording);
    se("stop-btn", state.state != RecordingState::idle);
    se("back-btn", state.hasTake);
    se("export-midi-btn", state.canExportMidiTake && state.state == RecordingState::idle);
    se("export-wav-btn", state.canExportWavTake && state.state == RecordingState::idle);
}

void MainComponent::setControlsPlaybackSpeed(double speed) {
    if (auto* sl = jc<juce::Slider>(*jiveRootItem, "speed-knob"))
        sl->setValue(juce::jlimit(0.5, 2.0, speed), juce::dontSendNotification);
}

juce::Rectangle<int> MainComponent::getRecentFilesButtonScreenBounds() const {
    if (auto* btn = jc<juce::Button>(*jiveRootItem, "recent-btn"))
        return btn->getScreenBounds();
    return {};
}

void MainComponent::setStatusPluginName(const juce::String& name) {
    if (auto* lbl = jc<juce::Label>(*jiveRootItem, "plugin-name-label"))
        lbl->setText(name, juce::dontSendNotification);
}
void MainComponent::setStatusAudioInfo(const juce::String& info) {
    if (auto* lbl = jc<juce::Label>(*jiveRootItem, "audio-info-label"))
        lbl->setText(info, juce::dontSendNotification);
}
void MainComponent::setStatusTimeDisplay(const juce::String& time) {
    if (auto* lbl = jc<juce::Label>(*jiveRootItem, "time-label"))
        lbl->setText(time, juce::dontSendNotification);
}
void MainComponent::setStatusMidiActivity(bool /*active*/) {
    if (jiveRootItem == nullptr)
        return;
    if (auto* dot = dynamic_cast<juce::Component*>(jiveRootItem->getComponent()->findChildWithID("midi-dot")))
        dot->repaint();
}
