#include "UI/jive/JiveModalDialog.h"
#include "UI/jive/DesignTokens.h"
#include "UI/jive/JiveBuilderHelpers.h"
#include "UI/jive/JiveUtils.h"
#include "UI/jive/StyleCatalog.h"

#include "UI/ViewHost.h"
namespace devpiano::ui::jive {

namespace {
// ============================================================================
// JiveDialogContent Component
// ============================================================================

class JiveDialogContent final : public juce::Component {
public:
    explicit JiveDialogContent(JiveModalDialog::LaunchOptions opts)
        : options(std::move(opts)) {
        viewHost.registerDefaultComponents();
        if (options.configureFactory) {
            viewHost.configureComponentFactory(options.configureFactory);
        }

        viewHost.loadLayout(options.layoutTree, true);

        if (auto* rootComp = viewHost.getRootComponent()) {
            addAndMakeVisible(*rootComp);
        }

        // Determine dialog content size
        auto width = options.defaultWidth;
        auto height = options.defaultHeight;
        if (options.layoutTree.hasProperty("width")) {
            width = static_cast<int>(options.layoutTree.getProperty("width"));
        }
        if (options.layoutTree.hasProperty("height")) {
            height = static_cast<int>(options.layoutTree.getProperty("height"));
        }
        setSize(width, height);

        // Hook up standard button callbacks
        if (auto* okBtn = viewHost.find<juce::Button>("dialog-ok-btn")) {
            okBtn->onClick = [this] { handleConfirm(); };
        }
        if (auto* cancelBtn = viewHost.find<juce::Button>("dialog-cancel-btn")) {
            cancelBtn->onClick = [this] { handleCancel(); };
        }

        // Hook return key on single-line editors
        if (auto* editor = viewHost.find<juce::TextEditor>("dialog-editor")) {
            editor->onReturnKey = [this] { handleConfirm(); };
        } else if (auto* titleEd = viewHost.find<juce::TextEditor>("title-editor")) {
            titleEd->onReturnKey = [this] { handleConfirm(); };
        }

        if (options.onInitHost) {
            options.onInitHost(viewHost);
        } else if (options.onInit && viewHost.getRootItem() != nullptr) {
            options.onInit(*viewHost.getRootItem());
        }

        setWantsKeyboardFocus(true);
    }

    ~JiveDialogContent() override {
        if (!completed && options.onCancel) {
            options.onCancel();
        }
        viewHost.reset();
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(devpiano::jive::DesignTokens::get().mainBg());
    }

    void resized() override {
        viewHost.setBounds(getLocalBounds());
    }

    void parentHierarchyChanged() override {
        if (viewHost.isValid()) {
            juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<JiveDialogContent>(this)] {
                if (safeThis == nullptr || !safeThis->viewHost.isValid()) {
                    return;
                }
                // Try focusing the first TextEditor
                if (auto* ed = safeThis->viewHost.find<juce::TextEditor>("dialog-editor")) {
                    ed->grabKeyboardFocus();
                } else if (auto* titleEd = safeThis->viewHost.find<juce::TextEditor>("title-editor")) {
                    titleEd->grabKeyboardFocus();
                } else if (auto* okBtn = safeThis->viewHost.find<juce::Button>("dialog-ok-btn")) {
                    okBtn->grabKeyboardFocus();
                }
            });
        }
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if (key.isKeyCode(juce::KeyPress::escapeKey)) {
            handleCancel();
            return true;
        }
        if (key.isKeyCode(juce::KeyPress::returnKey)) {
            // If focused component is not a multiline text editor, confirm
            if (auto* focused = juce::Component::getCurrentlyFocusedComponent()) {
                if (auto* ed = dynamic_cast<juce::TextEditor*>(focused)) {
                    if (ed->isMultiLine()) {
                        return false; // let newline happen
                    }
                }
            }
            handleConfirm();
            return true;
        }
        return false;
    }

    void handleConfirm() {
        if (options.onConfirmHost) {
            const auto shouldClose = options.onConfirmHost(viewHost);
            if (!shouldClose) {
                return;
            }
        } else if (options.onConfirm && viewHost.getRootItem() != nullptr) {
            const auto shouldClose = options.onConfirm(*viewHost.getRootItem());
            if (!shouldClose) {
                return;
            }
        }
        completeWith([] { });
    }

