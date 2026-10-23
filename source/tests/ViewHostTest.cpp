#include <JuceHeader.h>

#include "UI/ViewHost.h"
#include "UI/jive/DesignTokens.h"
#include "UI/jive/LayoutModel.h"
#include "UI/jive/StyleCatalog.h"
#include "UI/jive/core/values/jive_Identifiers.h"

class ViewHostTest final : public juce::UnitTest {
public:
    ViewHostTest()
        : juce::UnitTest("ViewHost", "DevPiano/UI") {
    }

    void runTest() override {
        devpiano::ui::jive::StyleCatalog::get().reset();
        devpiano::ui::DesignTokens::get().reset();

        testLifecycleAndBasicLayout();
        testSetupFlagIsClearedAfterLoad();
        testReloadMoveAndExternalTreeLifetime();
        testTypedComponentLookup();
        testPropertyAccessors();
        testSliderValueSyncsToState();
    }

    void testSliderValueSyncsToState() {
        beginTest("ViewHost: slider interaction writes back to the layout tree");

        juce::ValueTree root("Component");
        root.setProperty("width", 200, nullptr);
        root.setProperty("height", 100, nullptr);
        juce::ValueTree slider("Slider");
        slider.setProperty("id", "probe-slider", nullptr);
        slider.setProperty("width", 100, nullptr);
        slider.setProperty("height", 20, nullptr);
        root.addChild(slider, -1, nullptr);

        devpiano::ui::ViewHost host;
        host.registerDefaultComponents();
        expect(host.loadLayout(root, false));
        auto* component = host.find<juce::Slider>("probe-slider");
        expect(component != nullptr);
        if (component == nullptr) {
            return;
        }

        component->setValue(0.5, juce::sendNotificationSync);
        expectEquals(host.getProperty("probe-slider", "value").toString(), component->getTextFromValue(0.5));
    }

    void testSetupFlagIsClearedAfterLoad() {
        beginTest("ViewHost: interpretation leaves no setup flag behind");

        auto tree = devpiano::ui::jive::makePluginPanelTree();

        {
            devpiano::ui::ViewHost host;
            host.registerDefaultComponents();
            expect(host.loadLayout(tree, true));
        }

        juce::Array<juce::ValueTree> pending { tree };
        auto foundSetupFlag = false;

        while (!pending.isEmpty()) {
            const auto node = pending.removeAndReturn(pending.size() - 1);

            if (static_cast<bool>(node[::jive::ids::setupInProgress()])) {
                foundSetupFlag = true;
                break;
            }

            for (auto child : node) {
                pending.add(child);
            }
        }

        expect(!foundSetupFlag, "setup-in-progress must be cleared across the interpreted tree");
    }

    void testReloadMoveAndExternalTreeLifetime() {
        beginTest("ViewHost: reload, move and externally held layouts after teardown");

        auto first = devpiano::ui::jive::makePluginPanelTree();
        auto second = devpiano::ui::jive::makePluginPanelTree();

        {
            devpiano::ui::ViewHost host;
            host.registerDefaultComponents();
            expect(host.loadLayout(first, true));
            host.setBounds(0, 0, 900, 400);
            host.reset();
            expect(!host.isValid());

            expect(host.loadLayout(second, true));
            expect(host.isValid());

            devpiano::ui::ViewHost moved { std::move(host) };
            expect(moved.isValid());
            expect(moved.getRootComponent() != nullptr);
            moved.reset();
            expect(!moved.isValid());
        }

        // Both layout trees outlive the interpreted items; mutating them must
        // not reach any listener owned by the destroyed host.
        first.setProperty("height", 120, nullptr);
        second.addChild(juce::ValueTree("Component"), -1, nullptr);
        second.removeChild(0, nullptr);
        expect(first.isValid());
        expect(second.isValid());
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
