#include "MainComponent.h"

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
    : keyboardComponent(audioEngine.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
{
    settingsStore.load(appSettings);
    keyboardMidiMapper.setLayout(SettingsModel::keyMapToLayout(appSettings.keyMap));
    appSettings.keyMap = SettingsModel::layoutToKeyMap(keyboardMidiMapper.getLayout());
    audioEngine.setPluginHost(&pluginHost);

    setSize(980, 620);
    setWantsKeyboardFocus(true);

    titleLabel.setText("devpiano", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    hintLabel.setText("VST3 scan/load is ready: scan, select a plugin, then click Load.", juce::dontSendNotification);
    hintLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(hintLabel);

    midiStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    addAndMakeVisible(midiStatusLabel);

    pluginStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightblue);
    addAndMakeVisible(pluginStatusLabel);

    pluginPathLabel.setText("VST3 Path", juce::dontSendNotification);
    addAndMakeVisible(pluginPathLabel);

    pluginSelectionLabel.setText("Plugin", juce::dontSendNotification);
    addAndMakeVisible(pluginSelectionLabel);

    pluginListLabel.setText("Discovered Plugins", juce::dontSendNotification);
    addAndMakeVisible(pluginListLabel);

    addAndMakeVisible(settingsButton);
    settingsButton.onClick = [this] { showSettingsDialog(); };

    addAndMakeVisible(scanPluginsButton);
    scanPluginsButton.onClick = [this] { scanPlugins(); };

    addAndMakeVisible(loadPluginButton);
    loadPluginButton.onClick = [this] { loadSelectedPlugin(); };

    addAndMakeVisible(unloadPluginButton);
    unloadPluginButton.onClick = [this] { unloadCurrentPlugin(); };

    addAndMakeVisible(openEditorButton);
    openEditorButton.onClick = [this] { togglePluginEditor(); };

    pluginPathEditor.setMultiLine(false);
    pluginPathEditor.setReturnKeyStartsNewLine(false);
    pluginPathEditor.setText(pluginHost.getDefaultVst3SearchPath().toString(), juce::dontSendNotification);
    addAndMakeVisible(pluginPathEditor);

    pluginSelector.setTextWhenNothingSelected("Select a scanned plugin...");
    pluginSelector.setWantsKeyboardFocus(false);
    addAndMakeVisible(pluginSelector);

    pluginListEditor.setMultiLine(true);
    pluginListEditor.setReadOnly(true);
    pluginListEditor.setScrollbarsShown(true);
    pluginListEditor.setCaretVisible(false);
    pluginListEditor.setPopupMenuEnabled(true);
    pluginListEditor.setWantsKeyboardFocus(false);
    pluginListEditor.setMouseClickGrabsKeyboardFocus(false);
    addAndMakeVisible(pluginListEditor);

    configureSlider(volumeSlider, volumeLabel, "Volume", 0.0, 1.0, appSettings.masterGain);
    configureSlider(attackSlider, attackLabel, "Attack", 0.001, 2.0, appSettings.adsrAttack);
    configureSlider(decaySlider, decayLabel, "Decay", 0.001, 2.0, appSettings.adsrDecay);
    configureSlider(sustainSlider, sustainLabel, "Sustain", 0.0, 1.0, appSettings.adsrSustain);
    configureSlider(releaseSlider, releaseLabel, "Release", 0.001, 3.0, appSettings.adsrRelease);

    addAndMakeVisible(keyboardComponent);
    keyboardComponent.setAvailableRange(24, 96);
    keyboardComponent.setKeyWidth(24.0f);
    keyboardComponent.setScrollButtonsVisible(true);
    keyboardComponent.setWantsKeyboardFocus(false);
    keyboardComponent.setMouseClickGrabsKeyboardFocus(false);

    syncUiFromSettings();
    applyUiStateToAudioEngine();

    initialiseAudioDevice();
    suppressTextInputMethods();

    midiRouter.setCollector(&audioEngine.getMidiCollector());
    midiRouter.openAllInputs();
    updateMidiStatusLabel();
    updatePluginSelectionList();
    updatePluginStatusLabel();
    pluginListEditor.setText(pluginHost.getPluginListDescription(), juce::dontSendNotification);

    restoreKeyboardFocus();
}

MainComponent::~MainComponent()
{
    syncSettingsFromUi();
    captureAudioDeviceState();
    settingsStore.save(appSettings);

    midiRouter.closeInputs();

    shutdownAudio();
    pluginEditorWindow.reset();
    pluginHost.unloadPlugin();
    settingsWindow.reset();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    audioEngine.prepareToPlay(samplesPerBlockExpected, sampleRate);
    appSettings.sampleRate = sampleRate;
    appSettings.bufferSize = samplesPerBlockExpected;
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

    auto topRow = area.removeFromTop(30);
    titleLabel.setBounds(topRow.removeFromLeft(180));
    settingsButton.setBounds(topRow.removeFromRight(110));

    hintLabel.setBounds(area.removeFromTop(24));
    midiStatusLabel.setBounds(area.removeFromTop(22));
    pluginStatusLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(10);

    auto pathRow = area.removeFromTop(28);
    pluginPathLabel.setBounds(pathRow.removeFromLeft(80));
    scanPluginsButton.setBounds(pathRow.removeFromRight(120));
    pluginPathEditor.setBounds(pathRow.reduced(6, 0));

    area.removeFromTop(8);

    auto selectorRow = area.removeFromTop(28);
    pluginSelectionLabel.setBounds(selectorRow.removeFromLeft(80));
    openEditorButton.setBounds(selectorRow.removeFromRight(110));
    unloadPluginButton.setBounds(selectorRow.removeFromRight(90));
    loadPluginButton.setBounds(selectorRow.removeFromRight(90));
    pluginSelector.setBounds(selectorRow.reduced(6, 0));

    area.removeFromTop(8);

    pluginListLabel.setBounds(area.removeFromTop(22));
    pluginListEditor.setBounds(area.removeFromTop(110));
    area.removeFromTop(12);

    const auto rowHeight = 28;
    auto layoutSliderRow = [&](juce::Slider& slider, juce::Label& label)
    {
        auto row = area.removeFromTop(rowHeight);
        label.setBounds(row.removeFromLeft(80));
        slider.setBounds(row);
        area.removeFromTop(8);
    };

    layoutSliderRow(volumeSlider, volumeLabel);
    layoutSliderRow(attackSlider, attackLabel);
    layoutSliderRow(decaySlider, decayLabel);
    layoutSliderRow(sustainSlider, sustainLabel);
    layoutSliderRow(releaseSlider, releaseLabel);

    area.removeFromTop(8);
    keyboardComponent.setBounds(area.removeFromBottom(110));
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

void MainComponent::configureSlider(juce::Slider& slider,
                                    juce::Label& label,
                                    const juce::String& text,
                                    double minimum,
                                    double maximum,
                                    double initialValue,
                                    double interval)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(label);

    slider.setRange(minimum, maximum, interval);
    slider.setValue(initialValue, juce::dontSendNotification);
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 22);
    slider.onValueChange = [this]
    {
        applyUiStateToAudioEngine();
        syncSettingsFromUi();
        saveSettingsSoon();
    };
    addAndMakeVisible(slider);
}

