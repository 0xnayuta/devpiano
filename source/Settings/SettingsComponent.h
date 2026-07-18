#pragma once
#include <JuceHeader.h>

#include "Audio/AudioDeviceDiagnostics.h"
#include "Locale/LocaleManager.h"
#include "Settings/SettingsModel.h"

class SettingsComponent : public juce::Component, private juce::ChangeListener, public juce::ValueTree::Listener {
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
        keyboardGroup.setText(TRANS("Keyboard Display"));
        addAndMakeVisible(keyboardGroup);

        // Colour mode
        colourModeLabel.setText(TRANS("Colour Mode:"), juce::dontSendNotification);
        colourModeLabel.attachToComponent(&colourModeCombo, true);
        colourModeCombo.addItem(TRANS("Classic"), 1 + static_cast<int>(devpiano::ui::KeyColourMode::classic));
        colourModeCombo.addItem(TRANS("Channel"), 1 + static_cast<int>(devpiano::ui::KeyColourMode::channel));
        colourModeCombo.addItem(TRANS("Velocity"), 1 + static_cast<int>(devpiano::ui::KeyColourMode::velocity));
        if (model)
            colourModeCombo.setSelectedId(1 + static_cast<int>(model->keyboardColourMode), juce::dontSendNotification);
        colourModeCombo.onChange
            = [this] { editingState.setProperty("colourMode", colourModeCombo.getSelectedId(), nullptr); };
        addAndMakeVisible(colourModeCombo);

        // Note display
        noteDisplayLabel.setText(TRANS("Note Display:"), juce::dontSendNotification);
        noteDisplayLabel.attachToComponent(&noteDisplayCombo, true);
        noteDisplayCombo.addItem(TRANS("Do Re Mi"), 1 + static_cast<int>(devpiano::ui::NoteDisplayMode::doReMi));
        noteDisplayCombo.addItem(TRANS("Fixed Do"), 1 + static_cast<int>(devpiano::ui::NoteDisplayMode::fixedDo));
        noteDisplayCombo.addItem(TRANS("Note Name"), 1 + static_cast<int>(devpiano::ui::NoteDisplayMode::noteName));
        if (model)
            noteDisplayCombo.setSelectedId(1 + static_cast<int>(model->keyboardNoteDisplay),
                                           juce::dontSendNotification);
        noteDisplayCombo.onChange
            = [this] { editingState.setProperty("noteDisplay", noteDisplayCombo.getSelectedId(), nullptr); };
        addAndMakeVisible(noteDisplayCombo);

        // Fade speed
        fadeSpeedLabel.setText(TRANS("Fade Speed:"), juce::dontSendNotification);
        fadeSpeedLabel.attachToComponent(&fadeSpeedSlider, true);
        fadeSpeedSlider.setRange(0.50, 1.00, 0.01);
        fadeSpeedSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        fadeSpeedSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        if (model)
            fadeSpeedSlider.setValue(model->keyboardFadeSpeed, juce::dontSendNotification);
        fadeSpeedSlider.onValueChange
            = [this] { editingState.setProperty("fadeSpeed", fadeSpeedSlider.getValue(), nullptr); };
        addAndMakeVisible(fadeSpeedSlider);

        // Resizable window toggle
        resizableToggle.setButtonText(TRANS("Resizable Window"));
        if (model)
            resizableToggle.setToggleState(model->resizableWindow, juce::dontSendNotification);
        addAndMakeVisible(resizableToggle);

        // Instrument filter toggle
        instrumentFilterToggle.setButtonText(TRANS("Show MIDI/VSTi Instrument Filter"));
        if (model)
            instrumentFilterToggle.setToggleState(model->showInstrumentFilter, juce::dontSendNotification);
        addAndMakeVisible(instrumentFilterToggle);

        // Diagnostics editor + Save button (unchanged)
        diagnosticsEditor.setMultiLine(true);
        diagnosticsEditor.setReadOnly(true);

