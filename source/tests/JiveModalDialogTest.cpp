#include <JuceHeader.h>

#include "UI/jive/DesignTokens.h"
#include "UI/jive/JiveModalDialog.h"
#include "UI/jive/StyleCatalog.h"

namespace {

void clearJiveStyleSheets(juce::Component* comp) {
    if (comp == nullptr) {
        return;
    }
    for (int i = 0; i < comp->getNumChildComponents(); ++i) {
        clearJiveStyleSheets(comp->getChildComponent(i));
    }
    if (comp->getProperties().contains("style-sheet")) {
        comp->getProperties().remove("style-sheet");
    }
}

void collectJiveComponents(::jive::GuiItem& item, std::vector<std::shared_ptr<juce::Component>>& components) {
    if (auto component = item.getComponent()) {
        components.push_back(std::move(component));
    }
    for (auto* child : item.getChildren()) {
        collectJiveComponents(*child, components);
    }
}

void safeCleanupJiveTree(std::unique_ptr<::jive::GuiItem>& rootItem) {
    if (rootItem != nullptr) {
        std::vector<std::shared_ptr<juce::Component>> jiveComponents;
        collectJiveComponents(*rootItem, jiveComponents);
        clearJiveStyleSheets(rootItem->getComponent().get());
        rootItem.reset();
    }
}

juce::ValueTree findNodeById(const juce::ValueTree& root, const juce::String& id) {
    if (root.getProperty("id").toString() == id) {
        return root;
    }
    for (auto child : root) {
        if (auto found = findNodeById(child, id); found.isValid()) {
            return found;
        }
    }
    return {};
}

} // namespace

class JiveModalDialogTest final : public juce::UnitTest {
public:
    JiveModalDialogTest()
        : juce::UnitTest("JiveModalDialog", "DevPiano/UI") {
    }

    void runTest() override {
        devpiano::ui::jive::StyleCatalog::get().reset();
        devpiano::jive::DesignTokens::get().reset();

        testSingleInputLayoutBuilder();
        testConfirmLayoutBuilder();
        testMetadataEditLayoutBuilder();
        testInterpretationAndComponentLookup();
        testMetadataInterpretation();
    }

private:
    void testSingleInputLayoutBuilder() {
        beginTest("makeSingleInputLayout: tree structure and properties");

        auto tree
            = devpiano::ui::jive::JiveModalDialog::makeSingleInputLayout("Preset Name:", 400, 160, "Save", "Dismiss");
        expect(tree.isValid());
        expectEquals(tree.getType().toString(), juce::String("Component"));
        expectEquals(static_cast<int>(tree.getProperty("width")), 400);
        expectEquals(static_cast<int>(tree.getProperty("height")), 160);
        expectEquals(tree.getProperty("display").toString(), juce::String("flex"));
        expectEquals(tree.getProperty("flex-direction").toString(), juce::String("column"));

        auto labelNode = findNodeById(tree, "dialog-label");
        expect(labelNode.isValid());
        expectEquals(labelNode.getType().toString(), juce::String("Text"));
        expectEquals(labelNode.getProperty("text").toString(), juce::String("Preset Name:"));

        auto editorNode = findNodeById(tree, "dialog-editor");
        expect(editorNode.isValid());
        expectEquals(editorNode.getType().toString(), juce::String("PathEditor"));

        auto okBtn = findNodeById(tree, "dialog-ok-btn");
        expect(okBtn.isValid());
        expectEquals(okBtn.getType().toString(), juce::String("Button"));
        expectEquals(okBtn.getProperty("title").toString(), juce::String("Save"));

        auto cancelBtn = findNodeById(tree, "dialog-cancel-btn");
        expect(cancelBtn.isValid());
        expectEquals(cancelBtn.getType().toString(), juce::String("Button"));
        expectEquals(cancelBtn.getProperty("title").toString(), juce::String("Dismiss"));
    }

    void testConfirmLayoutBuilder() {
        beginTest("makeConfirmLayout: tree structure and message");

        auto tree
            = devpiano::ui::jive::JiveModalDialog::makeConfirmLayout("Delete this item?", 360, 130, "Delete", "Keep");
        expect(tree.isValid());
        expectEquals(static_cast<int>(tree.getProperty("width")), 360);
        expectEquals(static_cast<int>(tree.getProperty("height")), 130);

        auto msgNode = findNodeById(tree, "dialog-message");
        expect(msgNode.isValid());
        expectEquals(msgNode.getType().toString(), juce::String("Text"));
        expectEquals(msgNode.getProperty("text").toString(), juce::String("Delete this item?"));
        expectEquals(msgNode.getProperty("justification").toString(), juce::String("centred"));

        auto okBtn = findNodeById(tree, "dialog-ok-btn");
        expect(okBtn.isValid());
        expectEquals(okBtn.getProperty("title").toString(), juce::String("Delete"));

        auto cancelBtn = findNodeById(tree, "dialog-cancel-btn");
        expect(cancelBtn.isValid());
        expectEquals(cancelBtn.getProperty("title").toString(), juce::String("Keep"));
    }