void MainComponent::applyUiStateToAudioEngine()
{
    audioEngine.setMasterGain(static_cast<float>(volumeSlider.getValue()));
    audioEngine.setAdsr(static_cast<float>(attackSlider.getValue()),
                        static_cast<float>(decaySlider.getValue()),
                        static_cast<float>(sustainSlider.getValue()),
                        static_cast<float>(releaseSlider.getValue()));
}

void MainComponent::syncUiFromSettings()
{
    volumeSlider.setValue(appSettings.masterGain, juce::dontSendNotification);
    attackSlider.setValue(appSettings.adsrAttack, juce::dontSendNotification);
    decaySlider.setValue(appSettings.adsrDecay, juce::dontSendNotification);
    sustainSlider.setValue(appSettings.adsrSustain, juce::dontSendNotification);
    releaseSlider.setValue(appSettings.adsrRelease, juce::dontSendNotification);
}

void MainComponent::syncSettingsFromUi()
{
    appSettings.masterGain = static_cast<float>(volumeSlider.getValue());
    appSettings.adsrAttack = static_cast<float>(attackSlider.getValue());
    appSettings.adsrDecay = static_cast<float>(decaySlider.getValue());
    appSettings.adsrSustain = static_cast<float>(sustainSlider.getValue());
    appSettings.adsrRelease = static_cast<float>(releaseSlider.getValue());
    appSettings.keyMap = SettingsModel::layoutToKeyMap(keyboardMidiMapper.getLayout());
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

    if (appSettings.audioDeviceState != nullptr)
        deviceManager.initialise(0, 2, appSettings.audioDeviceState.get(), true);

    captureAudioDeviceState();
}