        // === Language ===
        languageLabel.setText(TRANS("Language:"), juce::dontSendNotification);
        languageLabel.attachToComponent(&languageCombo, true);
        addAndMakeVisible(languageLabel);
        using devpiano::locale::Language;
        languageCombo.addItem("English", 1);
        languageCombo.addItem(devpiano::locale::languageDisplayName(Language::zhCN), 2);
        if (model)
            languageCombo.setSelectedId(model->languageCode == "zh-CN" ? 2 : 1, juce::dontSendNotification);
        languageCombo.onChange = [this] {
            editingState.setProperty("languageCode",
                                     languageCombo.getSelectedId() == 2 ? juce::String("zh-CN") : juce::String("en"),
                                     nullptr);
        };
        addAndMakeVisible(languageCombo);

        // Diagnostics editor + Save button (unchanged)
        diagnosticsEditor.setScrollbarsShown(true);
        diagnosticsEditor.setCaretVisible(false);
        diagnosticsEditor.setPopupMenuEnabled(true);
        diagnosticsEditor.setWantsKeyboardFocus(false);
        diagnosticsEditor.setMouseClickGrabsKeyboardFocus(false);
        addAndMakeVisible(diagnosticsEditor);

        saveButton.setButtonText(TRANS("Save"));
        addAndMakeVisible(saveButton);
        saveButton.onClick = [this] {
            dirty = false;
            if (onSaveRequested) {
                auto callback = onSaveRequested;
                juce::MessageManager::callAsync([callback] { callback(); });
            }
        };

        // --- Load model into editingState (before listener) ---
        if (model) {
            editingState.setProperty("colourMode", colourModeCombo.getSelectedId(), nullptr);
            editingState.setProperty("noteDisplay", noteDisplayCombo.getSelectedId(), nullptr);
            editingState.setProperty("fadeSpeed", static_cast<double>(model->keyboardFadeSpeed), nullptr);
            editingState.setProperty("resizableWindow", model->resizableWindow, nullptr);
            editingState.setProperty("showInstrumentFilter", model->showInstrumentFilter, nullptr);
            editingState.setProperty("languageCode", model->languageCode, nullptr);
            editingState.addListener(this);

            // --- Value bindings (ToggleButtons auto-sync, zero callback) ---
            editingState.getPropertyAsValue("resizableWindow", nullptr).referTo(resizableToggle.getToggleStateValue());
            editingState.getPropertyAsValue("showInstrumentFilter", nullptr)
                .referTo(instrumentFilterToggle.getToggleStateValue());
        }

