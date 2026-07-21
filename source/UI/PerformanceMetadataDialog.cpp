#include "UI/PerformanceMetadataDialog.h"
#include "DevPianoLookAndFeel.h"

namespace {

// ============================================================================
// Dialog content component
// ============================================================================
class MetadataEditContent final : public juce::Component {
public:
    MetadataEditContent(devpiano::recording::PerformanceFileMetadata initial,
                        std::function<void(std::optional<devpiano::recording::PerformanceFileMetadata>)> onCompleteFn)
        : onComplete(std::move(onCompleteFn)) {
        // Title editor (single line)
        titleLabel.setText(TRANS("Song Title"), juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(13.0f));
        addAndMakeVisible(titleLabel);

        titleEditor.setText(initial.title, juce::dontSendNotification);
        titleEditor.setInputRestrictions(128, {});
        addAndMakeVisible(titleEditor);

        // Notes editor (multi-line)
        notesLabel.setText(TRANS("Notes"), juce::dontSendNotification);
        notesLabel.setFont(juce::FontOptions(13.0f));
        addAndMakeVisible(notesLabel);

        notesEditor.setMultiLine(true, false);
        notesEditor.setReturnKeyStartsNewLine(true);
        notesEditor.setText(initial.notes, juce::dontSendNotification);
        notesEditor.setInputRestrictions(2048, {});
        addAndMakeVisible(notesEditor);

        // Buttons
        okButton.onClick = [this] { confirm(); };
        addAndMakeVisible(okButton);

        cancelButton.onClick = [this] { dismiss(); };
        addAndMakeVisible(cancelButton);

        setSize(420, 260);
    }

    ~MetadataEditContent() override {
        // Fire onComplete if neither confirm() nor dismiss() was called
        // (e.g. user closed via title bar X or Escape key).
        if (!completed && onComplete)
            onComplete(std::nullopt);
    }

    void resized() override {
        auto r = getLocalBounds().reduced(12);

        // Title row
        titleLabel.setBounds(r.removeFromTop(20));
        r.removeFromTop(2);
        titleEditor.setBounds(r.removeFromTop(28));
        r.removeFromTop(12);

        // Notes row
        notesLabel.setBounds(r.removeFromTop(20));
        r.removeFromTop(2);
        notesEditor.setBounds(r.removeFromTop(80));
        r.removeFromTop(12);

        // Button row
        auto btnRow = r.removeFromTop(28);
        okButton.setBounds(btnRow.removeFromLeft(80));
        btnRow.removeFromLeft(8);
        cancelButton.setBounds(btnRow.removeFromLeft(80));
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(DevPianoLookAndFeel::kMainBg);
    }

    void confirm() {
        completed = true;
        devpiano::recording::PerformanceFileMetadata result;
        result.title = titleEditor.getText();
        result.notes = notesEditor.getText();

        if (onComplete)
            onComplete(std::move(result));
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    }

    void dismiss() {
        completed = true;
        if (onComplete)
            onComplete(std::nullopt);
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    }

    juce::Label titleLabel;
    juce::TextEditor titleEditor;
    juce::Label notesLabel;
    juce::TextEditor notesEditor;
    juce::TextButton okButton { TRANS("OK") };
    juce::TextButton cancelButton { TRANS("Cancel") };
    std::function<void(std::optional<devpiano::recording::PerformanceFileMetadata>)> onComplete;
    bool completed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MetadataEditContent)
};

} // anonymous namespace

// ============================================================================
// Public launch method
// ============================================================================
void PerformanceMetadataDialog::launch(
    const devpiano::recording::PerformanceFileMetadata& initialMetadata, juce::Component* componentToCentreAround,
    std::function<void(std::optional<devpiano::recording::PerformanceFileMetadata>)> onComplete) {
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = TRANS("Song Information");
    opts.dialogBackgroundColour = DevPianoLookAndFeel::kMainBg;
    opts.componentToCentreAround = componentToCentreAround;
    opts.content.setOwned(new MetadataEditContent(initialMetadata, std::move(onComplete)));
    if (componentToCentreAround != nullptr) {
        opts.content->setLookAndFeel(&componentToCentreAround->getLookAndFeel());
    }
    opts.runModal();
}
