#include "MainComponent.h"

#include "Diagnostics/Log.h"
#include "Plugin/PluginFlowSupport.h"
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
// Window size limits live in design_tokens.json (single source of truth);
// DesignTokens falls back to the shipped JSON values when the file is missing.

juce::String makeSafeUiText(juce::String text) {
    text = text.replaceCharacters("\r\n\t", "   ");

    constexpr auto maxLen = 1024;
    if (text.length() > maxLen) {
        text = text.substring(0, maxLen);
    }

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
    if (cwdFile.existsAsFile()) {
        return cwdFile;
    }

    auto dir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    for (int i = 0; i < 4; ++i) {
        auto candidate = dir.getChildFile(relativePath);
        if (candidate.existsAsFile()) {
            return candidate;
        }
        dir = dir.getParentDirectory();
    }

    return cwdFile; // best effort; caller handles missing file
}

// Recursively remove the "style-sheet" property from all components in a JIVE
// hierarchy before destruction. This ensures StyleSheet (and its ComponentInteractionState)
// is destroyed while the Component and its mouseListeners list are still completely alive,
// avoiding access violations during Component::~Component().
void clearJiveStyleSheets(juce::Component* comp) {
    if (comp == nullptr) {
        return;
    }

    for (int i = 0; i < comp->getNumChildComponents(); ++i) {
        clearJiveStyleSheets(comp->getChildComponent(i));
    }

    if (comp->getProperties().contains("style-sheet")) {
        comp->getProperties().remove("style-sheet");
    }
}

