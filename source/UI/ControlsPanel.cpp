#include "ControlsPanel.h"
#include "DevPianoLookAndFeel.h"

ControlsPanel::ControlsPanel() {
    configureKnob(volumeSlider, volumeLabel, TRANS("Volume"), 0.0, 1.0, 0.01,
                  [](double v) { return juce::String(v, 2); });
    configureKnob(attackSlider, attackLabel, TRANS("Attack"), 0.001, 2.0, 0.001,
                  [](double v) { return juce::String(v, 3) + "s"; });
    configureKnob(decaySlider, decayLabel, TRANS("Decay"), 0.001, 2.0, 0.001,
                  [](double v) { return juce::String(v, 3) + "s"; });
    configureKnob(sustainSlider, sustainLabel, TRANS("Sustain"), 0.0, 1.0, 0.01,
                  [](double v) { return juce::String(v, 2); });
    configureKnob(releaseSlider, releaseLabel, TRANS("Release"), 0.001, 3.0, 0.001,
                  [](double v) { return juce::String(v, 3) + "s"; });

    // --- Preset row ---
    presetLabel.setText(TRANS("Preset"), juce::dontSendNotification);
    presetLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(presetLabel);

    addAndMakeVisible(presetComboBox);
    presetComboBox.setTextWhenNothingSelected("Default");
    presetComboBox.onChange = [this] {
        const auto selectedId = presetComboBox.getSelectedId();
        if (selectedId <= 0 || !onPresetChanged)
            return;

        const auto index = selectedId - 1;
        if (!juce::isPositiveAndBelow(index, availablePresetIds.size()))
            return;

        onPresetChanged(availablePresetIds[index]);
        updatePresetActionButtons();
    };

    addAndMakeVisible(saveAsNewPresetButton);
    saveAsNewPresetButton.onClick = [this] {
        if (onSaveAsNewPresetRequested)
            onSaveAsNewPresetRequested();
    };

    addAndMakeVisible(renamePresetButton);
    renamePresetButton.onClick = [this] {
        if (onRenamePresetRequested)
            onRenamePresetRequested();
    };

    addAndMakeVisible(deletePresetButton);
    deletePresetButton.onClick = [this] {
        if (onDeletePresetRequested)
            onDeletePresetRequested();
    };

    // --- Recording row ---
    recordStatusLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(recordStatusLabel);

    addAndMakeVisible(recordButton);
    recordButton.onClick = [this] {
        if (onRecordClicked)
            onRecordClicked();
    };

    addAndMakeVisible(playButton);
    playButton.onClick = [this] {
        if (onPlayClicked)
            onPlayClicked();
    };

    addAndMakeVisible(stopButton);
    stopButton.onClick = [this] {
        if (onStopClicked)
            onStopClicked();
    };

    addAndMakeVisible(backToStartButton);
    backToStartButton.onClick = [this] {
        if (onBackToStartClicked)
            onBackToStartClicked();
    };

    addAndMakeVisible(importMidiButton);
    importMidiButton.onClick = [this] {
        if (onImportMidiClicked)
            onImportMidiClicked();
    };

    addAndMakeVisible(exportMidiButton);
    exportMidiButton.onClick = [this] {
        if (onExportMidiClicked)
            onExportMidiClicked();
    };

    addAndMakeVisible(exportWavButton);
    exportWavButton.onClick = [this] {
        if (onExportWavClicked)
            onExportWavClicked();
    };

    addAndMakeVisible(savePerformanceButton);
    savePerformanceButton.onClick = [this] {
        if (onSavePerformanceClicked)
            onSavePerformanceClicked();
    };

    addAndMakeVisible(openPerformanceButton);
    openPerformanceButton.onClick = [this] {
        if (onOpenPerformanceClicked)
            onOpenPerformanceClicked();
    };

    addAndMakeVisible(recentFilesButton);
    recentFilesButton.onClick = [this] {
        if (onRecentFilesClicked)
            onRecentFilesClicked();
    };

    addAndMakeVisible(songInfoButton);
    songInfoButton.onClick = [this] {
        if (onSongInfoRequested)
            onSongInfoRequested();
    };

    // Playback speed
    playbackSpeedLabel.setText(TRANS("Speed"), juce::dontSendNotification);
    playbackSpeedLabel.setJustificationType(juce::Justification::centred);
    playbackSpeedLabel.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(playbackSpeedLabel);
    playbackSpeedSlider.setRange(0.5, 2.0, 0.25);
    playbackSpeedSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    playbackSpeedSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 44, 16);
    playbackSpeedSlider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                                            juce::MathConstants<float>::pi * 2.75f, true);
    playbackSpeedSlider.textFromValueFunction = [](double v) { return juce::String(v, 1) + "x"; };
    playbackSpeedSlider.onValueChange = [this] {
        repaint();
        if (onPlaybackSpeedChange)
            onPlaybackSpeedChange(playbackSpeedSlider.getValue());
    };
    addAndMakeVisible(playbackSpeedSlider);

    setRecordingControlsState({});
}

