#include <JuceHeader.h>

#include "UI/ViewHost.h"
#include "UI/jive/DesignTokens.h"
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
        beginTest("ViewHost: enabled and visible properties update the live button");

        devpiano::ui::ViewHost host;
        host.registerDefaultComponents();

        juce::ValueTree root("Component");
        root.setProperty("id", "root", nullptr);
        root.setProperty("width", 400, nullptr);
        root.setProperty("height", 300, nullptr);

        juce::ValueTree btn("Button");
        btn.setProperty("id", "sample-btn", nullptr);
        root.addChild(btn, -1, nullptr);

        expect(host.loadLayout(root, false));

        // setEnabled / setVisible affect live component
        expect(host.setEnabled("sample-btn", false));
        expect(host.setVisible("sample-btn", false));
        expect(host.setButtonLabel("sample-btn", "[Group B]"));
        expectEquals(host.getProperty("sample-btn", "title").toString(), juce::String("[Group B]"));
        auto* button = host.find<juce::Button>("sample-btn");
        expect(button != nullptr);
        if (button != nullptr) {
            expect(!button->isEnabled());
            expect(!button->isVisible());
        }
    }
};

static ViewHostTest viewHostTest;
