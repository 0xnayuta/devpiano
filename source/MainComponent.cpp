#include "MainComponent.h"

#include "Diagnostics/Log.h"
#include "Plugin/PluginFlowSupport.h"
#include "Settings/SettingsSerialization.h"
#include "UI/CustomKeyboard.h"
#include "UI/KeyBindingEditDialog.h"
#include "UI/PluginPanelStateBuilder.h"
#include "UI/jive/DesignTokens.h"
#include "UI/native/AdsrCurveComponent.h"
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

// -- Gear icon path (simple cog / settings icon) --
std::unique_ptr<juce::Drawable> createGearIcon(juce::Colour colour) {
    juce::Path p;
    // Outer ring + four teeth as a single non-zero-winding composite
    p.addEllipse(-8, -8, 16, 16);
    for (int i = 0; i < 4; ++i) {
        auto angle = juce::MathConstants<float>::halfPi * static_cast<float>(i) - juce::MathConstants<float>::pi / 4.0f;
        auto cx = 9.0f * std::cos(angle);
        auto cy = 9.0f * std::sin(angle);
        p.addRectangle(cx - 2.5f, cy - 2.5f, 5.0f, 5.0f);
    }
    p.setUsingNonZeroWinding(true);
    auto drawable = std::make_unique<juce::DrawablePath>();
    drawable->setPath(p);
    drawable->setFill(colour);
    return drawable;
}

