#include <JuceHeader.h>

#include "UI/ViewHost.h"
#include "UI/jive/DesignTokens.h"
#include "UI/jive/JiveBuilderHelpers.h"
#include "UI/jive/StyleCatalog.h"

class ViewHostTest final : public juce::UnitTest {
public:
    ViewHostTest()
        : juce::UnitTest("ViewHost", "DevPiano/UI") {
    }

    void runTest() override {
        devpiano::ui::jive::StyleCatalog::get().reset();
        devpiano::ui::DesignTokens::get().reset();

        testLifecycleAndBasicLayout();
        testTypedComponentLookup();
        testPropertyAccessors();
        testSliderHelpers();
        testSequentialReloads();
    }

    void testLifecycleAndBasicLayout() {
        beginTest("ViewHost: lifecycle, layout loading, and bounds");

        devpiano::ui::ViewHost host;
        expect(!host.isValid());
        expect(host.getRootComponent() == nullptr);

        host.registerDefaultComponents();

        // Build a simple declarative layout with explicit top-level size
        juce::ValueTree root("Component");
        root.setProperty("id", "root-box", nullptr);
        root.setProperty("width", 400, nullptr);
        root.setProperty("height", 300, nullptr);

        juce::ValueTree label("Text");
        label.setProperty("id", "test-label", nullptr);
        label.setProperty("text", "Initial Text", nullptr);
        root.addChild(label, -1, nullptr);

        const bool loaded = host.loadLayout(root, false);
        expect(loaded);
        expect(host.isValid());
        expect(host.getRootComponent() != nullptr);

        host.setBounds(0, 0, 400, 300);
        expectEquals(host.getRootComponent()->getWidth(), 400);
        expectEquals(host.getRootComponent()->getHeight(), 300);

        host.reset();
        expect(!host.isValid());
        expect(host.getRootComponent() == nullptr);
    }

    void testTypedComponentLookup() {
        beginTest("ViewHost: typed component lookup with find<T>");

        devpiano::ui::ViewHost host;
        host.registerDefaultComponents();

        juce::ValueTree root("Component");
        root.setProperty("id", "root", nullptr);
        root.setProperty("width", 400, nullptr);
        root.setProperty("height", 300, nullptr);

        juce::ValueTree btn("Button");
        btn.setProperty("id", "action-btn", nullptr);
        root.addChild(btn, -1, nullptr);

        juce::ValueTree editor("TextEditor");
        editor.setProperty("id", "input-editor", nullptr);
        root.addChild(editor, -1, nullptr);

        juce::ValueTree knob("DevKnob");
        knob.setProperty("id", "volume-knob", nullptr);
        root.addChild(knob, -1, nullptr);

        expect(host.loadLayout(root, false));

        // Typed lookup
        auto* button = host.find<juce::Button>("action-btn");
        expect(button != nullptr);

        auto* textEditor = host.find<juce::TextEditor>("input-editor");
        expect(textEditor != nullptr);

        auto* slider = host.find<juce::Slider>("volume-knob");
        expect(slider != nullptr);

        // Mismatched type returns nullptr
        auto* wrongType = host.find<juce::Slider>("action-btn");
        expect(wrongType == nullptr);

        // Non-existent ID returns nullptr
        auto* nonExistent = host.find<juce::Button>("ghost-button");
        expect(nonExistent == nullptr);
    }

    void testPropertyAccessors() {
        beginTest("ViewHost: property accessors (setText, setProperty, setEnabled, setVisible)");

        devpiano::ui::ViewHost host;
        host.registerDefaultComponents();

        juce::ValueTree root("Component");
        root.setProperty("id", "root", nullptr);
        root.setProperty("width", 400, nullptr);
        root.setProperty("height", 300, nullptr);

        juce::ValueTree label("Text");
        label.setProperty("id", "status-text", nullptr);
        label.setProperty("text", "Old", nullptr);
        root.addChild(label, -1, nullptr);

        juce::ValueTree btn("Button");
        btn.setProperty("id", "sample-btn", nullptr);
        juce::ValueTree btnText("Text");
        btnText.setProperty("text", "Old Label", nullptr);
        btn.addChild(btnText, -1, nullptr);
        root.addChild(btn, -1, nullptr);

        expect(host.loadLayout(root, false));

        // setText
        expect(host.setText("status-text", "New Status"));
        expectEquals(host.getProperty("status-text", "text").toString(), juce::String("New Status"));

        // setButtonLabel
        expect(host.setButtonLabel("sample-btn", "New Label"));
        expectEquals(host.getProperty("sample-btn", "title").toString(), juce::String("New Label"));

        // setProperty / getProperty
        expect(host.setProperty("status-text", "custom-prop", 42));
        expectEquals(static_cast<int>(host.getProperty("status-text", "custom-prop", 0)), 42);

        // setEnabled / setVisible
        expect(host.setEnabled("sample-btn", false));
        expect(host.setVisible("sample-btn", false));
    }

    void testSliderHelpers() {
        beginTest("ViewHost: slider helper methods");

        devpiano::ui::ViewHost host;
        host.registerDefaultComponents();

        juce::ValueTree root("Component");
        root.setProperty("id", "root", nullptr);
        root.setProperty("width", 400, nullptr);
        root.setProperty("height", 300, nullptr);

        juce::ValueTree knob("DevKnob");
        knob.setProperty("id", "gain-knob", nullptr);
        root.addChild(knob, -1, nullptr);

        expect(host.loadLayout(root, false));

        expect(host.setSliderValue("gain-knob", 0.75));
        expectWithinAbsoluteError(static_cast<float>(host.getSliderValue("gain-knob", 0.0)), 0.75f, 0.001f);

        // Fallback for non-existent slider
        expectWithinAbsoluteError(static_cast<float>(host.getSliderValue("non-existent", 0.123)), 0.123f, 0.001f);
    }

    void testSequentialReloads() {
        beginTest("ViewHost: multiple loadLayout calls without leaks or crashes");

        devpiano::ui::ViewHost host;
        host.registerDefaultComponents();

        for (int i = 0; i < 5; ++i) {
            juce::ValueTree tree("Component");
            tree.setProperty("id", "pass-" + juce::String(i), nullptr);
            tree.setProperty("width", 400, nullptr);
            tree.setProperty("height", 300, nullptr);
            expect(host.loadLayout(tree, false));
            expect(host.isValid());
        }

        host.reset();
        expect(!host.isValid());
    }
};

static ViewHostTest viewHostTest;
