#pragma once
#include <JuceHeader.h>

#include "Audio/AudioDeviceDiagnostics.h"
#include "Settings/SettingsModel.h"

class SettingsComponent : public juce::Component, private juce::ChangeListener {
public:
    explicit SettingsComponent(juce::AudioDeviceManager& dm, const juce::XmlElement* savedAudioDeviceState,
                               SettingsModel* displayModel = nullptr)
        : deviceManager(dm)
        , model(displayModel) {
        if (savedAudioDeviceState != nullptr)
            savedStateSnapshot = std::make_unique<juce::XmlElement>(*savedAudioDeviceState);

        selector = std::make_unique<juce::AudioDeviceSelectorComponent>(deviceManager, 0, 2, 0, 2, false, false, true,
                                                                        false);

        addAndMakeVisible(selector.get());

        // === Keyboard Display group ===
        keyboardGroup.setText("Keyboard Display");
        addAndMakeVisible(keyboardGroup);

        // Colour mode
        colourModeLabel.setText("Colour Mode:", juce::dontSendNotification);
        colourModeLabel.attachToComponent(&colourModeCombo, true);
        colourModeCombo.addItem("Classic", 1 + static_cast<int>(devpiano::ui::KeyColourMode::classic));
        colourModeCombo.addItem("Channel", 1 + static_cast<int>(devpiano::ui::KeyColourMode::channel));
        colourModeCombo.addItem("Velocity", 1 + static_cast<int>(devpiano::ui::KeyColourMode::velocity));
        if (model)
            colourModeCombo.setSelectedId(1 + static_cast<int>(model->keyboardColourMode), juce::dontSendNotification);
        colourModeCombo.onChange = [this] {
            if (!model)
                return;
            model->keyboardColourMode = static_cast<devpiano::ui::KeyColourMode>(colourModeCombo.getSelectedId() - 1);
            setDirty(true);
            if (onDisplaySettingsChanged)
                onDisplaySettingsChanged();
        };
        addAndMakeVisible(colourModeCombo);

        // Note display
        noteDisplayLabel.setText("Note Display:", juce::dontSendNotification);
        noteDisplayLabel.attachToComponent(&noteDisplayCombo, true);
        noteDisplayCombo.addItem("Do Re Mi", 1 + static_cast<int>(devpiano::ui::NoteDisplayMode::doReMi));
        noteDisplayCombo.addItem("Fixed Do", 1 + static_cast<int>(devpiano::ui::NoteDisplayMode::fixedDo));
        noteDisplayCombo.addItem("Note Name", 1 + static_cast<int>(devpiano::ui::NoteDisplayMode::noteName));
        if (model)
            noteDisplayCombo.setSelectedId(1 + static_cast<int>(model->keyboardNoteDisplay),
                                           juce::dontSendNotification);
        noteDisplayCombo.onChange = [this] {
            if (!model)
                return;
            model->keyboardNoteDisplay
                = static_cast<devpiano::ui::NoteDisplayMode>(noteDisplayCombo.getSelectedId() - 1);
            setDirty(true);
            if (onDisplaySettingsChanged)
                onDisplaySettingsChanged();
        };
        addAndMakeVisible(noteDisplayCombo);

        // Fade speed
        fadeSpeedLabel.setText("Fade Speed:", juce::dontSendNotification);
        fadeSpeedLabel.attachToComponent(&fadeSpeedSlider, true);
        fadeSpeedSlider.setRange(0.50, 1.00, 0.01);
        fadeSpeedSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        fadeSpeedSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        if (model)
            fadeSpeedSlider.setValue(model->keyboardFadeSpeed, juce::dontSendNotification);
        fadeSpeedSlider.onValueChange = [this] {
            if (!model)
                return;
            model->keyboardFadeSpeed = static_cast<float>(fadeSpeedSlider.getValue());
            if (onDisplaySettingsChanged)
                onDisplaySettingsChanged();
        };
        addAndMakeVisible(fadeSpeedSlider);

        // Resizable window toggle
        resizableToggle.setButtonText("Resizable Window");
        if (model)
            resizableToggle.setToggleState(model->resizableWindow, juce::dontSendNotification);
        resizableToggle.onClick = [this] {
            if (!model)
                return;
            model->resizableWindow = resizableToggle.getToggleState();
            setDirty(true);
            if (onDisplaySettingsChanged)
                onDisplaySettingsChanged();
        };
        addAndMakeVisible(resizableToggle);

        // Instrument filter toggle
        instrumentFilterToggle.setButtonText("Show MIDI/VSTi Instrument Filter");
        if (model)
            instrumentFilterToggle.setToggleState(model->showInstrumentFilter, juce::dontSendNotification);
        instrumentFilterToggle.onClick = [this] {
            if (!model)
                return;
            model->showInstrumentFilter = instrumentFilterToggle.getToggleState();
            setDirty(true);
            if (onDisplaySettingsChanged)
                onDisplaySettingsChanged();
        };
        addAndMakeVisible(instrumentFilterToggle);

        // Diagnostics editor + Save button (unchanged)
        diagnosticsEditor.setMultiLine(true);
        diagnosticsEditor.setReadOnly(true);
        diagnosticsEditor.setScrollbarsShown(true);
        diagnosticsEditor.setCaretVisible(false);
        diagnosticsEditor.setPopupMenuEnabled(true);
        diagnosticsEditor.setWantsKeyboardFocus(false);
        diagnosticsEditor.setMouseClickGrabsKeyboardFocus(false);
        addAndMakeVisible(diagnosticsEditor);

        saveButton.setButtonText("Save");
        addAndMakeVisible(saveButton);
        saveButton.onClick = [this] {
            dirty = false;
            if (onSaveRequested) {
                auto callback = onSaveRequested;
                juce::MessageManager::callAsync([callback] { callback(); });
            }
        };

        deviceManager.addChangeListener(this);
        updateDiagnostics();
        setSize(560, 620);
    }

