#include <JuceHeader.h>

#include "Audio/AudioDeviceDiagnostics.h"
#include "Settings/SettingsModel.h"
#include "Settings/jive/SettingsLayoutModel.h"
#include "UI/jive/DesignTokens.h"
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

juce::Component* findComponentById(::jive::GuiItem& root, const juce::String& id) {
    if (root.state.getProperty("id").toString() == id) {
        return root.getComponent().get();
    }
    for (auto* child : root.getChildren()) {
        if (auto* found = findComponentById(*child, id)) {
            return found;
        }
    }
    return nullptr;
}

} // namespace

class SettingsLayoutModelTest final : public juce::UnitTest {
public:
    SettingsLayoutModelTest()
        : juce::UnitTest("SettingsLayoutModel", "DevPiano/UI") {
    }

    void runTest() override {
        devpiano::ui::jive::StyleCatalog::get().reset();
        devpiano::jive::DesignTokens::get().reset();

        testSettingsLayoutTreeStructure();
        testKeySignatureSectionAndGrid();
        testKeyboardDisplaySection();
        testInterpretationAndComponentLookup();
        testFollowKeyVisibilityToggle();
    }

private:
    void testSettingsLayoutTreeStructure() {
        beginTest("makeSettingsLayoutTree: top-level structure and child sections");

        auto tree = devpiano::ui::jive::makeSettingsLayoutTree();
        expect(tree.isValid());
        expectEquals(tree.getType().toString(), juce::String("Component"));
        expectEquals(tree.getProperty("id").toString(), juce::String("settings-root"));

        // All 5 core sections must be present
        expect(findNodeById(tree, "audio-device-section").isValid());
        expect(findNodeById(tree, "key-sig-card").isValid());
        expect(findNodeById(tree, "keyboard-display-card").isValid());
        expect(findNodeById(tree, "diagnostics-card").isValid());
        expect(findNodeById(tree, "save-action-row").isValid());
    }

    void testKeySignatureSectionAndGrid() {
        beginTest("makeKeySignatureSectionTree: 16-channel CSS Grid and controls");

        auto tree = devpiano::ui::jive::makeKeySignatureSectionTree();
        expect(tree.isValid());

        // Key signature controls
        expect(findNodeById(tree, "key-sig-title").isValid());
        expect(findNodeById(tree, "key-signature-combo").isValid());
        expect(findNodeById(tree, "midi-transpose-toggle").isValid());

        // 16 follow key toggles inside CSS grid
        auto gridNode = findNodeById(tree, "follow-key-grid");
        expect(gridNode.isValid());
        expectEquals(gridNode.getProperty("display").toString(), juce::String("grid"));
        expect(gridNode.getProperty("grid-template-columns").toString().contains("1fr"));

        for (int ch = 0; ch < 16; ++ch) {
            auto cb = findNodeById(tree, "follow-key-" + juce::String(ch));
            expect(cb.isValid());
            expectEquals(cb.getType().toString(), juce::String("Checkbox"));
            expectEquals(cb.getProperty("text").toString(), "Ch" + juce::String(ch + 1));
        }
    }

    void testKeyboardDisplaySection() {
        beginTest("makeKeyboardDisplaySectionTree: display options and language");

        auto tree = devpiano::ui::jive::makeKeyboardDisplaySectionTree();
        expect(tree.isValid());

        expect(findNodeById(tree, "keyboard-display-title").isValid());
        expect(findNodeById(tree, "colour-mode-combo").isValid());
        expect(findNodeById(tree, "note-display-combo").isValid());
        expect(findNodeById(tree, "fade-speed-slider").isValid());
        expect(findNodeById(tree, "resizable-toggle").isValid());
        expect(findNodeById(tree, "instrument-filter-toggle").isValid());
        expect(findNodeById(tree, "language-combo").isValid());
    }

