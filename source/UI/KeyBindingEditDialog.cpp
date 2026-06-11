#include "UI/KeyBindingEditDialog.h"

// ============================================================================
// Dialog content component
// ============================================================================
class BindingEditContent final : public juce::Component {
public:
    BindingEditContent(int note, const devpiano::core::KeyBinding* existing, juce::String noteName,
                       juce::String keyLabel,
                       std::function<void(std::optional<devpiano::core::KeyBinding>)> onCompleteFn)
        : midiNote(note)
        , existingBinding(existing)
        , onComplete(std::move(onCompleteFn)) {
        // Title / info area (non-editable)
        titleLabel.setText("Key Binding Editor — " + noteName + " (#" + juce::String(midiNote) + ")",
                           juce::dontSendNotification);
        titleLabel.setFont(juce::Font(14.0f, juce::Font::bold));
        addAndMakeVisible(titleLabel);

        if (existingBinding != nullptr) {
            // Editable form for an existing binding
            infoLabel.setText("Bound to keyboard key:  " + keyLabel, juce::dontSendNotification);
            infoLabel.setFont(juce::Font(13.0f));
            addAndMakeVisible(infoLabel);

            auto& act = existingBinding->action;

            // MIDI Channel
            channelLabel.setText("MIDI Channel:", juce::dontSendNotification);
            channelLabel.attachToComponent(&channelCombo, true);
            channelLabel.setFont(juce::Font(13.0f));
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
            noteLabel.setText("MIDI Note:", juce::dontSendNotification);
            noteLabel.attachToComponent(&noteSlider, true);
            noteLabel.setFont(juce::Font(13.0f));
            addAndMakeVisible(noteLabel);
            addAndMakeVisible(noteSlider);

            // Velocity
            velocitySlider.setRange(0.0, 127.0, 1.0);
            velocitySlider.setValue(juce::jlimit(0.0, 127.0, static_cast<double>(act.velocity * 127.0)));
            velocitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
            velocitySlider.setNumDecimalPlacesToDisplay(0);
            velocityLabel.setText("Velocity:", juce::dontSendNotification);
            velocityLabel.attachToComponent(&velocitySlider, true);
            velocityLabel.setFont(juce::Font(13.0f));
            addAndMakeVisible(velocityLabel);
            addAndMakeVisible(velocitySlider);

            // OK / Cancel / Unbind buttons
            okButton.onClick = [this] { confirmEdit(); };
            addAndMakeVisible(okButton);
            cancelButton.onClick = [this] { cancel(); };
            addAndMakeVisible(cancelButton);
            unbindButton.onClick = [this] { unbind(); };
            addAndMakeVisible(unbindButton);

            setSize(420, 230);
        } else {
            // Read-only: no binding exists for this note
            infoLabel.setText("No keyboard key is currently mapped to this note.", juce::dontSendNotification);
            infoLabel.setFont(juce::Font(13.0f));
            addAndMakeVisible(infoLabel);

            closeButton.onClick = [this] { cancel(); };
            addAndMakeVisible(closeButton);

            setSize(420, 120);
        }
    }

    void resized() override {
        auto r = getLocalBounds().reduced(12);

        titleLabel.setBounds(r.removeFromTop(24));

        if (existingBinding != nullptr) {
            infoLabel.setBounds(r.removeFromTop(20));
            r.removeFromTop(8);

            channelCombo.setBounds(r.removeFromTop(24).withTrimmedLeft(100));
            r.removeFromTop(6);

            noteSlider.setBounds(r.removeFromTop(24).withTrimmedLeft(100));
            r.removeFromTop(6);

            velocitySlider.setBounds(r.removeFromTop(24).withTrimmedLeft(100));
            r.removeFromTop(12);

            auto btnRow = r.removeFromTop(28);
            okButton.setBounds(btnRow.removeFromLeft(80));
            btnRow.removeFromLeft(8);
            cancelButton.setBounds(btnRow.removeFromLeft(80));
            btnRow.removeFromLeft(8);
            unbindButton.setBounds(btnRow.removeFromLeft(80));
        } else {
            infoLabel.setBounds(r.removeFromTop(20));
            r.removeFromTop(12);
            closeButton.setBounds(r.removeFromTop(28).withWidth(80));
        }
    }

private:
    void confirmEdit() {
        if (existingBinding == nullptr) {
            cancel();
            return;
        }

        auto updated = *existingBinding;
        updated.action.midiChannel = channelCombo.getSelectedId();
        updated.action.midiNote = static_cast<int>(noteSlider.getValue());
        updated.action.velocity = static_cast<float>(velocitySlider.getValue() / 127.0);

        if (onComplete)
            onComplete(updated);
    }

    void cancel() {
        if (onComplete)
            onComplete(std::nullopt);
    }

    void unbind() {
        if (existingBinding == nullptr) {
            cancel();
            return;
        }

        // Return a binding with keyCode set to signal "remove this binding".
        // The caller should erase it from the layout.
        auto removed = *existingBinding;
        removed.keyCode = -1;
        if (onComplete)
            onComplete(removed);
    }

    int midiNote;
    const devpiano::core::KeyBinding* existingBinding;
    std::function<void(std::optional<devpiano::core::KeyBinding>)> onComplete;

    juce::Label titleLabel;
    juce::Label infoLabel;

    juce::Label channelLabel;
    juce::ComboBox channelCombo;
    juce::Label noteLabel;
    juce::Slider noteSlider;
    juce::Label velocityLabel;
    juce::Slider velocitySlider;

    juce::TextButton okButton { "OK" };
    juce::TextButton cancelButton { "Cancel" };
    juce::TextButton unbindButton { "Unbind" };
    juce::TextButton closeButton { "Close" };
};

// ============================================================================
// Lightweight modal dialog window.
// Self-destructs when closed.
// ============================================================================
class BindingEditWindow final : public juce::DialogWindow {
public:
    BindingEditWindow(juce::String title, std::unique_ptr<juce::Component> content)
        : juce::DialogWindow(std::move(title), juce::Colour(0xfff0f0f0), true, true) {
        setUsingNativeTitleBar(true);
        setContentOwned(content.release(), true);
        centreAroundComponent(nullptr, getWidth(), getHeight());
        setVisible(true);
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
                                  std::function<void(std::optional<devpiano::core::KeyBinding>)> onComplete) {
    auto keyLabel = existingBinding != nullptr ? existingBinding->displayText : juce::String();
    auto* content = new BindingEditContent(midiNote, existingBinding, noteName, keyLabel, std::move(onComplete));

    // BindingEditWindow takes ownership and self-destructs on close.
    new BindingEditWindow("Key Binding Editor", std::unique_ptr<juce::Component>(content));
}