void MainComponent::captureAudioDeviceState()
{
    if (auto xml = deviceManager.createStateXml())
        appSettings.audioDeviceState = std::move(xml);
}

void MainComponent::saveSettingsSoon()
{
    syncSettingsFromUi();
    captureAudioDeviceState();
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
        if (safe == nullptr)
            return;

        auto shouldSave = false;
        if (safe->settingsWindow != nullptr)
            if (auto* settingsContent = dynamic_cast<SettingsComponent*>(safe->settingsWindow->getContentComponent()))
                shouldSave = settingsContent->isDirty();

        if (shouldSave)
        {
            safe->captureAudioDeviceState();
            safe->settingsStore.save(safe->appSettings);
        }

        juce::MessageManager::callAsync([safe]
        {
            if (safe == nullptr)
                return;

            if (safe->settingsWindow != nullptr)
                safe->settingsWindow->setVisible(false);

            safe->settingsWindow.reset();
            safe->restoreKeyboardFocus();
        });
    };

    contentPtr->onSaveRequested = [safe = juce::Component::SafePointer<MainComponent>(this), closeWindow]
    {
        if (safe != nullptr)
        {
            safe->captureAudioDeviceState();
            safe->settingsStore.save(safe->appSettings);
        }

        closeWindow();
    };

    settingsWindow = std::make_unique<SettingsDialogWindow>("Audio Settings", backgroundColour, closeWindow);
    settingsWindow->setUsingNativeTitleBar(true);
    settingsWindow->setContentOwned(content.release(), true);
    settingsWindow->centreAroundComponent(this, 620, 420);
    settingsWindow->setResizable(true, true);
    settingsWindow->setVisible(true);
}

void MainComponent::updateMidiStatusLabel()
{
    midiStatusLabel.setText("MIDI Inputs: " + juce::String(midiRouter.getOpenInputCount()),
                            juce::dontSendNotification);
}

void MainComponent::updatePluginStatusLabel()
{
    auto text = pluginHost.getAvailableFormatsDescription();
    if (pluginHost.supportsVst3())
        text << " [VST3 ready]";

    text << " | " << pluginHost.getLastScanSummary();

    if (pluginHost.hasLoadedPlugin())
    {
        text << " | Loaded: " << pluginHost.getCurrentPluginName();

        if (pluginHost.isPrepared())
            text << " @ " << juce::String(pluginHost.getPreparedSampleRate(), 0)
                 << " Hz / " << juce::String(pluginHost.getPreparedBlockSize());
        else
            text << " [not prepared]";
    }
    else if (const auto error = pluginHost.getLastLoadError(); error.isNotEmpty() && error != "No plugin load attempted yet.")
    {
        text << " | Load error: " << error;
    }

    pluginStatusLabel.setText(text, juce::dontSendNotification);
}

