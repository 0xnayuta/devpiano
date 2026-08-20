#include "UI/jive/JiveModalDialog.h"

#include "UI/jive/DesignTokens.h"
#include "UI/jive/JiveUtils.h"
#include "UI/jive/StyleCatalog.h"

namespace devpiano::ui::jive {

namespace {

// ============================================================================
// ValueTree Node Helpers
// ============================================================================

inline juce::ValueTree node(const juce::Identifier& type, const juce::String& id = {}) {
    auto t = juce::ValueTree(type);
    if (id.isNotEmpty()) {
        t.setProperty("id", id, nullptr);
    }
    return t;
}

inline juce::ValueTree text(const juce::String& content, const juce::String& id = {}) {
    auto t = node("Text", id);
    t.setProperty("text", content, nullptr);
    t.setProperty("title", content, nullptr);
    return t;
}

inline juce::ValueTree button(const juce::String& label, const juce::String& id = {}) {
    auto t = node("Button", id);
    t.setProperty("display", "flex", nullptr);
    t.setProperty("justify-content", "centre", nullptr);
    t.setProperty("align-items", "centre", nullptr);
    t.setProperty("title", label, nullptr);
    t.setProperty("border-width", "1", nullptr);

    auto labelText = text(label, id.isNotEmpty() ? id + "-text" : juce::String {});
    labelText.setProperty("justification", "centred", nullptr);
    labelText.setProperty("word-wrap", "none", nullptr);
    t.appendChild(labelText, nullptr);

    return t;
}

// ============================================================================
// JiveDialogContent Component
// ============================================================================

class JiveDialogContent final : public juce::Component {
public:
    explicit JiveDialogContent(JiveModalDialog::LaunchOptions opts)
        : options(std::move(opts)) {
        interpreter = std::make_unique<::jive::Interpreter>();
        auto& factory = interpreter->getComponentFactory();

        factory.set("PathEditor", [] {
            auto editor = std::make_unique<juce::TextEditor>();
            editor->setMultiLine(false);
            editor->setReturnKeyStartsNewLine(false);
            editor->setWantsKeyboardFocus(true);
            editor->setMouseClickGrabsKeyboardFocus(true);
            return editor;
        });

        factory.set("ListEditor", [] {
            auto editor = std::make_unique<juce::TextEditor>();
            editor->setMultiLine(true);
            editor->setReturnKeyStartsNewLine(true);
            editor->setScrollbarsShown(true);
            editor->setWantsKeyboardFocus(true);
            editor->setMouseClickGrabsKeyboardFocus(true);
            return editor;
        });

        factory.set("TextEditor", [] {
            auto editor = std::make_unique<juce::TextEditor>();
            editor->setMultiLine(false);
            editor->setWantsKeyboardFocus(true);
            editor->setMouseClickGrabsKeyboardFocus(true);
            return editor;
        });
        if (options.configureFactory) {
            options.configureFactory(factory);
        }

        // Apply global styles to the dialog layout tree
        StyleCatalog::get().applyToTree(options.layoutTree);

        rootItem = interpreter->interpret(options.layoutTree);
        jassert(rootItem != nullptr);

        if (rootItem != nullptr) {
            if (auto rootComp = rootItem->getComponent()) {
                addAndMakeVisible(*rootComp);
            }
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
        if (rootItem != nullptr) {
            if (auto* okBtn = JiveModalDialog::findButtonById(*rootItem, "dialog-ok-btn")) {
                okBtn->onClick = [this] { handleConfirm(); };
            }
            if (auto* cancelBtn = JiveModalDialog::findButtonById(*rootItem, "dialog-cancel-btn")) {
                cancelBtn->onClick = [this] { handleCancel(); };
            }

            // Hook return key on single-line editors
            if (auto* editor = JiveModalDialog::findTextEditorById(*rootItem, "dialog-editor")) {
                editor->onReturnKey = [this] { handleConfirm(); };
            } else if (auto* titleEd = JiveModalDialog::findTextEditorById(*rootItem, "title-editor")) {
                titleEd->onReturnKey = [this] { handleConfirm(); };
            }

            if (options.onInit) {
                options.onInit(*rootItem);
            }
        }

        setWantsKeyboardFocus(true);
    }

    ~JiveDialogContent() override {
        if (!completed && options.onCancel) {
            options.onCancel();
        }
        safeCleanupJiveTree(rootItem);
        interpreter.reset();
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(devpiano::jive::DesignTokens::get().mainBg());
    }

    void resized() override {
        if (rootItem != nullptr) {
            if (auto comp = rootItem->getComponent()) {
                comp->setBounds(getLocalBounds());
            }
        }
    }

    void parentHierarchyChanged() override {
        if (rootItem != nullptr) {
            juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<JiveDialogContent>(this)] {
                if (safeThis == nullptr || safeThis->rootItem == nullptr) {
                    return;
                }
                // Try focusing the first TextEditor
                if (auto* ed = JiveModalDialog::findTextEditorById(*safeThis->rootItem, "dialog-editor")) {
                    ed->grabKeyboardFocus();
                } else if (auto* titleEd = JiveModalDialog::findTextEditorById(*safeThis->rootItem, "title-editor")) {
                    titleEd->grabKeyboardFocus();
                } else if (auto* okBtn = JiveModalDialog::findButtonById(*safeThis->rootItem, "dialog-ok-btn")) {
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
        if (rootItem != nullptr && options.onConfirm) {
            const auto shouldClose = options.onConfirm(*rootItem);
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
    std::unique_ptr<::jive::Interpreter> interpreter;
    std::unique_ptr<::jive::GuiItem> rootItem;
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

void JiveModalDialog::launchSingleInput(const juce::String& title, const juce::String& labelText,
                                        const juce::String& initialValue, juce::Component* componentToCentreAround,
                                        std::function<void(std::optional<juce::String>)> onComplete, int maxChars,
                                        const juce::String& okButtonText, const juce::String& cancelButtonText) {
    auto layout = makeSingleInputLayout(labelText, 380, 150, okButtonText, cancelButtonText);

    LaunchOptions opts;
    opts.title = title;
    opts.layoutTree = layout;
    opts.componentToCentreAround = componentToCentreAround;

    opts.onInit = [initialValue, maxChars](::jive::GuiItem& root) {
        if (auto* editor = findTextEditorById(root, "dialog-editor")) {
            editor->setText(initialValue, juce::dontSendNotification);
            editor->setFont(juce::FontOptions(15.0f));
            if (maxChars > 0) {
                editor->setInputRestrictions(maxChars, {});
            }
            editor->selectAll();
        }
    };

    opts.onConfirm = [onComplete](::jive::GuiItem& root) -> bool {
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

    opts.onCancel = [onComplete] {
        if (onComplete) {
            onComplete(std::nullopt);
        }
    };

    launchCustom(opts);
}

void JiveModalDialog::launchConfirm(const juce::String& title, const juce::String& message, const juce::String& okLabel,
                                    const juce::String& cancelLabel, juce::Component* componentToCentreAround,
                                    std::function<void(bool)> onComplete) {
    auto layout = makeConfirmLayout(message, 380, 140, okLabel, cancelLabel);

    LaunchOptions opts;
    opts.title = title;
    opts.layoutTree = layout;
    opts.componentToCentreAround = componentToCentreAround;

    opts.onConfirm = [onComplete](::jive::GuiItem&) -> bool {
        if (onComplete) {
            onComplete(true);
        }
        return true;
    };

    opts.onCancel = [onComplete] {
        if (onComplete) {
            onComplete(false);
        }
    };

    launchCustom(opts);
}

void JiveModalDialog::launchMetadataEdit(const juce::String& title, const juce::String& initialTitle,
                                         const juce::String& initialNotes, juce::Component* componentToCentreAround,
                                         std::function<void(std::optional<MetadataResult>)> onComplete) {
    auto layout = makeMetadataEditLayout(420, 260);

    LaunchOptions opts;
    opts.title = title;
    opts.layoutTree = layout;
    opts.componentToCentreAround = componentToCentreAround;

    opts.onInit = [initialTitle, initialNotes](::jive::GuiItem& root) {
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

    opts.onConfirm = [onComplete](::jive::GuiItem& root) -> bool {
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

    opts.onCancel = [onComplete] {
        if (onComplete) {
            onComplete(std::nullopt);
        }
    };

    launchCustom(opts);
}

// ============================================================================
// Layout Builders
// ============================================================================

juce::ValueTree JiveModalDialog::makeSingleInputLayout(const juce::String& labelText, int width, int height,
                                                       const juce::String& okText, const juce::String& cancelText) {
    auto root = node("Component", "dialog-root");
    root.setProperty("display", "flex", nullptr);
    root.setProperty("flex-direction", "column", nullptr);
    root.setProperty("width", width, nullptr);
    root.setProperty("height", height, nullptr);
    root.setProperty("padding", "12", nullptr);

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

    auto btnRow = node("Component", "dialog-buttons");
    btnRow.setProperty("display", "flex", nullptr);
    btnRow.setProperty("flex-direction", "row", nullptr);
    btnRow.setProperty("justify-content", "flex-end", nullptr);
    btnRow.setProperty("align-items", "centre", nullptr);
    btnRow.setProperty("height", 28, nullptr);

    auto okBtn = button(okText, "dialog-ok-btn");
    okBtn.setProperty("width", 80, nullptr);
    okBtn.setProperty("height", 28, nullptr);
    okBtn.setProperty("margin", "0 8 0 0", nullptr);
    btnRow.appendChild(okBtn, nullptr);

    auto cancelBtn = button(cancelText, "dialog-cancel-btn");
    cancelBtn.setProperty("width", 80, nullptr);
    cancelBtn.setProperty("height", 28, nullptr);
    btnRow.appendChild(cancelBtn, nullptr);

    root.appendChild(btnRow, nullptr);
    return root;
}

juce::ValueTree JiveModalDialog::makeConfirmLayout(const juce::String& message, int width, int height,
                                                   const juce::String& okText, const juce::String& cancelText) {
    auto root = node("Component", "dialog-root");
    root.setProperty("display", "flex", nullptr);
    root.setProperty("flex-direction", "column", nullptr);
    root.setProperty("width", width, nullptr);
    root.setProperty("height", height, nullptr);
    root.setProperty("padding", "12", nullptr);

    auto label = text(message, "dialog-message");
    label.setProperty("justification", "centred", nullptr);
    label.setProperty("font-size", 15, nullptr);
    label.setProperty("height", 40, nullptr);
    label.setProperty("margin", "0 0 12 0", nullptr);
    root.appendChild(label, nullptr);

    auto btnRow = node("Component", "dialog-buttons");
    btnRow.setProperty("display", "flex", nullptr);
    btnRow.setProperty("flex-direction", "row", nullptr);
    btnRow.setProperty("justify-content", "flex-end", nullptr);
    btnRow.setProperty("align-items", "centre", nullptr);
    btnRow.setProperty("height", 28, nullptr);

    auto okBtn = button(okText, "dialog-ok-btn");
    okBtn.setProperty("width", 80, nullptr);
    okBtn.setProperty("height", 28, nullptr);
    okBtn.setProperty("margin", "0 8 0 0", nullptr);
    btnRow.appendChild(okBtn, nullptr);

    auto cancelBtn = button(cancelText, "dialog-cancel-btn");
    cancelBtn.setProperty("width", 80, nullptr);
    cancelBtn.setProperty("height", 28, nullptr);
    btnRow.appendChild(cancelBtn, nullptr);

    root.appendChild(btnRow, nullptr);
    return root;
}

juce::ValueTree JiveModalDialog::makeMetadataEditLayout(int width, int height, const juce::String& okText,
                                                        const juce::String& cancelText) {
    auto root = node("Component", "dialog-root");
    root.setProperty("display", "flex", nullptr);
    root.setProperty("flex-direction", "column", nullptr);
    root.setProperty("width", width, nullptr);
    root.setProperty("height", height, nullptr);
    root.setProperty("padding", "12", nullptr);

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
    auto btnRow = node("Component", "dialog-buttons");
    btnRow.setProperty("display", "flex", nullptr);
    btnRow.setProperty("flex-direction", "row", nullptr);
    btnRow.setProperty("justify-content", "flex-end", nullptr);
    btnRow.setProperty("align-items", "centre", nullptr);
    btnRow.setProperty("height", 28, nullptr);

    auto okBtn = button(okText, "dialog-ok-btn");
    okBtn.setProperty("width", 80, nullptr);
    okBtn.setProperty("height", 28, nullptr);
    okBtn.setProperty("margin", "0 8 0 0", nullptr);
    btnRow.appendChild(okBtn, nullptr);

    auto cancelBtn = button(cancelText, "dialog-cancel-btn");
    cancelBtn.setProperty("width", 80, nullptr);
    cancelBtn.setProperty("height", 28, nullptr);
    btnRow.appendChild(cancelBtn, nullptr);

    root.appendChild(btnRow, nullptr);
    return root;
}

juce::ValueTree JiveModalDialog::makeProgressLayout(const juce::String& initialMessage, int width, int height,
                                                    const juce::String& cancelText) {
    auto root = node("Component", "dialog-root");
    root.setProperty("display", "flex", nullptr);
    root.setProperty("flex-direction", "column", nullptr);
    root.setProperty("width", width, nullptr);
    root.setProperty("height", height, nullptr);
    root.setProperty("padding", "14", nullptr);

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

    auto btnRow = node("Component", "dialog-buttons");
    btnRow.setProperty("display", "flex", nullptr);
    btnRow.setProperty("flex-direction", "row", nullptr);
    btnRow.setProperty("justify-content", "flex-end", nullptr);
    btnRow.setProperty("align-items", "centre", nullptr);
    btnRow.setProperty("height", 28, nullptr);

    auto cancelBtn = button(cancelText, "dialog-cancel-btn");
    cancelBtn.setProperty("width", 80, nullptr);
    cancelBtn.setProperty("height", 28, nullptr);
    btnRow.appendChild(cancelBtn, nullptr);

    root.appendChild(btnRow, nullptr);
    return root;
}
} // namespace devpiano::ui::jive
