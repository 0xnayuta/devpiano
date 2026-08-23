#include <JuceHeader.h>

#include "UI/KeyBindingEditDialog.h"
#include "UI/PerformanceMetadataDialog.h"
#include "UI/PresetDialogs.h"
#include "UI/jive/DesignTokens.h"
#include "UI/jive/JiveModalDialog.h"
#include "UI/jive/JiveUtils.h"
#include "UI/jive/StyleCatalog.h"

namespace {
using namespace devpiano::ui::jive;
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
        testPresetAndMetadataDialogBuilders();
        testProgressLayoutBuilder();
        testKeyBindingEditDialogLayoutBuilder();
    }
    void testProgressLayoutBuilder() {
        beginTest("makeProgressLayout: progress bar and message nodes");

        auto tree
            = devpiano::ui::jive::JiveModalDialog::makeProgressLayout("Exporting WAV (50%)...", 380, 140, "Abort");
        expect(tree.isValid());
        expectEquals(static_cast<int>(tree.getProperty("width")), 380);
        expectEquals(static_cast<int>(tree.getProperty("height")), 140);

        auto msgNode = findNodeById(tree, "progress-status-message");
        expect(msgNode.isValid());
        expectEquals(msgNode.getType().toString(), juce::String("Text"));
        expectEquals(msgNode.getProperty("text").toString(), juce::String("Exporting WAV (50%)..."));

        auto barNode = findNodeById(tree, "dialog-progress-bar");
        expect(barNode.isValid());
        expectEquals(barNode.getType().toString(), juce::String("ProgressBar"));

        auto cancelBtn = findNodeById(tree, "dialog-cancel-btn");
        expect(cancelBtn.isValid());
        expectEquals(cancelBtn.getProperty("title").toString(), juce::String("Abort"));
    }

    void testKeyBindingEditDialogLayoutBuilder() {
        beginTest("KeyBindingEditDialog: makeKeyBindingEditLayout with and without existing binding");

        // 1. With existing binding
        auto treeBound = KeyBindingEditDialog::makeKeyBindingEditLayout(true, 420, 290);
        expect(treeBound.isValid());
        expectEquals(static_cast<int>(treeBound.getProperty("width")), 420);
        expectEquals(static_cast<int>(treeBound.getProperty("height")), 290);

        expect(findNodeById(treeBound, "binding-info-text").isValid());
        expect(findNodeById(treeBound, "channel-combo").isValid());
        expect(findNodeById(treeBound, "note-slider").isValid());
        expect(findNodeById(treeBound, "velocity-slider").isValid());
        expect(findNodeById(treeBound, "custom-label-editor").isValid());
        expect(static_cast<bool>(findNodeById(treeBound, "custom-label-editor").getProperty("focusable")),
               "label editor must be focusable to accept typed input");
        expect(findNodeById(treeBound, "custom-colour-palette").isValid());
        expectEquals(findNodeById(treeBound, "colour-btn-0").getType().toString(), juce::String("ColourSwatch"));
        expectEquals(findNodeById(treeBound, "colour-btn-7").getType().toString(), juce::String("ColourSwatch"));
        expect(findNodeById(treeBound, "clear-colour-btn").isValid());
        expect(findNodeById(treeBound, "dialog-unbind-btn").isValid());
        expect(findNodeById(treeBound, "dialog-ok-btn").isValid());
        expect(findNodeById(treeBound, "dialog-cancel-btn").isValid());

        // 2. Without existing binding (read-only / unbound)
        auto treeUnbound = KeyBindingEditDialog::makeKeyBindingEditLayout(false, 420, 200);
        expect(treeUnbound.isValid());
        expectEquals(static_cast<int>(treeUnbound.getProperty("width")), 420);
        expectEquals(static_cast<int>(treeUnbound.getProperty("height")), 200);

        expect(findNodeById(treeUnbound, "binding-info-text").isValid());
        expect(!findNodeById(treeUnbound, "channel-combo").isValid());
        expect(!findNodeById(treeUnbound, "note-slider").isValid());
        expect(!findNodeById(treeUnbound, "velocity-slider").isValid());
        expect(!findNodeById(treeUnbound, "dialog-unbind-btn").isValid());
        expect(findNodeById(treeUnbound, "dialog-bind-btn").isValid(), "unbound notes must expose Bind Key");
        expect(findNodeById(treeUnbound, "custom-label-editor").isValid());
        expect(findNodeById(treeUnbound, "dialog-ok-btn").isValid());
        expect(findNodeById(treeUnbound, "dialog-cancel-btn").isValid());
    }
    void testPresetAndMetadataDialogBuilders() {
        beginTest("Preset and Metadata dialog templates integration");

        // Verify default preset single-input template
        auto presetInputTree = devpiano::ui::jive::JiveModalDialog::makeSingleInputLayout(TRANS("Preset Name:"));
        expect(presetInputTree.isValid());
        expect(findNodeById(presetInputTree, "dialog-label").isValid());
        expect(findNodeById(presetInputTree, "dialog-editor").isValid());
        expect(findNodeById(presetInputTree, "dialog-ok-btn").isValid());
        expect(findNodeById(presetInputTree, "dialog-cancel-btn").isValid());

        // Verify preset confirmation template
        auto presetConfirmTree = devpiano::ui::jive::JiveModalDialog::makeConfirmLayout(
            "Delete preset \"MyPreset\"?", 380, 140, TRANS("Delete"), TRANS("Cancel"));
        expect(presetConfirmTree.isValid());
        expect(findNodeById(presetConfirmTree, "dialog-message").isValid());
        expect(findNodeById(presetConfirmTree, "dialog-ok-btn").isValid());
        expect(findNodeById(presetConfirmTree, "dialog-cancel-btn").isValid());

        // Verify metadata edit template
        auto metadataTree = devpiano::ui::jive::JiveModalDialog::makeMetadataEditLayout();
        expect(metadataTree.isValid());
        expect(findNodeById(metadataTree, "title-label").isValid());
        expect(findNodeById(metadataTree, "title-editor").isValid());
        expect(findNodeById(metadataTree, "notes-label").isValid());
        expect(findNodeById(metadataTree, "notes-editor").isValid());
        expect(findNodeById(metadataTree, "dialog-ok-btn").isValid());
        expect(findNodeById(metadataTree, "dialog-cancel-btn").isValid());
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
        expectEquals(editorNode.getProperty("focusable"), juce::var(true));
        expectEquals(editorNode.getProperty("cursor").toString(), juce::String("text"));
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
        expectEquals(titleEditor.getProperty("focusable"), juce::var(true));
        auto notesLabel = findNodeById(tree, "notes-label");
        expect(notesLabel.isValid());
        auto notesEditor = findNodeById(tree, "notes-editor");
        expect(notesEditor.isValid());
        expectEquals(notesEditor.getType().toString(), juce::String("ListEditor"));
        expectEquals(notesEditor.getProperty("focusable"), juce::var(true));
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
        devpiano::ui::jive::ScopedJiveTree rootItem = interpreter.interpret(tree);
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
                expect(editor->getWantsKeyboardFocus());
            }

            // Non-existent IDs should return nullptr safely
            expect(devpiano::ui::jive::JiveModalDialog::findButtonById(*rootItem, "non-existent-btn") == nullptr);
            expect(devpiano::ui::jive::JiveModalDialog::findTextEditorById(*rootItem, "non-existent-editor")
                   == nullptr);
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
        devpiano::ui::jive::ScopedJiveTree rootItem = interpreter.interpret(tree);
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
        }
    }
};

static JiveModalDialogTest jiveModalDialogTest;