ControlsPanel::~ControlsPanel() {
    volumeSlider.onValueChange = nullptr;
    attackSlider.onValueChange = nullptr;
    decaySlider.onValueChange = nullptr;
    sustainSlider.onValueChange = nullptr;
    releaseSlider.onValueChange = nullptr;
    presetComboBox.onChange = nullptr;
    saveAsNewPresetButton.onClick = nullptr;
    renamePresetButton.onClick = nullptr;
    deletePresetButton.onClick = nullptr;
    recordButton.onClick = nullptr;
    playButton.onClick = nullptr;
    stopButton.onClick = nullptr;
    backToStartButton.onClick = nullptr;
    songInfoButton.onClick = nullptr;
    exportMidiButton.onClick = nullptr;
    exportWavButton.onClick = nullptr;
    importMidiButton.onClick = nullptr;
    savePerformanceButton.onClick = nullptr;
    openPerformanceButton.onClick = nullptr;
    recentFilesButton.onClick = nullptr;
    playbackSpeedSlider.onValueChange = nullptr;
}

void ControlsPanel::resized() {
    auto area = getLocalBounds();
    constexpr int knobRowHeight = 68;
    constexpr int curveHeight = 44;
    constexpr int rowHeight = 24;

    // -- Knob row: 6 rotary knobs horizontal --
    auto knobRow = area.removeFromTop(knobRowHeight);
    const int knobSlotWidth = knobRow.getWidth() / 6;
    constexpr int sliderW = 50;
    constexpr int sliderH = 56; // 40px knob + 16px TextBoxBelow

    auto placeKnob = [&](juce::Slider& slider, juce::Label& label) {
        auto slot = knobRow.removeFromLeft(knobSlotWidth);
        slider.setBounds(slot.getCentreX() - sliderW / 2, slot.getY() + 2, sliderW, sliderH);
        label.setBounds(slot.getX(), slider.getBottom() + 2, knobSlotWidth, 12);
    };

    placeKnob(volumeSlider, volumeLabel);
    placeKnob(attackSlider, attackLabel);
    placeKnob(decaySlider, decayLabel);
    placeKnob(sustainSlider, sustainLabel);
    placeKnob(releaseSlider, releaseLabel);
    placeKnob(playbackSpeedSlider, playbackSpeedLabel);

    // -- ADSR curve area --
    area.removeFromTop(2);
    m_adsrCurveArea = area.removeFromTop(curveHeight);
    area.removeFromTop(4);

    // -- Preset row --
    auto row = area.removeFromTop(rowHeight);
    presetLabel.setBounds(row.removeFromLeft(80));
    presetComboBox.setBounds(row.removeFromLeft(200));
    row.removeFromLeft(8);
    saveAsNewPresetButton.setBounds(row.removeFromLeft(100));
    row.removeFromLeft(8);
    renamePresetButton.setBounds(row.removeFromLeft(70));
    row.removeFromLeft(8);
    deletePresetButton.setBounds(row.removeFromLeft(60));

    area.removeFromTop(8);

    // -- Recording controls row --
    auto buttonRow = area.removeFromTop(rowHeight);
    recordStatusLabel.setBounds(buttonRow.removeFromLeft(80));
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
    buttonRow.removeFromLeft(6);
    recentFilesButton.setBounds(buttonRow.removeFromLeft(60));
    buttonRow.removeFromLeft(6);
    songInfoButton.setBounds(buttonRow.removeFromLeft(80));
}