    ~SettingsComponent() override {
        deviceManager.removeChangeListener(this);
    }

    void resized() override {
        auto area = getLocalBounds().reduced(8);

        // Bottom: save button + diagnostics
        auto bottomArea = area.removeFromBottom(100);
        auto buttonRow = bottomArea.removeFromBottom(36);
        saveButton.setBounds(buttonRow.removeFromRight(120).reduced(4));
        diagnosticsEditor.setBounds(bottomArea);

        // Above diagnostics: keyboard display group
        auto keyboardArea = area.removeFromBottom(170);
        keyboardGroup.setBounds(keyboardArea);
        auto groupContent = keyboardArea.reduced(12, 16);

        constexpr int rowH = 26;
        constexpr int controlW = 200;

        auto row = groupContent.removeFromTop(rowH);
        colourModeCombo.setBounds(row.removeFromRight(controlW).reduced(2));

        row = groupContent.removeFromTop(rowH);
        noteDisplayCombo.setBounds(row.removeFromRight(controlW).reduced(2));

        row = groupContent.removeFromTop(rowH);
        fadeSpeedSlider.setBounds(row.removeFromRight(controlW).reduced(2));

        row = groupContent.removeFromTop(rowH);
        resizableToggle.setBounds(row.removeFromRight(controlW).reduced(2));

        row = groupContent.removeFromTop(rowH);
        instrumentFilterToggle.setBounds(row.removeFromRight(controlW).reduced(2));

        // Top: audio selector fills the rest
        selector->setBounds(area);
    }

    bool isDirty() const noexcept {
        return dirty;
    }
    void setDirty(bool d) noexcept {
        dirty = d;
    }

    std::function<void()> onSaveRequested;
    std::function<void()> onDisplaySettingsChanged;

private:
    juce::AudioDeviceManager& deviceManager;
    SettingsModel* model;

    std::unique_ptr<juce::AudioDeviceSelectorComponent> selector;

    juce::GroupComponent keyboardGroup;
    juce::Label colourModeLabel;
    juce::ComboBox colourModeCombo;
    juce::Label noteDisplayLabel;
    juce::ComboBox noteDisplayCombo;
    juce::Label fadeSpeedLabel;
    juce::Slider fadeSpeedSlider;
    juce::ToggleButton resizableToggle;
    juce::ToggleButton instrumentFilterToggle;

    juce::TextButton saveButton;
    juce::TextEditor diagnosticsEditor;
    std::unique_ptr<juce::XmlElement> savedStateSnapshot;

    bool dirty = false;

    void updateDiagnostics() {
        const auto diagnostics = devpiano::audio::buildAudioDeviceDiagnostics(savedStateSnapshot.get(), deviceManager);
        diagnosticsEditor.setText(diagnostics.detailedSummary, juce::dontSendNotification);
    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override {
        dirty = true;
        updateDiagnostics();
    }
};
