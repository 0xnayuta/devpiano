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
    DialogContentBase() = default;

    void paint(juce::Graphics& g) override {
        g.fillAll(devpiano::jive::DesignTokens::get().mainBg());
    }

    // NOTE: no destructor here. The "closed without confirming" (title bar X
    // / Escape) path is handled in the derived destructors instead, because
    // it fires `onComplete`, a derived member that has already been destroyed
    // by the time a base destructor body runs.
    void finish(int exitResult) {
        completed = true;
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>()) {
            dw->exitModalState(exitResult);
        }
    }

protected:
    // 共享的"完成并收尾"序列（QUAL-006）：防 double-callback 标记、取走回调
    // 通知（回调可能销毁 this）、仍存活才退出模态状态（防 UAF）。
    void completeWith(std::function<void()> notify) {
        if (completed) {
            return;
        }
        completed = true; // 先标记：析构不再触发取消回调，防止 double-callback
        auto n = std::move(notify); // 取走通知：通知内重入 completeWith() 直接返回
        if (n) {
            n();
        }
        // 回调可能已关闭对话框并销毁 this；仍存活才退出模态状态，防止 UAF
        if (juce::Component::SafePointer<DialogContentBase> alive(this); alive != nullptr) {
            alive->finish(0);
        }
    }

    bool completed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DialogContentBase)
};

// ============================================================================
// Single-line preset name input
// ============================================================================
class PresetNameContent final : public DialogContentBase {
public:
    PresetNameContent(const juce::String& initialName, std::function<void(std::optional<juce::String>)> onCompleteFn)
        : onComplete(std::move(onCompleteFn)) {
        nameLabel.setText(TRANS("Preset Name:"), juce::dontSendNotification);
        nameLabel.setFont(juce::FontOptions(14.0f));
        addAndMakeVisible(nameLabel);

        nameEditor.setText(initialName, juce::dontSendNotification);
        nameEditor.setFont(juce::FontOptions(14.0f));
        nameEditor.setInputRestrictions(64, {});
        nameEditor.onReturnKey = [this] { complete(nameEditor.getText().trim()); };
        addAndMakeVisible(nameEditor);
        okButton.onClick = [this] { complete(nameEditor.getText().trim()); };
        addAndMakeVisible(okButton);

        cancelButton.onClick = [this] { complete(std::nullopt); };
        addAndMakeVisible(cancelButton);

        setSize(380, 150);
    }

    ~PresetNameContent() override {
        // Fire the "cancelled" path if the window was closed without an
        // explicit confirm (title bar X / Escape).
        if (!completed && onComplete) {
            onComplete(std::nullopt);
        }
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
        completeWith([this, r = std::move(result)] {
            auto cb = std::move(onComplete); // 取走回调：回调内重入 complete() 直接返回
            if (cb) {
                cb(r); // r 为 lambda const 捕获；String 引用计数拷贝无额外开销
            }
        });
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
        : onComplete(std::move(onCompleteFn)) {
        messageLabel.setText(message, juce::dontSendNotification);
        messageLabel.setFont(juce::FontOptions(14.0f));
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

    ~PresetConfirmContent() override {
        // Title bar X / Escape: fire the cancel result while `onComplete`
        // (a derived member) is still alive. See PresetNameContent.
        if (!completed && onComplete) {
            onComplete(false);
        }
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
        completeWith([this, result] {
            auto cb = std::move(onComplete); // 取走回调：回调内重入 complete() 直接返回
            if (cb) {
                cb(result);
            }
        });
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
    if (componentToCentreAround != nullptr) {
        opts.content->setLookAndFeel(&componentToCentreAround->getLookAndFeel());
    }
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
