#include <JuceHeader.h>

#include "UI/ViewHost.h"
#include "UI/jive/core/jive_Length.h"
#include "UI/jive/core/jive_Object.h"
#include "UI/jive/core/jive_Property.h"
#include "UI/jive/core/jive_StyleSheet.h"

// ============================================================================
/// JiveCoreValuesTest — Phase 39.5-2 value/property/object contracts.
///
/// Covers the behaviour that the v2 value migration must preserve:
/// - px/%/auto Length semantics and unit-safe interpolation
/// - transition removal/replacement keeping a valid property
/// - parsed Object parent chain and paired nested subscriptions
/// - per-StyleSheet calculated state that never leaks into shared styles
// ============================================================================
class JiveCoreValuesTest final : public juce::UnitTest {
public:
    JiveCoreValuesTest()
        : juce::UnitTest("JiveCoreValues", "DevPiano/UI") {
    }

    void runTest() override {
        testLengthValues();
        testTransitionReplacement();
        testBlockCoordinateExclusivity();
        testParsedObjectParentChain();
        testStyleSheetCalculatedStateIsolation();
    }

private:
    void testLengthValues() {
        beginTest("Length: px/%/auto parsing and unit-safe interpolation");

        using Unit = jive::Length::Unit;
        expect(jive::Length::fromString("18px") == jive::Length(18.0f, Unit::absolute));
        expect(jive::Length::fromString("18") == jive::Length(18.0f, Unit::absolute));
        expect(jive::Length::fromString("25%") == jive::Length(25.0f, Unit::percentage));
        expect(jive::Length::fromString("auto") == jive::Length(0.0f, Unit::absolute));
        expect(jive::Length::fromString(jive::Length(999.0f, Unit::absolute).toString())
               == jive::Length(999.0f, Unit::absolute));

        expectEquals(jive::Length(25.0f, Unit::percentage).toPixels(400.0f), 100.0f);
        expectEquals(jive::Length(18.0f, Unit::absolute).toPixels(400.0f), 18.0f);

        const auto held
            = jive::interpolate(jive::Length(10.0f, Unit::absolute), jive::Length(50.0f, Unit::percentage), 0.5);
        expect(held == jive::Length(10.0f, Unit::absolute), "unit changes must not interpolate px values as %");
        expect(jive::interpolate(jive::Length(10.0f, Unit::absolute), jive::Length(50.0f, Unit::percentage), 1.0)
               == jive::Length(50.0f, Unit::percentage));
    }

    void testTransitionReplacement() {
        beginTest("Property: transition removal and replacement keep a valid state");

        juce::ValueTree state("Component");
        state.setProperty("width", 100, nullptr);
        state.setProperty("transition", "width 1s linear 0s", nullptr);

        jive::Property<jive::Length> width { state, "width" };
        expect(width.isTransitioning());

        state.setProperty("width", 200, nullptr);
        const auto mid = width.calculateCurrent();
        expect(mid.value >= 100.0f && mid.value <= 200.0f, "transition must interpolate towards the new width");

        state.removeProperty("transition", nullptr);
        expect(!width.isTransitioning(), "removing the definition must drop the transition");

        state.setProperty("transition", "width 2s linear 0s", nullptr);
        expect(width.isTransitioning(), "replacing the definition must attach the new transition");

        int changes = 0;
        width.onValueChange = [&changes] { ++changes; };
        state.removeProperty("width", nullptr);
        expectEquals(changes, 1, "property removal must notify the consumer");
        expect(width.get() == jive::Length(0.0f, jive::Length::Unit::absolute));

        state.setProperty("width", "auto", nullptr);
        expect(width.isAuto());
        state.setProperty("width", "50%", nullptr);
        expect(!width.isAuto());
        expect(width.get() == jive::Length(50.0f, jive::Length::Unit::percentage));
    }