    void testMetadataEditLayoutBuilder() {
        beginTest("makeMetadataEditLayout: title and notes sections");

        auto tree = devpiano::ui::jive::JiveModalDialog::makeMetadataEditLayout(440, 280, "OK", "Cancel");
        expect(tree.isValid());
        expectEquals(static_cast<int>(tree.getProperty("width")), 440);
        expectEquals(static_cast<int>(tree.getProperty("height")), 280);

        auto titleLabel = findNodeById(tree, "title-label");
        expect(titleLabel.isValid());
        auto titleEditor = findNodeById(tree, "title-editor");
        expect(titleEditor.isValid());
        expectEquals(titleEditor.getType().toString(), juce::String("PathEditor"));

        auto notesLabel = findNodeById(tree, "notes-label");
        expect(notesLabel.isValid());
        auto notesEditor = findNodeById(tree, "notes-editor");
        expect(notesEditor.isValid());
        expectEquals(notesEditor.getType().toString(), juce::String("ListEditor"));

        auto okBtn = findNodeById(tree, "dialog-ok-btn");
        expect(okBtn.isValid());
        auto cancelBtn = findNodeById(tree, "dialog-cancel-btn");
        expect(cancelBtn.isValid());
    }

    void testInterpretationAndComponentLookup() {
        beginTest("findButtonById and findTextEditorById on interpreted tree");

        auto tree = devpiano::ui::jive::JiveModalDialog::makeSingleInputLayout("Enter Value:", 380, 150);

        ::jive::Interpreter interpreter;
        auto& factory = interpreter.getComponentFactory();
        factory.set("PathEditor", [] {
            auto editor = std::make_unique<juce::TextEditor>();
            editor->setMultiLine(false);
            return editor;
        });

        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);
        auto rootItem = interpreter.interpret(tree);
        expect(rootItem != nullptr);

        if (rootItem != nullptr) {
            auto* okBtn = devpiano::ui::jive::JiveModalDialog::findButtonById(*rootItem, "dialog-ok-btn");
            expect(okBtn != nullptr);

            auto* cancelBtn = devpiano::ui::jive::JiveModalDialog::findButtonById(*rootItem, "dialog-cancel-btn");
            expect(cancelBtn != nullptr);

            auto* editor = devpiano::ui::jive::JiveModalDialog::findTextEditorById(*rootItem, "dialog-editor");
            expect(editor != nullptr);

            if (editor != nullptr) {
                editor->setText("Initial Test String");
                expectEquals(editor->getText(), juce::String("Initial Test String"));
                expect(!editor->isMultiLine());
            }

            // Non-existent IDs should return nullptr safely
            expect(devpiano::ui::jive::JiveModalDialog::findButtonById(*rootItem, "non-existent-btn") == nullptr);
            expect(devpiano::ui::jive::JiveModalDialog::findTextEditorById(*rootItem, "non-existent-editor")
                   == nullptr);

            safeCleanupJiveTree(rootItem);
        }
    }

    void testMetadataInterpretation() {
        beginTest("makeMetadataEditLayout: single-line vs multi-line editors");

        auto tree = devpiano::ui::jive::JiveModalDialog::makeMetadataEditLayout(420, 260);

        ::jive::Interpreter interpreter;
        auto& factory = interpreter.getComponentFactory();
        factory.set("PathEditor", [] {
            auto editor = std::make_unique<juce::TextEditor>();
            editor->setMultiLine(false);
            return editor;
        });
        factory.set("ListEditor", [] {
            auto editor = std::make_unique<juce::TextEditor>();
            editor->setMultiLine(true);
            editor->setReturnKeyStartsNewLine(true);
            return editor;
        });

        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);
        auto rootItem = interpreter.interpret(tree);
        expect(rootItem != nullptr);

        if (rootItem != nullptr) {
            auto* titleEd = devpiano::ui::jive::JiveModalDialog::findTextEditorById(*rootItem, "title-editor");
            expect(titleEd != nullptr);
            if (titleEd != nullptr) {
                expect(!titleEd->isMultiLine());
                titleEd->setText("My Song");
                expectEquals(titleEd->getText(), juce::String("My Song"));
            }

            auto* notesEd = devpiano::ui::jive::JiveModalDialog::findTextEditorById(*rootItem, "notes-editor");
            expect(notesEd != nullptr);
            if (notesEd != nullptr) {
                expect(notesEd->isMultiLine());
                notesEd->setText("Line 1\nLine 2");
                expectEquals(notesEd->getText(), juce::String("Line 1\nLine 2"));
            }

            safeCleanupJiveTree(rootItem);
        }
    }
};

static JiveModalDialogTest jiveModalDialogTest;