// Collect every Component in the JIVE tree (pre-order). GuiItem releases its
// `component` member before its `styleSheet` member, so without extra
// references the Components would be destroyed first, followed by their
// StyleSheets — whose ComponentInteractionState holds a raw Component
// reference and calls removeMouseListener() in its destructor. Holding the
// Components here keeps them alive until all StyleSheets are gone; the
// vector destroys its elements in reverse order, so children are deleted
// before their parents, matching JUCE's child-ownership rules.
void collectJiveComponents(::jive::GuiItem& item, std::vector<std::shared_ptr<juce::Component>>& components) {
    if (auto component = item.getComponent()) {
        components.push_back(std::move(component));
    }

    for (auto* child : item.getChildren()) {
        collectJiveComponents(*child, components);
    }
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
std::unique_ptr<juce::Drawable> createPauseIcon() {
    juce::Path p;
    // 两个竖条（II），与 Play 的 ▶ 对称；作为 PlayButton 的 on 态图标，
    // 播放/录制进行中按钮显示 Pause 语义。
    p.addRectangle(-6.0f, -7.0f, 4.0f, 14.0f);
    p.addRectangle(2.0f, -7.0f, 4.0f, 14.0f);
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
    // 两个三角形尖端朝左（<<），“Back to Start” 语义与 Play 的 ▶ 对称。
    p.addTriangle(7.0f, -6.0f, 7.0f, 6.0f, -1.0f, 0.0f);
    p.addTriangle(1.0f, -6.0f, 1.0f, 6.0f, -7.0f, 0.0f);
    auto d = std::make_unique<juce::DrawablePath>();
    d->setPath(p);
    d->setFill(devpiano::jive::DesignTokens::get().textSecondary());
    return d;
}

#if JUCE_WINDOWS
void suppressImeForPeer(juce::ComponentPeer* peer) {
    if (peer == nullptr) {
        return;
    }

    if (auto hwnd = static_cast<HWND>(peer->getNativeHandle())) {
        ImmAssociateContext(hwnd, nullptr);
    }
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
    keyboardMidiMapper.setSustainPedalCallback([this](bool isDown) {
        audioEngine.sendController(1, 64, isDown ? 127 : 0);
        notifyMidiActivity();
    });
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
    audioEngine.getKeyboardState().addListener(this);
    updateStatusBar();
}

MainComponent::~MainComponent() {
    setLookAndFeel(nullptr);
    // Restore the JUCE default global LookAndFeel (see initialiseUi).
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    stopTimer();
    audioEngine.getKeyboardState().removeListener(this);

#if DEBUG
    // Destroy the inspector BEFORE the JIVE component tree is torn down.
    // ComponentModel keeps juce::Value bindings over component properties;
    // once the tree's "style-sheet" properties are cleared below, that
    // binding is the last StyleSheet reference, and destroying the inspector
    // later (member teardown) would make StyleSheet die after its Component,
    // leaving ComponentInteractionState to call removeMouseListener() on a
    // destroyed Component (access violation at shutdown).
    inspector.reset();
#endif

    juce::Logger::setCurrentLogger(nullptr);
    appSettings.keyboardScrollOffsetX = getKeyboardViewPositionX();
    saveSettingsNow();

    pluginOperationController.reset();
    shutdownAudio();
    if (recordingSessionController != nullptr) {
        recordingSessionController->onFileOpened = {};
    }
    audioEngine.setRecordingEngine(nullptr);
    pluginHost.unloadPlugin();
    settingsWindowManager.reset();

    // Detach JIVE style sheets before component destruction: GuiItem's member
    // declaration order would destroy each Component before its StyleSheet,
    // leaving ComponentInteractionState to call removeMouseListener() on a
    // dead Component. Collect owning references first so every StyleSheet
    // dies while its Component is still alive; `jiveComponents` then unwinds
    // at the end of this scope, destroying the Components strictly after
    // their StyleSheets (children before parents, as JUCE requires).
    if (jiveRootItem != nullptr) {
        std::vector<std::shared_ptr<juce::Component>> jiveComponents;
        collectJiveComponents(*jiveRootItem, jiveComponents);
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

    std::uint16_t mask = 0;
    for (std::size_t i = 0; i < 16; ++i) {
        if (appSettings.channelMatrix.channels[i].followKey) {
            mask |= static_cast<std::uint16_t>(1U << i);
        }
    }
    audioEngine.setPlaybackTranspose(appSettings.midiTranspose, appSettings.keySignature, mask);
    updateStatusBar();
}

void MainComponent::handlePresetShortcut(int index) {
    if (presetFlowSupport != nullptr) {
        presetFlowSupport->applyPresetByIndex(index);
    }
}
void MainComponent::initialiseUi() {
    // 加载设计 token（单一配色真相源）— 必须在构造 LookAndFeel 之前
    {
        // 1. 基准兜底：从编译期嵌入的 BinaryData 加载（保证任何独立运行环境 100% 可用）
        auto embeddedTokens = juce::JSON::parse(
            juce::String::fromUTF8(BinaryData::design_tokens_json, BinaryData::design_tokens_jsonSize));
        if (!embeddedTokens.isVoid()) {
            devpiano::jive::DesignTokens::get().loadFromJSON(embeddedTokens);
        } else {
            DP_LOG_ERROR("[Style] BinaryData::design_tokens_json failed to parse");
        }

        // 2. 开发环境增强：若本地源码存在文件，覆盖加载并记录修改时间（支持热重载）
        const auto tokensFile = resolveSourceFile("source/UI/jive/design_tokens.json");
        if (tokensFile.existsAsFile()) {
            lastTokensModTime = tokensFile.getLastModificationTime();
            if (auto stream = tokensFile.createInputStream()) {
                auto json = juce::JSON::parse(*stream);
                if (json.isVoid()) {
                    DP_LOG_ERROR("[Style] design_tokens.json failed to parse: " + tokensFile.getFullPathName());
                } else {
                    devpiano::jive::DesignTokens::get().loadFromJSON(json);
                }
            }
        }
    }

    lookAndFeel = std::make_unique<DevPianoLookAndFeel>();
    setLookAndFeel(lookAndFeel.get());
    // Native dialogs (AlertWindow for preset rename/save/delete, FileChooser
    // for VST3 browsing) use the *global* default LookAndFeel, not the
    // component one. Install our dark theme globally so every native window
    // matches the main UI instead of JUCE's light default.
    juce::LookAndFeel::setDefaultLookAndFeel(lookAndFeel.get());
    setWantsKeyboardFocus(true);

    // ── JIVE root layout (header, plugin, controls, keyboard, status bar) ──
    {
        // 1. 基准兜底：从编译期嵌入的 BinaryData 加载全局样式表规则
        auto embeddedStyles = juce::JSON::parse(
            juce::String::fromUTF8(BinaryData::style_sheets_json, BinaryData::style_sheets_jsonSize));
        if (!embeddedStyles.isVoid()) {
            devpiano::ui::jive::StyleCatalog::get().loadFromJSON(embeddedStyles);
        } else {
            DP_LOG_ERROR("[Style] BinaryData::style_sheets_json failed to parse");
        }

        // 2. 开发环境增强：若本地源码存在文件，覆盖加载并记录修改时间（支持热重载）
        const auto styleFile = resolveSourceFile("source/UI/jive/style_sheets.json");
        if (styleFile.existsAsFile()) {
            lastStylesModTime = styleFile.getLastModificationTime();
            if (auto stream = styleFile.createInputStream()) {
                auto json = juce::JSON::parse(*stream);
                if (json.isVoid()) {
                    DP_LOG_ERROR("[Style] style_sheets.json failed to parse: " + styleFile.getFullPathName());
                } else {
                    devpiano::ui::jive::StyleCatalog::get().loadFromJSON(json);
                }
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
        // Icons are owned by static storage for the app lifetime.
        static const auto recordIcon = createRecordIcon();
        static const auto playIcon = createPlayIcon();
        static const auto pauseIcon = createPauseIcon();
        static const auto stopIcon = createStopIcon();
        static const auto backIcon = createBackIcon();

        const auto registerIconButton
            = [&factory](const char* type, const juce::Drawable* image, const juce::String& tooltip,
                         const juce::Drawable* onImage = nullptr) {
                  factory.set(type, [image, tooltip, onImage] {
                      auto btn = std::make_unique<juce::DrawableButton>(tooltip, juce::DrawableButton::ImageFitted);
                      // onImage (optional) is shown while the button is latched
                      // (toggle on) — PlayButton uses it to switch ▶/II.
                      btn->setImages(image, nullptr, nullptr, nullptr, onImage, nullptr, nullptr, nullptr);
                      // Inset the icon area so large transport buttons keep
                      // their size while the glyph renders at ~18 px.
                      btn->setEdgeIndent(10);
                      btn->setTooltip(tooltip);
                      return btn;
                  });
              };
        registerIconButton("RecordButton", recordIcon.get(), TRANS("Record"));
        registerIconButton("PlayButton", playIcon.get(), TRANS("Play"), pauseIcon.get());
        registerIconButton("StopButton", stopIcon.get(), TRANS("Stop"));
        registerIconButton("BackButton", backIcon.get(), TRANS("Back to Start"));
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
                if (auto* item = findItem(id)) {
                    return dynamic_cast<juce::Slider*>(item->getComponent().get());
                }
                return nullptr;
            };
            const auto findCombo = [&findItem](const char* id) -> juce::ComboBox* {
                if (auto* item = findItem(id)) {
                    return dynamic_cast<juce::ComboBox*>(item->getComponent().get());
                }
                return nullptr;
            };
            const auto findButton = [&findItem](const char* id) -> juce::Button* {
                if (auto* item = findItem(id)) {
                    return dynamic_cast<juce::Button*>(item->getComponent().get());
                }
                return nullptr;
            };

            // ── header ──
            if (auto* btn = findButton("settings-btn")) {
                btn->onClick = [this] { showSettingsDialog(); };
            }

            // ── plugin panel ──
            const auto wireButton = [&findButton](const char* id, const std::function<void()>& action) {
                if (auto* btn = findButton(id)) {
                    btn->onClick = action;
                }
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
                    if (isUpdatingPluginSelector) {
                        return;
                    }
                    if (combo->getSelectedItemIndex() >= 0) {
                        pluginOperationController->loadSelectedPlugin();
                    }
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
                    if (!isUpdatingPluginSelector) {
                        refreshPluginUiState();
                    }
                };
            }
            if (auto* item = findItem("plugin-path-editor")) {
                if (auto* editor = dynamic_cast<juce::TextEditor*>(item->getComponent().get())) {
                    editor->onReturnKey = [this] { pluginOperationController->scanPlugins(); };
                }
            }

            // ── controls panel ──
            AdsrCurveComponent* adsrCurve = nullptr;
            if (auto* item = findItem("adsr-curve")) {
                adsrCurve = dynamic_cast<AdsrCurveComponent*>(item->getComponent().get());
            }

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
                          if (curve != nullptr) {
                              curve->setParameters(getAttack(), getDecay(), getSustain(), getRelease());
                          }
                          handlePerformanceUiChanged();
                      });
                  };
            wireAdsrKnob("attack-knob", 0.001, 2.0, 0.001, [](double v) { return juce::String(v, 3) + "s"; });
            wireAdsrKnob("decay-knob", 0.001, 2.0, 0.001, [](double v) { return juce::String(v, 3) + "s"; });
            wireAdsrKnob("sustain-knob", 0.0, 1.0, 0.01,
                         [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; });
            wireAdsrKnob("release-knob", 0.001, 3.0, 0.001, [](double v) { return juce::String(v, 3) + "s"; });

            // ── piano tone row (Phase 12-3) ──
            const auto wirePianoKnob
                = [&wireKnob, this](const char* id, const std::function<juce::String(double)>& formatter) {
                      wireKnob(id, 0.0, 1.0, 0.01, formatter, [this] { handlePerformanceUiChanged(); });
                  };
            wirePianoKnob("brightness-knob", [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; });
            wirePianoKnob("hardness-knob", [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; });
            wirePianoKnob("resonance-knob", [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; });
            wireKnob(
                "speed-knob", 0.5, 2.0, 0.25,
                [](double v) {
                    // 0.25-step speeds: format with two decimals, then trim a
                    // single trailing zero ("1.00" → "1.0", "1.25" stays,
                    // "1.50" → "1.5"). A single decimal place uses banker's
                    // rounding, which mis-rendered 1.25 as "1.2" and 1.75 as
                    // "1.8".
                    auto text = juce::String(v, 2);
                    if (text.endsWith("0")) {
                        text = text.dropLastCharacters(1);
                    }
                    return text + "x";
                },
                [this] { recordingSessionController->handlePlaybackSpeedChange(getControlsPlaybackSpeed()); });

            wireButton("record-btn", [this] { recordingSessionController->handleRecordClicked(); });
            wireButton("play-btn", [this] { recordingSessionController->handlePlayClicked(); });
            wireButton("stop-btn", [this] { recordingSessionController->handleStopClicked(); });
            wireButton("back-btn", [this] { recordingSessionController->handleBackToStartClicked(); });

            // Latched (toggle-on) accent colours: red while recording, green while playing.
            const auto& tokens = devpiano::jive::DesignTokens::get();
            if (auto* btn = findButton("record-btn")) {
                btn->setColour(juce::TextButton::buttonOnColourId, tokens.recordActive());
            }
            if (auto* btn = findButton("play-btn")) {
                btn->setColour(juce::TextButton::buttonOnColourId, tokens.playActive());
            }
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
                    if (isUpdatingPresets) {
                        return;
                    }
                    const auto selectedId = combo->getSelectedId();
                    if (selectedId <= 0 || !juce::isPositiveAndBelow(selectedId - 1, availablePresetIds.size())) {
                        return;
                    }
                    presetFlowSupport->applyPresetById(availablePresetIds[selectedId - 1]);
                    updateControlsPresetActionButtons();
                };
            }

            setRecordingControlsState({});

            // ── keyboard area ──
            if (auto* item = findItem("custom-keyboard")) {
                if (auto* viewport = dynamic_cast<KeyboardViewport*>(item->getComponent().get())) {
                    customKeyboardRef = &viewport->getCustomKeyboard();
                }
            }
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
        if (midiChannelMapper != nullptr) {
            midiChannelMapper->sendNoteOn(sourceChannel, midiNote, 1.0f, audioEngine.getKeyboardState());
        } else {
            audioEngine.getKeyboardState().noteOn(1, midiNote, 1.0f);
        }
        suppressTextInputMethods();
    };
    customKeyboard.onNoteOff = [this](int midiNote, int sourceChannel) {
        if (midiChannelMapper != nullptr) {
            midiChannelMapper->sendNoteOff(sourceChannel, midiNote, 1.0f, audioEngine.getKeyboardState());
        } else {
            audioEngine.getKeyboardState().noteOff(1, midiNote, 1.0f);
        }
    };
    customKeyboard.onBindingEditRequested = [this](int midiNote) {
        // Find existing binding for this note
        const auto& layout = keyboardMidiMapper.getLayout();
        auto noteName = devpiano::ui::getNoteDisplayName(midiNote, devpiano::ui::NoteDisplayMode::noteName);

        std::optional<devpiano::core::KeyBinding> existingBinding;
        for (const auto& binding : layout.bindings) {
            if (binding.action.type == devpiano::core::KeyActionType::note && binding.action.midiNote == midiNote) {
                existingBinding = binding;
                break;
            }
        }

        // Read current per-key custom label and colour
        auto currentLabel = appSettings.keyboardDisplay.customKeyLabels[static_cast<std::size_t>(midiNote)];
        auto currentColour = appSettings.keyboardDisplay.customKeyColours[static_cast<std::size_t>(midiNote)];

        KeyBindingEditDialog::launch(
            midiNote, noteName, existingBinding, currentLabel, currentColour,
            [this, midiNote](KeyBindingEditResult result) {
                // Update custom label and colour if changed
                if (result.labelChanged) {
                    appSettings.keyboardDisplay.customKeyLabels[static_cast<std::size_t>(midiNote)]
                        = result.customLabel;
                }
                if (result.colourChanged) {
                    appSettings.keyboardDisplay.customKeyColours[static_cast<std::size_t>(midiNote)]
                        = result.customColour;
                }

                // Update binding if changed
                if (result.binding.has_value()) {
                    auto updatedLayout = keyboardMidiMapper.getLayout();

                    if (result.binding->keyCode < 0) {
                        // Unbind request: remove all bindings for this note
                        std::erase_if(updatedLayout.bindings, [note = result.binding->action.midiNote](const auto& b) {
                            return b.action.type == devpiano::core::KeyActionType::note && b.action.midiNote == note;
                        });
                    } else {
                        // Update the binding in-place if the key is already
                        // mapped; otherwise append a brand-new binding (Bind Key
                        // flow for previously unbound notes).
                        bool found = false;
                        for (auto& b : updatedLayout.bindings) {
                            if (b.keyCode == result.binding->keyCode) {
                                b = *result.binding;
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            updatedLayout.bindings.push_back(*result.binding);
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
                        if (!devpiano::layout::savePreset(updatedPreset, presetFile)) {
                            DP_LOG_WARN("[Preset] failed to auto-save after binding edit: " + currentId);
                        }
                    }
                }
            },
            this);
    };
    setBounds(getInitialMainContentBounds());
}

juce::Rectangle<int> MainComponent::getMainContentResizeLimits() {
    const auto& tokens = devpiano::jive::DesignTokens::get();
    return { tokens.windowMinWidth(), tokens.windowMinHeight(), tokens.windowMaxWidth(), tokens.windowMaxHeight() };
}

juce::Rectangle<int> MainComponent::getInitialMainContentBounds() const {
    const auto limits = getMainContentResizeLimits();
    const auto savedWidth = appSettings.mainWindowWidth;
    const auto savedHeight = appSettings.mainWindowHeight;

    const auto width
        = juce::jlimit(limits.getX(), limits.getWidth(),
                       savedWidth > 0 ? savedWidth : devpiano::jive::DesignTokens::get().windowDefaultWidth());
    const auto height
        = juce::jlimit(limits.getY(), limits.getHeight(),
                       savedHeight > 0 ? savedHeight : devpiano::jive::DesignTokens::get().windowDefaultHeight());

    return { 0, 0, width, height };
}

void MainComponent::persistMainContentSize(int width, int height) {
    const auto limits = getMainContentResizeLimits();
    const auto clampedWidth = juce::jlimit(limits.getX(), limits.getWidth(), width);
    const auto clampedHeight = juce::jlimit(limits.getY(), limits.getHeight(), height);

    if (appSettings.mainWindowWidth == clampedWidth && appSettings.mainWindowHeight == clampedHeight) {
        return;
    }

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

void MainComponent::handleNoteOn(juce::MidiKeyboardState*, int, int, float velocity) {
    if (velocity > 0.0f) {
        notifyMidiActivity();
    }
}

void MainComponent::handleNoteOff(juce::MidiKeyboardState*, int, int, float) {
}

void MainComponent::timerCallback() {
    recordingSessionController->checkPlaybackEnded();

    // Decay status bar MIDI activity dot
    if (auto* dot = getStatusBarMidiDot()) {
        dot->decayFrame();
    }

    // Step status toast timer
    if (statusToastTicksRemaining > 0) {
        if (--statusToastTicksRemaining == 0) {
            statusToastText.clear();
            updateStatusBar();
        }
    }

    // Throttle status bar refresh (~2Hz, every 15 ticks at 30Hz)
    if (++statusBarThrottleCounter >= 15) {
        statusBarThrottleCounter = 0;
        updateStatusBar();
    }

    // Drain pluginBuffer safety-net resize notifications from the audio
    // callback (ERR-002): the callback only counts, logging happens here.
    if (const auto resizeCount = audioEngine.consumePluginBufferResizeCount(); resizeCount > 0) {
        DP_LOG_WARN("AudioEngine: pluginBuffer resized " + juce::String(resizeCount)
                    + " time(s) in audio callback — prepareToPlay mismatch");
    }
    // Drain preset-change notifications from playback
    {
        auto changes = recordingEngine.drainPendingPresetChanges();
        for (const auto& change : changes) {
            presetFlowSupport->applyPresetByIndex(change.presetId);
        }
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

    // NOTE: no per-panel border drawing here. The JIVE root (#window) has an
    // opaque background that covers everything MainComponent::paint draws,
    // and panel borders/outlines now come from the style sheet (see
    // style_sheets.json "border" rules + border-width on the layout nodes).
    // The gradient above only matters before the JIVE tree exists.
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
    return std::ranges::any_of(files, [](const auto& f) {
        const auto ext = juce::File(f).getFileExtension().toLowerCase();
        return ext == ".devpiano" || ext == ".mid" || ext == ".midi" || ext == ".devpiano.preset" || ext == ".vst3";
    });
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

    for (const auto& f : files) {
        const auto file = juce::File(f);
        const auto ext = file.getFileExtension().toLowerCase();

        if (ext == ".devpiano") {
            if (recordingSessionController != nullptr) {
                recordingSessionController->handleOpenPerformanceFile(file);
            }
        } else if (ext == ".mid" || ext == ".midi") {
            if (recordingSessionController != nullptr) {
                recordingSessionController->handleImportMidiFile(file);
            }
        } else if (ext == ".devpiano.preset") {
            if (presetFlowSupport != nullptr) {
                presetFlowSupport->handleImportPresetFile(file);
            }
        } else if (ext == ".vst3") {
            if (pluginOperationController != nullptr) {
                pluginOperationController->handleImportVst3File(file);
            }
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

    if (isKeyboardInputSuppressed()) {
        return false;
    }

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
        notifyMidiActivity();
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
            } else {
                DP_LOG_ERROR("[Style] design_tokens.json failed to parse: " + tokensFile.getFullPathName());
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
            } else {
                DP_LOG_ERROR("[Style] style_sheets.json failed to parse: " + styleFile.getFullPathName());
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

    if (isKeyboardInputSuppressed()) {
        return false;
    }

    const auto handled = keyboardMidiMapper.handleKeyStateChanged(audioEngine.getKeyboardState());

    if (handled) {
        getCustomKeyboard().notifyNoteActivity();
        notifyMidiActivity();
        suppressTextInputMethods();
    }
    return handled;
}

bool MainComponent::isKeyboardInputSuppressed() const noexcept {
    // Only suppress keyboard input when focus is on an actively-edited
    // text component. Sliders, buttons, comboboxes and other child
    // components that don't consume text keys should not block piano input.
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent()) {
        if (focused == this || !isParentOf(focused)) {
            return false;
        }
        if (dynamic_cast<const juce::TextEditor*>(focused) != nullptr) {
            return true;
        }
        if (const auto* label = dynamic_cast<const juce::Label*>(focused)) {
            return label->isBeingEdited();
        }
        return false;
    }
    return false;
}

bool MainComponent::shouldTakeKeyboardFocus() const noexcept {
    if (auto* mcm = juce::ModalComponentManager::getInstanceWithoutCreating();
        mcm != nullptr && mcm->getNumModalComponents() > 0) {
        return false;
    }

    if (isSettingsWindowOpen()) {
        return false;
    }

    if (pluginOperationController != nullptr && pluginOperationController->hasEditorWindowOpen()) {
        return false;
    }

    return true;
}

void MainComponent::focusGained(juce::Component::FocusChangeType cause) {
    juce::AudioAppComponent::focusGained(cause);

    if (!shouldTakeKeyboardFocus()) {
        return;
    }

    // Windows may have already given us focus via WM_SETFOCUS before grabKeyboardFocus ran,
    // causing takeKeyboardFocus's early-return check to fire. Call grabKeyboardFocus() to
    // synchronize the global state. The early-return in takeKeyboardFocus will safely fire
    // (because currentlyFocusedComponent will already be set after the first call).
    if (juce::Component::getCurrentlyFocusedComponent() != this) {
        grabKeyboardFocus();
    }
}

void MainComponent::handleWindowFocusLost() {
    // 失焦不等于要打断演奏，分两种情况处理（行为矩阵详见
    // docs/reference/features/keyboard-mapping.md）：
    // 1. 焦点转移到本进程其他顶层窗口（插件编辑器、设置窗口）——应用内部
    //    切换，键盘演奏与 MIDI 回放都继续，不做任何处理；
    // 2. 焦点真正离开应用（Alt+Tab 到其他程序）——只释放交互演奏音
    //    （电脑键盘 heldKeys + 虚拟键盘鼠标按住的音符）防悬挂。MIDI 回放
    //    由纯时间线驱动（RecordingEngine），不能调用 requestAllNotesOff：
    //    全引擎静音会杀掉回放中正在发声的音符，且回放引擎不会重新发
    //    noteOn，声音会断到时间线上的下一个音符。
    // 判定延迟到消息循环：X11 焦点切换时 FocusOut 先于 FocusIn 到达，
    // 必须等新窗口的激活状态更新后才能可靠区分内部/外部切换。
    juce::MessageManager::callAsync([weak = juce::Component::SafePointer<MainComponent>(this)] {
        if (weak == nullptr) {
            return;
        }
        for (auto index = 0; index < juce::TopLevelWindow::getNumTopLevelWindows(); ++index) {
            auto* window = juce::TopLevelWindow::getTopLevelWindow(index);
            if (window != weak->getTopLevelComponent() && window->isVisible() && window->isActiveWindow()) {
                // 应用内部窗口切换：不打断任何演奏，但需要对账 heldKeys——
                // 焦点在内部窗口（插件编辑器/设置窗口）期间松开的键收不到
                // key-up 事件（按键事件只发给聚焦组件链），滞留条目会悬挂音。
                // handleKeyStateChanged 以 OS 实时按键状态（isKeyCurrentlyDown）
                // 为准：松开的键补发 note-off，仍按住的键保持原状。
                weak->keyboardMidiMapper.handleKeyStateChanged(weak->audioEngine.getKeyboardState());
                return;
            }
        }
        weak->keyboardMidiMapper.releaseAllHeldKeys(weak->audioEngine.getKeyboardState());
        weak->getCustomKeyboard().releaseHeldMouseNote();
    });
}

void MainComponent::focusLost(juce::Component::FocusChangeType cause) {
    juce::AudioAppComponent::focusLost(cause);
    handleWindowFocusLost();
}

SettingsModel::PerformanceSettingsView MainComponent::getPerformanceSettingsFromUi() const {
    return { .masterGain = getMasterGain(),
             .adsrAttack = getAttack(),
             .adsrDecay = getDecay(),
             .adsrSustain = getSustain(),
             .adsrRelease = getRelease(),
             .builtinTone = getBuiltinToneFromUi(),
             .pianoBrightness = getPianoBrightness(),
             .pianoHammerHardness = getPianoHammerHardness(),
             .pianoResonance = getPianoResonance() };
}

juce::String MainComponent::getLastPluginNameForRecoveryStateFromUi() const {
    if (pluginHost.hasLoadedPlugin()) {
        return pluginHost.getCurrentPluginName();
    }

    auto selected = getSelectedPluginName().trim();
    if (selected.isNotEmpty()) {
        return selected;
    }

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
    setControlsPianoValues(performance.builtinTone, performance.pianoBrightness, performance.pianoHammerHardness,
                           performance.pianoResonance);
}

void MainComponent::applyPerformanceSettingsToAudioEngine(const SettingsModel::PerformanceSettingsView& performance) {
    audioEngine.setMasterGain(performance.masterGain);
    audioEngine.setAdsr(performance.adsrAttack, performance.adsrDecay, performance.adsrSustain,
                        performance.adsrRelease);
    audioEngine.setBuiltinSynthTone(performance.builtinTone == SettingsModel::BuiltinTone::piano
                                        ? AudioEngine::BuiltinSynthTone::piano
                                        : AudioEngine::BuiltinSynthTone::sine);
    audioEngine.setPianoParameters(performance.pianoBrightness, performance.pianoHammerHardness,
                                   performance.pianoResonance);
}
void MainComponent::setBuiltinSynthTone(SettingsModel::BuiltinTone tone) {
    appSettings.builtinTone = tone;
    audioEngine.setBuiltinSynthTone(tone == SettingsModel::BuiltinTone::piano ? AudioEngine::BuiltinSynthTone::piano
                                                                              : AudioEngine::BuiltinSynthTone::sine);
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
        getCustomKeyboard().setKeyboardSettings(makeKeyboardSettings(kbs, appSettings.keySignature));
        setInstrumentFilterVisible(kbs.showInstrumentFilter);
    }
    if (appSettings.keyboardScrollOffsetX >= 0) {
        setKeyboardViewPosition(-1, appSettings.keyboardScrollOffsetX);
    } else {
        setKeyboardViewPosition(24); // default: align note 24 (C1) at left edge
    }
    updateStatusBar();
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
    if (!shouldTakeKeyboardFocus()) {
        return;
    }

    if (isShowing() && juce::Component::getCurrentlyFocusedComponent() != this) {
        grabKeyboardFocus();
    }

    suppressTextInputMethods();
}

void MainComponent::initialiseAudioDevice() {
    const auto audioSettings = appSettings.getAudioSettingsView();
    const auto* savedState = (audioSettings.hasSerializedDeviceState && appSettings.audioDeviceState != nullptr)
        ? appSettings.audioDeviceState.get()
        : nullptr;

    setAudioChannels(0, 2, savedState);

    if (deviceManager.getCurrentAudioDevice() == nullptr) {
        DP_LOG_ERROR("[AudioDevice] initialiseAudioDevice: no device available after initialization");
    }

    captureAudioDeviceState();
    logCurrentAudioDeviceDiagnostics("initialiseAudioDevice");
}

void MainComponent::captureAudioDeviceState() {
    if (auto xml = deviceManager.createStateXml()) {
        appSettings.setSerializedAudioDeviceState(std::move(xml));
        return;
    }

    // JUCE v8: initialise/initialiseDefault 路径（treatAsChosenDevice=false）不更新
    // lastExplicitSettings，createStateXml() 恒为 null（仅用户经设置 UI 显式改设备后有效）。
    // 按 updateXml 同款格式手工构造，保证默认设备启动路径的设备状态可持久化/恢复。
    if (auto* device = deviceManager.getCurrentAudioDevice()) {
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        deviceManager.getAudioDeviceSetup(setup);
        appSettings.setSerializedAudioDeviceState(devpiano::audio::createDeviceStateXml(*device, setup));
    }
}

void MainComponent::prepareForAudioDeviceRebuild() {
    captureAudioDeviceState();
    if (pluginOperationController) {
        pluginOperationController->closePluginEditorWindow();
    }
    shutdownAudio();
}

void MainComponent::finishAudioDeviceRebuild() {
    initialiseAudioDevice();
    restoreKeyboardFocus();
    updateStatusBar();
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
    renderReadOnlyUiState(buildAppStateSnapshot());
}

void MainComponent::refreshPluginUiState() {
    renderReadOnlyUiState(buildAppStateSnapshot());
}

// JIVE component accessors now live in their own TU (MainComponentJiveAccessors.cpp).