// -- Transport button icon paths --
std::unique_ptr<juce::Drawable> createRecordIcon() {
    juce::Path p;
    p.addEllipse(-7, -7, 14, 14);
    auto d = std::make_unique<juce::DrawablePath>();
    d->setPath(p);
    d->setFill(devpiano::jive::DesignTokens::get().recordActive());
    return d;
}
std::unique_ptr<juce::Drawable> createPlayIcon() {
    juce::Path p;
    p.addTriangle(-5.0f, -7.0f, -5.0f, 7.0f, 7.0f, 0.0f);
    auto d = std::make_unique<juce::DrawablePath>();
    d->setPath(p);
    d->setFill(devpiano::jive::DesignTokens::get().playActive());
    return d;
}
std::unique_ptr<juce::Drawable> createStopIcon() {
    juce::Path p;
    p.addRectangle(-6, -6, 12, 12);
    auto d = std::make_unique<juce::DrawablePath>();
    d->setPath(p);
    d->setFill(devpiano::jive::DesignTokens::get().textPrimary());
    return d;
}
std::unique_ptr<juce::Drawable> createBackIcon() {
    juce::Path p;
    p.addTriangle(-7.0f, -6.0f, -7.0f, 6.0f, 1.0f, 0.0f);
    p.addTriangle(-1.0f, -6.0f, -1.0f, 6.0f, 7.0f, 0.0f);
    auto d = std::make_unique<juce::DrawablePath>();
    d->setPath(p);
    d->setFill(devpiano::jive::DesignTokens::get().textSecondary());
    return d;
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

MainComponent::MainComponent()
    : keyboardPanel(audioEngine.getKeyboardState()) {
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
    appSettings.keyboardScrollOffsetX = keyboardPanel.getViewPositionX();
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
    // 加载设计 token（单一配色真相源）— 必须在构造 LookAndFeel 之前
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

    // ── JIVE header bar ──
    {
        // Global style rules (loaded once; re-loading is idempotent).
        const auto styleFile
            = juce::File::getCurrentWorkingDirectory().getChildFile("source/UI/jive/style_sheets.json");
        if (auto stream = styleFile.createInputStream()) {
            auto json = juce::JSON::parse(*stream);
            devpiano::ui::jive::StyleCatalog::get().loadFromJSON(json);
        }

        jiveInterpreter = std::make_unique<::jive::Interpreter>();
        jiveInterpreter->getComponentFactory().set("SettingsButton", [] {
            auto btn = std::make_unique<juce::DrawableButton>("settings", juce::DrawableButton::ImageFitted);
            btn->setImages(createGearIcon(devpiano::jive::DesignTokens::get().textSecondary()).get(),
                           createGearIcon(devpiano::jive::DesignTokens::get().primary()).get(), nullptr);
            return btn;
        });

        auto headerTree = devpiano::ui::jive::makeHeaderTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(headerTree);
        jiveHeaderItem = jiveInterpreter->interpret(headerTree);
        if (jiveHeaderItem != nullptr) {
            addAndMakeVisible(jiveHeaderItem->getComponent().get());
            if (auto* settingsItem = jive::findItemWithID(*jiveHeaderItem, "settings-btn"))
                if (auto* settingsBtn = dynamic_cast<juce::Button*>(settingsItem->getComponent().get()))
                    settingsBtn->onClick = [this] { showSettingsDialog(); };
        }
    }

    // ── JIVE plugin panel ──
    {
        jiveInterpreter->getComponentFactory().set("PathEditor", [] {
            auto editor = std::make_unique<juce::TextEditor>();
            editor->setMultiLine(false);
            editor->setReturnKeyStartsNewLine(false);
            return editor;
        });
        jiveInterpreter->getComponentFactory().set("ListEditor", [] {
            auto editor = std::make_unique<juce::TextEditor>();
            editor->setMultiLine(true);
            editor->setReadOnly(true);
            editor->setScrollbarsShown(true);
            editor->setCaretVisible(false);
            editor->setPopupMenuEnabled(true);
            editor->setWantsKeyboardFocus(false);
            editor->setMouseClickGrabsKeyboardFocus(false);
            return editor;
        });

        auto pluginTree = devpiano::ui::jive::makePluginPanelTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(pluginTree);
        jivePluginPanelItem = jiveInterpreter->interpret(pluginTree);
        if (jivePluginPanelItem != nullptr) {
            addAndMakeVisible(jivePluginPanelItem->getComponent().get());

            const auto wireButton = [this](const char* id, const std::function<void()>& action) {
                if (auto* item = jive::findItemWithID(*jivePluginPanelItem, id))
                    if (auto* btn = dynamic_cast<juce::Button*>(item->getComponent().get()))
                        btn->onClick = action;
            };
            wireButton("load-btn", [this] { pluginOperationController->loadSelectedPlugin(); });
            wireButton("unload-btn", [this] { pluginOperationController->unloadCurrentPlugin(); });
            wireButton("editor-btn", [this] { pluginOperationController->togglePluginEditor(); });
            wireButton("toggle-btn", [this] { setPluginPanelExpanded(!appSettings.pluginPanelExpanded); });
            wireButton("scan-btn", [this] { pluginOperationController->scanPlugins(); });
            wireButton("browse-btn", [this] { showPluginBrowseDialog(); });

            if (auto* item = jive::findItemWithID(*jivePluginPanelItem, "plugin-selector"))
                if (auto* combo = dynamic_cast<juce::ComboBox*>(item->getComponent().get())) {
                    combo->setTextWhenNothingSelected(TRANS("Select a scanned plugin..."));
                    combo->setWantsKeyboardFocus(false);
                    combo->onChange = [this, combo] {
                        if (combo->getSelectedItemIndex() >= 0)
                            pluginOperationController->loadSelectedPlugin();
                    };
                }

            if (auto* item = jive::findItemWithID(*jivePluginPanelItem, "plugin-filter-combo"))
                if (auto* combo = dynamic_cast<juce::ComboBox*>(item->getComponent().get())) {
                    combo->setWantsKeyboardFocus(false);
                    combo->onChange = [this] { refreshPluginUiState(); };
                }

            if (auto* item = jive::findItemWithID(*jivePluginPanelItem, "plugin-path-editor"))
                if (auto* editor = dynamic_cast<juce::TextEditor*>(item->getComponent().get()))
                    editor->onReturnKey = [this] { pluginOperationController->scanPlugins(); };
        }
    }

    const auto pluginRecovery = getPluginRecoverySettingsWithFallback();
    setPluginPathText(makeSafeUiText(pluginRecovery.pluginSearchPath));

    setPluginPanelExpanded(appSettings.pluginPanelExpanded);

    // ── JIVE controls panel ──
    {
        jiveInterpreter->getComponentFactory().set("DevKnob", [] {
            auto slider = std::make_unique<juce::Slider>();
            slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 44, 16);
            slider->setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f,
                                        true);
            return slider;
        });
        jiveInterpreter->getComponentFactory().set("AdsrCurve", [] { return std::make_unique<AdsrCurveComponent>(); });
        const auto registerIconButton
            = [this](const char* type, const std::unique_ptr<juce::Drawable>& image, const juce::String& tooltip) {
                  jiveInterpreter->getComponentFactory().set(type, [&image, tooltip] {
                      auto btn = std::make_unique<juce::DrawableButton>(tooltip, juce::DrawableButton::ImageFitted);
                      btn->setImages(image.get());
                      btn->setTooltip(tooltip);
                      return btn;
                  });
              };
        // Icons are owned by the factory closures for the app lifetime.
        static const auto recordIcon = createRecordIcon();
        static const auto playIcon = createPlayIcon();
        static const auto stopIcon = createStopIcon();
        static const auto backIcon = createBackIcon();
        registerIconButton("RecordButton", recordIcon, TRANS("Record"));
        registerIconButton("PlayButton", playIcon, TRANS("Play"));
        registerIconButton("StopButton", stopIcon, TRANS("Stop"));
        registerIconButton("BackButton", backIcon, TRANS("Back to Start"));

        auto controlsTree = devpiano::ui::jive::makeControlsPanelTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(controlsTree);
        jiveControlsPanelItem = jiveInterpreter->interpret(controlsTree);
        if (jiveControlsPanelItem != nullptr) {
            addAndMakeVisible(jiveControlsPanelItem->getComponent().get());

            auto* adsrCurve = []() -> AdsrCurveComponent* {
                // resolved after wiring below
                return nullptr;
            }();

            const auto findItem = [this](const char* id) -> ::jive::GuiItem* {
                return jive::findItemWithID(*jiveControlsPanelItem, id);
            };
            const auto findSlider = [&findItem](const char* id) -> juce::Slider* {
                if (auto* item = findItem(id))
                    return dynamic_cast<juce::Slider*>(item->getComponent().get());
                return nullptr;
            };
            const auto findCombo = [&findItem](const char* id) -> juce::ComboBox* {
                if (auto* item = findItem(id))
                    return dynamic_cast<juce::ComboBox*>(item->getComponent().get());
                return nullptr;
            };
            const auto findButton = [&findItem](const char* id) -> juce::Button* {
                if (auto* item = findItem(id))
                    return dynamic_cast<juce::Button*>(item->getComponent().get());
                return nullptr;
            };
            const auto setButtonEnabled = [this](const char* id, bool enabled) {
                if (auto* item = jive::findItemWithID(*jiveControlsPanelItem, id))
                    item->state.setProperty("enabled", enabled, nullptr);
            };

            if (auto* item = jive::findItemWithID(*jiveControlsPanelItem, "adsr-curve"))
                adsrCurve = dynamic_cast<AdsrCurveComponent*>(item->getComponent().get());

            const auto wireKnob = [&findSlider](const char* id, double min, double max, double interval,
                                                const std::function<juce::String(double)>& formatter,
                                                const std::function<void()>& onChanged) {
                if (auto* slider = findSlider(id)) {
                    slider->setRange(min, max, interval);
                    slider->textFromValueFunction = formatter;
                    slider->onValueChange = [onChanged] { onChanged(); };
                }
            };

            wireKnob(
                "volume-knob", 0.0, 1.0, 0.01, [](double v) { return juce::String(v, 2); },
                [this] { handlePerformanceUiChanged(); });
            const auto wireAdsrKnob
                = [this, curve = adsrCurve, &wireKnob](const char* id, double min, double max, double interval,
                                                       const std::function<juce::String(double)>& formatter) {
                      wireKnob(id, min, max, interval, formatter, [this, curve] {
                          if (curve != nullptr)
                              curve->setParameters(getAttack(), getDecay(), getSustain(), getRelease());
                          handlePerformanceUiChanged();
                      });
                  };
            wireAdsrKnob("attack-knob", 0.001, 2.0, 0.001, [](double v) { return juce::String(v, 3) + "s"; });
            wireAdsrKnob("decay-knob", 0.001, 2.0, 0.001, [](double v) { return juce::String(v, 3) + "s"; });
            wireAdsrKnob("sustain-knob", 0.0, 1.0, 0.01, [](double v) { return juce::String(v, 2); });
            wireAdsrKnob("release-knob", 0.001, 3.0, 0.001, [](double v) { return juce::String(v, 3) + "s"; });
            wireKnob(
                "speed-knob", 0.5, 2.0, 0.25, [](double v) { return juce::String(v, 1) + "x"; },
                [this] { recordingSessionController->handlePlaybackSpeedChange(getControlsPlaybackSpeed()); });

            const auto wireButtonAction = [this, &findButton](const char* id, const std::function<void()>& action) {
                if (auto* btn = findButton(id))
                    btn->onClick = action;
            };
            wireButtonAction("record-btn", [this] { recordingSessionController->handleRecordClicked(); });
            wireButtonAction("play-btn", [this] { recordingSessionController->handlePlayClicked(); });
            wireButtonAction("stop-btn", [this] { recordingSessionController->handleStopClicked(); });
            wireButtonAction("back-btn", [this] { recordingSessionController->handleBackToStartClicked(); });
            wireButtonAction("export-midi-btn", [this] { recordingSessionController->handleExportMidiClicked(); });
            wireButtonAction("export-wav-btn", [this] { recordingSessionController->handleExportWavClicked(); });
            wireButtonAction("import-midi-btn", [this] { recordingSessionController->handleImportMidiClicked(); });
            wireButtonAction("save-perf-btn", [this] { recordingSessionController->handleSavePerformanceClicked(); });
            wireButtonAction("open-perf-btn", [this] { recordingSessionController->handleOpenPerformanceClicked(); });
            wireButtonAction("song-info-btn", [this] { recordingSessionController->handleSongInfoClicked(); });
            wireButtonAction("recent-btn", [this] { showRecentFilesMenu(); });
            wireButtonAction("save-preset-btn", [this] { presetFlowSupport->handleSaveAsNewPreset(); });
            wireButtonAction("rename-preset-btn", [this] { presetFlowSupport->handleRenamePreset(); });
            wireButtonAction("delete-preset-btn", [this] { presetFlowSupport->handleDeletePreset(); });

            if (auto* combo = findCombo("preset-combo")) {
                combo->setTextWhenNothingSelected("Default");
                combo->onChange = [this, combo] {
                    const auto selectedId = combo->getSelectedId();
                    if (selectedId <= 0 || !juce::isPositiveAndBelow(selectedId - 1, availablePresetIds.size()))
                        return;
                    presetFlowSupport->applyPresetById(availablePresetIds[selectedId - 1]);
                    updateControlsPresetActionButtons();
                };
            }

            setRecordingControlsState({});
        }
    }

    recordingSessionController->onFileOpened = [this](const juce::File& file) {
        recentFiles.addFile(file);
        saveRecentFiles();
    };

    // Restore recently opened files list from settings.
    recentFiles.restoreFromString(appSettings.recentFilesSerialized);

    // Initialize playback speed to 1.0x (never persisted — default on every launch).
    setControlsPlaybackSpeed(1.0);
    recordingEngine.setPlaybackSpeedMultiplier(1.0);

    addAndMakeVisible(keyboardPanel);

    // ── JIVE status bar ──
    {
        jiveInterpreter->getComponentFactory().set("StatusBarMidiDot",
                                                   [] { return std::make_unique<StatusBarMidiDot>(); });

        auto statusTree = devpiano::ui::jive::makeStatusBarTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(statusTree);
        jiveStatusBarItem = jiveInterpreter->interpret(statusTree);
        if (jiveStatusBarItem != nullptr)
            addAndMakeVisible(jiveStatusBarItem->getComponent().get());
    }

    // Wire CustomKeyboard mouse interaction → sound (with MIDI matrix)
    auto& customKeyboard = keyboardPanel.getCustomKeyboard();
    customKeyboard.onNoteOn = [this](int midiNote, int sourceChannel) {
        if (midiChannelMapper != nullptr)
            midiChannelMapper->sendNoteOn(sourceChannel, midiNote, 1.0f, audioEngine.getKeyboardState());
        else
            audioEngine.getKeyboardState().noteOn(1, midiNote, 1.0f);
        suppressTextInputMethods();
    };
    customKeyboard.onNoteOff = [this](int midiNote, int sourceChannel) {
        if (midiChannelMapper != nullptr)
            midiChannelMapper->sendNoteOff(sourceChannel, midiNote, 1.0f, audioEngine.getKeyboardState());
        else
            audioEngine.getKeyboardState().noteOff(1, midiNote, 1.0f);
    };
    customKeyboard.onBindingEditRequested = [this](int midiNote) {
        // Find existing binding for this note
        const auto& layout = keyboardMidiMapper.getLayout();
        auto noteName = devpiano::ui::getNoteDisplayName(midiNote, devpiano::ui::NoteDisplayMode::noteName);

        const devpiano::core::KeyBinding* existingBinding = nullptr;
        for (const auto& binding : layout.bindings) {
            if (binding.action.type == devpiano::core::KeyActionType::note && binding.action.midiNote == midiNote) {
                existingBinding = &binding;
                break;
            }
        }

        // Read current per-key custom label and colour
        auto currentLabel = appSettings.customKeyLabels[static_cast<std::size_t>(midiNote)];
        auto currentColour = appSettings.customKeyColours[static_cast<std::size_t>(midiNote)];

        KeyBindingEditDialog::launch(
            midiNote, noteName, existingBinding, currentLabel, currentColour,
            [this, midiNote](KeyBindingEditResult result) {
                // Update custom label and colour if changed
                if (result.labelChanged)
                    appSettings.customKeyLabels[static_cast<std::size_t>(midiNote)] = result.customLabel;
                if (result.colourChanged)
                    appSettings.customKeyColours[static_cast<std::size_t>(midiNote)] = result.customColour;

                // Update binding if changed
                if (result.binding.has_value()) {
                    auto updatedLayout = keyboardMidiMapper.getLayout();

                    if (result.binding->keyCode < 0) {
                        // Unbind request: remove all bindings for this note
                        updatedLayout.bindings.erase(
                            std::remove_if(updatedLayout.bindings.begin(), updatedLayout.bindings.end(),
                                           [note = result.binding->action.midiNote](const auto& b) {
                                               return b.action.type == devpiano::core::KeyActionType::note
                                                   && b.action.midiNote == note;
                                           }),
                            updatedLayout.bindings.end());
                    } else {
                        // Update the binding in-place
                        for (auto& b : updatedLayout.bindings)
                            if (b.keyCode == result.binding->keyCode) {
                                b = *result.binding;
                                break;
                            }
                    }

                    keyboardMidiMapper.setLayout(updatedLayout);
                    keyboardPanel.setKeyboardLayout(updatedLayout);
                }

                // Refresh keyboard rendering (picks up label/colour changes)
                syncUiFromSettings();
                saveSettingsSoon();

                // Persist binding changes to the current preset file (if user preset)
                if (presetFlowSupport != nullptr) {
                    auto currentId = presetFlowSupport->getCurrentPresetId();
                    if (currentId.isNotEmpty()) {
                        auto updatedPreset = presetFlowSupport->captureCurrentState(currentId);
                        auto presetFile = devpiano::layout::getPresetDirectory().getChildFile(
                            devpiano::layout::sanitisePresetFileName(currentId) + ".devpiano.preset");
                        if (!devpiano::layout::savePreset(updatedPreset, presetFile))
                            DP_LOG_WARN("[Preset] failed to auto-save after binding edit: " + currentId);
                    }
                }
            },
            this);
    };
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

    // ── Panel 1px micro highlight top borders & dark outlines ──
    const auto drawPanelBorder = [&g](const juce::Rectangle<int>& b) {
        if (b.isEmpty())
            return;
        auto fb = b.toFloat();

        // Top 1px specular highlight
        g.setColour(juce::Colour(0x20ffffff));
        g.drawHorizontalLine(b.getY(), fb.getX(), fb.getRight());

        // 1px micro dark outline
        g.setColour(juce::Colour(0x28000000));
        g.drawRect(fb, 1.0f);
    };

    const auto headerBounds
        = (jiveHeaderItem != nullptr) ? jiveHeaderItem->getComponent()->getBounds() : juce::Rectangle<int> {};
    drawPanelBorder(headerBounds);
    const auto pluginBounds
        = (jivePluginPanelItem != nullptr) ? jivePluginPanelItem->getComponent()->getBounds() : juce::Rectangle<int> {};
    drawPanelBorder(pluginBounds);
    const auto controlsBounds = (jiveControlsPanelItem != nullptr) ? jiveControlsPanelItem->getComponent()->getBounds()
                                                                   : juce::Rectangle<int> {};
    drawPanelBorder(controlsBounds);
    drawPanelBorder(keyboardPanel.getBounds());
}

