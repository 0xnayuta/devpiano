#pragma once

#include "Audio/AudioDeviceDiagnostics.h"
#include "Locale/LocaleManager.h"
#include "Settings/SettingsModel.h"
#include "Settings/jive/SettingsLayoutModel.h"
#include "UI/jive/DesignTokens.h"
#include "UI/jive/JiveUtils.h"
#include "UI/jive/StyleCatalog.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace {
using namespace devpiano::ui::jive;
} // namespace
// ============================================================================
/// Settings Window Content Component
///
/// Refactored in Phase 15-C to use JIVE's declarative layout model
/// (SettingsLayoutModel) and CSS Grid for 16-channel follow key toggles,
/// eliminating 300+ lines of manual setBounds/removeFromTop calculations.
// ============================================================================
class SettingsComponent : public juce::Component, private juce::ChangeListener, public juce::ValueTree::Listener {
public:
    explicit SettingsComponent(juce::AudioDeviceManager& dm, const juce::XmlElement* savedAudioDeviceState,
                               SettingsModel* displayModel = nullptr)
        : deviceManager(dm)
        , model(displayModel) {
        setOpaque(true);
        if (savedAudioDeviceState != nullptr) {
            savedStateSnapshot = std::make_unique<juce::XmlElement>(*savedAudioDeviceState);
        }

        buildJiveUi();

        viewport.setViewedComponent(jiveRootItem != nullptr ? jiveRootItem->getComponent().get() : nullptr, false);
        viewport.setScrollBarsShown(true, false, true, false);
        viewport.setSingleStepSizes(0, 20);
        viewport.addMouseListener(this, true);
        addAndMakeVisible(viewport);

        setSize(680, 720);
        updateContentBounds();
        deviceManager.addChangeListener(this);

        refreshAllAudioControls();
        updateDiagnostics();
    }

    ~SettingsComponent() override {
        deviceManager.removeChangeListener(this);
        safeCleanupJiveTree(jiveRootItem);
        interpreter.reset();
    }

