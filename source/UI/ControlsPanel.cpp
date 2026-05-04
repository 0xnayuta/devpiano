#include "ControlsPanel.h"

juce::String ControlsPanel::makeLayoutDisplayName(const juce::String& layoutId)
{
    if (layoutId == "default.freepiano.minimal")
        return "FreePiano Minimal";

    if (layoutId == "default.freepiano.full")
        return "FreePiano Full";

    return layoutId;
}

ControlsPanel::ControlsPanel()
{
    configureSlider(volumeSlider, volumeLabel, "Volume", 0.0, 1.0);
    configureSlider(attackSlider, attackLabel, "Attack", 0.001, 2.0);
    configureSlider(decaySlider, decayLabel, "Decay", 0.001, 2.0);
    configureSlider(sustainSlider, sustainLabel, "Sustain", 0.0, 1.0);
    configureSlider(releaseSlider, releaseLabel, "Release", 0.001, 3.0);

    layoutLabel.setText("Layout", juce::dontSendNotification);
    layoutLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(layoutLabel);

    addAndMakeVisible(layoutComboBox);
    layoutComboBox.setTextWhenNothingSelected("FreePiano Minimal");
    layoutComboBox.onChange = [this]
    {
        updateLayoutActionButtons();

        const auto selectedId = layoutComboBox.getSelectedId();
        if (selectedId <= 0 || ! onLayoutChanged)
            return;

        const auto index = selectedId - 1;
        if (! juce::isPositiveAndBelow(index, availableLayoutIds.size()))
            return;

        onLayoutChanged(availableLayoutIds[index]);
    };
    layoutComboBox.addMouseListener(this, true);

    addAndMakeVisible(saveLayoutButton);
    saveLayoutButton.onClick = [this]
    {
        if (onSaveLayoutRequested)
            onSaveLayoutRequested();
    };

    addAndMakeVisible(resetLayoutButton);
    resetLayoutButton.onClick = [this]
    {
        if (onResetLayoutRequested)
            onResetLayoutRequested();
    };

    addAndMakeVisible(importLayoutButton);
    importLayoutButton.onClick = [this]
    {
        if (onImportLayoutRequested)
            onImportLayoutRequested();
    };

    addAndMakeVisible(renameLayoutButton);
    renameLayoutButton.onClick = [this]
    {
        if (onRenameLayoutRequested)
            onRenameLayoutRequested();
    };

    addAndMakeVisible(deleteLayoutButton);
    deleteLayoutButton.onClick = [this]
    {
        if (onDeleteLayoutRequested)
            onDeleteLayoutRequested();
    };

    recordStatusLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(recordStatusLabel);

    addAndMakeVisible(recordButton);
    recordButton.onClick = [this]
    {
        if (onRecordClicked)
            onRecordClicked();
    };

    addAndMakeVisible(playButton);
    playButton.onClick = [this]
    {
        if (onPlayClicked)
            onPlayClicked();
    };

    addAndMakeVisible(stopButton);
    stopButton.onClick = [this]
    {
        if (onStopClicked)
            onStopClicked();
    };

    addAndMakeVisible(backToStartButton);
    backToStartButton.onClick = [this]
    {
        if (onBackToStartClicked)
            onBackToStartClicked();
    };

    addAndMakeVisible(exportMidiButton);
    exportMidiButton.onClick = [this]
    {
        if (onExportMidiClicked)
            onExportMidiClicked();
    };

    addAndMakeVisible(exportWavButton);
    exportWavButton.onClick = [this]
    {
        if (onExportWavClicked)
            onExportWavClicked();
    };

    addAndMakeVisible(importMidiButton);
    importMidiButton.onClick = [this]
    {
        if (onImportMidiClicked)
            onImportMidiClicked();
    };

    addAndMakeVisible(savePerformanceButton);
    savePerformanceButton.onClick = [this]
    {
        if (onSavePerformanceClicked)
            onSavePerformanceClicked();
    };

    addAndMakeVisible(openPerformanceButton);
    openPerformanceButton.onClick = [this]
    {
        if (onOpenPerformanceClicked)
            onOpenPerformanceClicked();
    };

    playbackSpeedLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(playbackSpeedLabel);

    addAndMakeVisible(speedDownButton);
    speedDownButton.onClick = [this]
    {
        if (! onPlaybackSpeedChange)
            return;
        const auto current = currentPlaybackSpeed;
        if (current <= 0.5)
            return;
        const auto speeds = std::array { 0.5, 0.75, 1.0, 1.25, 1.5, 2.0 };
        for (std::size_t i = speeds.size(); i-- > 0; )
        {
            if (speeds[i] < current)
            {
                onPlaybackSpeedChange(speeds[i]);
                break;
            }
        }
    };

    addAndMakeVisible(speedUpButton);
    speedUpButton.onClick = [this]
    {
        if (! onPlaybackSpeedChange)
            return;
        const auto current = currentPlaybackSpeed;
        if (current >= 2.0)
            return;
        const auto speeds = std::array { 0.5, 0.75, 1.0, 1.25, 1.5, 2.0 };
        for (const auto speed : speeds)
        {
            if (speed > current)
            {
                onPlaybackSpeedChange(speed);
                break;
            }
        }
    };

    playbackSpeedValueLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(playbackSpeedValueLabel);

    setRecordingControlsState({});

    updateLayoutActionButtons();
}

