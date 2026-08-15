#include "UI/PresetDialogs.h"

#include "UI/jive/DesignTokens.h"

namespace {

// ============================================================================
// Shared dialog content base: dark background, parent L&F, consistent
// spacing (12px padding, 8px gaps, 28px editors, 80px buttons) mirroring
// PerformanceMetadataDialog.
// ============================================================================
class DialogContentBase : public juce::Component {
public:
    explicit DialogContentBase(std::function<void()> onDismiss)
        : dismissCallback(std::move(onDismiss)) {
    }

    ~DialogContentBase() override {
        // Fire the "cancelled" path if the window was closed without an
        // explicit confirm (title bar X / Escape).
        if (!completed && dismissCallback)
            dismissCallback();
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(devpiano::jive::DesignTokens::get().mainBg());
    }

    void finish(int exitResult) {
        completed = true;
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(exitResult);
    }

protected:
    std::function<void()> dismissCallback;
    bool completed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DialogContentBase)
};

// ============================================================================
// Single-line preset name input
// ============================================================================
class PresetNameContent final : public DialogContentBase {
public:
    PresetNameContent(const juce::String& initialName, std::function<void(std::optional<juce::String>)> onCompleteFn)
        : DialogContentBase([this] { complete(std::nullopt); })
        , onComplete(std::move(onCompleteFn)) {
        nameLabel.setText(TRANS("Preset Name:"), juce::dontSendNotification);
        nameLabel.setFont(juce::FontOptions(13.0f));
        addAndMakeVisible(nameLabel);

        nameEditor.setText(initialName, juce::dontSendNotification);
        nameEditor.setInputRestrictions(64, {});
        nameEditor.onReturnKey = [this] { complete(nameEditor.getText().trim()); };
        addAndMakeVisible(nameEditor);

        okButton.onClick = [this] { complete(nameEditor.getText().trim()); };
        addAndMakeVisible(okButton);

        cancelButton.onClick = [this] { complete(std::nullopt); };
        addAndMakeVisible(cancelButton);

        setSize(380, 150);
    }

    void resized() override {
        auto r = getLocalBounds().reduced(12);
        nameLabel.setBounds(r.removeFromTop(20));
        r.removeFromTop(2);
        nameEditor.setBounds(r.removeFromTop(28));
        r.removeFromTop(14);

        auto btnRow = r.removeFromTop(28);
        // Right-aligned button group: confirm (OK) left, cancel right.
        constexpr int btnW = 80;
        btnRow.removeFromLeft(btnRow.getWidth() - (btnW * 2 + 8));
        okButton.setBounds(btnRow.removeFromLeft(btnW));
        btnRow.removeFromLeft(8);
        cancelButton.setBounds(btnRow.removeFromLeft(btnW));
    }

private:
    void complete(std::optional<juce::String> result) {
        if (completed)
            return;
        if (onComplete)
            onComplete(std::move(result));
        finish(0);
    }

    juce::Label nameLabel;
    juce::TextEditor nameEditor;
    juce::TextButton okButton { TRANS("OK") };
    juce::TextButton cancelButton { TRANS("Cancel") };
    std::function<void(std::optional<juce::String>)> onComplete;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetNameContent)
};

// ============================================================================
// Yes/no confirmation
// ============================================================================
class PresetConfirmContent final : public DialogContentBase {
public:
    PresetConfirmContent(const juce::String& message, const juce::String& okLabel, const juce::String& cancelLabel,
                         std::function<void(bool)> onCompleteFn)
        : DialogContentBase([this] { complete(false); })
        , onComplete(std::move(onCompleteFn)) {
        messageLabel.setText(message, juce::dontSendNotification);
        messageLabel.setFont(juce::FontOptions(13.0f));
        messageLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(messageLabel);

        okButton.setButtonText(okLabel);
        okButton.onClick = [this] { complete(true); };
        addAndMakeVisible(okButton);

        cancelButton.setButtonText(cancelLabel);
        cancelButton.onClick = [this] { complete(false); };
        addAndMakeVisible(cancelButton);

        setSize(380, 140);
    }

    void resized() override {
        auto r = getLocalBounds().reduced(12);
        messageLabel.setBounds(r.removeFromTop(40));
        r.removeFromTop(12);

        auto btnRow = r.removeFromTop(28);
        // Right-aligned button group: affirmative (Delete/OK) left, cancel right.
        constexpr int btnW = 80;
        btnRow.removeFromLeft(btnRow.getWidth() - (btnW * 2 + 8));
        okButton.setBounds(btnRow.removeFromLeft(btnW));
        btnRow.removeFromLeft(8);
        cancelButton.setBounds(btnRow.removeFromLeft(btnW));
    }

private:
    void complete(bool result) {
        if (completed)
            return;
        if (onComplete)
            onComplete(result);
        finish(0);
    }

    juce::Label messageLabel;
    juce::TextButton okButton;
    juce::TextButton cancelButton;
    std::function<void(bool)> onComplete;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetConfirmContent)
};

// ============================================================================
// Shared launcher: DialogWindow with the dark theme (same pattern as
// PerformanceMetadataDialog::launch).
// ============================================================================
void launchDialog(const juce::String& title, juce::Component* componentToCentreAround,
                  std::unique_ptr<juce::Component> content) {
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = title;
    opts.dialogBackgroundColour = devpiano::jive::DesignTokens::get().mainBg();
    opts.componentToCentreAround = componentToCentreAround;
    opts.content.setOwned(content.release());
    if (componentToCentreAround != nullptr)
        opts.content->setLookAndFeel(&componentToCentreAround->getLookAndFeel());
    opts.runModal();
}

} // anonymous namespace

// ============================================================================
// Public launch methods
// ============================================================================
void PresetNameDialog::launch(const juce::String& title, const juce::String& initialName,
                              juce::Component* componentToCentreAround,
                              std::function<void(std::optional<juce::String>)> onComplete) {
    launchDialog(title, componentToCentreAround,
                 std::make_unique<PresetNameContent>(initialName, std::move(onComplete)));
}

void PresetConfirmDialog::show(const juce::String& title, const juce::String& message, const juce::String& okLabel,
                               const juce::String& cancelLabel, juce::Component* componentToCentreAround,
                               std::function<void(bool)> onComplete) {
    launchDialog(title, componentToCentreAround,
                 std::make_unique<PresetConfirmContent>(message, okLabel, cancelLabel, std::move(onComplete)));
}