void MainComponent::resized() {
    auto area = getLocalBounds();
    if (jiveStatusBarItem != nullptr)
        jiveStatusBarItem->getComponent()->setBounds(area.removeFromBottom(22));
    else
        area.removeFromBottom(22);

    auto content = area.reduced(16);
    if (jiveHeaderItem != nullptr)
        jiveHeaderItem->getComponent()->setBounds(content.removeFromTop(36));
    else
        content.removeFromTop(36);
    content.removeFromTop(10);

    if (jivePluginPanelItem != nullptr)
        jivePluginPanelItem->getComponent()->setBounds(
            content.removeFromTop(appSettings.pluginPanelExpanded ? 160 : 40));
    else
        content.removeFromTop(40);
    content.removeFromTop(12);

    // ── Dynamic allocation between ControlsPanel and KeyboardPanel ──
    constexpr int controlsBase = 174;
    constexpr int keyboardMin = 90;
    constexpr int keyboardMax = 200;
    constexpr int baseline = controlsBase + keyboardMin;
    constexpr float keyboardRatio = 0.6f;

    int alloc = content.getHeight() - 8; // gap between controls & keyboard

    int keyboardHeight = keyboardMin;
    if (alloc > baseline) {
        int extra = alloc - baseline;
        keyboardHeight = keyboardMin + static_cast<int>(static_cast<float>(extra) * keyboardRatio);
        keyboardHeight = juce::jmin(keyboardHeight, keyboardMax);
    } else {
        keyboardHeight = juce::jmax(keyboardMin, alloc - controlsBase);
    }

    int controlsHeight = alloc - keyboardHeight;

    if (jiveControlsPanelItem != nullptr)
        jiveControlsPanelItem->getComponent()->setBounds(content.removeFromTop(controlsHeight));
    else
        content.removeFromTop(controlsHeight);
    content.removeFromTop(8);
    keyboardPanel.setBounds(content.removeFromTop(keyboardHeight));
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
        keyboardPanel.getCustomKeyboard().notifyNoteActivity();
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
        keyboardPanel.getCustomKeyboard().notifyNoteActivity();
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
    return { .masterGain = getMasterGain(),
             .adsrAttack = getAttack(),
             .adsrDecay = getDecay(),
             .adsrSustain = getSustain(),
             .adsrRelease = getRelease() };
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
    return devpiano::plugin::makePluginRecoverySettings(getPluginPathText().trim(),
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

    keyboardPanel.setKeyboardLayout(keyboardMidiMapper.getLayout());
    {
        auto kbs = appSettings.getKeyboardDisplaySettingsView();
        devpiano::ui::KeyboardSettings ks;
        ks.colourMode = kbs.colourMode;
        ks.noteDisplay = kbs.noteDisplay;
        ks.fadeSpeed = kbs.fadeSpeed;
        ks.keySignature = appSettings.keySignature;
        ks.customKeyLabels = kbs.customKeyLabels;
        ks.customKeyColours = kbs.customKeyColours;
        keyboardPanel.getCustomKeyboard().setKeyboardSettings(ks);
    }
    // Restore keyboard scroll position (after layout is known); -1 sentinel = unset
    if (appSettings.keyboardScrollOffsetX >= 0)
        keyboardPanel.setViewPosition(-1, appSettings.keyboardScrollOffsetX);
    else
        keyboardPanel.setViewPosition(24); // default: align note 24 (C1) at left edge
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

// ── JIVE plugin panel accessors ────────────────────────────────────────────

void MainComponent::setPluginPathText(const juce::String& text) {
    if (jivePluginPanelItem == nullptr)
        return;
    if (auto* item = jive::findItemWithID(*jivePluginPanelItem, "plugin-path-editor"))
        if (auto* editor = dynamic_cast<juce::TextEditor*>(item->getComponent().get()))
            editor->setText(text, juce::dontSendNotification);
}

juce::String MainComponent::getPluginPathText() const {
    if (jivePluginPanelItem == nullptr)
        return {};
    if (auto* item = jive::findItemWithID(*jivePluginPanelItem, "plugin-path-editor"))
        if (auto* editor = dynamic_cast<juce::TextEditor*>(item->getComponent().get()))
            return editor->getText();
    return {};
}

juce::String MainComponent::getSelectedPluginName() const {
    if (jivePluginPanelItem == nullptr)
        return {};
    if (auto* item = jive::findItemWithID(*jivePluginPanelItem, "plugin-selector"))
        if (auto* combo = dynamic_cast<juce::ComboBox*>(item->getComponent().get()))
            return combo->getText();
    return {};
}

void MainComponent::setPluginPanelExpanded(bool expanded) {
    appSettings.pluginPanelExpanded = expanded;
    if (jivePluginPanelItem != nullptr) {
        if (auto* item = jive::findItemWithID(*jivePluginPanelItem, "plugin-expanded-area"))
            item->state.setProperty("height", expanded ? 112 : 0, nullptr);
        resized();
    }
    settingsStore.scheduleSave(appSettings);
}

void MainComponent::setInstrumentFilterVisible(bool visible) {
    if (jivePluginPanelItem == nullptr)
        return;
    if (auto* item = jive::findItemWithID(*jivePluginPanelItem, "plugin-filter-combo"))
        item->state.setProperty("visibility", visible, nullptr);
}

void MainComponent::showPluginBrowseDialog() {
    auto chooser = std::make_shared<juce::FileChooser>(TRANS("Select VST3 Plugin Folder"),
                                                       juce::File(getPluginPathText()), "", true);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                         [this, chooser](const juce::FileChooser& fc) {
                             auto folder = fc.getResult();
                             if (folder.exists()) {
                                 setPluginPathText(folder.getFullPathName());
                                 pluginOperationController->scanPlugins();
                             }
                         });
}

void MainComponent::updatePluginPanelState(const PluginPanelState& state) {
    if (jivePluginPanelItem == nullptr)
        return;

    auto* selectorItem = jive::findItemWithID(*jivePluginPanelItem, "plugin-selector");
    auto* statusItem = jive::findItemWithID(*jivePluginPanelItem, "plugin-status-label");
    auto* listItem = jive::findItemWithID(*jivePluginPanelItem, "plugin-list-editor");
    auto* filterItem = jive::findItemWithID(*jivePluginPanelItem, "plugin-filter-combo");

    auto* selectorCombo
        = selectorItem != nullptr ? dynamic_cast<juce::ComboBox*>(selectorItem->getComponent().get()) : nullptr;
    auto* listEditor = listItem != nullptr ? dynamic_cast<juce::TextEditor*>(listItem->getComponent().get()) : nullptr;

    const auto setEnabled = [](::jive::GuiItem* item, bool enabled) {
        if (item != nullptr)
            item->state.setProperty("enabled", enabled, nullptr);
    };

    if (state.isCurrentlyScanning) {
        if (selectorItem != nullptr)
            selectorItem->state.removeAllChildren(nullptr);
        if (selectorCombo != nullptr)
            selectorCombo->setTextWhenNothingSelected(TRANS("Scanning..."));
        if (listEditor != nullptr) {
            auto scanText = TRANS("Scanning VST3 plugins...") + "\n";
            scanText << (state.scanningPluginName.isNotEmpty() ? state.scanningPluginName : TRANS("Preparing..."));
            listEditor->setText(scanText, juce::dontSendNotification);
        }
        setEnabled(jive::findItemWithID(*jivePluginPanelItem, "scan-btn"), false);
        setEnabled(jive::findItemWithID(*jivePluginPanelItem, "browse-btn"), false);
        setEnabled(jive::findItemWithID(*jivePluginPanelItem, "load-btn"), false);
    } else {
        const auto& names = [&]() -> const juce::StringArray& {
            const auto filterId = filterItem != nullptr ? filterItem->state["selected"].toString().getIntValue() : 1;
            if (filterId == 2 && !state.instrumentPluginNames.isEmpty())
                return state.instrumentPluginNames;
            if (filterId == 3 && !state.effectPluginNames.isEmpty())
                return state.effectPluginNames;
            return state.availablePluginNames;
        }();

        if (selectorItem != nullptr) {
            selectorItem->state.removeAllChildren(nullptr);
            auto selectedIndex = -1;
            auto index = 0;
            for (const auto& name : names) {
                auto option = juce::ValueTree("Option");
                option.setProperty("text", name, nullptr);
                selectorItem->state.addChild(option, index, nullptr);
                if (name.equalsIgnoreCase(state.preferredSelection))
                    selectedIndex = index;
                ++index;
            }
            if (names.isEmpty())
                selectorItem->state.setProperty("selected", -1, nullptr);
            else if (selectedIndex >= 0)
                selectorItem->state.setProperty("selected", selectedIndex, nullptr);
            else
                selectorItem->state.setProperty("selected", 0, nullptr);
        }

        if (selectorCombo != nullptr)
            selectorCombo->setTextWhenNothingSelected(TRANS("Select a scanned plugin..."));
        if (listEditor != nullptr)
            listEditor->setText(TRANS(state.pluginListText), juce::dontSendNotification);

        setEnabled(jive::findItemWithID(*jivePluginPanelItem, "scan-btn"), true);
        setEnabled(jive::findItemWithID(*jivePluginPanelItem, "browse-btn"), true);
        setEnabled(jive::findItemWithID(*jivePluginPanelItem, "load-btn"), !names.isEmpty());
        setEnabled(jive::findItemWithID(*jivePluginPanelItem, "unload-btn"), state.hasLoadedPlugin);
        setEnabled(jive::findItemWithID(*jivePluginPanelItem, "editor-btn"), state.hasLoadedPlugin);
        if (auto* item = jive::findItemWithID(*jivePluginPanelItem, "plugin-path-editor"))
            item->state.setProperty("enabled", true, nullptr);
    }

    // Status line: formats + scan summary + loaded plugin info.
    auto text = TRANS(state.availableFormatsDescription);
    if (state.supportsVst3)
        text << TRANS(" [VST3 ready]");

    if (state.isCurrentlyScanning) {
        text << TRANS(" | Scanning: ") << state.scanningPluginName << "...";
    } else {
        auto summary = state.lastScanSummary;
        if (summary.startsWith("VST3 scan complete: ") && !summary.contains("no plugins")) {
            auto resultSuffix = (state.scanFailedCount > 0) ? TRANS(" failed (see log).") : TRANS(" failed.");
            text << " | " << TRANS("VST3 scan complete: ") << juce::String(state.scanPluginCount)
                 << TRANS(" plugin(s), ") << juce::String(state.scanFailedCount) << resultSuffix;
        } else if (summary.startsWith("VST3 scan found no plugins; ")) {
            text << " | " << TRANS("VST3 scan found no plugins: ") << juce::String(state.scanFailedCount)
                 << TRANS(" failed (see log).");
        } else if (summary.startsWith("Loaded cached plugin list: ")) {
            text << " | " << TRANS("Loaded cached plugin list: ") << juce::String(state.scanPluginCount)
                 << TRANS(" plugin(s).");
        } else {
            text << " | " << TRANS(summary);
        }
    }

    if (state.hasLoadedPlugin) {
        text << TRANS(" | Loaded: ") << state.currentPluginName;

        if (state.isPrepared)
            text << " @ " << juce::String(state.preparedSampleRate, 0) << " Hz / "
                 << juce::String(state.preparedBlockSize);
        else
            text << TRANS(" [not prepared]");

        if (state.isEditorOpen)
            text << TRANS(" | Editor open");
    } else if (state.lastLoadError.isNotEmpty() && state.lastLoadError != "No plugin load attempted yet.") {
        text << TRANS(" | Load error: ") << state.lastLoadError;
    } else if (state.lastPluginName.isNotEmpty()) {
        text << TRANS(" | Last plugin: ") << state.lastPluginName;
    }

    if (statusItem != nullptr)
        statusItem->state.setProperty("text", text, nullptr);
}

void MainComponent::refreshPluginPanelTexts() {
    if (jivePluginPanelItem == nullptr)
        return;

    if (auto* item = jive::findItemWithID(*jivePluginPanelItem, "plugin-path-label"))
        item->state.setProperty("text", TRANS("VST3 Path"), nullptr);
    const auto setButtonText = [this](const char* id, const juce::String& text) {
        if (auto* item = jive::findItemWithID(*jivePluginPanelItem, id))
            item->state.setProperty("text", text, nullptr);
    };
    setButtonText("scan-btn", TRANS("Scan VST3"));
    setButtonText("load-btn", TRANS("Load"));
    setButtonText("unload-btn", TRANS("Unload"));
    setButtonText("editor-btn", TRANS("Open Editor"));
    if (auto* item = jive::findItemWithID(*jivePluginPanelItem, "plugin-filter-combo")) {
        const juce::StringArray filterTexts { TRANS("All"), TRANS("Instruments Only"), TRANS("Effects Only") };
        auto childIndex = 0;
        for (auto child : item->state) {
            if (childIndex < filterTexts.size())
                child.setProperty("text", filterTexts[childIndex], nullptr);
            ++childIndex;
        }
    }
    if (auto* item = jive::findItemWithID(*jivePluginPanelItem, "plugin-selector"))
        if (auto* combo = dynamic_cast<juce::ComboBox*>(item->getComponent().get()))
            combo->setTextWhenNothingSelected(TRANS("Select a scanned plugin..."));

    // Re-apply the last state to refresh status text (locale-dependent).
    refreshPluginUiState();
}

// ── JIVE controls panel accessors ──────────────────────────────────────────

float MainComponent::getMasterGain() const {
    if (jiveControlsPanelItem == nullptr)
        return 0.0f;
    if (auto* item = jive::findItemWithID(*jiveControlsPanelItem, "volume-knob"))
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get()))
            return static_cast<float>(slider->getValue());
    return 0.0f;
}