        deviceManager.addChangeListener(this);
        setSize(560, 620);
        updateDiagnostics();
    }

    void refreshTexts() {
        keyboardGroup.setText(TRANS("Keyboard Display"));
        colourModeLabel.setText(TRANS("Colour Mode:"), juce::dontSendNotification);
        noteDisplayLabel.setText(TRANS("Note Display:"), juce::dontSendNotification);
        fadeSpeedLabel.setText(TRANS("Fade Speed:"), juce::dontSendNotification);
        resizableToggle.setButtonText(TRANS("Resizable Window"));
        instrumentFilterToggle.setButtonText(TRANS("Show MIDI/VSTi Instrument Filter"));
        languageLabel.setText(TRANS("Language:"), juce::dontSendNotification);
        saveButton.setButtonText(TRANS("Save"));

        // Rebuild ComboBox items so their entries reflect the new language.
        if (model) {
            {
                const auto selected = colourModeCombo.getSelectedId();
                colourModeCombo.clear(juce::dontSendNotification);
                colourModeCombo.addItem(TRANS("Classic"), 1 + static_cast<int>(devpiano::ui::KeyColourMode::classic));
                colourModeCombo.addItem(TRANS("Channel"), 1 + static_cast<int>(devpiano::ui::KeyColourMode::channel));
                colourModeCombo.addItem(TRANS("Velocity"), 1 + static_cast<int>(devpiano::ui::KeyColourMode::velocity));
                colourModeCombo.setSelectedId(selected, juce::dontSendNotification);
            }
            {
                const auto selected = noteDisplayCombo.getSelectedId();
                noteDisplayCombo.clear(juce::dontSendNotification);
                noteDisplayCombo.addItem(TRANS("Do Re Mi"),
                                         1 + static_cast<int>(devpiano::ui::NoteDisplayMode::doReMi));
                noteDisplayCombo.addItem(TRANS("Fixed Do"),
                                         1 + static_cast<int>(devpiano::ui::NoteDisplayMode::fixedDo));
                noteDisplayCombo.addItem(TRANS("Note Name"),
                                         1 + static_cast<int>(devpiano::ui::NoteDisplayMode::noteName));
                noteDisplayCombo.setSelectedId(selected, juce::dontSendNotification);
            }
        }

        // Re-create AudioDeviceSelectorComponent so its internal labels re-evaluate
        // TRANS() with the now-active language. JUCE's component only calls TRANS()
        // in its constructor and has no runtime i18n API.
        removeChildComponent(selector.get());
        selector = std::make_unique<juce::AudioDeviceSelectorComponent>(deviceManager, 0, 2, 0, 2, false, false, true,
                                                                        false);
        addAndMakeVisible(selector.get());
        resized();

        if (onRefreshTexts)
            onRefreshTexts();
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
        // Above diagnostics: keyboard display group + language
        auto keyboardArea = area.removeFromBottom(196);
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

        resizableToggle.setBounds(row.removeFromRight(controlW).reduced(2));

        row = groupContent.removeFromTop(rowH);
        instrumentFilterToggle.setBounds(row.removeFromRight(controlW).reduced(2));
        row = groupContent.removeFromTop(rowH);
        languageCombo.setBounds(row.removeFromRight(controlW).reduced(2));
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
    std::function<void(const juce::String&)> onLanguageChanged;
    std::function<void()> onRefreshTexts;

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
    juce::Label languageLabel;
    juce::ComboBox languageCombo;
    juce::TextButton saveButton;
    juce::TextEditor diagnosticsEditor;
    std::unique_ptr<juce::XmlElement> savedStateSnapshot;

    bool dirty = false;

    juce::ValueTree editingState { "Settings" };

    void updateDiagnostics() {
        const auto diagnostics = devpiano::audio::buildAudioDeviceDiagnostics(savedStateSnapshot.get(), deviceManager);
        diagnosticsEditor.setText(diagnostics.detailedSummary, juce::dontSendNotification);
    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override {
        dirty = true;
        updateDiagnostics();
    }

    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& prop) override {
        if (tree != editingState || !model)
            return;

        if (prop == juce::Identifier("colourMode"))
            model->keyboardColourMode = static_cast<devpiano::ui::KeyColourMode>((int)editingState[prop] - 1);
        else if (prop == juce::Identifier("noteDisplay"))
            model->keyboardNoteDisplay = static_cast<devpiano::ui::NoteDisplayMode>((int)editingState[prop] - 1);
        else if (prop == juce::Identifier("fadeSpeed"))
            model->keyboardFadeSpeed = static_cast<float>((double)editingState[prop]);
        else if (prop == juce::Identifier("resizableWindow"))
            model->resizableWindow = (bool)editingState[prop];
        else if (prop == juce::Identifier("showInstrumentFilter"))
            model->showInstrumentFilter = (bool)editingState[prop];
        else if (prop == juce::Identifier("languageCode")) {
            model->languageCode = editingState[prop].toString();
            if (onLanguageChanged)
                onLanguageChanged(model->languageCode);
            refreshTexts();
            return;
        }

        setDirty(true);
        if (onDisplaySettingsChanged)
            onDisplaySettingsChanged();
    }
};