    void testBlockCoordinateExclusivity() {
        beginTest("BlockItem: coordinate exclusivity survives removal notifications");

        juce::ValueTree root("Component");
        root.setProperty("width", 400, nullptr);
        root.setProperty("height", 300, nullptr);
        root.setProperty("display", "block", nullptr);

        juce::ValueTree child("Component");
        child.setProperty("id", "block-child", nullptr);
        child.setProperty("width", 20, nullptr);
        child.setProperty("height", 20, nullptr);
        child.setProperty("x", 10, nullptr);
        child.setProperty("y", 10, nullptr);
        root.addChild(child, -1, nullptr);

        devpiano::ui::ViewHost host;
        host.registerDefaultComponents();
        expect(host.loadLayout(root, false));
        host.setBounds(0, 0, 400, 300);

        expect(host.setProperty("block-child", "centre-x", 100));
        expect(host.getProperty("block-child", "x").isVoid(), "setting centre-x must clear x");
        expectEquals(static_cast<int>(host.getProperty("block-child", "centre-x")), 100);

        auto* component = host.find("block-child");
        expect(component != nullptr);
        if (component != nullptr) {
            expectEquals(component->getX(), 90);
        }

        expect(host.setProperty("block-child", "x", 30));
        expect(host.getProperty("block-child", "centre-x").isVoid(), "setting x must clear centre-x");
        expectEquals(static_cast<int>(host.getProperty("block-child", "x")), 30);

        if (component != nullptr) {
            expectEquals(component->getX(), 30);
        }
    }

    void testParsedObjectParentChain() {
        beginTest("Object: parsed parent chain and paired nested subscriptions");

        auto parsed = jive::parseJSON(R"({"child":{"value":1}})");
        auto* root = dynamic_cast<jive::Object*>(parsed.getDynamicObject());
        expect(root != nullptr);
        if (root == nullptr) {
            return;
        }

        auto child = juce::VariantConverter<jive::Object::ReferenceCountedPointer>::fromVar(root->getProperty("child"));
        expect(child != nullptr);
        if (child == nullptr) {
            return;
        }
        expect(child->getParent() == root, "nested parse must initialise the parent link");

        struct Counter final : jive::Object::Listener {
            void propertyChanged(jive::Object&, const juce::Identifier&) final {
                ++count;
            }

            int count = 0;
        } counter;

        root->addListener(counter);

        child->setProperty("value", 2);
        expectEquals(counter.count, 1, "nested changes must reach the parent's listeners");

        root->removeProperty("child");
        expect(child->getParent() == nullptr, "removed nested objects must lose their parent link");
        expectEquals(counter.count, 2);

        child->setProperty("value", 3);
        expectEquals(counter.count, 2, "removed nested objects must no longer notify the parent");

        root->removeListener(counter);
    }

    void testStyleSheetCalculatedStateIsolation() {
        beginTest("StyleSheet: calculated values stay private and follow style replacement");

        auto shared = jive::Object::ReferenceCountedPointer { new jive::Object {} };
        shared->setProperty("background", "#112233FF");
        shared->setProperty("transition", "background 1s linear 0s");

        juce::Component firstComponent;
        juce::Component secondComponent;

        juce::ValueTree firstState("Component");
        juce::ValueTree secondState("Component");
        firstState.setProperty("style", juce::var(shared.get()), nullptr);
        secondState.setProperty("style", juce::var(shared.get()), nullptr);

        auto firstSheet = jive::StyleSheet::create(firstComponent, firstState);
        auto secondSheet = jive::StyleSheet::create(secondComponent, secondState);

        expect(firstSheet->getBackground().getColour() == juce::Colour(0xFF112233));
        expect(secondSheet->getBackground().getColour() == juce::Colour(0xFF112233));
        expect(!shared->hasProperty("calculated-background"),
               "calculated values must not be written into the shared style object");
        expect(!shared->hasProperty("calculated-foreground"));

        auto replacement = jive::Object::ReferenceCountedPointer { new jive::Object {} };
        replacement->setProperty("background", "#445566FF");

        secondState.setProperty("style", juce::var(replacement.get()), nullptr);
        expect(secondSheet->getBackground().getColour() == juce::Colour(0xFF445566),
               "replacing the style must rebind the held object");
        expect(firstSheet->getBackground().getColour() == juce::Colour(0xFF112233),
               "replacing one style must not affect another sheet");

        secondState.removeProperty("style", nullptr);
        expect(secondSheet->getBackground().getColour() == juce::Colour {},
               "clearing the style must clear the calculated background colour");

        // 与 ViewHost::safeCleanupJiveTree 相同的拆卸顺序：组件属性先释放
        // StyleSheet 所有权，再销毁宿主持有的引用。
        firstComponent.getProperties().remove("style-sheet");
        secondComponent.getProperties().remove("style-sheet");
        secondSheet = nullptr;
        firstSheet = nullptr;
    }
};

static JiveCoreValuesTest jiveCoreValuesTest;