ControlsPanel::~ControlsPanel()
{
    volumeSlider.onValueChange = nullptr;
    attackSlider.onValueChange = nullptr;
    decaySlider.onValueChange = nullptr;
    sustainSlider.onValueChange = nullptr;
    releaseSlider.onValueChange = nullptr;
    layoutComboBox.onChange = nullptr;
    saveLayoutButton.onClick = nullptr;
    resetLayoutButton.onClick = nullptr;
    importLayoutButton.onClick = nullptr;
    renameLayoutButton.onClick = nullptr;
    deleteLayoutButton.onClick = nullptr;
    recordButton.onClick = nullptr;
    playButton.onClick = nullptr;
    stopButton.onClick = nullptr;
    backToStartButton.onClick = nullptr;
    exportMidiButton.onClick = nullptr;
    exportWavButton.onClick = nullptr;
    importMidiButton.onClick = nullptr;
    savePerformanceButton.onClick = nullptr;
    openPerformanceButton.onClick = nullptr;
    speedDownButton.onClick = nullptr;
    speedUpButton.onClick = nullptr;
}

void ControlsPanel::resized()
{
    auto area = getLocalBounds();
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

    auto row = area.removeFromTop(rowHeight);
    layoutLabel.setBounds(row.removeFromLeft(80));
    layoutComboBox.setBounds(row.removeFromLeft(200));
    row.removeFromLeft(8);
    saveLayoutButton.setBounds(row.removeFromLeft(90));
    row.removeFromLeft(8);
    resetLayoutButton.setBounds(row.removeFromLeft(60));
    row.removeFromLeft(8);
    importLayoutButton.setBounds(row.removeFromLeft(60));
    row.removeFromLeft(8);
    renameLayoutButton.setBounds(row.removeFromLeft(70));
    row.removeFromLeft(8);
    deleteLayoutButton.setBounds(row.removeFromLeft(60));

    area.removeFromTop(12);

    auto buttonRow = area.removeFromTop(rowHeight);
    recordStatusLabel.setBounds(buttonRow.removeFromLeft(80));
    buttonRow.removeFromLeft(6);
    savePerformanceButton.setBounds(buttonRow.removeFromLeft(50));
    buttonRow.removeFromLeft(6);
    openPerformanceButton.setBounds(buttonRow.removeFromLeft(50));
    buttonRow.removeFromLeft(6);
    recordButton.setBounds(buttonRow.removeFromLeft(60));
    buttonRow.removeFromLeft(6);
    playButton.setBounds(buttonRow.removeFromLeft(50));
    buttonRow.removeFromLeft(6);
    stopButton.setBounds(buttonRow.removeFromLeft(50));
    buttonRow.removeFromLeft(6);
    backToStartButton.setBounds(buttonRow.removeFromLeft(60));
    buttonRow.removeFromLeft(6);
    importMidiButton.setBounds(buttonRow.removeFromLeft(90));
    buttonRow.removeFromLeft(6);
    exportMidiButton.setBounds(buttonRow.removeFromLeft(90));
    buttonRow.removeFromLeft(6);
    exportWavButton.setBounds(buttonRow.removeFromLeft(90));
    buttonRow.removeFromLeft(10);
    playbackSpeedLabel.setBounds(buttonRow.removeFromLeft(50));
    buttonRow.removeFromLeft(4);
    speedDownButton.setBounds(buttonRow.removeFromLeft(30));
    buttonRow.removeFromLeft(2);
    playbackSpeedValueLabel.setBounds(buttonRow.removeFromLeft(50));
    buttonRow.removeFromLeft(2);
    speedUpButton.setBounds(buttonRow.removeFromLeft(30));
}

