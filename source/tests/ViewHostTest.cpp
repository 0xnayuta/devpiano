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
        testRelayoutContainer();
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

    void testRelayoutContainer() {
        beginTest("ViewHost: relayoutContainer handles Flex and Grid containers safely");

        devpiano::ui::ViewHost host;
        host.registerDefaultComponents();

        // Safe no-op on uninitialized host
        host.relayoutContainer("non-existent");

        juce::ValueTree root("Component");
        root.setProperty("id", "root", nullptr);
        root.setProperty("width", 500, nullptr);
        root.setProperty("height", 400, nullptr);

        auto flexBox = devpiano::ui::jive::flexRow("test-flex");
        flexBox.setProperty("width", 200, nullptr);
        flexBox.setProperty("height", 50, nullptr);
        root.addChild(flexBox, -1, nullptr);

        juce::ValueTree gridBox("Component");
        gridBox.setProperty("id", "test-grid", nullptr);
        gridBox.setProperty("display", "grid", nullptr);
        gridBox.setProperty("grid-template-columns", "1fr 1fr", nullptr);
        gridBox.setProperty("width", 200, nullptr);
        gridBox.setProperty("height", 60, nullptr);

        auto item1 = devpiano::ui::jive::button("Btn1", "btn-1");
        auto item2 = devpiano::ui::jive::button("Btn2", "btn-2");
        gridBox.addChild(item1, -1, nullptr);
        gridBox.addChild(item2, -1, nullptr);
        root.addChild(gridBox, -1, nullptr);

        expect(host.loadLayout(root, false));
        expect(host.isValid());

        // Test safe execution on Flex container
        host.relayoutContainer("test-flex");

        // Test safe execution on Grid container
        host.relayoutContainer("test-grid");

        // Test safe execution on non-existent ID
        host.relayoutContainer("ghost-container");
    }
};

static ViewHostTest viewHostTest;