void MainComponent::updatePluginSelectionList()
{
    const auto names = pluginHost.getKnownPluginNames();

    pluginSelector.clear(juce::dontSendNotification);

    auto itemId = 1;
    for (const auto& name : names)
        pluginSelector.addItem(name, itemId++);

    if (names.isEmpty())
        pluginSelector.setSelectedItemIndex(-1, juce::dontSendNotification);
    else if (pluginHost.hasLoadedPlugin())
        pluginSelector.setText(pluginHost.getCurrentPluginName(), juce::dontSendNotification);
    else
        pluginSelector.setSelectedItemIndex(0, juce::dontSendNotification);

    loadPluginButton.setEnabled(! names.isEmpty());
    unloadPluginButton.setEnabled(pluginHost.hasLoadedPlugin());
    openEditorButton.setEnabled(pluginHost.hasLoadedPlugin());
    openEditorButton.setButtonText(pluginEditorWindow != nullptr ? "Close Editor" : "Open Editor");
}

double MainComponent::getCurrentRuntimeSampleRate() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const auto rate = device->getCurrentSampleRate();
        if (rate > 0.0)
            return rate;
    }

    return appSettings.sampleRate;
}

int MainComponent::getCurrentRuntimeBlockSize() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const auto size = device->getCurrentBufferSizeSamples();
        if (size > 0)
            return size;
    }

    return appSettings.bufferSize;
}

void MainComponent::loadSelectedPlugin()
{
    const auto pluginName = pluginSelector.getText().trim();
    if (pluginName.isEmpty())
    {
        updatePluginStatusLabel();
        restoreKeyboardFocus();
        return;
    }

    const auto sampleRate = getCurrentRuntimeSampleRate();
    const auto blockSize = getCurrentRuntimeBlockSize();

    captureAudioDeviceState();
    shutdownAudio();
    pluginEditorWindow.reset();

    const auto success = pluginHost.loadPluginByName(pluginName, sampleRate, blockSize);
    juce::ignoreUnused(success);
    initialiseAudioDevice();

    updatePluginSelectionList();
    updatePluginStatusLabel();
    restoreKeyboardFocus();
}

void MainComponent::unloadCurrentPlugin()
{
    pluginEditorWindow.reset();
    captureAudioDeviceState();
    shutdownAudio();
    pluginHost.unloadPlugin();
    initialiseAudioDevice();
    updatePluginSelectionList();
    updatePluginStatusLabel();
    restoreKeyboardFocus();
}

void MainComponent::togglePluginEditor()
{
    if (pluginEditorWindow != nullptr)
    {
        pluginEditorWindow.reset();
        updatePluginSelectionList();
        restoreKeyboardFocus();
        return;
    }

    auto* instance = pluginHost.getInstance();
    if (instance == nullptr || ! instance->hasEditor())
    {
        updatePluginStatusLabel();
        restoreKeyboardFocus();
        return;
    }

    auto editor = std::unique_ptr<juce::AudioProcessorEditor>(instance->createEditorAndMakeActive());
    if (editor == nullptr)
    {
        updatePluginStatusLabel();
        restoreKeyboardFocus();
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
            safe->updatePluginSelectionList();
            safe->restoreKeyboardFocus();
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
    updatePluginSelectionList();
}

void MainComponent::scanPlugins()
{
    const auto text = pluginPathEditor.getText().trim();
    const auto path = text.isNotEmpty() ? juce::FileSearchPath(text)
                                        : pluginHost.getDefaultVst3SearchPath();

    pluginHost.scanVst3Plugins(path, true);
    pluginListEditor.setText(pluginHost.getPluginListDescription(), juce::dontSendNotification);
    updatePluginSelectionList();
    updatePluginStatusLabel();
    restoreKeyboardFocus();
}