void ControlsPanel::setRecordingControlsState(RecordingControlsState state)
{
    recordingControlsState = state;

    updateRecordingActionButtons();
}

void ControlsPanel::updateRecordingActionButtons()
{
    juce::String statusText;
    auto recordEnabled = true;
    auto playEnabled = recordingControlsState.hasTake;
    auto stopEnabled = false;
    auto backToStartEnabled = recordingControlsState.hasTake;
    auto exportMidiEnabled = recordingControlsState.hasTake && recordingControlsState.canExportMidiTake;
    auto exportWavEnabled = recordingControlsState.hasTake && recordingControlsState.canExportWavTake;
    auto importMidiEnabled = true;
    auto saveEnabled = false;
    auto openEnabled = false;
    auto speedDownEnabled = true;
    auto speedUpEnabled = true;

    switch (recordingControlsState.state)
    {
        case RecordingState::idle:
            statusText = "Idle";
            playEnabled = recordingControlsState.hasTake;
            backToStartEnabled = recordingControlsState.hasTake;
            exportMidiEnabled = recordingControlsState.hasTake && recordingControlsState.canExportMidiTake;
            exportWavEnabled = recordingControlsState.hasTake && recordingControlsState.canExportWavTake;
            importMidiEnabled = true;
            saveEnabled = recordingControlsState.hasTake;
            openEnabled = true;
            break;
        case RecordingState::recording:
            statusText = "Recording";
            recordEnabled = false;
            playEnabled = false;
            backToStartEnabled = false;
            exportMidiEnabled = false;
            exportWavEnabled = false;
            importMidiEnabled = false;
            stopEnabled = true;
            saveEnabled = false;
            openEnabled = false;
            speedDownEnabled = false;
            speedUpEnabled = false;
            break;
        case RecordingState::playing:
            statusText = "Playing";
            recordEnabled = false;
            playEnabled = false;
            backToStartEnabled = recordingControlsState.hasTake;
            exportMidiEnabled = false;
            exportWavEnabled = false;
            importMidiEnabled = false;
            stopEnabled = true;
            saveEnabled = false;
            openEnabled = false;
            break;
    }

    recordStatusLabel.setText(statusText, juce::dontSendNotification);
    recordButton.setEnabled(recordEnabled);
    playButton.setEnabled(playEnabled);
    stopButton.setEnabled(stopEnabled);
    backToStartButton.setEnabled(backToStartEnabled);
    importMidiButton.setEnabled(importMidiEnabled);
    exportMidiButton.setEnabled(exportMidiEnabled);
    exportWavButton.setEnabled(exportWavEnabled);
    savePerformanceButton.setEnabled(saveEnabled);
    openPerformanceButton.setEnabled(openEnabled);
    speedDownButton.setEnabled(speedDownEnabled);
    speedUpButton.setEnabled(speedUpEnabled);
}