float MainComponent::getAttack() const {
    if (jiveControlsPanelItem == nullptr)
        return 0.0f;
    if (auto* item = jive::findItemWithID(*jiveControlsPanelItem, "attack-knob"))
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get()))
            return static_cast<float>(slider->getValue());
    return 0.0f;
}

float MainComponent::getDecay() const {
    if (jiveControlsPanelItem == nullptr)
        return 0.0f;
    if (auto* item = jive::findItemWithID(*jiveControlsPanelItem, "decay-knob"))
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get()))
            return static_cast<float>(slider->getValue());
    return 0.0f;
}

float MainComponent::getSustain() const {
    if (jiveControlsPanelItem == nullptr)
        return 0.0f;
    if (auto* item = jive::findItemWithID(*jiveControlsPanelItem, "sustain-knob"))
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get()))
            return static_cast<float>(slider->getValue());
    return 0.0f;
}

float MainComponent::getRelease() const {
    if (jiveControlsPanelItem == nullptr)
        return 0.0f;
    if (auto* item = jive::findItemWithID(*jiveControlsPanelItem, "release-knob"))
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get()))
            return static_cast<float>(slider->getValue());
    return 0.0f;
}

double MainComponent::getControlsPlaybackSpeed() const {
    if (jiveControlsPanelItem == nullptr)
        return 1.0;
    if (auto* item = jive::findItemWithID(*jiveControlsPanelItem, "speed-knob"))
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get()))
            return slider->getValue();
    return 1.0;
}

