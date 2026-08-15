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

// Locate a project source file: CWD-relative first, then executable-relative
// (the Windows exe lives at <project>/build-win-msvc/devpiano_artefacts/Debug/,
// so walking up from the exe dir finds the project root).
juce::File resolveSourceFile(const juce::String& relativePath) {
    auto cwdFile = juce::File::getCurrentWorkingDirectory().getChildFile(relativePath);
    if (cwdFile.existsAsFile())
        return cwdFile;

    auto dir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    for (int i = 0; i < 4; ++i) {
        auto candidate = dir.getChildFile(relativePath);
        if (candidate.existsAsFile())
            return candidate;
        dir = dir.getParentDirectory();
    }

    return cwdFile; // best effort; caller handles missing file
}

// Recursively remove the "style-sheet" property from all components in a JIVE
// hierarchy before destruction. This ensures StyleSheet (and its ComponentInteractionState)
// is destroyed while the Component and its mouseListeners list are still completely alive,
// avoiding access violations during Component::~Component().
static void clearJiveStyleSheets(juce::Component* comp) {
    if (comp == nullptr)
        return;

    for (int i = 0; i < comp->getNumChildComponents(); ++i)
        clearJiveStyleSheets(comp->getChildComponent(i));

    if (comp->getProperties().contains("style-sheet"))
        comp->getProperties().remove("style-sheet");
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

    // Detach JIVE style sheets before component destruction to prevent
    // ComponentInteractionState from calling removeMouseListener on a destructing Component.
    if (jiveRootItem != nullptr) {
        clearJiveStyleSheets(jiveRootItem->getComponent().get());
        jiveRootItem.reset();
    }
    devpiano::ui::jive::StyleCatalog::get().releaseOwnedStyles();
    jiveInterpreter.reset();
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
        const auto tokensFile = resolveSourceFile("source/UI/jive/design_tokens.json");
        if (tokensFile.existsAsFile()) {
            lastTokensModTime = tokensFile.getLastModificationTime();
            if (auto stream = tokensFile.createInputStream()) {
                auto json = juce::JSON::parse(*stream);
                devpiano::jive::DesignTokens::get().loadFromJSON(json);
            }
        }
    }

    lookAndFeel = std::make_unique<DevPianoLookAndFeel>();
    setLookAndFeel(lookAndFeel.get());
    setWantsKeyboardFocus(true);

    // ── JIVE root layout (header, plugin, controls, keyboard, status bar) ──
    {
        // Global style rules (loaded once; re-loading is idempotent).
        const auto styleFile = resolveSourceFile("source/UI/jive/style_sheets.json");
        if (styleFile.existsAsFile()) {
            lastStylesModTime = styleFile.getLastModificationTime();
            if (auto stream = styleFile.createInputStream()) {
                auto json = juce::JSON::parse(*stream);
                devpiano::ui::jive::StyleCatalog::get().loadFromJSON(json);
            }
        }

        jiveInterpreter = std::make_unique<::jive::Interpreter>();
        auto& factory = jiveInterpreter->getComponentFactory();

        factory.set("SettingsButton", [] {
            auto btn = std::make_unique<juce::DrawableButton>("settings", juce::DrawableButton::ImageFitted);
            btn->setImages(createGearIcon(devpiano::jive::DesignTokens::get().textSecondary()).get(),
                           createGearIcon(devpiano::jive::DesignTokens::get().primary()).get(), nullptr);
            return btn;
        });
        factory.set("PathEditor", [] {
            auto editor = std::make_unique<juce::TextEditor>();
            editor->setMultiLine(false);
            editor->setReturnKeyStartsNewLine(false);
            return editor;
        });
        factory.set("ListEditor", [] {
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
        factory.set("DevKnob", [] {
            auto slider = std::make_unique<juce::Slider>();
            slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 44, 16);
            slider->setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f,
                                        true);
            return slider;
        });
        factory.set("SpeedSlider", [] {
            auto slider = std::make_unique<juce::Slider>();
            slider->setSliderStyle(juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 42, 16);
            return slider;
        });
        factory.set("AdsrCurve", [] { return std::make_unique<AdsrCurveComponent>(); });
        const auto registerIconButton
            = [&factory](const char* type, const std::unique_ptr<juce::Drawable>& image, const juce::String& tooltip) {
                  factory.set(type, [&image, tooltip] {
                      auto btn = std::make_unique<juce::DrawableButton>(tooltip, juce::DrawableButton::ImageFitted);
                      btn->setImages(image.get());
                      // Inset the icon area so large transport buttons keep
                      // their size while the glyph renders at ~18 px.
                      btn->setEdgeIndent(10);
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
        factory.set("CustomKeyboard",
                    [this] { return std::make_unique<KeyboardViewport>(audioEngine.getKeyboardState()); });
        factory.set("StatusBarMidiDot", [] { return std::make_unique<StatusBarMidiDot>(); });

        auto rootTree = devpiano::ui::jive::makeRootLayout();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(rootTree);
        jiveRootItem = jiveInterpreter->interpret(rootTree);
        if (jiveRootItem != nullptr) {
            addAndMakeVisible(jiveRootItem->getComponent().get());

            const auto findItem
                = [this](const char* id) -> ::jive::GuiItem* { return jive::findItemWithID(*jiveRootItem, id); };
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

            // ── header ──
            if (auto* btn = findButton("settings-btn"))
                btn->onClick = [this] { showSettingsDialog(); };

            // ── plugin panel ──
            const auto wireButton = [&findButton](const char* id, const std::function<void()>& action) {
                if (auto* btn = findButton(id))
                    btn->onClick = action;
            };
            wireButton("load-btn", [this] { pluginOperationController->loadSelectedPlugin(); });
            wireButton("unload-btn", [this] { pluginOperationController->unloadCurrentPlugin(); });
            wireButton("editor-btn", [this] { pluginOperationController->togglePluginEditor(); });
            wireButton("toggle-btn", [this] { setPluginPanelExpanded(!appSettings.pluginPanelExpanded); });
            wireButton("scan-btn", [this] { pluginOperationController->scanPlugins(); });
            wireButton("browse-btn", [this] { showPluginBrowseDialog(); });

            if (auto* combo = findCombo("plugin-selector")) {
                combo->setTextWhenNothingSelected(TRANS("Select a scanned plugin..."));
                combo->setWantsKeyboardFocus(false);
                combo->onChange = [this, combo] {
                    if (isUpdatingPluginSelector)
                        return;
                    if (combo->getSelectedItemIndex() >= 0)
                        pluginOperationController->loadSelectedPlugin();
                };
            }
            if (auto* combo = findCombo("plugin-filter-combo")) {
                combo->clear(juce::dontSendNotification);
                combo->addItem(TRANS("All"), 1);
                combo->addItem(TRANS("Instruments Only"), 2);
                combo->addItem(TRANS("Effects Only"), 3);
                combo->setSelectedId(1, juce::dontSendNotification);
                combo->setWantsKeyboardFocus(false);
                combo->onChange = [this] {
                    if (!isUpdatingPluginSelector)
                        refreshPluginUiState();
                };
            }
            if (auto* item = findItem("plugin-path-editor"))
                if (auto* editor = dynamic_cast<juce::TextEditor*>(item->getComponent().get()))
                    editor->onReturnKey = [this] { pluginOperationController->scanPlugins(); };

            // ── controls panel ──
            auto* adsrCurve = []() -> AdsrCurveComponent* { return nullptr; }();
            if (auto* item = findItem("adsr-curve"))
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

            wireButton("record-btn", [this] { recordingSessionController->handleRecordClicked(); });
            wireButton("play-btn", [this] { recordingSessionController->handlePlayClicked(); });
            wireButton("stop-btn", [this] { recordingSessionController->handleStopClicked(); });
            wireButton("back-btn", [this] { recordingSessionController->handleBackToStartClicked(); });
            wireButton("export-midi-btn", [this] { recordingSessionController->handleExportMidiClicked(); });
            wireButton("export-wav-btn", [this] { recordingSessionController->handleExportWavClicked(); });
            wireButton("import-midi-btn", [this] { recordingSessionController->handleImportMidiClicked(); });
            wireButton("save-perf-btn", [this] { recordingSessionController->handleSavePerformanceClicked(); });
            wireButton("open-perf-btn", [this] { recordingSessionController->handleOpenPerformanceClicked(); });
            wireButton("song-info-btn", [this] { recordingSessionController->handleSongInfoClicked(); });
            wireButton("recent-btn", [this] { showRecentFilesMenu(); });
            wireButton("save-preset-btn", [this] { presetFlowSupport->handleSaveAsNewPreset(); });
            wireButton("rename-preset-btn", [this] { presetFlowSupport->handleRenamePreset(); });
            wireButton("delete-preset-btn", [this] { presetFlowSupport->handleDeletePreset(); });

            if (auto* combo = findCombo("preset-combo")) {
                combo->setTextWhenNothingSelected(TRANS("Default"));
                combo->setWantsKeyboardFocus(false);
                combo->onChange = [this, combo] {
                    if (isUpdatingPresets)
                        return;
                    const auto selectedId = combo->getSelectedId();
                    if (selectedId <= 0 || !juce::isPositiveAndBelow(selectedId - 1, availablePresetIds.size()))
                        return;
                    presetFlowSupport->applyPresetById(availablePresetIds[selectedId - 1]);
                    updateControlsPresetActionButtons();
                };
            }

            setRecordingControlsState({});

            // ── keyboard area ──
            if (auto* item = findItem("custom-keyboard"))
                if (auto* viewport = dynamic_cast<KeyboardViewport*>(item->getComponent().get()))
                    customKeyboardRef = &viewport->getCustomKeyboard();
        }
    }

    const auto pluginRecovery = getPluginRecoverySettingsWithFallback();
    setPluginPathText(makeSafeUiText(pluginRecovery.pluginSearchPath));

    setPluginPanelExpanded(appSettings.pluginPanelExpanded);

    recordingSessionController->onFileOpened = [this](const juce::File& file) {
        recentFiles.addFile(file);
        saveRecentFiles();
    };

    // Restore recently opened files list from settings.
    recentFiles.restoreFromString(appSettings.recentFilesSerialized);

    // Initialize playback speed to 1.0x (never persisted — default on every launch).
    setControlsPlaybackSpeed(1.0);
    recordingEngine.setPlaybackSpeedMultiplier(1.0);

    // Wire CustomKeyboard mouse interaction → sound (with MIDI matrix)
    auto& customKeyboard = getCustomKeyboard();
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
                    setKeyboardLayout(updatedLayout);
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

#if DEBUG
    // Check file modification time every ~1 second (30 ticks at 30Hz) in debug builds
    if (++hotReloadCheckCounter >= 30) {
        hotReloadCheckCounter = 0;
        const auto tokensFile = resolveSourceFile("source/UI/jive/design_tokens.json");
        const auto styleFile = resolveSourceFile("source/UI/jive/style_sheets.json");
        const bool tokensChanged
            = tokensFile.existsAsFile() && tokensFile.getLastModificationTime() > lastTokensModTime;
        const bool stylesChanged = styleFile.existsAsFile() && styleFile.getLastModificationTime() > lastStylesModTime;
        if (tokensChanged || stylesChanged) {
            reloadStylesAndTokens();
        }
    }
#endif
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

    const auto panelBounds = [this](const char* id) {
        if (jiveRootItem == nullptr)
            return juce::Rectangle<int> {};
        if (auto* item = jive::findItemWithID(*jiveRootItem, id))
            return item->getComponent()->getBounds();
        return juce::Rectangle<int> {};
    };
    drawPanelBorder(panelBounds("header"));
    drawPanelBorder(panelBounds("plugin-panel"));
    drawPanelBorder(panelBounds("controls-panel"));
    drawPanelBorder(panelBounds("keyboard-area"));
}

void MainComponent::resized() {
    // The entire layout is a single JIVE FlexBox tree; resizing the root
    // component propagates to every panel.
    if (jiveRootItem != nullptr) {
        jiveRootItem->getComponent()->setBounds(getLocalBounds());
        // Layout just recomputed the status label's width — re-truncate the
        // status text to it (JIVE TextComponents never clip their text).
        refreshPluginStatusEllipsis();
    }
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
    // Ctrl+R (or Cmd+R) hot reload styles & design tokens
    if (key.getModifiers().isCtrlDown() && (key.getKeyCode() == 'r' || key.getKeyCode() == 'R')) {
        reloadStylesAndTokens();
        return true;
    }

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

void MainComponent::reloadStylesAndTokens() {
    bool tokensLoaded = false;
    bool stylesLoaded = false;

    // 1. Reload design tokens
    const auto tokensFile = resolveSourceFile("source/UI/jive/design_tokens.json");
    if (tokensFile.existsAsFile()) {
        lastTokensModTime = tokensFile.getLastModificationTime();
        if (auto stream = tokensFile.createInputStream()) {
            auto json = juce::JSON::parse(*stream);
            if (!json.isVoid()) {
                devpiano::jive::DesignTokens::get().loadFromJSON(json);
                tokensLoaded = true;
            }
        }
    }

    // 2. Refresh LookAndFeel
    if (lookAndFeel != nullptr) {
        lookAndFeel->refreshColours();
        sendLookAndFeelChange();
    }

    // 3. Reload StyleCatalog & apply to live JIVE tree
    const auto styleFile = resolveSourceFile("source/UI/jive/style_sheets.json");
    if (styleFile.existsAsFile()) {
        lastStylesModTime = styleFile.getLastModificationTime();
        if (auto stream = styleFile.createInputStream()) {
            auto json = juce::JSON::parse(*stream);
            if (!json.isVoid()) {
                devpiano::ui::jive::StyleCatalog::get().loadFromJSON(json);
                stylesLoaded = true;
            }
        }
    }

    if (jiveRootItem != nullptr) {
        devpiano::ui::jive::StyleCatalog::get().refreshStyles(jiveRootItem->state);

        // Update settings button icon colours with newly loaded tokens
        if (auto* item = jive::findItemWithID(*jiveRootItem, "settings-btn")) {
            if (auto* btn = dynamic_cast<juce::DrawableButton*>(item->getComponent().get())) {
                btn->setImages(createGearIcon(devpiano::jive::DesignTokens::get().textSecondary()).get(),
                               createGearIcon(devpiano::jive::DesignTokens::get().primary()).get(), nullptr);
            }
        }
    }

    repaint();

    DP_LOG_INFO("MainComponent: Hot reload completed (tokens: " + juce::String(tokensLoaded ? "ok" : "failed") + " ["
                + tokensFile.getFullPathName() + "], styles: " + juce::String(stylesLoaded ? "ok" : "failed") + " ["
                + styleFile.getFullPathName() + "])");
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

    setKeyboardLayout(keyboardMidiMapper.getLayout());
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

// JIVE component accessors (see MainComponentJiveAccessors.cpp)
#include "MainComponentJiveAccessors.cpp"
