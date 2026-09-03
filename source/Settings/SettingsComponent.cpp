#include "Settings/SettingsComponent.h"

#include "Audio/AudioDeviceDiagnostics.h"
#include "Locale/LocaleManager.h"
#include "Settings/jive/SettingsLayoutModel.h"
#include "UI/jive/DesignTokens.h"
#include "UI/jive/JiveUtils.h"
#include "UI/jive/StyleCatalog.h"

#include <cmath>

namespace {
using namespace devpiano::ui::jive;
} // namespace

SettingsComponent::SettingsComponent(juce::AudioDeviceManager& dm, const juce::XmlElement* savedAudioDeviceState,
                                     SettingsModel* displayModel)
    : deviceManager(dm)
    , model(displayModel) {
    setOpaque(true);
    if (savedAudioDeviceState != nullptr) {
        savedStateSnapshot = std::make_unique<juce::XmlElement>(*savedAudioDeviceState);
    }

    buildJiveUi();
    viewport.setViewedComponent(viewHost.getRootComponent(), false);
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

SettingsComponent::~SettingsComponent() {
    deviceManager.removeChangeListener(this);
    viewport.setViewedComponent(nullptr, false);
    viewHost.reset();
}

void SettingsComponent::buildJiveUi() {
    viewport.setViewedComponent(nullptr, false);
    viewHost.reset();

    viewHost.registerDefaultComponents();

    auto layoutTree = devpiano::ui::jive::makeSettingsLayoutTree();
    if (viewHost.loadLayout(layoutTree, true)) {
        // Find audio component references
        audioDeviceTypeCombo = viewHost.find<juce::ComboBox>("audio-device-type-combo");
        audioOutputDeviceCombo = viewHost.find<juce::ComboBox>("audio-output-device-combo");
        audioActiveChannelsCombo = viewHost.find<juce::ComboBox>("audio-active-channels-combo");
        audioTestButton = viewHost.find<juce::Button>("audio-test-button");
        audioSampleRateCombo = viewHost.find<juce::ComboBox>("audio-sample-rate-combo");
        audioBufferSizeCombo = viewHost.find<juce::ComboBox>("audio-buffer-size-combo");
        asioControlPanelButton = viewHost.find<juce::Button>("asio-control-panel-button");

        // Find other settings references
        keySignatureCombo = viewHost.find<juce::ComboBox>("key-signature-combo");
        midiTransposeToggle = viewHost.find<juce::ToggleButton>("midi-transpose-toggle");
        colourModeCombo = viewHost.find<juce::ComboBox>("colour-mode-combo");
        noteDisplayCombo = viewHost.find<juce::ComboBox>("note-display-combo");
        fadeSpeedSlider = viewHost.find<juce::Slider>("fade-speed-slider");
        instrumentFilterToggle = viewHost.find<juce::ToggleButton>("instrument-filter-toggle");
        languageCombo = viewHost.find<juce::ComboBox>("language-combo");
        diagnosticsEditor = viewHost.find<juce::TextEditor>("diagnostics-editor");
        saveButton = viewHost.find<juce::Button>("save-button");

        for (int ch = 0; ch < 16; ++ch) {
            followKeyToggles[static_cast<size_t>(ch)]
                = viewHost.find<juce::ToggleButton>("follow-key-" + juce::String(ch));
        }

        wireAudioControls();
        wireMidiControls();
        wireAppearanceAndLocaleControls();
        syncEditingStateFromModel();
    }
}

void SettingsComponent::wireAudioControls() {
    const auto wireComboChange = [this](juce::ComboBox* combo, const std::function<void(int)>& handler) {
        if (combo != nullptr) {
            combo->onChange = [this, combo, handler] {
                if (!isUpdatingAudioControls) {
                    handler(combo->getSelectedId());
                }
            };
        }
    };

    wireComboChange(audioDeviceTypeCombo, [this](int selId) {
        const auto& types = deviceManager.getAvailableDeviceTypes();
        if (selId >= 1 && selId <= types.size()) {
            const auto newType = types[selId - 1]->getTypeName();
            if (newType != deviceManager.getCurrentAudioDeviceType()) {
                deviceManager.setCurrentAudioDeviceType(newType, true);
                refreshAllAudioControls();
                setDirty(true);
            }
        }
    });

    wireComboChange(audioOutputDeviceCombo, [this](int selId) {
        auto setup = deviceManager.getAudioDeviceSetup();
        setup.outputDeviceName = (selId < 0) ? juce::String() : audioOutputDeviceCombo->getText();
        setup.useDefaultOutputChannels = true;
        deviceManager.setAudioDeviceSetup(setup, true);
        populateAudioActiveChannels();
        populateAudioSampleRates();
        populateAudioBufferSizes();
        setDirty(true);
    });

    wireComboChange(audioActiveChannelsCombo, [this](int selPairId) {
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
    });

    if (audioTestButton != nullptr) {
        audioTestButton->onClick = [this] { deviceManager.playTestSound(); };
    }

    wireComboChange(audioSampleRateCombo, [this](int selRate) {
        if (selRate > 0) {
            auto setup = deviceManager.getAudioDeviceSetup();
            setup.sampleRate = selRate;
            deviceManager.setAudioDeviceSetup(setup, true);
            populateAudioBufferSizes();
            setDirty(true);
        }
    });

    wireComboChange(audioBufferSizeCombo, [this](int selBs) {
        if (selBs > 0) {
            auto setup = deviceManager.getAudioDeviceSetup();
            setup.bufferSize = selBs;
            deviceManager.setAudioDeviceSetup(setup, true);
            setDirty(true);
        }
    });

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
}

void SettingsComponent::wireMidiControls() {
    if (keySignatureCombo != nullptr) {
        rebuildKeySignatureCombo();
        if (model != nullptr) {
            keySignatureCombo->setSelectedId(keySignatureToComboId(model->keySignature), juce::dontSendNotification);
        }
        keySignatureCombo->onChange = [this] {
            auto ks = comboKeyMapping[static_cast<size_t>(keySignatureCombo->getSelectedId())];
            editingState.setProperty("keySignature", ks, nullptr);
        };
    }

    if (midiTransposeToggle != nullptr) {
        midiTransposeToggle->setToggleable(true);
        midiTransposeToggle->setClickingTogglesState(true);
        if (model != nullptr) {
            midiTransposeToggle->setToggleState(model->midiTranspose, juce::dontSendNotification);
        }
        midiTransposeToggle->onStateChange
            = [this] { editingState.setProperty("midiTranspose", midiTransposeToggle->getToggleState(), nullptr); };
    }

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
            tb->onStateChange = [this, ch] {
                editingState.setProperty("followKey_" + juce::String(ch),
                                         followKeyToggles[static_cast<size_t>(ch)]->getToggleState(), nullptr);
            };
        }
    }
    updateFollowKeyTogglesEnablement();
}

