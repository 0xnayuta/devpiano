#include "TestHelpers.h"
#include "UI/jive/JiveUtils.h"
#include "UI/jive/LayoutModel.h"
#include "UI/jive/StyleCatalog.h"
#include "UI/jive/core/jive_layouts.h"
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
class PathEditorReproTest final : public juce::UnitTest {
public:
    PathEditorReproTest()
        : juce::UnitTest("PathEditorRepro", "DevPiano/UI") {
    }

    void runTest() override {
        beginTest("path editor text");

        // 独立基线（TEST-013）：不依赖其他测试文件对 StyleCatalog 单例的覆写。
        devpiano::ui::jive::StyleCatalog::get().reset();

        // Load the real style sheet (the PathEditor rule must match the
        // PathEditor factory type). Located relative to __FILE__ (TEST-014)
        // so it works from any working directory; skipped when the file is
        // missing (e.g. a bare binary build without the source tree).
        const juce::File styleSheetFile
            = devpiano::test::findRepoRoot().getChildFile("source/UI/jive/style_sheets.json");
        if (!styleSheetFile.existsAsFile()) {
            return; // not a repo checkout; skip
        }
        const auto styleJson = juce::JSON::parse(styleSheetFile.loadFileAsString());
        expect(!styleJson.isVoid(), "style_sheets.json must parse as JSON");
        if (styleJson.isVoid()) {
            return;
        }
        devpiano::ui::jive::StyleCatalog::get().loadFromJSON(styleJson);

        ::jive::Interpreter interpreter;
        auto& factory = interpreter.getComponentFactory();
        factory.set("PathEditor", [] {
            auto editor = std::make_unique<juce::TextEditor>();
            editor->setMultiLine(false);
            editor->setReturnKeyStartsNewLine(false);
            return editor;
        });

        juce::ValueTree tree("PathEditor");
        tree.setProperty("id", "plugin-path-editor", nullptr);
        tree.setProperty("flex-grow", 1.0, nullptr);
        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);
        devpiano::ui::jive::ScopedJiveTree item = interpreter.interpret(tree);
        expect(item != nullptr, "interpret");
        if (item == nullptr) {
            return;
        }

        auto* editor = dynamic_cast<juce::TextEditor*>(item->getComponent().get());
        expect(editor != nullptr, "is TextEditor");
        if (editor == nullptr) {
            return;
        }

        editor->setText("C:\\VST3\\pianoteq 9.vst3", juce::dontSendNotification);
        expectEquals(editor->getText(), juce::String("C:\\VST3\\pianoteq 9.vst3"), "text set");
        expect(editor->findColour(juce::TextEditor::textColourId).getAlpha() > 0, "text colour not transparent");
        // The PathEditor rule from style_sheets.json must have matched the
        // node type and attached a StyleSheet with its background canvas.
        expect(tree.hasProperty("style"), "style property applied before interpretation");
        expect(item->getComponent()->getProperties().contains("style-sheet"), "style sheet attached");
        expect(item->getComponent()->getNumChildComponents() > 0, "background canvas present");

        // Full panel layout: the editor is declared 26px tall. Its laid-out
        // height must stay on that declaration, not shrink below it.
        auto panelTree = devpiano::ui::jive::makePluginPanelTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(panelTree);
        devpiano::ui::jive::ScopedJiveTree panelItem = interpreter.interpret(panelTree);
        expect(panelItem != nullptr, "panel interpreted");
        if (panelItem == nullptr) {
            return;
        }
        panelItem->getComponent()->setBounds(0, 0, 500, 200);
        auto* pathEditorItem = jive::findItemWithID(*panelItem, "plugin-path-editor");
        expect(pathEditorItem != nullptr, "path editor item found");
        if (pathEditorItem == nullptr) {
            return;
        }
        const auto editorBounds = pathEditorItem->getComponent()->getBounds();

        // Expand the area (like setPluginPanelExpanded(true)) and re-check:
        // with height 0 the area's layOutChildren bails on empty bounds, so
        // the row children keep their pre-layout zero width.
        if (auto* area
            = dynamic_cast<jive::GuiItemDecorator*>(jive::findItemWithID(*panelItem, "plugin-expanded-area"))) {
            area->state.setProperty("height", 32, nullptr);
            if (auto* panel = dynamic_cast<jive::GuiItemDecorator*>(jive::findItemWithID(*panelItem, "plugin-panel"))) {
                panel->state.setProperty("height", 80, nullptr);
                panel->layOutChildren();
            }

            const auto expandedBounds = pathEditorItem->getComponent()->getBounds();
            expect(expandedBounds.getWidth() > 0, "expanded: path editor has visible width");
            expect(expandedBounds.getHeight() >= 26,
                   "expanded: path editor keeps its declared height: " + expandedBounds.toString());
        }

        // Full app flow: root tree + setPluginPathText equivalent.
        auto rootTree = devpiano::ui::jive::makeRootLayout();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(rootTree);
        devpiano::ui::jive::ScopedJiveTree rootItem = interpreter.interpret(rootTree);
        expect(rootItem != nullptr, "root interpreted");
        if (rootItem == nullptr) {
            return;
        }
        rootItem->getComponent()->setBounds(0, 0, 1120, 760);
        if (auto* rootPanel = dynamic_cast<jive::GuiItemDecorator*>(jive::findItemWithID(*rootItem, "plugin-panel"))) {
            if (auto* rootArea = jive::findItemWithID(*rootItem, "plugin-expanded-area")) {
                rootArea->state.setProperty("height", 32, nullptr);
            }
            rootPanel->state.setProperty("height", 80, nullptr);
            rootPanel->layOutChildren();
        }
        auto* rootEditorItem = jive::findItemWithID(*rootItem, "plugin-path-editor");
        expect(rootEditorItem != nullptr, "root path editor found");
        if (rootEditorItem == nullptr) {
            return;
        }
        auto* rootEditor = dynamic_cast<juce::TextEditor*>(rootEditorItem->getComponent().get());
        expect(rootEditor != nullptr, "root path editor is TextEditor");
        if (rootEditor == nullptr) {
            return;
        }
        rootEditor->setText(R"(C:\Program Files\Common Files\VST3)", juce::dontSendNotification);
        expectEquals(rootEditor->getText(), juce::String(R"(C:\Program Files\Common Files\VST3)"),
                     "root path text set");
        expect(rootEditorItem->getComponent()->getWidth() > 50, "root flow: path editor visible width");
        expect(rootEditorItem->getComponent()->getHeight() >= 26, "root flow: path editor keeps declared height");
        // Collapsed: the area has height 0 so JIVE skips laying out the row
        // (bounds.isEmpty()) — the editor keeps its initial 0 width, which is
        // correct for a hidden area. The expanded-state assertions above are
        // the real check.
        expect(editorBounds.getWidth() == 0, "collapsed: editor hidden (width 0)");

        // This test runs after StyleCatalogTest's cleanup; release the styles
        // owned by the catalog so the leak detector stays quiet.
        devpiano::ui::jive::StyleCatalog::get().releaseOwnedStyles();
        devpiano::test::drainMessages(2);
    }
};
static PathEditorReproTest pathEditorReproTest;
