#include <JuceHeader.h>

#include <cmath>
#include <memory>

#include "UI/KeyBindingEditDialog.h"
#include "UI/ViewHost.h"
#include "UI/jive/core/geometry/jive_BoxModel.h"
#include "UI/jive/core/geometry/jive_Length.h"
#include "UI/jive/core/layout/jive_NormalisedProgressBar.h"
#include "UI/jive/core/layout/jive_TextComponent.h"
#include "UI/jive/core/style/jive_StyleSheet.h"
#include "UI/jive/core/values/jive_Object.h"
#include "UI/jive/core/values/jive_Property.h"

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
        testPassiveDispatchThroughSetProperty();
        testFlexGapFillsBindingPalette();
        testFlexItemCacheInvalidation();
        testTextMeasurementReflowsFlexItem();
        testTextLayoutCacheReuse();
        testGridItemMarginInvalidatesCache();
        testParsedObjectParentChain();
        testStyleSheetCalculatedStateIsolation();
        testTransitionRemovalNotifiesConsumer();
        testTransitionCallbackDestruction();
        testNestedObjectRemovalWithoutExternalOwner();
        testAnimatedMarginAdvancesConstraintsVersion();
        testPaddedContainerMeasuresAtContentWidth();
        testShrunkenContainerMeasuresAtAllocatedWidth();
        testWordWrapChangeReflowsText();
        testStyleSheetLiveMutationKeepsTransition();
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

    void testPassiveDispatchThroughSetProperty() {
        beginTest("Retained widgets receive property changes through the owner listener");

        juce::ValueTree root("Component");
        root.setProperty("width", 400, nullptr);
        root.setProperty("height", 400, nullptr);
        root.setProperty("display", "flex", nullptr);
        root.setProperty("flex-direction", "column", nullptr);

        const auto add = [&root](const juce::Identifier& type, const juce::String& id) {
            juce::ValueTree node(type);
            node.setProperty("id", id, nullptr);
            node.setProperty("width", 100, nullptr);
            node.setProperty("height", 24, nullptr);
            root.addChild(node, -1, nullptr);
            return node;
        };

        add("Button", "probe-btn").setProperty("text", "Probe", nullptr);
        add("Text", "probe-text").setProperty("text", "one", nullptr);
        add("Slider", "probe-slider");
        add("ComboBox", "probe-combo");
        add("ProgressBar", "probe-bar");

        devpiano::ui::ViewHost host;
        host.registerDefaultComponents();
        expect(host.loadLayout(root, false));
        host.setBounds(0, 0, 400, 400);

        auto* button = host.find<juce::Button>("probe-btn");
        expect(button != nullptr);

        expect(host.setProperty("probe-btn", "enabled", false));
        if (button != nullptr) {
            expect(!button->isEnabled(), "enabled must reach the native button");
        }

        expect(host.setProperty("probe-btn", "opacity", 0.5f));
        if (button != nullptr) {
            expectWithinAbsoluteError(button->getAlpha(), 0.5f, 0.01f);
        }

        expect(host.setProperty("probe-btn", "toggled", true));
        if (button != nullptr) {
            expect(button->getToggleState(), "toggled must reach the native button");
        }

        expect(host.setProperty("probe-btn", "width", 120));
        if (button != nullptr) {
            expectEquals(button->getWidth(), 120);
        }

        expect(host.setProperty("probe-text", "text", "two"));
        auto* text = host.find<jive::TextComponent>("probe-text");
        expect(text != nullptr);
        if (text != nullptr) {
            expectEquals(text->getText(), juce::String("two"));
        }

        expect(host.setProperty("probe-slider", "value", "0.25"));
        auto* slider = host.find<juce::Slider>("probe-slider");
        expect(slider != nullptr);
        if (slider != nullptr) {
            expectWithinAbsoluteError(slider->getValue(), 0.25, 0.001);
        }

        expect(host.setProperty("probe-combo", "editable", true));
        auto* combo = host.find<juce::ComboBox>("probe-combo");
        expect(combo != nullptr);
        if (combo != nullptr) {
            expect(combo->isTextEditable());
        }

        expect(host.setProperty("probe-bar", "value", 0.75));
        auto* bar = host.find<jive::NormalisedProgressBar>("probe-bar");
        expect(bar != nullptr);
        if (bar != nullptr) {
            expectWithinAbsoluteError(bar->getValue(), 0.75, 0.001);
        }
    }

    void testFlexGapFillsBindingPalette() {
        beginTest("Flex gap keeps the binding palette evenly filled");

        auto tree = KeyBindingEditDialog::makeKeyBindingEditLayout(true);

        devpiano::ui::ViewHost host;
        host.registerDefaultComponents();
        expect(host.loadLayout(tree, true));
        host.fitToContent(static_cast<int>(tree.getProperty("width")));

        auto* palette = host.find("custom-colour-palette");
        auto* first = host.find("colour-btn-0");
        auto* lastSwatch = host.find("colour-btn-7");
        auto* clear = host.find("clear-colour-btn");
        expect(palette != nullptr && first != nullptr && lastSwatch != nullptr && clear != nullptr);
        if (palette == nullptr || first == nullptr || lastSwatch == nullptr || clear == nullptr) {
            return;
        }

        const auto firstBounds = palette->getLocalArea(first, first->getLocalBounds());
        const auto lastSwatchBounds = palette->getLocalArea(lastSwatch, lastSwatch->getLocalBounds());
        const auto clearBounds = palette->getLocalArea(clear, clear->getLocalBounds());

        expectEquals(firstBounds.getX(), 0, "the first swatch must start at the palette edge");
        expectEquals(lastSwatchBounds.getX(), 7 * (18 + 4));
        expectEquals(clearBounds.getX() - lastSwatchBounds.getRight(), 4, "one gap between the last swatch and Clear");
        expectEquals(clearBounds.getWidth(), 44);
        expectEquals(clearBounds.getRight(), palette->getWidth(), "Clear must end at the palette edge");
    }

    void testFlexItemCacheInvalidation() {
        beginTest("FlexItem: direction and margin invalidate cached geometry");

        juce::ValueTree root { "Component" };
        root.setProperty("id", "cache-root", nullptr);
        root.setProperty("display", "flex", nullptr);
        root.setProperty("flex-direction", "row", nullptr);
        root.setProperty("width", 200, nullptr);
        root.setProperty("height", 100, nullptr);

        juce::ValueTree child { "Text" };
        child.setProperty("id", "cache-child", nullptr);
        child.setProperty("text", "Hi", nullptr);
        child.setProperty("flex-shrink", 0.0f, nullptr);
        root.appendChild(child, nullptr);

        devpiano::ui::ViewHost host;
        host.registerDefaultComponents();
        expect(host.loadLayout(root, true));
        host.fitToContent(200);

        auto* component = host.find("cache-child");
        expect(component != nullptr);
        if (component == nullptr) {
            return;
        }

        expect(component->getWidth() < 200, "row layout uses the text's natural width");
        expect(component->getWidth() > 0);

        root.setProperty("flex-direction", "column", nullptr);
        host.relayoutContainer("cache-root");
        expectEquals(component->getWidth(), 200, "column layout must not reuse the row constraints");

        child.setProperty("margin", "0 0 0 20", nullptr);
        host.relayoutContainer("cache-root");
        expectEquals(component->getX(), 20, "margin changes must not reuse cached item geometry");
    }

    void testTextMeasurementReflowsFlexItem() {
        beginTest("FlexItem: text measurement changes reflow the cached geometry");

        juce::ValueTree root { "Component" };
        root.setProperty("id", "text-root", nullptr);
        root.setProperty("display", "flex", nullptr);
        root.setProperty("flex-direction", "column", nullptr);
        root.setProperty("width", 300, nullptr);

        juce::ValueTree text { "Text" };
        text.setProperty("id", "cache-text", nullptr);
        text.setProperty("text", "Hi", nullptr);
        text.setProperty("width", 100, nullptr);
        root.appendChild(text, nullptr);

        devpiano::ui::ViewHost host;
        host.registerDefaultComponents();
        expect(host.loadLayout(root, true));
        host.fitToContent(300);

        auto* component = host.find("cache-text");
        expect(component != nullptr);
        if (component == nullptr) {
            return;
        }

        const auto initialHeight = component->getHeight();
        text.setProperty("text", "A long line of text that must wrap onto several lines inside the narrow column.",
                         nullptr);

        expect(component->getHeight() > initialHeight, "wrapped text must grow the flex item");

        const auto heightBeforeLineSpacing = component->getHeight();
        text.setProperty("line-spacing", 20.0f, nullptr);
        expect(component->getHeight() > heightBeforeLineSpacing,
               "line spacing changes must invalidate the cached text height");
    }

    void testTextLayoutCacheReuse() {
        beginTest("TextComponent: layout cache reuses equivalent measurements");

        ::jive::TextComponent text;
        text.setFont(juce::Font { juce::FontOptions { 15.0f } });
        text.setText("The quick brown fox jumps over the lazy dog and keeps running through the field.");

        const auto* wrapped = &text.getTextLayout(120.0f);
        expect(wrapped->getNumLines() > 1);
        const auto* sameWidth = &text.getTextLayout(120.0f);
        expect(sameWidth == wrapped, "identical widths must reuse the cached layout");

        const auto wrappedWidth = wrapped->getWidth();
        const auto* narrower = &text.getTextLayout(wrappedWidth);
        expect(narrower == wrapped, "a narrower width that keeps the same line breaks must reuse the layout");

        const auto* wider = &text.getTextLayout(400.0f);
        expect(wider != nullptr);
        const auto wideWidth = wider->getWidth();

        text.setText("Hi");
        const auto* changed = &text.getTextLayout(400.0f);
        expect(changed->getWidth() < wideWidth, "changed text must not reuse the stale layout");

        text.setText("Cached layout rendering");
        text.setSize(120, 60);
        const auto image = text.createComponentSnapshot(text.getLocalBounds());
        expect(image.isValid());
        expectEquals(image.getWidth(), 120);

        const juce::Image::BitmapData pixels { image, juce::Image::BitmapData::readOnly };
        auto hasInk = false;
        for (auto y = 0; y < pixels.height && !hasInk; ++y) {
            for (auto x = 0; x < pixels.width; ++x) {
                if (pixels.getPixelColour(x, y).getAlpha() > 0) {
                    hasInk = true;
                    break;
                }
            }
        }
        expect(hasInk, "cached layout must still paint the text");
    }

    void testGridItemMarginInvalidatesCache() {
        beginTest("GridItem: margin changes invalidate cached geometry");

        juce::ValueTree root { "Component" };
        root.setProperty("id", "grid-root", nullptr);
        root.setProperty("display", "grid", nullptr);
        root.setProperty("justify-items", "start", nullptr);
        root.setProperty("align-items", "start", nullptr);
        root.setProperty("width", 200, nullptr);
        root.setProperty("height", 100, nullptr);

        juce::ValueTree child { "Button" };
        child.setProperty("id", "grid-child", nullptr);
        child.setProperty("width", 50, nullptr);
        child.setProperty("height", 20, nullptr);
        root.appendChild(child, nullptr);

        devpiano::ui::ViewHost host;
        host.registerDefaultComponents();
        expect(host.loadLayout(root, true));
        host.fitToContent(200);

        auto* component = host.find("grid-child");
        expect(component != nullptr);
        if (component == nullptr) {
            return;
        }

        expectEquals(component->getX(), 0);

        child.setProperty("margin", "0 0 0 20", nullptr);
        host.relayoutContainer("grid-root");
        expectEquals(component->getX(), 20, "grid item margin changes must not reuse cached geometry");
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

    void testTransitionRemovalNotifiesConsumer() {
        beginTest("Property: removing a running transition notifies the consumer");

        juce::ValueTree state("Component");
        state.setProperty("value", 0.0, nullptr);
        state.setProperty("transition", "value 1s linear 0s", nullptr);

        jive::Property<double> value { state, "value" };
        int changes = 0;
        value.onValueChange = [&changes] { ++changes; };

        state.setProperty("value", 1.0, nullptr);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(200);
        expect(changes > 0, "setting the target must notify the consumer");

        const auto changesBeforeRemoval = changes;
        state.removeProperty("transition", nullptr);
        expectEquals(changes, changesBeforeRemoval + 1, "removing the transition must notify the final value");
        expect(value.calculateCurrent() == 1.0, "calculateCurrent must resolve to the target after removal");
    }

    void testTransitionCallbackDestruction() {
        beginTest("Property: destroying the owner in its progress callback is safe");

        juce::ValueTree state("Component");
        state.setProperty("value", 0.0, nullptr);
        state.setProperty("transition", "value 200ms linear 0s", nullptr);

        auto value = std::make_unique<jive::Property<double>>(state, "value");
        auto destroyed = false;
        value->onTransitionProgressed = [&value, &destroyed] {
            if (!destroyed) {
                destroyed = true;
                value.reset();
            }
        };

        state.setProperty("value", 1.0, nullptr);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(300);

        expect(destroyed, "the progress callback must run");
        expect(value == nullptr, "the property must be destroyed by its callback");
    }

    void testNestedObjectRemovalWithoutExternalOwner() {
        beginTest("Object: removing a nested object without an external owner is safe");

        auto parsed = jive::parseJSON(R"({"child":{"value":1}})");
        auto* root = dynamic_cast<jive::Object*>(parsed.getDynamicObject());
        expect(root != nullptr);
        if (root == nullptr) {
            return;
        }

        root->removeProperty("child");
        expect(root->getProperty("child").isVoid());

        // 触发下一次 syncNestedObjects：旧实现会在这里解引用已释放的子对象。
        auto* replacement = new jive::Object {};
        root->setProperty("child", juce::var(replacement));
        expect(root->getProperty("child").getDynamicObject() == replacement);
        replacement->setProperty("value", 2);
        expectEquals(static_cast<int>(replacement->getProperty("value")), 2);
    }

    void testAnimatedMarginAdvancesConstraintsVersion() {
        beginTest("BoxModel: animated margin advances the constraints version");

        juce::ValueTree state("Component");
        state.setProperty("width", 100, nullptr);
        state.setProperty("height", 100, nullptr);
        state.setProperty("margin", "0", nullptr);
        state.setProperty("transition", "margin 100ms linear 0s", nullptr);

        jive::BoxModel box { state };
        state.setProperty("margin", "0 0 0 40", nullptr);
        const auto versionAfterSet = box.getConstraintsVersion();

        juce::MessageManager::getInstance()->runDispatchLoopUntil(80);
        expect(box.getConstraintsVersion() > versionAfterSet, "margin transition ticks must invalidate item caches");
    }

    void testPaddedContainerMeasuresAtContentWidth() {
        beginTest("Container: padding is not subtracted twice from child measurement");

        juce::ValueTree root("Component");
        root.setProperty("id", "padding-root", nullptr);
        root.setProperty("width", 100, nullptr);
        root.setProperty("padding", "10", nullptr);
        root.setProperty("display", "flex", nullptr);
        root.setProperty("flex-direction", "column", nullptr);

        juce::ValueTree column("Component");
        column.setProperty("id", "padding-column", nullptr);
        column.setProperty("width", 50, nullptr);
        column.setProperty("display", "flex", nullptr);
        column.setProperty("flex-direction", "column", nullptr);

        juce::ValueTree text("Text");
        text.setProperty("id", "padding-text", nullptr);
        text.setProperty("text", "One two three four five six seven eight nine ten.", nullptr);
        column.appendChild(text, nullptr);
        root.appendChild(column, nullptr);

        devpiano::ui::ViewHost host;
        host.registerDefaultComponents();
        expect(host.loadLayout(root, true));
        host.fitToContent(100);

        auto* columnComponent = host.find("padding-column");
        auto* textComponent = host.find<jive::TextComponent>("padding-text");
        expect(columnComponent != nullptr && textComponent != nullptr);
        if (columnComponent == nullptr || textComponent == nullptr) {
            return;
        }

        const auto expected = static_cast<int>(
            std::ceil(textComponent->getTextLayout(static_cast<float>(textComponent->getWidth())).getHeight()));
        expectEquals(columnComponent->getHeight(), expected,
                     "the nested column must be measured at its allocated content width");
    }

    void testShrunkenContainerMeasuresAtAllocatedWidth() {
        beginTest("Container: shrunken columns remeasure at their allocated width");

        juce::ValueTree root("Component");
        root.setProperty("id", "shrink-root", nullptr);
        root.setProperty("width", 100, nullptr);
        root.setProperty("display", "flex", nullptr);
        root.setProperty("flex-direction", "row", nullptr);

        const auto addColumn = [&root](int index) {
            juce::ValueTree column("Component");
            column.setProperty("id", "shrink-column-" + juce::String(index), nullptr);
            column.setProperty("display", "flex", nullptr);
            column.setProperty("flex-direction", "column", nullptr);
            column.setProperty("flex-shrink", 1.0, nullptr);

            juce::ValueTree text("Text");
            text.setProperty("id", "shrink-text-" + juce::String(index), nullptr);
            text.setProperty("text", "abcd abcd", nullptr);
            column.appendChild(text, nullptr);
            root.appendChild(column, nullptr);
        };
        addColumn(0);
        addColumn(1);

        devpiano::ui::ViewHost host;
        host.registerDefaultComponents();
        expect(host.loadLayout(root, true));
        host.fitToContent(100);

        auto* columnComponent = host.find("shrink-column-0");
        auto* textComponent = host.find<jive::TextComponent>("shrink-text-0");
        expect(columnComponent != nullptr && textComponent != nullptr);
        if (columnComponent == nullptr || textComponent == nullptr) {
            return;
        }

        const auto expected = static_cast<int>(
            std::ceil(textComponent->getTextLayout(static_cast<float>(textComponent->getWidth())).getHeight()));
        expectEquals(columnComponent->getHeight(), expected,
                     "a shrunk column must remeasure its wrapped content at the allocated width");
    }

    void testWordWrapChangeReflowsText() {
        beginTest("Text: changing word-wrap reflows a fixed-width text");

        juce::ValueTree root("Component");
        root.setProperty("id", "wrap-root", nullptr);
        root.setProperty("width", 100, nullptr);
        root.setProperty("display", "flex", nullptr);
        root.setProperty("flex-direction", "column", nullptr);

        juce::ValueTree text("Text");
        text.setProperty("id", "wrap-text", nullptr);
        text.setProperty("width", 100, nullptr);
        text.setProperty("text", "The quick brown fox jumps over the lazy dog and keeps running through the field.",
                         nullptr);
        root.appendChild(text, nullptr);

        devpiano::ui::ViewHost host;
        host.registerDefaultComponents();
        expect(host.loadLayout(root, true));
        host.fitToContent(100);

        auto* component = host.find("wrap-text");
        auto* textComponent = host.find<jive::TextComponent>("wrap-text");
        expect(component != nullptr && textComponent != nullptr);
        if (component == nullptr || textComponent == nullptr) {
            return;
        }

        const auto wrappedHeight = component->getHeight();
        expect(wrappedHeight > 20);

        expect(host.setProperty("wrap-text", "word-wrap", "none"));
        const auto expected = static_cast<int>(std::ceil(textComponent->getTextLayout(100.0f).getHeight()));
        expectEquals(component->getHeight(), expected, "a wrap change must reflow the text immediately");
        expect(component->getHeight() < wrappedHeight);
    }

    void testStyleSheetLiveMutationKeepsTransition() {
        beginTest("StyleSheet: live mutation of the same style object keeps its transition");

        juce::Component component;
        component.setSize(100, 100);

        juce::ValueTree state("Component");
        auto shared = jive::Object::ReferenceCountedPointer { new jive::Object {} };
        shared->setProperty("background", "#112233FF");
        shared->setProperty("transition", "background 200ms linear 0s");
        state.setProperty("style", juce::var(shared.get()), nullptr);

        auto sheet = jive::StyleSheet::create(component, state);
        expect(sheet->getBackground().getColour() == juce::Colour(0xFF112233));

        shared->setProperty("background", "#445566FF");
        expect(sheet->getBackground().getColour() == juce::Colour(0xFF112233),
               "a live style mutation must start from the previous colour");

        juce::MessageManager::getInstance()->runDispatchLoopUntil(300);
        expect(sheet->getBackground().getColour() == juce::Colour(0xFF445566),
               "the transition must reach the new colour");

        component.getProperties().remove("style-sheet");
        sheet = nullptr;
    }
};

static JiveCoreValuesTest jiveCoreValuesTest;