    void handleCancel() {
        completeWith([this] {
            if (options.onCancel) {
                options.onCancel();
            }
        });
    }

    void completeWith(std::function<void()> notify) {
        if (completed) {
            return;
        }
        completed = true;
        auto n = std::move(notify);
        if (n) {
            n();
        }
        if (juce::Component::SafePointer<JiveDialogContent> alive(this); alive != nullptr) {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>()) {
                dw->exitModalState(0);
            }
        }
    }

private:
    JiveModalDialog::LaunchOptions options;
    devpiano::ui::ViewHost viewHost;
    bool completed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JiveDialogContent)
};

} // namespace

// ============================================================================
// Public JiveModalDialog Methods
// ============================================================================

void JiveModalDialog::launchCustom(const LaunchOptions& options) {
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = options.title;
    opts.dialogBackgroundColour = devpiano::jive::DesignTokens::get().mainBg();
    opts.componentToCentreAround = options.componentToCentreAround;
    opts.resizable = options.isResizable;

    auto content = std::make_unique<JiveDialogContent>(options);
    if (options.componentToCentreAround != nullptr) {
        content->setLookAndFeel(&options.componentToCentreAround->getLookAndFeel());
    }
    opts.content.setOwned(content.release());
    opts.launchAsync();
}

void JiveModalDialog::launchSingleInput(const SingleInputOptions& options) {
    auto layout = makeSingleInputLayout(options.labelText, 380, 150, options.okButtonText, options.cancelButtonText);

    LaunchOptions opts;
    opts.title = options.title;
    opts.layoutTree = layout;
    opts.componentToCentreAround = options.componentToCentreAround;

    opts.onInit = [initialValue = options.initialValue, maxChars = options.maxChars](::jive::GuiItem& root) {
        if (auto* editor = findTextEditorById(root, "dialog-editor")) {
            editor->setText(initialValue, juce::dontSendNotification);
            editor->setFont(juce::FontOptions(15.0f));
            if (maxChars > 0) {
                editor->setInputRestrictions(maxChars, {});
            }
            editor->selectAll();
        }
    };

    opts.onConfirm = [onComplete = options.onComplete](::jive::GuiItem& root) -> bool {
        if (auto* editor = findTextEditorById(root, "dialog-editor")) {
            auto val = editor->getText().trim();
            if (onComplete) {
                onComplete(val);
            }
            return true;
        }
        if (onComplete) {
            onComplete(juce::String {});
        }
        return true;
    };

    opts.onCancel = [onComplete = options.onComplete] {
        if (onComplete) {
            onComplete(std::nullopt);
        }
    };

    launchCustom(opts);
}

void JiveModalDialog::launchSingleInput(const juce::String& title, const juce::String& labelText,
                                        const juce::String& initialValue, juce::Component* componentToCentreAround,
                                        const std::function<void(std::optional<juce::String>)>& onComplete,
                                        int maxChars, const juce::String& okButtonText,
                                        const juce::String& cancelButtonText) {
    launchSingleInput(SingleInputOptions { .title = title,
                                           .labelText = labelText,
                                           .initialValue = initialValue,
                                           .componentToCentreAround = componentToCentreAround,
                                           .onComplete = onComplete,
                                           .maxChars = maxChars,
                                           .okButtonText = okButtonText,
                                           .cancelButtonText = cancelButtonText });
}

void JiveModalDialog::launchConfirm(const ConfirmOptions& options) {
    auto layout = makeConfirmLayout(options.message, 380, 140, options.okLabel, options.cancelLabel);

    LaunchOptions opts;
    opts.title = options.title;
    opts.layoutTree = layout;
    opts.componentToCentreAround = options.componentToCentreAround;

    opts.onConfirm = [onComplete = options.onComplete](::jive::GuiItem&) -> bool {
        if (onComplete) {
            onComplete(true);
        }
        return true;
    };

    opts.onCancel = [onComplete = options.onComplete] {
        if (onComplete) {
            onComplete(false);
        }
    };

    launchCustom(opts);
}