    void testInterpretationAndComponentLookup() {
        beginTest("Interpretation and dynamic component lookup from SettingsLayoutModel");

        auto tree = devpiano::ui::jive::makeSettingsLayoutTree();

        ::jive::Interpreter interpreter;
        auto& factory = interpreter.getComponentFactory();

        factory.set("AudioDeviceSelector", [] {
            return std::make_unique<juce::Component>(); // dummy for test
        });
        factory.set("ListEditor", [] {
            auto ed = std::make_unique<juce::TextEditor>();
            ed->setMultiLine(true);
            return ed;
        });

        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);
        auto rootItem = interpreter.interpret(tree);
        expect(rootItem != nullptr);

        if (rootItem != nullptr) {
            // Verify component lookup for all major controls
            auto* audioSelector = findComponentById(*rootItem, "audio-device-selector");
            expect(audioSelector != nullptr);

            auto* ksCombo = dynamic_cast<juce::ComboBox*>(findComponentById(*rootItem, "key-signature-combo"));
            expect(ksCombo != nullptr);

            auto* midiTranspose
                = dynamic_cast<juce::ToggleButton*>(findComponentById(*rootItem, "midi-transpose-toggle"));
            expect(midiTranspose != nullptr);

            for (int ch = 0; ch < 16; ++ch) {
                auto* cb
                    = dynamic_cast<juce::ToggleButton*>(findComponentById(*rootItem, "follow-key-" + juce::String(ch)));
                expect(cb != nullptr);
            }

            auto* colourCombo = dynamic_cast<juce::ComboBox*>(findComponentById(*rootItem, "colour-mode-combo"));
            expect(colourCombo != nullptr);

            auto* noteCombo = dynamic_cast<juce::ComboBox*>(findComponentById(*rootItem, "note-display-combo"));
            expect(noteCombo != nullptr);

            auto* fadeSlider = dynamic_cast<juce::Slider*>(findComponentById(*rootItem, "fade-speed-slider"));
            expect(fadeSlider != nullptr);

            auto* resizableCb = dynamic_cast<juce::ToggleButton*>(findComponentById(*rootItem, "resizable-toggle"));
            expect(resizableCb != nullptr);

            auto* filterCb
                = dynamic_cast<juce::ToggleButton*>(findComponentById(*rootItem, "instrument-filter-toggle"));
            expect(filterCb != nullptr);

            auto* langCombo = dynamic_cast<juce::ComboBox*>(findComponentById(*rootItem, "language-combo"));
            expect(langCombo != nullptr);

            auto* diagEd = dynamic_cast<juce::TextEditor*>(findComponentById(*rootItem, "diagnostics-editor"));
            expect(diagEd != nullptr);
            if (diagEd != nullptr) {
                expect(diagEd->isMultiLine());
            }

            auto* saveBtn = dynamic_cast<juce::Button*>(findComponentById(*rootItem, "save-button"));
            expect(saveBtn != nullptr);

            safeCleanupJiveTree(rootItem);
        }
    }

    void testFollowKeyVisibilityToggle() {
        beginTest("Dynamic display property toggle for follow-key-area");

        auto tree = devpiano::ui::jive::makeSettingsLayoutTree();

        ::jive::Interpreter interpreter;
        auto& factory = interpreter.getComponentFactory();
        factory.set("AudioDeviceSelector", [] { return std::make_unique<juce::Component>(); });
        factory.set("ListEditor", [] { return std::make_unique<juce::TextEditor>(); });

        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);
        auto rootItem = interpreter.interpret(tree);
        expect(rootItem != nullptr);

        if (rootItem != nullptr) {
            auto* followKeyArea = devpiano::ui::jive::findGuiItemById(*rootItem, "channel-follow-key-area");
            expect(followKeyArea != nullptr);

            if (followKeyArea != nullptr) {
                // Toggle display property
                followKeyArea->state.setProperty("display", "none", nullptr);
                expectEquals(followKeyArea->state.getProperty("display").toString(), juce::String("none"));

                followKeyArea->state.setProperty("display", "flex", nullptr);
                expectEquals(followKeyArea->state.getProperty("display").toString(), juce::String("flex"));
            }

            safeCleanupJiveTree(rootItem);
        }
    }
};

static SettingsLayoutModelTest settingsLayoutModelTest;