void MainComponent::setControlsValues(float masterGain, float attack, float decay, float sustain, float release) {
    if (jiveControlsPanelItem == nullptr)
        return;
    const auto setSlider = [this](const char* id, double value) {
        if (auto* item = jive::findItemWithID(*jiveControlsPanelItem, id))
            if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get()))
                slider->setValue(value, juce::dontSendNotification);
    };
    setSlider("volume-knob", masterGain);
    setSlider("attack-knob", attack);
    setSlider("decay-knob", decay);
    setSlider("sustain-knob", sustain);
    setSlider("release-knob", release);
    if (auto* item = jive::findItemWithID(*jiveControlsPanelItem, "adsr-curve"))
        if (auto* curve = dynamic_cast<AdsrCurveComponent*>(item->getComponent().get()))
            curve->setParameters(attack, decay, sustain, release);
}

void MainComponent::setControlsPlaybackSpeed(double speed) {
    if (jiveControlsPanelItem == nullptr)
        return;
    if (auto* item = jive::findItemWithID(*jiveControlsPanelItem, "speed-knob"))
        if (auto* slider = dynamic_cast<juce::Slider*>(item->getComponent().get()))
            slider->setValue(juce::jlimit(0.5, 2.0, speed), juce::dontSendNotification);
}

void MainComponent::setControlsPresets(const juce::StringArray& presetIds, const juce::String& currentPresetId,
                                       const juce::StringArray& presetDisplayNames) {
    availablePresetIds = presetIds;
    if (jiveControlsPanelItem == nullptr)
        return;

    auto* comboItem = jive::findItemWithID(*jiveControlsPanelItem, "preset-combo");
    auto* combo = comboItem != nullptr ? dynamic_cast<juce::ComboBox*>(comboItem->getComponent().get()) : nullptr;

    if (comboItem != nullptr)
        comboItem->state.removeAllChildren(nullptr);
    auto selectedIndex = 0;
    for (int i = 0; i < presetIds.size(); ++i) {
        const auto displayName = (i < presetDisplayNames.size()) ? presetDisplayNames[i] : presetIds[i];
        if (comboItem != nullptr) {
            auto option = juce::ValueTree("Option");
            option.setProperty("text", displayName, nullptr);
            comboItem->state.addChild(option, i, nullptr);
        }
        if (presetIds[i] == currentPresetId)
            selectedIndex = i;
    }
    if (comboItem != nullptr)
        comboItem->state.setProperty("selected", selectedIndex, nullptr);

    updateControlsPresetActionButtons();
}