// ============================================================================
//  paint  (ADSR envelope curve)
// ============================================================================
void ControlsPanel::paint(juce::Graphics& g) {
    drawAdsrCurve(g, static_cast<float>(attackSlider.getValue()), static_cast<float>(decaySlider.getValue()),
                  static_cast<float>(sustainSlider.getValue()), static_cast<float>(releaseSlider.getValue()));
}

void ControlsPanel::drawAdsrCurve(juce::Graphics& g, float a, float d, float s, float r) {
    if (m_adsrCurveArea.isEmpty())
        return;

    // Scale each parameter to millisecond range for visual proportions
    const float attackMs = a * 1000.0f;
    const float decayMs = d * 1000.0f;
    const float releaseMs = r * 1000.0f;
    constexpr float sustainWeight = 200.0f; // fixed visual weight for sustain phase

    float totalX = attackMs + decayMs + sustainWeight + releaseMs;
    if (totalX < 1.0f)
        return;

    auto rect = m_adsrCurveArea.toFloat().reduced(12.0f, 6.0f);
    const float w = rect.getWidth();
    const float h = rect.getHeight();
    const float x0 = rect.getX();
    const float y0 = rect.getY();

    float xA = w * (attackMs / totalX);
    float xD = w * (decayMs / totalX);
    float xS = w * (sustainWeight / totalX);

    float peakY = y0; // attack peak at top
    float susY = y0 + (1.0f - s) * h; // sustain level
    float baseY = y0 + h; // zero level at bottom

    juce::Path path;
    path.startNewSubPath(x0, baseY);
    path.lineTo(x0 + xA, peakY); // attack
    path.lineTo(x0 + xA + xD, susY); // decay
    path.lineTo(x0 + xA + xD + xS, susY); // sustain
    path.lineTo(rect.getRight(), baseY); // release
    path.closeSubPath();

    const auto primary = DevPianoLookAndFeel::kPrimary;
    g.setColour(primary.withAlpha(0.15f));
    g.fillPath(path);
    g.setColour(primary);
    g.strokePath(path, juce::PathStrokeType(1.5f));
}

void ControlsPanel::setRecordingControlsState(RecordingControlsState state) {
    recordingControlsState = state;
    updateRecordingActionButtons();
}

