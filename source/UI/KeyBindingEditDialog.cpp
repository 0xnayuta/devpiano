#include "UI/KeyBindingEditDialog.h"

// ============================================================================
// Colour picker dialog content
// ============================================================================
class ColourPickerContent final : public juce::Component {
public:
    ColourPickerContent(juce::Colour initialColour, juce::Colour& resultRef, bool& acceptedRef)
        : result(resultRef)
        , accepted(acceptedRef) {
        selector = std::make_unique<juce::ColourSelector>(
            juce::ColourSelector::showColourAtTop | juce::ColourSelector::showAlphaChannel
            | juce::ColourSelector::showSliders | juce::ColourSelector::showColourspace);
        selector->setCurrentColour(initialColour);
        addAndMakeVisible(selector.get());

        okButton.onClick = [this] {
            result = selector->getCurrentColour();
            accepted = true;
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState(0);
        };
        okButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe0e0e0));
        addAndMakeVisible(okButton);

        cancelButton.onClick = [this] {
            accepted = false;
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState(0);
        };
        cancelButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe0e0e0));
        addAndMakeVisible(cancelButton);

        setSize(340, 340);
    }

    void resized() override {
        auto r = getLocalBounds().reduced(10);
        auto btnRow = r.removeFromBottom(32);
        r.removeFromBottom(8);
        okButton.setBounds(btnRow.removeFromLeft(80));
        btnRow.removeFromLeft(8);
        cancelButton.setBounds(btnRow.removeFromLeft(80));

        selector->setBounds(r);
    }

private:
    juce::Colour& result;
    bool& accepted;
    std::unique_ptr<juce::ColourSelector> selector;
    juce::TextButton okButton { TRANS("OK") };
    juce::TextButton cancelButton { TRANS("Cancel") };
};