void JiveModalDialog::launchConfirm(const juce::String& title, const juce::String& message, const juce::String& okLabel,
                                    const juce::String& cancelLabel, juce::Component* componentToCentreAround,
                                    const std::function<void(bool)>& onComplete) {
    launchConfirm(ConfirmOptions { .title = title,
                                   .message = message,
                                   .okLabel = okLabel,
                                   .cancelLabel = cancelLabel,
                                   .componentToCentreAround = componentToCentreAround,
                                   .onComplete = onComplete });
}

void JiveModalDialog::launchMetadataEdit(const MetadataEditOptions& options) {
    auto layout = makeMetadataEditLayout(420, 260);

    LaunchOptions opts;
    opts.title = options.title;
    opts.layoutTree = layout;
    opts.componentToCentreAround = options.componentToCentreAround;

    opts.onInit = [initialTitle = options.initialTitle, initialNotes = options.initialNotes](::jive::GuiItem& root) {
        if (auto* titleEd = findTextEditorById(root, "title-editor")) {
            titleEd->setText(initialTitle, juce::dontSendNotification);
            titleEd->setFont(juce::FontOptions(15.0f));
            titleEd->setInputRestrictions(128, {});
        }
        if (auto* notesEd = findTextEditorById(root, "notes-editor")) {
            notesEd->setMultiLine(true, false);
            notesEd->setReturnKeyStartsNewLine(true);
            notesEd->setText(initialNotes, juce::dontSendNotification);
            notesEd->setFont(juce::FontOptions(15.0f));
            notesEd->setInputRestrictions(2048, {});
        }
    };

    opts.onConfirm = [onComplete = options.onComplete](::jive::GuiItem& root) -> bool {
        MetadataResult res;
        if (auto* titleEd = findTextEditorById(root, "title-editor")) {
            res.title = titleEd->getText();
        }
        if (auto* notesEd = findTextEditorById(root, "notes-editor")) {
            res.notes = notesEd->getText();
        }
        if (onComplete) {
            onComplete(std::move(res));
        }
        return true;
    };

    opts.onCancel = [onComplete = options.onComplete] {
        if (onComplete) {
            onComplete(std::nullopt);
        }
    };

    launchCustom(opts);
}

void JiveModalDialog::launchMetadataEdit(const juce::String& title, const juce::String& initialTitle,
                                         const juce::String& initialNotes, juce::Component* componentToCentreAround,
                                         const std::function<void(std::optional<MetadataResult>)>& onComplete) {
    launchMetadataEdit(MetadataEditOptions { .title = title,
                                             .initialTitle = initialTitle,
                                             .initialNotes = initialNotes,
                                             .componentToCentreAround = componentToCentreAround,
                                             .onComplete = onComplete });
}

// ============================================================================
// Layout Builders
// ============================================================================

namespace {

juce::ValueTree makeBaseDialogRoot(int width, int height, int padding = 12) {
    auto root = node("Component", "dialog-root");
    root.setProperty("display", "flex", nullptr);
    root.setProperty("flex-direction", "column", nullptr);
    root.setProperty("width", width, nullptr);
    root.setProperty("height", height, nullptr);
    root.setProperty("padding", juce::String(padding), nullptr);
    return root;
}

juce::ValueTree makeDialogButtons(const juce::String& okText, const juce::String& cancelText) {
    auto btnRow = node("Component", "dialog-buttons");
    btnRow.setProperty("display", "flex", nullptr);
    btnRow.setProperty("flex-direction", "row", nullptr);
    btnRow.setProperty("justify-content", "flex-end", nullptr);
    btnRow.setProperty("align-items", "centre", nullptr);
    btnRow.setProperty("height", 28, nullptr);

    if (okText.isNotEmpty()) {
        auto okBtn = button(okText, "dialog-ok-btn");
        okBtn.setProperty("width", 80, nullptr);
        okBtn.setProperty("height", 28, nullptr);
        if (cancelText.isNotEmpty()) {
            okBtn.setProperty("margin", "0 8 0 0", nullptr);
        }
        btnRow.appendChild(okBtn, nullptr);
    }

    if (cancelText.isNotEmpty()) {
        auto cancelBtn = button(cancelText, "dialog-cancel-btn");
        cancelBtn.setProperty("width", 80, nullptr);
        cancelBtn.setProperty("height", 28, nullptr);
        btnRow.appendChild(cancelBtn, nullptr);
    }

    return btnRow;
}

} // namespace