void ControlsPanel::updateRecordingActionButtons() {
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

    switch (recordingControlsState.state) {
    case RecordingState::idle:
        statusText = TRANS("Idle");
        playEnabled = recordingControlsState.hasTake;
        backToStartEnabled = recordingControlsState.hasTake;
        exportMidiEnabled = recordingControlsState.hasTake && recordingControlsState.canExportMidiTake;
        exportWavEnabled = recordingControlsState.hasTake && recordingControlsState.canExportWavTake;
        importMidiEnabled = true;
        saveEnabled = recordingControlsState.hasTake;
        openEnabled = true;
        break;
    case RecordingState::recording:
        statusText = TRANS("Recording");
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
        statusText = TRANS("Playing");
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
}

void ControlsPanel::setValues(float masterGain, float attack, float decay, float sustain, float release) {
    volumeSlider.setValue(masterGain, juce::dontSendNotification);
    attackSlider.setValue(attack, juce::dontSendNotification);
    decaySlider.setValue(decay, juce::dontSendNotification);
    sustainSlider.setValue(sustain, juce::dontSendNotification);
    releaseSlider.setValue(release, juce::dontSendNotification);
}

void ControlsPanel::setPresets(const juce::StringArray& presetIds, const juce::String& currentPresetId,
                               const juce::StringArray& presetDisplayNames) {
    availablePresetIds = presetIds;

    presetComboBox.clear(juce::dontSendNotification);
    for (int i = 0; i < presetIds.size(); ++i) {
        auto displayName = (i < presetDisplayNames.size()) ? presetDisplayNames[i] : presetIds[i];
        presetComboBox.addItem(displayName, i + 1);
    }

    const auto index = presetIds.indexOf(currentPresetId);
    const auto selectedId = index >= 0 ? (index + 1) : 1;
    presetComboBox.setSelectedId(selectedId, juce::dontSendNotification);
    updatePresetActionButtons();
}

void ControlsPanel::updatePresetActionButtons() {
    // Rename and Delete only apply to user presets (non-empty currentPresetId)
    auto selectedId = getSelectedPresetId();
    auto isUserPreset = selectedId.isNotEmpty();
    renamePresetButton.setEnabled(isUserPreset);
    deletePresetButton.setEnabled(isUserPreset);
}

juce::String ControlsPanel::getSelectedPresetId() const {
    const auto selectedId = presetComboBox.getSelectedId();
    if (selectedId <= 0)
        return {};
    const auto index = selectedId - 1;
    if (!juce::isPositiveAndBelow(index, availablePresetIds.size()))
        return {};
    return availablePresetIds[index];
}

float ControlsPanel::getMasterGain() const {
    return static_cast<float>(volumeSlider.getValue());
}

float ControlsPanel::getAttack() const {
    return static_cast<float>(attackSlider.getValue());
}

float ControlsPanel::getDecay() const {
    return static_cast<float>(decaySlider.getValue());
}

float ControlsPanel::getSustain() const {
    return static_cast<float>(sustainSlider.getValue());
}

float ControlsPanel::getRelease() const {
    return static_cast<float>(releaseSlider.getValue());
}

void ControlsPanel::setPlaybackSpeed(double speed) {
    playbackSpeedSlider.setValue(juce::jlimit(0.5, 2.0, speed), juce::dontSendNotification);
}

void ControlsPanel::configureKnob(juce::Slider& slider, juce::Label& label, const juce::String& text, double minimum,
                                  double maximum, double interval, std::function<juce::String(double)> formatter) {
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(label);

    slider.setRange(minimum, maximum, interval);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 44, 16);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setDoubleClickReturnValue(true, 0.5);
    if (formatter)
        slider.textFromValueFunction = std::move(formatter);
    slider.onValueChange = [this] {
        repaint();
        if (onValuesChanged)
            onValuesChanged();
    };
    addAndMakeVisible(slider);
}

void ControlsPanel::refreshTexts() {
    volumeLabel.setText(TRANS("Volume"), juce::dontSendNotification);
    attackLabel.setText(TRANS("Attack"), juce::dontSendNotification);
    decayLabel.setText(TRANS("Decay"), juce::dontSendNotification);
    sustainLabel.setText(TRANS("Sustain"), juce::dontSendNotification);
    releaseLabel.setText(TRANS("Release"), juce::dontSendNotification);
    presetLabel.setText(TRANS("Preset"), juce::dontSendNotification);
    playbackSpeedLabel.setText(TRANS("Speed"), juce::dontSendNotification);

    saveAsNewPresetButton.setButtonText(TRANS("Save As New"));
    renamePresetButton.setButtonText(TRANS("Rename"));
    deletePresetButton.setButtonText(TRANS("Delete"));
    recordButton.setButtonText(TRANS("Record"));
    playButton.setButtonText(TRANS("Play"));
    stopButton.setButtonText(TRANS("Stop"));
    backToStartButton.setButtonText(TRANS("Back"));
    exportMidiButton.setButtonText(TRANS("Export MIDI"));
    exportWavButton.setButtonText(TRANS("Export WAV"));
    importMidiButton.setButtonText(TRANS("Import MIDI"));
    recentFilesButton.setButtonText(TRANS("Recent"));
    savePerformanceButton.setButtonText(TRANS("Save"));
    openPerformanceButton.setButtonText(TRANS("Open"));
    songInfoButton.setButtonText(TRANS("Song Info"));

    updateRecordingActionButtons();
}

juce::Rectangle<int> ControlsPanel::getRecentFilesButtonScreenBounds() const noexcept {
    return recentFilesButton.getScreenBounds();
}
