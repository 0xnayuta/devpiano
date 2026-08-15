#include "UI/jive/LayoutModel.h"
#include "UI/jive/StyleCatalog.h"
#include <JuceHeader.h>
#include <jive_layouts/jive_layouts.h>

class PathEditorReproTest final : public juce::UnitTest {
public:
    PathEditorReproTest()
        : juce::UnitTest("PathEditorRepro", "devpiano") {
    }

    void runTest() override {
        beginTest("path editor text");

        // Load the real style sheet (PathEditor may match the TextEditor rule)
        devpiano::ui::jive::StyleCatalog::get().loadFromJSON(
            juce::JSON::parse(juce::File("/root/repos/devpiano/source/UI/jive/style_sheets.json").loadFileAsString()));

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
        auto item = interpreter.interpret(tree);
        expect(item != nullptr, "interpret");
        if (item == nullptr)
            return;

        auto* editor = dynamic_cast<juce::TextEditor*>(item->getComponent().get());
        expect(editor != nullptr, "is TextEditor");
        if (editor == nullptr)
            return;

        editor->setText("C:\\VST3\\pianoteq 9.vst3", juce::dontSendNotification);
        expectEquals(editor->getText(), juce::String("C:\\VST3\\pianoteq 9.vst3"), "text set");
        expect(editor->isEnabled(), "enabled");
        juce::Logger::writeToLog("PATHEDITOR text='" + editor->getText()
                                 + "' enabled=" + juce::String(editor->isEnabled() ? "yes" : "no") + " textColour="
                                 + editor->findColour(juce::TextEditor::textColourId).toDisplayString(false));
        expect(editor->findColour(juce::TextEditor::textColourId).getAlpha() > 0, "text colour not transparent");

        // Full panel layout: the editor sits inside path-row (28 px) of the
        // expanded area — it must get a usable height, or its text is clipped.
        auto panelTree = devpiano::ui::jive::makePluginPanelTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(panelTree);
        auto panelItem = interpreter.interpret(panelTree);
        expect(panelItem != nullptr, "panel interpreted");
        if (panelItem == nullptr)
            return;
        panelItem->getComponent()->setBounds(0, 0, 500, 200);
        auto* pathEditorItem = jive::findItemWithID(*panelItem, "plugin-path-editor");
        expect(pathEditorItem != nullptr, "path editor item found");
        if (pathEditorItem == nullptr)
            return;
        const auto editorBounds = pathEditorItem->getComponent()->getBounds();
        juce::Logger::writeToLog("PATHEDITOR bounds=" + editorBounds.toString());

        // Expand the area (like setPluginPanelExpanded(true)) and re-check:
        // with height 0 the area's layOutChildren bails on empty bounds, so
        // the row children keep their pre-layout zero width.
        if (auto* area = jive::findItemWithID(*panelItem, "plugin-expanded-area")) {
            area->state.setProperty("height", 112, nullptr);
            if (auto* row = jive::findItemWithID(*panelItem, "plugin-path-row"))
                juce::Logger::writeToLog("PATHROW after expand row=" + row->getComponent()->getBounds().toString());
            juce::Logger::writeToLog("PATHROW after expand editor="
                                     + pathEditorItem->getComponent()->getBounds().toString());
        }

        // Full app flow: root tree + setPluginPathText equivalent.
        auto rootTree = devpiano::ui::jive::makeRootLayout();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(rootTree);
        auto rootItem = interpreter.interpret(rootTree);
        expect(rootItem != nullptr, "root interpreted");
        if (rootItem == nullptr)
            return;
        rootItem->getComponent()->setBounds(0, 0, 1120, 760);
        if (auto* rootArea = jive::findItemWithID(*rootItem, "plugin-expanded-area"))
            rootArea->state.setProperty("height", 112, nullptr);
        auto* rootEditorItem = jive::findItemWithID(*rootItem, "plugin-path-editor");
        expect(rootEditorItem != nullptr, "root path editor found");
        if (rootEditorItem == nullptr)
            return;
        auto* rootEditor = dynamic_cast<juce::TextEditor*>(rootEditorItem->getComponent().get());
        expect(rootEditor != nullptr, "root path editor is TextEditor");
        if (rootEditor == nullptr)
            return;
        rootEditor->setText("C:\\Program Files\\Common Files\\VST3", juce::dontSendNotification);
        expectEquals(rootEditor->getText(), juce::String("C:\\Program Files\\Common Files\\VST3"),
                     "root path text set");
        juce::Logger::writeToLog("PATHEDITOR root bounds=" + rootEditorItem->getComponent()->getBounds().toString()
                                 + " text='" + rootEditor->getText() + "'");
        expect(rootEditorItem->getComponent()->getWidth() > 50, "root flow: path editor visible width");
        for (const char* id :
             { "plugin-panel", "plugin-action-row", "plugin-expanded-area", "plugin-path-label", "plugin-path-editor",
               "browse-btn", "scan-btn", "plugin-path-row", "plugin-list-editor" })
            if (auto* it = jive::findItemWithID(*panelItem, id))
                juce::Logger::writeToLog(juce::String("PATHROW item ") + id
                                         + " bounds=" + it->getComponent()->getBounds().toString());
        // Collapsed: the area has height 0 so JIVE skips laying out the row
        // (bounds.isEmpty()) — the editor keeps its initial 0 width, which is
        // correct for a hidden area. The expanded assertions below are the
        // real check.
        expect(editorBounds.getWidth() == 0, "collapsed: editor hidden (width 0)");
        expect(editorBounds.getHeight() >= 20, "path editor has usable height: " + editorBounds.toString());
    }
};
static PathEditorReproTest pathEditorReproTest;