juce::ValueTree JiveModalDialog::makeSingleInputLayout(const juce::String& labelText, int width, int height,
                                                       const juce::String& okText, const juce::String& cancelText) {
    auto root = makeBaseDialogRoot(width, height, 12);

    auto label = text(labelText, "dialog-label");
    label.setProperty("height", 20, nullptr);
    label.setProperty("margin", "0 0 4 0", nullptr);
    label.setProperty("font-size", 15, nullptr);
    root.appendChild(label, nullptr);

    auto editor = node("PathEditor", "dialog-editor");
    editor.setProperty("height", 28, nullptr);
    editor.setProperty("margin", "0 0 14 0", nullptr);
    editor.setProperty("focusable", true, nullptr);
    editor.setProperty("cursor", "text", nullptr);
    root.appendChild(editor, nullptr);

    root.appendChild(makeDialogButtons(okText, cancelText), nullptr);
    return root;
}

juce::ValueTree JiveModalDialog::makeConfirmLayout(const juce::String& message, int width, int height,
                                                   const juce::String& okText, const juce::String& cancelText) {
    auto root = makeBaseDialogRoot(width, height, 12);

    auto label = text(message, "dialog-message");
    label.setProperty("justification", "centred", nullptr);
    label.setProperty("font-size", 15, nullptr);
    label.setProperty("height", 40, nullptr);
    label.setProperty("margin", "0 0 12 0", nullptr);
    root.appendChild(label, nullptr);

    root.appendChild(makeDialogButtons(okText, cancelText), nullptr);
    return root;
}

juce::ValueTree JiveModalDialog::makeMetadataEditLayout(int width, int height, const juce::String& okText,
                                                        const juce::String& cancelText) {
    auto root = makeBaseDialogRoot(width, height, 12);

    auto titleLabel = text(TRANS("Song Title"), "title-label");
    titleLabel.setProperty("height", 20, nullptr);
    titleLabel.setProperty("margin", "0 0 2 0", nullptr);
    titleLabel.setProperty("font-size", 15, nullptr);
    root.appendChild(titleLabel, nullptr);

    auto titleEditor = node("PathEditor", "title-editor");
    titleEditor.setProperty("height", 28, nullptr);
    titleEditor.setProperty("margin", "0 0 12 0", nullptr);
    titleEditor.setProperty("focusable", true, nullptr);
    titleEditor.setProperty("cursor", "text", nullptr);
    root.appendChild(titleEditor, nullptr);

    auto notesLabel = text(TRANS("Notes"), "notes-label");
    notesLabel.setProperty("height", 20, nullptr);
    notesLabel.setProperty("margin", "0 0 2 0", nullptr);
    notesLabel.setProperty("font-size", 15, nullptr);
    root.appendChild(notesLabel, nullptr);

    auto notesEditor = node("ListEditor", "notes-editor");
    notesEditor.setProperty("height", 80, nullptr);
    notesEditor.setProperty("margin", "0 0 12 0", nullptr);
    notesEditor.setProperty("focusable", true, nullptr);
    notesEditor.setProperty("cursor", "text", nullptr);
    root.appendChild(notesEditor, nullptr);

    root.appendChild(makeDialogButtons(okText, cancelText), nullptr);
    return root;
}

juce::ValueTree JiveModalDialog::makeProgressLayout(const juce::String& initialMessage, int width, int height,
                                                    const juce::String& cancelText) {
    auto root = makeBaseDialogRoot(width, height, 14);

    auto msg = text(initialMessage, "progress-status-message");
    msg.setProperty("font-size", 14, nullptr);
    msg.setProperty("height", 22, nullptr);
    msg.setProperty("margin", "0 0 10 0", nullptr);
    msg.setProperty("justification", "centred-left", nullptr);
    root.appendChild(msg, nullptr);

    auto bar = node("ProgressBar", "dialog-progress-bar");
    bar.setProperty("height", 16, nullptr);
    bar.setProperty("margin", "0 0 16 0", nullptr);
    root.appendChild(bar, nullptr);

    root.appendChild(makeDialogButtons({}, cancelText), nullptr);
    return root;
}
} // namespace devpiano::ui::jive