    void buildJiveUi() {
        safeCleanupJiveTree(jiveRootItem);
        interpreter = std::make_unique<::jive::Interpreter>();
        auto& factory = interpreter->getComponentFactory();

        factory.set("ListEditor", [] {
            auto ed = std::make_unique<juce::TextEditor>();
            ed->setMultiLine(true);
            ed->setReadOnly(true);
            ed->setScrollbarsShown(true);
            ed->setCaretVisible(false);
            ed->setPopupMenuEnabled(true);
            ed->setWantsKeyboardFocus(false);
            ed->setMouseClickGrabsKeyboardFocus(false);
            return ed;
        });

        auto layoutTree = devpiano::ui::jive::makeSettingsLayoutTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(layoutTree);

        jiveRootItem = interpreter->interpret(layoutTree);
        jassert(jiveRootItem != nullptr);

        if (jiveRootItem != nullptr) {
            // Find audio component references
            audioDeviceTypeCombo
                = dynamic_cast<juce::ComboBox*>(findComponentById(*jiveRootItem, "audio-device-type-combo"));
            audioOutputDeviceCombo
                = dynamic_cast<juce::ComboBox*>(findComponentById(*jiveRootItem, "audio-output-device-combo"));
            audioActiveChannelsCombo
                = dynamic_cast<juce::ComboBox*>(findComponentById(*jiveRootItem, "audio-active-channels-combo"));
            audioTestButton = dynamic_cast<juce::Button*>(findComponentById(*jiveRootItem, "audio-test-button"));
            audioSampleRateCombo
                = dynamic_cast<juce::ComboBox*>(findComponentById(*jiveRootItem, "audio-sample-rate-combo"));
            audioBufferSizeCombo
                = dynamic_cast<juce::ComboBox*>(findComponentById(*jiveRootItem, "audio-buffer-size-combo"));
            asioControlPanelButton
                = dynamic_cast<juce::Button*>(findComponentById(*jiveRootItem, "asio-control-panel-button"));
            asioControlPanelRowItem = devpiano::ui::jive::findGuiItemById(*jiveRootItem, "asio-control-panel-row");
            // Find other settings references
            keySignatureCombo = dynamic_cast<juce::ComboBox*>(findComponentById(*jiveRootItem, "key-signature-combo"));
            midiTransposeToggle
                = dynamic_cast<juce::ToggleButton*>(findComponentById(*jiveRootItem, "midi-transpose-toggle"));
            colourModeCombo = dynamic_cast<juce::ComboBox*>(findComponentById(*jiveRootItem, "colour-mode-combo"));
            noteDisplayCombo = dynamic_cast<juce::ComboBox*>(findComponentById(*jiveRootItem, "note-display-combo"));
            fadeSpeedSlider = dynamic_cast<juce::Slider*>(findComponentById(*jiveRootItem, "fade-speed-slider"));
            instrumentFilterToggle
                = dynamic_cast<juce::ToggleButton*>(findComponentById(*jiveRootItem, "instrument-filter-toggle"));
            languageCombo = dynamic_cast<juce::ComboBox*>(findComponentById(*jiveRootItem, "language-combo"));
            diagnosticsEditor = dynamic_cast<juce::TextEditor*>(findComponentById(*jiveRootItem, "diagnostics-editor"));
            saveButton = dynamic_cast<juce::Button*>(findComponentById(*jiveRootItem, "save-button"));
            followKeyAreaItem = devpiano::ui::jive::findGuiItemById(*jiveRootItem, "channel-follow-key-area");

            for (int ch = 0; ch < 16; ++ch) {
                followKeyToggles[static_cast<size_t>(ch)] = dynamic_cast<juce::ToggleButton*>(
                    findComponentById(*jiveRootItem, "follow-key-" + juce::String(ch)));
            }

            // Wire Audio Device Type combo
            if (audioDeviceTypeCombo != nullptr) {
                audioDeviceTypeCombo->onChange = [this] {
                    if (isUpdatingAudioControls) {
                        return;
                    }
                    const auto selId = audioDeviceTypeCombo->getSelectedId();
                    const auto& types = deviceManager.getAvailableDeviceTypes();
                    if (selId >= 1 && selId <= types.size()) {
                        const auto newType = types[selId - 1]->getTypeName();
                        if (newType != deviceManager.getCurrentAudioDeviceType()) {
                            deviceManager.setCurrentAudioDeviceType(newType, true);
                            refreshAllAudioControls();
                            setDirty(true);
                        }
                    }
                };
            }

            // Wire Audio Output Device combo
            if (audioOutputDeviceCombo != nullptr) {
                audioOutputDeviceCombo->onChange = [this] {
                    if (isUpdatingAudioControls) {
                        return;
                    }
                    auto setup = deviceManager.getAudioDeviceSetup();
                    const int selId = audioOutputDeviceCombo->getSelectedId();
                    setup.outputDeviceName = (selId < 0) ? juce::String() : audioOutputDeviceCombo->getText();
                    setup.useDefaultOutputChannels = true;
                    deviceManager.setAudioDeviceSetup(setup, true);
                    populateAudioActiveChannels();
                    populateAudioSampleRates();
                    populateAudioBufferSizes();
                    setDirty(true);
                };
            }

            // Wire Audio Active Output Channels combo
            if (audioActiveChannelsCombo != nullptr) {
                audioActiveChannelsCombo->onChange = [this] {
                    if (isUpdatingAudioControls) {
                        return;
                    }
                    const int selPairId = audioActiveChannelsCombo->getSelectedId();
                    if (selPairId > 0) {
                        auto setup = deviceManager.getAudioDeviceSetup();
                        setup.useDefaultOutputChannels = false;
                        setup.outputChannels.clear();
                        const int chStart = (selPairId - 1) * 2;
                        setup.outputChannels.setBit(chStart, true);
                        setup.outputChannels.setBit(chStart + 1, true);
                        deviceManager.setAudioDeviceSetup(setup, true);
                        setDirty(true);
                    }
                };
            }

            // Wire Audio Test button
            if (audioTestButton != nullptr) {
                audioTestButton->onClick = [this] { deviceManager.playTestSound(); };
            }

            // Wire Audio Sample Rate combo
            if (audioSampleRateCombo != nullptr) {
                audioSampleRateCombo->onChange = [this] {
                    if (isUpdatingAudioControls) {
                        return;
                    }
                    const int selRate = audioSampleRateCombo->getSelectedId();
                    if (selRate > 0) {
                        auto setup = deviceManager.getAudioDeviceSetup();
                        setup.sampleRate = selRate;
                        deviceManager.setAudioDeviceSetup(setup, true);
                        populateAudioBufferSizes();
                        setDirty(true);
                    }
                };
            }

            // Wire Audio Buffer Size combo
            if (audioBufferSizeCombo != nullptr) {
                audioBufferSizeCombo->onChange = [this] {
                    if (isUpdatingAudioControls) {
                        return;
                    }
                    const int selBs = audioBufferSizeCombo->getSelectedId();
                    if (selBs > 0) {
                        auto setup = deviceManager.getAudioDeviceSetup();
                        setup.bufferSize = selBs;
                        deviceManager.setAudioDeviceSetup(setup, true);
                        setDirty(true);
                    }
                };
            }

            // Wire ASIO Control Panel button
            if (asioControlPanelButton != nullptr) {
                asioControlPanelButton->onClick = [this] {
                    if (auto* currentDevice = deviceManager.getCurrentAudioDevice()) {
                        if (currentDevice->hasControlPanel()) {
                            currentDevice->showControlPanel();
                        }
                    }
                };
            }

            refreshAllAudioControls();

            // Populate & wire Key Signature
            if (keySignatureCombo != nullptr) {
                rebuildKeySignatureCombo();
                if (model != nullptr) {
                    keySignatureCombo->setSelectedId(keySignatureToComboId(model->keySignature),
                                                     juce::dontSendNotification);
                }
                keySignatureCombo->onChange = [this] {
                    auto ks = comboKeyMapping[static_cast<size_t>(keySignatureCombo->getSelectedId())];
                    editingState.setProperty("keySignature", ks, nullptr);
                };
            }

            // Wire MIDI Transpose toggle
            if (midiTransposeToggle != nullptr) {
                midiTransposeToggle->setToggleable(true);
                midiTransposeToggle->setClickingTogglesState(true);
                if (model != nullptr) {
                    midiTransposeToggle->setToggleState(model->midiTranspose, juce::dontSendNotification);
                }
                midiTransposeToggle->onClick = [this] {
                    editingState.setProperty("midiTranspose", midiTransposeToggle->getToggleState(), nullptr);
                };
                midiTransposeToggle->onStateChange = [this] {
                    editingState.setProperty("midiTranspose", midiTransposeToggle->getToggleState(), nullptr);
                };
            }

            // Wire 16 Channel Follow Key toggles
            for (int ch = 0; ch < 16; ++ch) {
                if (auto* tb = followKeyToggles[static_cast<size_t>(ch)]) {
                    tb->setToggleable(true);
                    tb->setClickingTogglesState(true);
                    tb->setButtonText("Ch" + juce::String(ch + 1));
                    tb->setTooltip(TRANS("Follow Key"));
                    if (model != nullptr) {
                        tb->setToggleState(model->channelMatrix.channels[static_cast<size_t>(ch)].followKey,
                                           juce::dontSendNotification);
                    }
                    tb->onClick = [this, ch] {
                        editingState.setProperty("followKey_" + juce::String(ch),
                                                 followKeyToggles[static_cast<size_t>(ch)]->getToggleState(), nullptr);
                    };
                    tb->onStateChange = [this, ch] {
                        editingState.setProperty("followKey_" + juce::String(ch),
                                                 followKeyToggles[static_cast<size_t>(ch)]->getToggleState(), nullptr);
                    };
                }
            }
            updateFollowKeyTogglesEnablement();

            // Populate & wire Colour Mode
            if (colourModeCombo != nullptr) {
                rebuildColourModeCombo();
                if (model != nullptr) {
                    colourModeCombo->setSelectedId(1 + static_cast<int>(model->keyboardDisplay.colourMode),
                                                   juce::dontSendNotification);
                }
                colourModeCombo->onChange
                    = [this] { editingState.setProperty("colourMode", colourModeCombo->getSelectedId(), nullptr); };
            }

            // Populate & wire Note Display
            if (noteDisplayCombo != nullptr) {
                rebuildNoteDisplayCombo();
                if (model != nullptr) {
                    noteDisplayCombo->setSelectedId(1 + static_cast<int>(model->keyboardDisplay.noteDisplay),
                                                    juce::dontSendNotification);
                }
                noteDisplayCombo->onChange
                    = [this] { editingState.setProperty("noteDisplay", noteDisplayCombo->getSelectedId(), nullptr); };
            }

            // Configure & wire Fade Speed Slider
            if (fadeSpeedSlider != nullptr) {
                fadeSpeedSlider->setRange(0.50, 1.00, 0.01);
                fadeSpeedSlider->setSliderStyle(juce::Slider::LinearHorizontal);
                fadeSpeedSlider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
                if (model != nullptr) {
                    fadeSpeedSlider->setValue(model->keyboardDisplay.fadeSpeed, juce::dontSendNotification);
                }
                fadeSpeedSlider->onValueChange
                    = [this] { editingState.setProperty("fadeSpeed", fadeSpeedSlider->getValue(), nullptr); };
            }

            // Wire Instrument Filter Toggle
            if (instrumentFilterToggle != nullptr) {
                if (model != nullptr) {
                    instrumentFilterToggle->setToggleState(model->keyboardDisplay.showInstrumentFilter,
                                                           juce::dontSendNotification);
                }
                instrumentFilterToggle->onStateChange = [this] {
                    editingState.setProperty("showInstrumentFilter", instrumentFilterToggle->getToggleState(), nullptr);
                };
            }

            // Populate & wire Language Combo
            if (languageCombo != nullptr) {
                languageCombo->clear(juce::dontSendNotification);
                languageCombo->addItem("English", 1);
                languageCombo->addItem(devpiano::locale::languageDisplayName(devpiano::locale::Language::zhCN), 2);
                if (model != nullptr) {
                    languageCombo->setSelectedId(model->languageCode == "zh-CN" ? 2 : 1, juce::dontSendNotification);
                }
                languageCombo->onChange = [this] {
                    editingState.setProperty(
                        "languageCode",
                        languageCombo->getSelectedId() == 2 ? juce::String("zh-CN") : juce::String("en"), nullptr);
                };
            }
            if (saveButton != nullptr) {
                saveButton->onClick = [this] {
                    dirty = false;
                    if (onSaveRequested) {
                        onSaveRequested();
                    }
                };
            }

            // Load model into editingState
            if (model != nullptr) {
                editingState.removeListener(this);
                if (colourModeCombo) {
                    editingState.setProperty("colourMode", colourModeCombo->getSelectedId(), nullptr);
                }
                if (noteDisplayCombo) {
                    editingState.setProperty("noteDisplay", noteDisplayCombo->getSelectedId(), nullptr);
                }
                if (fadeSpeedSlider) {
                    editingState.setProperty("fadeSpeed", static_cast<double>(model->keyboardDisplay.fadeSpeed),
                                             nullptr);
                }
                if (instrumentFilterToggle) {
                    editingState.setProperty("showInstrumentFilter", model->keyboardDisplay.showInstrumentFilter,
                                             nullptr);
                }
                editingState.setProperty("languageCode", model->languageCode, nullptr);
                editingState.setProperty("keySignature", model->keySignature, nullptr);
                editingState.setProperty("midiTranspose", model->midiTranspose, nullptr);
                for (int ch = 0; ch < 16; ++ch) {
                    editingState.setProperty("followKey_" + juce::String(ch),
                                             model->channelMatrix.channels[static_cast<size_t>(ch)].followKey, nullptr);
                }
                editingState.addListener(this);
            }
        }
    }

