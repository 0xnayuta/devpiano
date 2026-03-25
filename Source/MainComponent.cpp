#include "MainComponent.h"

namespace
{
const auto backgroundColour = juce::Colour(0xff202225);
}

MainComponent::MainComponent()
    : keyboardComponent(audioEngine.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
{
    settingsStore.load(appSettings);

    setSize(980, 620);
    setWantsKeyboardFocus(true);

    titleLabel.setText("devpiano", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    hintLabel.setText("当前主干已具备最小插件扫描能力：可扫描 VST3 目录并列出发现的插件。", juce::dontSendNotification);
    hintLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(hintLabel);

    midiStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    addAndMakeVisible(midiStatusLabel);

    pluginStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightblue);
    addAndMakeVisible(pluginStatusLabel);

    pluginPathLabel.setText("VST3 Path", juce::dontSendNotification);
    addAndMakeVisible(pluginPathLabel);

    pluginListLabel.setText("Discovered Plugins", juce::dontSendNotification);
    addAndMakeVisible(pluginListLabel);

    addAndMakeVisible(settingsButton);
    settingsButton.onClick = [this] { showSettingsDialog(); };

    addAndMakeVisible(scanPluginsButton);
    scanPluginsButton.onClick = [this] { scanPlugins(); };

    pluginPathEditor.setMultiLine(false);
    pluginPathEditor.setReturnKeyStartsNewLine(false);
    pluginPathEditor.setText(pluginHost.getDefaultVst3SearchPath().toString(), juce::dontSendNotification);
    addAndMakeVisible(pluginPathEditor);

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

    midiRouter.setCollector(&audioEngine.getMidiCollector());
    midiRouter.openAllInputs();
    updateMidiStatusLabel();
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
    settingsWindow.reset();
    shutdownAudio();
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

    pluginListLabel.setBounds(area.removeFromTop(22));
    pluginListEditor.setBounds(area.removeFromTop(130));
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
        restoreKeyboardFocus();
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    return keyboardMidiMapper.handleKeyPressed(key, audioEngine.getKeyboardState());
}

bool MainComponent::keyStateChanged(bool isKeyDown)
{
    juce::ignoreUnused(isKeyDown);
    return keyboardMidiMapper.handleKeyStateChanged(audioEngine.getKeyboardState());
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
}

void MainComponent::restoreKeyboardFocus()
{
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<MainComponent>(this)]
    {
        if (safe != nullptr && safe->isShowing() && ! safe->hasKeyboardFocus(true))
            safe->grabKeyboardFocus();
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
    captureAudioDeviceState();
    settingsStore.scheduleSave(appSettings);
}

void MainComponent::showSettingsDialog()
{
    auto content = std::make_unique<SettingsComponent>(deviceManager);
    auto* contentPtr = content.get();
    contentPtr->onSaveRequested = [this]
    {
        captureAudioDeviceState();
        settingsStore.save(appSettings);
        if (settingsWindow != nullptr)
            settingsWindow->setVisible(false);
        settingsWindow.reset();
    };

    settingsWindow = std::make_unique<juce::DialogWindow>("Audio Settings", backgroundColour, true);
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
    pluginStatusLabel.setText(text, juce::dontSendNotification);
}

void MainComponent::scanPlugins()
{
    const auto text = pluginPathEditor.getText().trim();
    const auto path = text.isNotEmpty() ? juce::FileSearchPath(text)
                                        : pluginHost.getDefaultVst3SearchPath();

    pluginHost.scanVst3Plugins(path, true);
    pluginListEditor.setText(pluginHost.getPluginListDescription(), juce::dontSendNotification);
    updatePluginStatusLabel();
    restoreKeyboardFocus();
}