void SettingsComponent::wireAppearanceAndLocaleControls() {
    if (colourModeCombo != nullptr) {
        rebuildColourModeCombo();
        if (model != nullptr) {
            colourModeCombo->setSelectedId(1 + static_cast<int>(model->keyboardDisplay.colourMode),
                                           juce::dontSendNotification);
        }
        colourModeCombo->onChange
            = [this] { editingState.setProperty("colourMode", colourModeCombo->getSelectedId(), nullptr); };
    }

    if (noteDisplayCombo != nullptr) {
        rebuildNoteDisplayCombo();
        if (model != nullptr) {
            noteDisplayCombo->setSelectedId(1 + static_cast<int>(model->keyboardDisplay.noteDisplay),
                                            juce::dontSendNotification);
        }
        noteDisplayCombo->onChange
            = [this] { editingState.setProperty("noteDisplay", noteDisplayCombo->getSelectedId(), nullptr); };
    }

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

    if (instrumentFilterToggle != nullptr) {
        if (model != nullptr) {
            instrumentFilterToggle->setToggleState(model->keyboardDisplay.showInstrumentFilter,
                                                   juce::dontSendNotification);
        }
        instrumentFilterToggle->onStateChange = [this] {
            editingState.setProperty("showInstrumentFilter", instrumentFilterToggle->getToggleState(), nullptr);
        };
    }

    if (languageCombo != nullptr) {
        languageCombo->clear(juce::dontSendNotification);
        languageCombo->addItem("English", 1);
        languageCombo->addItem(devpiano::locale::languageDisplayName(devpiano::locale::Language::zhCN), 2);
        if (model != nullptr) {
            languageCombo->setSelectedId(model->languageCode == "zh-CN" ? 2 : 1, juce::dontSendNotification);
        }
        languageCombo->onChange = [this] {
            editingState.setProperty("languageCode",
                                     languageCombo->getSelectedId() == 2 ? juce::String("zh-CN") : juce::String("en"),
                                     nullptr);
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
}

void SettingsComponent::syncEditingStateFromModel() {
    if (model == nullptr) {
        return;
    }

    editingState.removeListener(this);
    if (colourModeCombo != nullptr) {
        editingState.setProperty("colourMode", colourModeCombo->getSelectedId(), nullptr);
    }
    if (noteDisplayCombo != nullptr) {
        editingState.setProperty("noteDisplay", noteDisplayCombo->getSelectedId(), nullptr);
    }
    if (fadeSpeedSlider != nullptr) {
        editingState.setProperty("fadeSpeed", static_cast<double>(model->keyboardDisplay.fadeSpeed), nullptr);
    }
    if (instrumentFilterToggle != nullptr) {
        editingState.setProperty("showInstrumentFilter", model->keyboardDisplay.showInstrumentFilter, nullptr);
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

void SettingsComponent::rebuildColourModeCombo() {
    if (colourModeCombo == nullptr) {
        return;
    }
    colourModeCombo->clear(juce::dontSendNotification);
    colourModeCombo->addItem(TRANS("Classic"), 1 + static_cast<int>(devpiano::ui::KeyColourMode::classic));
    colourModeCombo->addItem(TRANS("Channel"), 1 + static_cast<int>(devpiano::ui::KeyColourMode::channel));
    colourModeCombo->addItem(TRANS("Velocity"), 1 + static_cast<int>(devpiano::ui::KeyColourMode::velocity));
}

void SettingsComponent::rebuildNoteDisplayCombo() {
    if (noteDisplayCombo == nullptr) {
        return;
    }
    noteDisplayCombo->clear(juce::dontSendNotification);
    noteDisplayCombo->addItem(TRANS("Do Re Mi"), 1 + static_cast<int>(devpiano::ui::NoteDisplayMode::doReMi));
    noteDisplayCombo->addItem(TRANS("Fixed Do"), 1 + static_cast<int>(devpiano::ui::NoteDisplayMode::fixedDo));
    noteDisplayCombo->addItem(TRANS("Note Name"), 1 + static_cast<int>(devpiano::ui::NoteDisplayMode::noteName));
}

void SettingsComponent::rebuildKeySignatureCombo() {
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

void SettingsComponent::refreshTexts() {
    const auto savedViewPos = viewport.getViewPosition();
    buildJiveUi();
    if (viewport.getViewedComponent() != viewHost.getRootComponent()) {
        viewport.setViewedComponent(viewHost.getRootComponent(), false);
    }
    resized();
    updateDiagnostics();
    viewport.setViewPosition(savedViewPos);
    if (onRefreshTexts) {
        onRefreshTexts();
    }
}

void SettingsComponent::paint(juce::Graphics& g) {
    g.fillAll(devpiano::jive::DesignTokens::get().mainBg());
}

void SettingsComponent::resized() {
    viewport.setBounds(getLocalBounds());
    updateContentBounds();
}

void SettingsComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
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

void SettingsComponent::populateAudioDeviceTypes() {
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

void SettingsComponent::populateAudioOutputDevices() {
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

void SettingsComponent::populateAudioActiveChannels() {
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

void SettingsComponent::populateAudioSampleRates() {
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

void SettingsComponent::populateAudioBufferSizes() {
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

void SettingsComponent::updateAsioControlPanelVisibility() {
    const bool isAsio = deviceManager.getCurrentAudioDeviceType().containsIgnoreCase("ASIO");
    viewHost.setVisible("asio-control-panel-row", isAsio);
    viewHost.setProperty("asio-control-panel-row", "height", isAsio ? 28 : 0);
    viewHost.setProperty("asio-control-panel-row", "max-height", isAsio ? 28 : 0);
    viewHost.setProperty("asio-control-panel-row", "margin", isAsio ? "0 0 6 0" : "0 0 0 0");

    if (asioControlPanelButton != nullptr) {
        asioControlPanelButton->setVisible(isAsio);
    }
}

void SettingsComponent::refreshAllAudioControls() {
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

int SettingsComponent::keySignatureToComboId(int ks) {
    for (int id = 1; id <= 12; ++id) {
        if (comboKeyMapping[static_cast<size_t>(id)] == ks) {
            return id;
        }
    }
    return 1; // default C
}

void SettingsComponent::updateContentBounds() {
    if (viewHost.isValid()) {
        const auto previousViewPos = viewport.getViewPosition();
        const auto availableWidth = viewport.getMaximumVisibleWidth();
        const auto contentWidth = juce::jmax(640, availableWidth);
        const auto contentHeight = calculateSettingsContentHeight();
        if (auto* rootComp = viewHost.getRootComponent()) {
            rootComp->setSize(contentWidth, contentHeight);
        }
        viewport.setViewPosition(previousViewPos);
    }
}

void SettingsComponent::updateFollowKeyTogglesEnablement() {
    const auto enabled = midiTransposeToggle != nullptr && midiTransposeToggle->getToggleState();
    viewHost.setEnabled("channel-follow-key-area", enabled);
    for (int ch = 0; ch < 16; ++ch) {
        if (auto* tb = followKeyToggles[static_cast<size_t>(ch)]) {
            tb->setEnabled(enabled);
        }
    }
}

void SettingsComponent::updateDiagnostics() {
    if (diagnosticsEditor != nullptr) {
        const auto diagnostics = devpiano::audio::buildAudioDeviceDiagnostics(savedStateSnapshot.get(), deviceManager);
        diagnosticsEditor->setText(diagnostics.detailedSummary, juce::dontSendNotification);
    }
}

void SettingsComponent::changeListenerCallback(juce::ChangeBroadcaster* source) {
    if (source == &deviceManager) {
        dirty = true;
        refreshAllAudioControls();
        updateDiagnostics();
    }
}

void SettingsComponent::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& prop) {
    if (tree != editingState || model == nullptr) {
        return;
    }

    const auto& propName = prop.toString();

    if (propName == "colourMode") {
        model->keyboardDisplay.colourMode = static_cast<devpiano::ui::KeyColourMode>((int)editingState[prop] - 1);
    } else if (propName == "noteDisplay") {
        model->keyboardDisplay.noteDisplay = static_cast<devpiano::ui::NoteDisplayMode>((int)editingState[prop] - 1);
    } else if (propName == "fadeSpeed") {
        model->keyboardDisplay.fadeSpeed = static_cast<float>((double)editingState[prop]);
    } else if (propName == "showInstrumentFilter") {
        model->keyboardDisplay.showInstrumentFilter = (bool)editingState[prop];
    } else if (propName == "keySignature") {
        model->keySignature = (int)editingState[prop];
    } else if (propName == "midiTranspose") {
        model->midiTranspose = (bool)editingState[prop];
        updateFollowKeyTogglesEnablement();
    } else if (propName.startsWith("followKey_")) {
        const auto chIdx = propName.substring(10).getIntValue();
        if (chIdx >= 0 && chIdx < 16) {
            model->channelMatrix.channels[static_cast<size_t>(chIdx)].followKey = (bool)editingState[prop];
        }
    } else if (propName == "languageCode") {
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