// ============================================================================
// Dialog content component
// ============================================================================
class BindingEditContent final : public juce::Component {
public:
    BindingEditContent(int note, const devpiano::core::KeyBinding* existing, juce::String noteName,
                       juce::String keyLabel, juce::String customLabel, juce::Colour customColour,
                       std::function<void(KeyBindingEditResult)> onCompleteFn)
        : midiNote(note)
        , existingBinding(existing)
        , initialCustomLabel(customLabel)
        , initialCustomColour(customColour)
        , selectedCustomColour(customColour)
        , onComplete(std::move(onCompleteFn)) {
        // Title / info area (non-editable)
        titleLabel.setText(TRANS("Key Binding Editor") + " — " + noteName + " (#" + juce::String(midiNote) + ")",
                           juce::dontSendNotification);
        titleLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe0e0e0));
        addAndMakeVisible(titleLabel);

        if (existingBinding != nullptr) {
            // Editable form for an existing binding
            infoLabel.setText(TRANS("Bound to keyboard key:") + "  " + keyLabel, juce::dontSendNotification);
            infoLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
            infoLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe0e0e0));
            addAndMakeVisible(infoLabel);

            auto& act = existingBinding->action;

            // MIDI Channel
            channelLabel.setText(TRANS("MIDI Channel:"), juce::dontSendNotification);
            channelLabel.attachToComponent(&channelCombo, true);
            channelLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
            channelLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe0e0e0));
            addAndMakeVisible(channelLabel);
            for (int ch = 1; ch <= 16; ++ch)
                channelCombo.addItem(juce::String(ch), ch);
            channelCombo.setSelectedId(juce::jlimit(1, 16, act.midiChannel));
            addAndMakeVisible(channelCombo);

            // MIDI Note
            noteSlider.setRange(0.0, 127.0, 1.0);
            noteSlider.setValue(juce::jlimit(0, 127, act.midiNote));
            noteSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
            noteSlider.setNumDecimalPlacesToDisplay(0);
            noteLabel.setText(TRANS("MIDI Note:"), juce::dontSendNotification);
            noteLabel.attachToComponent(&noteSlider, true);
            noteLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
            noteLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe0e0e0));
            addAndMakeVisible(noteLabel);
            addAndMakeVisible(noteSlider);

            // Velocity
            velocitySlider.setRange(0.0, 127.0, 1.0);
            velocitySlider.setValue(juce::jlimit(0.0, 127.0, static_cast<double>(act.velocity * 127.0)));
            velocitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
            velocitySlider.setNumDecimalPlacesToDisplay(0);
            velocityLabel.setText(TRANS("Velocity:"), juce::dontSendNotification);
            velocityLabel.attachToComponent(&velocitySlider, true);
            velocityLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
            velocityLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe0e0e0));
            addAndMakeVisible(velocityLabel);
            addAndMakeVisible(velocitySlider);

            // OK / Cancel / Unbind buttons
            okButton.onClick = [this] { confirmEdit(); };
            okButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe0e0e0));
            addAndMakeVisible(okButton);
            cancelButton.onClick = [this] { cancel(); };
            cancelButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe0e0e0));
            addAndMakeVisible(cancelButton);
            unbindButton.onClick = [this] { unbind(); };
            unbindButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe0e0e0));
            addAndMakeVisible(unbindButton);

            setSize(420, 310);
        } else {
            // Read-only: no binding exists for this note
            infoLabel.setText(TRANS("No keyboard key is currently mapped to this note."), juce::dontSendNotification);
            infoLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
            infoLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe0e0e0));
            addAndMakeVisible(infoLabel);

            closeButton.onClick = [this] { confirmOrClose(); };
            closeButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe0e0e0));
            addAndMakeVisible(closeButton);

            setSize(420, 200);
        }

        // ---- Per-key custom label (always shown, regardless of binding) ----
        customLabelLabel.setText(TRANS("Label:"), juce::dontSendNotification);
        customLabelLabel.attachToComponent(&customLabelEditor, true);
        customLabelLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
        customLabelLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe0e0e0));
        addAndMakeVisible(customLabelLabel);

        customLabelEditor.setText(customLabel, juce::dontSendNotification);
        customLabelEditor.setInputRestrictions(32, {});
        customLabelEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xffe0e0e0));
        customLabelEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2c30));
        customLabelEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff555555));
        customLabelEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff7799cc));
        addAndMakeVisible(customLabelEditor);

        // ---- Per-key custom colour (always shown) ----
        colourPickerButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe0e0e0));
        colourPickerButton.onClick = [this] { showColourPicker(); };
        addAndMakeVisible(colourPickerButton);
        updateColourButtonText();

        clearColourButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe0e0e0));
        clearColourButton.onClick = [this] {
            selectedCustomColour = juce::Colour(0x00000000);
            updateColourButtonText();
        };
        addAndMakeVisible(clearColourButton);
    }

    void resized() override {
        auto r = getLocalBounds().reduced(12);

        titleLabel.setBounds(r.removeFromTop(24));
        r.removeFromTop(4);

        if (existingBinding != nullptr) {
            infoLabel.setBounds(r.removeFromTop(20));
            r.removeFromTop(8);

            channelCombo.setBounds(r.removeFromTop(24).withTrimmedLeft(100));
            r.removeFromTop(6);

            noteSlider.setBounds(r.removeFromTop(24).withTrimmedLeft(100));
            r.removeFromTop(6);

            velocitySlider.setBounds(r.removeFromTop(24).withTrimmedLeft(100));
            r.removeFromTop(12);
        } else {
            infoLabel.setBounds(r.removeFromTop(20));
            r.removeFromTop(12);
        }

        // Custom label row
        customLabelEditor.setBounds(r.removeFromTop(24).withTrimmedLeft(100));
        r.removeFromTop(6);

        // Custom colour row: button + clear button
        auto colourRow = r.removeFromTop(24).withTrimmedLeft(100);
        colourPickerButton.setBounds(colourRow.removeFromLeft(100));
        colourRow.removeFromLeft(8);
        clearColourButton.setBounds(colourRow.removeFromLeft(60));
        r.removeFromTop(12);

        // Button row
        if (existingBinding != nullptr) {
            auto btnRow = r.removeFromTop(28);
            okButton.setBounds(btnRow.removeFromLeft(80));
            btnRow.removeFromLeft(8);
            cancelButton.setBounds(btnRow.removeFromLeft(80));
            btnRow.removeFromLeft(8);
            unbindButton.setBounds(btnRow.removeFromLeft(80));
        } else {
            closeButton.setBounds(r.removeFromTop(28).withWidth(80));
        }
    }

