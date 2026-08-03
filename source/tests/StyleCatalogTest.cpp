#include <JuceHeader.h>

#include "UI/jive/LayoutModel.h"
#include "UI/jive/StyleCatalog.h"

#include <jive_layouts/jive_layouts.h>

// =============================================================================
// Regression test for the JIVE style injection chain.
//
// The first JIVE migration attempt stored styles as plain juce::DynamicObject
// vars, which jive::VariantConverter<jive::Object::Ptr> rejects (jassert +
// nullptr) — so no styles ever applied and text stayed invisible (black on
// dark). This test locks in the fix: StyleCatalog emits style as a JSON
// string, which JIVE parses into a jive::Object, and interpretation must
// yield a styled TextComponent.
// =============================================================================

class StyleCatalogTest final : public juce::UnitTest {
public:
    StyleCatalogTest()
        : juce::UnitTest("StyleCatalog", "devpiano") {
    }

    void runTest() override {
        testJsonStringParsesToJiveObject();
        testAppliedStylesReachInterpretedComponents();
    }

private:
    void testJsonStringParsesToJiveObject() {
        beginTest("style stored as JSON string parses into jive::Object");

        const juce::var json = juce::JSON::parse(
            R"({ "Text": { "foreground": "#EEEEEE", "font-size": 14 },
                 "#title": { "font-size": 18, "font-weight": "bold" },
                 "Button": { "background": "#2D3035",
                             "hover": { "background": "#35383D" } },
                 "#settings-btn": { "background": "#2D3035",
                                    "hover": { "background": "#35383D" } } })");
        auto& catalog = devpiano::ui::jive::StyleCatalog::get();
        catalog.loadFromJSON(json);

        auto tree = devpiano::ui::jive::makeHeaderTree();
        catalog.applyToTree(tree);

        // The title node must carry a style var holding a jive::Object —
        // plain juce::DynamicObject vars are rejected by JIVE's converter.
        const auto title = tree.getChildWithProperty("id", "title");
        expect(title.isValid(), "title node missing");
        expect(title.hasProperty("style"), "title has no style property");

        if (!title.hasProperty("style"))
            return;

        const auto styleVar = title["style"];
        expect(styleVar.getObject() != nullptr, "style must be a DynamicObject");
        if (styleVar.getObject() == nullptr)
            return;

        auto* object = dynamic_cast<::jive::Object*>(styleVar.getObject());
        expect(object != nullptr, "style must be a jive::Object (plain DynamicObject is rejected by JIVE)");
        if (object == nullptr)
            return;

        expectEquals(object->getProperty("foreground").toString(), juce::String("#EEEEEE"));
        expectEquals(object->getProperty("font-size").toString(), juce::String("18"));
        expectEquals(object->getProperty("font-weight").toString(), juce::String("bold"));

        // Pseudo-state rules must be nested objects with the JIVE names
        // (hover/active/focus...), not ":hover" / "pressed".
        const auto settings = tree.getChildWithProperty("id", "settings-btn");
        expect(settings.isValid(), "settings-btn node missing");
        if (!settings.isValid())
            return;

        auto* btnStyle = dynamic_cast<::jive::Object*>(settings["style"].getObject());
        expect(btnStyle != nullptr, "settings-btn style must be a jive::Object");
        if (btnStyle == nullptr)
            return;

        expectEquals(btnStyle->getProperty("background").toString(), juce::String("#2D3035"));
        auto* hover = btnStyle->getProperty("hover").getDynamicObject();
        expect(hover != nullptr, "hover sub-rule missing");
        if (hover != nullptr)
            expectEquals(hover->getProperty("background").toString(), juce::String("#35383D"));
    }

    void testAppliedStylesReachInterpretedComponents() {
        beginTest("interpreted header renders with applied styles");

        ::jive::Interpreter interpreter;
        interpreter.getComponentFactory().set("SettingsButton", [] { return std::make_unique<juce::Component>(); });

        auto tree = devpiano::ui::jive::makeHeaderTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);

        auto item = interpreter.interpret(tree);
        expect(item != nullptr, "interpretation failed");
        if (item == nullptr)
            return;

        auto* titleItem = ::jive::findItemWithID(*item, "title");
        expect(titleItem != nullptr, "title item not found");
        if (titleItem == nullptr)
            return;

        auto* text = dynamic_cast<jive::TextComponent*>(titleItem->getComponent().get());
        expect(text != nullptr, "title component is not a TextComponent");
        if (text == nullptr)
            return;

        // StyleSheet must have applied the #title rule — the text colour
        // proves the style pipeline end to end (the font-size value itself is
        // asserted at the Object level in the first test; JIVE's font-height
        // application is environment-dependent and not observable here).
        expectEquals(text->getTextColour(), juce::Colour(0xFFEEEEEE));
    }
};

static StyleCatalogTest styleCatalogTest;