juce::String MainComponent::getSelectedPresetId() const {
    if (jiveControlsPanelItem == nullptr)
        return {};
    if (auto* item = jive::findItemWithID(*jiveControlsPanelItem, "preset-combo"))
        if (auto* combo = dynamic_cast<juce::ComboBox*>(item->getComponent().get())) {
            const auto index = combo->getSelectedItemIndex();
            if (juce::isPositiveAndBelow(index, availablePresetIds.size()))
                return availablePresetIds[index];
        }
    return {};
}

void MainComponent::updateControlsPresetActionButtons() {
    const auto isUserPreset = getSelectedPresetId().isNotEmpty();
    if (jiveControlsPanelItem == nullptr)
        return;
    const auto setEnabled = [this](const char* id, bool enabled) {
        if (auto* item = jive::findItemWithID(*jiveControlsPanelItem, id))
            item->state.setProperty("enabled", enabled, nullptr);
    };
    setEnabled("rename-preset-btn", isUserPreset);
    setEnabled("delete-preset-btn", isUserPreset);
}

void MainComponent::setRecordingControlsState(RecordingControlsState state) {
    recordingControlsState = state;
    if (jiveControlsPanelItem == nullptr)
        return;

    auto recordEnabled = true;
    auto playEnabled = state.hasTake;
    auto stopEnabled = false;
    auto backToStartEnabled = state.hasTake;
    auto exportMidiEnabled = state.hasTake && state.canExportMidiTake;
    auto exportWavEnabled = state.hasTake && state.canExportWavTake;
    auto importMidiEnabled = true;
    auto saveEnabled = false;
    auto openEnabled = false;

    switch (state.state) {
    case RecordingState::idle:
        playEnabled = state.hasTake;
        backToStartEnabled = state.hasTake;
        exportMidiEnabled = state.hasTake && state.canExportMidiTake;
        exportWavEnabled = state.hasTake && state.canExportWavTake;
        importMidiEnabled = true;
        saveEnabled = state.hasTake;
        openEnabled = true;
        break;
    case RecordingState::recording:
        recordEnabled = false;
        playEnabled = false;
        backToStartEnabled = false;
        exportMidiEnabled = false;
        exportWavEnabled = false;
        importMidiEnabled = false;
        stopEnabled = true;
        saveEnabled = false;
        openEnabled = false;
        break;
    case RecordingState::playing:
        recordEnabled = false;
        playEnabled = false;
        backToStartEnabled = state.hasTake;
        exportMidiEnabled = false;
        exportWavEnabled = false;
        importMidiEnabled = false;
        stopEnabled = true;
        saveEnabled = false;
        openEnabled = false;
        break;
    }

    const auto setEnabled = [this](const char* id, bool enabled) {
        if (auto* item = jive::findItemWithID(*jiveControlsPanelItem, id))
            item->state.setProperty("enabled", enabled, nullptr);
    };
    setEnabled("record-btn", recordEnabled);
    setEnabled("play-btn", playEnabled);
    setEnabled("stop-btn", stopEnabled);
    setEnabled("back-btn", backToStartEnabled);
    setEnabled("import-midi-btn", importMidiEnabled);
    setEnabled("export-midi-btn", exportMidiEnabled);
    setEnabled("export-wav-btn", exportWavEnabled);
    setEnabled("save-perf-btn", saveEnabled);
    setEnabled("open-perf-btn", openEnabled);
}