void ControlsPanel::setValues(float masterGain,
                              float attack,
                              float decay,
                              float sustain,
                              float release)
{
    volumeSlider.setValue(masterGain, juce::dontSendNotification);
    attackSlider.setValue(attack, juce::dontSendNotification);
    decaySlider.setValue(decay, juce::dontSendNotification);
    sustainSlider.setValue(sustain, juce::dontSendNotification);
    releaseSlider.setValue(release, juce::dontSendNotification);
}

void ControlsPanel::setLayouts(const juce::StringArray& layoutIds,
                               const juce::String& currentLayoutId,
                               const juce::StringArray& layoutDisplayNames)
{
    availableLayoutIds = layoutIds;

    layoutComboBox.clear(juce::dontSendNotification);
    for (int i = 0; i < layoutIds.size(); ++i)
    {
        auto displayName = (i < layoutDisplayNames.size()) ? layoutDisplayNames[i] : makeLayoutDisplayName(layoutIds[i]);
        layoutComboBox.addItem(displayName, i + 1);
    }

    const auto index = layoutIds.indexOf(currentLayoutId);
    const auto selectedId = index >= 0 ? (index + 1) : 1;
    layoutComboBox.setSelectedId(selectedId, juce::dontSendNotification);
    updateLayoutActionButtons();
}

void ControlsPanel::updateLayoutActionButtons()
{
    const auto selectedLayoutId = getSelectedLayoutId();
    const auto isUserLayout = selectedLayoutId.isNotEmpty() && !selectedLayoutId.startsWith("default.");
    renameLayoutButton.setEnabled(isUserLayout);
    deleteLayoutButton.setEnabled(isUserLayout);
}

float ControlsPanel::getMasterGain() const
{
    return static_cast<float>(volumeSlider.getValue());
}

float ControlsPanel::getAttack() const
{
    return static_cast<float>(attackSlider.getValue());
}

float ControlsPanel::getDecay() const
{
    return static_cast<float>(decaySlider.getValue());
}

float ControlsPanel::getSustain() const
{
    return static_cast<float>(sustainSlider.getValue());
}

float ControlsPanel::getRelease() const
{
    return static_cast<float>(releaseSlider.getValue());
}

juce::String ControlsPanel::getSelectedLayoutId() const
{
    const auto selectedId = layoutComboBox.getSelectedId();
    if (selectedId <= 0)
        return {};
    const auto index = selectedId - 1;
    if (!juce::isPositiveAndBelow(index, availableLayoutIds.size()))
        return {};
    return availableLayoutIds[index];
}

void ControlsPanel::setPlaybackSpeed(double speed)
{
    currentPlaybackSpeed = std::clamp(speed, 0.5, 2.0);
    playbackSpeedValueLabel.setText(juce::String(currentPlaybackSpeed, 2) + "x",
                                    juce::dontSendNotification);
    // Disable speedDown at lower boundary, speedUp at upper boundary
    speedDownButton.setEnabled(currentPlaybackSpeed > 0.5);
    speedUpButton.setEnabled(currentPlaybackSpeed < 2.0);
}

double ControlsPanel::getCurrentPlaybackSpeed() const
{
    return currentPlaybackSpeed;
}

void ControlsPanel::configureSlider(juce::Slider& slider,
                                    juce::Label& label,
                                    const juce::String& text,
                                    double minimum,
                                    double maximum,
                                    double interval)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(label);

    slider.setRange(minimum, maximum, interval);
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 22);
    slider.onValueChange = [this]
    {
        if (onValuesChanged)
            onValuesChanged();
    };
    addAndMakeVisible(slider);
}
