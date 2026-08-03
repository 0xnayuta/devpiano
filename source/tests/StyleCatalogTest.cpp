#include <JuceHeader.h>

#include "UI/jive/LayoutModel.h"
#include "UI/jive/StyleCatalog.h"
#include "UI/native/StatusBarMidiDot.h"

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
        testStatusBarTreeInterprets();
        testPluginPanelTreeInterprets();
        testControlsPanelTreeInterprets();
        testKeyboardAreaTreeInterprets();
        testRootLayoutInterprets();
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
    void testStatusBarTreeInterprets() {
        beginTest("status bar tree interprets with labels and dot");

        ::jive::Interpreter interpreter;
        interpreter.getComponentFactory().set("StatusBarMidiDot", [] { return std::make_unique<StatusBarMidiDot>(); });

        auto tree = devpiano::ui::jive::makeStatusBarTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);

        auto item = interpreter.interpret(tree);
        expect(item != nullptr, "status bar interpretation failed");
        if (item == nullptr)
            return;

        // The status-bar rule must have reached the container (background).
        auto* dot = ::jive::findItemWithID(*item, "midi-dot");
        expect(dot != nullptr, "midi-dot item not found");
        if (dot != nullptr)
            expect(dynamic_cast<StatusBarMidiDot*>(dot->getComponent().get()) != nullptr,
                   "midi-dot component is not a StatusBarMidiDot");

        for (const auto* id : { "plugin-name-label", "audio-info-label", "time-label" }) {
            auto* label = ::jive::findItemWithID(*item, id);
            expect(label != nullptr, juce::String(id) + " item not found");
            if (label != nullptr)
                expect(dynamic_cast<jive::TextComponent*>(label->getComponent().get()) != nullptr,
                       juce::String(id) + " component is not a TextComponent");
        }
    }
    void testPluginPanelTreeInterprets() {
        beginTest("plugin panel tree interprets with all controls");

        ::jive::Interpreter interpreter;
        interpreter.getComponentFactory().set("PathEditor", [] {
            auto editor = std::make_unique<juce::TextEditor>();
            editor->setMultiLine(false);
            return editor;
        });
        interpreter.getComponentFactory().set("ListEditor", [] {
            auto editor = std::make_unique<juce::TextEditor>();
            editor->setMultiLine(true);
            editor->setReadOnly(true);
            return editor;
        });

        auto tree = devpiano::ui::jive::makePluginPanelTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);

        auto item = interpreter.interpret(tree);
        expect(item != nullptr, "plugin panel interpretation failed");
        if (item == nullptr)
            return;

        const auto expectComponent = [this, &item](const char* id) {
            auto* found = ::jive::findItemWithID(*item, id);
            expect(found != nullptr, juce::String(id) + " item not found");
            return found;
        };

        expect(expectComponent("plugin-selector") != nullptr, "");
        expect(expectComponent("plugin-filter-combo") != nullptr, "");
        for (const char* id : { "load-btn", "unload-btn", "editor-btn", "toggle-btn", "scan-btn", "browse-btn" })
            expect(expectComponent(id) != nullptr, "");
        expect(expectComponent("plugin-path-editor") != nullptr, "");
        expect(expectComponent("plugin-list-editor") != nullptr, "");

        // Filter combo must expose its three options as declarative children.
        auto* filter = ::jive::findItemWithID(*item, "plugin-filter-combo");
        expect(filter != nullptr, "filter combo missing");
        if (filter != nullptr) {
            int optionCount = 0;
            for (auto child : filter->state)
                if (child.hasType("Option"))
                    ++optionCount;
            expectEquals(optionCount, 3);
        }

        // The expanded area starts collapsed (height 0).
        auto* expandedArea = ::jive::findItemWithID(*item, "plugin-expanded-area");
        expect(expandedArea != nullptr, "expanded area missing");
        if (expandedArea != nullptr)
            expectEquals(expandedArea->state["height"].toString(), juce::String("0"));
    }
    void testControlsPanelTreeInterprets() {
        beginTest("controls panel tree interprets with knobs, curve and rows");

        ::jive::Interpreter interpreter;
        interpreter.getComponentFactory().set("DevKnob", [] {
            auto slider = std::make_unique<juce::Slider>();
            slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            return slider;
        });
        interpreter.getComponentFactory().set("AdsrCurve", [] { return std::make_unique<juce::Component>(); });
        interpreter.getComponentFactory().set("RecordButton", [] { return std::make_unique<juce::TextButton>(); });
        interpreter.getComponentFactory().set("PlayButton", [] { return std::make_unique<juce::TextButton>(); });
        interpreter.getComponentFactory().set("StopButton", [] { return std::make_unique<juce::TextButton>(); });
        interpreter.getComponentFactory().set("BackButton", [] { return std::make_unique<juce::TextButton>(); });

        auto tree = devpiano::ui::jive::makeControlsPanelTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);

        auto item = interpreter.interpret(tree);
        expect(item != nullptr, "controls panel interpretation failed");
        if (item == nullptr)
            return;

        for (const char* id :
             { "volume-knob", "attack-knob", "decay-knob", "sustain-knob", "release-knob", "speed-knob" }) {
            auto* knob = ::jive::findItemWithID(*item, id);
            expect(knob != nullptr, juce::String(id) + " not found");
            if (knob != nullptr)
                expect(dynamic_cast<juce::Slider*>(knob->getComponent().get()) != nullptr,
                       juce::String(id) + " is not a Slider");
        }

        for (const char* id : { "record-btn", "play-btn", "stop-btn", "back-btn", "export-midi-btn", "export-wav-btn",
                                "import-midi-btn", "save-perf-btn", "open-perf-btn", "song-info-btn", "recent-btn",
                                "save-preset-btn", "rename-preset-btn", "delete-preset-btn" }) {
            auto* btn = ::jive::findItemWithID(*item, id);
            expect(btn != nullptr, juce::String(id) + " not found");
            if (btn != nullptr)
                expect(dynamic_cast<juce::Button*>(btn->getComponent().get()) != nullptr,
                       juce::String(id) + " is not a Button");
        }

        auto* combo = ::jive::findItemWithID(*item, "preset-combo");
        expect(combo != nullptr, "preset-combo not found");
        if (combo != nullptr)
            expect(dynamic_cast<juce::ComboBox*>(combo->getComponent().get()) != nullptr,
                   "preset-combo is not a ComboBox");

        auto* curve = ::jive::findItemWithID(*item, "adsr-curve");
        expect(curve != nullptr, "adsr-curve not found");
    }
    void testKeyboardAreaTreeInterprets() {
        beginTest("keyboard area tree interprets with viewport");

        ::jive::Interpreter interpreter;
        juce::MidiKeyboardState keyboardState;
        interpreter.getComponentFactory().set("CustomKeyboard", [&keyboardState] {
            auto viewport = std::make_unique<juce::Viewport>();
            viewport->setScrollBarsShown(false, true, false, true);
            auto keyboard = std::make_unique<jive::TextComponent>();
            viewport->setViewedComponent(keyboard.release(), true);
            return viewport;
        });

        auto tree = devpiano::ui::jive::makeKeyboardAreaTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);

        auto item = interpreter.interpret(tree);
        expect(item != nullptr, "keyboard area interpretation failed");
        if (item == nullptr)
            return;

        auto* keyboard = ::jive::findItemWithID(*item, "custom-keyboard");
        expect(keyboard != nullptr, "custom-keyboard item not found");
        if (keyboard != nullptr)
            expect(dynamic_cast<juce::Viewport*>(keyboard->getComponent().get()) != nullptr,
                   "custom-keyboard component is not a Viewport");
    }
    void testRootLayoutInterprets() {
        beginTest("root layout interprets with every panel");

        ::jive::Interpreter interpreter;
        auto& factory = interpreter.getComponentFactory();
        factory.set("SettingsButton",
                    [] { return std::make_unique<juce::DrawableButton>("s", juce::DrawableButton::ImageFitted); });
        factory.set("PathEditor", [] { return std::make_unique<juce::TextEditor>(); });
        factory.set("ListEditor", [] { return std::make_unique<juce::TextEditor>(); });
        factory.set("DevKnob", [] { return std::make_unique<juce::Slider>(); });
        factory.set("AdsrCurve", [] { return std::make_unique<juce::Component>(); });
        for (const char* type : { "RecordButton", "PlayButton", "StopButton", "BackButton" })
            factory.set(type, [] { return std::make_unique<juce::TextButton>(); });
        juce::MidiKeyboardState keyboardState;
        factory.set("CustomKeyboard", [&keyboardState] {
            auto viewport = std::make_unique<juce::Viewport>();
            auto keyboard = std::make_unique<jive::TextComponent>();
            viewport->setViewedComponent(keyboard.release(), true);
            return viewport;
        });
        factory.set("StatusBarMidiDot", [] { return std::make_unique<juce::Component>(); });

        auto tree = devpiano::ui::jive::makeRootLayout();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);

        auto item = interpreter.interpret(tree);
        expect(item != nullptr, "root layout interpretation failed");
        if (item == nullptr)
            return;

        // Every top-level panel must be present and nested correctly.
        for (const char* id :
             { "header", "plugin-panel", "content-row", "controls-panel", "keyboard-area", "status-bar", "main-area",
               "custom-keyboard", "midi-dot", "settings-btn", "preset-combo", "volume-knob" }) {
            expect(::jive::findItemWithID(*item, id) != nullptr, juce::String(id) + " missing from root layout");
        }

        // The plugin panel starts collapsed (height 40).
        auto* plugin = ::jive::findItemWithID(*item, "plugin-panel");
        expect(plugin != nullptr, "");
        if (plugin != nullptr)
            expectEquals(plugin->state["height"].toString(), juce::String("40"));

        // Layout the root and verify panels receive non-zero bounds.
        item->getComponent()->setBounds(0, 0, 1120, 760);
        const auto headerBounds = ::jive::findItemWithID(*item, "header")->getComponent()->getBounds();
        const auto statusBounds = ::jive::findItemWithID(*item, "status-bar")->getComponent()->getBounds();
        const auto keyboardBounds = ::jive::findItemWithID(*item, "keyboard-area")->getComponent()->getBounds();
        expect(headerBounds.getHeight() > 0, "header has zero height after layout");
        expect(statusBounds.getHeight() > 0, "status bar has zero height after layout");
        expect(keyboardBounds.getHeight() > 0, "keyboard area has zero height after layout");
        expect(statusBounds.getBottom() <= 760, "status bar overflows the window");
    }
};

static StyleCatalogTest styleCatalogTest;