juce::Rectangle<int> MainComponent::getRecentFilesButtonScreenBounds() const {
    if (jiveControlsPanelItem == nullptr)
        return {};
    if (auto* item = jive::findItemWithID(*jiveControlsPanelItem, "recent-btn"))
        return item->getComponent()->getScreenBounds();
    return {};
}

void MainComponent::refreshControlsTexts() {
    if (jiveControlsPanelItem == nullptr)
        return;

    const auto setText = [this](const char* id, const juce::String& text) {
        if (auto* item = jive::findItemWithID(*jiveControlsPanelItem, id))
            item->state.setProperty("text", text, nullptr);
    };
    const auto setButtonText = [this](const char* id, const juce::String& text) {
        if (auto* item = jive::findItemWithID(*jiveControlsPanelItem, id))
            item->state.setProperty("text", text, nullptr);
    };
    const auto setTooltip = [this](const char* id, const juce::String& tooltip) {
        if (auto* item = jive::findItemWithID(*jiveControlsPanelItem, id))
            item->state.setProperty("tooltip", tooltip, nullptr);
    };

    setText("volume-label", TRANS("Volume"));
    setText("attack-label", TRANS("Attack"));
    setText("decay-label", TRANS("Decay"));
    setText("sustain-label", TRANS("Sustain"));
    setText("release-label", TRANS("Release"));
    setText("preset-label", TRANS("Preset"));
    setText("speed-label", TRANS("Speed"));
    setButtonText("save-preset-btn", TRANS("Save As New"));
    setButtonText("rename-preset-btn", TRANS("Rename"));
    setButtonText("delete-preset-btn", TRANS("Delete"));
    setTooltip("record-btn", TRANS("Record"));
    setTooltip("play-btn", TRANS("Play"));
    setTooltip("stop-btn", TRANS("Stop"));
    setTooltip("back-btn", TRANS("Back to Start"));
    setButtonText("export-midi-btn", TRANS("Export MIDI"));
    setButtonText("export-wav-btn", TRANS("Export WAV"));
    setButtonText("import-midi-btn", TRANS("Import MIDI"));
    setButtonText("recent-btn", TRANS("Recent"));
    setButtonText("save-perf-btn", TRANS("Save"));
    setButtonText("open-perf-btn", TRANS("Open"));
    setButtonText("song-info-btn", TRANS("Song Info"));

    setRecordingControlsState(recordingControlsState);
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
    refreshPluginPanelTexts();
    refreshControlsTexts();
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