private:
    void updateColourButtonText() {
        if (selectedCustomColour.isTransparent()) {
            colourPickerButton.setButtonText(TRANS("Choose Colour..."));
        } else {
            colourPickerButton.setButtonText(TRANS("Colour Set"));
            colourPickerButton.setColour(juce::TextButton::buttonColourId, selectedCustomColour);
        }
    }

    void showColourPicker() {
        auto initialColour = selectedCustomColour.isTransparent() ? juce::Colours::white : selectedCustomColour;

        // Track the picked colour across the modal dialog lifecycle
        juce::Colour pickedColour = initialColour;
        bool accepted = false;

        // Wrap ColourSelector with OK / Cancel buttons in a DialogWindow.
        juce::DialogWindow::LaunchOptions opts;
        opts.dialogTitle = TRANS("Choose Colour");
        opts.dialogBackgroundColour = juce::Colour(0xff202225);
        opts.componentToCentreAround = this;
        opts.content.setOwned(new ColourPickerContent(initialColour, pickedColour, accepted));
        opts.runModal();

        if (accepted) {
            selectedCustomColour = pickedColour;
            updateColourButtonText();
        }
    }

    void confirmEdit() {
        KeyBindingEditResult result;
        result.customLabel = customLabelEditor.getText();
        result.customColour = selectedCustomColour;
        result.labelChanged = (result.customLabel != initialCustomLabel);
        result.colourChanged = (result.customColour != initialCustomColour);

        if (existingBinding != nullptr) {
            auto updated = *existingBinding;
            updated.action.midiChannel = channelCombo.getSelectedId();
            updated.action.midiNote = static_cast<int>(noteSlider.getValue());
            updated.action.velocity = static_cast<float>(velocitySlider.getValue() / 127.0);
            result.binding = updated;
        }

        if (onComplete)
            onComplete(result);
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    }

    void cancel() {
        KeyBindingEditResult result;
        if (onComplete)
            onComplete(result);
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    }

    void confirmOrClose() {
        // Read-only mode: still return label/colour changes but no binding.
        confirmEdit();
    }

    void unbind() {
        if (existingBinding == nullptr) {
            cancel();
            return;
        }

        KeyBindingEditResult result;
        result.customLabel = customLabelEditor.getText();
        result.customColour = selectedCustomColour;
        result.labelChanged = true;
        result.colourChanged = true;

        // Return a binding with keyCode set to signal "remove this binding".
        auto removed = *existingBinding;
        removed.keyCode = -1;
        result.binding = removed;

        if (onComplete)
            onComplete(result);
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    }

    int midiNote;
    const devpiano::core::KeyBinding* existingBinding;
    juce::String initialCustomLabel;
    juce::Colour initialCustomColour;
    juce::Colour selectedCustomColour;
    std::function<void(KeyBindingEditResult)> onComplete;

    juce::Label titleLabel;
    juce::Label infoLabel;

    juce::Label channelLabel;
    juce::ComboBox channelCombo;
    juce::Label noteLabel;
    juce::Slider noteSlider;
    juce::Label velocityLabel;
    juce::Slider velocitySlider;

    juce::Label customLabelLabel;
    juce::TextEditor customLabelEditor;
    juce::TextButton colourPickerButton { TRANS("Choose Colour...") };
    juce::TextButton clearColourButton { TRANS("Clear") };

    juce::TextButton okButton { TRANS("OK") };
    juce::TextButton cancelButton { TRANS("Cancel") };
    juce::TextButton unbindButton { TRANS("Unbind") };
    juce::TextButton closeButton { TRANS("Close") };
};

// ============================================================================
// Lightweight modal dialog window.
// Self-destructs when closed.
// ============================================================================
class BindingEditWindow final : public juce::DialogWindow {
public:
    BindingEditWindow(juce::String title, std::unique_ptr<juce::Component> content)
        : juce::DialogWindow(std::move(title), juce::Colour(0xff202225), true, true) {
        setUsingNativeTitleBar(true);
        setContentOwned(content.release(), true);
        centreAroundComponent(nullptr, getWidth(), getHeight());
        // enterModalState implicitly makes the component visible.
        enterModalState(true, juce::ModalCallbackFunction::create([this](int) { delete this; }), true);
    }

    void closeButtonPressed() override {
        exitModalState(0);
    }
    bool escapeKeyPressed() override {
        closeButtonPressed();
        return true;
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BindingEditWindow)
};

// ============================================================================
// KeyBindingEditDialog static launch
void KeyBindingEditDialog::launch(int midiNote, const juce::String& noteName,
                                  const devpiano::core::KeyBinding* existingBinding,
                                  const juce::String& currentCustomLabel, const juce::Colour& currentCustomColour,
                                  std::function<void(KeyBindingEditResult)> onComplete) {
    auto keyLabel = existingBinding != nullptr ? existingBinding->displayText : juce::String();
    auto* content = new BindingEditContent(midiNote, existingBinding, noteName, keyLabel,
                                           currentCustomLabel, currentCustomColour, std::move(onComplete));

    // BindingEditWindow takes ownership and self-destructs on close.
    new BindingEditWindow(TRANS("Key Binding Editor"), std::unique_ptr<juce::Component>(content));
}