    void rebuildColourModeCombo() {
        if (colourModeCombo == nullptr) {
            return;
        }
        colourModeCombo->clear(juce::dontSendNotification);
        colourModeCombo->addItem(TRANS("Classic"), 1 + static_cast<int>(devpiano::ui::KeyColourMode::classic));
        colourModeCombo->addItem(TRANS("Channel"), 1 + static_cast<int>(devpiano::ui::KeyColourMode::channel));
        colourModeCombo->addItem(TRANS("Velocity"), 1 + static_cast<int>(devpiano::ui::KeyColourMode::velocity));
    }

    void rebuildNoteDisplayCombo() {
        if (noteDisplayCombo == nullptr) {
            return;
        }
        noteDisplayCombo->clear(juce::dontSendNotification);
        noteDisplayCombo->addItem(TRANS("Do Re Mi"), 1 + static_cast<int>(devpiano::ui::NoteDisplayMode::doReMi));
        noteDisplayCombo->addItem(TRANS("Fixed Do"), 1 + static_cast<int>(devpiano::ui::NoteDisplayMode::fixedDo));
        noteDisplayCombo->addItem(TRANS("Note Name"), 1 + static_cast<int>(devpiano::ui::NoteDisplayMode::noteName));
    }

    void rebuildKeySignatureCombo() {
        if (keySignatureCombo == nullptr) {
            return;
        }
        keySignatureCombo->clear(juce::dontSendNotification);
        keySignatureCombo->addItem("C", 1);
        keySignatureCombo->addItem("C# / Db", 2);
        keySignatureCombo->addItem("D", 3);
        keySignatureCombo->addItem("D# / Eb", 4);
        keySignatureCombo->addItem("E", 5);
        keySignatureCombo->addItem("F", 6);
        keySignatureCombo->addItem("F# / Gb", 7);
        keySignatureCombo->addItem("G", 8);
        keySignatureCombo->addItem("G# / Ab", 9);
        keySignatureCombo->addItem("A", 10);
        keySignatureCombo->addItem("A# / Bb", 11);
        keySignatureCombo->addItem("B", 12);
    }

    void refreshTexts() {
        const auto savedViewPos = viewport.getViewPosition();
        buildJiveUi();
        if (viewport.getViewedComponent() != (jiveRootItem != nullptr ? jiveRootItem->getComponent().get() : nullptr)) {
            viewport.setViewedComponent(jiveRootItem != nullptr ? jiveRootItem->getComponent().get() : nullptr, false);
        }
        resized();
        updateDiagnostics();
        viewport.setViewPosition(savedViewPos);
        if (onRefreshTexts) {
            onRefreshTexts();
        }
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(devpiano::jive::DesignTokens::get().mainBg());
    }

    void resized() override {
        viewport.setBounds(getLocalBounds());
        updateContentBounds();
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override {
        // Isolate wheel events originating from interactive controls that handle their
        // own value changes or scrolling (Slider, ComboBox, TextEditor) so that adjusting
        // them does not simultaneously scroll the parent Viewport.
        if (e.eventComponent != nullptr) {
            if (dynamic_cast<const juce::Slider*>(e.eventComponent) != nullptr
                || e.eventComponent->findParentComponentOfClass<juce::Slider>() != nullptr
                || dynamic_cast<const juce::ComboBox*>(e.eventComponent) != nullptr
                || e.eventComponent->findParentComponentOfClass<juce::ComboBox>() != nullptr
                || dynamic_cast<const juce::TextEditor*>(e.eventComponent) != nullptr) {
                return;
            }
        }
        viewport.useMouseWheelMoveIfNeeded(e, wheel);
    }

    bool isDirty() const noexcept {
        return dirty;
    }
    void setDirty(bool d) noexcept {
        dirty = d;
    }

    std::function<void()> onSaveRequested;
    std::function<void()> onDisplaySettingsChanged;
    std::function<void(const juce::String&)> onLanguageChanged;
    std::function<void()> onRefreshTexts;

private:
    juce::AudioDeviceManager& deviceManager;
    SettingsModel* model;

    juce::Viewport viewport;
    std::unique_ptr<::jive::Interpreter> interpreter;
    std::unique_ptr<::jive::GuiItem> jiveRootItem;

    juce::ComboBox* audioDeviceTypeCombo = nullptr;
    juce::ComboBox* audioOutputDeviceCombo = nullptr;
    juce::ComboBox* audioActiveChannelsCombo = nullptr;
    juce::Button* audioTestButton = nullptr;
    juce::ComboBox* audioSampleRateCombo = nullptr;
    juce::ComboBox* audioBufferSizeCombo = nullptr;
    juce::Button* asioControlPanelButton = nullptr;
    ::jive::GuiItem* asioControlPanelRowItem = nullptr;
    bool isUpdatingAudioControls = false;

    juce::ComboBox* keySignatureCombo = nullptr;
    juce::ToggleButton* midiTransposeToggle = nullptr;
    std::array<juce::ToggleButton*, 16> followKeyToggles {};

    juce::ComboBox* colourModeCombo = nullptr;
    juce::ComboBox* noteDisplayCombo = nullptr;
    juce::Slider* fadeSpeedSlider = nullptr;
    juce::ToggleButton* instrumentFilterToggle = nullptr;
    juce::ComboBox* languageCombo = nullptr;
    juce::TextEditor* diagnosticsEditor = nullptr;
    juce::Button* saveButton = nullptr;
    ::jive::GuiItem* followKeyAreaItem = nullptr;

    void populateAudioDeviceTypes() {
        if (audioDeviceTypeCombo == nullptr) {
            return;
        }
        audioDeviceTypeCombo->clear(juce::dontSendNotification);
        const auto& types = deviceManager.getAvailableDeviceTypes();
        for (int i = 0; i < types.size(); ++i) {
            audioDeviceTypeCombo->addItem(types[i]->getTypeName(), i + 1);
        }
        const auto currentType = deviceManager.getCurrentAudioDeviceType();
        for (int i = 0; i < types.size(); ++i) {
            if (types[i]->getTypeName() == currentType) {
                audioDeviceTypeCombo->setSelectedId(i + 1, juce::dontSendNotification);
                break;
            }
        }
    }

    void populateAudioOutputDevices() {
        if (audioOutputDeviceCombo == nullptr) {
            return;
        }
        audioOutputDeviceCombo->clear(juce::dontSendNotification);

        auto* currentTypeObj = deviceManager.getCurrentDeviceTypeObject();
        if (currentTypeObj == nullptr) {
            return;
        }

        currentTypeObj->scanForDevices();
        auto deviceNames = currentTypeObj->getDeviceNames(false); // false = output devices

        audioOutputDeviceCombo->addItem("<< " + TRANS("none") + " >>", -1);

        for (int i = 0; i < deviceNames.size(); ++i) {
            audioOutputDeviceCombo->addItem(deviceNames[i], i + 1);
        }

        auto setup = deviceManager.getAudioDeviceSetup();
        if (setup.outputDeviceName.isEmpty()) {
            audioOutputDeviceCombo->setSelectedId(-1, juce::dontSendNotification);
        } else {
            const int index = deviceNames.indexOf(setup.outputDeviceName);
            if (index >= 0) {
                audioOutputDeviceCombo->setSelectedId(index + 1, juce::dontSendNotification);
            } else if (deviceNames.size() > 0) {
                audioOutputDeviceCombo->setSelectedId(1, juce::dontSendNotification);
            }
        }
        populateAudioActiveChannels();
    }

    void populateAudioActiveChannels() {
        if (audioActiveChannelsCombo == nullptr) {
            return;
        }
        audioActiveChannelsCombo->clear(juce::dontSendNotification);

        auto* currentDevice = deviceManager.getCurrentAudioDevice();
        if (currentDevice == nullptr) {
            return;
        }

        auto channelNames = currentDevice->getOutputChannelNames();
        if (channelNames.isEmpty()) {
            audioActiveChannelsCombo->addItem(TRANS("(no audio output channels found)"), -1);
            audioActiveChannelsCombo->setSelectedId(-1, juce::dontSendNotification);
            return;
        }

        // Build stereo pairs (1+2, 3+4, ...)
        int pairId = 1;
        for (int i = 0; i < channelNames.size(); i += 2) {
            juce::String pairName;
            if (i + 1 < channelNames.size()) {
                pairName = channelNames[i].trim() + " + " + channelNames[i + 1].trim();
            } else {
                pairName = channelNames[i].trim();
            }
            audioActiveChannelsCombo->addItem(pairName, pairId);
            ++pairId;
        }

        auto setup = deviceManager.getAudioDeviceSetup();
        int activePairId = 1;
        for (int i = 0; i < channelNames.size(); i += 2) {
            if (setup.outputChannels[i] || (i + 1 < channelNames.size() && setup.outputChannels[i + 1])) {
                activePairId = (i / 2) + 1;
                break;
            }
        }
        audioActiveChannelsCombo->setSelectedId(activePairId, juce::dontSendNotification);
    }
    void populateAudioSampleRates() {
        if (audioSampleRateCombo == nullptr) {
            return;
        }
        audioSampleRateCombo->clear(juce::dontSendNotification);

        auto* currentDevice = deviceManager.getCurrentAudioDevice();
        if (currentDevice == nullptr) {
            return;
        }

        auto rates = currentDevice->getAvailableSampleRates();
        for (auto r : rates) {
            const int intRate = juce::roundToInt(r);
            audioSampleRateCombo->addItem(juce::String(intRate) + " Hz", intRate);
        }

        const int currentRate = juce::roundToInt(currentDevice->getCurrentSampleRate());
        audioSampleRateCombo->setSelectedId(currentRate, juce::dontSendNotification);
    }

    void populateAudioBufferSizes() {
        if (audioBufferSizeCombo == nullptr) {
            return;
        }
        audioBufferSizeCombo->clear(juce::dontSendNotification);

        auto* currentDevice = deviceManager.getCurrentAudioDevice();
        if (currentDevice == nullptr) {
            return;
        }

        const double currentRate
            = currentDevice->getCurrentSampleRate() > 0.0 ? currentDevice->getCurrentSampleRate() : 48000.0;
        auto bufferSizes = currentDevice->getAvailableBufferSizes();

        for (auto bs : bufferSizes) {
            const double ms = static_cast<double>(bs) * 1000.0 / currentRate;
            const auto text = juce::String(bs) + " samples (" + juce::String(ms, 1) + " ms)";
            audioBufferSizeCombo->addItem(text, bs);
        }

        const int currentBs = currentDevice->getCurrentBufferSizeSamples();
        audioBufferSizeCombo->setSelectedId(currentBs, juce::dontSendNotification);
    }

    void updateAsioControlPanelVisibility() {
        const bool isAsio = deviceManager.getCurrentAudioDeviceType().containsIgnoreCase("ASIO");
        if (asioControlPanelRowItem != nullptr) {
            asioControlPanelRowItem->state.setProperty("visibility", isAsio, nullptr);
            asioControlPanelRowItem->state.setProperty("height", isAsio ? 28 : 0, nullptr);
            asioControlPanelRowItem->state.setProperty("max-height", isAsio ? 28 : 0, nullptr);
            asioControlPanelRowItem->state.setProperty("margin", isAsio ? "0 0 6 0" : "0 0 0 0", nullptr);
            if (auto comp = asioControlPanelRowItem->getComponent()) {
                comp->setVisible(isAsio);
            }
        }
        if (asioControlPanelButton != nullptr) {
            asioControlPanelButton->setVisible(isAsio);
        }
    }

    void refreshAllAudioControls() {
        if (isUpdatingAudioControls) {
            return;
        }
        const juce::ScopedValueSetter<bool> updatingSetter(isUpdatingAudioControls, true);

        populateAudioDeviceTypes();
        populateAudioOutputDevices();
        populateAudioActiveChannels();
        populateAudioSampleRates();
        populateAudioBufferSizes();
        updateAsioControlPanelVisibility();
    }
    static constexpr std::array<int, 13> comboKeyMapping { 0, 0, 1, 2, 3, 4, 5, 6, -5, -4, -3, -2, -1 };

    [[nodiscard]] static int keySignatureToComboId(int ks) {
        for (int id = 1; id <= 12; ++id) {
            if (comboKeyMapping[static_cast<size_t>(id)] == ks) {
                return id;
            }
        }
        return 1; // default C
    }

    std::unique_ptr<juce::XmlElement> savedStateSnapshot;
    bool dirty = false;
    juce::ValueTree editingState { "Settings" };

    [[nodiscard]] int calculateSettingsContentHeight() const {
        return 960;
    }

    void updateContentBounds() {
        if (jiveRootItem != nullptr) {
            const auto previousViewPos = viewport.getViewPosition();
            const auto availableWidth = viewport.getMaximumVisibleWidth();
            const auto contentWidth = juce::jmax(640, availableWidth);
            const auto contentHeight = calculateSettingsContentHeight();
            if (auto rootComp = jiveRootItem->getComponent()) {
                rootComp->setSize(contentWidth, contentHeight);
            }
            viewport.setViewPosition(previousViewPos);
        }
    }

    void updateFollowKeyTogglesEnablement() {
        const auto enabled = midiTransposeToggle != nullptr && midiTransposeToggle->getToggleState();
        if (followKeyAreaItem != nullptr) {
            if (auto comp = followKeyAreaItem->getComponent()) {
                comp->setEnabled(enabled);
            }
        }
        for (int ch = 0; ch < 16; ++ch) {
            if (auto* tb = followKeyToggles[static_cast<size_t>(ch)]) {
                tb->setEnabled(enabled);
            }
        }
    }
    void updateDiagnostics() {
        if (diagnosticsEditor != nullptr) {
            const auto diagnostics
                = devpiano::audio::buildAudioDeviceDiagnostics(savedStateSnapshot.get(), deviceManager);
            diagnosticsEditor->setText(diagnostics.detailedSummary, juce::dontSendNotification);
        }
    }

    void changeListenerCallback(juce::ChangeBroadcaster* source) override {
        if (source == &deviceManager) {
            dirty = true;
            refreshAllAudioControls();
            updateDiagnostics();
        }
    }

    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& prop) override {
        if (tree != editingState || !model) {
            return;
        }

        if (prop == juce::Identifier("colourMode")) {
            model->keyboardDisplay.colourMode = static_cast<devpiano::ui::KeyColourMode>((int)editingState[prop] - 1);
        } else if (prop == juce::Identifier("noteDisplay")) {
            model->keyboardDisplay.noteDisplay
                = static_cast<devpiano::ui::NoteDisplayMode>((int)editingState[prop] - 1);
        } else if (prop == juce::Identifier("fadeSpeed")) {
            model->keyboardDisplay.fadeSpeed = static_cast<float>((double)editingState[prop]);
        } else if (prop == juce::Identifier("showInstrumentFilter")) {
            model->keyboardDisplay.showInstrumentFilter = (bool)editingState[prop];
        } else if (prop == juce::Identifier("keySignature")) {
            model->keySignature = (int)editingState[prop];
        } else if (prop == juce::Identifier("midiTranspose")) {
            model->midiTranspose = (bool)editingState[prop];
            updateFollowKeyTogglesEnablement();
        } else if (prop.toString().startsWith("followKey_")) {
            auto chIdx = prop.toString().substring(10).getIntValue();
            if (chIdx >= 0 && chIdx < 16) {
                model->channelMatrix.channels[static_cast<size_t>(chIdx)].followKey = (bool)editingState[prop];
            }
        } else if (prop == juce::Identifier("languageCode")) {
            model->languageCode = editingState[prop].toString();
            if (onLanguageChanged) {
                onLanguageChanged(model->languageCode);
            }
            refreshTexts();
            return;
        }

        setDirty(true);
        if (onDisplaySettingsChanged) {
            onDisplaySettingsChanged();
        }
    }
};
